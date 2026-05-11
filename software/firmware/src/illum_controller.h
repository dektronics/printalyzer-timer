/*
 * Illumination Controller
 *
 * Responsible for managing the actual on/off control of any part
 * of the device that emits light, with the exception of the
 * enlarger relay.
 * This includes the safelights, the display lighting,
 * and the panel LEDs.
 * Also responsible for controlling whether the DMX control port
 * is transmitting.
 */

#ifndef ILLUM_CONTROLLER_H
#define ILLUM_CONTROLLER_H

#include <led.h>

typedef enum {
    ILLUM_SAFELIGHT_HOME = 0,
    ILLUM_SAFELIGHT_FOCUS,
    ILLUM_SAFELIGHT_EXPOSURE,
    ILLUM_SAFELIGHT_MEASUREMENT,
} illum_safelight_t;

typedef enum {
    ILLUM_BUTTON_NORMAL = 0,
    ILLUM_BUTTON_BRIGHT,
    ILLUM_BUTTON_DIM
} illum_panel_brightness_t;

void illum_controller_init();

/**
 * Refresh the illumination configuration.
 *
 * Must be called once at startup, then later after any configuration
 * change to the illumination hardware.
 */
void illum_controller_refresh();

void illum_controller_safelight_state(illum_safelight_t mode);

void illum_controller_set_panel(led_t panel_led, illum_panel_brightness_t panel_brightness);

bool illum_controller_is_blackout();

void illum_controller_keypad_blackout_callback(bool enabled, void *user_data);

#endif /* ILLUM_CONTROLLER_H */
