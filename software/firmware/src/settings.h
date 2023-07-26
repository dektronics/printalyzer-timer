/*
 * System settings which are mostly user-configurable
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <stm32f4xx_hal.h>
#include <cmsis_os.h>
#include <stdint.h>
#include <stdbool.h>
#include "buzzer.h"
#include "exposure_state.h"
#include "enlarger_config.h"
#include "paper_profile.h"
#include "step_wedge.h"

#define MAX_ENLARGER_CONFIGS 16
#define MAX_PAPER_PROFILES 16

typedef enum {
    SAFELIGHT_MODE_OFF = 0, /*!< Safelight is always off */
    SAFELIGHT_MODE_ON,      /*!< Safelight is off when the enlarger is on */
    SAFELIGHT_MODE_AUTO     /*!< Safelight is off during metering and printing */
} safelight_mode_t;

typedef enum {
    SAFELIGHT_CONTROL_RELAY = 0, /*!< Safelight power is relay controlled */
    SAFELIGHT_CONTROL_DMX,       /*!< Safelight power is DMX controlled */
    SAFELIGHT_CONTROL_BOTH       /*!< Safelight power is controlled by both relay and DMX */
} safelight_control_t;

typedef struct {
    safelight_mode_t mode;       /*!< Safelight operation mode */
    safelight_control_t control; /*!< Set power control method */
    uint16_t dmx_address;        /*!< DMX device address */
    bool dmx_wide_mode;          /*!< True for 16-bit mode, false for 8-bit mode */
    uint16_t dmx_on_value;       /*!< Brightness value for the on state */
} safelight_config_t;

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
 * @param hi2c Handle for the I2C peripheral used by the EEPROM
 * @param i2c_mutex Mutex used to guard access to the I2C peripheral
 */
HAL_StatusTypeDef settings_init(I2C_HandleTypeDef *hi2c, osMutexId_t i2c_mutex);

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
contrast_grade_t settings_get_default_contrast_grade();

void settings_set_default_contrast_grade(contrast_grade_t contrast_grade);

/**
 * Default exposure adjustment step size.
 */
exposure_adjustment_increment_t settings_get_default_step_size();

void settings_set_default_step_size(exposure_adjustment_increment_t step_size);

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
 * Index of the default enlarger configuration.
 *
 * @return An index value from 0 to 15
 */
uint8_t settings_get_default_enlarger_config_index();

void settings_set_default_enlarger_config_index(uint8_t index);

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
 * Safelight control configuration.
 */
bool settings_get_safelight_config(safelight_config_t *safelight_config);

bool settings_set_safelight_config(const safelight_config_t *safelight_config);

/**
 * Get the name of the enlarger configuration saved at the specified index
 *
 * This function is intended to provide an more efficient way of building
 * a list of saved configurations than loading all of them into memory.
 *
 * @param name Pointer to a buffer with at least 32 bytes.
 * @param index An index value from 0 to 15
 * @return True if the configuration name was successfully loaded, false otherwise
 */
bool settings_get_enlarger_config_name(char *name, uint8_t index);

/**
 * Get the enlarger configuration saved at the specified index
 *
 * @param profile Pointer to the configuration struct to load data into
 * @param index An index value from 0 to 15
 * @return True if the configuration was successfully loaded, false if no valid
 *         configuration was found at the specified index.
 */
bool settings_get_enlarger_config(enlarger_config_t *config, uint8_t index);

/**
 * Save an enlarger configuration at the specified index
 *
 * @param profile Pointer to the configuration struct to save data from
 * @param index An index value from 0 to 15
 * @return True if the configuration was successfully saved
 */
bool settings_set_enlarger_config(const enlarger_config_t *config, uint8_t index);

/**
 * Clear any enlarger configuration saved at the specified index
 *
 * @param index An index value from 0 to 15
 */
void settings_clear_enlarger_config(uint8_t index);

/**
 * Get the paper profile saved at the specified index
 *
 * @param profile Pointer to the profile struct to load data into
 * @param index An index value from 0 to 15
 *
 * @return True if the profile was successfully loaded, false if no valid
 *         profile was found at the specified index.
 */
bool settings_get_paper_profile(paper_profile_t *profile, uint8_t index);

/**
 * Save a paper profile at the specified index
 *
 * @param profile Pointer to the profile struct to save data from
 * @param index An index value from 0 to 15
 * @return True if the profile was successfully saved
 */
bool settings_set_paper_profile(const paper_profile_t *profile, uint8_t index);

/**
 * Clear any paper profile saved at the specified index
 *
 * @param index An index value from 0 to 15
 */
void settings_clear_paper_profile(uint8_t index);

/**
 * Get the configured step wedge.
 *
 * Since step wedges are of variable size, the result will be dynamically
 * allocated. As such, it must be freed when finished using it.
 *
 * @param wedge Pointer to the loaded step wedge configuration.
 * @return True if a wedge configuration was successfully loaded, false
 *         otherwise.
 */
bool settings_get_step_wedge(step_wedge_t **wedge);

/**
 * Save the configured step wedge.
 *
 * @param wedge Pointer to the step wedge profile to save.
 * @return True if the profile was successfully saved.
 */
bool settings_set_step_wedge(const step_wedge_t *wedge);

#endif /* SETTINGS_H */
