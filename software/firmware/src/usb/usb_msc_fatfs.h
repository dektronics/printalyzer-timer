#ifndef USB_MSC_FATFS_H
#define USB_MSC_FATFS_H

#include <stdbool.h>

struct usbh_msc;

bool usbh_msc_fatfs_init();
void usbh_msc_fatfs_attached(struct usbh_msc *msc_class);
void usbh_msc_fatfs_detached(struct usbh_msc *msc_class);

bool usbh_msc_is_mounted();

#endif /* USB_MSC_FATFS_H */
