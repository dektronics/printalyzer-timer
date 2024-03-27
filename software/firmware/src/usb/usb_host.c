#include "usb_host.h"

#include <cmsis_os.h>
#include <ff.h>

#include "stm32f4xx.h"
#include "usbh_core.h"
#include "usbh_msc.h"
#include "usbh_hid.h"
#include "usb_hid_keyboard.h"
#include "usb_msc_fatfs.h"
#include "usb_serial.h"
#include "usb_ft260.h"
#include "board_config.h"

#define LOG_TAG "usb_host"
#include <elog.h>

#define SERIAL_RECV_BUF_SIZE 256

extern SMBUS_HandleTypeDef hsmbus2;

extern void Error_Handler(void);

static const uint8_t USB2422_ADDRESS = 0x2C << 1;

/* Mutex to synchronize attach and detach event handling */
static osMutexId_t usb_attach_mutex;
static const osMutexAttr_t usb_attach_mutex_attributes = {
    .name = "usb_attach_mutex"
};

/* Mutex to guard the serial buffers */
static osMutexId_t usb_serial_mutex = NULL;
static const osMutexAttr_t usb_serial_mutex_attributes = {
    .name = "usb_serial_mutex"
};

/* Semaphore for serial transmit */
static osSemaphoreId_t usb_serial_transmit_semaphore = NULL;
static const osSemaphoreAttr_t usb_serial_transmit_semaphore_attributes = {
    .name = "usb_serial_transmit_semaphore"
};

/* Semaphore for serial receive */
static osSemaphoreId_t usb_serial_receive_semaphore = NULL;
static const osSemaphoreAttr_t usb_serial_receive_semaphore_attributes = {
    .name = "usb_serial_receive_semaphore"
};

static uint8_t usb_serial_recv_buf[SERIAL_RECV_BUF_SIZE] = {0};
static size_t usb_serial_recv_buf_len = 0;

typedef enum {
    HID_DEVICE_UNKNOWN = 0,
    HID_DEVICE_KEYBOARD,
    HID_DEVICE_MOUSE,
    HID_DEVICE_FT260,
    HID_DEVICE_METER_PROBE
} hid_device_type_t;

static HAL_StatusTypeDef smbus_master_block_write(
    SMBUS_HandleTypeDef *hsmbus,
    uint8_t dev_address, uint8_t mem_address,
    const uint8_t *data, uint8_t len);

static hid_device_type_t usbh_hid_check_device_type(const struct usbh_hid *hid_class);

static void usbh_serial_receive_callback(uint8_t *data, size_t length);
static void usbh_serial_transmit_callback();

void usb_hc_low_level_init(struct usbh_bus *bus)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    /* Initialize the peripherals clock */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_CLK48;
    PeriphClkInitStruct.PLLSAI.PLLSAIM = 8;
    PeriphClkInitStruct.PLLSAI.PLLSAIN = 96;
    PeriphClkInitStruct.PLLSAI.PLLSAIQ = 2;
    PeriphClkInitStruct.PLLSAI.PLLSAIP = RCC_PLLSAIP_DIV4;
    PeriphClkInitStruct.PLLSAIDivQ = 1;
    PeriphClkInitStruct.Clk48ClockSelection = RCC_CLK48CLKSOURCE_PLLSAIP;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
        Error_Handler();
    }

    __HAL_RCC_GPIOB_CLK_ENABLE();

    /*
     * USB_OTG_HS GPIO Configuration
     * PB14     ------> USB_OTG_HS_DM
     * PB15     ------> USB_OTG_HS_DP
     */
    GPIO_InitStruct.Pin = GPIO_PIN_14|GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_OTG_HS_FS;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* Peripheral clock enable */
    __HAL_RCC_USB_OTG_HS_CLK_ENABLE();

    /* Peripheral interrupt init */
    HAL_NVIC_SetPriority(OTG_HS_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(OTG_HS_IRQn);
}

bool usb_host_init()
{
    /* Make sure the USB2422 hub is in reset, and the VBUS signal is low */
    HAL_GPIO_WritePin(GPIOB, USB_HUB_RESET_Pin|USB_DRIVE_VBUS_Pin, GPIO_PIN_RESET);

    /* Initialize attach event mutex */
    usb_attach_mutex = osMutexNew(&usb_attach_mutex_attributes);
    if (!usb_attach_mutex) {
        log_e("usb_attach_mutex create error");
        return false;
    }

    /* Initialize the serial buffer mutex */
    usb_serial_mutex = osMutexNew(&usb_serial_mutex_attributes);
    if (!usb_serial_mutex) {
        log_e("usb_serial_mutex create error");
        return false;
    }

    /* Initialize the serial transmit semaphore */
    usb_serial_transmit_semaphore = osSemaphoreNew(1, 0, &usb_serial_transmit_semaphore_attributes);
    if (!usb_serial_transmit_semaphore) {
        log_e("usb_serial_transmit_semaphore create error");
        return false;
    }

    /* Initialize the serial receive semaphore */
    usb_serial_receive_semaphore = osSemaphoreNew(1, 0, &usb_serial_receive_semaphore_attributes);
    if (!usb_serial_receive_semaphore) {
        log_e("usb_serial_receive_semaphore create error");
        return false;
    }

    /* Initialize class drivers */
    if (!usbh_hid_keyboard_init()) {
        return false;
    }
    if (!usbh_msc_fatfs_init()) {
        return false;
    }
    if (!usbh_serial_init(usbh_serial_receive_callback, usbh_serial_transmit_callback)) {
        return false;
    }
    if (!usbh_ft260_init()) {
        return false;
    }

    /*
     * May need to modify this library function so the error code
     * is actually returned. It isn't, so not handling it.
     */
    usbh_initialize(0, USB_OTG_HS_PERIPH_BASE);
    return true;
}

bool usb_hub_init()
{
    HAL_StatusTypeDef ret = HAL_OK;

    /*
     * Initialize the USB2422 hub that sits between the MCU host controller
     * and the external USB ports.
     */

    /* Bring the USB2422 out of reset */
    HAL_GPIO_WritePin(USB_HUB_RESET_GPIO_Port, USB_HUB_RESET_Pin, GPIO_PIN_SET);
    osDelay(2);

    /* Enabling VBUS pin */
    HAL_GPIO_WritePin(USB_DRIVE_VBUS_GPIO_Port, USB_DRIVE_VBUS_Pin, GPIO_PIN_SET);
    osDelay(2);

    log_d("Loading USB2422 configuration");

    /* Initial contiguous set of USB2422 configuration values */
    static const uint8_t USB2422_MEM0[] = {
        0x24, /* VIDL (Vendor ID, LSB) */
        0x04, /* VIDM (Vendor ID, MSB) */
        0x22, /* PIDL (Product ID, LSB) */
        0x24, /* PIDM (Product ID, MSB) */
        0xA0, /* DIDL (Device ID, LSB) */
        0x00, /* DIDM (Device ID, MSB) */
        0xAB, /* CFG1 (SELF_BUS_PWR, HS_DISABLE, EOP_DISABLE, CURRENT_SNS=port, PORT_PWR=port) */
        0x20, /* CFG2 (Default) */
        0x02, /* CFG3 (Default) */
        0x00, /* NRD (all downstream ports removable) */
        0x00, /* PDS */
        0x00, /* PDB */
        0x01, /* MAXPS (MAX_PWR_SP=2mA) */
        0x32, /* MAXPB (MAX_PWR_BP=100mA) */
        0x01, /* HCMCS (HC_MAX_C_SP=2mA) */
        0x32, /* HCMCB (MAX_PWR_BP=100mA) */
        0x32  /* PWRT (POWER_ON_TIME=100ms) */
    };
    ret = smbus_master_block_write(&hsmbus2, USB2422_ADDRESS, 0x00, USB2422_MEM0, sizeof(USB2422_MEM0));
    if (ret != HAL_OK) {
        return false;
    }

    /* Enable pin-swap on all ports */
    static const uint8_t USB2422_PRTSP = 0x07;
    ret = smbus_master_block_write(&hsmbus2, USB2422_ADDRESS, 0xFA, &USB2422_PRTSP, 1);
    if (ret != HAL_OK) {
        return false;
    }

    /* Finish configuration and trigger USB_ATTACH */
    log_d("Triggering USB attach");
    static const uint8_t USB2422_STCD = 0x01;
    ret = smbus_master_block_write(&hsmbus2, USB2422_ADDRESS, 0xFF, &USB2422_STCD, 1);
    if (ret != HAL_OK) {
        return false;
    }

    return true;
}

HAL_StatusTypeDef smbus_master_block_write(
    SMBUS_HandleTypeDef *hsmbus,
    uint8_t dev_address, uint8_t mem_address,
    const uint8_t *data, uint8_t len)
{
    HAL_StatusTypeDef ret = HAL_OK;
    HAL_SMBUS_StateTypeDef state = HAL_SMBUS_STATE_RESET;
    uint8_t buf[34];

    if (!data || len == 0 || len > 32) { return HAL_ERROR; }

    /* Prepare the block write buffer */
    buf[0] = mem_address;
    buf[1] = len;
    memcpy(buf + 2, data, len);

    /* Send the block write to the bus */
    ret = HAL_SMBUS_Master_Transmit_IT(hsmbus,
        dev_address, buf, len + 2,
        SMBUS_FIRST_AND_LAST_FRAME_NO_PEC);
    if (ret != HAL_OK) {
        log_w("HAL_SMBUS_Master_Transmit_IT error: %d", ret);
        return ret;
    }

    /* Busy-wait until the block write is finished */
    do {
        state = HAL_SMBUS_GetState(hsmbus);
        switch (state) {
        case HAL_SMBUS_STATE_ABORT:
        case HAL_SMBUS_STATE_ERROR:
            ret = HAL_ERROR;
            break;
        case HAL_SMBUS_STATE_TIMEOUT:
            ret = HAL_TIMEOUT;
            break;
        default:
            break;
        }
        osThreadYield();
    } while(state != HAL_SMBUS_STATE_READY && ret == HAL_OK);

    if (ret != HAL_OK) {
        log_w("HAL_SMBUS_GetState error: %d", state);
        return ret;
    }

    return HAL_OK;
}

bool usb_msc_is_mounted()
{
    for (uint8_t i = 0; i < usbh_msc_max_drives(); i++) {
        if (usbh_msc_is_mounted(i)) { return true; }
    }
    return false;
}

void usbh_msc_run(struct usbh_msc *msc_class)
{
    log_d("usbh_msc_run");
    osMutexAcquire(usb_attach_mutex, portMAX_DELAY);
    usbh_msc_fatfs_attached(msc_class);
    osMutexRelease(usb_attach_mutex);
}

void usbh_msc_stop(struct usbh_msc *msc_class)
{
    log_d("usbh_msc_stop");
    osMutexAcquire(usb_attach_mutex, portMAX_DELAY);
    usbh_msc_fatfs_detached(msc_class);
    osMutexRelease(usb_attach_mutex);
}

hid_device_type_t usbh_hid_check_device_type(const struct usbh_hid *hid_class)
{
    if (hid_class->hport->device_desc.idVendor == 0x0403 && hid_class->hport->device_desc.idProduct == 0x6030) {
        return HID_DEVICE_FT260;
    } else if (hid_class->hport->device_desc.idVendor == 0x16D0 && hid_class->hport->device_desc.idProduct == 0x132C) {
        return HID_DEVICE_METER_PROBE;
    } else {
        /* Grab the interface descriptor */
        struct usb_interface_descriptor *intf_desc;
        intf_desc = &hid_class->hport->config.intf[hid_class->intf].altsetting[0].intf_desc;

        if (intf_desc->bInterfaceSubClass == HID_SUBCLASS_BOOTIF &&
            intf_desc->bInterfaceProtocol == HID_PROTOCOL_KEYBOARD) {
            return HID_DEVICE_KEYBOARD;
        } else if (intf_desc->bInterfaceSubClass == HID_SUBCLASS_BOOTIF &&
                   intf_desc->bInterfaceProtocol == HID_PROTOCOL_MOUSE) {
            return HID_DEVICE_MOUSE;
        }
    }
    return HID_DEVICE_UNKNOWN;
}

void usbh_hid_run(struct usbh_hid *hid_class)
{
    log_d("usbh_hid_run");

    osMutexAcquire(usb_attach_mutex, portMAX_DELAY);

    hid_device_type_t device_type = usbh_hid_check_device_type(hid_class);

    switch (device_type) {
    case HID_DEVICE_FT260: //XXX for testing
    case HID_DEVICE_METER_PROBE:
        log_d("Meter probe attached (intf=%d)", hid_class->intf);
        usbh_ft260_attached(hid_class);
        break;
    case HID_DEVICE_KEYBOARD:
        log_d("Keyboard attached");
        usbh_hid_keyboard_attached(hid_class);
        break;
    case HID_DEVICE_MOUSE:
        log_d("Mouse attached");
        /* Mice are not supported */
        break;
    default:
        break;
    }

    osMutexRelease(usb_attach_mutex);
}

void usbh_hid_stop(struct usbh_hid *hid_class)
{
    log_d("usbh_hid_stop");

    osMutexAcquire(usb_attach_mutex, portMAX_DELAY);

    hid_device_type_t device_type = usbh_hid_check_device_type(hid_class);

    switch (device_type) {
    case HID_DEVICE_FT260: //XXX for testing
    case HID_DEVICE_METER_PROBE:
        log_d("Meter probe detached (intf=%d)", hid_class->intf);
        usbh_ft260_detached(hid_class);
        break;
    case HID_DEVICE_KEYBOARD:
        log_d("Keyboard detached");
        usbh_hid_keyboard_detached(hid_class);
        break;
    case HID_DEVICE_MOUSE:
        log_d("Mouse detached");
        /* Mice are not supported */
        break;
    default:
        break;
    }

    osMutexRelease(usb_attach_mutex);
}

void usbh_serial_cdc_acm_run(struct usbh_serial_cdc_acm *cdc_acm_class)
{
    osMutexAcquire(usb_attach_mutex, portMAX_DELAY);
    usbh_serial_cdc_attached(cdc_acm_class);
    osMutexRelease(usb_attach_mutex);
}

void usbh_serial_cdc_acm_stop(struct usbh_serial_cdc_acm *cdc_acm_class)
{
    osMutexAcquire(usb_attach_mutex, portMAX_DELAY);
    usbh_serial_cdc_detached(cdc_acm_class);
    osMutexRelease(usb_attach_mutex);
}

void usbh_serial_ftdi_run(struct usbh_serial_ftdi *ftdi_class)
{
    osMutexAcquire(usb_attach_mutex, portMAX_DELAY);
    usbh_serial_ftdi_attached(ftdi_class);
    osMutexRelease(usb_attach_mutex);
}

void usbh_serial_ftdi_stop(struct usbh_serial_ftdi *ftdi_class)
{
    osMutexAcquire(usb_attach_mutex, portMAX_DELAY);
    usbh_serial_ftdi_detached(ftdi_class);
    osMutexRelease(usb_attach_mutex);
}

void usbh_serial_cp210x_run(struct usbh_serial_cp210x *cp210x_class)
{
    osMutexAcquire(usb_attach_mutex, portMAX_DELAY);
    usbh_serial_cp210x_attached(cp210x_class);
    osMutexRelease(usb_attach_mutex);
}

void usbh_serial_cp210x_stop(struct usbh_serial_cp210x *cp210x_class)
{
    osMutexAcquire(usb_attach_mutex, portMAX_DELAY);
    usbh_serial_cp210x_detached(cp210x_class);
    osMutexRelease(usb_attach_mutex);
}

void usbh_serial_ch34x_run(struct usbh_serial_ch34x *ch34x_class)
{
    osMutexAcquire(usb_attach_mutex, portMAX_DELAY);
    usbh_serial_ch34x_attached(ch34x_class);
    osMutexRelease(usb_attach_mutex);
}

void usbh_serial_ch34x_stop(struct usbh_serial_ch34x *ch34x_class)
{
    osMutexAcquire(usb_attach_mutex, portMAX_DELAY);
    usbh_serial_ch34x_detached(ch34x_class);
    osMutexRelease(usb_attach_mutex);
}

void usbh_serial_pl2303_run(struct usbh_serial_pl2303 *pl2303_class)
{
    osMutexAcquire(usb_attach_mutex, portMAX_DELAY);
    usbh_serial_pl2303_attached(pl2303_class);
    osMutexRelease(usb_attach_mutex);
}

void usbh_serial_pl2303_stop(struct usbh_serial_pl2303 *pl2303_class)
{
    osMutexAcquire(usb_attach_mutex, portMAX_DELAY);
    usbh_serial_pl2303_detached(pl2303_class);
    osMutexRelease(usb_attach_mutex);
}

bool usb_serial_is_attached()
{
    return usbh_serial_is_attached();
}

void usbh_serial_receive_callback(uint8_t *data, size_t length)
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

void usbh_serial_transmit_callback()
{
    osSemaphoreRelease(usb_serial_transmit_semaphore);
}

osStatus_t usb_serial_transmit(const uint8_t *buf, size_t length)
{
    //TODO
    return osErrorParameter;
}

void usb_serial_clear_receive_buffer()
{
    osMutexAcquire(usb_serial_mutex, portMAX_DELAY);
    memset(usb_serial_recv_buf, 0, sizeof(usb_serial_recv_buf));
    usb_serial_recv_buf_len = 0;
    osMutexRelease(usb_serial_mutex);
}

osStatus_t usb_serial_receive_line(uint8_t *buf, size_t length, uint32_t ms_to_wait)
{
    uint32_t start_wait_ticks = 0;
    uint32_t elapsed_wait_ticks = 0;
    uint32_t remaining_ticks = pdMS_TO_TICKS(ms_to_wait);
    bool has_line = false;

    if (!buf || length == 0) {
        return osErrorParameter;
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

    return has_line ? osOK : osErrorTimeout;
}

bool usb_meter_probe_is_attached()
{
    return usbh_ft260_is_attached(0x16D0, 0x132C)
     || usbh_ft260_is_attached(0x0403, 0x6030); //XXX also checking base FT260 for testing
}

i2c_handle_t *usb_get_meter_probe_i2c_interface()
{
    //TODO Make sure this actually matches the meter probe VID/PID
    return usbh_ft260_get_i2c_interface();
}
