/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBH_HUB_H
#define USBH_HUB_H

#include "usb_hub.h"

struct usbh_hub;

#ifdef __cplusplus
extern "C" {
#endif

int usbh_hub_set_feature(struct usbh_hub *hub, uint8_t port, uint8_t feature);
int usbh_hub_clear_feature(struct usbh_hub *hub, uint8_t port, uint8_t feature);

void usbh_hub_thread_wakeup(struct usbh_hub *hub);

int usbh_hub_initialize(struct usbh_bus *bus);
int usbh_hub_deinitialize(struct usbh_bus *bus);

void usbh_hub_run(struct usbh_hub *hub); /* (DK) */
void usbh_hub_stop(struct usbh_hub *hub); /* (DK) */

#ifdef __cplusplus
}
#endif

#endif /* USBH_HUB_H */
