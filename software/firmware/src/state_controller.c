#include "state_controller.h"

#include <FreeRTOS.h>
#include <cmsis_os.h>
#include <stdlib.h>
#include <task.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#define LOG_TAG "state_controller"
#include <elog.h>

#include "keypad.h"
#include "display.h"
#include "exposure_state.h"
#include "main_menu.h"
#include "buzzer.h"
#include "led.h"
#include "relay.h"
#include "exposure_timer.h"
#include "enlarger_config.h"
#include "enlarger_control.h"
#include "illum_controller.h"
#include "meter_probe.h"
#include "util.h"
#include "settings.h"
#include "state_home.h"
#include "state_densitometer.h"
#include "state_timer.h"
#include "state_test_strip.h"
#include "state_adjustment.h"

struct __state_controller_t {
    state_identifier_t current_state;
    uint32_t current_state_param;
    state_identifier_t next_state;
    uint32_t next_state_param;
    exposure_state_t *exposure_state;
    enlarger_config_t enlarger_config;
    bool enlarger_focus_mode;
    bool enable_meter_probe;
    TickType_t focus_start_ticks;
};

static state_controller_t state_controller = {0};
static state_t *state_map[STATE_MAX] = {0};

static bool state_menu_process(state_t *state_base, state_controller_t *controller);
static state_t state_menu_data = {
    .state_process = state_menu_process
};

void state_controller_init()
{
    if (state_controller.exposure_state) {
        exposure_state_free(state_controller.exposure_state);
        state_controller.exposure_state = NULL;
    }

    state_controller.current_state = STATE_MAX;
    state_controller.current_state_param = 0;
    state_controller.next_state = STATE_HOME;
    state_controller.next_state_param = 0;
    state_controller.exposure_state = exposure_state_create();
    state_controller.focus_start_ticks = 0;

    if (!state_controller.exposure_state) {
        abort();
    }

    state_controller_reload_enlarger_config(&state_controller);

    state_map[STATE_HOME] = state_home();
    state_map[STATE_HOME_CHANGE_TIME_INCREMENT] = state_home_change_time_increment();
    state_map[STATE_HOME_CHANGE_MODE] = state_home_change_mode();
    state_map[STATE_HOME_ADJUST_FINE] = state_home_adjust_fine();
    state_map[STATE_HOME_ADJUST_ABSOLUTE] = state_home_adjust_absolute();
    state_map[STATE_DENSITOMETER] = state_densitometer();
    state_map[STATE_TIMER] = state_timer();
    state_map[STATE_TEST_STRIP] = state_test_strip();
    state_map[STATE_EDIT_ADJUSTMENT] = state_edit_adjustment();
    state_map[STATE_LIST_ADJUSTMENTS] = state_list_adjustments();
    state_map[STATE_MENU] = &state_menu_data;
}

void state_controller_loop()
{
    state_t *state = NULL;
    for (;;) {
        // Check if we need to do a state transition
        if (state_controller.next_state != state_controller.current_state
            || state_controller.next_state_param != state_controller.current_state_param) {
            // Transition to the new state
            log_i("State transition: %d[%lu] -> %d[%lu]",
                state_controller.current_state, state_controller.current_state_param,
                state_controller.next_state, state_controller.next_state_param);
            state_identifier_t prev_state = state_controller.current_state;
            state_controller.current_state = state_controller.next_state;
            state_controller.current_state_param = state_controller.next_state_param;

            if (state_controller.current_state < STATE_MAX && state_map[state_controller.current_state]) {
                state = state_map[state_controller.current_state];
            } else {
                state = NULL;
            }

            // Call the state entry function
            if (state && state->state_entry) {
                state->state_entry(state, &state_controller, prev_state, state_controller.current_state_param);
            }
        }

        // Call the process function for the state
        if (state && state->state_process) {
            bool result = state->state_process(state, &state_controller);

            // Reset inactivity timeouts as necessary
            if (result) {
                if (state_controller_is_enlarger_focus(&state_controller)) {
                    state_controller_start_focus_timeout(&state_controller);
                }
            }
        }

        // Handle behaviors resulting from inactivity timeouts
        TickType_t max_focus_ticks = pdMS_TO_TICKS(settings_get_enlarger_focus_timeout());
        if (state_controller_is_enlarger_focus(&state_controller) && (xTaskGetTickCount() - state_controller.focus_start_ticks) >= max_focus_ticks) {
            log_i("Focus mode disabled due to timeout");
            meter_probe_handle_t *handle = meter_probe_handle();
            state_controller_set_enlarger_focus(&state_controller, false);
            illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);
            state_controller.focus_start_ticks = 0;
            state_controller_set_enable_meter_probe(&state_controller, false);
            meter_probe_sensor_disable(handle);
            meter_probe_stop(handle);
        }

        // Check if we will do a state transition on the next loop
        if (state_controller.next_state != state_controller.current_state
            || state_controller.next_state_param != state_controller.current_state_param) {
            if (state && state->state_exit) {
                state->state_exit(state, &state_controller, state_controller.next_state);
            }
        }
    }
}

void state_controller_set_next_state(state_controller_t *controller, state_identifier_t next_state, uint32_t param)
{
    if (!controller) { return; }
    controller->next_state = next_state;
    controller->next_state_param = param;
}

exposure_state_t *state_controller_get_exposure_state(state_controller_t *controller)
{
    if (!controller) { return NULL; }
    return controller->exposure_state;
}

const enlarger_config_t *state_controller_get_enlarger_config(state_controller_t *controller)
{
    if (!controller) { return NULL; }
    return &(controller->enlarger_config);
}

void state_controller_set_enlarger_focus(state_controller_t *controller, bool enabled)
{
    if (!controller) { return; }

    if (enabled) {
        enlarger_control_set_state_focus(&(controller->enlarger_config.control), false);
    } else {
        enlarger_control_set_state_off(&(controller->enlarger_config.control), false);
    }

    controller->enlarger_focus_mode = enabled;
}

bool state_controller_is_enlarger_focus(const state_controller_t *controller)
{
    if (!controller) { return false; }
    return controller->enlarger_focus_mode;
}

void state_controller_set_enable_meter_probe(state_controller_t *controller, bool enabled)
{
    if (!controller) { return; }
    // Currently just treating this as a flag
    controller->enable_meter_probe = enabled;
}

bool state_controller_is_enable_meter_probe(const state_controller_t *controller)
{
    if (!controller) { return false; }
    return controller->enable_meter_probe;
}

void state_controller_reload_enlarger_config(state_controller_t *controller)
{
    if (!controller) { return; }

    uint8_t profile_index = settings_get_default_enlarger_config_index();
    bool result = settings_get_enlarger_config(&controller->enlarger_config, profile_index);
    if (!(result && enlarger_config_is_valid(&controller->enlarger_config))) {
        enlarger_config_set_defaults(&controller->enlarger_config);
    }
    exposure_set_min_exposure_time(controller->exposure_state, enlarger_config_min_exposure(&controller->enlarger_config) / 1000.0F);
    exposure_set_channel_wide_mode(controller->exposure_state, controller->enlarger_config.control.dmx_wide_mode);

    uint16_t ch_max = 0;
    if (controller->enlarger_config.control.grade_values[CONTRAST_GRADE_2].channel_red > ch_max) {
        ch_max = controller->enlarger_config.control.grade_values[CONTRAST_GRADE_2].channel_red;
    }
    if (controller->enlarger_config.control.grade_values[CONTRAST_GRADE_2].channel_green > ch_max) {
        ch_max = controller->enlarger_config.control.grade_values[CONTRAST_GRADE_2].channel_green;
    }
    if (controller->enlarger_config.control.grade_values[CONTRAST_GRADE_2].channel_blue > ch_max) {
        ch_max = controller->enlarger_config.control.grade_values[CONTRAST_GRADE_2].channel_blue;
    }
    if (controller->enlarger_config.control.grade_values[CONTRAST_GRADE_2].channel_white > ch_max) {
        ch_max = controller->enlarger_config.control.grade_values[CONTRAST_GRADE_2].channel_white;
    }
    exposure_set_channel_default_value(controller->exposure_state, 0, ch_max);
    exposure_set_channel_default_value(controller->exposure_state, 1, ch_max);
    exposure_set_channel_default_value(controller->exposure_state, 2, ch_max);
}

void state_controller_reload_paper_profile(state_controller_t *controller, bool use_default)
{
    if (!controller) { return; }

    uint8_t profile_index = exposure_get_active_paper_profile_index(controller->exposure_state);

    if (use_default || !exposure_set_active_paper_profile_index(controller->exposure_state, profile_index)) {
        profile_index = settings_get_default_paper_profile_index();
        if (!exposure_set_active_paper_profile_index(controller->exposure_state, profile_index)) {
            exposure_clear_active_paper_profile(controller->exposure_state);
        }
    }
}

void state_controller_start_focus_timeout(state_controller_t *controller)
{
    if (!controller) { return; }
    controller->focus_start_ticks = xTaskGetTickCount();
}

void state_controller_stop_focus_timeout(state_controller_t *controller)
{
    if (!controller) { return; }
    controller->focus_start_ticks = 0;
}

bool state_menu_process(state_t *state_base, state_controller_t *controller)
{
    // Because of how u8g2 menu functions are designed, it is simply easier
    // to just implement the menu system as a tree of simple function calls.
    // As such, the menu system is simply hooked in this way and doesn't
    // really participate in the main state machine.
    main_menu_start(controller);
    state_controller_set_next_state(controller, STATE_HOME, 0);
    return true;
}
