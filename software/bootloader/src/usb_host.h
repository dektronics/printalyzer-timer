#ifndef USB_HOST_H
#define USB_HOST_H

#include <stm32f4xx.h>
#include <stm32f4xx_hal.h>
#include <stdbool.h>

bool usb_host_init();
void usb_host_deinit();
bool usb_hub_init();

bool usb_msc_is_mounted();
int usb_msc_find_device(const uint8_t *dev_serial, size_t len);
void usb_msc_unmount();

#endif /* USB_HOST_H */
