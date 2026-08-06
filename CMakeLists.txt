cmake_minimum_required(VERSION 3.15)
project(VBDE VERSION 1.0 LANGUAGES CXX)
# Flags for Neovim clangd LSP
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SRC_FILES CONFIGURE_DEPENDS "src/*.cpp")
add_executable(vbde ${SRC_FILES})

target_include_directories(vbde PRIVATE include)
