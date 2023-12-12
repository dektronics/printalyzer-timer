cmake_minimum_required(VERSION 3.20)

add_library(easylogger
    src/elog_async.c
    src/elog_buf.c
    src/elog.c
    src/elog_port.c
    src/elog_utils.c
)

target_include_directories(easylogger PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(easylogger PRIVATE
    cmsis
    hal
    freertos
)
