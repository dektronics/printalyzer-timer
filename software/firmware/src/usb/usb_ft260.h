#ifndef USB_METER_PROBE_H
#define USB_METER_PROBE_H

#include <stdint.h>
#include <stdbool.h>

#define FT260_SERIAL_SIZE 32

typedef enum {
    FT260_DEFAULT = 0,
    FT260_METER_PROBE
} ft260_device_type_t;

typedef enum {
    FT260_EVENT_INTERRUPT = 0,
    FT260_EVENT_BUTTON_DOWN,
    FT260_EVENT_BUTTON_UP
} ft260_device_event_t;

struct usbh_hid;
typedef struct i2c_handle_t i2c_handle_t;
typedef struct ft260_device_t ft260_device_t;

typedef void (*ft260_device_event_callback_t)(ft260_device_t *device, ft260_device_event_t event_type, uint32_t ticks);

bool usbh_ft260_init();
void usbh_ft260_attached(struct usbh_hid *hid_class);
void usbh_ft260_detached(struct usbh_hid *hid_class);

bool usbh_ft260_is_attached(ft260_device_type_t device_type);

ft260_device_t *usbh_ft260_get_device(ft260_device_type_t device_type);
void usbh_ft260_set_device_callback(ft260_device_t *device, ft260_device_event_callback_t callback);
bool usbh_ft260_get_device_serial_number(const ft260_device_t *device, char *str);
i2c_handle_t *usbh_ft260_get_device_i2c(ft260_device_t *device);
bool usbh_ft260_set_device_gpio(ft260_device_t *device, bool value);

#endif /* USB_METER_PROBE_H */
