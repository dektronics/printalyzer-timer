#include "usbh_serial_class.h"

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

int usbh_serial_bulk_in_transfer(struct usbh_serial_class *serial_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout)
{
    int ret;
    if (serial_class->vtable->bulk_in_transfer) {
        ret = serial_class->vtable->bulk_in_transfer(serial_class, buffer, buflen, timeout);
    } else {
        struct usbh_urb *urb = &serial_class->bulkin_urb;

        usbh_bulk_urb_fill(urb, serial_class->hport, serial_class->bulkin, buffer, buflen, timeout, NULL, NULL);
        ret = usbh_submit_urb(urb);
        if (ret == 0) {
            ret = urb->actual_length;
        }
    }
    return ret;
}

int usbh_serial_bulk_out_transfer(struct usbh_serial_class *serial_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout)
{
    int ret;
    if (serial_class->vtable->bulk_out_transfer) {
        ret = serial_class->vtable->bulk_out_transfer(serial_class, buffer, buflen, timeout);
    } else {
        struct usbh_urb *urb = &serial_class->bulkout_urb;

        usbh_bulk_urb_fill(urb, serial_class->hport, serial_class->bulkout, buffer, buflen, timeout, NULL, NULL);
        ret = usbh_submit_urb(urb);
        if (ret == 0) {
            ret = urb->actual_length;
        }
    }
    return ret;
}
