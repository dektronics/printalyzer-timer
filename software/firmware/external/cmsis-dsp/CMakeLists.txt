cmake_minimum_required(VERSION 3.20)

add_library(cmsis-dsp STATIC IMPORTED GLOBAL)
set_property(TARGET cmsis-dsp PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/lib/libarm_cortexM4lf_math.a)
set_property(TARGET cmsis-dsp PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/include)
