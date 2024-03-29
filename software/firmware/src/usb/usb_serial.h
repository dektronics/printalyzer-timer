#ifndef USB_SERIAL_H
#define USB_SERIAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <cmsis_os.h>

struct usbh_serial_class;

bool usbh_serial_init();

void usbh_serial_attached(struct usbh_serial_class *serial_class);
void usbh_serial_detached(struct usbh_serial_class *serial_class);

bool usbh_serial_is_attached();

osStatus_t usbh_serial_transmit(const uint8_t *buf, size_t length);
void usbh_serial_clear_receive_buffer();
osStatus_t usbh_serial_receive_line(uint8_t *buf, size_t length);

#endif /* USB_SERIAL_H */
