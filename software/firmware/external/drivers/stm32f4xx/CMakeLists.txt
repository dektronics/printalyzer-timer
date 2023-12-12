cmake_minimum_required(VERSION 3.20)

file(GLOB_RECURSE HAL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

add_library(hal
    ${HAL_SOURCES}
)

target_include_directories(hal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Legacy
)

target_link_libraries(hal PRIVATE cmsis)
