#include "enlarger_profile.h"

#include <string.h>

bool enlarger_profile_is_valid(const enlarger_profile_t *profile)
{
    if (!profile) { return false; }

    // All values are limited to a sensible maximum of just over a
    // minute, so we do not need to worry about overflow when applying
    // them.
    if (profile->turn_on_delay > UINT16_MAX
        || profile->rise_time > UINT16_MAX || profile->rise_time_equiv > UINT16_MAX
        || profile->turn_off_delay > UINT16_MAX
        || profile->rise_time > UINT16_MAX || profile->rise_time_equiv > UINT16_MAX) {
        return false;
    }

    // Equivalent rise time must not exceed actual rise time
    if (profile->rise_time_equiv > profile->rise_time) {
        return false;
    }

    // Equivalent fall time must not exceed actual rise time
    if (profile->fall_time_equiv > profile->fall_time) {
        return false;
    }

    // Color temperature must be within reasonable bounds
    if (profile->color_temperature > 30000) {
        return false;
    }

    return true;
}

void enlarger_profile_set_defaults(enlarger_profile_t *profile)
{
    memset(profile, 0, sizeof(enlarger_profile_t));
    strcpy(profile->name, "Default");
    profile->turn_on_delay = 40;
    profile->turn_off_delay = 10;
    profile->color_temperature = 3000;
}

uint32_t enlarger_profile_min_exposure(const enlarger_profile_t *profile)
{
    if (!profile) { return 0; }

    return profile->rise_time_equiv + profile->fall_time_equiv + profile->turn_off_delay;
}
