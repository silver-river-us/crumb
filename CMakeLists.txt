cmake_minimum_required(VERSION 3.23)

project(crumb
    VERSION 0.1.0
    DESCRIPTION "A modern C++ project"
    LANGUAGES CXX
)

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CRUMB_WARNINGS_AS_ERRORS "Treat Crumb compiler warnings as errors" OFF)
set(CRUMB_SANITIZERS "" CACHE STRING "Comma-separated sanitizers: address,undefined,thread")
option(CRUMB_ENABLE_COVERAGE "Enable LLVM source-based coverage instrumentation" OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    if(CRUMB_WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()
    if(CRUMB_ENABLE_COVERAGE)
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            message(FATAL_ERROR "CRUMB_ENABLE_COVERAGE requires Clang")
        endif()
        add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
        add_link_options(-fprofile-instr-generate -fcoverage-mapping)
    endif()
    if(CRUMB_SANITIZERS)
        string(REPLACE "," ";" CRUMB_SANITIZER_LIST "${CRUMB_SANITIZERS}")
        foreach(sanitizer IN LISTS CRUMB_SANITIZER_LIST)
            if(NOT sanitizer MATCHES "^(address|undefined|thread)$")
                message(FATAL_ERROR "Unsupported CRUMB_SANITIZERS value: ${sanitizer}")
            endif()
            add_compile_options("-fsanitize=${sanitizer}" -fno-omit-frame-pointer)
            add_link_options("-fsanitize=${sanitizer}")
        endforeach()
    endif()
endif()

add_library(crumb_lib INTERFACE)
target_include_directories(crumb_lib INTERFACE src src/lib)
target_compile_features(crumb_lib INTERFACE cxx_std_23)

add_library(crumb_application
    src/application/reconcile_directory.cpp
    src/application/rebuild_search_index.cpp
    src/application/search_manifest.cpp
)
target_include_directories(crumb_application PUBLIC src src/lib)
target_link_libraries(crumb_application PUBLIC crumb_lib)

add_library(crumb_infrastructure
    src/infrastructure/persistence/toml_manifest_mapper.cpp
    src/infrastructure/persistence/toml_manifest_repository.cpp
    src/infrastructure/persistence/binary_search_index_repository.cpp
    src/infrastructure/filesystem/native_filesystem.cpp
    src/infrastructure/hashing/streaming_hash.cpp
    src/infrastructure/system/system_clock.cpp
    src/infrastructure/system/ulid_generator.cpp
)
target_include_directories(crumb_infrastructure PUBLIC src src/lib)
target_link_libraries(crumb_infrastructure PRIVATE crumb_lib ZLIB::ZLIB Threads::Threads)

add_library(crumb_google_drive
    src/plugins/google_drive/google_drive_plugin.cpp
)
target_include_directories(crumb_google_drive PUBLIC src src/lib)
target_link_libraries(crumb_google_drive PUBLIC crumb_application crumb_infrastructure)

add_library(crumb_boundary
    src/boundary/cli/command_router.cpp
    src/boundary/cli/search_tap.cpp
    src/boundary/cli/user_config.cpp
)
target_include_directories(crumb_boundary PUBLIC src src/lib)
target_link_libraries(crumb_boundary PRIVATE crumb_application crumb_google_drive)

add_executable(crumb src/main.cpp)
target_link_libraries(crumb PRIVATE crumb_boundary crumb_google_drive crumb_infrastructure)

find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy
    HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin)
if(CLANG_TIDY_EXECUTABLE)
    if(APPLE)
        execute_process(COMMAND xcrun --show-sdk-path
            OUTPUT_VARIABLE CMAKE_OSX_SYSROOT
            OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
    add_custom_target(lint
        COMMAND ${CLANG_TIDY_EXECUTABLE} -p ${CMAKE_BINARY_DIR}
                $<$<BOOL:${APPLE}>:--extra-arg-before=-isysroot>
                $<$<BOOL:${APPLE}>:--extra-arg-before=${CMAKE_OSX_SYSROOT}>
                ${CMAKE_SOURCE_DIR}/src/main.cpp
                ${CMAKE_SOURCE_DIR}/src/application/reconcile_directory.cpp
                ${CMAKE_SOURCE_DIR}/src/application/rebuild_search_index.cpp
                ${CMAKE_SOURCE_DIR}/src/application/search_manifest.cpp
                ${CMAKE_SOURCE_DIR}/src/boundary/cli/command_router.cpp
                ${CMAKE_SOURCE_DIR}/src/boundary/cli/search_tap.cpp
                ${CMAKE_SOURCE_DIR}/src/boundary/cli/user_config.cpp
                ${CMAKE_SOURCE_DIR}/src/infrastructure/filesystem/native_filesystem.cpp
                ${CMAKE_SOURCE_DIR}/src/infrastructure/hashing/streaming_hash.cpp
                ${CMAKE_SOURCE_DIR}/src/infrastructure/persistence/binary_search_index_repository.cpp
                ${CMAKE_SOURCE_DIR}/src/infrastructure/persistence/toml_manifest_mapper.cpp
                ${CMAKE_SOURCE_DIR}/src/infrastructure/persistence/toml_manifest_repository.cpp
                ${CMAKE_SOURCE_DIR}/src/infrastructure/system/system_clock.cpp
                ${CMAKE_SOURCE_DIR}/src/infrastructure/system/ulid_generator.cpp
                ${CMAKE_SOURCE_DIR}/src/plugins/google_drive/google_drive_plugin.cpp
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy"
        VERBATIM)
else()
    add_custom_target(lint
        COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy is required; install it with: brew install llvm"
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM)
endif()

file(GLOB_RECURSE CRUMB_SOURCE_FILES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_SOURCE_DIR}/tests/*.cpp")

find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format
    HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin)
if(CLANG_FORMAT_EXECUTABLE)
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_EXECUTABLE} --dry-run --Werror ${CRUMB_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Checking C++ formatting"
        VERBATIM)
else()
    add_custom_target(format-check
        COMMAND ${CMAKE_COMMAND} -E echo "clang-format is required; install it with: brew install llvm"
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM)
endif()

find_program(CPPCHECK_EXECUTABLE NAMES cppcheck)
if(CPPCHECK_EXECUTABLE)
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK_EXECUTABLE} --project=${CMAKE_BINARY_DIR}/compile_commands.json
                --enable=warning,performance,portability --error-exitcode=1
                --suppress=missingIncludeSystem --suppress=identicalConditionAfterEarlyExit
                --suppress=assertWithSideEffect
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running cppcheck"
        VERBATIM)
else()
    add_custom_target(cppcheck
        COMMAND ${CMAKE_COMMAND} -E echo "cppcheck is required; install it with: brew install cppcheck"
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM)
endif()

find_program(IWYU_TOOL_EXECUTABLE NAMES iwyu_tool.py iwyu_tool)
if(IWYU_TOOL_EXECUTABLE)
    add_custom_target(iwyu
        COMMAND ${IWYU_TOOL_EXECUTABLE} -p ${CMAKE_BINARY_DIR}
                -- -Xiwyu --no_comments
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running Include-What-You-Use"
        VERBATIM)
else()
    add_custom_target(iwyu
        COMMAND ${CMAKE_COMMAND} -E echo "Include-What-You-Use is required; install it with: brew install include-what-you-use"
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM)
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME crumb_runs COMMAND crumb)
    add_test(NAME crumb_config_error COMMAND ${CMAKE_COMMAND} -E env HOME= $<TARGET_FILE:crumb> scan .)
    set_tests_properties(crumb_config_error PROPERTIES WILL_FAIL TRUE)
    add_test(NAME crumb_interactive_table COMMAND sh -c
        "printf q | script -q /dev/null $<TARGET_FILE:crumb> search . crumb --table")
    set_tests_properties(crumb_interactive_table PROPERTIES WILL_FAIL TRUE)
    add_executable(crumb_manifest_tests tests/manifest_tests.cpp)
    target_link_libraries(crumb_manifest_tests PRIVATE crumb_infrastructure)
    add_test(NAME crumb_manifest_tests COMMAND crumb_manifest_tests)
    add_executable(crumb_search_tests tests/search_tests.cpp)
    target_link_libraries(crumb_search_tests PRIVATE crumb_application)
    add_test(NAME crumb_search_tests COMMAND crumb_search_tests)
    add_executable(crumb_search_tap_tests tests/search_tap_tests.cpp)
    target_link_libraries(crumb_search_tap_tests PRIVATE crumb_boundary)
    add_test(NAME crumb_search_tap_tests COMMAND crumb_search_tap_tests)
    add_executable(crumb_user_config_tests tests/user_config_tests.cpp)
    target_link_libraries(crumb_user_config_tests PRIVATE crumb_boundary)
    add_test(NAME crumb_user_config_tests COMMAND crumb_user_config_tests)
    add_executable(crumb_google_drive_tests tests/google_drive_tests.cpp)
    target_link_libraries(crumb_google_drive_tests PRIVATE crumb_google_drive)
    add_test(NAME crumb_google_drive_tests COMMAND crumb_google_drive_tests)
    add_executable(crumb_reconcile_tests tests/reconcile_tests.cpp)
    target_link_libraries(crumb_reconcile_tests PRIVATE crumb_application crumb_infrastructure)
    add_test(NAME crumb_reconcile_tests COMMAND crumb_reconcile_tests)
    add_executable(crumb_infrastructure_tests tests/infrastructure_tests.cpp)
    target_link_libraries(crumb_infrastructure_tests PRIVATE crumb_infrastructure)
    add_test(NAME crumb_infrastructure_tests COMMAND crumb_infrastructure_tests)
    add_executable(crumb_command_router_tests tests/command_router_tests.cpp)
    target_link_libraries(crumb_command_router_tests PRIVATE crumb_boundary crumb_google_drive)
    add_test(NAME crumb_command_router_tests COMMAND crumb_command_router_tests)
    add_executable(crumb_edge_case_tests tests/edge_case_tests.cpp)
    target_link_libraries(crumb_edge_case_tests PRIVATE crumb_application)
    add_test(NAME crumb_edge_case_tests COMMAND crumb_edge_case_tests)
    add_executable(crumb_boundary_internal_tests tests/boundary_internal_tests.cpp)
    target_link_libraries(crumb_boundary_internal_tests PRIVATE crumb_application crumb_google_drive crumb_infrastructure)
    add_test(NAME crumb_boundary_internal_tests COMMAND crumb_boundary_internal_tests)
    add_executable(crumb_persistence_internal_tests tests/persistence_internal_tests.cpp)
    target_link_libraries(crumb_persistence_internal_tests PRIVATE crumb_lib ZLIB::ZLIB Threads::Threads)
    add_test(NAME crumb_persistence_internal_tests COMMAND crumb_persistence_internal_tests)
    add_executable(crumb_plugin_internal_tests tests/plugin_internal_tests.cpp)
    target_link_libraries(crumb_plugin_internal_tests PRIVATE crumb_application crumb_infrastructure)
    add_test(NAME crumb_plugin_internal_tests COMMAND crumb_plugin_internal_tests)
endif()
