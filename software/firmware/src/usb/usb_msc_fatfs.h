#ifndef USB_MSC_FATFS_H
#define USB_MSC_FATFS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct usbh_msc;

bool usbh_msc_fatfs_init();
void usbh_msc_fatfs_attached(struct usbh_msc *msc_class);
void usbh_msc_fatfs_detached(struct usbh_msc *msc_class);

bool usbh_msc_is_mounted(uint8_t num);
const char *usbh_msc_drive_label(uint8_t num);
bool usbh_msc_drive_serial_fixed_index(uint8_t num, char *buf, size_t len);
bool usbh_msc_drive_serial(uint8_t num, char *buf, size_t len);

/**
 * Get the unique ID to represent the selected device.
 *
 * This ID follows the format suggested in the USB MSC specification,
 * which is as follows:
 * "The host may generate a globally unique identifier by concatenating
 * the 16 bit idVendor, the 16 bit idProduct and the value represented
 * by the last 12 characters of the string descriptor indexed by iSerialNumber."
 *
 * @param num Selected device number
 * @param buf Buffer to store the unique ID into
 * @param len Length of the buffer, must be at least 10 bytes
 * @return
 */
bool usbh_msc_drive_unique_id(uint8_t num, uint8_t *buf, size_t len);

uint8_t usbh_msc_max_drives();

#endif /* USB_MSC_FATFS_H */
