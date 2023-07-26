#ifndef ENLARGER_CONFIG_H
#define ENLARGER_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "util.h"

typedef struct {
    /*
     * Time period (ms) from when the enlarger relay is activated until
     * its light level starts to rise.
     */
    uint32_t turn_on_delay;

    /*
     * Time period (ms) from when the enlarger light level starts to
     * rise until it approaches its peak.
     */
    uint32_t rise_time;

    /*
     * Time period (ms) at full output to get an exposure that is equivalent
     * to the output across the rise time period.
     */
    uint32_t rise_time_equiv;

    /*
     * Time period (ms) from when the enlarger relay is deactivated until
     * its light level starts to fall.
     */
    uint32_t turn_off_delay;

    /*
     * Time period (ms) from when the enlarger light level starts to
     * fall until it is completely off.
     */
    uint32_t fall_time;

    /*
     * Time period (ms) at full output to get an exposure that is equivalent
     * to the output across the fall time period.
     */
    uint32_t fall_time_equiv;

    /*
     * Color temperature (K) of the enlarger lamp.
     */
    uint32_t color_temperature;
} enlarger_timing_t;

/**
 * Enlarger configuration, which contains all the control and timing
 * information specific to the operation of a particular enlarger.
 */
typedef struct {
    /*
     * Name of the enlarger configuration, to be displayed in settings.
     */
    char name[PROFILE_NAME_LEN];

    enlarger_timing_t timing; /*!< Enlarger on/off timing profile */

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
 * Get the minimum supported exposure for the enlarger.
 */
uint32_t enlarger_config_min_exposure(const enlarger_config_t *config);

#endif /* ENLARGER_CONFIG_H */
