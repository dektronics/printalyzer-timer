#include "state_timer.h"

#include <FreeRTOS.h>
#include <cmsis_os.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <esp_log.h>

#include "display.h"
#include "keypad.h"
#include "util.h"
#include "illum_controller.h"
#include "exposure_timer.h"
#include "settings.h"

static const char *TAG = "state_timer";

static bool state_timer_process(state_t *state_base, state_controller_t *controller);
static state_t state_timer_data = {
    .state_process = state_timer_process
};

static bool state_timer_main_exposure(exposure_state_t *exposure_state, const enlarger_profile_t *enlarger_profile);
static bool state_timer_main_exposure_callback(exposure_timer_state_t state, uint32_t time_ms, void *user_data);
static bool state_timer_burn_dodge_exposure(exposure_state_t *exposure_state, const enlarger_profile_t *enlarger_profile, int burn_dodge_index);
static bool state_timer_burn_dodge_exposure_callback(exposure_timer_state_t state, uint32_t time_ms, void *user_data);

state_t *state_timer()
{
    return (state_t *)&state_timer_data;
}

bool state_timer_process(state_t *state_base, state_controller_t *controller)
{
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);
    const enlarger_profile_t *enlarger_profile = state_controller_get_enlarger_profile(controller);

    if (state_timer_main_exposure(exposure_state, enlarger_profile)) {
        for (int i = 0; i < exposure_burn_dodge_count(exposure_state); i++) {
            if (!state_timer_burn_dodge_exposure(exposure_state, enlarger_profile, i)) {
                break;
            }
        }
    }

    state_controller_set_next_state(controller, STATE_HOME, 0);
    return true;
}

bool state_timer_main_exposure(exposure_state_t *exposure_state, const enlarger_profile_t *enlarger_profile)
{
    bool result;

    float adjusted_exposure_time = exposure_get_exposure_time(exposure_state);

    // If a dodge adjustment is configured, then reduce the base exposure time
    const exposure_burn_dodge_t *burn_dodge_entry = exposure_burn_dodge_get(exposure_state, 0);
    if (burn_dodge_entry && burn_dodge_entry->numerator < 0) {
        float adj_stops = (float)burn_dodge_entry->numerator / (float)burn_dodge_entry->denominator;
        float adj_time = exposure_get_exposure_time(exposure_state) * powf(2.0f, adj_stops);
        float dodge_time = fabs(exposure_get_exposure_time(exposure_state) - adj_time);
        adjusted_exposure_time -= dodge_time;
        if (adjusted_exposure_time < 0) { adjusted_exposure_time = 0; }
        ESP_LOGI(TAG, "Exposure time reduced by %.2fs due to dodge", dodge_time);
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

    exposure_timer_set_config_time(&timer_config, exposure_time_ms, enlarger_profile);

    exposure_timer_set_config(&timer_config);

    ESP_LOGI(TAG, "Starting exposure timer for %ldms", exposure_time_ms);

    display_draw_exposure_timer(&elements, 0);

    HAL_StatusTypeDef ret = exposure_timer_run();
    if (ret == HAL_TIMEOUT) {
        ESP_LOGE(TAG, "Exposure timer canceled");
        result = false;
    } else if (ret != HAL_OK) {
        ESP_LOGE(TAG, "Exposure timer error");
        result = false;
    } else {
        result = true;
    }

    ESP_LOGI(TAG, "Exposure timer complete");

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

    // Handle the next keypad event without blocking
    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, 0) == HAL_OK) {
        if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
            ESP_LOGI(TAG, "Canceling exposure timer at %ldms", time_ms);
            return false;
        }
    }

    return true;
}

bool state_timer_burn_dodge_exposure(exposure_state_t *exposure_state, const enlarger_profile_t *enlarger_profile, int burn_dodge_index)
{
    bool result;
    const exposure_burn_dodge_t *entry = exposure_burn_dodge_get(exposure_state, burn_dodge_index);

    display_adjustment_exposure_elements_t elements = {0};

    elements.burn_dodge_index = burn_dodge_index + 1;

    // Set the title text, which includes the stops adjustment
    size_t offset = 0;
    char buf_title[32];
    if (entry->numerator < 0) {
        offset = sprintf(buf_title, "Dodge ");
    } else {
        offset = sprintf(buf_title, "Burn ");
    }
    append_signed_fraction(buf_title + offset, entry->numerator, entry->denominator);
    elements.title = buf_title;

    // Set the contrast grade, if applicable
    if (entry->numerator < 0) {
        elements.contrast_grade = DISPLAY_GRADE_NONE;
    } else {
        if (entry->contrast_grade == CONTRAST_GRADE_MAX) {
            elements.contrast_grade = convert_exposure_to_display_contrast(exposure_get_contrast_grade(exposure_state));
        } else {
            elements.contrast_grade = convert_exposure_to_display_contrast(entry->contrast_grade);
        }
    }

    // Calculate the raw time for the adjustment exposure
    float adj_stops = (float)entry->numerator / (float)entry->denominator;
    float adj_time = exposure_get_exposure_time(exposure_state) * powf(2.0f, adj_stops);
    float burn_dodge_time = 0;
    if (entry->numerator < 0) {
        // Dodge adjustment
        burn_dodge_time = fabs(exposure_get_exposure_time(exposure_state) - adj_time);
    } else {
        // Burn adjustment
        burn_dodge_time = adj_time - exposure_get_exposure_time(exposure_state);
    }

    // Set the exposure time for the adjustment
    uint32_t exposure_time_ms = rounded_exposure_time_ms(burn_dodge_time);
    convert_exposure_to_display_timer(&(elements.time_elements), exposure_time_ms);

    display_draw_adjustment_exposure_elements(&elements);

    // Wait for start or cancel
    keypad_event_t keypad_event;
    do {
        if (keypad_wait_for_event(&keypad_event, -1) == HAL_OK) {
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_START)
                || keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_FOOTSWITCH)) {
                ESP_LOGI(TAG, "Starting dodge/burn exposure");
                break;
            } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                ESP_LOGI(TAG, "Canceling dodge/burn exposure");
                return false;
            }
        }
    } while (1);

    // Prepare the exposure timer
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

    exposure_timer_set_config_time(&timer_config, exposure_time_ms, enlarger_profile);

    exposure_timer_set_config(&timer_config);

    ESP_LOGI(TAG, "Starting burn/dodge exposure timer for %ldms", exposure_time_ms);

    // Redraw the display elements in exposure timer mode
    elements.contrast_grade = DISPLAY_GRADE_MAX;
    display_draw_adjustment_exposure_elements(&elements);

    HAL_StatusTypeDef ret = exposure_timer_run();
    if (ret == HAL_TIMEOUT) {
        ESP_LOGE(TAG, "Exposure timer canceled");
        result = false;
    } else if (ret != HAL_OK) {
        ESP_LOGE(TAG, "Exposure timer error");
        result = false;
    } else {
        result = true;
    }

    ESP_LOGI(TAG, "Exposure timer complete");

    return result;
}

bool state_timer_burn_dodge_exposure_callback(exposure_timer_state_t state, uint32_t time_ms, void *user_data)
{
    display_exposure_timer_t *time_elements = user_data;

    if (time_ms != UINT32_MAX) {
        update_display_timer(time_elements, time_ms);
        display_redraw_adjustment_exposure_timer(time_elements);
    }

    // Handle the next keypad event without blocking
    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, 0) == HAL_OK) {
        if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
            ESP_LOGI(TAG, "Canceling burn/dodge exposure timer at %ldms", time_ms);
            return false;
        }
    }

    return true;
}
