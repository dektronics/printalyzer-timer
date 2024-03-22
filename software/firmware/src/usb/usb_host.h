#ifndef USB_HOST_H
#define USB_HOST_H

#include <stdbool.h>

bool usb_host_init();

bool usb_hub_init();

bool usb_msc_is_mounted();
bool usb_serial_is_attached();

#endif /* USB_HOST_H */
