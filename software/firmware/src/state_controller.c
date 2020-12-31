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

static const char *TAG = "state_controller";

/* State data used by the home screen and related screens */
exposure_state_t exposure_state;

TickType_t focus_start_ticks;

static state_identifier_t state_add_adjustment();
static state_identifier_t state_menu();

void state_controller_init()
{
    exposure_state_defaults(&exposure_state);
    focus_start_ticks = 0;
}

void state_controller_loop()
{
    // State data initialization
    bool state_changed = true;
    state_home_data_t home_data;
    state_home_init(&home_data);

    state_identifier_t current_state = STATE_HOME;
    state_identifier_t next_state = current_state;
    for (;;) {
        switch (current_state) {
        case STATE_HOME:
            next_state = state_home(&home_data);
            break;
        case STATE_HOME_CHANGE_TIME_INCREMENT:
            next_state = state_home_change_time_increment();
            break;
        case STATE_HOME_ADJUST_FINE:
            next_state = state_home_adjust_fine(state_changed);
            break;
        case STATE_HOME_ADJUST_ABSOLUTE:
            next_state = state_home_adjust_absolute(state_changed);
            break;
        case STATE_TIMER:
            next_state = state_timer();
            break;
        case STATE_TEST_STRIP:
            next_state = state_test_strip();
            break;
        case STATE_ADD_ADJUSTMENT:
            next_state = state_add_adjustment();
            break;
        case STATE_MENU:
            next_state = state_menu();
            break;
        default:
            break;
        }

        TickType_t max_focus_ticks = pdMS_TO_TICKS(settings_get_enlarger_focus_timeout());
        if (relay_enlarger_is_enabled() && (xTaskGetTickCount() - focus_start_ticks) >= max_focus_ticks) {
            ESP_LOGI(TAG, "Focus mode disabled due to timeout");
            relay_enlarger_enable(false);
            illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);
            focus_start_ticks = 0;
        }

        if (next_state != current_state) {
            ESP_LOGI(TAG, "State transition: %d -> %d", current_state, next_state);
            state_changed = true;
            current_state = next_state;
        } else {
            state_changed = false;
        }
    }
}

state_identifier_t state_add_adjustment()
{
    //TODO

    return STATE_HOME;
}

state_identifier_t state_menu()
{
    main_menu_start();
    return STATE_HOME;
}
