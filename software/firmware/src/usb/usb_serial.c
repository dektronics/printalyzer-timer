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

typedef enum {
    USB_SERIAL_UNKNOWN = 0,
    USB_SERIAL_CDC_ACM,
    USB_SERIAL_FTDI,
    USB_SERIAL_CP210X,
    USB_SERIAL_CH34X,
    USB_SERIAL_PL2303
} usb_serial_driver_t;

typedef struct {
    struct usbh_serial_class *serial_class;
    usb_serial_driver_t driver;
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

static void usb_serial_attached();
static void usb_serial_detached();
static void usb_serial_thread(void *argument);

bool usbh_serial_init(usbh_serial_receive_callback_t receive_callback, usbh_serial_transmit_callback_t transmit_callback)
{
    serial_task = NULL;
    serial_receive_callback = receive_callback;
    serial_transmit_callback = transmit_callback;
    return true;
}

void usbh_serial_cdc_attached(struct usbh_serial_cdc_acm *cdc_acm_class)
{
    if (handle.connected) {
        return;
    }
    handle.serial_class = (struct usbh_serial_class *)cdc_acm_class;
    handle.driver = USB_SERIAL_CDC_ACM;
    handle.connected = 1;
    usb_serial_attached();
}

void usbh_serial_cdc_detached(struct usbh_serial_cdc_acm *cdc_acm_class)
{
    if (handle.driver == USB_SERIAL_CDC_ACM && (struct usbh_serial_cdc_acm *)handle.serial_class == cdc_acm_class) {
        usb_serial_detached();
    }
}

void usbh_serial_ftdi_attached(struct usbh_serial_ftdi *ftdi_class)
{
    if (handle.connected) {
        return;
    }
    handle.serial_class = (struct usbh_serial_class *)ftdi_class;
    handle.driver = USB_SERIAL_FTDI;
    handle.connected = 1;
    usb_serial_attached();
}

void usbh_serial_ftdi_detached(struct usbh_serial_ftdi *ftdi_class)
{
    if (handle.driver == USB_SERIAL_FTDI && (struct usbh_serial_ftdi *)handle.serial_class == ftdi_class) {
        usb_serial_detached();
    }
}

void usbh_serial_cp210x_attached(struct usbh_serial_cp210x *cp210x_class)
{
    if (handle.connected) {
        return;
    }
    handle.serial_class = (struct usbh_serial_class *)cp210x_class;
    handle.driver = USB_SERIAL_CP210X;
    handle.connected = 1;
    usb_serial_attached();
}

void usbh_serial_cp210x_detached(struct usbh_serial_cp210x *cp210x_class)
{
    if (handle.driver == USB_SERIAL_CP210X && (struct usbh_serial_cp210x *)handle.serial_class == cp210x_class) {
        usb_serial_detached();
    }
}

void usbh_serial_ch34x_attached(struct usbh_serial_ch34x *ch34x_class)
{
    if (handle.connected) {
        return;
    }
    handle.serial_class = (struct usbh_serial_class *)ch34x_class;
    handle.driver = USB_SERIAL_CH34X;
    handle.connected = 1;
    usb_serial_attached();
}

void usbh_serial_ch34x_detached(struct usbh_serial_ch34x *ch34x_class)
{
    if (handle.driver == USB_SERIAL_CH34X && (struct usbh_serial_ch34x *)handle.serial_class == ch34x_class) {
        usb_serial_detached();
    }
}

void usbh_serial_pl2303_attached(struct usbh_serial_pl2303 *pl2303_class)
{
    if (handle.connected) {
        return;
    }
    handle.serial_class = (struct usbh_serial_class *)pl2303_class;
    handle.driver = USB_SERIAL_PL2303;
    handle.connected = 1;
    usb_serial_attached();
}

void usbh_serial_pl2303_detached(struct usbh_serial_pl2303 *pl2303_class)
{
    if (handle.driver == USB_SERIAL_PL2303 && (struct usbh_serial_pl2303 *)handle.serial_class == pl2303_class) {
        usb_serial_detached();
    }
}

void usb_serial_attached()
{
    if (!serial_task) {
        serial_task = osThreadNew(usb_serial_thread, NULL, &serial_task_attrs);
    }
}

void usb_serial_detached()
{
    if (handle.connected) {
        memset(&handle, 0, sizeof(usb_serial_handle_t));
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
            if (handle.driver == USB_SERIAL_FTDI) {
                /* FTDI returns two modem status bytes that need to be skipped */
                if (ret > 2) {
                    //log_d("[len=%d], \"%.*s\"", ret - 2, ret - 2, bulk_in_buffer + 2);
                    if (serial_receive_callback) {
                        serial_receive_callback(bulk_in_buffer + 2, ret - 2);
                    }
                }
            } else {
                log_d("[len=%d], \"%.*s\"", ret, ret, bulk_in_buffer);
                if (serial_receive_callback) {
                    serial_receive_callback(bulk_in_buffer, ret);
                }
            }
        }
    }
    serial_task = NULL;
    osThreadExit();
}
