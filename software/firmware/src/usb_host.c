#include "usb_host.h"

#include <usbh_core.h>
#include <usbh_cdc.h>
#include <usbh_msc.h>
#include <usbh_hid.h>
#include <usbh_serial_ftdi.h>
#include <usbh_serial_plcom.h>
#include <usbh_serial_slcom.h>
#include <ff.h>

#include <esp_log.h>

#include "keypad.h"
#include "fatfs.h"

static const char *TAG = "usb_host";

#define MIN(a,b) (((a)<(b))?(a):(b))

#define REPORT_KEY_NUM_LOCK 0x01
#define REPORT_KEY_CAPS_LOCK 0x02
#define REPORT_KEY_SCROLL_LOCK 0x04

#define SERIAL_RECV_BUF_SIZE 256

typedef enum {
    USB_SERIAL_DRIVER_NONE = 0,
    USB_SERIAL_DRIVER_FTDI,
    USB_SERIAL_DRIVER_PLCOM,
    USB_SERIAL_DRIVER_SLCOM
} usb_serial_driver_t;

USBH_HandleTypeDef hUsbHostFS;
static usb_app_state_t app_state = APPLICATION_IDLE;
static bool usb_msc_mounted = false;
static uint8_t usb_hid_keyboard_report_state = 0x00;
static HID_KEYBD_Info_TypeDef usb_hid_keyboard_last_event = {0};
static uint32_t usb_hid_keyboard_last_event_time = 0;

static osMutexId_t usb_serial_mutex = NULL;
static const osMutexAttr_t usb_serial_mutex_attributes = {
  .name = "usb_serial_mutex"
};
static osSemaphoreId_t usb_serial_transmit_semaphore = NULL;
static const osSemaphoreAttr_t usb_serial_transmit_semaphore_attributes = {
    .name = "usb_serial_transmit_semaphore"
};
static osSemaphoreId_t usb_serial_receive_semaphore = NULL;
static const osSemaphoreAttr_t usb_serial_receive_semaphore_attributes = {
    .name = "usb_serial_receive_semaphore"
};
static usb_serial_driver_t usb_serial_active_driver = USB_SERIAL_DRIVER_NONE;
static usb_serial_line_coding_t usb_serial_line_coding = {
    .baud_rate = 9600,
    .data_bits = USB_SERIAL_DATA_BITS_8,
    .stop_bits = USB_SERIAL_STOP_BITS_1,
    .parity = USB_SERIAL_PARITY_NONE
};
static uint8_t usb_serial_recv_buf[SERIAL_RECV_BUF_SIZE];
static size_t usb_serial_recv_buf_len = 0;

static void usb_host_userprocess(USBH_HandleTypeDef *phost, uint8_t id);
static void usb_msc_active();
static void usb_msc_disconnect();

static void usb_serial_ftdi_active(USBH_HandleTypeDef *phost);
static uint8_t usb_serial_ftdi_convert_data(const usb_serial_line_coding_t *line_coding);
static USBH_StatusTypeDef usb_serial_ftdi_set_line_coding(USBH_HandleTypeDef *phost, const usb_serial_line_coding_t *line_coding);

static void usb_serial_plcom_active(USBH_HandleTypeDef *phost);
static void usb_serial_plcom_convert_line_coding(CDC_LineCodingTypeDef *cdc_linecoding, const usb_serial_line_coding_t *line_coding);
static USBH_StatusTypeDef usb_serial_plcom_set_line_coding(USBH_HandleTypeDef *phost, const usb_serial_line_coding_t *line_coding);

static void usb_serial_slcom_active(USBH_HandleTypeDef *phost);
static uint16_t usb_serial_slcom_convert_line_control(const usb_serial_line_coding_t *line_coding);
static USBH_StatusTypeDef usb_serial_slcom_set_line_coding(USBH_HandleTypeDef *phost, const usb_serial_line_coding_t *line_coding);

static void usb_serial_transmit_callback(USBH_HandleTypeDef *phost);
static void usb_serial_receive_callback(USBH_HandleTypeDef *phost, uint8_t *data, size_t length);

USBH_StatusTypeDef usb_host_init(void)
{
    ESP_LOGD(TAG, "usb_host_init");

    usb_serial_mutex = osMutexNew(&usb_serial_mutex_attributes);
    if (!usb_serial_mutex) {
        ESP_LOGE(TAG, "osMutexNew error");
        return USBH_FAIL;
    }

    usb_serial_transmit_semaphore = osSemaphoreNew(1, 0, &usb_serial_transmit_semaphore_attributes);
    if (!usb_serial_mutex) {
        ESP_LOGE(TAG, "osSemaphoreNew error");
        return USBH_FAIL;
    }

    usb_serial_receive_semaphore = osSemaphoreNew(1, 0, &usb_serial_receive_semaphore_attributes);
    if (!usb_serial_mutex) {
        ESP_LOGE(TAG, "osSemaphoreNew error");
        return USBH_FAIL;
    }

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
    if (USBH_RegisterClass(&hUsbHostFS, USBH_VENDOR_SERIAL_PLCOM_CLASS) != USBH_OK) {
        ESP_LOGE(TAG, "USBH_RegisterClass VENDOR_SERIAL_PLCOM fail");
        return USBH_FAIL;
    }
    if (USBH_RegisterClass(&hUsbHostFS, USBH_VENDOR_SERIAL_SLCOM_CLASS) != USBH_OK) {
        ESP_LOGE(TAG, "USBH_RegisterClass VENDOR_SERIAL_SLCOM fail");
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
        } else if (active_class == 0xFFU) { /* VENDOR class */
            osMutexAcquire(usb_serial_mutex, portMAX_DELAY);
            usb_serial_active_driver = USB_SERIAL_DRIVER_NONE;
            memset(usb_serial_recv_buf, 0, sizeof(usb_serial_recv_buf));
            usb_serial_recv_buf_len = 0;
            osMutexRelease(usb_serial_mutex);
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
        } else if (active_class == 0xFFU) { /* VENDOR class */
            osMutexAcquire(usb_serial_mutex, portMAX_DELAY);

            /* Clear out the receive buffer */
            memset(usb_serial_recv_buf, 0, sizeof(usb_serial_recv_buf));
            usb_serial_recv_buf_len = 0;

            if (USBH_FTDI_IsDeviceType(phost)) {
                usb_serial_active_driver = USB_SERIAL_DRIVER_FTDI;
                usb_serial_ftdi_active(phost);
            } else if (USBH_PLCOM_IsDeviceType(phost)) {
                usb_serial_active_driver = USB_SERIAL_DRIVER_PLCOM;
                usb_serial_plcom_active(phost);
            } else if (USBH_SLCOM_IsDeviceType(phost)) {
                usb_serial_active_driver = USB_SERIAL_DRIVER_SLCOM;
                usb_serial_slcom_active(phost);
            }
            osMutexRelease(usb_serial_mutex);
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

    status = usb_serial_ftdi_set_line_coding(phost, &usb_serial_line_coding);
    if (status != USBH_OK) {
        ESP_LOGE(TAG, "usb_serial_ftdi_set_line_coding failed: %d", status);
        return;
    }

    do {
        status = USBH_FTDI_SetFlowControl(phost, FTDI_SIO_DISABLE_FLOW_CTRL);
    } while (status == USBH_BUSY);
    if (status != USBH_OK) {
        ESP_LOGE(TAG, "USBH_FTDI_SetFlowControl failed: %d", status);
        return;
    }

    do {
        status = USBH_FTDI_SetDtr(phost, false);
    } while (status == USBH_BUSY);
    if (status != USBH_OK) {
        ESP_LOGE(TAG, "USBH_FTDI_SetDtr failed: %d", status);
        return;
    }

    do {
        status = USBH_FTDI_SetRts(phost, false);
    } while (status == USBH_BUSY);
    if (status != USBH_OK) {
        ESP_LOGE(TAG, "USBH_FTDI_SetRts failed: %d", status);
        return;
    }

    ESP_LOGD(TAG, "FTDI: active");
}

uint8_t usb_serial_ftdi_convert_data(const usb_serial_line_coding_t *line_coding)
{
    uint8_t data = 0;

    switch (line_coding->data_bits) {
    case USB_SERIAL_DATA_BITS_5:
        data |= FTDI_SIO_SET_DATA_BITS(5);
        break;
    case USB_SERIAL_DATA_BITS_6:
        data |= FTDI_SIO_SET_DATA_BITS(6);
        break;
    case USB_SERIAL_DATA_BITS_7:
        data |= FTDI_SIO_SET_DATA_BITS(7);
        break;
    case USB_SERIAL_DATA_BITS_8:
        data |= FTDI_SIO_SET_DATA_BITS(8);
        break;
    default:
        data |= FTDI_SIO_SET_DATA_BITS(8);
        break;
    }

    switch (line_coding->stop_bits) {
    case USB_SERIAL_STOP_BITS_1:
        data |= FTDI_SIO_SET_DATA_STOP_BITS_1;
        break;
    case USB_SERIAL_STOP_BITS_1_5:
        data |= FTDI_SIO_SET_DATA_STOP_BITS_15;
        break;
    case USB_SERIAL_STOP_BITS_2:
        data |= FTDI_SIO_SET_DATA_STOP_BITS_2;
        break;
    default:
        data |= FTDI_SIO_SET_DATA_STOP_BITS_1;
        break;
    }

    switch (line_coding->parity) {
    case USB_SERIAL_PARITY_NONE:
        data |= FTDI_SIO_SET_DATA_PARITY_NONE;
        break;
    case USB_SERIAL_PARITY_ODD:
        data |= FTDI_SIO_SET_DATA_PARITY_ODD;
        break;
    case USB_SERIAL_PARITY_EVEN:
        data |= FTDI_SIO_SET_DATA_PARITY_EVEN;
        break;
    case USB_SERIAL_PARITY_MARK:
        data |= FTDI_SIO_SET_DATA_PARITY_MARK;
        break;
    case USB_SERIAL_PARITY_SPACE:
        data |= FTDI_SIO_SET_DATA_PARITY_SPACE;
        break;
    default:
        data |= FTDI_SIO_SET_DATA_PARITY_NONE;
        break;
    }

    return data;
}

USBH_StatusTypeDef usb_serial_ftdi_set_line_coding(USBH_HandleTypeDef *phost, const usb_serial_line_coding_t *line_coding)
{
    USBH_StatusTypeDef status;

    do {
        status = USBH_FTDI_SetBaudRate(phost, line_coding->baud_rate);
    } while (status == USBH_BUSY);
    if (status != USBH_OK) {
        ESP_LOGE(TAG, "USBH_FTDI_SetBaudRate failed: %d", status);
        return status;
    }

    uint8_t data_val = usb_serial_ftdi_convert_data(line_coding);
    do {
        status = USBH_FTDI_SetData(phost, data_val);
    } while (status == USBH_BUSY);
    if (status != USBH_OK) {
        ESP_LOGE(TAG, "USBH_FTDI_SetData failed: %d", status);
        return status;
    }

    return status;
}

void usb_serial_plcom_active(USBH_HandleTypeDef *phost)
{
    ESP_LOGD(TAG, "usb_serial_plcom_active");

    USBH_StatusTypeDef status;

    status = usb_serial_plcom_set_line_coding(phost, &usb_serial_line_coding);
    if (status != USBH_OK) {
        ESP_LOGE(TAG, "usb_serial_plcom_set_line_coding failed: %d", status);
        return;
    }

    do {
        status = USBH_PLCOM_SetRtsCts(phost, false);
    } while (status == USBH_BUSY);
    if (status != USBH_OK) {
        ESP_LOGE(TAG, "USBH_PLCOM_SetRtsCts failed: %d", status);
        return;
    }

    do {
        status = USBH_PLCOM_SetDtr(phost, false);
    } while (status == USBH_BUSY);
    if (status != USBH_OK) {
        ESP_LOGE(TAG, "USBH_PLCOM_SetDtr failed: %d", status);
        return;
    }

    do {
        status = USBH_PLCOM_SetRts(phost, false);
    } while (status == USBH_BUSY);
    if (status != USBH_OK) {
        ESP_LOGE(TAG, "USBH_PLCOM_SetRts failed: %d", status);
        return;
    }

    ESP_LOGD(TAG, "PLCOM: active");
}

void usb_serial_plcom_convert_line_coding(CDC_LineCodingTypeDef *cdc_linecoding, const usb_serial_line_coding_t *line_coding)
{
    cdc_linecoding->b.dwDTERate = line_coding->baud_rate;

    switch (line_coding->data_bits) {
    case USB_SERIAL_DATA_BITS_5:
        cdc_linecoding->b.bDataBits = 5;
        break;
    case USB_SERIAL_DATA_BITS_6:
        cdc_linecoding->b.bDataBits = 6;
        break;
    case USB_SERIAL_DATA_BITS_7:
        cdc_linecoding->b.bDataBits = 7;
        break;
    case USB_SERIAL_DATA_BITS_8:
        cdc_linecoding->b.bDataBits = 8;
        break;
    default:
        cdc_linecoding->b.bDataBits = 8;
        break;
    }

    switch (line_coding->stop_bits) {
    case USB_SERIAL_STOP_BITS_1:
        cdc_linecoding->b.bCharFormat = 0;
        break;
    case USB_SERIAL_STOP_BITS_1_5:
        cdc_linecoding->b.bCharFormat = 1;
        break;
    case USB_SERIAL_STOP_BITS_2:
        cdc_linecoding->b.bCharFormat = 2;
        break;
    default:
        cdc_linecoding->b.bCharFormat = 0;
        break;
    }

    switch (line_coding->parity) {
    case USB_SERIAL_PARITY_NONE:
        cdc_linecoding->b.bParityType = 0;
        break;
    case USB_SERIAL_PARITY_ODD:
        cdc_linecoding->b.bParityType = 1;
        break;
    case USB_SERIAL_PARITY_EVEN:
        cdc_linecoding->b.bParityType = 2;
        break;
    case USB_SERIAL_PARITY_MARK:
        cdc_linecoding->b.bParityType = 3;
        break;
    case USB_SERIAL_PARITY_SPACE:
        cdc_linecoding->b.bParityType = 4;
        break;
    default:
        cdc_linecoding->b.bParityType = 0;
        break;
    }
}

USBH_StatusTypeDef usb_serial_plcom_set_line_coding(USBH_HandleTypeDef *phost, const usb_serial_line_coding_t *line_coding)
{
    USBH_StatusTypeDef status;
    CDC_LineCodingTypeDef cdc_linecoding;

    usb_serial_plcom_convert_line_coding(&cdc_linecoding, line_coding);
    do {
        status = USBH_PLCOM_SetLineCoding(phost, &cdc_linecoding);
    } while (status == USBH_BUSY);

    return status;
}

void usb_serial_slcom_active(USBH_HandleTypeDef *phost)
{
    ESP_LOGD(TAG, "usb_serial_slcom_active");

    USBH_StatusTypeDef status;

    do {
        status = USBH_SLCOM_Open(phost);
    } while (status == USBH_BUSY);
    if (status != USBH_OK) {
        ESP_LOGE(TAG, "USBH_SLCOM_Open failed: %d", status);
        return;
    }

    status = usb_serial_slcom_set_line_coding(phost, &usb_serial_line_coding);
    if (status != USBH_OK) {
        ESP_LOGE(TAG, "usb_serial_slcom_set_line_coding failed: %d", status);
        return;
    }

    do {
        status = USBH_SLCOM_SetRtsCts(phost, false);
    } while (status == USBH_BUSY);
    if (status != USBH_OK) {
        ESP_LOGE(TAG, "USBH_SLCOM_SetRtsCts failed: %d", status);
        return;
    }

    do {
        status = USBH_SLCOM_SetDtr(phost, false);
    } while (status == USBH_BUSY);
    if (status != USBH_OK) {
        ESP_LOGE(TAG, "USBH_SLCOM_SetDtr failed: %d", status);
        return;
    }

    do {
        status = USBH_SLCOM_SetRts(phost, false);
    } while (status == USBH_BUSY);
    if (status != USBH_OK) {
        ESP_LOGE(TAG, "USBH_SLCOM_SetRts failed: %d", status);
        return;
    }

    ESP_LOGD(TAG, "SLCOM: active");
}

uint16_t usb_serial_slcom_convert_line_control(const usb_serial_line_coding_t *line_coding)
{
    uint16_t line_control = 0;

    switch (line_coding->data_bits) {
    case USB_SERIAL_DATA_BITS_5:
        line_control |= SLCOM_SET_DATA_BITS(5);
        break;
    case USB_SERIAL_DATA_BITS_6:
        line_control |= SLCOM_SET_DATA_BITS(6);
        break;
    case USB_SERIAL_DATA_BITS_7:
        line_control |= SLCOM_SET_DATA_BITS(7);
        break;
    case USB_SERIAL_DATA_BITS_8:
        line_control |= SLCOM_SET_DATA_BITS(8);
        break;
    default:
        line_control |= SLCOM_SET_DATA_BITS(8);
        break;
    }

    switch (line_coding->stop_bits) {
    case USB_SERIAL_STOP_BITS_1:
        line_control |= SLCOM_STOP_BITS_1;
        break;
    case USB_SERIAL_STOP_BITS_1_5:
        line_control |= SLCOM_STOP_BITS_15;
        break;
    case USB_SERIAL_STOP_BITS_2:
        line_control |= SLCOM_STOP_BITS_2;
        break;
    default:
        line_control |= SLCOM_STOP_BITS_1;
        break;
    }

    switch (line_coding->parity) {
    case USB_SERIAL_PARITY_NONE:
        line_control |= SLCOM_PARITY_NONE;
        break;
    case USB_SERIAL_PARITY_ODD:
        line_control |= SLCOM_PARITY_ODD;
        break;
    case USB_SERIAL_PARITY_EVEN:
        line_control |= SLCOM_PARITY_EVEN;
        break;
    case USB_SERIAL_PARITY_MARK:
        line_control |= SLCOM_PARITY_MARK;
        break;
    case USB_SERIAL_PARITY_SPACE:
        line_control |= SLCOM_PARITY_SPACE;
        break;
    default:
        line_control |= SLCOM_PARITY_NONE;
        break;
    }

    return line_control;
}

USBH_StatusTypeDef usb_serial_slcom_set_line_coding(USBH_HandleTypeDef *phost, const usb_serial_line_coding_t *line_coding)
{
    USBH_StatusTypeDef status;

    do {
        status = USBH_SLCOM_SetBaudRate(phost, line_coding->baud_rate);
    } while (status == USBH_BUSY);
    if (status != USBH_OK) {
        ESP_LOGE(TAG, "USBH_SLCOM_SetBaudRate failed: %d", status);
        return status;
    }

    uint16_t linecontrol = usb_serial_slcom_convert_line_control(line_coding);
    do {
        status = USBH_SLCOM_SetLineControl(phost, linecontrol);
    } while (status == USBH_BUSY);
    if (status != USBH_OK) {
        ESP_LOGE(TAG, "USBH_SLCOM_SetLineControl failed: %d", status);
        return status;
    }

    return status;
}

bool usb_serial_is_attached()
{
    bool result;

    osMutexAcquire(usb_serial_mutex, portMAX_DELAY);

    result = usb_serial_active_driver != USB_SERIAL_DRIVER_NONE;

    osMutexRelease(usb_serial_mutex);

    return result;
}

USBH_StatusTypeDef usb_serial_set_line_coding(const usb_serial_line_coding_t *line_coding)
{
    USBH_StatusTypeDef status;

    if (!line_coding) {
        return USBH_FAIL;
    }

    /*
     * Validate the line coding parameters so we can safely skip
     * dealing with a lot of this in the per-driver implementations.
     */

    /* Only support standard baud rates all adapters are likely to handle */
    switch (line_coding->baud_rate) {
    case 300:
    case 600:
    case 1200:
    case 2400:
    case 4800:
    case 9600:
    case 19200:
    case 38400:
    case 57600:
    case 115200:
        break;
    default:
        ESP_LOGW(TAG, "Invalid baud rate: %lu", line_coding->baud_rate);
        return USBH_NOT_SUPPORTED;
    }

    switch (line_coding->data_bits) {
    case USB_SERIAL_DATA_BITS_5:
    case USB_SERIAL_DATA_BITS_6:
    case USB_SERIAL_DATA_BITS_7:
    case USB_SERIAL_DATA_BITS_8:
        break;
    default:
        ESP_LOGW(TAG, "Invalid data bits: %d", line_coding->data_bits);
        return USBH_NOT_SUPPORTED;
    }

    switch (line_coding->stop_bits) {
    case USB_SERIAL_STOP_BITS_1:
    case USB_SERIAL_STOP_BITS_1_5:
    case USB_SERIAL_STOP_BITS_2:
        break;
    default:
        ESP_LOGW(TAG, "Invalid stop bits: %d", line_coding->stop_bits);
        return USBH_NOT_SUPPORTED;
    }

    switch (line_coding->parity) {
    case USB_SERIAL_PARITY_NONE:
    case USB_SERIAL_PARITY_ODD:
    case USB_SERIAL_PARITY_EVEN:
    case USB_SERIAL_PARITY_MARK:
    case USB_SERIAL_PARITY_SPACE:
        break;
    default:
        ESP_LOGW(TAG, "Invalid parity: %d", line_coding->parity);
        return USBH_NOT_SUPPORTED;
    }

    osMutexAcquire(usb_serial_mutex, portMAX_DELAY);

    /* If serial is already attached, change active settings */
    switch (usb_serial_active_driver) {
    case USB_SERIAL_DRIVER_FTDI:
        status = usb_serial_ftdi_set_line_coding(&hUsbHostFS, line_coding);
        break;
    case USB_SERIAL_DRIVER_PLCOM:
        status = usb_serial_plcom_set_line_coding(&hUsbHostFS, line_coding);
        break;
    case USB_SERIAL_DRIVER_SLCOM:
        status = usb_serial_slcom_set_line_coding(&hUsbHostFS, line_coding);
        break;
    case USB_SERIAL_DRIVER_NONE:
    default:
        status = USBH_OK;
        break;
    }

    memcpy(&usb_serial_line_coding, line_coding, sizeof(usb_serial_line_coding_t));

    osMutexRelease(usb_serial_mutex);

    return status;
}

USBH_StatusTypeDef usb_serial_get_line_coding(usb_serial_line_coding_t *line_coding)
{
    if (!line_coding) {
        return USBH_FAIL;
    }

    osMutexAcquire(usb_serial_mutex, portMAX_DELAY);

    memcpy(line_coding, &usb_serial_line_coding, sizeof(usb_serial_line_coding_t));

    osMutexRelease(usb_serial_mutex);

    return USBH_OK;
}

USBH_StatusTypeDef usb_serial_transmit(const uint8_t *buf, size_t length)
{
    USBH_StatusTypeDef status;

    if (!buf || length == 0) {
        return USBH_FAIL;
    }

    if (app_state != APPLICATION_READY) {
        return USBH_FAIL;
    }

    osMutexAcquire(usb_serial_mutex, portMAX_DELAY);

    do {
        switch (usb_serial_active_driver) {
        case USB_SERIAL_DRIVER_FTDI:
            status = USBH_FTDI_Transmit(&hUsbHostFS, buf, length);
            break;
        case USB_SERIAL_DRIVER_PLCOM:
            status = USBH_PLCOM_Transmit(&hUsbHostFS, buf, length);
            break;
        case USB_SERIAL_DRIVER_SLCOM:
            status = USBH_SLCOM_Transmit(&hUsbHostFS, buf, length);
            break;
        case USB_SERIAL_DRIVER_NONE:
        default:
            status = USBH_NOT_SUPPORTED;
            break;
        }
    } while (status == USBH_BUSY);

    if (status == USBH_OK) {
        if (osSemaphoreAcquire(usb_serial_transmit_semaphore, pdMS_TO_TICKS(1000)) == osErrorTimeout) {
            status = USBH_FAIL;
        } else {
            status = USBH_OK;
        }
    }

    osMutexRelease(usb_serial_mutex);

    return status;
}

void usb_serial_transmit_callback(USBH_HandleTypeDef *phost)
{
    osSemaphoreRelease(usb_serial_transmit_semaphore);
}

USBH_StatusTypeDef usb_serial_clear_receive_buffer()
{
    osMutexAcquire(usb_serial_mutex, portMAX_DELAY);
    memset(usb_serial_recv_buf, 0, sizeof(usb_serial_recv_buf));
    usb_serial_recv_buf_len = 0;
    osMutexRelease(usb_serial_mutex);

    return USBH_OK;
}

USBH_StatusTypeDef usb_serial_receive_line(uint8_t *buf, size_t length, uint32_t ms_to_wait)
{
    uint32_t start_wait_ticks = 0;
    uint32_t elapsed_wait_ticks = 0;
    uint32_t remaining_ticks = pdMS_TO_TICKS(ms_to_wait);
    bool has_line = false;

    if (!buf || length == 0) {
        return USBH_FAIL;
    }

    do {
        /* Before waiting, check the buffer to see if it already contains data */
        osMutexAcquire(usb_serial_mutex, portMAX_DELAY);

        /* Scan the buffer to see if it contains a line break */
        for (size_t i = 0; i < usb_serial_recv_buf_len; i++) {
            if (usb_serial_recv_buf[i] == '\r' || usb_serial_recv_buf[i] == '\n') {
                has_line = true;
                break;
            }
        }

        /* Process the buffer if it has a line break or is otherwise full */
        if (has_line || usb_serial_recv_buf_len == SERIAL_RECV_BUF_SIZE) {
            size_t i = 0;
            size_t j = 0;
            bool has_start = false;
            bool has_end = false;
            for (i = 0; i < usb_serial_recv_buf_len; i++) {
                /* Strip line breaks out of the result, but use them as start/end markers */
                if (usb_serial_recv_buf[i] == '\r' || usb_serial_recv_buf[i] == '\n') {
                    if (has_start) {
                        has_end = true;
                    }
                } else {
                    if (has_end) {
                        break;
                    }
                    has_start = true;
                    /* Populate the provided buffer as long as it has space */
                    if (j < length) {
                        buf[j++] = usb_serial_recv_buf[i];
                    }
                }
            }

            /* Ensure that the buffer is null-terminated */
            if (j > length - 1) {
                j = length - 1;
            }
            buf[j] = '\0';

            /* Clear or adjust the internal buffer for the next receive */
            if (i == usb_serial_recv_buf_len) {
                usb_serial_recv_buf_len = 0;
            } else {
                size_t remaining = usb_serial_recv_buf_len - i;
                memmove(usb_serial_recv_buf, usb_serial_recv_buf + i, remaining);
                usb_serial_recv_buf_len -= i;
            }
        }

        osMutexRelease(usb_serial_mutex);

        /* If data was received, then exit the loop */
        if (has_line) {
            break;
        }

        /* Wait to be notified of new data, up to the specified maximum */
        start_wait_ticks = osKernelGetTickCount();
        if (osSemaphoreAcquire(usb_serial_receive_semaphore, remaining_ticks) == osErrorTimeout) {
            remaining_ticks = 0;
        } else {
            elapsed_wait_ticks = start_wait_ticks - osKernelGetTickCount();
            if (remaining_ticks > elapsed_wait_ticks) {
                remaining_ticks -= elapsed_wait_ticks;
            } else {
                remaining_ticks = 0;
            }
        }

    } while (!has_line && remaining_ticks > 0);

    return has_line ? USBH_OK : USBH_FAIL;
}

void usb_serial_receive_callback(USBH_HandleTypeDef *phost, uint8_t *data, size_t length)
{

    osMutexAcquire(usb_serial_mutex, portMAX_DELAY);

    /* Append to the receive buffer */
    size_t copy_len = MIN(SERIAL_RECV_BUF_SIZE - usb_serial_recv_buf_len, length);
    if (copy_len > 0) {
        memcpy(usb_serial_recv_buf + usb_serial_recv_buf_len, data, copy_len);
        usb_serial_recv_buf_len += copy_len;
    }

    osMutexRelease(usb_serial_mutex);

    osSemaphoreRelease(usb_serial_receive_semaphore);
}

void USBH_FTDI_TransmitCallback(USBH_HandleTypeDef *phost)
{
    usb_serial_transmit_callback(phost);
}

void USBH_FTDI_ReceiveCallback(USBH_HandleTypeDef *phost, uint8_t *data, size_t length)
{
    usb_serial_receive_callback(phost, data, length);
}

void USBH_PLCOM_TransmitCallback(USBH_HandleTypeDef *phost)
{
    usb_serial_transmit_callback(phost);
}

void USBH_PLCOM_ReceiveCallback(USBH_HandleTypeDef *phost, uint8_t *data, size_t length)
{
    usb_serial_receive_callback(phost, data, length);
}

void USBH_PLCOM_LineCodingChanged(USBH_HandleTypeDef *phost)
{
    ESP_LOGI(TAG, "USBH_PLCOM_LineCodingChanged");
}

void USBH_SLCOM_TransmitCallback(USBH_HandleTypeDef *phost)
{
    usb_serial_transmit_callback(phost);
}

void USBH_SLCOM_ReceiveCallback(USBH_HandleTypeDef *phost, uint8_t *data, size_t length)
{
    usb_serial_receive_callback(phost, data, length);
}
