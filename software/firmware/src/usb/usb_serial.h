#ifndef USB_SERIAL_H
#define USB_SERIAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct usbh_serial_class;

typedef void (*usbh_serial_receive_callback_t)(uint8_t *data, size_t length);
typedef void (*usbh_serial_transmit_callback_t)();

bool usbh_serial_init(usbh_serial_receive_callback_t receive_callback, usbh_serial_transmit_callback_t transmit_callback);

void usbh_serial_attached(struct usbh_serial_class *serial_class);
void usbh_serial_detached(struct usbh_serial_class *serial_class);

bool usbh_serial_is_attached();

#endif /* USB_SERIAL_H */
