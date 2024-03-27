/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBH_SERIAL_CDC_ACM_H
#define USBH_SERIAL_CDC_ACM_H

#include "usb_cdc.h"
#include "usbh_serial_class.h"

struct usbh_serial_cdc_acm {
    struct usbh_serial_class base;

#ifdef CONFIG_USBHOST_CDC_ACM_NOTIFY
    struct usb_endpoint_descriptor *intin;   /* INTR IN endpoint (optional) */
    struct usbh_urb intin_urb;
#endif

    uint8_t intf;
    uint8_t minor;
};

#endif /* USBH_SERIAL_CDC_ACM_H */
