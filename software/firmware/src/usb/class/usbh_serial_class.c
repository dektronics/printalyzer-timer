#include "usbh_serial_class.h"

#include <FreeRTOS.h>
#include <atomic.h>

volatile uint8_t usbh_serial_count = 0;
volatile static uint32_t usbh_serial_dev_in_use = 0;

bool usbh_serial_increment_count(uint8_t *devnum)
{
    bool has_dev = false;
    uint8_t devno;

    ATOMIC_ENTER_CRITICAL();
    {
        for (devno = 0; devno < CONFIG_USBHOST_MAX_SERIAL_CLASS; devno++) {
            if ((usbh_serial_dev_in_use & (1 << devno)) == 0) {
                usbh_serial_dev_in_use |= (1 << devno);
                has_dev = true;
                break;
            }
        }
    }
    ATOMIC_EXIT_CRITICAL();

    if (has_dev && devnum) {
        *devnum = devno;
    }
    return has_dev;
}

void usbh_serial_decrement_count(uint8_t devnum)
{
    uint8_t devno = devnum;

    ATOMIC_ENTER_CRITICAL();
    {
        if (devnum < 32) {
            usbh_serial_dev_in_use &= ~(1 << devno);
        }
    }
    ATOMIC_EXIT_CRITICAL();
}

int usbh_serial_set_line_coding(struct usbh_serial_class *serial_class, struct cdc_line_coding *line_coding)
{
    return serial_class->vtable->set_line_coding(serial_class, line_coding);
}

int usbh_serial_get_line_coding(struct usbh_serial_class *serial_class, struct cdc_line_coding *line_coding)
{
    return serial_class->vtable->get_line_coding(serial_class, line_coding);
}

int usbh_serial_set_line_state(struct usbh_serial_class *serial_class, bool dtr, bool rts)
{
    return serial_class->vtable->set_line_state(serial_class, dtr, rts);
}

int usbh_serial_bulk_in_transfer(struct usbh_serial_class *serial_class, uint8_t *buffer, uint32_t buflen,
    uint32_t timeout, usbh_complete_callback_t complete, void *arg)
{
    int ret;
    if (serial_class->vtable->bulk_in_transfer) {
        ret = serial_class->vtable->bulk_in_transfer(serial_class, buffer, buflen, timeout, complete, arg);
    } else {
        struct usbh_urb *urb = &serial_class->bulkin_urb;

        usbh_bulk_urb_fill(urb, serial_class->hport, serial_class->bulkin, buffer, buflen,
            timeout, complete, arg);
        ret = usbh_submit_urb(urb);
        if (ret == 0) {
            ret = (int)urb->actual_length;
        }
    }
    return ret;
}

int usbh_serial_bulk_in_check_result(struct usbh_serial_class *serial_class, uint8_t *buffer, int nbytes)
{
    int ret;
    if (serial_class->vtable->bulk_in_check_result) {
        ret = serial_class->vtable->bulk_in_check_result(serial_class, buffer, nbytes);
    } else {
        ret = nbytes;
    }
    return ret;
}

int usbh_serial_bulk_out_transfer(struct usbh_serial_class *serial_class, uint8_t *buffer, uint32_t buflen,
    uint32_t timeout, usbh_complete_callback_t complete, void *arg)
{
    int ret;
    if (serial_class->vtable->bulk_out_transfer) {
        ret = serial_class->vtable->bulk_out_transfer(serial_class, buffer, buflen, timeout, complete, arg);
    } else {
        struct usbh_urb *urb = &serial_class->bulkout_urb;

        usbh_bulk_urb_fill(urb, serial_class->hport, serial_class->bulkout, buffer, buflen,
            timeout, complete, arg);
        ret = usbh_submit_urb(urb);
        if (ret == 0) {
            ret = (int)urb->actual_length;
        }
    }
    return ret;
}

__WEAK void usbh_serial_run(struct usbh_serial_class *serial_class)
{
}

__WEAK void usbh_serial_stop(struct usbh_serial_class *serial_class)
{
}
