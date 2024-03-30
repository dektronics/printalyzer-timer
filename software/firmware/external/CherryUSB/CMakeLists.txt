#
# Custom CMakeLists.txt for integration with the
# Printalyzer Enlarging Timer project.
#
# The upstream project has switched to providing a configurable include file,
# named "cherryusb.cmake". That file should be used as a master reference,
# but it is a lot easier to have a project specific CMakeLists here for
# integration purposes.
#

add_library(cherryusb STATIC)

target_include_directories(cherryusb PUBLIC
    common
    core
    osal
    class/cdc
    class/hid
    class/msc
    class/hub
)

target_sources(cherryusb PRIVATE
    core/usbh_core.c
    class/hub/usbh_hub.c
    class/hid/usbh_hid.c
    class/msc/usbh_msc.c
    port/dwc2/usb_hc_dwc2.c
    port/dwc2/usb_glue_st.c
    osal/usb_osal_freertos.c
)
