#include "usb_host.h"

#include <usbh_core.h>
#include <usbh_cdc.h>
#include <usbh_msc.h>
#include <usbh_hid.h>
#include <usbh_serial_ftdi.h>

#include <ff.h>

#include <esp_log.h>

#include "keypad.h"
#include "fatfs.h"

static const char *TAG = "usb_host";

#define REPORT_KEY_NUM_LOCK 0x01
#define REPORT_KEY_CAPS_LOCK 0x02
#define REPORT_KEY_SCROLL_LOCK 0x04

USBH_HandleTypeDef hUsbHostFS;
static usb_app_state_t app_state = APPLICATION_IDLE;
static bool usb_msc_mounted = false;
static uint8_t usb_hid_keyboard_report_state = 0x00;
static HID_KEYBD_Info_TypeDef usb_hid_keyboard_last_event = {0};
static uint32_t usb_hid_keyboard_last_event_time = 0;

static void usb_host_userprocess(USBH_HandleTypeDef *phost, uint8_t id);
static void usb_msc_active();
static void usb_msc_disconnect();
static void usb_serial_ftdi_active(USBH_HandleTypeDef *phost);

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
    if (USBH_RegisterClass(&hUsbHostFS, USBH_VENDOR_SERIAL_FTDI_CLASS) != USBH_OK) {
        ESP_LOGE(TAG, "USBH_RegisterClass VENDOR_SERIAL_FTDI fail");
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
        } else if (active_class == USB_HID_CLASS && USBH_HID_GetDeviceType(phost) == HID_KEYBOARD) {
            usb_hid_keyboard_report_state = 0x00;
            memset(&usb_hid_keyboard_last_event, 0, sizeof(HID_KEYBD_Info_TypeDef));
            usb_hid_keyboard_last_event_time = 0;
        } else if (USBH_FTDI_IsDeviceType(phost)) {
            usb_serial_ftdi_active(phost);
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
        uint8_t report_state = usb_hid_keyboard_report_state;
        uint32_t event_time = HAL_GetTick();

        keyboard_info = USBH_HID_GetKeybdInfo(phost);

        /*
         * Sometimes we seem to receive the same event twice, in very quick
         * succession. This mostly happens with the CapsLock/NumLock keys
         * during the first press after startup, and it messes up our
         * handling of those keys. For now, the easy fix is to simply
         * detect and ignore such events.
         */
        if (memcmp(keyboard_info, &usb_hid_keyboard_last_event, sizeof(HID_KEYBD_Info_TypeDef)) == 0
            && (event_time - usb_hid_keyboard_last_event_time) < 20) {
            return;
        } else {
            memcpy(&usb_hid_keyboard_last_event, keyboard_info, sizeof(HID_KEYBD_Info_TypeDef));
            usb_hid_keyboard_last_event_time = event_time;
        }

        key = USBH_HID_GetASCIICode(keyboard_info);

        if (key >= 32 && key <= 126) {
            ESP_LOGD(TAG, "Keyboard: key='%c', code={%02X,%02X,%02X,%02X,%02X,%02X}",
                key, keyboard_info->keys[0], keyboard_info->keys[1], keyboard_info->keys[2],
                keyboard_info->keys[3], keyboard_info->keys[4], keyboard_info->keys[5]);
        } else {
            ESP_LOGD(TAG, "Keyboard: key=0x%02X, code={%02X,%02X,%02X,%02X,%02X,%02X}",
                key, keyboard_info->keys[0], keyboard_info->keys[1], keyboard_info->keys[2],
                keyboard_info->keys[3], keyboard_info->keys[4], keyboard_info->keys[5]);
        }

        if (keyboard_info->keys[0] == KEY_KEYPAD_NUM_LOCK_AND_CLEAR) {
            report_state ^= REPORT_KEY_NUM_LOCK;
        } else if (keyboard_info->keys[0] == KEY_CAPS_LOCK) {
            report_state ^= REPORT_KEY_CAPS_LOCK;
        } else if (keyboard_info->keys[0] == KEY_SCROLL_LOCK) {
            report_state ^= REPORT_KEY_SCROLL_LOCK;
        }

        if (report_state != usb_hid_keyboard_report_state) {
            USBH_StatusTypeDef res = USBH_BUSY;
            do {
                res = USBH_HID_SetReport(phost,
                    0x02 /*reportType*/,
                    0x00 /*reportId*/,
                    &report_state, 1);
            } while(res == USBH_BUSY);

            if (res == USBH_OK) {
                usb_hid_keyboard_report_state = report_state;
            }
        } else {
            /*
             * Toggle the state of the shift keys based on the state
             * of CapsLock so the correct character is generated.
             */
            if ((report_state & REPORT_KEY_CAPS_LOCK) != 0
                && ((key >= 'A' && key <= 'Z') || (key >= 'a' && key <= 'z'))) {
                if ((keyboard_info->lshift == 1U) || (keyboard_info->rshift)) {
                    keyboard_info->lshift = 0;
                } else {
                    keyboard_info->lshift = 1;
                }
                keyboard_info->rshift = 0;
                key = USBH_HID_GetASCIICode(keyboard_info);
            }

            keypad_event_t keypad_event = {
                .key = KEYPAD_USB_KEYBOARD,
                .pressed = true,
                .keypad_state = ((uint16_t)keyboard_info->keys[0] << 8) | key
            };
            keypad_inject_event(&keypad_event);
        }
    }
}

void usb_msc_active()
{
    ESP_LOGD(TAG, "usb_msc_active");

    if (fatfs_mount() != HAL_OK) {
        return;
    }
    usb_msc_mounted = true;

    FRESULT res;

    char str[12];
    res = f_getlabel("0:", str, 0);
    if (res == FR_OK) {
        ESP_LOGI(TAG, "Drive label: \"%s\"", str);
    }

    FATFS *fs;
    DWORD free_clusters, free_sectors, total_sectors;
    res = f_getfree("0:", &free_clusters, &fs);
    if (res == FR_OK) {
        total_sectors = (fs->n_fatent - 2) * fs->csize;
        free_sectors = free_clusters * fs->csize;
        ESP_LOGI(TAG, "Drive has %ld KB total space, %ld KB available", total_sectors / 2, free_sectors / 2);
    }
}

void usb_msc_disconnect()
{
    ESP_LOGD(TAG, "usb_msc_disconnect");
    fatfs_unmount();
    usb_msc_mounted = false;
}

bool usb_msc_is_mounted()
{
    return usb_msc_mounted;
}

void usb_serial_ftdi_active(USBH_HandleTypeDef *phost)
{
    ESP_LOGD(TAG, "usb_serial_ftdi_active");

    USBH_StatusTypeDef status;

    do {
        status = USBH_FTDI_SetBaudRate(phost, 9600);
    } while (status == USBH_BUSY);
    if (status != USBH_OK) {
        USBH_ErrLog("FTDI: USBH_FTDI_SetBaudRate failed: %d", status);
        return;
    }

    do {
        status = USBH_FTDI_SetData(phost, FTDI_SIO_SET_DATA_BITS(8)
            | FTDI_SIO_SET_DATA_PARITY_NONE | FTDI_SIO_SET_DATA_STOP_BITS_1);
    } while (status == USBH_BUSY);
    if (status != USBH_OK) {
        USBH_ErrLog("FTDI: USBH_FTDI_SetData failed: %d", status);
        return;
    }

    do {
        status = USBH_FTDI_SetFlowControl(phost, FTDI_SIO_DISABLE_FLOW_CTRL);
    } while (status == USBH_BUSY);
    if (status != USBH_OK) {
        USBH_ErrLog("FTDI: USBH_FTDI_SetFlowControl failed: %d", status);
        return;
    }

    do {
        status = USBH_FTDI_SetDtr(phost, false);
    } while (status == USBH_BUSY);
    if (status != USBH_OK) {
        USBH_ErrLog("FTDI: USBH_FTDI_SetDtr failed: %d", status);
        return;
    }

    do {
        status = USBH_FTDI_SetRts(phost, false);
    } while (status == USBH_BUSY);
    if (status != USBH_OK) {
        USBH_ErrLog("FTDI: USBH_FTDI_SetRts failed: %d", status);
        return;
    }
}

void USBH_FTDI_TransmitCallback(USBH_HandleTypeDef *phost)
{
}

void USBH_FTDI_ReceiveCallback(USBH_HandleTypeDef *phost, uint8_t *data, size_t length)
{
}
