#include "state_test_strip.h"

#include <FreeRTOS.h>
#include <cmsis_os.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define LOG_TAG "state_test_strip"
#include <elog.h>

#include "display.h"
#include "keypad.h"
#include "led.h"
#include "util.h"
#include "illum_controller.h"
#include "enlarger_control.h"
#include "exposure_timer.h"
#include "settings.h"
#include "buzzer.h"

typedef struct {
    state_t base;
    buzzer_volume_t current_volume;
    pam8904e_freq_t current_frequency;
    teststrip_patches_t teststrip_patches;
    teststrip_mode_t teststrip_mode;
    int exposure_patch_min;
    unsigned int exposure_patch_count;
    unsigned int exposure_patch_offset;
    unsigned int patches_covered;
    display_test_strip_elements_t elements;
} state_test_strip_t;

static bool state_test_strip_countdown(const exposure_state_t *exposure_state, const enlarger_config_t *enlarger_config, uint32_t patch_time_ms, bool last_patch);

static void state_test_strip_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param);
static void state_test_strip_prepare_elements(state_test_strip_t *state, state_controller_t *controller);
static bool state_test_strip_process(state_t *state_base, state_controller_t *controller);
static void state_test_strip_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state);
static state_test_strip_t state_test_strip_data = {
    .base = {
        .state_entry = state_test_strip_entry,
        .state_process = state_test_strip_process,
        .state_exit = state_test_strip_exit
    }
};

state_t *state_test_strip()
{
    return (state_t *)&state_test_strip_data;
}

void state_test_strip_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param)
{
    state_test_strip_t *state = (state_test_strip_t *)state_base;

    state->current_volume = buzzer_get_volume();
    state->current_frequency = buzzer_get_frequency();
    state->teststrip_patches = settings_get_teststrip_patches();
    state->teststrip_mode = settings_get_teststrip_mode();
    state->exposure_patch_min = 0;
    state->exposure_patch_count = 0;
    state->exposure_patch_offset = 0;
    state->patches_covered = 0;
    memset(&state->elements, 0, sizeof(display_test_strip_elements_t));

    state_test_strip_prepare_elements(state, controller);
}

void state_test_strip_prepare_elements(state_test_strip_t *state, state_controller_t *controller)
{
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);

    state->elements.title1 = "Test Strip";
    state->elements.time_elements.time_seconds = 25;
    state->elements.time_elements.time_milliseconds = 1;
    state->elements.time_elements.fraction_digits = 1;

    switch (exposure_adj_increment_get(exposure_state)) {
    case EXPOSURE_ADJ_TWELFTH:
        state->elements.title2 = "1/12 Stop";
        break;
    case EXPOSURE_ADJ_SIXTH:
        state->elements.title2 = "1/6 Stop";
        break;
    case EXPOSURE_ADJ_QUARTER:
        state->elements.title2 = "1/4 Stop";
        break;
    case EXPOSURE_ADJ_THIRD:
        state->elements.title2 = "1/3 Stop";
        break;
    case EXPOSURE_ADJ_HALF:
        state->elements.title2 = "1/2 Stop";
        break;
    case EXPOSURE_ADJ_WHOLE:
        state->elements.title2 = "1 Stop";
        break;
    }

    switch (state->teststrip_patches) {
    case TESTSTRIP_PATCHES_5:
        state->exposure_patch_min = -2;
        state->exposure_patch_count = 5;
        state->elements.patches = DISPLAY_PATCHES_5;
        break;
    case TESTSTRIP_PATCHES_7:
    default:
        state->exposure_patch_min = -3;
        state->exposure_patch_count = 7;
        state->elements.patches = DISPLAY_PATCHES_7;
        break;
    }

    if (exposure_get_mode(exposure_state) == EXPOSURE_MODE_CALIBRATION) {
        for (int i = 0; i < state->exposure_patch_count; i++) {
            state->elements.patch_cal_values[i] =
                exposure_get_test_strip_patch_pev(exposure_state, state->exposure_patch_min + i);
        }
    }

    /* Iterate across all patch times and mark those that are too short */
    state->elements.invalid_patches = 0;
    float min_exposure_time = exposure_get_min_exposure_time(exposure_state);
    if (!(min_exposure_time > 0)) { return; }

    /* Find the valid patch offset to deal with short exposure times */
    unsigned int patch_offset = 0;
    if (state->teststrip_mode == TESTSTRIP_MODE_SEPARATE) {
        /* In separate mode, just block off invalid patches */
        for (int i = 0; i < state->exposure_patch_count; i++) {
            float patch_time = exposure_get_test_strip_time_complete(exposure_state, state->exposure_patch_min + i);
            if (patch_time < min_exposure_time) {
                state->elements.invalid_patches |= (1 << (state->exposure_patch_count - i - 1));
                patch_offset = i + 1;
            }
        }
    } else {
        /* In incremental mode, adjust the minimum patch */
        for (unsigned int i = 0; i < state->exposure_patch_count; i++) {
            float base_time = exposure_get_test_strip_time_incremental(exposure_state, state->exposure_patch_min + i, 0);
            float patch_time = exposure_get_test_strip_time_incremental(exposure_state, state->exposure_patch_min + i, 1);
            if (base_time > min_exposure_time && ((i == state->exposure_patch_count - 1) || (patch_time > min_exposure_time))) {
                break;
            }
            patch_offset = i + 1;
        }
    }

    if (patch_offset >= state->exposure_patch_count) {
        log_w("All test strip times are too short: min_time=%f", min_exposure_time);
        state->elements.invalid_patches = 0xFF;
        state->patches_covered = state->exposure_patch_count;
    } else if (patch_offset > 0) {
        log_w("Starting strip on patch: %d, min_time=%f", state->exposure_patch_min + patch_offset, min_exposure_time);
        for (int i = 0; i < patch_offset; i++) {
            state->elements.invalid_patches |= (1 << (state->exposure_patch_count - i - 1));
        }
        state->exposure_patch_offset = patch_offset;
        state->patches_covered = patch_offset;
    }
}

bool state_test_strip_process(state_t *state_base, state_controller_t *controller)
{
    state_test_strip_t *state = (state_test_strip_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);
    const enlarger_config_t *enlarger_config = state_controller_get_enlarger_config(controller);

    enlarger_control_set_state_safe(&enlarger_config->control, false);

    bool canceled = false;
    float patch_time;
    if (state->patches_covered == state->exposure_patch_count) {
        patch_time = exposure_get_test_strip_time_complete(exposure_state, 0);
        state->elements.covered_patches = 0xFF;
        canceled = true;
    } else if (state->teststrip_mode == TESTSTRIP_MODE_SEPARATE) {
        patch_time = exposure_get_test_strip_time_complete(exposure_state, state->exposure_patch_min + state->patches_covered);

        state->elements.covered_patches = 0xFF;
        state->elements.covered_patches ^= (1 << (state->exposure_patch_count - state->patches_covered - 1));
    } else {
        patch_time = exposure_get_test_strip_time_incremental(exposure_state,
            state->exposure_patch_min + state->exposure_patch_offset,
            state->patches_covered - state->exposure_patch_offset);

        state->elements.covered_patches = 0;
        for (int i = 0; i < state->patches_covered; i++) {
            state->elements.covered_patches |= (1 << (state->exposure_patch_count - i - 1));
        }
    }

    uint32_t patch_time_ms = rounded_exposure_time_ms(patch_time);

    convert_exposure_to_display_timer(&(state->elements.time_elements), patch_time_ms);

    display_draw_test_strip_elements(&state->elements);

    /* Abort if the test strip can't be created */
    if (canceled) {
        buzzer_volume_t current_volume = buzzer_get_volume();
        pam8904e_freq_t current_frequency = buzzer_get_frequency();
        buzzer_set_volume(settings_get_buzzer_volume());

        /* This is the same beep sequence as an aborted exposure */
        buzzer_set_frequency(PAM8904E_FREQ_1000HZ);
        buzzer_start();
        osDelay(pdMS_TO_TICKS(100));
        buzzer_stop();
        osDelay(pdMS_TO_TICKS(100));
        buzzer_start();
        osDelay(pdMS_TO_TICKS(100));
        buzzer_stop();
        osDelay(pdMS_TO_TICKS(450));

        buzzer_set_volume(current_volume);
        buzzer_set_frequency(current_frequency);

        state_controller_set_next_state(controller, STATE_HOME, 0);
        return true;
    }

    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, -1) == HAL_OK) {
        if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_START)
            || keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_FOOTSWITCH)) {

            if (state_test_strip_countdown(exposure_state, enlarger_config, patch_time_ms, state->patches_covered == (state->exposure_patch_count - 1))) {
                if (state->patches_covered < state->exposure_patch_count) {
                    state->patches_covered++;
                }
            } else {
                state_controller_set_next_state(controller, STATE_HOME, 0);
                canceled = true;
            }
        } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
            state_controller_set_next_state(controller, STATE_HOME, 0);
            canceled = true;
        }
    }

    if (!canceled && state->patches_covered >= state->exposure_patch_count) {
        osDelay(pdMS_TO_TICKS(500));
        state_controller_set_next_state(controller, STATE_HOME, 0);
    }

    return true;
}

static bool state_test_strip_exposure_callback(exposure_timer_state_t state, uint32_t time_ms, void *user_data)
{
    display_exposure_timer_t *elements = user_data;

    update_display_timer(elements, time_ms);
    display_redraw_test_strip_timer(elements);

    /* Handle the next keypad event without blocking */
    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, 0) == HAL_OK) {
        if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
            log_i("Canceling test strip timer at %ldms", time_ms);
            return false;
        }
    }

    return true;
}

bool state_test_strip_countdown(const exposure_state_t *exposure_state, const enlarger_config_t *enlarger_config, uint32_t patch_time_ms, bool last_patch)
{
    display_exposure_timer_t elements;
    convert_exposure_to_display_timer(&elements, patch_time_ms);

    exposure_timer_config_t timer_config = {0};
    timer_config.end_tone = last_patch ? EXPOSURE_TIMER_END_TONE_REGULAR : EXPOSURE_TIMER_END_TONE_SHORT;
    timer_config.timer_callback = state_test_strip_exposure_callback;
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

    exposure_timer_set_config_time(&timer_config, patch_time_ms, enlarger_config);

    exposure_timer_set_config(&timer_config, &enlarger_config->control);

    display_redraw_test_strip_timer(&elements);

    HAL_StatusTypeDef ret = exposure_timer_run();
    if (ret == HAL_TIMEOUT) {
        log_e("Exposure timer canceled");
    } else if (ret != HAL_OK) {
        log_e("Exposure timer error");
    }

    log_i("Exposure timer complete");

    return ret == HAL_OK;
}

void state_test_strip_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state)
{
    state_test_strip_t *state = (state_test_strip_t *)state_base;

    const enlarger_config_t *enlarger_config = state_controller_get_enlarger_config(controller);
    enlarger_control_set_state_off(&enlarger_config->control, false);

    buzzer_set_volume(state->current_volume);
    buzzer_set_frequency(state->current_frequency);
}
