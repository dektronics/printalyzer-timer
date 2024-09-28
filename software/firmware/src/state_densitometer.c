#include "state_densitometer.h"

#define LOG_TAG "state_densitometer"
#include <elog.h>

#include <math.h>
#include <string.h>

#include "keypad_action.h"
#include "display.h"
#include "illum_controller.h"
#include "meter_probe.h"
#include "buzzer.h"
#include "usb_host.h"
#include "densitometer.h"
#include "util.h"

typedef enum {
    ACTION_NONE = 0,
    ACTION_TIMER,
    ACTION_FOCUS,
    ACTION_PREV_TYPE,
    ACTION_NEXT_TYPE,
    ACTION_MENU,
    ACTION_CLEAR_READINGS,
    ACTION_TAKE_PROBE_READING,
    ACTION_TAKE_STICK_READING,
    ACTION_CHANGE_MODE
} state_home_actions_t;

typedef struct {
    state_t base;
    bool enable_meter_probe;
    bool enable_densistick;
    bool enable_densistick_idle_light;
    bool display_dirty;
    uint8_t selected_mode;
    float probe_reading_base;
    float probe_reading_current;
    densitometer_reading_t dens_reading;
} state_densitometer_t;

static void state_densitometer_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param);
static bool state_densitometer_process(state_t *state_base, state_controller_t *controller);
static void state_densitometer_check_meter_probe(state_densitometer_t *state);
static void state_densitometer_check_densistick(state_densitometer_t *state);
static bool state_densitometer_take_probe_reading(state_densitometer_t *state, state_controller_t *controller);
static bool state_densitometer_take_densistick_reading(state_densitometer_t *state, state_controller_t *controller);
static float state_densitometer_probe_relative_density(state_densitometer_t *state);
static void state_densitometer_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state);
static state_densitometer_t state_densitometer_data = {
    .base = {
        .state_entry = state_densitometer_entry,
        .state_process = state_densitometer_process,
        .state_exit = state_densitometer_exit
    },
    .display_dirty = true,
    .enable_meter_probe = false,
    .enable_densistick = false,
    .enable_densistick_idle_light = false,
    .selected_mode = 0,
    .probe_reading_base = NAN,
    .probe_reading_current = NAN,
    .dens_reading = {
        .mode = DENSITOMETER_MODE_UNKNOWN,
        .visual = NAN,
        .red = NAN,
        .green = NAN,
        .blue = NAN
    }
};

state_t *state_densitometer()
{
    return (state_t *)&state_densitometer_data;
}

void state_densitometer_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param)
{
    state_densitometer_t *state = (state_densitometer_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);
    state->display_dirty = true;

    /* Configure keypad actions */
    keypad_action_clear();
    keypad_action_add(KEYPAD_START, ACTION_TIMER, 0, true);
    keypad_action_add(KEYPAD_FOOTSWITCH, ACTION_TIMER, 0, true);
    keypad_action_add(KEYPAD_FOCUS, ACTION_FOCUS, 0, true);
    keypad_action_add(KEYPAD_INC_EXPOSURE, ACTION_PREV_TYPE, 0, true);
    keypad_action_add(KEYPAD_DEC_EXPOSURE, ACTION_NEXT_TYPE, 0, true);
    keypad_action_add(KEYPAD_MENU, ACTION_MENU, 0, false);
    keypad_action_add(KEYPAD_CANCEL, ACTION_CLEAR_READINGS, 0, true);
    keypad_action_add(KEYPAD_METER_PROBE, ACTION_TAKE_PROBE_READING, 0, false);
    keypad_action_add(KEYPAD_DENSISTICK, ACTION_TAKE_STICK_READING, 0, false);
    keypad_action_add_combo(KEYPAD_INC_CONTRAST, KEYPAD_DEC_CONTRAST, ACTION_CHANGE_MODE);

    /* Select the lowest light reading, if available, and make it the base value */
    if (isnan(state->probe_reading_base)) {
        float lowest_lux = exposure_get_lowest_meter_reading(exposure_state);
        state->probe_reading_base = lowest_lux;
        state->probe_reading_current = NAN;
    }

    if (state->selected_mode == 0) {
        state->enable_densistick_idle_light = false;
    } else {
        state->enable_densistick_idle_light = true;
    }

    usb_serial_clear_receive_buffer();
}

bool state_densitometer_process(state_t *state_base, state_controller_t *controller)
{
    state_densitometer_t *state = (state_densitometer_t *)state_base;
    densitometer_result_t dens_result = DENSITOMETER_RESULT_UNKNOWN;
    densitometer_reading_t reading;

    state_densitometer_check_meter_probe(state);
    state_densitometer_check_densistick(state);

    dens_result = densitometer_reading_poll(&reading);
    if (dens_result == DENSITOMETER_RESULT_OK) {
        densitometer_log_reading(&reading);
        /* Only accept "visual" readings since this screen cannot currently display color readings */
        if (is_valid_number(reading.visual)) {
            memcpy(&state->dens_reading, &reading, sizeof(densitometer_reading_t));
            state->selected_mode = 1;
            state->display_dirty = true;
        }
    }

    /* Draw current exposure state */
    if (state->display_dirty) {
        display_main_densitometer_elements_t main_elements;
        main_elements.show_ind_probe = (state->selected_mode == 0) ? 2 : 1;
        main_elements.show_ind_dens = (state->selected_mode == 1) ? 2 : 1;
        main_elements.ind_dens = (state->dens_reading.mode == DENSITOMETER_MODE_TRANSMISSION) ? 1 : 0;

        float density;
        if (state->selected_mode == 0) {
            density = state_densitometer_probe_relative_density(state);
        } else {
            density = state->dens_reading.visual;
        }
        convert_density_to_display_densitometer(&main_elements, density);

        display_draw_main_elements_densitometer(&main_elements);

        state->display_dirty = false;
    }

    /* Handle the next keypad action */
    keypad_action_t keypad_action;
    if (keypad_action_wait(&keypad_action, STATE_KEYPAD_WAIT) == osOK) {
        if (keypad_action.action_id == ACTION_CHANGE_MODE) {
            state_controller_set_next_state(controller, STATE_HOME_CHANGE_MODE, 0);
        } else if (keypad_action.action_id == ACTION_FOCUS) {
            if (!state_controller_is_enlarger_focus(controller)) {
                log_i("Focus mode enabled");
                illum_controller_safelight_state(ILLUM_SAFELIGHT_FOCUS);
                state_controller_set_enlarger_focus(controller, true);
                state_controller_start_focus_timeout(controller);
                state->enable_meter_probe = true;
            } else {
                log_i("Focus mode disabled");
                state_controller_set_enlarger_focus(controller, false);
                illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);
                state_controller_stop_focus_timeout(controller);
                state->enable_meter_probe = false;
            }
        } else if (keypad_action.action_id == ACTION_PREV_TYPE) {
            if (state->selected_mode == 1) {
                state->selected_mode = 0;
                state->enable_densistick_idle_light = false;
                state_densitometer_check_densistick(state);
                state->display_dirty = true;
            }
        } else if (keypad_action.action_id == ACTION_NEXT_TYPE) {
            if (state->selected_mode == 0) {
                state->selected_mode = 1;
                state->enable_densistick = true;
                state->enable_densistick_idle_light = true;
                state_densitometer_check_densistick(state);
                state->display_dirty = true;
            }
        } else if (keypad_action.action_id == ACTION_MENU) {
            state_controller_set_next_state(controller, STATE_MENU, 0);
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_CLEAR_READINGS) {
            state->probe_reading_base = NAN;
            state->probe_reading_current = NAN;
            state->dens_reading.mode = DENSITOMETER_MODE_UNKNOWN;
            state->dens_reading.visual = NAN;
            state->dens_reading.red = NAN;
            state->dens_reading.green = NAN;
            state->dens_reading.blue = NAN;
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_TAKE_PROBE_READING) {
            if (state_controller_is_enlarger_focus(controller) && meter_probe_is_started(meter_probe_handle())) {
                if (state_densitometer_take_probe_reading(state, controller)) {
                    state->selected_mode = 0;
                }
                state->display_dirty = true;
            }
        } else if (keypad_action.action_id == ACTION_TAKE_STICK_READING) {
            if (!state->enable_densistick) {
                state->enable_densistick = true;
            }
            state_densitometer_check_densistick(state);
            if (meter_probe_is_started(densistick_handle())) {
                if (state_densitometer_take_densistick_reading(state, controller)) {
                    state->selected_mode = 1;
                    state->enable_densistick_idle_light = true;
                }
                state_densitometer_check_densistick(state);
                state->display_dirty = true;
            }
        }
        return true;
    } else {
        return false;
    }
}

void state_densitometer_check_meter_probe(state_densitometer_t *state)
{
    /* Start with an integration time of 100ms */
    static uint16_t sample_time = 719;
    static uint16_t sample_count = 99;

    meter_probe_handle_t *handle = meter_probe_handle();

    if (state->enable_meter_probe) {
        if (meter_probe_is_attached(handle) && !meter_probe_is_started(handle)) {
            if (meter_probe_start(handle) == osOK) {
                meter_probe_sensor_set_gain(handle, TSL2585_GAIN_256X);
                meter_probe_sensor_set_integration(handle, sample_time, sample_count);
                meter_probe_sensor_set_mod_calibration(handle, 1);
                meter_probe_sensor_enable_agc(handle, sample_count);
                meter_probe_sensor_enable(handle);
            }
        }
    } else {
        if (meter_probe_is_started(handle)) {
            meter_probe_sensor_disable(handle);
            meter_probe_stop(handle);
        }
    }
}

void state_densitometer_check_densistick(state_densitometer_t *state)
{
    meter_probe_handle_t *handle = densistick_handle();

    if (state->enable_densistick) {
        if (meter_probe_is_attached(handle) && !meter_probe_is_started(handle)) {
            meter_probe_start(handle);
        }
        if (state->enable_densistick_idle_light) {
            if (densistick_get_light_brightness(handle) != 127) {
                densistick_set_light_brightness(handle, 127);
            }
            if (!densistick_get_light_enable(handle)) {
                densistick_set_light_enable(handle, true);
            }
        } else {
            if (densistick_get_light_enable(handle)) {
                densistick_set_light_enable(handle, false);
            }
        }
    } else {
        if (meter_probe_is_started(handle)) {
            densistick_set_light_enable(handle, false);
            meter_probe_sensor_disable(handle);
            meter_probe_stop(handle);
        }
    }
}

bool state_densitometer_take_probe_reading(state_densitometer_t *state, state_controller_t *controller)
{
    meter_probe_result_t result = METER_READING_OK;
    float lux = 0;
    bool success = false;

    display_draw_mode_text("Measuring");

    do {
        illum_controller_safelight_state(ILLUM_SAFELIGHT_MEASUREMENT);
        osDelay(SAFELIGHT_OFF_DELAY / 2);

        result = meter_probe_measure(meter_probe_handle(), &lux);
        if (result != METER_READING_OK) {
            break;
        }

    } while (0);

    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

    if (result == METER_READING_OK) {
        if (isnanf(state->probe_reading_base) || lux > state->probe_reading_base) {
            state->probe_reading_base = lux;
        } else {
            state->probe_reading_current = lux;
        }
        success = true;
    } else if (result == METER_READING_LOW) {
        display_draw_mode_text("Light Low");
        buzzer_sequence(BUZZER_SEQUENCE_PROBE_WARNING);
        osDelay(pdMS_TO_TICKS(2000));
    } else if (result == METER_READING_HIGH) {
        display_draw_mode_text("Light High");
        buzzer_sequence(BUZZER_SEQUENCE_PROBE_WARNING);
        osDelay(pdMS_TO_TICKS(2000));
    } else if (result == METER_READING_TIMEOUT) {
        display_draw_mode_text("Timeout");
        buzzer_sequence(BUZZER_SEQUENCE_PROBE_ERROR);
        osDelay(pdMS_TO_TICKS(2000));
    } else if (result == METER_READING_FAIL) {
        display_draw_mode_text("Meter Error");
        buzzer_sequence(BUZZER_SEQUENCE_PROBE_ERROR);
        osDelay(pdMS_TO_TICKS(2000));
    }

    return success;
}

bool state_densitometer_take_densistick_reading(state_densitometer_t *state, state_controller_t *controller)
{
    meter_probe_result_t result = METER_READING_OK;
    float density = NAN;
    bool success = false;

    display_draw_mode_text("Measuring");

    result = densistick_measure(densistick_handle(), &density, NULL);
    if (result == METER_READING_OK) {
        state->dens_reading.mode = DENSITOMETER_MODE_REFLECTION;
        state->dens_reading.visual = density;
        success = true;
    } else if (result == METER_READING_TIMEOUT) {
        display_draw_mode_text("Timeout");
        buzzer_sequence(BUZZER_SEQUENCE_PROBE_ERROR);
        osDelay(pdMS_TO_TICKS(2000));
    } else  {
        display_draw_mode_text("Reading Error");
        buzzer_sequence(BUZZER_SEQUENCE_PROBE_ERROR);
        osDelay(pdMS_TO_TICKS(2000));
    }

    return success;
}

float state_densitometer_probe_relative_density(state_densitometer_t *state)
{
    float result;
    if (!state) { return NAN; }

    const bool base_valid = is_valid_number(state->probe_reading_base);
    const bool current_valid = is_valid_number(state->probe_reading_current);

    if (!base_valid) {
        result =  NAN;
    } else if (base_valid && !current_valid) {
        result = 0.0F;
    } else {
        result = -1.0F * log10f(state->probe_reading_current / state->probe_reading_base);
    }

    /* Prevent negative results */
    if (isnormal(result) && result < 0.0F) {
        result = 0.0F;
    }

    return result;
}

void state_densitometer_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state)
{
    state_densitometer_t *state = (state_densitometer_t *)state_base;

    if (state_controller_is_enlarger_focus(controller)) {
        log_i("Focus mode disabled due to state change");
        illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);
        state_controller_set_enlarger_focus(controller, false);
        state_controller_stop_focus_timeout(controller);
        state->enable_meter_probe = false;
        state_densitometer_check_meter_probe(state);
    }

    state->enable_densistick = false;
    state->enable_densistick_idle_light = false;
    state_densitometer_check_densistick(state);
}
