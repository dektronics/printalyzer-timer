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

bool enlarger_profile_compare(const enlarger_profile_t *profile1, const enlarger_profile_t *profile2)
{
    /* Both are the same pointer */
    if (profile1 == profile2) {
        return true;
    }

    /* Both are null */
    if (!profile1 && !profile2) {
        return true;
    }

    /* One is null and the other is not */
    if ((profile1 && !profile2) || (!profile1 && profile2)) {
        return false;
    }

    /* Compare the name strings */
    if (strncmp(profile1->name, profile2->name, sizeof(profile1->name)) != 0) {
        return false;
    }

    /* Compare all the integer fields */
    return profile1->turn_on_delay == profile2->turn_on_delay
        && profile1->rise_time == profile2->rise_time
        && profile1->rise_time_equiv == profile2->rise_time_equiv
        && profile1->turn_off_delay == profile2->turn_off_delay
        && profile1->fall_time == profile2->fall_time
        && profile1->fall_time_equiv == profile2->fall_time_equiv
        && profile1->color_temperature == profile2->color_temperature;
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
