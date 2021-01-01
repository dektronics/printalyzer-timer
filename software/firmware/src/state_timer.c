#include "state_timer.h"

#include <FreeRTOS.h>
#include <cmsis_os.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
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

state_t *state_timer()
{
    return (state_t *)&state_timer_data;
}

static bool state_timer_exposure_callback(exposure_timer_state_t state, uint32_t time_ms, void *user_data)
{
    display_exposure_timer_t *elements = user_data;
    display_exposure_timer_t prev_elements;

    memcpy(&prev_elements, elements, sizeof(display_exposure_timer_t));

    update_display_timer(elements, time_ms);
    display_draw_exposure_timer(elements, &prev_elements);

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

bool state_timer_process(state_t *state_base, state_controller_t *controller)
{
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);
    uint32_t exposure_time_ms = rounded_exposure_time_ms(exposure_state->adjusted_time);

    display_exposure_timer_t elements;
    convert_exposure_to_display_timer(&elements, exposure_time_ms);

    exposure_timer_config_t timer_config = {0};
    timer_config.end_tone = EXPOSURE_TIMER_END_TONE_REGULAR;
    timer_config.timer_callback = state_timer_exposure_callback;
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

    const enlarger_profile_t *enlarger_profile = settings_get_default_enlarger_profile();
    exposure_timer_set_config_time(&timer_config, exposure_time_ms, enlarger_profile);

    exposure_timer_set_config(&timer_config);

    ESP_LOGI(TAG, "Starting exposure timer for %ldms", exposure_time_ms);

    display_draw_exposure_timer(&elements, 0);

    illum_controller_safelight_state(ILLUM_SAFELIGHT_EXPOSURE);
    osDelay(SAFELIGHT_OFF_DELAY);

    HAL_StatusTypeDef ret = exposure_timer_run();
    if (ret == HAL_TIMEOUT) {
        ESP_LOGE(TAG, "Exposure timer canceled");
    } else if (ret != HAL_OK) {
        ESP_LOGE(TAG, "Exposure timer error");
    }

    ESP_LOGI(TAG, "Exposure timer complete");

    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

    state_controller_set_next_state(controller, STATE_HOME, 0);
    return true;
}
