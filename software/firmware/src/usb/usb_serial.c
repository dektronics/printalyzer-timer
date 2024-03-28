#include "usb_serial.h"

#include <string.h>
#include <cmsis_os.h>

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

#define SERIAL_RECV_BUF_SIZE 256

typedef struct {
    struct usbh_serial_class *serial_class;
    uint8_t connected;
    uint8_t active;
} usb_serial_handle_t;

static usb_serial_handle_t handle = {0};

static osThreadId_t serial_task = NULL;
static const osThreadAttr_t serial_task_attrs = {
    .name = "usb_serial_task",
    .stack_size = 2048,
    .priority = osPriorityNormal
};

static struct cdc_line_coding serial_line_coding = {
    .dwDTERate = 9600, /* Data terminal rate in bits per second */
    .bCharFormat = 0,  /* Number of stop bits (0 = 1) */
    .bParityType = 0,  /* Parity bit type (0 = None) */
    .bDataBits = 8     /* Number of data bits */
};

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t bulk_in_buffer[64];

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

static void usb_serial_thread(void *argument);

bool usbh_serial_init()
{
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

    serial_task = NULL;

    return true;
}

void usbh_serial_attached(struct usbh_serial_class *serial_class)
{
    if (handle.connected) {
        return;
    }
    handle.serial_class = serial_class;
    handle.connected = 1;
    if (!serial_task) {
        serial_task = osThreadNew(usb_serial_thread, NULL, &serial_task_attrs);
    }
}

void usbh_serial_detached(struct usbh_serial_class *serial_class)
{
    if (handle.serial_class == serial_class) {
        if (handle.connected) {
            memset(&handle, 0, sizeof(usb_serial_handle_t));
        }
    }
}

bool usbh_serial_is_attached()
{
    return handle.connected == 1 && handle.active == 1;
}

void usb_serial_thread(void *argument)
{
    int ret;

    ret = usbh_serial_set_line_coding(handle.serial_class, &serial_line_coding);
    if (ret < 0) {
        log_w("usb_serial_set_line_coding error: %d", ret);
        return;
    }

    ret = usbh_serial_set_line_state(handle.serial_class, true, true);
    if (ret < 0) {
        log_w("usb_serial_set_line_state error: %d", ret);
        return;
    }

    handle.active = 1;

    log_d("USB serial active");

    for (;;) {
        ret = usbh_serial_bulk_in_transfer(handle.serial_class, bulk_in_buffer, sizeof(bulk_in_buffer), 0xfffffff);
        if (ret < 0 && ret != -USB_ERR_TIMEOUT) {
            log_w("usb_serial_bulk_in_transfer error: %d", ret);
            break;
        }

        if (ret > 0) {
            log_d("[len=%d], \"%.*s\"", ret, ret, bulk_in_buffer);

            osMutexAcquire(usb_serial_mutex, portMAX_DELAY);

            /* Append to the receive buffer */
            size_t copy_len = MIN(SERIAL_RECV_BUF_SIZE - usb_serial_recv_buf_len, ret);
            if (copy_len > 0) {
                memcpy(usb_serial_recv_buf + usb_serial_recv_buf_len, bulk_in_buffer, copy_len);
                usb_serial_recv_buf_len += copy_len;
            }

            osMutexRelease(usb_serial_mutex);

            osSemaphoreRelease(usb_serial_receive_semaphore);
        }
    }
    serial_task = NULL;
    osThreadExit();
}

#if 0
void usbh_serial_transmit_callback()
{
    osSemaphoreRelease(usb_serial_transmit_semaphore);
}
#endif

osStatus_t usbh_serial_transmit(const uint8_t *buf, size_t length)
{
    //TODO
    return osErrorParameter;
}

void usbh_serial_clear_receive_buffer()
{
    osMutexAcquire(usb_serial_mutex, portMAX_DELAY);
    memset(usb_serial_recv_buf, 0, sizeof(usb_serial_recv_buf));
    usb_serial_recv_buf_len = 0;
    osMutexRelease(usb_serial_mutex);
}

osStatus_t usbh_serial_receive_line(uint8_t *buf, size_t length, uint32_t ms_to_wait)
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
