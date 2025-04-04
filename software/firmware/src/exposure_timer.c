#include <FreeRTOS.h>
#include <cmsis_os.h>

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define LOG_TAG "exposure_timer"
#include <elog.h>

#include "exposure_timer.h"
#include "illum_controller.h"
#include "enlarger_config.h"
#include "enlarger_control.h"
#include "buzzer.h"
#include "dmx.h"
#include "settings.h"
#include "util.h"

static TIM_HandleTypeDef *timer_htim = 0;
static exposure_timer_config_t timer_config = {0};
static enlarger_control_t enlarger_control = {0};

static TaskHandle_t timer_task_handle = 0;
static bool enlarger_activated = false;
static bool enlarger_deactivated = false;
static bool enlarger_deactivate_pending = false;
static bool timer_notify_end = false;
static bool timer_cancel_request = false;
static exposure_timer_state_t timer_state = EXPOSURE_TIMER_STATE_NONE;
static uint32_t time_elapsed = 0;
static uint32_t buzz_start = 0;
static uint32_t buzz_stop = 0;
static uint32_t enlarger_on_event_ticks = 0;
static uint32_t enlarger_off_event_ticks = 0;

void exposure_timer_init(TIM_HandleTypeDef *htim)
{
    timer_htim = htim;
    memset(&timer_config, 0, sizeof(exposure_timer_config_t));
    timer_config.callback_rate = EXPOSURE_TIMER_RATE_1_SEC;
}

void exposure_timer_set_config_time(exposure_timer_config_t *config,
    uint32_t exposure_time, const enlarger_config_t *enlarger_config)
{
    if (!config) {
        return;
    }
    if (!enlarger_config || !enlarger_config_is_valid(enlarger_config)) {
        log_i("Setting defaults based on missing or invalid enlarger config");
        config->exposure_time = exposure_time;
        config->enlarger_on_delay = 0;
        config->enlarger_off_delay = 0;
        return;
    }

    /*
     * Log a warning if the exposure is too short.
     * Not yet sure what should be done in this case, but any user alerts
     * should probably happen before we even get to this code.
     */
    uint32_t min_exposure_time = enlarger_config_min_exposure(enlarger_config);
    log_d("Minimum allowable exposure time: %ldms", min_exposure_time);
    if (exposure_time < round_to_10(min_exposure_time)) {
        log_e("Cannot accurately time short exposure: %ldms < %ldms",
            exposure_time, min_exposure_time);
    }

    /* Assign the time fields based on the enlarger profile */
    config->exposure_time = exposure_time;
    config->enlarger_on_delay = round_to_10(enlarger_config->timing.turn_on_delay + (enlarger_config->timing.rise_time - enlarger_config->timing.rise_time_equiv));
    config->enlarger_off_delay = round_to_10(enlarger_config->timing.turn_off_delay + enlarger_config->timing.fall_time_equiv);
    config->exposure_end_delay = round_to_10(enlarger_config->timing.fall_time - enlarger_config->timing.fall_time_equiv);

    /* Log all the relevant time properties */
    log_d("Set for desired time of %ldms", exposure_time);
    log_d("Adjusted exposure time: %ldms", config->enlarger_on_delay + config->exposure_time);
    log_d("On delay: %dms", config->enlarger_on_delay);
    log_d("Off delay: %dms", config->enlarger_off_delay);
    log_d("End delay: %dms", config->exposure_end_delay);
}

void exposure_timer_set_config(const exposure_timer_config_t *config, const enlarger_control_t *control)
{
    if (!config) {
        memset(&timer_config, 0, sizeof(exposure_timer_config_t));
    } else {
        memcpy(&timer_config, config, sizeof(exposure_timer_config_t));
    }

    if (!control) {
        memset(&enlarger_control, 0, sizeof(enlarger_control_t));
    } else {
        memcpy(&enlarger_control, control, sizeof(enlarger_control_t));
    }
}

HAL_StatusTypeDef exposure_timer_run()
{
    if (!timer_htim || timer_config.exposure_time == 0) {
        log_e("Exposure timer not configured");
        return HAL_ERROR;
    }

    if (timer_config.exposure_time > 0x100000UL) {
        log_e("Exposure time too long: %ld > %ld", timer_config.exposure_time, 0x100000UL);
        return HAL_ERROR;
    }
    if (timer_config.enlarger_off_delay >= timer_config.exposure_time) {
        log_e("Enlarger off delay cannot be longer than the exposure time: %d >= %ld",
            timer_config.enlarger_off_delay, timer_config.exposure_time);
        return HAL_ERROR;
    }

    timer_task_handle = xTaskGetCurrentTaskHandle();
    enlarger_activated = false;
    enlarger_deactivated = false;
    enlarger_deactivate_pending = false;
    timer_notify_end = false;
    timer_cancel_request = false;
    timer_state = EXPOSURE_TIMER_STATE_NONE;
    time_elapsed = 0;
    enlarger_on_event_ticks = 0;
    enlarger_off_event_ticks = 0;

    buzzer_volume_t current_volume = buzzer_get_volume();
    uint16_t current_frequency = buzzer_get_frequency();
    buzzer_set_volume(settings_get_buzzer_volume());

    if (timer_config.start_tone == EXPOSURE_TIMER_START_TONE_COUNTDOWN) {
        do {
            buzzer_set_frequency(2000);
            buzzer_start();
            osDelay(pdMS_TO_TICKS(50));
            buzzer_stop();
            osDelay(pdMS_TO_TICKS(950));
            if (!timer_config.timer_callback(EXPOSURE_TIMER_STATE_NONE, UINT32_MAX, timer_config.user_data)) {
                timer_cancel_request = true;
                break;
            }

            buzzer_set_frequency(1500);
            buzzer_start();
            osDelay(pdMS_TO_TICKS(50));
            buzzer_stop();
            osDelay(pdMS_TO_TICKS(950));
            if (!timer_config.timer_callback(EXPOSURE_TIMER_STATE_NONE, UINT32_MAX, timer_config.user_data)) {
                timer_cancel_request = true;
                break;
            }

            buzzer_set_frequency(500);
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
        buzzer_set_frequency(500);

        illum_controller_safelight_state(ILLUM_SAFELIGHT_EXPOSURE);
        osDelay(SAFELIGHT_OFF_DELAY);

        log_i("Starting exposure timer");

        if (enlarger_control.dmx_control) {
            dmx_pause();
            dmx_enable_direct_frame_update();
        }

        HAL_TIM_Base_Start_IT(timer_htim);

        uint32_t ulNotifiedValue = 0;
        for (;;) {
            xTaskNotifyWait(0, UINT32_MAX, &ulNotifiedValue, portMAX_DELAY);
            uint8_t state = (uint8_t)((ulNotifiedValue & 0xFF000000) >> 24);
            uint32_t timer = ulNotifiedValue & 0x00FFFFFF;

            if (timer_config.timer_callback) {
                if (!timer_config.timer_callback(state, timer, timer_config.user_data)) {
                    log_i("Timer cancel requested");
                    taskENTER_CRITICAL();
                    timer_cancel_request = true;
                    taskEXIT_CRITICAL();
                }
            }

            if (state == EXPOSURE_TIMER_STATE_START) {
                log_i("Exposure timer started");
            } else if (state == EXPOSURE_TIMER_STATE_END) {
                log_i("Exposure timer ended");
            } else if (state == EXPOSURE_TIMER_STATE_DONE) {
                log_i("Exposure timer process complete");
                break;
            }
        }

        if (enlarger_control.dmx_control) {
            osDelay(30);
            dmx_start();
        }

        log_i("Exposure timer complete");

        illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

        log_d("Actual enlarger on/off time: %lums",
            (enlarger_off_event_ticks - enlarger_on_event_ticks) / portTICK_RATE_MS);

        /* Handling the completion beep outside the ISR for simplicity. */
        if (timer_cancel_request) {
            buzzer_set_frequency(1000);
            buzzer_start();
            osDelay(pdMS_TO_TICKS(100));
            buzzer_stop();
            osDelay(pdMS_TO_TICKS(100));
            buzzer_start();
            osDelay(pdMS_TO_TICKS(100));
            buzzer_stop();
        } else {
            if (timer_config.end_tone == EXPOSURE_TIMER_END_TONE_SHORT) {
                buzzer_set_frequency(1000);
                buzzer_start();
                osDelay(pdMS_TO_TICKS(50));
                buzzer_set_frequency(2000);
                osDelay(pdMS_TO_TICKS(50));
                buzzer_set_frequency(1500);
                osDelay(pdMS_TO_TICKS(50));
                buzzer_stop();
            } else if (timer_config.end_tone == EXPOSURE_TIMER_END_TONE_REGULAR) {
                buzzer_set_frequency(1000);
                buzzer_start();
                osDelay(pdMS_TO_TICKS(120));
                buzzer_set_frequency(2000);
                osDelay(pdMS_TO_TICKS(120));
                buzzer_set_frequency(1500);
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

    /*
     * If we are in the DONE state, then make sure this is the last
     * time we enter this function
     */
    if (timer_state == EXPOSURE_TIMER_STATE_DONE) {
        HAL_TIM_Base_Stop_IT(timer_htim);
    }

    if (!enlarger_activated) {
        enlarger_on_event_ticks = osKernelGetTickCount();
        enlarger_control_set_state(&enlarger_control,
            ENLARGER_CONTROL_STATE_EXPOSURE, timer_config.contrast_grade,
            timer_config.channel_red, timer_config.channel_green, timer_config.channel_blue,
            false);
        if (enlarger_control.dmx_control) {
            dmx_send_frame_explicit();
        }
        enlarger_activated = true;
    } else {
        time_elapsed += 10;

        if (!enlarger_deactivated && !enlarger_deactivate_pending
            && ((time_elapsed >= timer_config.enlarger_on_delay + (timer_config.exposure_time - timer_config.enlarger_off_delay)) || cancel_flag)) {
            if (enlarger_control.dmx_control) {
                enlarger_deactivate_pending = true;
            } else {
                enlarger_control_set_state_off(&enlarger_control, false);
                enlarger_off_event_ticks = osKernelGetTickCount();
                enlarger_deactivated = true;
            }
        }

        if (enlarger_control.dmx_control && (time_elapsed % 30) == 0) {
            if (enlarger_deactivate_pending) {
                enlarger_control_set_state_off(&enlarger_control, false);
            }
            dmx_send_frame_explicit();
            if (enlarger_deactivate_pending) {
                enlarger_off_event_ticks = osKernelGetTickCount();
                enlarger_deactivate_pending = false;
                enlarger_deactivated = true;
            }
        }
    }

    if (timer_state == EXPOSURE_TIMER_STATE_NONE) {
        if (time_elapsed >= timer_config.enlarger_on_delay) {
            /* After the enlarger on delay, mark the visible timer as active */
            timer_state = EXPOSURE_TIMER_STATE_START;

            /* Set the time of the first beep to be second-aligned */
            if ((timer_config.exposure_time % 1000) == 0) {
                buzz_start = time_elapsed + 1000;
                buzz_stop = 0;
            } else {
                buzz_start = time_elapsed + (timer_config.exposure_time % 1000);
                buzz_stop = 0;
            }
        } else {
            /* Don't do any further processing until the visible timer is active */
            return;
        }
    }

    /* If we are at the end of the exposure time, transition to the end state */
    if ((timer_state == EXPOSURE_TIMER_STATE_START || timer_state == EXPOSURE_TIMER_STATE_TICK)
        && ((time_elapsed >= timer_config.exposure_time + timer_config.enlarger_on_delay) || cancel_flag)) {
        timer_state = EXPOSURE_TIMER_STATE_END;
    }

    /* Manage starting and stopping of the buzzer */
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
        notify_timer = timer_config.exposure_time - (time_elapsed - timer_config.enlarger_on_delay);
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
    } else if (timer_state == EXPOSURE_TIMER_STATE_END && buzz_stop == 0 && !enlarger_deactivate_pending
        && (time_elapsed > timer_config.enlarger_on_delay + timer_config.exposure_time + timer_config.exposure_end_delay || cancel_flag)) {
        timer_state = EXPOSURE_TIMER_STATE_DONE;
    }
}
