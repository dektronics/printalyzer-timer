cmake_minimum_required(VERSION 3.20)

add_library(freertos
    croutine.c
    event_groups.c
    list.c
    queue.c
    stream_buffer.c
    tasks.c
    timers.c
    portable/GCC/ARM_CM4F/port.c
    portable/GCC/ARM_CM4F/portmacro.h
    portable/MemMang/heap_4.c
    CMSIS_RTOS_V2/cmsis_os2.c
    CMSIS_RTOS_V2/cmsis_os2.h
    CMSIS_RTOS_V2/cmsis_os.h
    CMSIS_RTOS_V2/freertos_mpool.h
    CMSIS_RTOS_V2/freertos_os2.h
)

target_include_directories(freertos PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/portable/GCC/ARM_CM4F
    ${CMAKE_CURRENT_SOURCE_DIR}/CMSIS_RTOS_V2
)

target_link_libraries(freertos PRIVATE cmsis)
