/*
 * System settings which are mostly user-configurable
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <stm32f4xx_hal.h>
#include <stdint.h>
#include <stdbool.h>
#include "buzzer.h"
#include "exposure_state.h"
#include "enlarger_profile.h"

#define MAX_ENLARGER_PROFILES 16
#define MAX_PAPER_PROFILES 16

typedef enum {
    SAFELIGHT_MODE_OFF = 0, /* Safelight is always off */
    SAFELIGHT_MODE_ON,      /* Safelight is off when the enlarger is on */
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
 * Initialize the settings store and load any persisted values.
 *
 * @param Pointer to a handle for the I2C peripheral used by the EEPROM
 */
HAL_StatusTypeDef settings_init(I2C_HandleTypeDef *hi2c);

/**
 * Clear the settings store to factory blank
 *
 * @param Pointer to a handle for the I2C peripheral used by the EEPROM
 */
HAL_StatusTypeDef settings_clear(I2C_HandleTypeDef *hi2c);

/**
 * Default exposure time displayed at startup and reset
 *
 * @return Exposure time in milliseconds
 */
uint32_t settings_get_default_exposure_time();

void settings_set_default_exposure_time(uint32_t exposure_time);

/**
 * Default contrast grade displayed at startup and reset
 */
exposure_contrast_grade_t settings_get_default_contrast_grade();

void settings_set_default_contrast_grade(exposure_contrast_grade_t contrast_grade);

/**
 * Default exposure adjustment step size.
 */
exposure_adjustment_increment_t settings_get_default_step_size();

void settings_set_default_step_size(exposure_adjustment_increment_t step_size);

/**
 * Behavior of the safelight control
 */
safelight_mode_t settings_get_safelight_mode();

void settings_set_safelight_mode(safelight_mode_t mode);

/**
 * Enlarger timeout when in focus mode.
 *
 * @return Timeout in milliseconds
 */
uint32_t settings_get_enlarger_focus_timeout();

void settings_set_enlarger_focus_timeout(uint32_t timeout);

/**
 * Brightness for the graphic display module.
 *
 * @return A brightness value from 0 to 15
 */
uint8_t settings_get_display_brightness();

void settings_set_display_brightness(uint8_t brightness);

/**
 * Brightness for the illumination and indicator LEDs.
 *
 * @return A brightness value from 0 to 255
 */
uint8_t settings_get_led_brightness();

void settings_set_led_brightness(uint8_t brightness);

/**
 * Volume for the piezo buzzer.
 */
buzzer_volume_t settings_get_buzzer_volume();

void settings_set_buzzer_volume(buzzer_volume_t volume);

/**
 * Default behavior mode for teststrip exposures.
 */
teststrip_mode_t settings_get_teststrip_mode();

void settings_set_teststrip_mode(teststrip_mode_t mode);

/**
 * Default patch count for teststrip exposures.
 */
teststrip_patches_t settings_get_teststrip_patches();

void settings_set_teststrip_patches(teststrip_patches_t patches);

/**
 * Index of the default enlarger profile.
 *
 * @return An index value from 0 to 15
 */
uint8_t settings_get_default_enlarger_profile_index();

void settings_set_default_enlarger_profile_index(uint8_t index);

/**
 * Index of the default paper profile.
 *
 * @return An index value from 0 to 15
 */
uint8_t settings_get_default_paper_profile_index();

void settings_set_default_paper_profile_index(uint8_t index);

/**
 * TCS3472 sensor's glass attenuation factor.
 */
float settings_get_tcs3472_ga_factor();

void settings_set_tcs3472_ga_factor(float value);

/**
 * Get the enlarger profile saved at the specified index
 *
 * @param profile Pointer to the profile struct to load data into
 * @param index An index value from 0 to 15
 *
 * @return True if the profile was successfully loaded, false if no valid
 *         profile was found at the specified index.
 */
bool settings_get_enlarger_profile(enlarger_profile_t *profile, uint8_t index);

/**
 * Save an enlarger profile at the specified index
 *
 * @param profile Pointer to the profile struct to save data from
 * @param index An index value from 0 to 15
 * @return True if the profile was successfully saved
 */
bool settings_set_enlarger_profile(const enlarger_profile_t *profile, uint8_t index);

/**
 * Clear any enlarger profile saved at the specified index
 *
 * @param index An index value from 0 to 15
 */
void settings_clear_enlarger_profile(uint8_t index);

#endif /* SETTINGS_H */
