#include "menu_enlarger.h"

#include <FreeRTOS.h>
#include <task.h>
#include <cmsis_os.h>

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include <esp_log.h>

#include "display.h"
#include "keypad.h"
#include "relay.h"
#include "tcs3472.h"
#include "enlarger_profile.h"
#include "illum_controller.h"

#define REFERENCE_READING_ITERATIONS 100
#define PROFILE_ITERATIONS 5
#define MAX_LOOP_DURATION pdMS_TO_TICKS(10000)

static const char *TAG = "menu_enlarger";

/*
 * Right now this is mostly scratch/placeholder code to work out the
 * sequence and data collection of the enlarger calibration process.
 * Eventually it should be replaced with an actual profile manager
 * and a wizard for creating/updating profiles.
 *
 * The sensor polling loop involves a channel read, followed by a 5ms delay.
 * The sensor itself is configured for an integration time of 4.8ms,
 * and the channel read operation has been measured to take 0.14ms.
 * There does not appear to be a way to synchronize ourselves to the exact
 * integration state of the sensor, so this approach is hopefully sufficient
 * for the data we are trying to collect.
 */

extern I2C_HandleTypeDef hi2c2;

typedef struct {
    float mean;
    uint16_t min;
    uint16_t max;
    float stddev;
} reading_stats_t;

static void enlarger_calibration_start();
static bool calibration_initialize_sensor();
static bool calibration_collect_reference_stats(reading_stats_t *stats_on, reading_stats_t *stats_off);
static bool calibration_validate_reference_stats(reading_stats_t *stats_on, reading_stats_t *stats_off);
static bool calibration_build_profile(enlarger_profile_t *profile, const reading_stats_t *stats_on, const reading_stats_t *stats_off);

static void calculate_reading_stats(reading_stats_t *stats, uint16_t *readings, size_t len);
static bool delay_with_cancel(uint32_t time_ms);

menu_result_t menu_enlarger_calibration()
{
    menu_result_t menu_result = MENU_OK;

    illum_controller_safelight_state(ILLUM_SAFELIGHT_EXPOSURE);

    uint8_t option = display_message(
            "Enlarger Calibration",
            NULL,
            "\nMake sure your safelight is off,\n"
            "the meter probe is connected,\n"
            "and your enlarger is ready.",
            " Start ");

    if (option == 1) {
        enlarger_calibration_start();
        relay_enlarger_enable(false);
    } else if (option == UINT8_MAX) {
        menu_result = MENU_TIMEOUT;
    }

    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

    return menu_result;
}

void enlarger_calibration_start()
{
    ESP_LOGI(TAG, "Starting enlarger calibration process");

    display_static_list("Enlarger Calibration", "\n\nInitializing...");

    // Turn everything off, just in case it isn't already off
    relay_safelight_enable(false);
    relay_enlarger_enable(false);

    if (!calibration_initialize_sensor()) {
        ESP_LOGE(TAG, "Could not initialize sensor");
        return;
    }

    osDelay(pdMS_TO_TICKS(1000));

    display_static_list("Enlarger Calibration", "\n\nCollecting Reference Points");

    reading_stats_t enlarger_on_stats;
    reading_stats_t enlarger_off_stats;
    if (!calibration_collect_reference_stats(&enlarger_on_stats, &enlarger_off_stats)) {
        ESP_LOGE(TAG, "Could not collect reference stats");
        tcs3472_disable(&hi2c2);
        if (relay_enlarger_is_enabled()) {
            relay_enlarger_enable(false);
        }
        return;
    }

    // Log statistics used for reference points
    ESP_LOGI(TAG, "Enlarger on");
    ESP_LOGI(TAG, "Mean = %.1f, Min = %d, Max = %d, StdDev = %.1f",
        enlarger_on_stats.mean, enlarger_on_stats.min, enlarger_on_stats.max,
        enlarger_on_stats.stddev);
    ESP_LOGI(TAG, "Enlarger off");
    ESP_LOGI(TAG, "Mean = %.1f, Min = %d, Max = %d, StdDev = %.1f",
        enlarger_off_stats.mean, enlarger_off_stats.min, enlarger_off_stats.max,
        enlarger_off_stats.stddev);

    if (!calibration_validate_reference_stats(&enlarger_on_stats, &enlarger_off_stats)) {
        ESP_LOGW(TAG, "Reference stats are not usable for calibration");
        tcs3472_disable(&hi2c2);
        illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

        do {
            uint8_t option = display_message(
                "Calibration Failed",
                NULL,
                "\n"
                "Enlarger may not be connected\n"
                "or is not bright enough for\n"
                "calibration.",
                " OK ");
            if (option == 1) { break; }
        } while (1);
        return;
    }

    if (!delay_with_cancel(1000)) {
        tcs3472_disable(&hi2c2);
        return;
    }

    // Do the profiling process
    display_static_list("Enlarger Calibration", "\n\nProfiling enlarger...");

    enlarger_profile_t profile;
    enlarger_profile_t profile_sum;
    enlarger_profile_t profile_inc[PROFILE_ITERATIONS];

    memset(&profile, 0, sizeof(enlarger_profile_t));
    memset(&profile_sum, 0, sizeof(enlarger_profile_t));

    for (int i = 0; i < PROFILE_ITERATIONS; i++) {
        ESP_LOGI(TAG, "Profile run %d...", i + 1);

        if (!calibration_build_profile(&profile_inc[i], &enlarger_on_stats, &enlarger_off_stats)) {
            ESP_LOGE(TAG, "Could not build profile");
            tcs3472_disable(&hi2c2);
            illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

            if (relay_enlarger_is_enabled()) {
                relay_enlarger_enable(false);
            }

            do {
                uint8_t option = display_message(
                    "Profiling Failed",
                    NULL,
                    "\n"
                    "Either the enlarger or the meter\n"
                    "probe may not be working\n"
                    "correctly.",
                    " OK ");
                if (option == 1) { break; }
            } while (1);
            return;
        }

        profile_sum.turn_on_delay += profile_inc[i].turn_on_delay;
        profile_sum.rise_time += profile_inc[i].rise_time;
        profile_sum.rise_time_equiv += profile_inc[i].rise_time_equiv;
        profile_sum.turn_off_delay += profile_inc[i].turn_off_delay;
        profile_sum.fall_time += profile_inc[i].fall_time;
        profile_sum.fall_time_equiv += profile_inc[i].fall_time_equiv;
    }
    ESP_LOGI(TAG, "Profile runs complete");

    // Average across all the runs
    profile.turn_on_delay = roundf((float)profile_sum.turn_on_delay / PROFILE_ITERATIONS);
    profile.rise_time = roundf((float)profile_sum.rise_time / PROFILE_ITERATIONS);
    profile.rise_time_equiv = roundf((float)profile_sum.rise_time_equiv / PROFILE_ITERATIONS);
    profile.turn_off_delay = roundf((float)profile_sum.turn_off_delay / PROFILE_ITERATIONS);
    profile.fall_time = roundf((float)profile_sum.fall_time / PROFILE_ITERATIONS);
    profile.fall_time_equiv = roundf((float)profile_sum.fall_time_equiv / PROFILE_ITERATIONS);

    tcs3472_disable(&hi2c2);
    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

    ESP_LOGI(TAG, "Relay on delay: %ldms", profile.turn_on_delay);
    ESP_LOGI(TAG, "Rise time: %ldms (full_equiv=%ldms)", profile.rise_time, profile.rise_time_equiv);
    ESP_LOGI(TAG, "Relay off delay: %ldms", profile.turn_off_delay);
    ESP_LOGI(TAG, "Fall time: %ldms (full_equiv=%ldms)", profile.fall_time, profile.fall_time_equiv);

    char buf[256];
    sprintf(buf, "\n"
        "Relay on: %ldms\n"
        "Rise time: %ldms (%ldms)\n"
        "Relay off: %ldms\n"
        "Fall time: %ldms (%ldms)",
        profile.turn_on_delay,
        profile.rise_time, profile.rise_time_equiv,
        profile.turn_off_delay,
        profile.fall_time, profile.fall_time_equiv);
    do {
        uint8_t option = display_message(
            "Enlarger Calibration Done",
            NULL, buf,
            " Done ");
        if (option == 1) {
            break;
        }
    } while (1);
}

bool calibration_initialize_sensor()
{
    HAL_StatusTypeDef ret = HAL_OK;
    do {
        ESP_LOGI(TAG, "Initializing sensor");
        ret = tcs3472_init(&hi2c2);
        if (ret != HAL_OK) {
            ESP_LOGE(TAG, "Error initializing TCS3472: %d", ret);
            break;
        }

        ret = tcs3472_enable(&hi2c2);
        if (ret != HAL_OK) {
            ESP_LOGE(TAG, "Error enabling TCS3472: %d", ret);
            break;
        }

        //TODO Implement auto-ranging on gain/time

        ret = tcs3472_set_gain(&hi2c2, TCS3472_AGAIN_60X);
        if (ret != HAL_OK) {
            ESP_LOGE(TAG, "Error setting TCS3472 gain: %d", ret);
            break;
        }

        ret = tcs3472_set_time(&hi2c2, TCS3472_ATIME_4_8MS);
        if (ret != HAL_OK) {
            ESP_LOGE(TAG, "Error setting TCS3472 time: %d", ret);
            break;
        }

        bool valid = false;
        do {
            ret = tcs3472_get_status_valid(&hi2c2, &valid);
            if (ret != HAL_OK) {
                ESP_LOGE(TAG, "Error getting TCS3472 status: %d", ret);
                break;
            }
        } while (!valid);

        ESP_LOGI(TAG, "Sensor initialized");
    } while(0);

    if (ret != HAL_OK) {
        tcs3472_disable(&hi2c2);
        return false;
    }
    return true;
}

bool calibration_collect_reference_stats(reading_stats_t *stats_on, reading_stats_t *stats_off)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint16_t readings[REFERENCE_READING_ITERATIONS];

    if (!stats_on || !stats_off) { return false; }

    ESP_LOGI(TAG, "Turning enlarger on for baseline reading");
    relay_enlarger_enable(true);

    ESP_LOGI(TAG, "Waiting for light to stabilize");
    if (!delay_with_cancel(5000)) {
        return false;
    }

    ESP_LOGI(TAG, "Collecting data with enlarger on");
    for (int i = 0; i < REFERENCE_READING_ITERATIONS; i++) {
        uint16_t channel_data;
        ret = tcs3472_get_clear_channel_data(&hi2c2, &channel_data);
        if (ret != HAL_OK) {
            ESP_LOGE(TAG, "Error getting TCS3472 channel data: %d", ret);
            return false;
        }
        readings[i] = channel_data;
        osDelay(pdMS_TO_TICKS(5));
    }

    relay_enlarger_enable(false);

    ESP_LOGI(TAG, "Computing baseline enlarger reading statistics");
    calculate_reading_stats(stats_on, readings, REFERENCE_READING_ITERATIONS);

    ESP_LOGI(TAG, "Waiting for light to stabilize");
    if (!delay_with_cancel(2000)) {
        return false;
    }

    ESP_LOGI(TAG, "Collecting data with enlarger off");
    for (int i = 0; i < REFERENCE_READING_ITERATIONS; i++) {
        uint16_t channel_data;
        ret = tcs3472_get_clear_channel_data(&hi2c2, &channel_data);
        if (ret != HAL_OK) {
            ESP_LOGE(TAG, "Error getting TCS3472 channel data: %d", ret);
            return false;
        }
        readings[i] = channel_data;
        osDelay(pdMS_TO_TICKS(5));
    }

    ESP_LOGI(TAG, "Computing baseline darkness reading statistics");
    calculate_reading_stats(stats_off, readings, REFERENCE_READING_ITERATIONS);

    return true;
}

bool calibration_validate_reference_stats(reading_stats_t *stats_on, reading_stats_t *stats_off)
{
    if (!stats_on || !stats_off) { return false; }

    if (stats_on->min <= stats_off->max) {
        ESP_LOGW(TAG, "On and off ranges overlap");
        return false;
    }

    if ((stats_on->min - stats_off->max) < 50) {
        ESP_LOGW(TAG, "Insufficient separation between on and off ranges");
        return false;
    }

    if ((stats_on->mean - stats_off->mean) < 200) {
        ESP_LOGW(TAG, "Insufficient separation between on and off mean values");
        return false;
    }

    return true;
}

bool calibration_build_profile(enlarger_profile_t *profile, const reading_stats_t *stats_on, const reading_stats_t *stats_off)
{
    if (!profile || !stats_on || !stats_off) {
        return false;
    }

    HAL_StatusTypeDef ret = HAL_OK;
    uint16_t rising_threshold = stats_off->max;
    if (rising_threshold < 2) {
        rising_threshold = 2;
    }

    uint16_t falling_threshold = roundf(stats_off->mean + stats_off->stddev);
    if (falling_threshold < 2) {
        falling_threshold = 2;
    }

    uint16_t channel_data = 0;

    ESP_LOGI(TAG, "Collecting profile data...");

    TaskHandle_t current_task_handle = xTaskGetCurrentTaskHandle();
    UBaseType_t current_task_priority = uxTaskPriorityGet(current_task_handle);

    TickType_t time_mark = 0;
    uint32_t integrated_rise = 0;
    uint32_t rise_counts = 0;
    uint32_t integrated_fall = 0;
    uint32_t fall_counts = 0;

    vTaskPrioritySet(current_task_handle, osPriorityRealtime);
    TickType_t time_relay_on = xTaskGetTickCount();
    time_mark = time_relay_on;
    relay_enlarger_enable(true);
    do {
        ret = tcs3472_get_clear_channel_data(&hi2c2, &channel_data);
        if (ret != HAL_OK) {
            ESP_LOGE(TAG, "Error getting TCS3472 channel data: %d", ret);
            vTaskPrioritySet(current_task_handle, current_task_priority);
            return false;
        }
        if (channel_data > rising_threshold) { break; }

        time_mark += pdMS_TO_TICKS(5);
        osDelayUntil(time_mark);
        if (time_mark - time_relay_on > MAX_LOOP_DURATION) { return false; }
    } while (1);

    TickType_t time_rise_start = xTaskGetTickCount();
    time_mark = time_rise_start;
    do {
        ret = tcs3472_get_clear_channel_data(&hi2c2, &channel_data);
        if (ret != HAL_OK) {
            ESP_LOGE(TAG, "Error getting TCS3472 channel data: %d", ret);
            vTaskPrioritySet(current_task_handle, current_task_priority);
            return false;
        }
        integrated_rise += channel_data;
        rise_counts++;
        if (channel_data >= (uint16_t)roundf(stats_on->mean - stats_on->stddev)) { break; }

        time_mark += pdMS_TO_TICKS(5);
        osDelayUntil(time_mark);
        if (time_mark - time_rise_start > MAX_LOOP_DURATION) { return false; }
    } while (1);

    TickType_t time_rise_end = xTaskGetTickCount();

    vTaskPrioritySet(current_task_handle, current_task_priority);

    if (!delay_with_cancel(5000)) {
        return false;
    }

    vTaskPrioritySet(current_task_handle, osPriorityRealtime);

    TickType_t time_relay_off = xTaskGetTickCount();
    time_mark = time_relay_off;
    relay_enlarger_enable(false);
    do {
        ret = tcs3472_get_clear_channel_data(&hi2c2, &channel_data);
        if (ret != HAL_OK) {
            ESP_LOGE(TAG, "Error getting TCS3472 channel data: %d", ret);
            vTaskPrioritySet(current_task_handle, current_task_priority);
            return false;
        }
        if (channel_data < stats_on->min) { break; }

        time_mark += pdMS_TO_TICKS(5);
        osDelayUntil(time_mark);
        if (time_mark - time_relay_off > MAX_LOOP_DURATION) { return false; }
    } while (1);

    TickType_t time_fall_start = xTaskGetTickCount();
    time_mark = time_fall_start;
    do {
        ret = tcs3472_get_clear_channel_data(&hi2c2, &channel_data);
        if (ret != HAL_OK) {
            ESP_LOGE(TAG, "Error getting TCS3472 channel data: %d", ret);
            vTaskPrioritySet(current_task_handle, current_task_priority);
            return false;
        }
        integrated_fall += channel_data;
        fall_counts++;
        if (channel_data < falling_threshold) { break; }

        time_mark += pdMS_TO_TICKS(5);
        osDelayUntil(time_mark);
        if (time_mark - time_fall_start > MAX_LOOP_DURATION) { return false; }
    } while (1);

    TickType_t time_fall_end = xTaskGetTickCount();

    vTaskPrioritySet(current_task_handle, current_task_priority);

    if (!delay_with_cancel(5000)) {
        return false;
    }

    memset(profile, 0, sizeof(enlarger_profile_t));
    profile->turn_on_delay = (time_rise_start - time_relay_on) / portTICK_PERIOD_MS;
    profile->rise_time = (time_rise_end - time_rise_start) / portTICK_PERIOD_MS;

    float rise_scale_factor = (float)integrated_rise / (stats_on->mean * rise_counts);
    profile->rise_time_equiv = roundf(profile->rise_time * rise_scale_factor);


    profile->turn_off_delay = (time_fall_start - time_relay_off) / portTICK_PERIOD_MS;
    profile->fall_time = (time_fall_end - time_fall_start) / portTICK_PERIOD_MS;

    float fall_scale_factor = (float)integrated_fall / (stats_on->mean * fall_counts);
    profile->fall_time_equiv = roundf(profile->fall_time * fall_scale_factor);

    ESP_LOGI(TAG, "Relay on delay: %ldms", profile->turn_on_delay);
    ESP_LOGI(TAG, "Rise time: %ldms (full_equiv=%ldms)", profile->rise_time, profile->rise_time_equiv);
    ESP_LOGI(TAG, "Relay off delay: %ldms", profile->turn_off_delay);
    ESP_LOGI(TAG, "Fall time: %ldms (full_equiv=%ldms)", profile->fall_time, profile->fall_time_equiv);

    return true;
}

void calculate_reading_stats(reading_stats_t *stats, uint16_t *readings, size_t len)
{
    if (!stats || !readings) { return; }

    uint16_t reading_min = UINT16_MAX;
    uint16_t reading_max = 0;
    float reading_sum = 0;

    for (int i = 0; i < len; i++) {
        reading_sum += readings[i];
        if (readings[i] > reading_max) {
            reading_max = readings[i];
        }
        if (readings[i] < reading_min) {
            reading_min = readings[i];
        }
    }
    float reading_mean = reading_sum / len;

    float mean_dist_sum = 0;
    for (int i = 0; i < len; i++) {
        mean_dist_sum += powf(fabsf(readings[i] - reading_mean), 2);
    }

    float reading_stddev = sqrtf((mean_dist_sum / len));

    stats->mean = reading_mean;
    stats->min = reading_min;
    stats->max = reading_max;
    stats->stddev = reading_stddev;
}

bool delay_with_cancel(uint32_t time_ms)
{
    keypad_event_t keypad_event;
    TickType_t tick_start = xTaskGetTickCount();
    TickType_t tick_time = pdMS_TO_TICKS(time_ms);

    do {
        if (keypad_wait_for_event(&keypad_event, 10) == HAL_OK) {
            if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                ESP_LOGW(TAG, "Canceling enlarger calibration");
                return false;
            }
        }
    } while ((xTaskGetTickCount() - tick_start) < tick_time);

    return true;
}
