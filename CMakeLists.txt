cmake_minimum_required(VERSION 3.15)

project(VBDE VERSION 1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# -------------------------
# Library
# -------------------------

add_library(vectordb
    src/api/database.cpp
    src/storage/file_header.cpp
    src/storage/pager.cpp
)

add_library(vectordb::vectordb ALIAS vectordb)

target_include_directories(vectordb
    PUBLIC
        include

    PRIVATE
        src
)


# -------------------------
# Demo
# -------------------------

add_executable(vbde_demo
    examples/main.cpp
)

target_link_libraries(vbde_demo
    PRIVATE
        vectordb::vectordb
)


# -------------------------
# Tests
# -------------------------

enable_testing()

include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)

FetchContent_MakeAvailable(googletest)


add_executable(database_tests
    tests/database_open_test.cpp
)

target_link_libraries(database_tests
    PRIVATE
        vectordb::vectordb
        GTest::gtest_main
)


include(GoogleTest)

gtest_discover_tests(database_tests)