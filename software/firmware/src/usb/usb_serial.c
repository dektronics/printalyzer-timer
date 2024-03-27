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

static usbh_serial_receive_callback_t serial_receive_callback;
static usbh_serial_transmit_callback_t serial_transmit_callback;

static void usb_serial_thread(void *argument);

bool usbh_serial_init(usbh_serial_receive_callback_t receive_callback, usbh_serial_transmit_callback_t transmit_callback)
{
    serial_task = NULL;
    serial_receive_callback = receive_callback;
    serial_transmit_callback = transmit_callback;
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
            if (serial_receive_callback) {
                serial_receive_callback(bulk_in_buffer, ret);
            }
        }
    }
    serial_task = NULL;
    osThreadExit();
}
