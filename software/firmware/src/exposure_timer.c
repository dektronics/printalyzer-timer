#include <FreeRTOS.h>
#include <cmsis_os.h>

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <esp_log.h>

#include "exposure_timer.h"
#include "illum_controller.h"
#include "buzzer.h"
#include "relay.h"
#include "settings.h"
#include "util.h"

static const char *TAG = "exposure_timer";

static TIM_HandleTypeDef *timer_htim = 0;
static exposure_timer_config_t timer_config = {0};

static TaskHandle_t timer_task_handle = 0;
static bool relay_activated = false;
static bool relay_deactivated = false;
static bool timer_notify_end = false;
static bool timer_cancel_request = false;
static exposure_timer_state_t timer_state = EXPOSURE_TIMER_STATE_NONE;
static uint32_t time_elapsed = 0;
static uint32_t buzz_start = 0;
static uint32_t buzz_stop = 0;
static uint32_t relay_on_event_ticks = 0;
static uint32_t relay_off_event_ticks = 0;

void exposure_timer_init(TIM_HandleTypeDef *htim)
{
    timer_htim = htim;
    memset(&timer_config, 0, sizeof(exposure_timer_config_t));
    timer_config.callback_rate = EXPOSURE_TIMER_RATE_1_SEC;
}

void exposure_timer_set_config_time(exposure_timer_config_t *config,
    uint32_t exposure_time, const enlarger_profile_t *profile)
{
    if (!config) {
        return;
    }
    if (!profile || !enlarger_profile_is_valid(profile)) {
        ESP_LOGI(TAG, "Setting defaults based on missing or invalid enlarger profile");
        config->exposure_time = exposure_time;
        config->relay_on_delay = 0;
        config->relay_off_delay = 0;
        return;
    }

    // Log a warning if the exposure is too short.
    // Not yet sure what should be done in this case, but any user alerts
    // should probably happen before we even get to this code.
    uint32_t min_exposure_time = enlarger_profile_min_exposure(profile);
    ESP_LOGD(TAG, "Minimum allowable exposure time: %ldms", min_exposure_time);
    if (exposure_time < round_to_10(min_exposure_time)) {
        ESP_LOGE(TAG, "Cannot accurately time short exposure: %ldms < %ldms",
            exposure_time, min_exposure_time);
    }

    // Assign the time fields based on the enlarger profile
    config->exposure_time = exposure_time;
    config->relay_on_delay = round_to_10(profile->turn_on_delay + (profile->rise_time - profile->rise_time_equiv));
    config->relay_off_delay = round_to_10(profile->turn_off_delay + profile->fall_time_equiv);
    config->exposure_end_delay = round_to_10(profile->fall_time - profile->fall_time_equiv);

    // Log all the relevant time properties
    ESP_LOGD(TAG, "Set for desired time of %ldms", exposure_time);
    ESP_LOGD(TAG, "Adjusted exposure time: %ldms", config->relay_on_delay + config->exposure_time);
    ESP_LOGD(TAG, "On delay: %dms", config->relay_on_delay);
    ESP_LOGD(TAG, "Off delay: %dms", config->relay_off_delay);
    ESP_LOGD(TAG, "End delay: %dms", config->exposure_end_delay);
}

void exposure_timer_set_config(const exposure_timer_config_t *config)
{
    if (!config) {
        memset(&timer_config, 0, sizeof(exposure_timer_config_t));
    } else {
        memcpy(&timer_config, config, sizeof(exposure_timer_config_t));
    }
}

HAL_StatusTypeDef exposure_timer_run()
{
    if (!timer_htim || timer_config.exposure_time == 0) {
        ESP_LOGE(TAG, "Exposure timer not configured");
        return HAL_ERROR;
    }

    if (timer_config.exposure_time > 0x100000UL) {
        ESP_LOGE(TAG, "Exposure time too long: %ld > %ld", timer_config.exposure_time, 0x100000UL);
        return HAL_ERROR;
    }
    if (timer_config.relay_off_delay >= timer_config.exposure_time) {
        ESP_LOGE(TAG, "Relay off delay cannot be longer than the exposure time: %d >= %ld",
            timer_config.relay_off_delay, timer_config.exposure_time);
        return HAL_ERROR;
    }

    timer_task_handle = xTaskGetCurrentTaskHandle();
    relay_activated = false;
    relay_deactivated = false;
    timer_notify_end = false;
    timer_cancel_request = false;
    timer_state = EXPOSURE_TIMER_STATE_NONE;
    time_elapsed = 0;
    relay_on_event_ticks = 0;
    relay_off_event_ticks = 0;

    buzzer_volume_t current_volume = buzzer_get_volume();
    pam8904e_freq_t current_frequency = buzzer_get_frequency();
    buzzer_set_volume(settings_get_buzzer_volume());

    if (timer_config.start_tone == EXPOSURE_TIMER_START_TONE_COUNTDOWN) {
        do {
            buzzer_set_frequency(PAM8904E_FREQ_2000HZ);
            buzzer_start();
            osDelay(pdMS_TO_TICKS(50));
            buzzer_stop();
            osDelay(pdMS_TO_TICKS(950));
            if (!timer_config.timer_callback(EXPOSURE_TIMER_STATE_NONE, UINT32_MAX, timer_config.user_data)) {
                timer_cancel_request = true;
                break;
            }

            buzzer_set_frequency(PAM8904E_FREQ_1500HZ);
            buzzer_start();
            osDelay(pdMS_TO_TICKS(50));
            buzzer_stop();
            osDelay(pdMS_TO_TICKS(950));
            if (!timer_config.timer_callback(EXPOSURE_TIMER_STATE_NONE, UINT32_MAX, timer_config.user_data)) {
                timer_cancel_request = true;
                break;
            }

            buzzer_set_frequency(PAM8904E_FREQ_500HZ);
            buzzer_start();
            osDelay(pdMS_TO_TICKS(50));
            buzzer_stop();
            osDelay(pdMS_TO_TICKS(950));
            if (!timer_config.timer_callback(EXPOSURE_TIMER_STATE_NONE, UINT32_MAX, timer_config.user_data)) {
                timer_cancel_request = true;
                break;
            }
        } while (0);
    }

    if (!timer_cancel_request) {
        buzzer_set_frequency(PAM8904E_FREQ_500HZ);

        illum_controller_safelight_state(ILLUM_SAFELIGHT_EXPOSURE);
        osDelay(SAFELIGHT_OFF_DELAY);

        ESP_LOGI(TAG, "Starting exposure timer");

        HAL_TIM_Base_Start_IT(timer_htim);

        uint32_t ulNotifiedValue = 0;
        for (;;) {
            xTaskNotifyWait(0, UINT32_MAX, &ulNotifiedValue, portMAX_DELAY);
            uint8_t state = (uint8_t)((ulNotifiedValue & 0xFF000000) >> 24);
            uint32_t timer = ulNotifiedValue & 0x00FFFFFF;

            if (timer_config.timer_callback) {
                if (!timer_config.timer_callback(state, timer, timer_config.user_data)) {
                    ESP_LOGI(TAG, "Timer cancel requested");
                    taskENTER_CRITICAL();
                    timer_cancel_request = true;
                    taskEXIT_CRITICAL();
                }
            }

            if (state == EXPOSURE_TIMER_STATE_START) {
                ESP_LOGI(TAG, "Exposure timer started");
            } else if (state == EXPOSURE_TIMER_STATE_END) {
                ESP_LOGI(TAG, "Exposure timer ended");
            } else if (state == EXPOSURE_TIMER_STATE_DONE) {
                ESP_LOGI(TAG, "Exposure timer process complete");
                break;
            }
        }

        ESP_LOGI(TAG, "Exposure timer complete");

        illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

        ESP_LOGD(TAG, "Actual relay on/off time: %lums",
            (relay_off_event_ticks - relay_on_event_ticks) / portTICK_RATE_MS);

        // Handling the completion beep outside the ISR for simplicity.
        if (timer_cancel_request) {
            buzzer_set_frequency(PAM8904E_FREQ_1000HZ);
            buzzer_start();
            osDelay(pdMS_TO_TICKS(100));
            buzzer_stop();
            osDelay(pdMS_TO_TICKS(100));
            buzzer_start();
            osDelay(pdMS_TO_TICKS(100));
            buzzer_stop();
        } else {
            if (timer_config.end_tone == EXPOSURE_TIMER_END_TONE_SHORT) {
                buzzer_set_frequency(PAM8904E_FREQ_1000HZ);
                buzzer_start();
                osDelay(pdMS_TO_TICKS(50));
                buzzer_set_frequency(PAM8904E_FREQ_2000HZ);
                osDelay(pdMS_TO_TICKS(50));
                buzzer_set_frequency(PAM8904E_FREQ_1500HZ);
                osDelay(pdMS_TO_TICKS(50));
                buzzer_stop();
            } else if (timer_config.end_tone == EXPOSURE_TIMER_END_TONE_REGULAR) {
                buzzer_set_frequency(PAM8904E_FREQ_1000HZ);
                buzzer_start();
                osDelay(pdMS_TO_TICKS(120));
                buzzer_set_frequency(PAM8904E_FREQ_2000HZ);
                osDelay(pdMS_TO_TICKS(120));
                buzzer_set_frequency(PAM8904E_FREQ_1500HZ);
                osDelay(pdMS_TO_TICKS(120));
                buzzer_stop();
            }
        }
        osDelay(pdMS_TO_TICKS(500));
    }
    buzzer_set_volume(current_volume);
    buzzer_set_frequency(current_frequency);

    return timer_cancel_request ? HAL_TIMEOUT : HAL_OK;
}

void exposure_timer_notify()
{
    if (!timer_htim || timer_config.exposure_time == 0) { return; }

    bool cancel_flag = timer_cancel_request;

    // If we are in the DONE state, then make sure this is the last
    // time we enter this function
    if (timer_state == EXPOSURE_TIMER_STATE_DONE) {
        HAL_TIM_Base_Stop_IT(timer_htim);
    }

    if (!relay_activated) {
        relay_on_event_ticks = osKernelGetTickCount();
        relay_enlarger_enable(true);
        relay_activated = true;
    } else {
        time_elapsed += 10;
    }

    if (timer_state == EXPOSURE_TIMER_STATE_NONE) {
        if (time_elapsed >= timer_config.relay_on_delay) {
            // After the relay on delay, mark the visible timer as active
            timer_state = EXPOSURE_TIMER_STATE_START;

            // Set the time of the first beep to be second-aligned
            if ((timer_config.exposure_time % 1000) == 0) {
                buzz_start = time_elapsed + 1000;
                buzz_stop = 0;
            } else {
                buzz_start = time_elapsed + (timer_config.exposure_time % 1000);
                buzz_stop = 0;
            }
        } else {
            // Don't do any further processing until the visible timer is active
            return;
        }
    }

    if (!relay_deactivated
        && ((time_elapsed >= timer_config.relay_on_delay + (timer_config.exposure_time - timer_config.relay_off_delay)) || cancel_flag)) {
        relay_off_event_ticks = osKernelGetTickCount();
        relay_enlarger_enable(false);
        relay_deactivated = true;
    }

    // If we are at the end of the exposure time, transition to the end state
    if ((timer_state == EXPOSURE_TIMER_STATE_START || timer_state == EXPOSURE_TIMER_STATE_TICK)
        && ((time_elapsed >= timer_config.exposure_time + timer_config.relay_on_delay) || cancel_flag)) {
        timer_state = EXPOSURE_TIMER_STATE_END;
    }

    // Manage starting and stopping of the buzzer
    if (buzz_start > 0 && time_elapsed >= buzz_start) {
        buzzer_start();
        buzz_stop = buzz_start + 40;
        if (timer_state == EXPOSURE_TIMER_STATE_START || timer_state == EXPOSURE_TIMER_STATE_TICK) {
            buzz_start += 1000;
        } else {
            buzz_start = 0;
        }
    } else if (buzz_stop > 0 && time_elapsed >= buzz_stop) {
        buzzer_stop();
        buzz_stop = 0;
    }

    uint8_t notify_state = timer_state;
    uint32_t notify_timer;
    switch (timer_state) {
    case EXPOSURE_TIMER_STATE_NONE:
    case EXPOSURE_TIMER_STATE_START:
        notify_timer = timer_config.exposure_time;
        break;
    case EXPOSURE_TIMER_STATE_TICK:
        notify_timer = timer_config.exposure_time - (time_elapsed - timer_config.relay_on_delay);
        break;
    case EXPOSURE_TIMER_STATE_END:
    case EXPOSURE_TIMER_STATE_DONE:
    default:
        notify_timer = 0;
        break;
    }

    bool should_notify;
    if (timer_state == EXPOSURE_TIMER_STATE_TICK) {
        switch (timer_config.callback_rate) {
        case EXPOSURE_TIMER_RATE_10_MS:
            should_notify = true;
            break;
        case EXPOSURE_TIMER_RATE_100_MS:
            should_notify = ((notify_timer % 100) == 0);
            break;
        case EXPOSURE_TIMER_RATE_1_SEC:
        default:
            should_notify = ((notify_timer % 1000) == 0);
        }
    } else if (timer_state == EXPOSURE_TIMER_STATE_END) {
        if (!timer_notify_end) {
            should_notify = true;
            timer_notify_end = true;
        } else {
            should_notify = false;
        }
    } else {
        should_notify = true;
    }

    if (should_notify) {
        uint32_t notify_value = ((uint32_t)notify_state << 24) | (notify_timer & 0x00FFFFFF);
        xTaskNotifyFromISR(timer_task_handle, notify_value, eSetValueWithOverwrite, NULL);
    }

    if (timer_state == EXPOSURE_TIMER_STATE_START) {
        timer_state = EXPOSURE_TIMER_STATE_TICK;
    } else if (timer_state == EXPOSURE_TIMER_STATE_END && buzz_stop == 0
        && (time_elapsed > timer_config.relay_on_delay + timer_config.exposure_time + timer_config.exposure_end_delay || cancel_flag)) {
        timer_state = EXPOSURE_TIMER_STATE_DONE;
    }
}
