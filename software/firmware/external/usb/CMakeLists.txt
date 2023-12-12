cmake_minimum_required(VERSION 3.20)

add_library(mw_usb_host
    core/src/usbh_ctlreq.c
    core/src/usbh_pipes.c
    core/src/usbh_core.c
    core/src/usbh_ioreq.c
    class/src/usbh_serial_ftdi.c
    class/src/usbh_msc.c
    class/src/usbh_cdc.c
    class/src/usbh_hid.c
    class/src/usbh_hid_keybd.c
    class/src/usbh_msc_bot.c
    class/src/usbh_msc_scsi.c
    class/src/usbh_hid_mouse.c
    class/src/usbh_serial_slcom.c
    class/src/usbh_serial_plcom.c
    class/src/usbh_hid_parser.c
    target/usbh_platform.c
    target/usbh_conf.c
)

target_include_directories(mw_usb_host PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/class/include
    ${CMAKE_CURRENT_SOURCE_DIR}/target
)

target_link_libraries(mw_usb_host PRIVATE
    cmsis
    hal
    freertos
)
