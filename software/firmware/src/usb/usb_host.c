#include "usb_host.h"

#include <cmsis_os.h>
#include <FreeRTOS.h>
#include <timers.h>
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

#define SOF_WATCHDOG_PERIOD          pdMS_TO_TICKS(10)
#define SOF_WATCHDOG_ATTACH_TIMEOUT  pdMS_TO_TICKS(2000)
#define SOF_WATCHDOG_RUNNING_TIMEOUT pdMS_TO_TICKS(500)
#define SOF_WATCHDOG_REATTACH_DELAY  pdMS_TO_TICKS(20)

extern SMBUS_HandleTypeDef hsmbus2;

extern void Error_Handler(void);

static const uint8_t USB2422_ADDRESS = 0x2C << 1;

/* Mutex to synchronize attach and detach event handling */
static osMutexId_t usb_attach_mutex;
static const osMutexAttr_t usb_attach_mutex_attributes = {
    .name = "usb_attach_mutex"
};

volatile bool internal_hub_attached = false;
volatile uint32_t sof_timer = 0;

static TimerHandle_t sof_watchdog_timer = NULL;

typedef enum {
    SOF_WATCHDOG_INIT = 0,
    SOF_WATCHDOG_ATTACH,
    SOF_WATCHDOG_RUNNING,
    SOF_WATCHDOG_TIMEOUT
} sof_watchdog_state_t;

sof_watchdog_state_t sof_watchdog_state = SOF_WATCHDOG_INIT;
uint32_t sof_watchdog_ticks = 0;
uint32_t sof_watchdog_last_value = 0;

typedef enum {
    HID_DEVICE_UNKNOWN = 0,
    HID_DEVICE_KEYBOARD,
    HID_DEVICE_MOUSE,
    HID_DEVICE_FT260_DEFAULT,
    HID_DEVICE_FT260_METER_PROBE,
    HID_DEVICE_FT260_DENSISTICK
} hid_device_type_t;

static bool usb_hub_init();

static HAL_StatusTypeDef smbus_master_block_write(
    SMBUS_HandleTypeDef *hsmbus,
    uint8_t dev_address, uint8_t mem_address,
    const uint8_t *data, uint8_t len);

static void sof_watchdog_timer_callback(TimerHandle_t xTimer);

static hid_device_type_t usbh_hid_check_device_type(const struct usbh_hid *hid_class);

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

    /* Create the timer used to act as a watchdog on the SOF event */
    sof_watchdog_timer = xTimerCreate(
        "sof_watchdog", SOF_WATCHDOG_PERIOD, pdTRUE, (void *)0,
        sof_watchdog_timer_callback);
    if (!sof_watchdog_timer) {
        log_e("Unable to create SOF watchdog timer");
        return false;
    }

    /* Initialize class drivers */
    if (!usbh_hid_keyboard_init()) {
        return false;
    }
    if (!usbh_msc_fatfs_init()) {
        return false;
    }
    if (!usbh_serial_init()) {
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

    log_i("USB stack initialized");

    if (!usb_hub_init()) {
        log_e("Unable to initialize USB hub");
        return false;
    }

    return true;
}

void usb_host_deinit()
{
    /* Put the USB2422 into reset */
    HAL_GPIO_WritePin(USB_HUB_RESET_GPIO_Port, USB_HUB_RESET_Pin, GPIO_PIN_RESET);

    /* Wait for the stack to notice everything detached */
    osDelay(200);

    /* Disable the VBUS pin */
    HAL_GPIO_WritePin(USB_DRIVE_VBUS_GPIO_Port, USB_DRIVE_VBUS_Pin, GPIO_PIN_RESET);
    osDelay(2);

    /* Shutdown the USB stack */
    usbh_deinitialize(0);
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

    /* Start the SOF watchdog timer */
    sof_watchdog_state = SOF_WATCHDOG_ATTACH;
    sof_watchdog_ticks = osKernelGetTickCount();
    if (xTimerStart(sof_watchdog_timer, portMAX_DELAY) != pdPASS) {
        log_w("Unable to start SOF watchdog timer");
        return false;
    }

    /* Finish configuration and trigger USB_ATTACH */
    log_d("Triggering USB attach");
    static const uint8_t USB2422_STCD = 0x01;
    ret = smbus_master_block_write(&hsmbus2, USB2422_ADDRESS, 0xFF, &USB2422_STCD, 1);
    if (ret != HAL_OK) {
        sof_watchdog_state = SOF_WATCHDOG_INIT;
        xTimerStop(sof_watchdog_timer, portMAX_DELAY);
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

void usb_hc_sof(struct usbh_bus *bus)
{
    sof_timer++;
}

void sof_watchdog_timer_callback(TimerHandle_t xTimer)
{
    uint32_t ticks = osKernelGetTickCount();
    sof_watchdog_state_t next_state = sof_watchdog_state;

    if (sof_watchdog_state == SOF_WATCHDOG_ATTACH) {
        if (internal_hub_attached) {
            next_state = SOF_WATCHDOG_RUNNING;
        } else if (ticks - sof_watchdog_ticks > SOF_WATCHDOG_ATTACH_TIMEOUT) {
            log_w("USB internal hub attach timeout");
            next_state = SOF_WATCHDOG_TIMEOUT;
        }
    } else if (sof_watchdog_state == SOF_WATCHDOG_RUNNING) {
        if (sof_timer != sof_watchdog_last_value) {
            sof_watchdog_ticks = ticks;
        } else if (ticks - sof_watchdog_ticks > SOF_WATCHDOG_RUNNING_TIMEOUT) {
            log_w("USB SOF timeout");
            next_state = SOF_WATCHDOG_TIMEOUT;
        }
    } else if (sof_watchdog_state == SOF_WATCHDOG_TIMEOUT) {
        if (!internal_hub_attached && ticks - sof_watchdog_ticks > SOF_WATCHDOG_REATTACH_DELAY) {
            log_i("Triggering USB hub reattach");
            next_state = SOF_WATCHDOG_ATTACH;
        }
    }

    if (next_state != sof_watchdog_state) {
        if (next_state == SOF_WATCHDOG_ATTACH) {
            /* Indicate a return of VBUS_DETECT to cause the internal hub to reattach */
            HAL_GPIO_WritePin(USB_DRIVE_VBUS_GPIO_Port, USB_DRIVE_VBUS_Pin, GPIO_PIN_SET);
        } else if (next_state == SOF_WATCHDOG_TIMEOUT) {
            /* Indicate a loss of VBUS_DETECT to cause the internal hub to detach */
            HAL_GPIO_WritePin(USB_DRIVE_VBUS_GPIO_Port, USB_DRIVE_VBUS_Pin, GPIO_PIN_RESET);
        }
        sof_watchdog_state = next_state;
        sof_watchdog_ticks = ticks;
    }
    sof_watchdog_last_value = sof_timer;
}

void usbh_hub_run(struct usbh_hub *hub)
{
    if (!hub->is_roothub && hub->parent->parent->is_roothub) {
        log_d("Internal hub attached");
        internal_hub_attached = true;
    }
}

void usbh_hub_stop(struct usbh_hub *hub)
{
    if (!hub->is_roothub && hub->parent->parent->is_roothub) {
        log_d("Internal hub detached");
        internal_hub_attached = false;
    }
}

bool usb_msc_is_mounted()
{
    for (uint8_t i = 0; i < usbh_msc_max_drives(); i++) {
        if (usbh_msc_is_mounted(i)) { return true; }
    }
    return false;
}

bool usb_msc_get_serial(uint8_t num, char *buf, size_t len)
{
    bool result;
    osMutexAcquire(usb_attach_mutex, portMAX_DELAY);
    result = usbh_msc_drive_serial(num, buf, len);
    osMutexRelease(usb_attach_mutex);
    return result;
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
        return HID_DEVICE_FT260_DEFAULT;
    } else if (hid_class->hport->device_desc.idVendor == 0x16D0 && hid_class->hport->device_desc.idProduct == 0x132C) {
        return HID_DEVICE_FT260_METER_PROBE;
    } else if (hid_class->hport->device_desc.idVendor == 0x16D0 && hid_class->hport->device_desc.idProduct == 0x1382) {
        return HID_DEVICE_FT260_DENSISTICK;
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
    case HID_DEVICE_FT260_DEFAULT:
    case HID_DEVICE_FT260_METER_PROBE:
    case HID_DEVICE_FT260_DENSISTICK:
        log_d("FT260 device attached (intf=%d, minor=%d)", hid_class->intf, hid_class->minor);
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
    case HID_DEVICE_FT260_DEFAULT:
    case HID_DEVICE_FT260_METER_PROBE:
    case HID_DEVICE_FT260_DENSISTICK:
        log_d("FT260 device detached (intf=%d, minor=%d)", hid_class->intf, hid_class->minor);
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

void usbh_serial_run(struct usbh_serial_class *serial_class)
{
    osMutexAcquire(usb_attach_mutex, portMAX_DELAY);
    usbh_serial_attached(serial_class);
    osMutexRelease(usb_attach_mutex);
}

void usbh_serial_stop(struct usbh_serial_class *serial_class)
{
    osMutexAcquire(usb_attach_mutex, portMAX_DELAY);
    usbh_serial_detached(serial_class);
    osMutexRelease(usb_attach_mutex);
}

bool usb_serial_is_attached()
{
    bool result;
    osMutexAcquire(usb_attach_mutex, portMAX_DELAY);
    result = usbh_serial_is_attached();
    osMutexRelease(usb_attach_mutex);
    return result;
}

void usbh_serial_receive_callback(uint8_t *data, size_t length)
{
}

osStatus_t usb_serial_transmit(const uint8_t *buf, size_t length)
{
    return usbh_serial_transmit(buf, length);
}

void usb_serial_clear_receive_buffer()
{
    osMutexAcquire(usb_attach_mutex, portMAX_DELAY);
    usbh_serial_clear_receive_buffer();
    osMutexRelease(usb_attach_mutex);
}

osStatus_t usb_serial_receive_line(uint8_t *buf, size_t length)
{
    osStatus_t ret = osErrorTimeout;
    osMutexAcquire(usb_attach_mutex, portMAX_DELAY);
    ret = usbh_serial_receive_line(buf, length);
    osMutexRelease(usb_attach_mutex);
    return ret;
}

bool usb_meter_probe_is_attached()
{
    bool result;
    osMutexAcquire(usb_attach_mutex, portMAX_DELAY);
    result = usbh_ft260_is_attached(FT260_METER_PROBE);
    osMutexRelease(usb_attach_mutex);
    return result;
}

bool usb_densistick_is_attached()
{
    bool result;
    osMutexAcquire(usb_attach_mutex, portMAX_DELAY);
    result = usbh_ft260_is_attached(FT260_DENSISTICK);
    osMutexRelease(usb_attach_mutex);
    return result;
}

const char *usb_error_str(int err)
{
    switch (err) {
    case USB_ERR_NOMEM:
        return "USB_ERR_NOMEM";
    case USB_ERR_INVAL:
        return "USB_ERR_INVAL";
    case USB_ERR_NODEV:
        return "USB_ERR_NODEV";
    case USB_ERR_NOTCONN:
        return "USB_ERR_NOTCONN";
    case USB_ERR_NOTSUPP:
        return "USB_ERR_NOTSUPP";
    case USB_ERR_BUSY:
        return "USB_ERR_BUSY";
    case USB_ERR_RANGE:
        return "USB_ERR_RANGE";
    case USB_ERR_STALL:
        return "USB_ERR_STALL";
    case USB_ERR_BABBLE:
        return "USB_ERR_BABBLE";
    case USB_ERR_NAK:
        return "USB_ERR_NAK";
    case USB_ERR_DT:
        return "USB_ERR_DT";
    case USB_ERR_IO:
        return "USB_ERR_IO";
    case USB_ERR_SHUTDOWN:
        return "USB_ERR_SHUTDOWN";
    case USB_ERR_TIMEOUT :
        return "USB_ERR_TIMEOUT";
    default:
        return "USB_ERR_UNKNOWN";
    }
}
