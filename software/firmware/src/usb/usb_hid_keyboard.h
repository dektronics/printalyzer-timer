#ifndef USB_HID_KEYBOARD_H
#define USB_HID_KEYBOARD_H

#include <stdbool.h>

struct usbh_hid;

bool usbh_hid_keyboard_init();
void usbh_hid_keyboard_attached(struct usbh_hid *hid_class);
void usbh_hid_keyboard_detached(struct usbh_hid *hid_class);

#endif /* USB_HID_KEYBOARD_H */
