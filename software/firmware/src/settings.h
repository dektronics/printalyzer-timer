/*
 * System settings which are mostly user-configurable
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>
#include "buzzer.h"
#include "exposure_state.h"

typedef enum {
    SAFELIGHT_MODE_OFF = 0, /* Safelight is always off */
    SAFELIGHT_MODE_ON,      /* Safelight is off when the enlarger is off */
    SAFELIGHT_MODE_AUTO     /* Safelight is off during metering and printing */
} safelight_mode_t;

typedef enum {
    TESTSTRIP_MODE_INCREMENTAL = 0,
    TESTSTRIP_MODE_SEPARATE
} teststrip_mode_t;

typedef enum {
    TESTSTRIP_PATCHES_7 = 0,
    TESTSTRIP_PATCHES_5
} teststrip_patches_t;

/**
 * Default exposure time displayed at startup and reset
 *
 * @return Exposure time in milliseconds
 */
uint32_t settings_get_default_exposure_time();

/**
 * Default contrast grade displayed at startup and reset
 */
exposure_contrast_grade_t settings_get_default_contrast_grade();

/**
 * Default exposure adjustment step size.
 */
exposure_adjustment_increment_t settings_get_default_step_size();

/**
 * Behavior of the safelight control
 */
safelight_mode_t settings_get_safelight_mode();

/**
 * Enlarger timeout when in focus mode.
 *
 * @return Timeout in milliseconds
 */
uint32_t settings_get_enlarger_focus_timeout();

/**
 * Brightness for the graphic display module.
 *
 * @return A brightness value from 0 to 15
 */
uint8_t settings_get_display_brightness();

/**
 * Brightness for the illumination and indicator LEDs.
 *
 * @return A brightness value from 0 to 255
 */
uint8_t settings_get_led_brightness();

/**
 * Volume for the piezo buzzer.
 */
buzzer_volume_t settings_get_buzzer_volume();

/**
 * Default behavior mode for teststrip exposures.
 */
teststrip_mode_t settings_get_teststrip_mode();

/**
 * Default patch count for teststrip exposures.
 */
teststrip_patches_t settings_get_teststrip_patches();

#endif /* SETTINGS_H */
