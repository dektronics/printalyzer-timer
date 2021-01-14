#include "state_controller.h"

#include <FreeRTOS.h>
#include <cmsis_os.h>
#include <task.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <esp_log.h>

#include "keypad.h"
#include "display.h"
#include "exposure_state.h"
#include "enlarger_profile.h"
#include "main_menu.h"
#include "buzzer.h"
#include "led.h"
#include "relay.h"
#include "exposure_timer.h"
#include "enlarger_profile.h"
#include "illum_controller.h"
#include "util.h"
#include "settings.h"
#include "state_home.h"
#include "state_timer.h"
#include "state_test_strip.h"
#include "state_adjustment.h"

static const char *TAG = "state_controller";

struct __state_controller_t {
    state_identifier_t current_state;
    uint32_t current_state_param;
    state_identifier_t next_state;
    uint32_t next_state_param;
    exposure_state_t exposure_state;
    enlarger_profile_t enlarger_profile;
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
    state_controller.current_state = STATE_MAX;
    state_controller.current_state_param = 0;
    state_controller.next_state = STATE_HOME;
    state_controller.next_state_param = 0;
    exposure_state_defaults(&(state_controller.exposure_state));
    state_controller.focus_start_ticks = 0;

    uint8_t profile_index = settings_get_default_enlarger_profile_index();
    bool result = settings_get_enlarger_profile(&state_controller.enlarger_profile, profile_index);
    if (!result || !enlarger_profile_is_valid(&state_controller.enlarger_profile)) {
        enlarger_profile_set_defaults(&state_controller.enlarger_profile);
    }

    state_map[STATE_HOME] = state_home();
    state_map[STATE_HOME_CHANGE_TIME_INCREMENT] = state_home_change_time_increment();
    state_map[STATE_HOME_ADJUST_FINE] = state_home_adjust_fine();
    state_map[STATE_HOME_ADJUST_ABSOLUTE] = state_home_adjust_absolute();
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
            ESP_LOGI(TAG, "State transition: %d[%ld] -> %d[%ld]",
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
                if (relay_enlarger_is_enabled()) {
                    state_controller_start_focus_timeout(&state_controller);
                }
            }
        }

        // Handle behaviors resulting from inactivity timeouts
        TickType_t max_focus_ticks = pdMS_TO_TICKS(settings_get_enlarger_focus_timeout());
        if (relay_enlarger_is_enabled() && (xTaskGetTickCount() - state_controller.focus_start_ticks) >= max_focus_ticks) {
            ESP_LOGI(TAG, "Focus mode disabled due to timeout");
            relay_enlarger_enable(false);
            illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);
            state_controller.focus_start_ticks = 0;
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
    return &(controller->exposure_state);
}

enlarger_profile_t *state_controller_get_enlarger_profile(state_controller_t *controller)
{
    if (!controller) { return NULL; }
    return &(controller->enlarger_profile);
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
    main_menu_start();
    state_controller_set_next_state(controller, STATE_HOME, 0);
    return true;
}
