add_library(cherryusb STATIC)

target_include_directories(cherryusb PUBLIC
    common
    core
    osal
    class/hub
    class/msc
)

if(CONFIG_CHERRYUSB_HOST)
    target_sources(cherryusb PRIVATE
        core/usbh_core.c
        class/hub/usbh_hub.c
        class/msc/usbh_msc.c
        port/dwc2/usb_hc_dwc2.c
        port/dwc2/usb_glue_st.c
    )
endif()

if(DEFINED CONFIG_CHERRYUSB_OSAL)
    if("${CONFIG_CHERRYUSB_OSAL}" STREQUAL "freertos")
    target_sources(cherryusb PRIVATE osal/usb_osal_freertos.c)
    endif()
endif()