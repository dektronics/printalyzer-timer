#include "enlarger_calibration.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include <FreeRTOS.h>
#include <task.h>
#include <cmsis_os.h>

#define LOG_TAG "enlarger_calibration"
#include <elog.h>

#include "main_menu.h"
#include "display.h"
#include "keypad.h"
#include "enlarger_config.h"
#include "enlarger_control.h"
#include "meter_probe.h"
#include "relay.h"
#include "tcs3472.h"
#include "illum_controller.h"
#include "util.h"

#define REFERENCE_READING_ITERATIONS 100
#define PROFILE_ITERATIONS 5
#define MAX_LOOP_DURATION pdMS_TO_TICKS(10000)

/*
 * The enlarger calibration process works by measuring the light output
 * of the enlarger over time, while running through a series of simulated
 * exposure cycles. The result of this process is a series of numbers that
 * define how the light output of the enlarger behaves on either end of
 * the controllable exposure process.
 *
 * The sensor polling loop used for calibration involves a channel read,
 * followed by a 5ms delay. The sensor itself is configured for an
 * integration time of 4.8ms, and the channel read operation has been
 * measured to take 0.14ms. There does not appear to be a way to synchronize
 * ourselves to the exact integration state of the sensor, so this approach
 * is hopefully sufficient for the data we are trying to collect.
 */

extern I2C_HandleTypeDef hi2c2;

typedef struct {
    float mean;
    uint16_t min;
    uint16_t max;
    float stddev;
} reading_stats_t;

typedef enum {
    CALIBRATION_OK,
    CALIBRATION_CANCEL,
    CALIBRATION_SENSOR_ERROR,
    CALIBRATION_ZERO_READING,
    CALIBRATION_SENSOR_SATURATED,
    CALIBRATION_INVALID_REFERENCE_STATS,
    CALIBRATION_FAIL
} calibration_result_t;

static calibration_result_t enlarger_calibration_start(enlarger_config_t *result_config);
static void enlarger_calibration_preview_result(const enlarger_config_t *profile);
static void enlarger_calibration_show_error(calibration_result_t result_error);
static calibration_result_t calibration_initialize_sensor();
static calibration_result_t calibration_collect_reference_stats(reading_stats_t *stats_on, reading_stats_t *stats_off, reading_stats_t *stats_color);
static calibration_result_t calibration_validate_reference_stats(reading_stats_t *stats_on, reading_stats_t *stats_off);
static calibration_result_t calibration_build_timing_profile(enlarger_timing_t *timing_profile, const reading_stats_t *stats_on, const reading_stats_t *stats_off);

static void calculate_reading_stats(reading_stats_t *stats, uint16_t *readings, size_t len);
static bool delay_with_cancel(uint32_t time_ms);

menu_result_t menu_enlarger_calibration(enlarger_config_t *config)
{
    //TODO Enlarger calibration needs major rework to support the new meter probe sensor
    menu_result_t menu_result = MENU_CANCEL;

    illum_controller_safelight_state(ILLUM_SAFELIGHT_EXPOSURE);

    uint8_t option = display_message(
            "Enlarger Calibration",
            NULL,
            "\nMake sure your safelight is off,\n"
            "the meter probe is connected,\n"
            "and your enlarger is ready.",
            " Start ");

    if (option == 1) {
        calibration_result_t calibration_result = enlarger_calibration_start(config);
        if (calibration_result == CALIBRATION_OK) {
            enlarger_calibration_preview_result(config);
            menu_result = MENU_OK;
        } else if (calibration_result == CALIBRATION_CANCEL) {
            menu_result = MENU_CANCEL;
        } else {
            enlarger_calibration_show_error(calibration_result);
            menu_result = MENU_CANCEL;
        }
        relay_enlarger_enable(false);
    } else if (option == UINT8_MAX) {
        menu_result = MENU_TIMEOUT;
    }

    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

    return menu_result;
}

calibration_result_t enlarger_calibration_start(enlarger_config_t *result_config)
{
    calibration_result_t calibration_result;
    log_i("Starting enlarger calibration process");

    display_static_list("Enlarger Calibration", "\n\nInitializing...");

    // Turn everything off, just in case it isn't already off
    relay_safelight_enable(false);
    relay_enlarger_enable(false);

    calibration_result = calibration_initialize_sensor();
    if (calibration_result != CALIBRATION_OK) {
        log_e("Could not initialize sensor");
        return calibration_result;
    }

    if (!delay_with_cancel(1000)) {
        tcs3472_disable(&hi2c2);
        return CALIBRATION_CANCEL;
    }

    display_static_list("Enlarger Calibration", "\n\nCollecting Reference Points");

    reading_stats_t enlarger_on_stats;
    reading_stats_t enlarger_off_stats;
    reading_stats_t enlarger_color_stats;
    calibration_result = calibration_collect_reference_stats(&enlarger_on_stats, &enlarger_off_stats, &enlarger_color_stats);
    if (calibration_result != CALIBRATION_OK) {
        log_e("Could not collect reference stats");
        tcs3472_disable(&hi2c2);
        if (relay_enlarger_is_enabled()) {
            relay_enlarger_enable(false);
        }
        return calibration_result;
    }

    // Log statistics used for reference points
    log_i("Enlarger on");
    log_i("Mean = %.1f, Min = %d, Max = %d, StdDev = %.1f",
        enlarger_on_stats.mean, enlarger_on_stats.min, enlarger_on_stats.max,
        enlarger_on_stats.stddev);
    log_i("Enlarger off");
    log_i("Mean = %.1f, Min = %d, Max = %d, StdDev = %.1f",
        enlarger_off_stats.mean, enlarger_off_stats.min, enlarger_off_stats.max,
        enlarger_off_stats.stddev);
    log_i("Enlarger color temperature");
    log_i("Mean = %.1f, Min = %d, Max = %d, StdDev = %.1f",
        enlarger_color_stats.mean, enlarger_color_stats.min, enlarger_color_stats.max,
        enlarger_color_stats.stddev);

    calibration_result = calibration_validate_reference_stats(&enlarger_on_stats, &enlarger_off_stats);
    if (calibration_result != CALIBRATION_OK) {
        log_w("Reference stats are not usable for calibration");
        tcs3472_disable(&hi2c2);
        illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);
        return calibration_result;
    }

    if (!delay_with_cancel(1000)) {
        tcs3472_disable(&hi2c2);
        return CALIBRATION_CANCEL;
    }

    // Do the profiling process
    display_static_list("Enlarger Calibration", "\n\nProfiling enlarger...");

    enlarger_timing_t timing_profile;
    enlarger_timing_t timing_profile_sum;
    enlarger_timing_t timing_profile_inc[PROFILE_ITERATIONS];

    memset(&timing_profile, 0, sizeof(enlarger_timing_t));
    memset(&timing_profile_sum, 0, sizeof(enlarger_timing_t));

    for (int i = 0; i < PROFILE_ITERATIONS; i++) {
        log_i("Profile run %d...", i + 1);

        calibration_result = calibration_build_timing_profile(&timing_profile_inc[i], &enlarger_on_stats, &enlarger_off_stats);
        if (calibration_result != CALIBRATION_OK) {
            log_e("Could not build profile");
            tcs3472_disable(&hi2c2);
            illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

            if (relay_enlarger_is_enabled()) {
                relay_enlarger_enable(false);
            }

            return calibration_result;
        }

        timing_profile_sum.turn_on_delay += timing_profile_inc[i].turn_on_delay;
        timing_profile_sum.rise_time += timing_profile_inc[i].rise_time;
        timing_profile_sum.rise_time_equiv += timing_profile_inc[i].rise_time_equiv;
        timing_profile_sum.turn_off_delay += timing_profile_inc[i].turn_off_delay;
        timing_profile_sum.fall_time += timing_profile_inc[i].fall_time;
        timing_profile_sum.fall_time_equiv += timing_profile_inc[i].fall_time_equiv;
    }
    log_i("Profile runs complete");

    // Average across all the runs
    timing_profile.turn_on_delay = roundf((float)timing_profile_sum.turn_on_delay / PROFILE_ITERATIONS);
    timing_profile.rise_time = roundf((float)timing_profile_sum.rise_time / PROFILE_ITERATIONS);
    timing_profile.rise_time_equiv = roundf((float)timing_profile_sum.rise_time_equiv / PROFILE_ITERATIONS);
    timing_profile.turn_off_delay = roundf((float)timing_profile_sum.turn_off_delay / PROFILE_ITERATIONS);
    timing_profile.fall_time = roundf((float)timing_profile_sum.fall_time / PROFILE_ITERATIONS);
    timing_profile.fall_time_equiv = roundf((float)timing_profile_sum.fall_time_equiv / PROFILE_ITERATIONS);
    timing_profile.color_temperature = roundf(enlarger_color_stats.mean);

    tcs3472_disable(&hi2c2);
    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

    log_i("Relay on delay: %ldms", timing_profile.turn_on_delay);
    log_i("Rise time: %ldms (full_equiv=%ldms)", timing_profile.rise_time, timing_profile.rise_time_equiv);
    log_i("Relay off delay: %ldms", timing_profile.turn_off_delay);
    log_i("Fall time: %ldms (full_equiv=%ldms)", timing_profile.fall_time, timing_profile.fall_time_equiv);
    log_i("Color temperature: %ldK", timing_profile.color_temperature);

    if (result_config) {
        result_config->timing.turn_on_delay = timing_profile.turn_on_delay;
        result_config->timing.rise_time = timing_profile.rise_time;
        result_config->timing.rise_time_equiv = timing_profile.rise_time_equiv;
        result_config->timing.turn_off_delay = timing_profile.turn_off_delay;
        result_config->timing.fall_time = timing_profile.fall_time;
        result_config->timing.fall_time_equiv = timing_profile.fall_time_equiv;
        result_config->timing.color_temperature = timing_profile.color_temperature;
    }

    return CALIBRATION_OK;
}

void enlarger_calibration_preview_result(const enlarger_config_t *config)
{
    char buf[256];
    sprintf(buf, "\n"
        "Enlarger on: %ldms\n"
        "Rise time: %ldms (%ldms)\n"
        "Enlarger off: %ldms\n"
        "Fall time: %ldms (%ldms)",
        config->timing.turn_on_delay,
        config->timing.rise_time, config->timing.rise_time_equiv,
        config->timing.turn_off_delay,
        config->timing.fall_time, config->timing.fall_time_equiv);
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

void enlarger_calibration_show_error(calibration_result_t result_error)
{
    const char *msg = NULL;

    switch (result_error) {
    case CALIBRATION_SENSOR_ERROR:
        msg =
            "The meter probe is either\n"
            "disconnected or not working\n"
            "correctly.";
        break;
    case CALIBRATION_ZERO_READING:
        msg =
            "The meter probe is incorrectly\n"
            "positioned or the enlarger is\n"
            "not turning on.";
        break;
    case CALIBRATION_SENSOR_SATURATED:
        msg =
            "The enlarger is too bright\n"
            "for calibration.";
        break;
    case CALIBRATION_INVALID_REFERENCE_STATS:
        msg =
            "Could not detect a large enough\n"
            "brightness difference between\n"
            "the enlarger being on and off.";
        break;
    case CALIBRATION_FAIL:
    default:
        msg =
            "Unable to complete\n"
            "enlarger calibration.";
        break;
    }

    do {
        uint8_t option = display_message(
            "Enlarger Calibration Failed\n",
            NULL, msg, " OK ");
        if (option == 1) { break; }
    } while (1);
}

calibration_result_t calibration_initialize_sensor()
{
    HAL_StatusTypeDef ret = HAL_OK;
    do {
        log_i("Initializing sensor");
        ret = tcs3472_init(&hi2c2);
        if (ret != HAL_OK) {
            log_e("Error initializing TCS3472: %d", ret);
            break;
        }

        ret = tcs3472_enable(&hi2c2);
        if (ret != HAL_OK) {
            log_e("Error enabling TCS3472: %d", ret);
            break;
        }

        ret = tcs3472_set_gain(&hi2c2, TCS3472_AGAIN_60X);
        if (ret != HAL_OK) {
            log_e("Error setting TCS3472 gain: %d", ret);
            break;
        }

        ret = tcs3472_set_time(&hi2c2, TCS3472_ATIME_4_8MS);
        if (ret != HAL_OK) {
            log_e("Error setting TCS3472 time: %d", ret);
            break;
        }

        bool valid = false;
        do {
            ret = tcs3472_get_status_valid(&hi2c2, &valid);
            if (ret != HAL_OK) {
                log_e("Error getting TCS3472 status: %d", ret);
                break;
            }
        } while (!valid);

        log_i("Sensor initialized");
    } while(0);

    if (ret != HAL_OK) {
        tcs3472_disable(&hi2c2);
        return CALIBRATION_SENSOR_ERROR;
    }
    return CALIBRATION_OK;
}

calibration_result_t calibration_collect_reference_stats(reading_stats_t *stats_on, reading_stats_t *stats_off, reading_stats_t *stats_color)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint16_t readings[REFERENCE_READING_ITERATIONS];
    uint16_t color_readings[REFERENCE_READING_ITERATIONS];

    if (!stats_on || !stats_off) { return false; }

    log_i("Turning enlarger on for baseline reading");
    relay_enlarger_enable(true);

    log_i("Waiting for light to stabilize");
    if (!delay_with_cancel(5000)) {
        return CALIBRATION_CANCEL;
    }

    log_i("Finding appropriate gain setting");
    bool gain_selected = false;
    for (tcs3472_again_t gain = TCS3472_AGAIN_60X; gain >= TCS3472_AGAIN_1X; --gain) {
        tcs3472_channel_data_t channel_data;

        ret = tcs3472_set_gain(&hi2c2, gain);
        if (ret != HAL_OK) {
            log_e("Error setting TCS3472 gain: %d (gain=%d)", ret, gain);
            return CALIBRATION_SENSOR_ERROR;
        }

        osDelay(pdMS_TO_TICKS(20));

        ret = tcs3472_get_full_channel_data(&hi2c2, &channel_data);
        if (ret != HAL_OK) {
            log_e("Error getting TCS3472 channel data: %d", ret);
            return CALIBRATION_SENSOR_ERROR;
        }

        if (channel_data.clear == 0) {
            log_w("No reading on clear channel");
            return CALIBRATION_ZERO_READING;
        }
        if (tcs3472_calculate_color_temp(&channel_data) > 0) {
            log_i("Selected gain: %s", tcs3472_gain_str(gain));
            gain_selected = true;
            break;
        }
    }
    if (!gain_selected) {
        log_w("Could not find a gain setting with a valid unsaturated reading");
        return CALIBRATION_SENSOR_SATURATED;
    }

    log_i("Collecting data with enlarger on");
    for (int i = 0; i < REFERENCE_READING_ITERATIONS; i++) {
        tcs3472_channel_data_t channel_data;
        ret = tcs3472_get_full_channel_data(&hi2c2, &channel_data);
        if (ret != HAL_OK) {
            log_e("Error getting TCS3472 channel data: %d", ret);
            return CALIBRATION_SENSOR_ERROR;
        }
        readings[i] = channel_data.clear;
        color_readings[i] = tcs3472_calculate_color_temp(&channel_data);
        osDelay(pdMS_TO_TICKS(5));
    }

    relay_enlarger_enable(false);

    log_i("Computing baseline enlarger reading statistics");
    calculate_reading_stats(stats_on, readings, REFERENCE_READING_ITERATIONS);
    if (stats_color) {
        calculate_reading_stats(stats_color, color_readings, REFERENCE_READING_ITERATIONS);
    }

    log_i("Waiting for light to stabilize");
    if (!delay_with_cancel(2000)) {
        return false;
    }

    log_i("Collecting data with enlarger off");
    for (int i = 0; i < REFERENCE_READING_ITERATIONS; i++) {
        uint16_t channel_data;
        ret = tcs3472_get_clear_channel_data(&hi2c2, &channel_data);
        if (ret != HAL_OK) {
            log_e("Error getting TCS3472 channel data: %d", ret);
            return false;
        }
        readings[i] = channel_data;
        osDelay(pdMS_TO_TICKS(5));
    }

    log_i("Computing baseline darkness reading statistics");
    calculate_reading_stats(stats_off, readings, REFERENCE_READING_ITERATIONS);

    return CALIBRATION_OK;
}

calibration_result_t calibration_validate_reference_stats(reading_stats_t *stats_on, reading_stats_t *stats_off)
{
    if (!stats_on || !stats_off) { return CALIBRATION_FAIL; }

    if (stats_on->min <= stats_off->max) {
        log_w("On and off ranges overlap");
        return CALIBRATION_INVALID_REFERENCE_STATS;
    }

    if ((stats_on->min - stats_off->max) < 10) {
        log_w("Insufficient separation between on and off ranges");
        return CALIBRATION_INVALID_REFERENCE_STATS;
    }

    if ((stats_on->mean - stats_off->mean) < 20) {
        log_w("Insufficient separation between on and off mean values");
        return CALIBRATION_INVALID_REFERENCE_STATS;
    }

    return CALIBRATION_OK;
}

calibration_result_t calibration_build_timing_profile(enlarger_timing_t *timing_profile, const reading_stats_t *stats_on, const reading_stats_t *stats_off)
{
    if (!timing_profile || !stats_on || !stats_off) {
        return CALIBRATION_FAIL;
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

    log_i("Collecting profile data...");

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
            log_e("Error getting TCS3472 channel data: %d", ret);
            vTaskPrioritySet(current_task_handle, current_task_priority);
            return CALIBRATION_SENSOR_ERROR;
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
            log_e("Error getting TCS3472 channel data: %d", ret);
            vTaskPrioritySet(current_task_handle, current_task_priority);
            return CALIBRATION_SENSOR_ERROR;
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
        return CALIBRATION_CANCEL;
    }

    vTaskPrioritySet(current_task_handle, osPriorityRealtime);

    TickType_t time_relay_off = xTaskGetTickCount();
    time_mark = time_relay_off;
    relay_enlarger_enable(false);
    do {
        ret = tcs3472_get_clear_channel_data(&hi2c2, &channel_data);
        if (ret != HAL_OK) {
            log_e("Error getting TCS3472 channel data: %d", ret);
            vTaskPrioritySet(current_task_handle, current_task_priority);
            return CALIBRATION_SENSOR_ERROR;
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
            log_e("Error getting TCS3472 channel data: %d", ret);
            vTaskPrioritySet(current_task_handle, current_task_priority);
            return CALIBRATION_SENSOR_ERROR;
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
        return CALIBRATION_CANCEL;
    }

    memset(timing_profile, 0, sizeof(enlarger_timing_t));
    timing_profile->turn_on_delay = (time_rise_start - time_relay_on) / portTICK_PERIOD_MS;
    timing_profile->rise_time = (time_rise_end - time_rise_start) / portTICK_PERIOD_MS;

    float rise_scale_factor = (float)integrated_rise / (stats_on->mean * rise_counts);
    timing_profile->rise_time_equiv = roundf(timing_profile->rise_time * rise_scale_factor);

    timing_profile->turn_off_delay = (time_fall_start - time_relay_off) / portTICK_PERIOD_MS;
    timing_profile->fall_time = (time_fall_end - time_fall_start) / portTICK_PERIOD_MS;

    float fall_scale_factor = (float)integrated_fall / (stats_on->mean * fall_counts);
    timing_profile->fall_time_equiv = roundf(timing_profile->fall_time * fall_scale_factor);

    log_i("Relay on delay: %ldms", timing_profile->turn_on_delay);
    log_i("Rise time: %ldms (full_equiv=%ldms)", timing_profile->rise_time, timing_profile->rise_time_equiv);
    log_i("Relay off delay: %ldms", timing_profile->turn_off_delay);
    log_i("Fall time: %ldms (full_equiv=%ldms)", timing_profile->fall_time, timing_profile->fall_time_equiv);

    return CALIBRATION_OK;
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
                log_w("Canceling enlarger calibration");
                return false;
            }
        }
    } while ((xTaskGetTickCount() - tick_start) < tick_time);

    return true;
}
