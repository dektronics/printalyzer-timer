#include "state_timer.h"

#include <FreeRTOS.h>
#include <cmsis_os.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#define LOG_TAG "state_timer"
#include <elog.h>

#include "display.h"
#include "keypad.h"
#include "util.h"
#include "illum_controller.h"
#include "enlarger_config.h"
#include "enlarger_control.h"
#include "exposure_timer.h"
#include "settings.h"

static bool state_timer_process(state_t *state_base, state_controller_t *controller);
static state_t state_timer_data = {
    .state_process = state_timer_process
};

static bool state_timer_main_exposure(exposure_state_t *exposure_state, const enlarger_config_t *enlarger_config);
static bool state_timer_main_exposure_callback(exposure_timer_state_t state, uint32_t time_ms, void *user_data);
static bool state_timer_burn_dodge_exposure(exposure_state_t *exposure_state, const enlarger_config_t *enlarger_config, int burn_dodge_index);
static bool state_timer_burn_dodge_exposure_callback(exposure_timer_state_t state, uint32_t time_ms, void *user_data);

state_t *state_timer()
{
    return (state_t *)&state_timer_data;
}

bool state_timer_process(state_t *state_base, state_controller_t *controller)
{
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);
    const enlarger_config_t *enlarger_config = state_controller_get_enlarger_config(controller);

    if (state_timer_main_exposure(exposure_state, enlarger_config)) {
        for (int i = 0; i < exposure_burn_dodge_count(exposure_state); i++) {
            if (!state_timer_burn_dodge_exposure(exposure_state, enlarger_config, i)) {
                break;
            }
        }
    }

    state_controller_set_next_state(controller, STATE_HOME, 0);
    return true;
}

bool state_timer_main_exposure(exposure_state_t *exposure_state, const enlarger_config_t *enlarger_config)
{
    bool result;

    float adjusted_exposure_time = exposure_get_exposure_time(exposure_state);

    /* If a dodge adjustment is configured, then reduce the base exposure time */
    const exposure_burn_dodge_t *burn_dodge_entry = exposure_burn_dodge_get(exposure_state, 0);
    if (burn_dodge_entry && burn_dodge_entry->numerator < 0) {
        float adj_stops = (float)burn_dodge_entry->numerator / (float)burn_dodge_entry->denominator;
        float adj_time = exposure_get_exposure_time(exposure_state) * powf(2.0f, adj_stops);
        float dodge_time = fabsf(exposure_get_exposure_time(exposure_state) - adj_time);
        adjusted_exposure_time -= dodge_time;
        if (adjusted_exposure_time < 0) { adjusted_exposure_time = 0; }
        log_i("Exposure time reduced by %.2fs due to dodge", dodge_time);
    }

    uint32_t exposure_time_ms = rounded_exposure_time_ms(adjusted_exposure_time);

    display_exposure_timer_t elements;
    convert_exposure_to_display_timer(&elements, exposure_time_ms);

    exposure_timer_config_t timer_config = {0};
    timer_config.end_tone = EXPOSURE_TIMER_END_TONE_REGULAR;
    timer_config.timer_callback = state_timer_main_exposure_callback;
    timer_config.user_data = &elements;

    if (elements.fraction_digits == 0) {
        timer_config.callback_rate = EXPOSURE_TIMER_RATE_1_SEC;
    } else if (elements.fraction_digits == 1) {
        timer_config.callback_rate = EXPOSURE_TIMER_RATE_100_MS;
    } else if (elements.fraction_digits == 2) {
        timer_config.callback_rate = EXPOSURE_TIMER_RATE_10_MS;
    } else {
        timer_config.callback_rate = EXPOSURE_TIMER_RATE_1_SEC;
    }

    const exposure_mode_t mode = exposure_get_mode(exposure_state);
    if (mode == EXPOSURE_MODE_PRINTING_COLOR) {
        timer_config.contrast_grade = CONTRAST_GRADE_MAX;
        timer_config.channel_red = exposure_get_channel_value(exposure_state, 0);
        timer_config.channel_green = exposure_get_channel_value(exposure_state, 1);
        timer_config.channel_blue = exposure_get_channel_value(exposure_state, 2);
    } else {
        timer_config.contrast_grade = exposure_get_contrast_grade(exposure_state);
    }

    exposure_timer_set_config_time(&timer_config, exposure_time_ms, enlarger_config);

    exposure_timer_set_config(&timer_config, &enlarger_config->control);

    log_i("Starting exposure timer for %ldms", exposure_time_ms);

    display_draw_exposure_timer(&elements, 0);

    HAL_StatusTypeDef ret = exposure_timer_run();
    if (ret == HAL_TIMEOUT) {
        log_e("Exposure timer canceled");
        result = false;
    } else if (ret != HAL_OK) {
        log_e("Exposure timer error");
        result = false;
    } else {
        result = true;
    }

    log_i("Exposure timer complete");

    return result;
}

bool state_timer_main_exposure_callback(exposure_timer_state_t state, uint32_t time_ms, void *user_data)
{
    display_exposure_timer_t *elements = user_data;
    display_exposure_timer_t prev_elements;

    if (time_ms != UINT32_MAX) {
        memcpy(&prev_elements, elements, sizeof(display_exposure_timer_t));
        update_display_timer(elements, time_ms);
        display_draw_exposure_timer(elements, &prev_elements);
    }

    /* Handle the next keypad event without blocking */
    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, 0) == HAL_OK) {
        if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
            log_i("Canceling exposure timer at %ldms", time_ms);
            return false;
        }
    }

    return true;
}

bool state_timer_burn_dodge_exposure(exposure_state_t *exposure_state, const enlarger_config_t *enlarger_config, int burn_dodge_index)
{
    bool result;
    const exposure_burn_dodge_t *entry = exposure_burn_dodge_get(exposure_state, burn_dodge_index);

    display_adjustment_exposure_elements_t elements = {0};

    elements.burn_dodge_index = burn_dodge_index + 1;

    /* Set the title text, which includes the stops adjustment */
    size_t offset = 0;
    char buf_title[32];
    if (entry->numerator < 0) {
        offset = sprintf(buf_title, "Dodge ");
    } else {
        offset = sprintf(buf_title, "Burn ");
    }
    append_signed_fraction(buf_title + offset, entry->numerator, entry->denominator);
    elements.title = buf_title;

    /* Set the contrast grade, if applicable */
    if (entry->numerator < 0) {
        elements.contrast_grade = CONTRAST_GRADE_MAX;
    } else {
        if (entry->contrast_grade == CONTRAST_GRADE_MAX) {
            elements.contrast_grade = exposure_get_contrast_grade(exposure_state);
        } else {
            elements.contrast_grade = entry->contrast_grade;
        }
    }
    elements.contrast_note = contrast_filter_grade_str(
        (enlarger_config->control.dmx_control ? CONTRAST_FILTER_REGULAR : enlarger_config->contrast_filter),
        elements.contrast_grade);

    /* Calculate the raw time for the adjustment exposure */
    float adj_stops = (float)entry->numerator / (float)entry->denominator;
    float adj_time = exposure_get_exposure_time(exposure_state) * powf(2.0f, adj_stops);
    float burn_dodge_time = 0;
    if (entry->numerator < 0) {
        /* Dodge adjustment */
        burn_dodge_time = fabsf(exposure_get_exposure_time(exposure_state) - adj_time);
    } else {
        /* Burn adjustment */
        burn_dodge_time = adj_time - exposure_get_exposure_time(exposure_state);
    }

    /* Set the exposure time for the adjustment */
    uint32_t exposure_time_ms = rounded_exposure_time_ms(burn_dodge_time);
    convert_exposure_to_display_timer(&(elements.time_elements), exposure_time_ms);

    /* Check for the short-time case */
    uint32_t min_exposure_time_ms = enlarger_config_min_exposure(enlarger_config);
    elements.time_too_short = (min_exposure_time_ms > 0) && (exposure_time_ms < min_exposure_time_ms);

    display_draw_adjustment_exposure_elements(&elements);

    /* Enable the enlarger in safe mode */
    enlarger_control_set_state_safe(&enlarger_config->control, false);

    /* Wait for start or cancel */
    keypad_event_t keypad_event;
    do {
        if (keypad_wait_for_event(&keypad_event, -1) == HAL_OK) {
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_START)
                || keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_FOOTSWITCH)) {
                log_i("Starting dodge/burn exposure");
                break;
            } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                log_i("Canceling dodge/burn exposure");
                return false;
            }
        }
    } while (1);

    /* Prepare the exposure timer */
    exposure_timer_config_t timer_config = {0};
    timer_config.start_tone = EXPOSURE_TIMER_START_TONE_COUNTDOWN;
    timer_config.end_tone = EXPOSURE_TIMER_END_TONE_REGULAR;
    timer_config.timer_callback = state_timer_burn_dodge_exposure_callback;
    timer_config.user_data = &(elements.time_elements);

    if (elements.time_elements.fraction_digits == 0) {
        timer_config.callback_rate = EXPOSURE_TIMER_RATE_1_SEC;
    } else if (elements.time_elements.fraction_digits == 1) {
        timer_config.callback_rate = EXPOSURE_TIMER_RATE_100_MS;
    } else if (elements.time_elements.fraction_digits == 2) {
        timer_config.callback_rate = EXPOSURE_TIMER_RATE_10_MS;
    } else {
        timer_config.callback_rate = EXPOSURE_TIMER_RATE_1_SEC;
    }

    timer_config.contrast_grade = exposure_get_contrast_grade(exposure_state);
    const exposure_mode_t mode = exposure_get_mode(exposure_state);
    if (mode == EXPOSURE_MODE_PRINTING_COLOR) {
        timer_config.contrast_grade = CONTRAST_GRADE_MAX;
        timer_config.channel_red = exposure_get_channel_value(exposure_state, 0);
        timer_config.channel_green = exposure_get_channel_value(exposure_state, 1);
        timer_config.channel_blue = exposure_get_channel_value(exposure_state, 2);
    } else {
        if (elements.contrast_grade == CONTRAST_GRADE_MAX) {
            timer_config.contrast_grade = exposure_get_contrast_grade(exposure_state);
        } else {
            timer_config.contrast_grade = elements.contrast_grade;
        }
    }

    exposure_timer_set_config_time(&timer_config, exposure_time_ms, enlarger_config);

    exposure_timer_set_config(&timer_config, &enlarger_config->control);

    log_i("Starting burn/dodge exposure timer for %ldms", exposure_time_ms);

    /* Redraw the display elements in exposure timer mode */
    elements.contrast_grade = CONTRAST_GRADE_MAX;
    elements.contrast_note = NULL;
    display_draw_adjustment_exposure_elements(&elements);

    HAL_StatusTypeDef ret = exposure_timer_run();
    if (ret == HAL_TIMEOUT) {
        log_e("Exposure timer canceled");
        result = false;
    } else if (ret != HAL_OK) {
        log_e("Exposure timer error");
        result = false;
    } else {
        result = true;
    }

    log_i("Exposure timer complete");

    enlarger_control_set_state_off(&enlarger_config->control, false);

    return result;
}

bool state_timer_burn_dodge_exposure_callback(exposure_timer_state_t state, uint32_t time_ms, void *user_data)
{
    display_exposure_timer_t *time_elements = user_data;

    if (time_ms != UINT32_MAX) {
        update_display_timer(time_elements, time_ms);
        display_redraw_adjustment_exposure_timer(time_elements);
    }

    /* Handle the next keypad event without blocking */
    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, 0) == HAL_OK) {
        if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
            log_i("Canceling burn/dodge exposure timer at %ldms", time_ms);
            return false;
        }
    }

    return true;
}
