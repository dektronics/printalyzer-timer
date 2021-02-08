#ifndef USB_HOST_H
#define USB_HOST_H

#include <stm32f4xx.h>
#include <stm32f4xx_hal.h>
#include <stdbool.h>
#include <usbh_def.h>

typedef enum {
  APPLICATION_IDLE = 0,
  APPLICATION_START,
  APPLICATION_READY,
  APPLICATION_DISCONNECT
} usb_app_state_t;

typedef enum {
    USB_SERIAL_DATA_BITS_5 = 5,
    USB_SERIAL_DATA_BITS_6 = 6,
    USB_SERIAL_DATA_BITS_7 = 7,
    USB_SERIAL_DATA_BITS_8 = 8
} usb_serial_data_bits_t;

typedef enum {
    USB_SERIAL_STOP_BITS_1 = 0,
    USB_SERIAL_STOP_BITS_1_5,
    USB_SERIAL_STOP_BITS_2
} usb_serial_stop_bits_t;

typedef enum {
    USB_SERIAL_PARITY_NONE = 0,
    USB_SERIAL_PARITY_ODD,
    USB_SERIAL_PARITY_EVEN,
    USB_SERIAL_PARITY_MARK,
    USB_SERIAL_PARITY_SPACE
} usb_serial_parity_t;

typedef struct {
    uint32_t baud_rate;
    usb_serial_data_bits_t data_bits;
    usb_serial_stop_bits_t stop_bits;
    usb_serial_parity_t parity;
} usb_serial_line_coding_t;

USBH_StatusTypeDef usb_host_init(void);

bool usb_msc_is_mounted();

bool usb_serial_is_attached();
USBH_StatusTypeDef usb_serial_set_line_coding(const usb_serial_line_coding_t *line_coding);
USBH_StatusTypeDef usb_serial_get_line_coding(usb_serial_line_coding_t *line_coding);

USBH_StatusTypeDef usb_serial_transmit(const uint8_t *buf, size_t length);
USBH_StatusTypeDef usb_serial_clear_receive_buffer();
USBH_StatusTypeDef usb_serial_receive_line(uint8_t *buf, size_t length, uint32_t ms_to_wait);

#endif /* USB_HOST_H */
