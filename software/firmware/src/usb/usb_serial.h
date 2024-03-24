#ifndef USB_SERIAL_H
#define USB_SERIAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct usbh_cdc_acm;
struct usbh_serial_ftdi;
struct usbh_serial_cp210x;
struct usbh_serial_ch34x;
struct usbh_serial_pl2303;

typedef void (*usbh_serial_receive_callback_t)(uint8_t *data, size_t length);
typedef void (*usbh_serial_transmit_callback_t)();

bool usbh_serial_init(usbh_serial_receive_callback_t receive_callback, usbh_serial_transmit_callback_t transmit_callback);

void usbh_serial_cdc_attached(struct usbh_cdc_acm *cdc_acm_class);
void usbh_serial_cdc_detached(struct usbh_cdc_acm *cdc_acm_class);

void usbh_serial_ftdi_attached(struct usbh_serial_ftdi *ftdi_class);
void usbh_serial_ftdi_detached(struct usbh_serial_ftdi *ftdi_class);

void usbh_serial_cp210x_attached(struct usbh_serial_cp210x *cp210x_class);
void usbh_serial_cp210x_detached(struct usbh_serial_cp210x *cp210x_class);

void usbh_serial_ch34x_attached(struct usbh_serial_ch34x *ch34x_class);
void usbh_serial_ch34x_detached(struct usbh_serial_ch34x *ch34x_class);

void usbh_serial_pl2303_attached(struct usbh_serial_pl2303 *pl2303_class);
void usbh_serial_pl2303_detached(struct usbh_serial_pl2303 *pl2303_class);

bool usbh_serial_is_attached();

#endif /* USB_SERIAL_H */
