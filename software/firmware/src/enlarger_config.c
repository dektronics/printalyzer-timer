#include "enlarger_config.h"
#include <string.h>

bool enlarger_config_is_valid(const enlarger_config_t *config)
{
    if (!config) { return false; }

    const enlarger_timing_t *timing = &config->timing;

    /*
     * All values are limited to a sensible maximum of just over a
     * minute, so we do not need to worry about overflow when applying
     * them.
     */
    if (timing->turn_on_delay > UINT16_MAX
        || timing->rise_time > UINT16_MAX || timing->rise_time_equiv > UINT16_MAX
        || timing->turn_off_delay > UINT16_MAX
        || timing->rise_time > UINT16_MAX || timing->rise_time_equiv > UINT16_MAX) {
        return false;
    }

    /* Equivalent rise time must not exceed actual rise time */
    if (timing->rise_time_equiv > timing->rise_time) {
        return false;
    }

    /* Equivalent fall time must not exceed actual rise time */
    if (timing->fall_time_equiv > timing->fall_time) {
        return false;
    }

    /* Color temperature must be within reasonable bounds */
    if (timing->color_temperature > 30000) {
        return false;
    }

    return true;
}

bool enlarger_config_compare(const enlarger_config_t *config1, const enlarger_config_t *config2)
{
    /* Both are the same pointer */
    if (config1 == config2) {
        return true;
    }

    /* Both are null */
    if (!config1 && !config2) {
        return true;
    }

    /* One is null and the other is not */
    if ((config1 && !config2) || (!config1 && config2)) {
        return false;
    }

    /* Compare the name strings */
    if (strncmp(config1->name, config2->name, sizeof(config1->name)) != 0) {
        return false;
    }

    /* Compare all the timing fields */
    const enlarger_timing_t *timing1 = &config1->timing;
    const enlarger_timing_t *timing2 = &config2->timing;
    return timing1->turn_on_delay == timing2->turn_on_delay
        && timing1->rise_time == timing2->rise_time
        && timing1->rise_time_equiv == timing2->rise_time_equiv
        && timing1->turn_off_delay == timing2->turn_off_delay
        && timing1->fall_time == timing2->fall_time
        && timing1->fall_time_equiv == timing2->fall_time_equiv
        && timing1->color_temperature == timing2->color_temperature;

    /* Compare all the control fields */
    //TODO
}

void enlarger_config_set_defaults(enlarger_config_t *config)
{
    if (!config) { return; }

    memset(config, 0, sizeof(enlarger_config_t));
    strcpy(config->name, "Default");

    config->control.dmx_control = false;
    config->control.channel_set = ENLARGER_CHANNEL_SET_RGBW;
    config->control.dmx_wide_mode = false;
    config->control.dmx_channel_red = 0;
    config->control.dmx_channel_green = 1;
    config->control.dmx_channel_blue = 2;
    config->control.dmx_channel_white = 3;
    config->control.contrast_mode = ENLARGER_CONTRAST_MODE_WHITE;
    config->control.focus_value = UINT8_MAX;
    config->control.safe_value = UINT8_MAX;
    for (size_t i = 0; i < CONTRAST_GRADE_MAX; i++) {
        config->control.grade_values[i].channel_red = UINT8_MAX;
        config->control.grade_values[i].channel_green = UINT8_MAX;
        config->control.grade_values[i].channel_blue = UINT8_MAX;
        config->control.grade_values[i].channel_white = UINT8_MAX;
    }

    config->timing.turn_on_delay = 40;
    config->timing.turn_off_delay = 10;
    config->timing.color_temperature = 3000;
}

void enlarger_config_recalculate(enlarger_config_t *config)
{
    if (!config) { return; }
    //TODO
}

uint32_t enlarger_config_min_exposure(const enlarger_config_t *config)
{
    if (!config) { return 0; }

    return config->timing.rise_time_equiv + config->timing.fall_time_equiv + config->timing.turn_off_delay;
}
