cmake_minimum_required(VERSION 3.23)

project(crumb
    VERSION 0.1.0
    DESCRIPTION "A modern C++ project"
    LANGUAGES CXX
)

find_package(ZLIB REQUIRED)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(crumb_lib
    src/lib/application/reconcile_directory.cpp
    src/lib/application/move_file.cpp
    src/lib/application/recover_operations.cpp
    src/lib/application/search_manifest.cpp
)
target_include_directories(crumb_lib PUBLIC src src/lib)
target_compile_features(crumb_lib PUBLIC cxx_std_23)

add_library(crumb_boundary src/boundary/cli/command_router.cpp)
target_include_directories(crumb_boundary PUBLIC src src/lib)
target_link_libraries(crumb_boundary PRIVATE crumb_lib)

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
target_link_libraries(crumb_infrastructure PRIVATE crumb_lib ZLIB::ZLIB)

add_executable(crumb src/main.cpp)
target_link_libraries(crumb PRIVATE crumb_boundary crumb_infrastructure)

target_compile_options(crumb PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME crumb_runs COMMAND crumb)
    add_executable(crumb_manifest_tests tests/manifest_tests.cpp)
    target_link_libraries(crumb_manifest_tests PRIVATE crumb_infrastructure)
    add_test(NAME crumb_manifest_tests COMMAND crumb_manifest_tests)
    add_executable(crumb_search_tests tests/search_tests.cpp)
    target_link_libraries(crumb_search_tests PRIVATE crumb_lib)
    add_test(NAME crumb_search_tests COMMAND crumb_search_tests)
endif()
