#include "state_test_strip.h"

#include <FreeRTOS.h>
#include <cmsis_os.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <esp_log.h>

#include "display.h"
#include "keypad.h"
#include "led.h"
#include "util.h"
#include "illum_controller.h"
#include "exposure_timer.h"
#include "settings.h"

static const char *TAG = "state_test_strip";

extern exposure_state_t exposure_state;

state_identifier_t state_test_strip()
{
    state_identifier_t next_state = STATE_TEST_STRIP;

    buzzer_volume_t current_volume = buzzer_get_volume();
    pam8904e_freq_t current_frequency = buzzer_get_frequency();
    led_set_on(LED_IND_TEST_STRIP);

    int exposure_patch_min;
    unsigned int exposure_patch_count;

    display_test_strip_elements_t elements = {0};
    elements.title1 = "Test Strip";
    elements.time_elements.time_seconds = 25;
    elements.time_elements.time_milliseconds = 1;
    elements.time_elements.fraction_digits = 1;

    switch (exposure_state.adjustment_increment) {
    case EXPOSURE_ADJ_TWELFTH:
        elements.title2 = "1/24 Stop";
        break;
    case EXPOSURE_ADJ_SIXTH:
        elements.title2 = "1/6 Stop";
        break;
    case EXPOSURE_ADJ_QUARTER:
        elements.title2 = "1/4 Stop";
        break;
    case EXPOSURE_ADJ_THIRD:
        elements.title2 = "1/3 Stop";
        break;
    case EXPOSURE_ADJ_HALF:
        elements.title2 = "1/2 Stop";
        break;
    case EXPOSURE_ADJ_WHOLE:
        elements.title2 = "1 Stop";
        break;
    }

    switch (settings_get_teststrip_patches()) {
    case TESTSTRIP_PATCHES_5:
        exposure_patch_min = -2;
        exposure_patch_count = 5;
        elements.patches = DISPLAY_PATCHES_5;
        break;
    case TESTSTRIP_PATCHES_7:
    default:
        exposure_patch_min = -3;
        exposure_patch_count = 7;
        elements.patches = DISPLAY_PATCHES_7;
        break;
    }

    teststrip_mode_t teststrip_mode = settings_get_teststrip_mode();

    unsigned int patches_covered = 0;
    do {
        float patch_time;
        if (teststrip_mode == TESTSTRIP_MODE_SEPARATE) {
            patch_time = exposure_get_test_strip_time_complete(&exposure_state, exposure_patch_min + patches_covered);

            elements.covered_patches = 0xFF;
            elements.covered_patches ^= (1 << (exposure_patch_count - patches_covered - 1));
        } else {
            patch_time = exposure_get_test_strip_time_incremental(&exposure_state, exposure_patch_min, patches_covered);

            elements.covered_patches = 0;
            for (int i = 0; i < patches_covered; i++) {
                elements.covered_patches |= (1 << (exposure_patch_count - i - 1));
            }
        }
        uint32_t patch_time_ms = rounded_exposure_time_ms(patch_time);

        convert_exposure_to_display_timer(&(elements.time_elements), patch_time_ms);

        display_draw_test_strip_elements(&elements);

        keypad_event_t keypad_event;
        if (keypad_wait_for_event(&keypad_event, -1) == HAL_OK) {
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_START)
                || keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_FOOTSWITCH)) {

                if (patches_covered == 0) {
                    illum_controller_safelight_state(ILLUM_SAFELIGHT_EXPOSURE);
                    osDelay(SAFELIGHT_OFF_DELAY);
                }

                if (state_test_strip_countdown(patch_time_ms, patches_covered == (exposure_patch_count - 1))) {
                    if (patches_covered < exposure_patch_count) {
                        patches_covered++;
                    }
                } else {
                    next_state = STATE_HOME;
                }
            } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                next_state = STATE_HOME;
            }
        }
    } while (next_state == STATE_TEST_STRIP && patches_covered < exposure_patch_count);

    if (next_state == STATE_TEST_STRIP && patches_covered == exposure_patch_count) {
        osDelay(pdMS_TO_TICKS(500));
        next_state = STATE_HOME;
    }

    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

    led_set_off(LED_IND_TEST_STRIP);
    buzzer_set_volume(current_volume);
    buzzer_set_frequency(current_frequency);

    return STATE_HOME;
}

static bool state_test_strip_exposure_callback(exposure_timer_state_t state, uint32_t time_ms, void *user_data)
{
    display_exposure_timer_t *elements = user_data;

    update_display_timer(elements, time_ms);
    display_draw_test_strip_timer(elements);

    // Handle the next keypad event without blocking
    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, 0) == HAL_OK) {
        if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
            ESP_LOGI(TAG, "Canceling test strip timer at %ldms", time_ms);
            return false;
        }
    }

    return true;
}

bool state_test_strip_countdown(uint32_t patch_time_ms, bool last_patch)
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

    const enlarger_profile_t *enlarger_profile = settings_get_default_enlarger_profile();
    exposure_timer_set_config_time(&timer_config, patch_time_ms, enlarger_profile);

    exposure_timer_set_config(&timer_config);

    display_draw_test_strip_timer(&elements);

    HAL_StatusTypeDef ret = exposure_timer_run();
    if (ret == HAL_TIMEOUT) {
        ESP_LOGE(TAG, "Exposure timer canceled");
    } else if (ret != HAL_OK) {
        ESP_LOGE(TAG, "Exposure timer error");
    }

    ESP_LOGI(TAG, "Exposure timer complete");

    return ret == HAL_OK;
}
