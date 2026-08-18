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
    src/catalog/catalog.cpp
    src/storage/buffer_manager.cpp
    src/storage/slotted_page.cpp
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
    tests/file_header_test.cpp
    tests/buffer_manager_test.cpp
    tests/slotted_page_test.cpp
)

target_link_libraries(database_tests
    PRIVATE
        vectordb::vectordb
        GTest::gtest_main
)

target_include_directories(database_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

include(GoogleTest)
gtest_discover_tests(database_tests)