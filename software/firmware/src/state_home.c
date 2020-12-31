#include "state_home.h"

#include <FreeRTOS.h>
#include <cmsis_os.h>
#include <esp_log.h>

#include "keypad.h"
#include "display.h"
#include "exposure_state.h"
#include "relay.h"
#include "illum_controller.h"
#include "util.h"

static const char *TAG = "state_home";

extern exposure_state_t exposure_state;
extern TickType_t focus_start_ticks;

void state_home_init(state_home_data_t *state_data)
{
    state_data->change_inc_pending = false;
    state_data->change_inc_swallow_release_up = false;
    state_data->change_inc_swallow_release_down = false;
    state_data->encoder_repeat = 0;
    state_data->cancel_repeat = 0;
    state_data->display_dirty = true;
}

state_identifier_t state_home(state_home_data_t *state_data)
{
    state_identifier_t next_state = STATE_HOME;

    if (state_data->display_dirty) {
        // Draw current exposure state
        display_main_elements_t main_elements;
        convert_exposure_to_display(&main_elements, &exposure_state);
        display_draw_main_elements(&main_elements);
        state_data->display_dirty = false;
    }

    // Handle the next keypad event
    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, 200) == HAL_OK) {
        if (state_data->change_inc_pending) {
            if (keypad_event.key == KEYPAD_INC_EXPOSURE && !keypad_event.pressed) {
                state_data->change_inc_swallow_release_up = false;
            } else if (keypad_event.key == KEYPAD_DEC_EXPOSURE && !keypad_event.pressed) {
                state_data->change_inc_swallow_release_down = false;
            }

            if (!state_data->change_inc_swallow_release_up && !state_data->change_inc_swallow_release_down) {
                state_data->change_inc_pending = false;
                next_state = STATE_HOME_CHANGE_TIME_INCREMENT;
            }
        } else {
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_START)
                || keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_FOOTSWITCH)) {
                next_state = STATE_TIMER;
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_FOCUS)) {
                if (!relay_enlarger_is_enabled()) {
                    ESP_LOGI(TAG, "Focus mode enabled");
                    illum_controller_safelight_state(ILLUM_SAFELIGHT_FOCUS);
                    relay_enlarger_enable(true);
                    focus_start_ticks = xTaskGetTickCount();
                } else {
                    ESP_LOGI(TAG, "Focus mode disabled");
                    relay_enlarger_enable(false);
                    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);
                    focus_start_ticks = 0;
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_EXPOSURE)) {
                exposure_adj_increase(&exposure_state);
                state_data->display_dirty = true;
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_EXPOSURE)) {
                exposure_adj_decrease(&exposure_state);
                state_data->display_dirty = true;
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_CONTRAST)) {
                exposure_contrast_increase(&exposure_state);
                state_data->display_dirty = true;
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_CONTRAST)) {
                exposure_contrast_decrease(&exposure_state);
                state_data->display_dirty = true;
            } else if (keypad_event.key == KEYPAD_ADD_ADJUSTMENT && !keypad_event.pressed) {
                next_state = STATE_ADD_ADJUSTMENT;
            } else if (keypad_event.key == KEYPAD_TEST_STRIP && !keypad_event.pressed) {
                next_state = STATE_TEST_STRIP;
            } else if (keypad_event.key == KEYPAD_ENCODER) {
                if (keypad_event.pressed || keypad_event.repeated) {
                    state_data->encoder_repeat++;
                } else {
                    if (state_data->encoder_repeat > 2) {
                        next_state = STATE_HOME_ADJUST_ABSOLUTE;
                    } else {
                        next_state = STATE_HOME_ADJUST_FINE;
                    }
                    state_data->encoder_repeat = 0;
                    state_data->display_dirty = true;
                }
            } else if (keypad_event.key == KEYPAD_MENU && !keypad_event.pressed) {
                next_state = STATE_MENU;
            } else if (keypad_event.key == KEYPAD_CANCEL) {
                if (keypad_event.pressed || keypad_event.repeated) {
                    state_data->cancel_repeat++;
                } else {
                    if (state_data->cancel_repeat > 2) {
                        exposure_state_defaults(&exposure_state);
                        state_data->display_dirty = true;
                    }
                    state_data->cancel_repeat = 0;
                }
            } else if (keypad_is_key_combo_pressed(&keypad_event, KEYPAD_INC_EXPOSURE, KEYPAD_DEC_EXPOSURE)) {
                state_data->change_inc_pending = true;
                state_data->change_inc_swallow_release_up = true;
                state_data->change_inc_swallow_release_down = true;
            }
        }

        // Key events should reset the focus timeout
        if (relay_enlarger_is_enabled()) {
            focus_start_ticks = xTaskGetTickCount();
        }
    }

    if (next_state != STATE_HOME
        && next_state != STATE_HOME_CHANGE_TIME_INCREMENT
        && next_state != STATE_ADD_ADJUSTMENT) {
        if (relay_enlarger_is_enabled()) {
            ESP_LOGI(TAG, "Focus mode disabled due to state change");
            relay_enlarger_enable(false);
            illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);
            focus_start_ticks = 0;
        }
    }

    if (next_state != STATE_HOME) {
        state_data->display_dirty = true;
    }

    return next_state;
}

state_identifier_t state_home_change_time_increment()
{
    state_identifier_t next_state = STATE_HOME_CHANGE_TIME_INCREMENT;

    // Draw current stop increment
    uint8_t stop_inc_den = exposure_adj_increment_get_denominator(&exposure_state);
    display_draw_stop_increment(stop_inc_den);

    // Handle the next keypad event
    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, 200) == HAL_OK) {
        if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_EXPOSURE)) {
            exposure_adj_increment_increase(&exposure_state);
        } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_EXPOSURE)) {
            exposure_adj_increment_decrease(&exposure_state);
        } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
            next_state = STATE_HOME;
        }

        // Key events should reset the focus timeout
        if (relay_enlarger_is_enabled()) {
            focus_start_ticks = xTaskGetTickCount();
        }
    }

    return next_state;
}

state_identifier_t state_home_adjust_fine(bool state_changed)
{
    static int working_value = 0;
    static int min_value = 0;
    static int max_value = 0;

    state_identifier_t next_state = STATE_HOME_ADJUST_FINE;

    if (state_changed) {
        working_value = exposure_state.adjustment_value;
        min_value = exposure_adj_min(&exposure_state);
        max_value = exposure_adj_max(&exposure_state);
    }

    display_draw_exposure_adj(working_value);

    // Handle the next keypad event
    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, 200) == HAL_OK) {
        if (keypad_event.key == KEYPAD_ENCODER_CW) {
            if (working_value <= max_value) {
                working_value++;
            }
        } else if (keypad_event.key == KEYPAD_ENCODER_CCW) {
            if (working_value >= min_value) {
                working_value--;
            }
        } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_ENCODER)) {
            exposure_adj_set(&exposure_state, working_value);
            next_state = STATE_HOME;
        } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
            next_state = STATE_HOME;
        }

        // Key events should reset the focus timeout
        if (relay_enlarger_is_enabled()) {
            focus_start_ticks = xTaskGetTickCount();
        }
    }

    return next_state;
}

state_identifier_t state_home_adjust_absolute(bool state_changed)
{
    static uint32_t working_value = 0;

    state_identifier_t next_state = STATE_HOME_ADJUST_ABSOLUTE;

    if (state_changed) {
        working_value = rounded_exposure_time_ms(exposure_state.adjusted_time);
    }

    display_exposure_timer_t elements;
    convert_exposure_to_display_timer(&elements, working_value);

    display_draw_timer_adj(&elements);

    // Handle the next keypad event
    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, 200) == HAL_OK) {
        if (keypad_event.key == KEYPAD_ENCODER_CW) {
            if (working_value < 10000) {
                working_value += 10;
                if (working_value > 10000) { working_value = 10000; }
            } else if (working_value < 100000) {
                working_value += 100;
                if (working_value > 100000) { working_value = 100000; }
            } else if (working_value < 999000) {
                working_value += 1000;
                if (working_value > 999000) { working_value = 999000; }
            }
        } else if (keypad_event.key == KEYPAD_ENCODER_CCW) {
            if (working_value <= 10000) {
                working_value -= 10;
                if (working_value < 10 || working_value > 1000000) { working_value = 10; }
            } else if (working_value <= 100000) {
                working_value -= 100;
                if (working_value < 10000) { working_value = 10000; }
            } else if (working_value <= 999000) {
                working_value -= 1000;
                if (working_value < 100000) { working_value = 100000; }
            }
        } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_EXPOSURE)) {
            if (working_value < 10000) {
                working_value += 100;
                if (working_value > 10000) { working_value = 10000; }
            } else if (working_value < 100000) {
                working_value += 1000;
                if (working_value > 100000) { working_value = 100000; }
            } else if (working_value < 999000) {
                working_value += 10000;
                if (working_value > 999000) { working_value = 999000; }
            }
        } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_EXPOSURE)) {
            if (working_value <= 10000) {
                working_value -= 100;
                if (working_value < 10 || working_value > 1000000) { working_value = 10; }
            } else if (working_value <= 100000) {
                working_value -= 1000;
                if (working_value < 10000) { working_value = 10000; }
            } else if (working_value <= 999000) {
                working_value -= 10000;
                if (working_value < 100000) { working_value = 100000; }
            }
        } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_ENCODER)) {
            exposure_set_base_time(&exposure_state, working_value / 1000.0f);
            next_state = STATE_HOME;
        } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
            next_state = STATE_HOME;
        }

        // Key events should reset the focus timeout
        if (relay_enlarger_is_enabled()) {
            focus_start_ticks = xTaskGetTickCount();
        }
    }

    return next_state;
}
