#include "enlarger_profile.h"

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

    return true;
}

uint32_t enlarger_profile_min_exposure(const enlarger_profile_t *profile)
{
    if (!profile) { return 0; }

    return profile->rise_time_equiv + profile->fall_time_equiv + profile->turn_off_delay;
}
