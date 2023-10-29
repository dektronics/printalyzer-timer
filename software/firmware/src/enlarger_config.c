#include "enlarger_config.h"

#include <string.h>
#include <math.h>

static void enlarger_control_calculate_midpoint_grade(
    enlarger_grade_values_t *mid_grade,
    const enlarger_grade_values_t *grade_a,
    const enlarger_grade_values_t *grade_b,
    bool wide_mode);

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
        || timing->fall_time > UINT16_MAX || timing->fall_time_equiv > UINT16_MAX) {
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

    /* Compare the top-level fields */
    if (config1->contrast_filter != config2->contrast_filter) {
        return false;
    }

    /* Compare all the control fields */
    const enlarger_control_t *control1 = &config1->control;
    const enlarger_control_t *control2 = &config2->control;
    if (control1->dmx_control != control2->dmx_control
        || control1->channel_set != control2->channel_set
        || control1->dmx_wide_mode != control2->dmx_wide_mode
        || control1->dmx_channel_red != control2->dmx_channel_red
        || control1->dmx_channel_green != control2->dmx_channel_green
        || control1->dmx_channel_blue != control2->dmx_channel_blue
        || control1->dmx_channel_white != control2->dmx_channel_white
        || control1->contrast_mode != control2->contrast_mode
        || control1->focus_value != control2->focus_value
        || control1->safe_value != control2->safe_value) {
        return false;
    }
    for (size_t i = 0; i < CONTRAST_WHOLE_GRADE_COUNT; i++) {
        if (control1->grade_values[i].channel_red != control2->grade_values[CONTRAST_WHOLE_GRADES[i]].channel_red
            || control1->grade_values[i].channel_green != control2->grade_values[CONTRAST_WHOLE_GRADES[i]].channel_green
            || control1->grade_values[i].channel_blue != control2->grade_values[CONTRAST_WHOLE_GRADES[i]].channel_blue
            || control1->grade_values[i].channel_white != control2->grade_values[CONTRAST_WHOLE_GRADES[i]].channel_white) {
            return false;
        }
    }

    /* Compare all the timing fields */
    const enlarger_timing_t *timing1 = &config1->timing;
    const enlarger_timing_t *timing2 = &config2->timing;
    if (timing1->turn_on_delay != timing2->turn_on_delay
        || timing1->rise_time != timing2->rise_time
        || timing1->rise_time_equiv != timing2->rise_time_equiv
        || timing1->turn_off_delay != timing2->turn_off_delay
        || timing1->fall_time != timing2->fall_time
        || timing1->fall_time_equiv != timing2->fall_time_equiv
        || timing1->color_temperature != timing2->color_temperature) {
        return false;
    }

    return true;
}

void enlarger_config_set_defaults(enlarger_config_t *config)
{
    if (!config) { return; }

    memset(config, 0, sizeof(enlarger_config_t));
    strcpy(config->name, "Default");
    config->contrast_filter = CONTRAST_FILTER_REGULAR;

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
    enlarger_config_set_contrast_defaults(&config->control);

    config->timing.turn_on_delay = 40;
    config->timing.turn_off_delay = 10;
    config->timing.color_temperature = 3000;

    enlarger_config_recalculate(config);
}

void enlarger_config_set_contrast_defaults(enlarger_control_t *control)
{
    if (!control) { return; }

    if (control->contrast_mode == ENLARGER_CONTRAST_MODE_GREEN_BLUE) {
        const uint16_t max_val = control->dmx_wide_mode ? UINT16_MAX : UINT8_MAX;

        for (size_t i = 0; i < CONTRAST_WHOLE_GRADE_COUNT; i++) {
            const uint8_t grade = (uint8_t)CONTRAST_WHOLE_GRADES[i];
            control->grade_values[grade].channel_red = 0;
            control->grade_values[grade].channel_white = 0;

            if (i == 0) {
                control->grade_values[grade].channel_green = max_val;
                control->grade_values[grade].channel_blue = 0;
            } else if (i == CONTRAST_WHOLE_GRADE_COUNT - 1) {
                control->grade_values[grade].channel_green = 0;
                control->grade_values[grade].channel_blue = max_val;
            } else {
                float green_val = ((float)grade * (-1.0F * max_val / ((float)CONTRAST_GRADE_MAX - 1.0F))) + max_val;
                float blue_val = (float)grade * (max_val / ((float)CONTRAST_GRADE_MAX - 1.0F));
                control->grade_values[grade].channel_green = lroundf(green_val);
                control->grade_values[grade].channel_blue = lroundf(blue_val);
            }
        }
    } else {
        for (size_t i = 0; i < CONTRAST_GRADE_MAX; i++) {
            control->grade_values[i].channel_red = UINT8_MAX;
            control->grade_values[i].channel_green = UINT8_MAX;
            control->grade_values[i].channel_blue = UINT8_MAX;
            control->grade_values[i].channel_white = UINT8_MAX;
        }
    }
}

void enlarger_config_recalculate(enlarger_config_t *config)
{
    if (!config) { return; }

    /*
     * Fill in the enlarger settings for contrast half-grades
     * using a simple midpoint formula for the time being.
     */

    enlarger_control_t *control = &config->control;
    enlarger_control_calculate_midpoint_grade(
        &control->grade_values[CONTRAST_GRADE_0_HALF],
        &control->grade_values[CONTRAST_GRADE_0],
        &control->grade_values[CONTRAST_GRADE_1],
        control->dmx_wide_mode);

    enlarger_control_calculate_midpoint_grade(
        &control->grade_values[CONTRAST_GRADE_1_HALF],
        &control->grade_values[CONTRAST_GRADE_1],
        &control->grade_values[CONTRAST_GRADE_2],
        control->dmx_wide_mode);

    enlarger_control_calculate_midpoint_grade(
        &control->grade_values[CONTRAST_GRADE_2_HALF],
        &control->grade_values[CONTRAST_GRADE_2],
        &control->grade_values[CONTRAST_GRADE_3],
        control->dmx_wide_mode);

    enlarger_control_calculate_midpoint_grade(
        &control->grade_values[CONTRAST_GRADE_3_HALF],
        &control->grade_values[CONTRAST_GRADE_3],
        &control->grade_values[CONTRAST_GRADE_4],
        control->dmx_wide_mode);

    enlarger_control_calculate_midpoint_grade(
        &control->grade_values[CONTRAST_GRADE_4_HALF],
        &control->grade_values[CONTRAST_GRADE_4],
        &control->grade_values[CONTRAST_GRADE_5],
        control->dmx_wide_mode);
}

void enlarger_control_calculate_midpoint_grade(
    enlarger_grade_values_t *mid_grade,
    const enlarger_grade_values_t *grade_a,
    const enlarger_grade_values_t *grade_b,
    bool wide_mode)
{
    uint32_t mid_red = lroundf(((float)grade_a->channel_red + (float)grade_b->channel_red) / 2.0F);
    uint32_t mid_green = lroundf(((float)grade_a->channel_green + (float)grade_b->channel_green) / 2.0F);
    uint32_t mid_blue = lroundf(((float)grade_a->channel_blue + (float)grade_b->channel_blue) / 2.0F);
    uint32_t mid_white = lroundf(((float)grade_a->channel_white + (float)grade_b->channel_white) / 2.0F);

    if (wide_mode) {
        mid_grade->channel_red = (uint16_t)mid_red;
        mid_grade->channel_green = (uint16_t)mid_green;
        mid_grade->channel_blue = (uint16_t)mid_blue;
        mid_grade->channel_white = (uint16_t)mid_white;
    } else {
        mid_grade->channel_red = (uint8_t)mid_red;
        mid_grade->channel_green = (uint8_t)mid_green;
        mid_grade->channel_blue = (uint8_t)mid_blue;
        mid_grade->channel_white = (uint8_t)mid_white;
    }
}

uint32_t enlarger_config_min_exposure(const enlarger_config_t *config)
{
    if (!config) { return 0; }

    return config->timing.rise_time_equiv + config->timing.fall_time_equiv + config->timing.turn_off_delay;
}

bool enlarger_config_has_rgb(const enlarger_config_t *config)
{
    if (!config) {
        return false;
    }

    return config->control.dmx_control
        && (config->control.channel_set == ENLARGER_CHANNEL_SET_RGB
            || config->control.channel_set == ENLARGER_CHANNEL_SET_RGBW);
}
