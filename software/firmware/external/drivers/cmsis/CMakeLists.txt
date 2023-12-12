cmake_minimum_required(VERSION 3.20)

add_library(cmsis INTERFACE)

target_include_directories(cmsis INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
