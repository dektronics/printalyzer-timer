#include "state_home.h"

#include <FreeRTOS.h>
#include <cmsis_os.h>
#include <esp_log.h>
#include <string.h>

#include "keypad.h"
#include "display.h"
#include "exposure_state.h"
#include "relay.h"
#include "illum_controller.h"
#include "util.h"

static const char *TAG = "state_home";

typedef struct {
    state_t base;
    bool change_inc_pending;
    bool change_inc_swallow_release_up;
    bool change_inc_swallow_release_down;
    int encoder_repeat;
    int cancel_repeat;
    bool display_dirty;
} state_home_t;

typedef struct {
    state_t base;
} state_home_change_time_increment_t;

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

static void state_home_entry(state_t *state_base, state_controller_t *controller);
static bool state_home_process(state_t *state_base, state_controller_t *controller);
static void state_home_exit(state_t *state_base, state_controller_t *controller);
static state_home_t state_home_data = {
    .base = {
        .state_entry = state_home_entry,
        .state_process = state_home_process,
        .state_exit = state_home_exit
    },
    .change_inc_pending = false,
    .change_inc_swallow_release_up = false,
    .change_inc_swallow_release_down = false,
    .encoder_repeat = 0,
    .cancel_repeat = 0,
    .display_dirty = true
};

static bool state_home_change_time_increment_process(state_t *state_base, state_controller_t *controller);
static state_home_change_time_increment_t state_home_change_time_increment_data = {
    .base = {
        .state_process = state_home_change_time_increment_process
    }
};

static void state_home_adjust_fine_entry(state_t *state_base, state_controller_t *controller);
static bool state_home_adjust_fine_process(state_t *state_base, state_controller_t *controller);
static void state_home_adjust_fine_exit(state_t *state_base, state_controller_t *controller);
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

static void state_home_adjust_absolute_entry(state_t *state_base, state_controller_t *controller);
static bool state_home_adjust_absolute_process(state_t *state_base, state_controller_t *controller);
static void state_home_adjust_absolute_exit(state_t *state_base, state_controller_t *controller);
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

void state_home_entry(state_t *state_base, state_controller_t *controller)
{
    state_home_t *state = (state_home_t *)state_base;
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
        if (state->change_inc_pending) {
            if (keypad_event.key == KEYPAD_INC_EXPOSURE && !keypad_event.pressed) {
                state->change_inc_swallow_release_up = false;
            } else if (keypad_event.key == KEYPAD_DEC_EXPOSURE && !keypad_event.pressed) {
                state->change_inc_swallow_release_down = false;
            }

            if (!state->change_inc_swallow_release_up && !state->change_inc_swallow_release_down) {
                state->change_inc_pending = false;
                state_controller_set_next_state(controller, STATE_HOME_CHANGE_TIME_INCREMENT);
            }
        } else {
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_START)
                || keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_FOOTSWITCH)) {
                state_controller_set_next_state(controller, STATE_TIMER);
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
                exposure_contrast_increase(exposure_state);
                state->display_dirty = true;
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_CONTRAST)) {
                exposure_contrast_decrease(exposure_state);
                state->display_dirty = true;
            } else if (keypad_event.key == KEYPAD_ADD_ADJUSTMENT && !keypad_event.pressed) {
                state_controller_set_next_state(controller, STATE_ADD_ADJUSTMENT);
            } else if (keypad_event.key == KEYPAD_TEST_STRIP && !keypad_event.pressed) {
                state_controller_set_next_state(controller, STATE_TEST_STRIP);
            } else if (keypad_event.key == KEYPAD_ENCODER) {
                if (keypad_event.pressed || keypad_event.repeated) {
                    state->encoder_repeat++;
                } else {
                    if (state->encoder_repeat > 2) {
                        state_controller_set_next_state(controller, STATE_HOME_ADJUST_ABSOLUTE);
                    } else {
                        state_controller_set_next_state(controller, STATE_HOME_ADJUST_FINE);
                    }
                    state->encoder_repeat = 0;
                    state->display_dirty = true;
                }
            } else if (keypad_event.key == KEYPAD_MENU && !keypad_event.pressed) {
                state_controller_set_next_state(controller, STATE_MENU);
            } else if (keypad_event.key == KEYPAD_CANCEL) {
                if (keypad_event.pressed || keypad_event.repeated) {
                    state->cancel_repeat++;
                } else {
                    if (state->cancel_repeat > 2) {
                        exposure_state_defaults(exposure_state);
                        state->display_dirty = true;
                    }
                    state->cancel_repeat = 0;
                }
            } else if (keypad_is_key_combo_pressed(&keypad_event, KEYPAD_INC_EXPOSURE, KEYPAD_DEC_EXPOSURE)) {
                state->change_inc_pending = true;
                state->change_inc_swallow_release_up = true;
                state->change_inc_swallow_release_down = true;
            }
        }

        return true;
    } else {
        return false;
    }
}

void state_home_exit(state_t *state_base, state_controller_t *controller)
{
    state_identifier_t next_state = state_controller_get_next_state(controller);
    if (next_state != STATE_HOME_CHANGE_TIME_INCREMENT
        && next_state != STATE_HOME_ADJUST_FINE
        && next_state != STATE_HOME_ADJUST_ABSOLUTE
        && next_state != STATE_ADD_ADJUSTMENT) {
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
            state_controller_set_next_state(controller, STATE_HOME);
        }
        return true;
    } else {
        return false;
    }
}

state_t *state_home_adjust_fine()
{
    return (state_t *)&state_home_adjust_fine_data;
}

void state_home_adjust_fine_entry(state_t *state_base, state_controller_t *controller)
{
    state_home_adjust_fine_t *state = (state_home_adjust_fine_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);

    state->working_value = exposure_state->adjustment_value;
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
            state_controller_set_next_state(controller, STATE_HOME);
        } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
            state_controller_set_next_state(controller, STATE_HOME);
        }

        return true;
    } else {
        return false;
    }
}

void state_home_adjust_fine_exit(state_t *state_base, state_controller_t *controller)
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

void state_home_adjust_absolute_entry(state_t *state_base, state_controller_t *controller)
{
    state_home_adjust_absolute_t *state = (state_home_adjust_absolute_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);

    state->working_value = rounded_exposure_time_ms(exposure_state->adjusted_time);
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
            state_controller_set_next_state(controller, STATE_HOME);
        } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
            state_controller_set_next_state(controller, STATE_HOME);
        }

        return true;
    } else {
        return false;
    }
}

void state_home_adjust_absolute_exit(state_t *state_base, state_controller_t *controller)
{
    state_home_adjust_absolute_t *state = (state_home_adjust_absolute_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);

    if (state->value_accepted) {
        exposure_set_base_time(exposure_state, state->working_value / 1000.0f);
    }
}
