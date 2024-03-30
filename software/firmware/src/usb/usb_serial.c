#include "usb_serial.h"

#include <string.h>
#include <cmsis_os.h>
#include <FreeRTOS.h>
#include <timers.h>

#include "usbh_core.h"
#include "usbh_msc.h"
#include "usbh_hid.h"
#include "usbh_cdc_acm.h"
#include "class/usbh_serial_class.h"
#include "class/usbh_serial_ch34x.h"
#include "class/usbh_serial_ftdi.h"
#include "class/usbh_serial_cp210x.h"
#include "class/usbh_serial_pl2303.h"

#define LOG_TAG "usb_serial"
#include <elog.h>

#define SERIAL_BULK_IN_BUF_SIZE 64
#define SERIAL_RECV_BUF_SIZE 256

typedef enum {
    USB_SERIAL_ATTACH = 0,
    USB_SERIAL_DETACH,
    USB_SERIAL_CLEAR_RECV,
    USB_SERIAL_RECV_LINE,
    USB_SERIAL_DATA
} usb_serial_event_type_t;

typedef struct {
    usb_serial_event_type_t event_type;
    struct usbh_serial_class *serial_class;
} usb_serial_event_t;

typedef struct {
    struct usbh_serial_class *serial_class;
    uint8_t active;
    TimerHandle_t int_timer;
    USB_MEM_ALIGNX uint8_t bulk_in_buf[SERIAL_BULK_IN_BUF_SIZE];
    uint8_t bulk_in_buf_len;
    uint8_t recv_buf[SERIAL_RECV_BUF_SIZE];
    size_t recv_buf_len;
} usb_serial_handle_t;

static usb_serial_handle_t *serial_handles[CONFIG_USBHOST_MAX_SERIAL_CLASS] = {0};

static uint8_t *recv_line_buf;
static size_t recv_line_buf_len;

static osThreadId_t serial_task = NULL;
static const osThreadAttr_t serial_task_attrs = {
    .name = "usb_serial_task",
    .stack_size = 2048,
    .priority = osPriorityNormal
};

/* Semaphore to synchronize startup and shutdown of the USB serial task */
static osSemaphoreId_t usb_serial_task_semaphore = NULL;
static const osSemaphoreAttr_t usb_serial_task_semaphore_attrs = {
    .name = "usb_serial_task_semaphore"
};

/* Queue for communication with the USB serial task */
static osMessageQueueId_t usb_serial_queue = NULL;
static const osMessageQueueAttr_t usb_serial_queue_attributes = {
    .name = "usb_serial_queue"
};

static struct cdc_line_coding serial_line_coding = {
    .dwDTERate = 9600, /* Data terminal rate in bits per second */
    .bCharFormat = 0,  /* Number of stop bits (0 = 1) */
    .bParityType = 0,  /* Parity bit type (0 = None) */
    .bDataBits = 8     /* Number of data bits */
};

static void usb_serial_thread(void *argument);
static bool usb_serial_start_device(usb_serial_handle_t *dev_handle);
static bool usb_serial_start_next_bulk_in(usb_serial_handle_t *dev_handle);
static void usb_serial_bulk_in_complete(void *arg, int nbytes);
static void usb_serial_int_timer(TimerHandle_t xTimer);
static bool usb_serial_handle_recv_line(usb_serial_handle_t *dev_handle);

bool usbh_serial_init()
{
    /* Create the semaphore used to synchronize serial task startup and shutdown */
    usb_serial_task_semaphore = osSemaphoreNew(1, 0, &usb_serial_task_semaphore_attrs);
    if (!usb_serial_task_semaphore) {
        log_e("usb_serial_task_semaphore create error");
        return false;
    }

    /* Create the serial task event queue */
    usb_serial_queue = osMessageQueueNew(10, sizeof(usb_serial_event_t), &usb_serial_queue_attributes);
    if (!usb_serial_queue) {
        log_e("usb_serial_queue create error");
        return false;
    }

    serial_task = NULL;

    return true;
}

void usbh_serial_attached(struct usbh_serial_class *serial_class)
{
    /* If the serial task isn't running, then start it */
    if (!serial_task) {
        serial_task = osThreadNew(usb_serial_thread, NULL, &serial_task_attrs);
        if (!serial_task) {
            log_w("serial_task create error");
            return;
        }
    }

    /* Send a message informing the task of the attach event */
    usb_serial_event_t event = {
        .event_type = USB_SERIAL_ATTACH,
        .serial_class = serial_class
    };
    osMessageQueuePut(usb_serial_queue, &event, 0, 0);

    /* Block until the attach has been processed */
    osSemaphoreAcquire(usb_serial_task_semaphore, portMAX_DELAY);
}

void usbh_serial_detached(struct usbh_serial_class *serial_class)
{
    if (!serial_task) {
        log_w("Serial task not running");
        return;
    }

    /* Send a message informing the task of the detach event */
    usb_serial_event_t event = {
        .event_type = USB_SERIAL_DETACH,
        .serial_class = serial_class
    };
    osMessageQueuePut(usb_serial_queue, &event, 0, 0);

    /* Block until the detach has been processed */
    osSemaphoreAcquire(usb_serial_task_semaphore, portMAX_DELAY);
}

bool usbh_serial_is_attached()
{
    /* Assume that a running serial task means an attached device */
    return serial_task != NULL;
}

void usb_serial_thread(void *argument)
{
    for (;;) {
        usb_serial_event_t event;
        if(osMessageQueueGet(usb_serial_queue, &event, NULL, portMAX_DELAY) == osOK) {
            if (event.event_type == USB_SERIAL_ATTACH) {
                do {
                    if (serial_handles[event.serial_class->devnum]) {
                        log_w("Serial device %d already attached", event.serial_class->devnum);
                        break;
                    }

                    /* Create and initialize the device-specific state handle */
                    usb_serial_handle_t *dev_handle = pvPortMalloc(sizeof(usb_serial_handle_t));
                    if (!dev_handle) {
                        log_w("Unable to create device handle");
                        break;
                    }

                    TimerHandle_t int_timer = xTimerCreate(
                        "serial_int_tim",
                        pdMS_TO_TICKS(10),
                        pdFALSE, (void *)dev_handle,
                        usb_serial_int_timer);
                    if (!int_timer) {
                        log_w("Unable to create device int timer");
                        vPortFree(dev_handle);
                        break;
                    }

                    memset(dev_handle, 0, sizeof(usb_serial_handle_t));
                    dev_handle->serial_class = event.serial_class;
                    dev_handle->int_timer = int_timer;

                    serial_handles[event.serial_class->devnum] = dev_handle;

                    if (!usb_serial_start_device(dev_handle)) {
                        xTimerDelete(dev_handle->int_timer, portMAX_DELAY);
                        vPortFree(dev_handle);
                        serial_handles[event.serial_class->devnum] = NULL;
                        break;
                    }

                } while (0);

            } else if (event.event_type == USB_SERIAL_DETACH) {
                do {
                    if (!serial_handles[event.serial_class->devnum]) {
                        log_w("Serial device %d already detached", event.serial_class->devnum);
                        break;
                    }

                    /* Destroy the device-specific state handle */
                    xTimerDelete(serial_handles[event.serial_class->devnum]->int_timer, portMAX_DELAY);
                    vPortFree(serial_handles[event.serial_class->devnum]);
                    serial_handles[event.serial_class->devnum] = NULL;
                } while (0);

            } else if (event.event_type == USB_SERIAL_CLEAR_RECV) {
                for (uint8_t i = 0; i < CONFIG_USBHOST_MAX_SERIAL_CLASS; i++) {
                    usb_serial_handle_t *dev_handle = serial_handles[i];
                    if (dev_handle && dev_handle->recv_buf_len > 0) {
                        memset(dev_handle->recv_buf, 0, SERIAL_RECV_BUF_SIZE);
                        dev_handle->recv_buf_len = 0;
                    }
                }

            } else if (event.event_type == USB_SERIAL_RECV_LINE) {
                bool has_line = false;
                if (recv_line_buf && recv_line_buf_len > 0) {
                    for (uint8_t i = 0; i < CONFIG_USBHOST_MAX_SERIAL_CLASS; i++) {
                        usb_serial_handle_t *dev_handle = serial_handles[i];
                        if (dev_handle && dev_handle->recv_buf_len > 0) {
                            if (usb_serial_handle_recv_line(dev_handle)) {
                                has_line = true;
                                break;
                            }
                        }
                    }
                }

                /* Clear the result buffer pointer to indicate that nothing was returned */
                if (!has_line) {
                    recv_line_buf = NULL;
                }

            } else if (event.event_type == USB_SERIAL_DATA) {
                usb_serial_handle_t *dev_handle = serial_handles[event.serial_class->devnum];
                if (!dev_handle) {
                    log_w("Invalid device %d", event.serial_class->devnum);
                    continue;
                }

                /* Update the receive buffer */
                if (dev_handle->bulk_in_buf_len > 0) {
                    log_d("[len=%d], \"%.*s\"", dev_handle->bulk_in_buf_len, dev_handle->bulk_in_buf_len, dev_handle->bulk_in_buf);

                    /* Append to the receive buffer */
                    size_t copy_len = MIN(SERIAL_RECV_BUF_SIZE - dev_handle->recv_buf_len, dev_handle->bulk_in_buf_len);
                    if (copy_len > 0) {
                        memcpy(dev_handle->recv_buf + dev_handle->recv_buf_len, dev_handle->bulk_in_buf, copy_len);
                        dev_handle->recv_buf_len += copy_len;
                    }
                }

                /* Start the next bulk in transfer */
                if (!usb_serial_start_next_bulk_in(dev_handle)) {
                    dev_handle->active = 0;
                }
            }

            if (event.event_type != USB_SERIAL_DATA) {
                /* Check if anything is still connected */
                bool devices_connected = false;
                for (uint8_t i = 0; i < CONFIG_USBHOST_MAX_SERIAL_CLASS; i++) {
                    if (serial_handles[i]) {
                        devices_connected = true;
                        break;
                    }
                }

                if (devices_connected) {
                    /* If devices are still connected, release the semaphore here */
                    osSemaphoreRelease(usb_serial_task_semaphore);
                } else {
                    /* Otherwise, exit the task loop then release the semaphore */
                    break;
                }
            }
        }

    }

    serial_task = NULL;
    osSemaphoreRelease(usb_serial_task_semaphore);
    osThreadExit();
}

bool usb_serial_start_device(usb_serial_handle_t *dev_handle)
{
    int ret;

    ret = usbh_serial_set_line_coding(dev_handle->serial_class, &serial_line_coding);
    if (ret < 0) {
        log_w("usb_serial_set_line_coding error: %d", ret);
        return false;
    }

    ret = usbh_serial_set_line_state(dev_handle->serial_class, true, true);
    if (ret < 0) {
        log_w("usb_serial_set_line_state error: %d", ret);
        return false;
    }

    dev_handle->active = 1;

    if (!usb_serial_start_next_bulk_in(dev_handle)) {
        dev_handle->active = 0;
        return false;
    }

    return true;
}

bool usb_serial_start_next_bulk_in(usb_serial_handle_t *dev_handle)
{
    int ret = usbh_serial_bulk_in_transfer(dev_handle->serial_class, dev_handle->bulk_in_buf, SERIAL_BULK_IN_BUF_SIZE,
        0, usb_serial_bulk_in_complete, dev_handle);
    if (ret < 0) {
        log_d("usbh_serial_bulk_in_transfer error: %d", ret);
        return false;
    }
    return true;
}

void usb_serial_bulk_in_complete(void *arg, int nbytes)
{
    int ret;
    usb_serial_handle_t *dev_handle = (usb_serial_handle_t *)arg;

    /* Do any device-specific post-processing of the buffer */
    ret = usbh_serial_bulk_in_check_result(dev_handle->serial_class, dev_handle->bulk_in_buf, nbytes);

    /* If we got a valid result, then inform the task */
    if (ret >= 0) {
        dev_handle->bulk_in_buf_len = ret;

        usb_serial_event_t event = {
            .event_type = USB_SERIAL_DATA,
            .serial_class = dev_handle->serial_class
        };
        osMessageQueuePut(usb_serial_queue, &event, 0, 0);
    } else if (ret == -USB_ERR_NAK) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (xTimerStartFromISR(dev_handle->int_timer, &xHigherPriorityTaskWoken) == pdPASS) {
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    } else if (ret < 0) {
        log_w("usb_serial_bulk_in_complete[dev=%d] error: %d", dev_handle->serial_class->devnum, ret);
    }
}

void usb_serial_int_timer(TimerHandle_t xTimer)
{
    usb_serial_handle_t *dev_handle = (usb_serial_handle_t *)pvTimerGetTimerID(xTimer);
    usb_serial_start_next_bulk_in(dev_handle);
}

bool usb_serial_handle_recv_line(usb_serial_handle_t *dev_handle)
{
    bool has_line = false;

    /* Scan the buffer to see if it contains a line break */
    for (size_t i = 0; i < dev_handle->recv_buf_len; i++) {
        if (dev_handle->recv_buf[i] == '\r' || dev_handle->recv_buf[i] == '\n') {
            has_line = true;
            break;
        }
    }

    /* Process the buffer if it has a line break or is otherwise full */
    if (has_line || dev_handle->recv_buf_len == SERIAL_RECV_BUF_SIZE) {
        size_t i = 0;
        size_t j = 0;
        bool has_start = false;
        bool has_end = false;
        for (i = 0; i < dev_handle->recv_buf_len; i++) {
            /* Strip line breaks out of the result, but use them as start/end markers */
            if (dev_handle->recv_buf[i] == '\r' || dev_handle->recv_buf[i] == '\n') {
                if (has_start) {
                    has_end = true;
                }
            } else {
                if (has_end) {
                    break;
                }
                has_start = true;
                /* Populate the provided buffer as long as it has space */
                if (j < recv_line_buf_len) {
                    recv_line_buf[j++] = dev_handle->recv_buf[i];
                }
            }
        }

        /* Ensure that the buffer is null-terminated */
        if (j > recv_line_buf_len - 1) {
            j = recv_line_buf_len - 1;
        }
        recv_line_buf[j] = '\0';

        /* Clear or adjust the internal buffer for the next receive */
        if (i == dev_handle->recv_buf_len) {
            dev_handle->recv_buf_len = 0;
        } else {
            size_t remaining = dev_handle->recv_buf_len - i;
            memmove(dev_handle->recv_buf, dev_handle->recv_buf + i, remaining);
            dev_handle->recv_buf_len -= i;
        }

        return true;
    } else {
        return false;
    }
}

osStatus_t usbh_serial_transmit(const uint8_t *buf, size_t length)
{
    //TODO
    return osErrorParameter;
}

void usbh_serial_clear_receive_buffer()
{
    if (!serial_task) {
        return;
    }

    /* Send a message informing the task of the detach event */
    usb_serial_event_t event = {
        .event_type = USB_SERIAL_CLEAR_RECV
    };
    osMessageQueuePut(usb_serial_queue, &event, 0, 0);

    /* Block until the command has been processed */
    osSemaphoreAcquire(usb_serial_task_semaphore, portMAX_DELAY);
}

osStatus_t usbh_serial_receive_line(uint8_t *buf, size_t length)
{
    bool has_line;

    /* Set the buffer pointer used by the task */
    recv_line_buf = buf;
    recv_line_buf_len = length;

    /* Send a message requesting the next line */
    usb_serial_event_t event = {
        .event_type = USB_SERIAL_RECV_LINE
    };
    osMessageQueuePut(usb_serial_queue, &event, 0, 0);

    /* Block until the command has been processed */
    osSemaphoreAcquire(usb_serial_task_semaphore, portMAX_DELAY);

    has_line = recv_line_buf != NULL;

    /* Clear the buffer pointer used by the task */
    recv_line_buf = NULL;
    recv_line_buf_len = 0;

    if (has_line) {
        return osOK;
    } else {
        return osErrorTimeout;
    }
}
