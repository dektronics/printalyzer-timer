#ifndef USB_HOST_H
#define USB_HOST_H

#include <stdbool.h>
#include <cmsis_os.h>

typedef struct i2c_handle_t i2c_handle_t;

bool usb_host_init();

bool usb_msc_is_mounted();
bool usb_serial_is_attached();
bool usb_meter_probe_is_attached();

osStatus_t usb_serial_transmit(const uint8_t *buf, size_t length);
void usb_serial_clear_receive_buffer();
osStatus_t usb_serial_receive_line(uint8_t *buf, size_t length);

#endif /* USB_HOST_H */
