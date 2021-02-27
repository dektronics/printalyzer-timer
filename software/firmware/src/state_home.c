#include "state_home.h"

#include <FreeRTOS.h>
#include <cmsis_os.h>
#include <esp_log.h>
#include <string.h>
#include <math.h>

#include "keypad.h"
#include "display.h"
#include "exposure_state.h"
#include "relay.h"
#include "illum_controller.h"
#include "meter_probe.h"
#include "buzzer.h"
#include "settings.h"
#include "util.h"

static const char *TAG = "state_home";

typedef struct {
    state_t base;
    bool change_inc_pending;
    bool change_inc_swallow_release_up;
    bool change_inc_swallow_release_down;
    bool change_mode_pending;
    bool change_mode_swallow_release_left;
    bool change_mode_swallow_release_right;
    int encoder_repeat;
    int adjustment_repeat;
    int cancel_repeat;
    bool display_dirty;
} state_home_t;

typedef struct {
    state_t base;
} state_home_change_time_increment_t;

typedef struct {
    state_t base;
    exposure_mode_t working_value;
} state_home_change_mode_t;

typedef struct {
    state_t base;
    int working_value;
    int min_value;
    int max_value;
    bool value_accepted;
} state_home_adjust_fine_t;

typedef struct {
    state_t base;
    uint32_t working_value;
    bool value_accepted;
} state_home_adjust_absolute_t;

static void state_home_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param);
static bool state_home_process(state_t *state_base, state_controller_t *controller);
static void state_home_take_reading(state_home_t *state, state_controller_t *controller);
static void state_home_reading_warning_beep();
static void state_home_reading_error_beep();
static void state_home_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state);
static state_home_t state_home_data = {
    .base = {
        .state_entry = state_home_entry,
        .state_process = state_home_process,
        .state_exit = state_home_exit
    },
    .change_inc_pending = false,
    .change_inc_swallow_release_up = false,
    .change_inc_swallow_release_down = false,
    .change_mode_pending = false,
    .change_mode_swallow_release_left = false,
    .change_mode_swallow_release_right = false,
    .encoder_repeat = 0,
    .adjustment_repeat = 0,
    .cancel_repeat = 0,
    .display_dirty = true
};

static bool state_home_change_time_increment_process(state_t *state_base, state_controller_t *controller);
static state_home_change_time_increment_t state_home_change_time_increment_data = {
    .base = {
        .state_process = state_home_change_time_increment_process
    }
};

static void state_home_change_mode_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param);
static bool state_home_change_mode_process(state_t *state_base, state_controller_t *controller);
static void state_home_change_mode_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state);
static state_home_change_mode_t state_home_change_mode_data = {
    .base = {
        .state_entry = state_home_change_mode_entry,
        .state_process = state_home_change_mode_process,
        .state_exit = state_home_change_mode_exit
    }
};

static void state_home_adjust_fine_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param);
static bool state_home_adjust_fine_process(state_t *state_base, state_controller_t *controller);
static void state_home_adjust_fine_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state);
static state_home_adjust_fine_t state_home_adjust_fine_data = {
    .base = {
        .state_entry = state_home_adjust_fine_entry,
        .state_process = state_home_adjust_fine_process,
        .state_exit = state_home_adjust_fine_exit
    },
    .working_value = 0,
    .min_value = 0,
    .max_value = 0,
    .value_accepted = false
};

static void state_home_adjust_absolute_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param);
static bool state_home_adjust_absolute_process(state_t *state_base, state_controller_t *controller);
static void state_home_adjust_absolute_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state);
static state_home_adjust_absolute_t state_home_adjust_absolute_data = {
    .base = {
        .state_entry = state_home_adjust_absolute_entry,
        .state_process = state_home_adjust_absolute_process,
        .state_exit = state_home_adjust_absolute_exit
    },
    .working_value = 0,
    .value_accepted = false
};

state_t *state_home()
{
    return (state_t *)&state_home_data;
}

void state_home_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param)
{
    state_home_t *state = (state_home_t *)state_base;

    state->change_inc_pending = false;
    state->change_inc_swallow_release_up = false;
    state->change_inc_swallow_release_down = false;
    state->change_mode_pending = false;
    state->change_mode_swallow_release_left = false;
    state->change_mode_swallow_release_right = false;
    state->encoder_repeat = 0;
    state->adjustment_repeat = 0;
    state->cancel_repeat = 0;
    state->display_dirty = true;
}

bool state_home_process(state_t *state_base, state_controller_t *controller)
{
    state_home_t *state = (state_home_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);

    if (state->display_dirty) {
        // Draw current exposure state
        display_main_elements_t main_elements;
        convert_exposure_to_display(&main_elements, exposure_state);
        display_draw_main_elements(&main_elements);
        state->display_dirty = false;
    }

    // Handle the next keypad event
    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, STATE_KEYPAD_WAIT) == HAL_OK) {
        exposure_mode_t mode = exposure_get_mode(exposure_state);

        if (state->change_inc_pending) {
            if (keypad_event.key == KEYPAD_INC_EXPOSURE && !keypad_event.pressed) {
                state->change_inc_swallow_release_up = false;
            } else if (keypad_event.key == KEYPAD_DEC_EXPOSURE && !keypad_event.pressed) {
                state->change_inc_swallow_release_down = false;
            }

            if (!state->change_inc_swallow_release_up && !state->change_inc_swallow_release_down) {
                state->change_inc_pending = false;
                state_controller_set_next_state(controller, STATE_HOME_CHANGE_TIME_INCREMENT, 0);
            }
        } else if (state->change_mode_pending) {
            if (keypad_event.key == KEYPAD_INC_CONTRAST && !keypad_event.pressed) {
                state->change_mode_swallow_release_right = false;
            } else if (keypad_event.key == KEYPAD_DEC_CONTRAST && !keypad_event.pressed) {
                state->change_mode_swallow_release_left = false;
            }

            if (!state->change_mode_swallow_release_right && !state->change_mode_swallow_release_left) {
                state->change_mode_pending = false;
                state_controller_set_next_state(controller, STATE_HOME_CHANGE_MODE, 0);
            }
        } else {
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_START)
                || keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_FOOTSWITCH)) {
                state_controller_set_next_state(controller, STATE_TIMER, 0);
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_FOCUS)) {
                if (!relay_enlarger_is_enabled()) {
                    ESP_LOGI(TAG, "Focus mode enabled");
                    illum_controller_safelight_state(ILLUM_SAFELIGHT_FOCUS);
                    relay_enlarger_enable(true);
                    state_controller_start_focus_timeout(controller);
                } else {
                    ESP_LOGI(TAG, "Focus mode disabled");
                    relay_enlarger_enable(false);
                    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);
                    state_controller_stop_focus_timeout(controller);
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_EXPOSURE)) {
                exposure_adj_increase(exposure_state);
                state->display_dirty = true;
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_EXPOSURE)) {
                exposure_adj_decrease(exposure_state);
                state->display_dirty = true;
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_CONTRAST)) {
                if (mode == EXPOSURE_MODE_PRINTING) {
                    exposure_contrast_increase(exposure_state);
                } else if (mode == EXPOSURE_MODE_CALIBRATION) {
                    exposure_calibration_pev_increase(exposure_state);
                }
                state->display_dirty = true;
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_CONTRAST)) {
                if (mode == EXPOSURE_MODE_PRINTING) {
                    exposure_contrast_decrease(exposure_state);
                } else if (mode == EXPOSURE_MODE_CALIBRATION) {
                    exposure_calibration_pev_decrease(exposure_state);
                }
                state->display_dirty = true;
            } else if (keypad_event.key == KEYPAD_ADD_ADJUSTMENT
                && mode == EXPOSURE_MODE_PRINTING) {
                if (keypad_event.pressed || keypad_event.repeated) {
                    state->adjustment_repeat++;
                } else {
                    int burn_dodge_count = exposure_burn_dodge_count(exposure_state);
                    if (state->adjustment_repeat > 2) {
                        if (burn_dodge_count > 0) {
                            state_controller_set_next_state(controller, STATE_LIST_ADJUSTMENTS, 0);
                        }
                    } else {
                        if (burn_dodge_count < EXPOSURE_BURN_DODGE_MAX) {
                            state_controller_set_next_state(controller, STATE_EDIT_ADJUSTMENT, burn_dodge_count);
                        }
                    }
                    state->adjustment_repeat = 0;
                    state->display_dirty = true;
                }
            } else if (keypad_event.key == KEYPAD_TEST_STRIP && !keypad_event.pressed) {
                state_controller_set_next_state(controller, STATE_TEST_STRIP, 0);
            } else if (keypad_event.key == KEYPAD_ENCODER) {
                if (keypad_event.pressed || keypad_event.repeated) {
                    state->encoder_repeat++;
                } else {
                    if (state->encoder_repeat > 2) {
                        state_controller_set_next_state(controller, STATE_HOME_ADJUST_ABSOLUTE, 0);
                    } else {
                        state_controller_set_next_state(controller, STATE_HOME_ADJUST_FINE, 0);
                    }
                    state->encoder_repeat = 0;
                    state->display_dirty = true;
                }
            } else if (keypad_event.key == KEYPAD_MENU && !keypad_event.pressed) {
                state_controller_set_next_state(controller, STATE_MENU, 0);
            } else if (keypad_event.key == KEYPAD_CANCEL) {
                if (keypad_event.pressed || keypad_event.repeated) {
                    state->cancel_repeat++;
                } else {
                    if (state->cancel_repeat > 2) {
                        exposure_clear_meter_readings(exposure_state);
                        exposure_state_defaults(exposure_state);
                    } else {
                        exposure_clear_meter_readings(exposure_state);
                    }
                    state->display_dirty = true;
                    state->cancel_repeat = 0;
                }
            } else if (keypad_event.key == KEYPAD_METER_PROBE && !keypad_event.pressed
                && relay_enlarger_is_enabled()) {
                state_home_take_reading(state, controller);
                state->display_dirty = true;
            } else if (keypad_is_key_combo_pressed(&keypad_event, KEYPAD_INC_EXPOSURE, KEYPAD_DEC_EXPOSURE)) {
                state->change_inc_pending = true;
                state->change_inc_swallow_release_up = true;
                state->change_inc_swallow_release_down = true;
            } else if (keypad_is_key_combo_pressed(&keypad_event, KEYPAD_INC_CONTRAST, KEYPAD_DEC_CONTRAST)) {
                state->change_mode_pending = true;
                state->change_mode_swallow_release_left = true;
                state->change_mode_swallow_release_right = true;
            }
        }

        return true;
    } else {
        return false;
    }
}

void state_home_take_reading(state_home_t *state, state_controller_t *controller)
{
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);
    meter_probe_result_t result = METER_READING_OK;
    float lux = 0;
    uint32_t cct = 0;

    display_draw_mode_text("Measuring");

    do {
        illum_controller_safelight_state(ILLUM_SAFELIGHT_MEASUREMENT);
        osDelay(SAFELIGHT_OFF_DELAY / 2);

        result = meter_probe_initialize();
        if (result != METER_READING_OK) {
            break;
        }

        result = meter_probe_measure(&lux, &cct);
        if (result != METER_READING_OK) {
            break;
        }
    } while (0);

    meter_probe_shutdown();

    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

    if (result == METER_READING_OK) {
        //TODO If CCT is far from the enlarger profile, warn about filters/safelights/etc
        exposure_add_meter_reading(exposure_state, lux);
        ESP_LOGI(TAG, "Measured PEV=%lu, CCT=%luK", exposure_get_calibration_pev(exposure_state), cct);
    } else if (result == METER_READING_LOW) {
        display_draw_mode_text("Light Low");
        state_home_reading_warning_beep();
        osDelay(pdMS_TO_TICKS(2000));
    } else if (result == METER_READING_HIGH) {
        display_draw_mode_text("Light High");
        state_home_reading_warning_beep();
        osDelay(pdMS_TO_TICKS(2000));
    } else if (result == METER_READING_TIMEOUT) {
        display_draw_mode_text("Timeout");
        state_home_reading_error_beep();
        osDelay(pdMS_TO_TICKS(2000));
    } else if (result == METER_READING_FAIL) {
        display_draw_mode_text("Meter Error");
        state_home_reading_error_beep();
        osDelay(pdMS_TO_TICKS(2000));
    }
}

void state_home_reading_warning_beep()
{
    buzzer_volume_t current_volume = buzzer_get_volume();
    pam8904e_freq_t current_frequency = buzzer_get_frequency();
    buzzer_set_volume(settings_get_buzzer_volume());

    buzzer_set_frequency(PAM8904E_FREQ_2000HZ);
    buzzer_start();
    osDelay(pdMS_TO_TICKS(50));
    buzzer_stop();
    osDelay(pdMS_TO_TICKS(100));
    buzzer_start();
    osDelay(pdMS_TO_TICKS(50));
    buzzer_stop();

    buzzer_set_volume(current_volume);
    buzzer_set_frequency(current_frequency);
}

void state_home_reading_error_beep()
{
    buzzer_volume_t current_volume = buzzer_get_volume();
    pam8904e_freq_t current_frequency = buzzer_get_frequency();
    buzzer_set_volume(settings_get_buzzer_volume());

    buzzer_set_frequency(PAM8904E_FREQ_500HZ);
    buzzer_start();
    osDelay(pdMS_TO_TICKS(100));
    buzzer_stop();
    osDelay(pdMS_TO_TICKS(100));
    buzzer_start();
    osDelay(pdMS_TO_TICKS(100));
    buzzer_stop();

    buzzer_set_volume(current_volume);
    buzzer_set_frequency(current_frequency);
}

void state_home_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state)
{
    if (next_state != STATE_HOME_CHANGE_TIME_INCREMENT
        && next_state != STATE_HOME_ADJUST_FINE
        && next_state != STATE_HOME_ADJUST_ABSOLUTE
        && next_state != STATE_EDIT_ADJUSTMENT
        && next_state != STATE_LIST_ADJUSTMENTS) {
        if (relay_enlarger_is_enabled()) {
            ESP_LOGI(TAG, "Focus mode disabled due to state change");
            relay_enlarger_enable(false);
            illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);
            state_controller_stop_focus_timeout(controller);
        }
    }
}

state_t *state_home_change_time_increment()
{
    return (state_t *)&state_home_change_time_increment_data;
}

bool state_home_change_time_increment_process(state_t *state_base, state_controller_t *controller)
{
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);

    // Draw current stop increment
    uint8_t stop_inc_den = exposure_adj_increment_get_denominator(exposure_state);
    display_draw_stop_increment(stop_inc_den);

    // Handle the next keypad event
    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, STATE_KEYPAD_WAIT) == HAL_OK) {
        if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_EXPOSURE)) {
            exposure_adj_increment_increase(exposure_state);
        } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_EXPOSURE)) {
            exposure_adj_increment_decrease(exposure_state);
        } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
            state_controller_set_next_state(controller, STATE_HOME, 0);
        }
        return true;
    } else {
        return false;
    }
}

state_t *state_home_change_mode()
{
    return (state_t *)&state_home_change_mode_data;
}

void state_home_change_mode_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param)
{
    state_home_change_mode_t *state = (state_home_change_mode_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);

    state->working_value = exposure_get_mode(exposure_state);
}

bool state_home_change_mode_process(state_t *state_base, state_controller_t *controller)
{
    state_home_change_mode_t *state = (state_home_change_mode_t *)state_base;

    // Draw the current mode
    switch (state->working_value) {
    case EXPOSURE_MODE_PRINTING:
        display_draw_mode_text("Printing");
        break;
    case EXPOSURE_MODE_CALIBRATION:
        display_draw_mode_text("Calibration");
        break;
    default:
        display_draw_mode_text("Unknown");
        break;
    }

    // Handle the next keypad event
    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, STATE_KEYPAD_WAIT) == HAL_OK) {
        if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_CONTRAST)
            || keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_CONTRAST)) {
            switch (state->working_value) {
            case EXPOSURE_MODE_PRINTING:
                state->working_value = EXPOSURE_MODE_CALIBRATION;
                break;
            case EXPOSURE_MODE_CALIBRATION:
                state->working_value = EXPOSURE_MODE_PRINTING;
                break;
            default:
                state->working_value = EXPOSURE_MODE_PRINTING;
                break;
            }
        } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
            state_controller_set_next_state(controller, STATE_HOME, 0);
        }
        return true;
    } else {
        return false;
    }

    return true;
}

void state_home_change_mode_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state)
{
    state_home_change_mode_t *state = (state_home_change_mode_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);

    exposure_set_mode(exposure_state, state->working_value);
}

state_t *state_home_adjust_fine()
{
    return (state_t *)&state_home_adjust_fine_data;
}

void state_home_adjust_fine_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param)
{
    state_home_adjust_fine_t *state = (state_home_adjust_fine_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);

    state->working_value = exposure_adj_get(exposure_state);
    state->min_value = exposure_adj_min(exposure_state);
    state->max_value = exposure_adj_max(exposure_state);
    state->value_accepted = false;
}

bool state_home_adjust_fine_process(state_t *state_base, state_controller_t *controller)
{
    state_home_adjust_fine_t *state = (state_home_adjust_fine_t *)state_base;

    display_draw_exposure_adj(state->working_value);

    // Handle the next keypad event
    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, STATE_KEYPAD_WAIT) == HAL_OK) {
        if (keypad_event.key == KEYPAD_ENCODER_CW) {
            if (state->working_value <= state->max_value) {
                state->working_value++;
            }
        } else if (keypad_event.key == KEYPAD_ENCODER_CCW) {
            if (state->working_value >= state->min_value) {
                state->working_value--;
            }
        } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_ENCODER)) {
            state->value_accepted = true;
            state_controller_set_next_state(controller, STATE_HOME, 0);
        } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
            state_controller_set_next_state(controller, STATE_HOME, 0);
        }

        return true;
    } else {
        return false;
    }
}

void state_home_adjust_fine_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state)
{
    state_home_adjust_fine_t *state = (state_home_adjust_fine_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);

    if (state->value_accepted) {
        exposure_adj_set(exposure_state, state->working_value);
    }
}

state_t *state_home_adjust_absolute()
{
    return (state_t *)&state_home_adjust_absolute_data;
}

void state_home_adjust_absolute_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param)
{
    state_home_adjust_absolute_t *state = (state_home_adjust_absolute_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);

    state->working_value = rounded_exposure_time_ms(exposure_get_exposure_time(exposure_state));
    state->value_accepted = false;
}

bool state_home_adjust_absolute_process(state_t *state_base, state_controller_t *controller)
{
    state_home_adjust_absolute_t *state = (state_home_adjust_absolute_t *)state_base;

    display_exposure_timer_t elements;
    convert_exposure_to_display_timer(&elements, state->working_value);

    display_draw_timer_adj(&elements);

    // Handle the next keypad event
    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, STATE_KEYPAD_WAIT) == HAL_OK) {
        if (keypad_event.key == KEYPAD_ENCODER_CW) {
            if (state->working_value < 10000) {
                state->working_value += 10;
                if (state->working_value > 10000) { state->working_value = 10000; }
            } else if (state->working_value < 100000) {
                state->working_value += 100;
                if (state->working_value > 100000) { state->working_value = 100000; }
            } else if (state->working_value < 999000) {
                state->working_value += 1000;
                if (state->working_value > 999000) { state->working_value = 999000; }
            }
        } else if (keypad_event.key == KEYPAD_ENCODER_CCW) {
            if (state->working_value <= 10000) {
                state->working_value -= 10;
                if (state->working_value < 10 || state->working_value > 1000000) { state->working_value = 10; }
            } else if (state->working_value <= 100000) {
                state->working_value -= 100;
                if (state->working_value < 10000) { state->working_value = 10000; }
            } else if (state->working_value <= 999000) {
                state->working_value -= 1000;
                if (state->working_value < 100000) { state->working_value = 100000; }
            }
        } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_EXPOSURE)) {
            if (state->working_value < 10000) {
                state->working_value += 100;
                if (state->working_value > 10000) { state->working_value = 10000; }
            } else if (state->working_value < 100000) {
                state->working_value += 1000;
                if (state->working_value > 100000) { state->working_value = 100000; }
            } else if (state->working_value < 999000) {
                state->working_value += 10000;
                if (state->working_value > 999000) { state->working_value = 999000; }
            }
        } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_EXPOSURE)) {
            if (state->working_value <= 10000) {
                state->working_value -= 100;
                if (state->working_value < 10 || state->working_value > 1000000) { state->working_value = 10; }
            } else if (state->working_value <= 100000) {
                state->working_value -= 1000;
                if (state->working_value < 10000) { state->working_value = 10000; }
            } else if (state->working_value <= 999000) {
                state->working_value -= 10000;
                if (state->working_value < 100000) { state->working_value = 100000; }
            }
        } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_ENCODER)) {
            state->value_accepted = true;
            state_controller_set_next_state(controller, STATE_HOME, 0);
        } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
            state_controller_set_next_state(controller, STATE_HOME, 0);
        }

        return true;
    } else {
        return false;
    }
}

void state_home_adjust_absolute_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state)
{
    state_home_adjust_absolute_t *state = (state_home_adjust_absolute_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);

    if (state->value_accepted) {
        exposure_set_base_time(exposure_state, state->working_value / 1000.0f);
    }
}
