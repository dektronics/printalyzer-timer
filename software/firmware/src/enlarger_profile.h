#ifndef ENLARGER_PROFILE_H
#define ENLARGER_PROFILE_H

#include <stdint.h>
#include <stdbool.h>

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

} enlarger_profile_t;

/**
 * Check whether the profile is valid, based on relationships between
 * the various values.
 */
bool enlarger_profile_is_valid(const enlarger_profile_t *profile);

/**
 * Get the minimum supported exposure for the profile.
 */
uint32_t enlarger_profile_min_exposure(const enlarger_profile_t *profile);

#endif /* ENLARGER_PROFILE_H */