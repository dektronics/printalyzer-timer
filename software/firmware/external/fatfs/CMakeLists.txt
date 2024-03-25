cmake_minimum_required(VERSION 3.20)

add_library(fatfs
    src/ff_gen_drv.c
    src/diskio.c
    src/ff.c
    src/ffsystem_cmsis_os.c
    src/ffunicode.c
    target/usbh_diskio.c
)

target_include_directories(fatfs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/target
)

target_link_libraries(fatfs PRIVATE
    cmsis
    hal
    freertos
    cherryusb
)
