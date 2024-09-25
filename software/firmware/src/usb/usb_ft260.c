#include "usb_ft260.h"

#include <cmsis_os.h>

#include "usbh_core.h"
#include "usbh_hid.h"
#include "ft260.h"
#include "i2c_interface.h"

#define LOG_TAG "usb_meter_probe"
#include <elog.h>

#include "util.h"

#define FT260_IN_BUF_SIZE 128

/*
 * Internal device handle whose lifetime matches the FT260 control task's
 * handling of the actual device from the USB host stack.
 */
typedef struct {
    ft260_device_type_t device_type;
    struct usbh_hid *hid_class0; /*!< HID endpoint for I2C and control */
    struct usbh_hid *hid_class1; /*!< HID endpoint for interrupts */
    uint8_t serial_number[FT260_SERIAL_SIZE];
    bool button_pressed;
    uint8_t connected;
    uint8_t active;
    USB_MEM_ALIGNX uint8_t ft260_in_buffer[FT260_IN_BUF_SIZE];
} usb_ft260_handle_t;

static usb_ft260_handle_t *ft260_handles[CONFIG_USBHOST_MAX_HID_CLASS] = {0};

/*
 * External device handle which doesn't go away, and can be safely
 * used by other code in the system across disconnect events.
 */
struct ft260_device_t {
    i2c_handle_t i2c_handle;
    usb_ft260_handle_t *dev_handle;
    ft260_device_event_callback_t callback;
    osMutexId_t mutex;
};

static ft260_device_t meter_probe_handle = {0};
static ft260_device_t densistick_handle = {0};

typedef enum {
    FT260_CONTROL_ATTACH = 0,
    FT260_CONTROL_DETACH,
    FT260_CONTROL_INTERRUPT,
    FT260_CONTROL_SUBMIT_URB
} ft260_control_event_type_t;

typedef struct {
    struct usbh_hid *hid_class;
} ft260_connection_params_t;

typedef struct {
    usb_ft260_handle_t *handle;
    uint32_t ticks;
    uint8_t status_int;
    uint8_t status_dcd_ri;
} ft260_interrupt_params_t;

typedef struct {
    ft260_control_event_type_t event_type;
    osStatus_t *result;
    union {
        ft260_connection_params_t connection;
        ft260_interrupt_params_t interrupt;
        struct usbh_urb *urb;
    };
} ft260_control_event_t;

static osThreadId_t ft260_task = NULL;
static const osThreadAttr_t ft260_task_attrs = {
    .name = "ft260_task",
    .stack_size = 2048,
    .priority = osPriorityNormal
};

/* Semaphore to synchronize FT260 task communication */
static osSemaphoreId_t ft260_task_semaphore = NULL;
static const osSemaphoreAttr_t ft260_task_semaphore_attrs = {
    .name = "ft260_task_semaphore"
};

/* Queue for FT260 control events */
static osMessageQueueId_t ft260_control_queue = NULL;
static const osMessageQueueAttr_t ft260_control_queue_attrs = {
    .name = "ft260_control_queue"
};

static void usbh_ft260_thread(void *argument);
static void usbh_ft260_control_attach(struct usbh_hid *hid_class);
static void usbh_ft260_control_detach(struct usbh_hid *hid_class);
static void usbh_ft260_control_startup(usb_ft260_handle_t *dev_handle);
static void usbh_ft260_control_interrupt(ft260_interrupt_params_t *params);
static void log_system_status(const ft260_system_status_t *system_status);
static void usbh_ft260_int_callback(void *arg, int nbytes);

static HAL_StatusTypeDef usb_ft260_i2c_transmit(i2c_handle_t *hi2c, uint8_t dev_address, const uint8_t *data, uint16_t len, uint32_t timeout);
static HAL_StatusTypeDef usb_ft260_i2c_receive(i2c_handle_t *hi2c, uint8_t dev_address, uint8_t *data, uint16_t len, uint32_t timeout);
static HAL_StatusTypeDef usb_ft260_i2c_mem_write(i2c_handle_t *hi2c, uint8_t dev_address, uint16_t mem_address, uint16_t mem_addr_size, const uint8_t *data, uint16_t len, uint32_t timeout);
static HAL_StatusTypeDef usb_ft260_i2c_mem_read(i2c_handle_t *hi2c, uint8_t dev_address, uint16_t mem_address, uint16_t mem_addr_size, uint8_t *data, uint16_t len, uint32_t timeout);
static HAL_StatusTypeDef usb_ft260_i2c_is_device_ready(i2c_handle_t *hi2c, uint8_t dev_address, uint32_t timeout);
static HAL_StatusTypeDef usb_ft260_i2c_reset(i2c_handle_t *hi2c);

bool usbh_ft260_init()
{
    /* Create the semaphore used to synchronize FT260 task communication */
    ft260_task_semaphore = osSemaphoreNew(1, 0, &ft260_task_semaphore_attrs);
    if (!ft260_task_semaphore) {
        log_e("ft260_task_semaphore create error");
        return false;
    }

    /* Create the queue for meter probe control events */
    ft260_control_queue = osMessageQueueNew(20, sizeof(ft260_control_event_t), &ft260_control_queue_attrs);
    if (!ft260_control_queue) {
        log_e("Unable to create control queue");
        return false;
    }

    /* Initialize the persistent device handle */
    static const i2c_handle_t i2c_handle_ft260 = {
        .transmit = usb_ft260_i2c_transmit,
        .receive = usb_ft260_i2c_receive,
        .mem_write = usb_ft260_i2c_mem_write,
        .mem_read = usb_ft260_i2c_mem_read,
        .is_device_ready = usb_ft260_i2c_is_device_ready,
        .reset = usb_ft260_i2c_reset,
        .priv = NULL
    };
    memcpy(&meter_probe_handle.i2c_handle, &i2c_handle_ft260, sizeof(i2c_handle_t));
    meter_probe_handle.i2c_handle.priv = &meter_probe_handle;
    meter_probe_handle.mutex = osMutexNew(NULL);
    if (!meter_probe_handle.mutex) {
        log_e("Unable to create handle mutex");
        return false;
    }

    ft260_task = NULL;

    return true;
}

void usbh_ft260_attached(struct usbh_hid *hid_class)
{
    /* If the FT260 task isn't running, then start it */
    if (!ft260_task) {
        ft260_task = osThreadNew(usbh_ft260_thread, NULL, &ft260_task_attrs);
        if (!ft260_task) {
            log_w("ft260_task create error");
            return;
        }
    }

    /* Send a message informing the task of the attach event */
    ft260_control_event_t event = {
        .event_type = FT260_CONTROL_ATTACH,
        .connection = {
            .hid_class = hid_class
        }
    };
    osMessageQueuePut(ft260_control_queue, &event, 0, 0);

    /* Block until the attach has been processed */
    osSemaphoreAcquire(ft260_task_semaphore, portMAX_DELAY);
}

void usbh_ft260_detached(struct usbh_hid *hid_class)
{
    if (!ft260_task) {
        log_w("FT260 task not running");
        return;
    }

    /* Send a message informing the task of the detach event */
    ft260_control_event_t event = {
        .event_type = FT260_CONTROL_DETACH,
        .connection = {
            .hid_class = hid_class
        }
    };
    osMessageQueuePut(ft260_control_queue, &event, 0, 0);

    /* Block until the detach has been processed */
    osSemaphoreAcquire(ft260_task_semaphore, portMAX_DELAY);
}

void usbh_ft260_thread(void *argument)
{
    for (;;) {
        ft260_control_event_t event;
        if (osMessageQueueGet(ft260_control_queue, &event, NULL, portMAX_DELAY) == osOK) {
            if (event.event_type == FT260_CONTROL_ATTACH) {
                ft260_connection_params_t *params = &event.connection;
                usbh_ft260_control_attach(params->hid_class);

            } else if (event.event_type == FT260_CONTROL_DETACH) {
                ft260_connection_params_t *params = &event.connection;
                usbh_ft260_control_detach(params->hid_class);

            } else if (event.event_type == FT260_CONTROL_INTERRUPT) {
                ft260_interrupt_params_t *params = &event.interrupt;
                usbh_ft260_control_interrupt(params);
            } else if (event.event_type == FT260_CONTROL_SUBMIT_URB) {
                usbh_submit_urb(event.urb);
            }

            if (event.event_type != FT260_CONTROL_INTERRUPT && event.event_type != FT260_CONTROL_SUBMIT_URB) {
                /* Check if anything is still connected */
                bool devices_connected = false;
                for (uint8_t i = 0; i < CONFIG_USBHOST_MAX_HID_CLASS; i++) {
                    if (ft260_handles[i]) {
                        devices_connected = true;
                        break;
                    }
                }

                if (devices_connected) {
                    /* If devices are still connected, release the semaphore here */
                    osSemaphoreRelease(ft260_task_semaphore);
                } else {
                    /* Otherwise, exit the task loop then release the semaphore */
                    break;
                }
            }
        }
    }

    ft260_task = NULL;
    osSemaphoreRelease(ft260_task_semaphore);
    osThreadExit();
}

void usbh_ft260_control_attach(struct usbh_hid *hid_class)
{
    /* Check if device is already attached */
    if (ft260_handles[hid_class->minor]) {
        log_w("FT260 device %d already attached", hid_class->minor);
        return;
    }

    if (hid_class->intf != 0 && hid_class->intf != 1) {
        log_w("Unexpected FT260 interface ID: %d", hid_class->intf);
        return;
    }

    ft260_device_type_t device_type;
    if (hid_class->hport->device_desc.idVendor == 0x0403 && hid_class->hport->device_desc.idProduct == 0x6030) {
        device_type = FT260_DEFAULT;
        log_d("Device is stock FT260");
    } else if (hid_class->hport->device_desc.idVendor == 0x16D0 && hid_class->hport->device_desc.idProduct == 0x132C) {
        device_type = FT260_METER_PROBE;
        log_d("Device is Printalyzer Meter Probe");
    } else if (hid_class->hport->device_desc.idVendor == 0x16D0 && hid_class->hport->device_desc.idProduct == 0x1382) {
        device_type = FT260_DENSISTICK;
        log_d("Device is Printalyzer DensiStick");
    } else {
        log_w("Unknown device type: VID=0x%04X, PID=0x%04X",
            hid_class->hport->device_desc.idVendor,
            hid_class->hport->device_desc.idProduct);
        return;
    }

    /* Check if the companion interface is already attached, and assign */
    usb_ft260_handle_t *dev_handle = NULL;
    for (uint8_t i = 0; i < CONFIG_USBHOST_MAX_HID_CLASS; i++) {
        if(!ft260_handles[i]) { continue; }

        if (ft260_handles[i]->hid_class0 && !ft260_handles[i]->hid_class1 && hid_class->intf == 1
                   && ft260_handles[i]->hid_class0->hport == hid_class->hport) {
            dev_handle = ft260_handles[i];
            dev_handle->hid_class1 = hid_class;
            ft260_handles[hid_class->minor] = dev_handle;
        } else if (!ft260_handles[i]->hid_class0 && ft260_handles[i]->hid_class1 && hid_class->intf == 0
                   && ft260_handles[i]->hid_class1->hport == hid_class->hport) {
            dev_handle = ft260_handles[i];
            dev_handle->hid_class0 = hid_class;
            ft260_handles[hid_class->minor] = dev_handle;
        }
    }

    /* If this is the first time we've see the device, create a new handle */
    if (!dev_handle) {
        dev_handle = pvPortMalloc(sizeof(usb_ft260_handle_t));
        if (!dev_handle) {
            log_w("Unable to create FT260 handle");
            return;
        }

        memset(dev_handle, 0, sizeof(usb_ft260_handle_t));
        dev_handle->device_type = device_type;

        if (hid_class->intf == 0) {
            dev_handle->hid_class0 = hid_class;
        } else {
            dev_handle->hid_class1 = hid_class;
        }
        ft260_handles[hid_class->minor] = dev_handle;
    }

    /* If both interfaces are now there, set the connected flag and start the device */
    if (dev_handle->hid_class0 && dev_handle->hid_class1) {
        dev_handle->connected = 1;
        usbh_ft260_control_startup(dev_handle);
    }

    /* Invoke the callback to notify of the connection event */
    ft260_device_event_callback_t callback = NULL;
    if (meter_probe_handle.dev_handle == dev_handle) {
        osMutexAcquire(meter_probe_handle.mutex, portMAX_DELAY);
        callback = meter_probe_handle.callback;
        osMutexRelease(meter_probe_handle.mutex);
    }

    if (callback) {
        callback(&meter_probe_handle, FT260_EVENT_ATTACH, osKernelGetTickCount());
    }
}

void usbh_ft260_control_detach(struct usbh_hid *hid_class)
{
    /* Check if device is already detached */
    if (!ft260_handles[hid_class->minor]) {
        log_w("FT260 device %d already detached", hid_class->minor);
        return;
    }

    usb_ft260_handle_t *dev_handle = ft260_handles[hid_class->minor];
    ft260_device_event_callback_t callback = NULL;

    /* Disconnect the external handle before we break any internal state */
    if (dev_handle->device_type == FT260_METER_PROBE) {
        osMutexAcquire(meter_probe_handle.mutex, portMAX_DELAY);
        if (meter_probe_handle.dev_handle == dev_handle) {
            meter_probe_handle.dev_handle = NULL;
        }
        callback = meter_probe_handle.callback;
        osMutexRelease(meter_probe_handle.mutex);
    } else if (dev_handle->device_type == FT260_DENSISTICK) {
        osMutexAcquire(densistick_handle.mutex, portMAX_DELAY);
        if (densistick_handle.dev_handle == dev_handle) {
            densistick_handle.dev_handle = NULL;
        }
        callback = densistick_handle.callback;
        osMutexRelease(densistick_handle.mutex);
    }

    dev_handle->connected = 0;
    dev_handle->active = 0;

    /* Null the appropriate handle pointer and do any specific cleanup */
    if (dev_handle->hid_class0 == hid_class) {
        dev_handle->hid_class0 = NULL;
    } else if (dev_handle->hid_class1 == hid_class) {
        dev_handle->hid_class1 = NULL;
    }

    ft260_handles[hid_class->minor] = NULL;

    /* If both interfaces have detached, then delete the handle */
    if (!dev_handle->hid_class0 && !dev_handle->hid_class1) {
        log_d("FT260_CONTROL_DETACH");

        if (callback) {
            /* Inject a button release event just in case */
            if (dev_handle->button_pressed) {
                callback(&meter_probe_handle, FT260_EVENT_BUTTON_UP, osKernelGetTickCount());
            }

            callback(&meter_probe_handle, FT260_EVENT_DETACH, osKernelGetTickCount());
        }

        vPortFree(dev_handle);
    }
}

void usbh_ft260_control_startup(usb_ft260_handle_t *dev_handle)
{
    int ret;

    /* Query USB device descriptors */
    do {
        uint8_t string_buffer[128];

        /* Get SerialNumber string */
        memset(string_buffer, 0, 128);
        ret = usbh_get_string_desc(dev_handle->hid_class0->hport, USB_STRING_SERIAL_INDEX, string_buffer);
        if (ret < 0) {
            log_w("Failed to get device serial: %d", ret);
            break;
        }

        log_d("serial = %s", string_buffer);

        memcpy(dev_handle->serial_number, string_buffer, FT260_SERIAL_SIZE);
        dev_handle->serial_number[FT260_SERIAL_SIZE - 1] = '\0';
    } while (0);
    if (ret < 0) {
        log_e("Unable to query FT260 USB descriptors");
        return;
    }

    /* Initialize the primary endpoint */
    do {
        ft260_chip_version_t chip_version;
        ft260_system_status_t system_status;
        uint8_t bus_status;
        uint16_t speed;

        ret = ft260_get_chip_version(dev_handle->hid_class0, &chip_version);
        if (ret < 0) { break; }

        log_d("chip=%04X, minor=%02X, major=%02X", chip_version.chip, chip_version.minor, chip_version.major);

        ret = ft260_get_system_status(dev_handle->hid_class0, &system_status);
        if (ret < 0) { break; }

        log_system_status(&system_status);

        ret = ft260_set_i2c_clock_speed(dev_handle->hid_class0, 400);
        if (ret < 0) { break; }

        ret = ft260_get_i2c_status(dev_handle->hid_class0, &bus_status, &speed);
        if (ret < 0) { break; }

        log_d("I2C bus status: 0x%02X", bus_status);
        log_d("I2C speed: %dkHz", speed);
    } while (0);

    if (ret < 0) {
        log_e("Unable to initialize FT260 primary endpoint");
        return;
    }

    /* Initialize the secondary endpoint */
    do {
        ret = ft260_set_uart_enable_ri_wakeup(dev_handle->hid_class1, true);
        if (ret < 0) { break; }

        ret = ft260_set_uart_ri_wakeup_config(dev_handle->hid_class1, false /* rising edge */);
        if (ret < 0) { break; }

        ret = ft260_set_uart_enable_dcd_ri(dev_handle->hid_class1, true);
        if (ret < 0) { break; }

    } while (0);

    if (ret < 0) {
        log_e("Unable to initialize FT260 secondary endpoint");
        return;
    }

    log_d("FT260_CONTROL_ATTACH");

    /* Connect the external handle */
    if (dev_handle->device_type == FT260_METER_PROBE) {
        osMutexAcquire(meter_probe_handle.mutex, portMAX_DELAY);
        if (!meter_probe_handle.dev_handle) {
            meter_probe_handle.dev_handle = dev_handle;
        }
        osMutexRelease(meter_probe_handle.mutex);
    } else if (dev_handle->device_type == FT260_DENSISTICK) {
        osMutexAcquire(densistick_handle.mutex, portMAX_DELAY);
        if (!densistick_handle.dev_handle) {
            densistick_handle.dev_handle = dev_handle;
        }
        osMutexRelease(densistick_handle.mutex);
    }

    /* Start the interrupt polling */
    usbh_int_urb_fill(&dev_handle->hid_class1->intin_urb,
        dev_handle->hid_class1->hport,
        dev_handle->hid_class1->intin,
        dev_handle->ft260_in_buffer,
        MIN(dev_handle->hid_class1->intin->wMaxPacketSize, FT260_IN_BUF_SIZE), 0,
        usbh_ft260_int_callback, dev_handle);
    ret = usbh_submit_urb(&dev_handle->hid_class1->intin_urb);
    if (ret < 0) {
        log_e("Unable to start FT260 interrupt polling");
        return;
    }

    dev_handle->active = 1;
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

void usbh_ft260_control_interrupt(ft260_interrupt_params_t *params)
{
#if 0
    log_d("Interrupt: [dcd_ri=%02X int=%02X]",
                    control_event.interrupt.status_int, control_event.interrupt.status_dcd_ri);
#endif
    ft260_device_event_callback_t callback = NULL;
    if (meter_probe_handle.dev_handle == params->handle) {
        osMutexAcquire(meter_probe_handle.mutex, portMAX_DELAY);
        callback = meter_probe_handle.callback;
        osMutexRelease(meter_probe_handle.mutex);
    }

    if (params->status_int & 0x01) {
        if (callback) {
            callback(&meter_probe_handle, FT260_EVENT_INTERRUPT, params->ticks);
        }
    }
    if (params->status_dcd_ri & 0x04) {
        const bool pressed = (params->status_dcd_ri & 0x08) == 0;
        if (params->handle->button_pressed != pressed) {
            params->handle->button_pressed = pressed;
            if (callback) {
                callback(&meter_probe_handle,
                    pressed ? FT260_EVENT_BUTTON_DOWN : FT260_EVENT_BUTTON_UP,
                    params->ticks);
            }
        }
    }
}

void usbh_ft260_int_callback(void *arg, int nbytes)
{
    usb_ft260_handle_t *dev_handle = (usb_ft260_handle_t *)arg;
    bool submit_urb = false;

    if (!dev_handle->active || !dev_handle->connected) { return; }

    if (nbytes == 3 && dev_handle->ft260_in_buffer[0] == 0xB1) {

        ft260_control_event_t control_event = {
            .event_type = FT260_CONTROL_INTERRUPT,
            .interrupt = {
                .handle = dev_handle,
                .ticks = osKernelGetTickCount(),
                .status_int = dev_handle->ft260_in_buffer[1],
                .status_dcd_ri = dev_handle->ft260_in_buffer[2]
            }
        };

        osMessageQueuePut(ft260_control_queue, &control_event, 0, 0);

        submit_urb = true;

    } else if (nbytes > 0) {
        for (size_t i = 0; i < nbytes; i++) {
            log_d("0x%02x ", dev_handle->ft260_in_buffer[i]);
        }
        log_d("nbytes:%d\r\n", nbytes);
        submit_urb = true;

    } else if (nbytes == -USB_ERR_NAK) { /* for dwc2 */
        submit_urb = true;

    } else {
        log_d("Fell out of callback: %d", nbytes);
    }

    if (submit_urb) {
        ft260_control_event_t control_event = {
            .event_type = FT260_CONTROL_SUBMIT_URB,
            .urb = &dev_handle->hid_class1->intin_urb
        };
        osMessageQueuePut(ft260_control_queue, &control_event, 0, 0);
    }
}

bool usbh_ft260_is_attached(ft260_device_type_t device_type)
{
    for (uint8_t i = 0; i < CONFIG_USBHOST_MAX_HID_CLASS; i++) {
        if (ft260_handles[i] && ft260_handles[i]->device_type == device_type
            && ft260_handles[i]->connected && ft260_handles[i]->active) {
            return true;
        }
    }
    return false;
}

static HAL_StatusTypeDef usb_ft260_i2c_transmit(i2c_handle_t *hi2c, uint8_t dev_address, const uint8_t *data, uint16_t len, uint32_t timeout)
{
    // Not actually using this function, not yet implemented
    return HAL_ERROR;
}

static HAL_StatusTypeDef usb_ft260_i2c_receive(i2c_handle_t *hi2c, uint8_t dev_address, uint8_t *data, uint16_t len, uint32_t timeout)
{
    HAL_StatusTypeDef result = HAL_OK;
    if (!hi2c || !hi2c->priv) { return HAL_ERROR; }

    ft260_device_t *device = (ft260_device_t *)hi2c->priv;

    osMutexAcquire(device->mutex, portMAX_DELAY);
    do {
        usb_ft260_handle_t *dev_handle = device->dev_handle;
        if (!dev_handle || !dev_handle->connected || !dev_handle->active) {
            result = HAL_ERROR;
            break;
        }

        struct usbh_hid *hid_class = dev_handle->hid_class0;

        int status = ft260_i2c_receive(hid_class, dev_address, data, len);
        if (status < 0) {
            result = usb_to_hal_status(status);
        }
    } while (0);
    osMutexRelease(device->mutex);

    return result;
}

static HAL_StatusTypeDef usb_ft260_i2c_mem_write(i2c_handle_t *hi2c, uint8_t dev_address, uint16_t mem_address, uint16_t mem_addr_size, const uint8_t *data, uint16_t len, uint32_t timeout)
{
    HAL_StatusTypeDef result = HAL_OK;
    if (!hi2c || !hi2c->priv) { return HAL_ERROR; }

    if (mem_addr_size != I2C_MEMADD_SIZE_8BIT || mem_address > 0xFF) {
        return HAL_ERROR;
    }

    ft260_device_t *device = (ft260_device_t *)hi2c->priv;

    osMutexAcquire(device->mutex, portMAX_DELAY);
    do {
        usb_ft260_handle_t *dev_handle = device->dev_handle;
        if (!dev_handle || !dev_handle->connected || !dev_handle->active) {
            result = HAL_ERROR;
            break;
        }

        struct usbh_hid *hid_class = dev_handle->hid_class0;

        int status = ft260_i2c_mem_write(hid_class, dev_address, mem_address, data, len);
        if (status < 0) {
            result = usb_to_hal_status(status);
        }
    } while (0);
    osMutexRelease(device->mutex);

    return result;
}

static HAL_StatusTypeDef usb_ft260_i2c_mem_read(i2c_handle_t *hi2c, uint8_t dev_address, uint16_t mem_address, uint16_t mem_addr_size, uint8_t *data, uint16_t len, uint32_t timeout)
{
    HAL_StatusTypeDef result = HAL_OK;
    if (!hi2c || !hi2c->priv) { return HAL_ERROR; }

    if (mem_addr_size != I2C_MEMADD_SIZE_8BIT || mem_address > 0xFF) {
        return HAL_ERROR;
    }

    ft260_device_t *device = (ft260_device_t *)hi2c->priv;

    osMutexAcquire(device->mutex, portMAX_DELAY);
    do {
        usb_ft260_handle_t *dev_handle = device->dev_handle;
        if (!dev_handle || !dev_handle->connected || !dev_handle->active) {
            result = HAL_ERROR;
            break;
        }

        struct usbh_hid *hid_class = dev_handle->hid_class0;

        int status = ft260_i2c_mem_read(hid_class, dev_address, mem_address, data, len);
        if (status < 0) {
            result = usb_to_hal_status(status);
        }
    } while (0);
    osMutexRelease(device->mutex);

    return result;
}

static HAL_StatusTypeDef usb_ft260_i2c_is_device_ready(i2c_handle_t *hi2c, uint8_t dev_address, uint32_t timeout)
{
    HAL_StatusTypeDef result = HAL_OK;
    if (!hi2c || !hi2c->priv) { return HAL_ERROR; }

    ft260_device_t *device = (ft260_device_t *)hi2c->priv;

    osMutexAcquire(device->mutex, portMAX_DELAY);
    do {
        usb_ft260_handle_t *dev_handle = device->dev_handle;
        if (!dev_handle || !dev_handle->connected || !dev_handle->active) {
            result = HAL_ERROR;
            break;
        }

        struct usbh_hid *hid_class = dev_handle->hid_class0;

        int status = ft260_i2c_is_device_ready(hid_class, dev_address);
        if (status < 0) {
            result = usb_to_hal_status(status);
        }
    } while (0);
    osMutexRelease(device->mutex);

    return result;
}

HAL_StatusTypeDef usb_ft260_i2c_reset(i2c_handle_t *hi2c)
{
    HAL_StatusTypeDef result = HAL_OK;
    if (!hi2c || !hi2c->priv) { return HAL_ERROR; }

    ft260_device_t *device = (ft260_device_t *)hi2c->priv;

    osMutexAcquire(device->mutex, portMAX_DELAY);
    do {
        usb_ft260_handle_t *dev_handle = device->dev_handle;
        if (!dev_handle || !dev_handle->connected || !dev_handle->active) {
            result = HAL_ERROR;
            break;
        }

        struct usbh_hid *hid_class = dev_handle->hid_class0;

        int status = ft260_i2c_reset(hid_class);
        if (status < 0) {
            result = usb_to_hal_status(status);
        }
    } while (0);
    osMutexRelease(device->mutex);

    return result;
}

ft260_device_t *usbh_ft260_get_device(ft260_device_type_t device_type)
{
    if (device_type == FT260_METER_PROBE) {
        return &meter_probe_handle;
    } else if (device_type == FT260_DENSISTICK) {
        return &densistick_handle;
    } else {
        return NULL;
    }
}

void usbh_ft260_set_device_callback(ft260_device_t *device, ft260_device_event_callback_t callback)
{
    if (!device) { return; }

    osMutexAcquire(device->mutex, portMAX_DELAY);
    device->callback = callback;
    osMutexRelease(device->mutex);
}

osStatus_t usbh_ft260_get_device_serial_number(const ft260_device_t *device, char *str)
{
    osStatus_t result = osOK;
    if (!device || !str) { return osErrorParameter; }

    osMutexAcquire(device->mutex, portMAX_DELAY);
    do {
        usb_ft260_handle_t *dev_handle = device->dev_handle;
        if (!dev_handle || !dev_handle->connected || !dev_handle->active) {
            result = osErrorResource;
            break;
        }
        strncpy(str, (const char *)dev_handle->serial_number, FT260_SERIAL_SIZE);
    } while (0);
    osMutexRelease(device->mutex);

    return result;
}

osStatus_t usbh_ft260_set_i2c_clock_speed(ft260_device_t *device, uint16_t speed)
{
    osStatus_t result = osOK;
    if (!device) { return osErrorParameter; }

    osMutexAcquire(device->mutex, portMAX_DELAY);
    do {
        usb_ft260_handle_t *dev_handle = device->dev_handle;
        if (!dev_handle || !dev_handle->connected || !dev_handle->active) {
            result = osErrorResource;
            break;
        }

        struct usbh_hid *hid_class = dev_handle->hid_class0;

        int status = ft260_set_i2c_clock_speed(hid_class, speed);
        if (status < 0) {
            result = usb_to_os_status(status);
        }
    } while (0);
    osMutexRelease(device->mutex);

    return result;

}

i2c_handle_t *usbh_ft260_get_device_i2c(ft260_device_t *device)
{
    if (!device) { return NULL; }
    return &device->i2c_handle;
}

osStatus_t usbh_ft260_set_device_gpio(ft260_device_t *device, bool value)
{
    osStatus_t result = osOK;
    if (!device) { return osErrorParameter; }

    osMutexAcquire(device->mutex, portMAX_DELAY);
    do {
        usb_ft260_handle_t *dev_handle = device->dev_handle;
        if (!dev_handle || !dev_handle->connected || !dev_handle->active) {
            result = osErrorResource;
            break;
        }

        struct usbh_hid *hid_class = dev_handle->hid_class0;

        ft260_gpio_report_t gpio_report = {
            .gpio_ex_value = value ? FT260_GPIOEX_A : 0,
            .gpio_ex_dir = FT260_GPIOEX_A
        };
        int status = ft260_gpio_write(hid_class, &gpio_report);
        if (status < 0) {
            result = usb_to_os_status(status);
        }
    } while (0);
    osMutexRelease(device->mutex);

    return result;
}
