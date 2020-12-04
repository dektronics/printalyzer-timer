#include "usb_host.h"

#include <usbh_core.h>
#include <usbh_cdc.h>
#include <usbh_msc.h>
#include <usbh_hid.h>

#include <esp_log.h>

#include "keypad.h"

static const char *TAG = "main_task";

USBH_HandleTypeDef hUsbHostFS;
usb_app_state_t app_state = APPLICATION_IDLE;

static void usb_host_userprocess(USBH_HandleTypeDef *phost, uint8_t id);
static void usb_msc_active();
static void usb_msc_disconnect();

USBH_StatusTypeDef usb_host_init(void)
{
    ESP_LOGD(TAG, "usb_host_init");
    if (USBH_Init(&hUsbHostFS, usb_host_userprocess, HOST_FS) != USBH_OK) {
        ESP_LOGE(TAG, "USBH_Init fail");
        return USBH_FAIL;
    }
    if (USBH_RegisterClass(&hUsbHostFS, USBH_CDC_CLASS) != USBH_OK) {
        ESP_LOGE(TAG, "USBH_RegisterClass CDC fail");
        return USBH_FAIL;
    }
    if (USBH_RegisterClass(&hUsbHostFS, USBH_MSC_CLASS) != USBH_OK) {
        ESP_LOGE(TAG, "USBH_RegisterClass MSC fail");
        return USBH_FAIL;
    }
    if (USBH_RegisterClass(&hUsbHostFS, USBH_HID_CLASS) != USBH_OK) {
        ESP_LOGE(TAG, "USBH_RegisterClass HID fail");
        return USBH_FAIL;
    }
    if (USBH_Start(&hUsbHostFS) != USBH_OK) {
        ESP_LOGE(TAG, "USBH_Start fail");
        return USBH_FAIL;
    }
    ESP_LOGD(TAG, "usb_host_init complete");
    return USBH_OK;
}

static void usb_host_userprocess(USBH_HandleTypeDef *phost, uint8_t id)
{
    uint8_t active_class = USBH_GetActiveClass(phost);

    switch(id) {
    case HOST_USER_SELECT_CONFIGURATION:
        ESP_LOGI(TAG, "usb_host_userprocess: CONFIGURATION");
        break;
    case HOST_USER_DISCONNECTION:
        ESP_LOGI(TAG, "usb_host_userprocess: DISCONNECTION");
        app_state = APPLICATION_DISCONNECT;
        if (active_class == USB_MSC_CLASS) {
            usb_msc_disconnect();
        }
        break;
    case HOST_USER_CLASS_ACTIVE:
        ESP_LOGI(TAG, "usb_host_userprocess: ACTIVE");
        app_state = APPLICATION_READY;
        if (active_class == USB_MSC_CLASS) {
            usb_msc_active();
        }
        break;
    case HOST_USER_CLASS_SELECTED:
        ESP_LOGI(TAG, "usb_host_userprocess: SELECTED");
        break;
    case HOST_USER_CONNECTION:
        ESP_LOGI(TAG, "usb_host_userprocess: CONNECTION");
        app_state = APPLICATION_START;
        break;
    case HOST_USER_UNRECOVERED_ERROR:
        ESP_LOGI(TAG, "usb_host_userprocess: UNRECOVERED_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "usb_host_userprocess: %d", id);
        break;
    }
}

void USBH_HID_EventCallback(USBH_HandleTypeDef *phost)
{
    /*
     * This callback is defined with weak linkage in the USB Host library,
     * so we use it by declaring it here.
     */
    if(USBH_HID_GetDeviceType(phost) == HID_KEYBOARD) {
        uint8_t key;
        HID_KEYBD_Info_TypeDef *keyboard_info;
        keyboard_info = USBH_HID_GetKeybdInfo(phost);
        key = USBH_HID_GetASCIICode(keyboard_info);
        ESP_LOGD(TAG, "Keyboard: key='%c', code=0x%02X", key, keyboard_info->keys[0]);

        keypad_event_t keypad_event = {
            .key = 0,
            .pressed = true
        };
        switch (keyboard_info->keys[0]) {
        case KEY_UPARROW:
            keypad_event.key = KEYPAD_INC_EXPOSURE;
            keypad_inject_event(&keypad_event);
            break;
        case KEY_DOWNARROW:
            keypad_event.key = KEYPAD_DEC_EXPOSURE;
            keypad_inject_event(&keypad_event);
            break;
        case KEY_LEFTARROW:
            keypad_event.key = KEYPAD_DEC_CONTRAST;
            keypad_inject_event(&keypad_event);
            break;
        case KEY_RIGHTARROW:
            keypad_event.key = KEYPAD_INC_CONTRAST;
            keypad_inject_event(&keypad_event);
            break;
        case KEY_SPACEBAR:
            keypad_event.key = KEYPAD_FOCUS;
            keypad_inject_event(&keypad_event);
            break;
        case KEY_ENTER:
            keypad_event.key = KEYPAD_START;
            keypad_inject_event(&keypad_event);
            break;
        case KEY_A:
        case KEY_EQUAL_PLUS:
        case KEY_KEYPAD_PLUS:
            keypad_event.key = KEYPAD_ADD_ADJUSTMENT;
            keypad_inject_event(&keypad_event);
            break;
        case KEY_T:
        case KEY_8_ASTERISK:
            keypad_event.key = KEYPAD_TEST_STRIP;
            keypad_inject_event(&keypad_event);
            break;
        case KEY_F1:
            keypad_event.key = KEYPAD_MENU;
            keypad_inject_event(&keypad_event);
            break;
        case KEY_ESCAPE:
            keypad_event.key = KEYPAD_CANCEL;
            keypad_inject_event(&keypad_event);
            break;
        default:
            break;
        }
    }
}

void usb_msc_active()
{
    //TODO Mount the mass storage device
}

void usb_msc_disconnect()
{
    //TODO Cleanup from mass storage device disconnect
}
