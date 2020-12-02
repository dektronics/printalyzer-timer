#include "usb_host.h"

#include <usbh_core.h>
#include <usbh_cdc.h>
#include <usbh_msc.h>
#include <usbh_hid.h>

USBH_HandleTypeDef hUsbHostFS;
usb_app_state_t app_state = APPLICATION_IDLE;

static void usb_host_userprocess(USBH_HandleTypeDef *phost, uint8_t id);
static void usb_msc_active();
static void usb_msc_disconnect();

USBH_StatusTypeDef usb_host_init(void)
{
    if (USBH_Init(&hUsbHostFS, usb_host_userprocess, HOST_FS) != USBH_OK) {
        return USBH_FAIL;
    }
    if (USBH_RegisterClass(&hUsbHostFS, USBH_CDC_CLASS) != USBH_OK) {
        return USBH_FAIL;
    }
    if (USBH_RegisterClass(&hUsbHostFS, USBH_MSC_CLASS) != USBH_OK) {
        return USBH_FAIL;
    }
    if (USBH_RegisterClass(&hUsbHostFS, USBH_HID_CLASS) != USBH_OK) {
        return USBH_FAIL;
    }
    if (USBH_Start(&hUsbHostFS) != USBH_OK) {
        return USBH_FAIL;
    }
    return USBH_OK;
}

static void usb_host_userprocess(USBH_HandleTypeDef *phost, uint8_t id)
{
    uint8_t active_class = USBH_GetActiveClass(phost);

    switch(id) {
    case HOST_USER_SELECT_CONFIGURATION:
        break;
    case HOST_USER_DISCONNECTION:
        app_state = APPLICATION_DISCONNECT;
        if (active_class == USB_MSC_CLASS) {
            usb_msc_disconnect();
        }
        break;
    case HOST_USER_CLASS_ACTIVE:
        app_state = APPLICATION_READY;
        if (active_class == USB_MSC_CLASS) {
            usb_msc_active();
        }
        break;
    case HOST_USER_CLASS_SELECTED:
        break;
    case HOST_USER_CONNECTION:
        app_state = APPLICATION_START;
        break;
    case HOST_USER_UNRECOVERED_ERROR:
        break;
    default:
        break;
    }
}

void USBH_HID_EventCallback(USBH_HandleTypeDef *phost)
{
    /*
     * This callback is defined with weak linkage in the USB Host library,
     * so we use it by declaring it here.
     */
    //TODO handle events from HID devices
}

void usb_msc_active()
{
    //TODO Mount the mass storage device
}

void usb_msc_disconnect()
{
    //TODO Cleanup from mass storage device disconnect
}
