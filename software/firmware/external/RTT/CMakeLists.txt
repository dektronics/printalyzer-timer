cmake_minimum_required(VERSION 3.20)

add_library(RTT
    src/SEGGER_RTT_ASM_ARMv7M.S
    src/SEGGER_RTT.c
    src/SEGGER_RTT_printf.c
)

target_include_directories(RTT PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
