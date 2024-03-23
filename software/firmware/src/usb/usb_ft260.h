#ifndef USB_METER_PROBE_H
#define USB_METER_PROBE_H

#include <stdint.h>
#include <stdbool.h>

struct usbh_hid;
typedef struct i2c_handle_t i2c_handle_t;

bool usbh_ft260_init();
void usbh_ft260_attached(struct usbh_hid *hid_class);
void usbh_ft260_detached(struct usbh_hid *hid_class);

bool usbh_ft260_is_attached(uint16_t vid, uint16_t pid);
i2c_handle_t *usbh_ft260_get_i2c_interface();

#endif /* USB_METER_PROBE_H */
