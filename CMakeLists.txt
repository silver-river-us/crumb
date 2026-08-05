cmake_minimum_required(VERSION 4.0)

project(crumb
    VERSION 0.1.0
    DESCRIPTION "A modern C++ project"
    LANGUAGES CXX
)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(crumb src/main.cpp)

target_compile_features(crumb PRIVATE cxx_std_23)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(crumb PRIVATE -Wall -Wextra -Wpedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(crumb PRIVATE /W4)
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME crumb_runs COMMAND crumb)
endif()
