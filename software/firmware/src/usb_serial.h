#ifndef USB_SERIAL_H
#define USB_SERIAL_H

#include <stdbool.h>

struct usbh_cdc_acm;
struct usbh_ftdi;
struct usbh_cp210x;
struct usbh_ch34x;

bool usbh_serial_init();

void usbh_serial_cdc_attached(struct usbh_cdc_acm *cdc_acm_class);
void usbh_serial_cdc_detached(struct usbh_cdc_acm *cdc_acm_class);

void usbh_serial_ftdi_attached(struct usbh_ftdi *ftdi_class);
void usbh_serial_ftdi_detached(struct usbh_ftdi *ftdi_class);

void usbh_serial_cp210x_attached(struct usbh_cp210x *cp210x_class);
void usbh_serial_cp210x_detached(struct usbh_cp210x *cp210x_class);

void usbh_serial_ch34x_attached(struct usbh_ch34x *ch34x_class);
void usbh_serial_ch34x_detached(struct usbh_ch34x *ch34x_class);

bool usbh_serial_is_attached();

#endif /* USB_SERIAL_H */
