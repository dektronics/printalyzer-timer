/*
 * Enlarger Configuration
 *
 * The data structures and support functions here are indented to capture
 * the complete configuration of an enlarger.
 */
#ifndef ENLARGER_CONFIG_H
#define ENLARGER_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "contrast.h"
#include "util.h"

typedef enum {
    ENLARGER_CHANNEL_SET_WHITE = 0, /*!< One white channel */
    ENLARGER_CHANNEL_SET_RGB,       /*!< Three channels for RGB */
    ENLARGER_CHANNEL_SET_RGBW       /*!< Four channels for RGBW */
} enlarger_channel_set_t;

typedef enum {
    ENLARGER_CONTRAST_MODE_WHITE = 0, /*!< Always emit white light and expect external contrast filters */
    ENLARGER_CONTRAST_MODE_GREEN_BLUE /*!< Use configured per-grade Green+Blue combinations */
} enlarger_contrast_mode_t;

/**
 * DMX channel IDs for the enlarger.
 *
 * Note: Channel IDs are stored starting at 0,
 * but are displayed to the user starting from 1.
 */
typedef struct {
    uint16_t channel_red;
    uint16_t channel_green;
    uint16_t channel_blue;
    uint16_t channel_white;
} enlarger_grade_values_t;

/**
 * Enlarger control configuration, which is used to select between
 * Relay and DMX control and to set all the relevant DMX control
 * parameters.
 *
 * The use and interpretation of many of the more detailed values
 * will depend on the channel set and contrast mode settings.
 * It is easier to keep the structure all inclusive and handle
 * those variations on the other side.
 */
typedef struct {
    bool dmx_control;           /*!< True for DMX control, false for Relay control */
    enlarger_channel_set_t channel_set; /*!< The set of light channels this enlarger has */
    bool dmx_wide_mode;         /*!< True for 16-bit mode, false for 8-bit mode */
    uint16_t dmx_channel_red;   /*!< DMX channel ID for the Red light */
    uint16_t dmx_channel_green; /*!< DMX channel ID for the Green light */
    uint16_t dmx_channel_blue;  /*!< DMX channel ID for the Blue light */
    uint16_t dmx_channel_white; /*!< DMX channel ID for the White light */
    enlarger_contrast_mode_t contrast_mode; /*!< Selected method for controlling print contrast */
    uint16_t focus_value;       /*!< White light intensity to emit when in focus mode */
    uint16_t safe_value;        /*!< Red light intensity to emit when pausing mid-exposure sequence */
    enlarger_grade_values_t grade_values[CONTRAST_GRADE_MAX]; /*!< Per-grade light intensity settings */
} enlarger_control_t;

/**
 * Enlarger timing profile, which contains all the values captured
 * by the enlarger calibration process to more consistently time
 * multiple exposures of varying duration.
 */
typedef struct {
    /**
     * Time period (ms) from when the enlarger relay is activated until
     * its light level starts to rise.
     */
    uint32_t turn_on_delay;

    /**
     * Time period (ms) from when the enlarger light level starts to
     * rise until it approaches its peak.
     */
    uint32_t rise_time;

    /**
     * Time period (ms) at full output to get an exposure that is equivalent
     * to the output across the rise time period.
     */
    uint32_t rise_time_equiv;

    /**
     * Time period (ms) from when the enlarger relay is deactivated until
     * its light level starts to fall.
     */
    uint32_t turn_off_delay;

    /**
     * Time period (ms) from when the enlarger light level starts to
     * fall until it is completely off.
     */
    uint32_t fall_time;

    /**
     * Time period (ms) at full output to get an exposure that is equivalent
     * to the output across the fall time period.
     */
    uint32_t fall_time_equiv;

    /**
     * Color temperature (K) of the enlarger lamp.
     */
    uint32_t color_temperature;
} enlarger_timing_t;

/**
 * Enlarger configuration, which contains all the control and timing
 * information specific to the operation of a particular enlarger.
 */
typedef struct __enlarger_config_t {
    char name[PROFILE_NAME_LEN];       /*!< Enlarger configuration name */
    enlarger_control_t control;        /*!< Enlarger power control settings */
    enlarger_timing_t timing;          /*!< Enlarger on/off timing profile */

    /**
     * Type of contrast filter used when exposing the paper.
     *
     * This value is used purely for display purposes, to improve the
     * usefulness of the displayed contrast grade to those using dichroic
     * enlarger heads.
     */
    contrast_filter_t contrast_filter;

} enlarger_config_t;

/**
 * Check whether the configuration is valid, based on relationships between
 * the various values.
 */
bool enlarger_config_is_valid(const enlarger_config_t *config);

/**
 * Check whether the two enlarger configurations are equivalent.
 */
bool enlarger_config_compare(const enlarger_config_t *config1, const enlarger_config_t *config2);

/**
 * Clear out the provided configuration and set sensible default values.
 */
void enlarger_config_set_defaults(enlarger_config_t *config);

/**
 * This fills in portions of the enlarger configuration that are not saved,
 * and are derived from other values, such as half-grade contrast brightness
 * settings.
 */
void enlarger_config_recalculate(enlarger_config_t *config);

/**
 * Get the minimum supported exposure for the enlarger.
 */
uint32_t enlarger_config_min_exposure(const enlarger_config_t *config);

/**
 * Gets whether the enlarger has the necessary capabilities for RGB printing.
 */
bool enlarger_config_has_rgb(const enlarger_config_t *config);

#endif /* ENLARGER_CONFIG_H */
