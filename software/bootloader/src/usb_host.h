#ifndef USB_HOST_H
#define USB_HOST_H

#include <stm32f4xx.h>
#include <stm32f4xx_hal.h>
#include <stdbool.h>

//typedef enum {
//  APPLICATION_IDLE = 0,
//  APPLICATION_START,
//  APPLICATION_READY,
//  APPLICATION_DISCONNECT
//} usb_app_state_t;

void usb_host_init(void);
void usb_host_deinit(void);
void usb_host_process(void);

bool usb_msc_is_mounted();
void usb_msc_unmount();

#endif /* USB_HOST_H */
