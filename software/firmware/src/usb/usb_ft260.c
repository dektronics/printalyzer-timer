#include "usb_ft260.h"

#include <cmsis_os.h>

#include "usbh_core.h"
#include "usbh_hid.h"
#include "ft260.h"
#include "i2c_interface.h"
#include "meter_probe.h"
#include "keypad.h"

#define LOG_TAG "usb_meter_probe"
#include <elog.h>

#include "util.h"

typedef enum {
    FT260_CONTROL_ATTACH = 0,
    FT260_CONTROL_DETACH,
    FT260_CONTROL_INTERRUPT
} ft260_control_event_type_t;

typedef struct {
    uint32_t ticks;
    uint8_t status_int;
    uint8_t status_dcd_ri;
} ft260_interrupt_params_t;

typedef struct {
    ft260_control_event_type_t event_type;
    osStatus_t *result;
    union {
        ft260_interrupt_params_t interrupt;
    };
} ft260_control_event_t;

typedef struct {
    struct usbh_hid *hid_class0; /*!< HID endpoint for I2C and control */
    struct usbh_hid *hid_class1; /*!< HID endpoint for interrupts */
    uint8_t connected;
    uint8_t active;
} usb_ft260_handle_t;

static usb_ft260_handle_t handle = {0};

static osThreadId_t ft260_task = NULL;
static const osThreadAttr_t ft260_task_attrs = {
    .name = "ft260_task",
    .stack_size = 2048,
    .priority = osPriorityNormal
};

/* Queue for FT260 control events */
static osMessageQueueId_t ft260_control_queue = NULL;
static const osMessageQueueAttr_t ft260_control_queue_attrs = {
    .name = "ft260_control_queue"
};

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t ft260_in_buffer[128];

static void usbh_ft260_thread(void *argument);
static void log_system_status(const ft260_system_status_t *system_status);
static void usbh_ft260_int_callback(void *arg, int nbytes);

bool usbh_ft260_init()
{
    /* Create the queue for meter probe control events */
    ft260_control_queue = osMessageQueueNew(20, sizeof(ft260_control_event_t), &ft260_control_queue_attrs);
    if (!ft260_control_queue) {
        log_e("Unable to create control queue");
        return false;
    }

    return true;
}

void usbh_ft260_attached(struct usbh_hid *hid_class)
{
    if (handle.connected) {
        return;
    }

    /*
     * We're going to be called twice, for each HID endpoint,
     * so assign the handles correctly
     */
    if (!handle.hid_class0 && !handle.hid_class1) {
        if (hid_class->intf == 0) {
            handle.hid_class0 = hid_class;
        } else if (hid_class->intf == 1) {
            handle.hid_class1 = hid_class;
        }
    } else if (handle.hid_class0 && !handle.hid_class1 && hid_class->intf == 1
               && handle.hid_class0->hport == hid_class->hport) {
        handle.hid_class1 = hid_class;
    } else if (!handle.hid_class0 && handle.hid_class1 && hid_class->intf == 0
               && handle.hid_class1->hport == hid_class->hport) {
        handle.hid_class0 = hid_class;
    }

    /* If we now have both HID endpoints, then we are connected */
    if (handle.hid_class0 && handle.hid_class1) {
        handle.connected = true;
        log_i("--> FT260 device connected"); //XXX

        if (!ft260_task) {
            ft260_task = osThreadNew(usbh_ft260_thread, NULL, &ft260_task_attrs);
        }

        ft260_control_event_t event = {
            .event_type = FT260_CONTROL_ATTACH,
        };
        osMessageQueuePut(ft260_control_queue, &event, 0, 0);
    }
}

void usbh_ft260_detached(struct usbh_hid *hid_class)
{
    if (handle.hid_class0 == hid_class) {
        handle.hid_class0 = NULL;
    } else if (handle.hid_class1 == hid_class) {
        handle.hid_class1 = NULL;
    }

    if (!handle.hid_class0 && !handle.hid_class1 && handle.connected) {
        //TODO handle any detach behaviors
        log_i("--> FT260 device disconnected"); //XXX
        handle.connected = 0;

        ft260_control_event_t event = {
            .event_type = FT260_CONTROL_DETACH,
        };
        osMessageQueuePut(ft260_control_queue, &event, 0, 0);

        ft260_task = NULL;
    }
}

void usbh_ft260_thread(void *argument)
{
    int ret;
    ft260_control_event_t control_event;

    /* Initialize the primary endpoint */
    do {
        ft260_chip_version_t chip_version;
        ft260_system_status_t system_status;
        uint8_t bus_status;
        uint16_t speed;

        ret = ft260_get_chip_version(handle.hid_class0, &chip_version);
        if (ret < 0) { break; }

        log_d("chip=%04X, minor=%02X, major=%02X", chip_version.chip, chip_version.minor, chip_version.major);

        ret = ft260_get_system_status(handle.hid_class0, &system_status);
        if (ret < 0) { break; }

        log_system_status(&system_status);

        ret = ft250_set_i2c_clock_speed(handle.hid_class0, 400);
        if (ret < 0) { break; }

        ret = ft260_get_i2c_status(handle.hid_class0, &bus_status, &speed);
        if (ret < 0) { break; }

        log_d("I2C bus status: 0x%02X", bus_status);
        log_d("I2C speed: %dkHz", speed);
    } while (0);

    if (ret < 0) {
        log_e("Unable to initialize FT260 primary endpoint");
        osThreadExit();
    }

    /* Initialize the secondary endpoint */
    do {
        ret = ft260_set_uart_enable_ri_wakeup(handle.hid_class1, true);
        if (ret < 0) { break; }

        ret = ft260_set_uart_ri_wakeup_config(handle.hid_class1, false /* rising edge */);
        if (ret < 0) { break; }

        ret = ft260_set_uart_enable_dcd_ri(handle.hid_class1, true);
        if (ret < 0) { break; }

    } while (0);

    if (ret < 0) {
        log_e("Unable to initialize FT260 secondary endpoint");
        osThreadExit();
    }


    /* Start the main control event loop */
    for (;;) {
        if (osMessageQueueGet(ft260_control_queue, &control_event, NULL, portMAX_DELAY) == osOK) {
            osStatus_t ret = osOK;
            if (control_event.event_type == FT260_CONTROL_ATTACH) {
                log_d("FT260_CONTROL_ATTACH");
                /* Start the interrupt polling */
                usbh_int_urb_fill(&handle.hid_class1->intin_urb,
                    handle.hid_class1->hport,
                    handle.hid_class1->intin,
                    ft260_in_buffer,
                    handle.hid_class1->intin->wMaxPacketSize, 0,
                    usbh_ft260_int_callback, handle.hid_class1);
                ret = usbh_submit_urb(&handle.hid_class1->intin_urb);
                if (ret < 0) {
                    log_e("Unable to start FT260 interrupt polling");
                } else {
                    handle.active = 1;
                }
            } else if (control_event.event_type == FT260_CONTROL_DETACH) {
                log_d("FT260_CONTROL_DETACH");
                handle.active = 0;
                /* Inject a release event just in case */
                keypad_inject_raw_event(KEYPAD_METER_PROBE, false, osKernelGetTickCount());
                break;
            } else if (control_event.event_type == FT260_CONTROL_INTERRUPT) {
#if 0
                log_d("Interrupt: [dcd_ri=%02X int=%02X]",
                    control_event.interrupt.status_int, control_event.interrupt.status_dcd_ri);
#endif
                if (control_event.interrupt.status_int & 0x01) {
                    meter_probe_int_handler();
                }
                if (control_event.interrupt.status_dcd_ri & 0x04) {
                    const bool pressed = (control_event.interrupt.status_dcd_ri & 0x08) == 0;
                    keypad_inject_raw_event(KEYPAD_METER_PROBE, pressed, control_event.interrupt.ticks);
                }
            }
        }
    }

    osThreadExit();
}

void log_system_status(const ft260_system_status_t *system_status)
{
#if 0
    log_d("Chip mode: DCNF0=%d, DCNF1=%d",
        (system_status->chip_mode & FT260_SYSTEM_STATUS_MODE_DCFN0) != 0,
        (system_status->chip_mode & FT260_SYSTEM_STATUS_MODE_DCFN1) != 0);

    switch (system_status->clock_control) {
    case FT260_SYSTEM_STATUS_CLOCK_12MHZ:
        log_d("Clock: 12MHz");
        break;
    case FT260_SYSTEM_STATUS_CLOCK_24MHZ:
        log_d("Clock: 24MHz");
        break;
    case FT260_SYSTEM_STATUS_CLOCK_48MHZ:
        log_d("Clock: 48MHz");
        break;
    default:
        log_d("Clock: [%d]", system_status->clock_control);
        break;
    }

    log_d("SUSPEND: %s", (system_status->suspend_status == 0) ? "Not suspended": "Suspended");
    log_d("PWREN: %s", (system_status->pwren_status == 0) ? "Not ready" : "Ready");
    log_d("I2C: %s", (system_status->i2c_enable == 0) ? "Disabled" : "Enabled");

    switch (system_status->uart_mode) {
    case FT260_SYSTEM_STATUS_UART_OFF:
        log_d("UART: OFF");
        break;
    case FT260_SYSTEM_STATUS_UART_RTS_CTS:
        log_d("UART: RTS/CTS");
        break;
    case FT260_SYSTEM_STATUS_UART_DTR_DSR:
        log_d("UART: DTR/DSR");
        break;
    case FT260_SYSTEM_STATUS_UART_XON_XOFF:
        log_d("UART: XON/XOFF");
        break;
    case FT260_SYSTEM_STATUS_UART_NO_FLOW:
        log_d("UART: No flow control");
        break;
    default:
        log_d("UART: [%d]", system_status->uart_mode);
    }

    log_d("HID-over-I2C: %s", (system_status->hid_over_i2c_enable == 0) ? "Not configured" : "Configured");
    log_d("GPIO2=%d, GPIOA=%d, GPIOG=%d", system_status->gpio2_function, system_status->gpioa_function, system_status->gpiog_function);
    log_d("Suspend output: %s", (system_status->suspend_output_polarity == 0) ? "Active high" : "Active low");
    log_d("Wakeup interrupt: %s", (system_status->enable_wakeup_int == 0) ? "Disabled" : "Enabled");

    switch (system_status->interrupt_condition & FT260_SYSTEM_STATUS_INTR_MASK) {
    case FT260_SYSTEM_STATUS_INTR_RISING_EDGE:
        log_d("Intr condition: Rising edge");
        break;
    case FT260_SYSTEM_STATUS_INTR_LEVEL_HIGH:
        log_d("Intr condition: Level (high)");
        break;
    case FT260_SYSTEM_STATUS_INTR_FALLING_EDGE:
        log_d("Intr condition: Falling edge");
        break;
    case FT260_SYSTEM_STATUS_INTR_LEVEL_LOW:
        log_d("Intr condition: Level (low)");
        break;
    default:
        break;
    }

    switch (system_status->interrupt_condition & FT260_SYSTEM_STATUS_INTR_DUR_MASK) {
    case FT260_SYSTEM_STATUS_INTR_DUR_1MS:
        log_d("Intr duration: 1ms");
        break;
    case FT260_SYSTEM_STATUS_INTR_DUR_5MS:
        log_d("Intr duration: 5ms");
        break;
    case FT260_SYSTEM_STATUS_INTR_DUR_30MS:
        log_d("Intr duration: 30ms");
        break;
    default:
        break;
    }

    log_d("Power saving: %s", (system_status->enable_power_saving == 0) ? "Disabled" : "Enabled");
#endif
}

void usbh_ft260_int_callback(void *arg, int nbytes)
{
    struct usbh_hid *hid_class = (struct usbh_hid *)arg;

    if (!handle.active || !handle.connected) { return; }

    if (nbytes == 3 && ft260_in_buffer[0] == 0xB1) {

        ft260_control_event_t control_event = {
            .event_type = FT260_CONTROL_INTERRUPT,
            .interrupt = {
                .ticks = osKernelGetTickCount(),
                .status_int = ft260_in_buffer[1],
                .status_dcd_ri = ft260_in_buffer[2]
            }
        };

        osMessageQueuePut(ft260_control_queue, &control_event, 0, 0);

        usbh_submit_urb(&hid_class->intin_urb);
    } else if (nbytes > 0) {
        for (size_t i = 0; i < nbytes; i++) {
            log_d("0x%02x ", ft260_in_buffer[i]);
        }
        log_d("nbytes:%d\r\n", nbytes);
        usbh_submit_urb(&hid_class->intin_urb);
    } else if (nbytes == -USB_ERR_NAK) { /* for dwc2 */
        usbh_submit_urb(&hid_class->intin_urb);
    } else {
        log_d("Fell out of callback: %d", nbytes);
    }
}

bool usbh_ft260_is_attached(uint16_t vid, uint16_t pid)
{
    return handle.connected && handle.active
           && handle.hid_class0->hport->device_desc.idVendor == vid
           && handle.hid_class0->hport->device_desc.idProduct == pid;
}

static HAL_StatusTypeDef usb_ft260_i2c_transmit(i2c_handle_t *hi2c, uint8_t dev_address, const uint8_t *data, uint16_t len, uint32_t timeout)
{
    //struct usbh_hid *hid_class = ((usb_ft260_handle_t *)i2c_handle->priv)->hid_class0;
    // Not actually using this function, not yet implemented
    return HAL_ERROR;
}

static HAL_StatusTypeDef usb_ft260_i2c_receive(i2c_handle_t *hi2c, uint8_t dev_address, uint8_t *data, uint16_t len, uint32_t timeout)
{
    //struct usbh_hid *hid_class = ((usb_ft260_handle_t *)i2c_handle->priv)->hid_class0;
    // Not actually using this function, not yet implemented
    return HAL_ERROR;
}

static HAL_StatusTypeDef usb_ft260_i2c_mem_write(i2c_handle_t *hi2c, uint8_t dev_address, uint16_t mem_address, uint16_t mem_addr_size, const uint8_t *data, uint16_t len, uint32_t timeout)
{
    usb_ft260_handle_t *ft260_handle = (usb_ft260_handle_t *)hi2c->priv;
    struct usbh_hid *hid_class = ft260_handle->hid_class0;
    if (!ft260_handle->connected || !ft260_handle->active) { return HAL_ERROR; }

    if (mem_addr_size != I2C_MEMADD_SIZE_8BIT || mem_address > 0xFF) {
        return HAL_ERROR;
    }

    int ret = ft260_i2c_mem_write(hid_class, dev_address, mem_address, data, len);
    if (ret < 0) {
        return usb_to_hal_status(ret);
    }

    return HAL_OK;
}

static HAL_StatusTypeDef usb_ft260_i2c_mem_read(i2c_handle_t *hi2c, uint8_t dev_address, uint16_t mem_address, uint16_t mem_addr_size, uint8_t *data, uint16_t len, uint32_t timeout)
{
    usb_ft260_handle_t *ft260_handle = (usb_ft260_handle_t *)hi2c->priv;
    struct usbh_hid *hid_class = ft260_handle->hid_class0;
    if (!ft260_handle->connected || !ft260_handle->active) { return HAL_ERROR; }

    if (mem_addr_size != I2C_MEMADD_SIZE_8BIT || mem_address > 0xFF) {
        return HAL_ERROR;
    }

    int ret = ft260_i2c_mem_read(hid_class, dev_address, mem_address, data, len);
    if (ret < 0) {
        return usb_to_hal_status(ret);
    }

    return HAL_OK;
}

static HAL_StatusTypeDef usb_ft260_i2c_is_device_ready(i2c_handle_t *hi2c, uint8_t dev_address, uint32_t timeout)
{
    //usb_ft260_handle_t *ft260_handle = (usb_ft260_handle_t *)hi2c->priv;
    //struct usbh_hid *hid_class = ft260_handle->hid_class0;
    //if (!ft260_handle->connected || !ft260_handle->active) { return HAL_ERROR; }
    //TODO Figure out how this request is supposed to work
    //return HAL_I2C_IsDeviceReady(hi2c, dev_address, 1, timeout);
    return HAL_ERROR;
}

static i2c_handle_t i2c_handle_ft260 = {
    .transmit = usb_ft260_i2c_transmit,
    .receive = usb_ft260_i2c_receive,
    .mem_write = usb_ft260_i2c_mem_write,
    .mem_read = usb_ft260_i2c_mem_read,
    .is_device_ready = usb_ft260_i2c_is_device_ready,
    .priv = NULL
};

i2c_handle_t *usbh_ft260_get_i2c_interface()
{
    i2c_handle_ft260.priv = &handle;
    return &i2c_handle_ft260;
}
