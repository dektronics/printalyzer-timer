#include "enlarger_control.h"

#define LOG_TAG "enlarger_control"
#include <elog.h>

#include "enlarger_config.h"
#include "relay.h"
#include "dmx.h"
#include "util.h"

static osStatus_t enlarger_control_set_state_relay(enlarger_control_state_t state);
static osStatus_t enlarger_control_set_state_dmx(const enlarger_control_t *enlarger_control,
    enlarger_control_state_t state, contrast_grade_t grade,
    uint16_t channel_red, uint16_t channel_green, uint16_t channel_blue,
    bool blocking);
static osStatus_t enlarger_control_set_frame(const enlarger_control_t *enlarger_control,
    uint16_t red, uint16_t green, uint16_t blue, uint16_t white,
    bool blocking);

osStatus_t enlarger_control_set_state(const enlarger_control_t *enlarger_control,
    enlarger_control_state_t state, contrast_grade_t grade,
    uint16_t channel_red, uint16_t channel_green, uint16_t channel_blue,
    bool blocking)
{
    if (!enlarger_control) { return osErrorParameter; }

    if (enlarger_control->dmx_control) {
        return enlarger_control_set_state_dmx(enlarger_control, state, grade,
            channel_red, channel_green, channel_blue, blocking);
    } else {
        return enlarger_control_set_state_relay(state);
    }
}

osStatus_t enlarger_control_set_state_relay(enlarger_control_state_t state)
{
    bool enabled;

    switch (state) {
    case ENLARGER_CONTROL_STATE_OFF:
        enabled = false;
        break;
    case ENLARGER_CONTROL_STATE_FOCUS:
        enabled = true;
        break;
    case ENLARGER_CONTROL_STATE_SAFE:
        enabled = false;
        break;
    case ENLARGER_CONTROL_STATE_EXPOSURE:
        enabled = true;
        break;
    default:
        enabled = false;
        break;
    }

    relay_enlarger_enable(enabled);

    return osOK;
}

osStatus_t enlarger_control_set_state_dmx(const enlarger_control_t *enlarger_control,
    enlarger_control_state_t state, contrast_grade_t grade,
    uint16_t channel_red, uint16_t channel_green, uint16_t channel_blue,
    bool blocking)
{
    const bool has_rgb =
        enlarger_control->channel_set == ENLARGER_CHANNEL_SET_RGB
        || enlarger_control->channel_set == ENLARGER_CHANNEL_SET_RGBW;
    const bool has_white =
        enlarger_control->channel_set == ENLARGER_CHANNEL_SET_WHITE
        || enlarger_control->channel_set == ENLARGER_CHANNEL_SET_RGBW;

    uint16_t red = 0;
    uint16_t green = 0;
    uint16_t blue = 0;
    uint16_t white = 0;

    if (state == ENLARGER_CONTROL_STATE_OFF) {
        red = 0;
        green = 0;
        blue = 0;
        white = 0;
    } else if (state == ENLARGER_CONTROL_STATE_FOCUS) {
        if (enlarger_control->channel_set == ENLARGER_CHANNEL_SET_RGB) {
            red = enlarger_control->focus_value;
            green = enlarger_control->focus_value;
            blue = enlarger_control->focus_value;
        } else {
            white = enlarger_control->focus_value;
        }
    } else if (state == ENLARGER_CONTROL_STATE_SAFE) {
        if (has_rgb) {
            red = enlarger_control->safe_value;
        }
    } else if (state == ENLARGER_CONTROL_STATE_EXPOSURE) {
        if (has_rgb && grade == CONTRAST_GRADE_MAX) {
            red = channel_red;
            green = channel_green;
            blue = channel_blue;
        } else if (has_rgb && enlarger_control->contrast_mode == ENLARGER_CONTRAST_MODE_GREEN_BLUE) {
            if (grade >= CONTRAST_GRADE_MAX) { return osErrorParameter; }
            green = enlarger_control->grade_values[grade].channel_green;
            blue = enlarger_control->grade_values[grade].channel_blue;
        } else {
            if (has_rgb) {
                red = enlarger_control->grade_values[CONTRAST_GRADE_2].channel_red;
                green = enlarger_control->grade_values[CONTRAST_GRADE_2].channel_green;
                blue = enlarger_control->grade_values[CONTRAST_GRADE_2].channel_blue;
            }
            if (has_white) {
                white = enlarger_control->grade_values[CONTRAST_GRADE_2].channel_white;
            }
        }
    }

    return enlarger_control_set_frame(enlarger_control, red, green, blue, white, blocking);
}

osStatus_t enlarger_control_set_frame(const enlarger_control_t *enlarger_control,
    uint16_t red, uint16_t green, uint16_t blue, uint16_t white,
    bool blocking)
{
    const bool has_rgb =
        enlarger_control->channel_set == ENLARGER_CHANNEL_SET_RGB
        || enlarger_control->channel_set == ENLARGER_CHANNEL_SET_RGBW;

    uint16_t channels[8] = {0};
    uint8_t values[8] = {0};
    size_t len = 0;

    if (has_rgb) {
        if (enlarger_control->dmx_wide_mode) {
            channels[0] = enlarger_control->dmx_channel_red;
            channels[1] = enlarger_control->dmx_channel_red + 1;
            channels[2] = enlarger_control->dmx_channel_green;
            channels[3] = enlarger_control->dmx_channel_green + 1;
            channels[4] = enlarger_control->dmx_channel_blue;
            channels[5] = enlarger_control->dmx_channel_blue + 1;
            conv_u16_array(values, red);
            conv_u16_array(values + 2, green);
            conv_u16_array(values + 4, blue);
            len = 6;
        } else {
            channels[0] = enlarger_control->dmx_channel_red;
            channels[1] = enlarger_control->dmx_channel_green;
            channels[2] = enlarger_control->dmx_channel_blue;
            values[0] = red;
            values[1] = green;
            values[2] = blue;
            len = 3;
        }

        if (enlarger_control->channel_set == ENLARGER_CHANNEL_SET_RGBW) {
            if (enlarger_control->dmx_wide_mode) {
                channels[6] = enlarger_control->dmx_channel_white;
                channels[7] = enlarger_control->dmx_channel_white + 1;
                conv_u16_array(values + 6, white);
                len = 8;
            } else {
                channels[3] = enlarger_control->dmx_channel_white;
                values[3] = white;
                len = 4;
            }
        }
    } else {
        if (enlarger_control->dmx_wide_mode) {
            channels[0] = enlarger_control->dmx_channel_white;
            channels[1] = enlarger_control->dmx_channel_white + 1;
            conv_u16_array(values, white);
            len = 2;
        } else {
            channels[0] = enlarger_control->dmx_channel_white;
            values[0] = white;
            len = 1;
        }
    }

    return dmx_set_sparse_frame(channels, values, len, blocking);
}
