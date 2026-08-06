cmake_minimum_required(VERSION 3.15)
project(VBDE VERSION 1.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    "src/*.cpp"
)

add_executable(vbde
    main.cpp
    ${SRC_FILES}
)

target_include_directories(vbde PRIVATE include src)
