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

USBH_StatusTypeDef usb_host_init(void);

bool usb_msc_is_mounted();

#endif /* USB_HOST_H */
