#ifndef USBH_SERIAL_CLASS_H
#define USBH_SERIAL_CLASS_H

#include "usbh_core.h"
#include "usb_cdc.h"

struct usbh_serial_class_interface;

struct usbh_serial_class {
    struct usbh_serial_class_interface const *vtable;
    struct usbh_hubport *hport;
    struct usb_endpoint_descriptor *bulkin;  /* Bulk IN endpoint */
    struct usb_endpoint_descriptor *bulkout; /* Bulk OUT endpoint */
    struct usbh_urb bulkout_urb;
    struct usbh_urb bulkin_urb;

    struct cdc_line_coding line_coding;
};

struct usbh_serial_class_interface {
    int (*set_line_coding)(struct usbh_serial_class *serial_class, struct cdc_line_coding *line_coding);
    int (*get_line_coding)(struct usbh_serial_class *serial_class, struct cdc_line_coding *line_coding);
    int (*set_line_state)(struct usbh_serial_class *serial_class, bool dtr, bool rts);
    int (*bulk_in_transfer)(struct usbh_serial_class *serial_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout);
    int (*bulk_out_transfer)(struct usbh_serial_class *serial_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout);
};

#ifdef __cplusplus
extern "C" {
#endif

int usbh_serial_set_line_coding(struct usbh_serial_class *serial_class, struct cdc_line_coding *line_coding);
int usbh_serial_get_line_coding(struct usbh_serial_class *serial_class, struct cdc_line_coding *line_coding);
int usbh_serial_set_line_state(struct usbh_serial_class *serial_class, bool dtr, bool rts);
int usbh_serial_bulk_in_transfer(struct usbh_serial_class *serial_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout);
int usbh_serial_bulk_out_transfer(struct usbh_serial_class *serial_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout);

void usbh_serial_run(struct usbh_serial_class *serial_class);
void usbh_serial_stop(struct usbh_serial_class *serial_class);

#ifdef __cplusplus
}
#endif

#endif /* USBH_SERIAL_CLASS_H */
