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
#include "tsl2585.h"
#include "relay.h"
#include "illum_controller.h"
#include "dmx.h"
#include "util.h"

#define LIGHT_STABLIZE_WAIT_MS       (5000U)
#define REFERENCE_READING_ITERATIONS (MAX_ALS_COUNT * 12U)
#define PROFILE_ITERATIONS           (5U)
#define MAX_LOOP_DURATION pdMS_TO_TICKS(10000)

/* Sensor settings for a 2.5ms integration time */
#define SENSOR_SAMPLE_TIME 359
#define SENSOR_NUM_SAMPLES 4

#define SENSOR_READING_TIMEOUT (50U)

/* Minimum value we'll accept for a non-zero reading */
#define SENSOR_ZERO_THRESHOLD (100U)

/* Duration of each sampling loop */
#define SAMPLE_TICKS pdMS_TO_TICKS(10)

static const char *DISPLAY_TITLE = "Enlarger Calibration";

/*
 * The enlarger calibration process works by measuring the light output
 * of the enlarger over time, while running through a series of simulated
 * exposure cycles. The result of this process is a series of numbers that
 * define how the light output of the enlarger behaves on either end of
 * the controllable exposure process.
 */

extern I2C_HandleTypeDef hi2c2;

typedef struct {
    float mean;
    uint32_t min;
    uint32_t max;
    float stddev;
} reading_stats_t;

typedef enum {
    CALIBRATION_OK,
    CALIBRATION_CANCEL,
    CALIBRATION_SENSOR_ERROR,
    CALIBRATION_ZERO_READING,
    CALIBRATION_SENSOR_SATURATED,
    CALIBRATION_INVALID_REFERENCE_STATS,
    CALIBRATION_TIMEOUT,
    CALIBRATION_FAIL
} calibration_result_t;

static calibration_result_t enlarger_calibration_start(enlarger_config_t *config);
static void enlarger_calibration_preview_result(const enlarger_config_t *config);
static void enlarger_calibration_show_error(calibration_result_t result_error);
static calibration_result_t calibration_collect_reference_stats(const enlarger_control_t *enlarger_control,
    reading_stats_t *stats_on, reading_stats_t *stats_off, reading_stats_t *stats_sensor);
static calibration_result_t calibration_validate_reference_stats(
    const reading_stats_t *stats_on, const reading_stats_t *stats_off, const reading_stats_t *stats_sensor);
static calibration_result_t calibration_build_timing_profile(const enlarger_control_t *enlarger_control,
    enlarger_timing_t *timing_profile,
    const reading_stats_t *stats_on, const reading_stats_t *stats_off, const reading_stats_t *stats_sensor);

static void calculate_reading_stats(reading_stats_t *stats, uint32_t *readings, size_t len);
static bool delay_with_cancel(uint32_t time_ms);

menu_result_t menu_enlarger_calibration(enlarger_config_t *config)
{
    menu_result_t menu_result = MENU_CANCEL;

    illum_controller_safelight_state(ILLUM_SAFELIGHT_EXPOSURE);

    /* If we are calibrating a DMX enlarger, then make sure the DMX task is ready */
    if (config->control.dmx_control) {
        /* Make sure the DMX port is running and the frame is clear */
        if (dmx_get_port_state() == DMX_PORT_DISABLED) {
            dmx_enable();
        }
        if (dmx_get_port_state() == DMX_PORT_ENABLED_IDLE) {
            dmx_start();
        }
        if (dmx_get_port_state() == DMX_PORT_ENABLED_TRANSMITTING) {
            dmx_clear_frame(false);
        }
    }

    uint8_t option = display_message(
        DISPLAY_TITLE,
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
        enlarger_control_set_state_off(&config->control, false);
    } else if (option == UINT8_MAX) {
        menu_result = MENU_TIMEOUT;
    }

    /* If we are calibrating a DMX enlarger, then make sure the DMX frame is clean afterwards */
    if (config->control.dmx_control) {
        /* Clear DMX output frame */
        if (dmx_get_port_state() == DMX_PORT_ENABLED_TRANSMITTING) {
            dmx_stop();
        }
    }

    illum_controller_refresh();
    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

    return menu_result;
}

calibration_result_t enlarger_calibration_start(enlarger_config_t *config)
{
    calibration_result_t calibration_result;
    log_i("Starting enlarger calibration process");

    meter_probe_handle_t *handle = meter_probe_handle();

    display_static_list(DISPLAY_TITLE, "\n\nInitializing...");

    /* Turn everything off, just in case it isn't already off */
    illum_controller_safelight_state(ILLUM_SAFELIGHT_EXPOSURE);
    enlarger_control_set_state_off(&config->control, true);

    /* Activate the meter probe */
    if (meter_probe_start(handle) != osOK) {
        log_e("Could not initialize meter probe");
        return CALIBRATION_SENSOR_ERROR;
    }

    if (!delay_with_cancel(1000)) {
        meter_probe_stop(handle);
        return CALIBRATION_CANCEL;
    }

    display_static_list(DISPLAY_TITLE, "\n\nCollecting Reference Points");

    reading_stats_t enlarger_on_stats;
    reading_stats_t enlarger_off_stats;
    reading_stats_t sensor_stats;
    calibration_result = calibration_collect_reference_stats(&config->control,
        &enlarger_on_stats, &enlarger_off_stats, &sensor_stats);
    if (calibration_result != CALIBRATION_OK) {
        log_e("Could not collect reference stats");
        meter_probe_stop(handle);
        enlarger_control_set_state_off(&config->control, true);
        return calibration_result;
    }

    /* Log statistics used for reference points */
    log_i("Enlarger on");
    log_i("Mean = %.1f, Min = %d, Max = %d, StdDev = %.1f",
        enlarger_on_stats.mean, enlarger_on_stats.min, enlarger_on_stats.max,
        enlarger_on_stats.stddev);
    log_i("Enlarger off");
    log_i("Mean = %.1f, Min = %d, Max = %d, StdDev = %.1f",
        enlarger_off_stats.mean, enlarger_off_stats.min, enlarger_off_stats.max,
        enlarger_off_stats.stddev);
    log_i("Sensor integration time");
    log_i("Mean = %.1f, Min = %d, Max = %d, StdDev = %.1f",
        sensor_stats.mean, sensor_stats.min, sensor_stats.max,
        sensor_stats.stddev);

    calibration_result = calibration_validate_reference_stats(&enlarger_on_stats, &enlarger_off_stats, &sensor_stats);
    if (calibration_result != CALIBRATION_OK) {
        log_w("Reference stats are not usable for calibration");
        meter_probe_stop(handle);
        illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);
        return calibration_result;
    }

    if (!delay_with_cancel(1000)) {
        meter_probe_stop(handle);
        return CALIBRATION_CANCEL;
    }

    /* Do the profiling process */
    enlarger_timing_t timing_profile;
    float turn_on_delay_sum = 0;
    float rise_time_sum = 0;
    float rise_time_equiv_sum = 0;
    float turn_off_delay_sum = 0;
    float fall_time_sum = 0;
    float fall_time_equiv_sum = 0;

    for (unsigned int i = 0; i < PROFILE_ITERATIONS; i++) {
        char buf[64];
        enlarger_timing_t timing_profile_inc;
        log_i("Profile run %d...", i + 1);

        sprintf(buf, "\nProfiling enlarger...\n\nCycle %d of %d", i + 1, PROFILE_ITERATIONS);
        display_static_list(DISPLAY_TITLE, buf);

        calibration_result = calibration_build_timing_profile(&config->control,
            &timing_profile_inc,
            &enlarger_on_stats, &enlarger_off_stats, &sensor_stats);
        if (calibration_result != CALIBRATION_OK) {
            log_e("Could not build profile");
            meter_probe_stop(handle);
            illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);
            enlarger_control_set_state_off(&config->control, true);
            return calibration_result;
        }

        turn_on_delay_sum += logf(timing_profile_inc.turn_on_delay);
        rise_time_sum += logf(timing_profile_inc.rise_time);
        rise_time_equiv_sum += logf(timing_profile_inc.rise_time_equiv);
        turn_off_delay_sum += logf(timing_profile_inc.turn_off_delay);
        fall_time_sum += logf(timing_profile_inc.fall_time);
        fall_time_equiv_sum += logf(timing_profile_inc.fall_time_equiv);
    }
    log_i("Profile runs complete");

    meter_probe_stop(handle);
    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);
    enlarger_control_set_state_off(&config->control, true);

    /* Geometric mean across all the runs */
    memset(&timing_profile, 0, sizeof(enlarger_timing_t));
    timing_profile.turn_on_delay = lroundf(expf(turn_on_delay_sum / (float)PROFILE_ITERATIONS));
    timing_profile.rise_time = lroundf(expf(rise_time_sum / (float)PROFILE_ITERATIONS));
    timing_profile.rise_time_equiv = lroundf(expf(rise_time_equiv_sum / (float)PROFILE_ITERATIONS));
    timing_profile.turn_off_delay = lroundf(expf(turn_off_delay_sum / (float)PROFILE_ITERATIONS));
    timing_profile.fall_time = lroundf(expf(fall_time_sum / (float)PROFILE_ITERATIONS));
    timing_profile.fall_time_equiv = lroundf(expf(fall_time_equiv_sum / (float)PROFILE_ITERATIONS));

    log_i("Relay on delay: %ldms", timing_profile.turn_on_delay);
    log_i("Rise time: %ldms (full_equiv=%ldms)", timing_profile.rise_time, timing_profile.rise_time_equiv);
    log_i("Relay off delay: %ldms", timing_profile.turn_off_delay);
    log_i("Fall time: %ldms (full_equiv=%ldms)", timing_profile.fall_time, timing_profile.fall_time_equiv);

    if (config) {
        config->timing.turn_on_delay = timing_profile.turn_on_delay;
        config->timing.rise_time = timing_profile.rise_time;
        config->timing.rise_time_equiv = timing_profile.rise_time_equiv;
        config->timing.turn_off_delay = timing_profile.turn_off_delay;
        config->timing.fall_time = timing_profile.fall_time;
        config->timing.fall_time_equiv = timing_profile.fall_time_equiv;
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
    case CALIBRATION_TIMEOUT:
        msg =
            "The enlarger took too long to\n"
            "respond while being measured.";
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

calibration_result_t calibration_collect_reference_stats(const enlarger_control_t *enlarger_control,
    reading_stats_t *stats_on, reading_stats_t *stats_off, reading_stats_t *stats_sensor)
{
    meter_probe_sensor_reading_t sensor_reading = {0};
    tsl2585_gain_t sensor_gain;
    uint32_t last_sensor_ticks = 0;
    uint32_t readings[REFERENCE_READING_ITERATIONS];
    uint32_t sensor_times[REFERENCE_READING_ITERATIONS / MAX_ALS_COUNT];
    uint32_t index;
    meter_probe_handle_t *handle = meter_probe_handle();

    if (!stats_on || !stats_off || !stats_sensor) { return CALIBRATION_FAIL; }

    log_i("Turning enlarger on for baseline reading");
    enlarger_control_set_state_focus(enlarger_control, true);

    log_i("Activating light sensor with AGC");
    if (meter_probe_sensor_set_gain(handle, TSL2585_GAIN_256X) != osOK) {
        return CALIBRATION_SENSOR_ERROR;
    }
    if (meter_probe_sensor_set_integration(handle, SENSOR_SAMPLE_TIME, SENSOR_NUM_SAMPLES) != osOK) {
        return CALIBRATION_SENSOR_ERROR;
    }
    if (meter_probe_sensor_set_mod_calibration(handle, 1) != osOK) {
        return CALIBRATION_SENSOR_ERROR;
    }
    if (meter_probe_sensor_enable_agc(handle, SENSOR_NUM_SAMPLES) != osOK) {
        return CALIBRATION_SENSOR_ERROR;
    }
    if (meter_probe_sensor_enable_fast_mode(handle) != osOK) {
        return CALIBRATION_SENSOR_ERROR;
    }

    log_i("Waiting for light to stabilize");
    if (!delay_with_cancel(LIGHT_STABLIZE_WAIT_MS)) {
        return CALIBRATION_CANCEL;
    }

    log_i("Finding appropriate gain setting");
    if (meter_probe_sensor_get_next_reading(handle, &sensor_reading, 100) != osOK) {
        return CALIBRATION_SENSOR_ERROR;
    }
    if (sensor_reading.reading[0].status != METER_SENSOR_RESULT_VALID) {
        log_w("Could not find a gain setting with a valid unsaturated reading");
        return CALIBRATION_SENSOR_SATURATED;
    }

    if (sensor_reading.reading[0].data < SENSOR_ZERO_THRESHOLD) {
        log_w("Could not find a gain setting with a reading above the zero threshold");
        return CALIBRATION_ZERO_READING;
    }
    sensor_gain = sensor_reading.reading[0].gain;

    log_i("Selected gain: %s", tsl2585_gain_str(sensor_gain));

    log_i("Reconfiguring light sensor without AGC");
    if (meter_probe_sensor_disable(handle) != osOK) {
        return CALIBRATION_SENSOR_ERROR;
    }
    if (meter_probe_sensor_disable_agc(handle) != osOK) {
        return CALIBRATION_SENSOR_ERROR;
    }
    if (meter_probe_sensor_set_mod_calibration(handle, 0xFF) != osOK) {
        return CALIBRATION_SENSOR_ERROR;
    }
    if (meter_probe_sensor_set_gain(handle, sensor_gain) != osOK) {
        return CALIBRATION_SENSOR_ERROR;
    }
    if (meter_probe_sensor_set_integration(handle, SENSOR_SAMPLE_TIME, SENSOR_NUM_SAMPLES) != osOK) {
        return CALIBRATION_SENSOR_ERROR;
    }
    if (meter_probe_sensor_enable_fast_mode(handle) != osOK) {
        return CALIBRATION_SENSOR_ERROR;
    }

    /* Skip a few integration cycles, just to be safe */
    osDelay(100);

    log_i("Collecting data with enlarger on");
    index = 0;
    for (unsigned int i = 0; i < REFERENCE_READING_ITERATIONS / MAX_ALS_COUNT; i++) {
        if (meter_probe_sensor_get_next_reading(handle, &sensor_reading, 100) != osOK) {
            return CALIBRATION_SENSOR_ERROR;
        }
        for (unsigned int j = 0; j < MAX_ALS_COUNT; j++) {
            readings[index++] = sensor_reading.reading[j].data;
        }
    }

    /* Turn the enlarger off */
    enlarger_control_set_state_off(enlarger_control, true);

    log_i("Computing baseline enlarger reading statistics");
    calculate_reading_stats(stats_on, readings, REFERENCE_READING_ITERATIONS);

    log_i("Waiting for light to stabilize");
    if (!delay_with_cancel(LIGHT_STABLIZE_WAIT_MS / 2)) {
        return CALIBRATION_TIMEOUT;
    }

    log_i("Collecting data with enlarger off");

    /* Capture a reading as the starting point for sensor cycle measurement */
    if (meter_probe_sensor_get_next_reading(handle, &sensor_reading, 100) != osOK) {
        return CALIBRATION_SENSOR_ERROR;
    }
    last_sensor_ticks = sensor_reading.ticks;

    index = 0;
    for (unsigned int i = 0; i < REFERENCE_READING_ITERATIONS / MAX_ALS_COUNT; i++) {
        if (meter_probe_sensor_get_next_reading(handle, &sensor_reading, 100) != osOK) {
            return CALIBRATION_SENSOR_ERROR;
        }
        for (unsigned int j = 0; j < MAX_ALS_COUNT; j++) {
            readings[index++] = sensor_reading.reading[j].data;
        }
        sensor_times[i] = (sensor_reading.ticks - last_sensor_ticks) / MAX_ALS_COUNT;
        last_sensor_ticks = sensor_reading.ticks;
    }

    log_i("Computing baseline darkness reading statistics");
    calculate_reading_stats(stats_off, readings, REFERENCE_READING_ITERATIONS);

    calculate_reading_stats(stats_sensor, sensor_times, REFERENCE_READING_ITERATIONS / MAX_ALS_COUNT);

    return CALIBRATION_OK;
}

calibration_result_t calibration_validate_reference_stats(
    const reading_stats_t *stats_on, const reading_stats_t *stats_off, const reading_stats_t *stats_sensor)
{
    if (!stats_on || !stats_off || !stats_sensor) { return CALIBRATION_FAIL; }

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

    const float sensor_integration_time = tsl2585_integration_time_ms(SENSOR_SAMPLE_TIME, SENSOR_NUM_SAMPLES);
    if (stats_sensor->mean < sensor_integration_time) {
        log_w("Sensor cycle time is less than integration time");
        return CALIBRATION_INVALID_REFERENCE_STATS;
    }

    return CALIBRATION_OK;
}

calibration_result_t calibration_build_timing_profile(const enlarger_control_t *enlarger_control,
    enlarger_timing_t *timing_profile,
    const reading_stats_t *stats_on, const reading_stats_t *stats_off, const reading_stats_t *stats_sensor)
{
    if (!enlarger_control || !timing_profile || !stats_on || !stats_off) {
        return CALIBRATION_FAIL;
    }

    meter_probe_handle_t *handle = meter_probe_handle();

    uint16_t rising_threshold = stats_off->max;
    if (rising_threshold < 2) {
        rising_threshold = 2;
    }

    uint16_t falling_threshold = lroundf(stats_off->mean + stats_off->stddev);
    if (falling_threshold < 2) {
        falling_threshold = 2;
    }

    calibration_result_t result = CALIBRATION_OK;
    meter_probe_sensor_reading_t sensor_reading = {0};
    TickType_t time_relay_on = 0;
    TickType_t time_relay_off = 0;
    TickType_t time_rise_start = 0;
    TickType_t time_rise_end = 0;
    TickType_t time_fall_start = 0;
    TickType_t time_fall_end = 0;
    uint32_t time_reading = 0;
    uint32_t integrated_rise = 0;
    uint32_t rise_counts = 0;
    uint32_t integrated_fall = 0;
    uint32_t fall_counts = 0;

    const float sensor_integration_time = tsl2585_integration_time_ms(SENSOR_SAMPLE_TIME, SENSOR_NUM_SAMPLES);
    //const uint32_t sensor_integration_ceil = lroundf(ceilf(sensor_integration_time));
    const float sensor_integration_mid = sensor_integration_time / 2.0F;

    /*
     * The reading threshold that is used to determine that the enlarger is
     * completely on is normally based on the standard deviation of the value
     * as measured during reference stats collection.
     * However, if the light source is sufficiently stable that the standard
     * deviation is less than 1% of the mean, then its safer to use that 1%
     * value instead.
     */
    uint32_t enlarger_on_threshold;
    if ((stats_on->mean / stats_on->stddev) < 0.01F) {
        enlarger_on_threshold = (uint32_t)lroundf(stats_on->mean - (stats_on->mean * 0.01F));
    } else {
        enlarger_on_threshold = (uint32_t)lroundf(stats_on->mean - stats_on->stddev);
    }


    log_i("Collecting profile data...");

    /*
     * This code used to raise the task priority to osPriorityRealtime
     * during each measurement loop. However, that was when it was in
     * complete control over the entire process. Now it depends on various
     * other tasks, and it records time based on the sensor interrupt.
     *
     * Unpredictable timing of the main task could still cause issues,
     * but the risk of those issues significantly affecting the results
     * is much lower. So for now, the task priority control has been
     * removed. If it is ever re-added, then all of the dependent tasks
     * will also need attention.
     */

    do {
        /* Activate enlarger in blocking mode, which should return just before the start of frame */
        enlarger_control_set_state_focus(enlarger_control, true);
        time_relay_on = xTaskGetTickCount();

        /* Loop until the light level starts to rise */
        do {
            /* Get the next sensor reading */
            if (meter_probe_sensor_get_next_reading(handle, &sensor_reading, SENSOR_READING_TIMEOUT) != osOK) {
                log_w("Unable to get next sensor reading");
                result = CALIBRATION_SENSOR_ERROR;
                break;
            }

            /* Calculate the start time of the reading group */
            const uint32_t reading_start_ticks = sensor_reading.ticks - lroundf(stats_sensor->mean * MAX_ALS_COUNT);

            for (unsigned int i = 0; i < MAX_ALS_COUNT; i++) {
                /* Calculate the offset of the current reading within the group */
                float element_offset = stats_sensor->mean * (float)i;

                /* Calculate the start time of the current element */
                uint32_t element_start_ceil = reading_start_ticks + lroundf(ceilf(element_offset));

                /* Skip if this element started before the time mark */
                if (element_start_ceil < time_relay_on) {
                    continue;
                }

                /* Record the reading time as the midpoint of the integration cycle */
                time_reading = reading_start_ticks + lroundf(element_offset + sensor_integration_mid);

                /* Break if the reading exceeds the rising threshold */
                if (sensor_reading.reading[i].data > rising_threshold) {
                    time_rise_start = time_reading;
                    break;
                }
            }
            if (time_rise_start > 0) { break; }

            /* Check if we've been waiting in this loop for too long */
            if (xTaskGetTickCount() - time_relay_on > MAX_LOOP_DURATION) {
                log_w("Took too long for the light to turn on");
                result = CALIBRATION_TIMEOUT;
                break;
            }
        } while (1);
        if (result != CALIBRATION_OK) { break; }

        /* Loop until the light level reaches its steady on threshold */
        do {
            /* Get the next sensor reading */
            if (meter_probe_sensor_get_next_reading(handle, &sensor_reading, SENSOR_READING_TIMEOUT) != osOK) {
                log_w("Unable to get next sensor reading");
                result = CALIBRATION_SENSOR_ERROR;
                break;
            }

            /* Calculate the start time of the reading group */
            const uint32_t reading_start_ticks = sensor_reading.ticks - lroundf(stats_sensor->mean * MAX_ALS_COUNT);

            for (unsigned int i = 0; i < MAX_ALS_COUNT; i++) {
                /* Calculate the offset of the current reading within the group */
                float element_offset = stats_sensor->mean * (float)i;

                /* Record the reading time as the midpoint of the integration cycle */
                time_reading = reading_start_ticks + lroundf(element_offset + sensor_integration_mid);

                /* Integrate all readings during the rise period */
                integrated_rise += sensor_reading.reading[i].data;
                rise_counts++;

                /* Break once we've reached the fully-on state */
                if (sensor_reading.reading[i].data >= enlarger_on_threshold) {
                    time_rise_end = time_reading;
                    break;
                }
            }
            if (time_rise_end > 0) { break; }

            if (xTaskGetTickCount() - time_rise_start > MAX_LOOP_DURATION) {
                log_w("Took too long for the light level to rise");
                result = CALIBRATION_TIMEOUT;
                break;
            }
        } while (1);
        if (result != CALIBRATION_OK) { break; }

        /* Wait a while to make sure things have stabilized */
        if (!delay_with_cancel(LIGHT_STABLIZE_WAIT_MS)) {
            result = CALIBRATION_CANCEL;
            break;
        }

        /* Deactivate enlarger in blocking mode, which should return just before the start of frame */
        enlarger_control_set_state_off(enlarger_control, true);
        time_relay_off = xTaskGetTickCount();

        /* Loop until the light level starts to fall */
        do {
            /* Get the next sensor reading */
            if (meter_probe_sensor_get_next_reading(handle, &sensor_reading, SENSOR_READING_TIMEOUT) != osOK) {
                log_w("Unable to get next sensor reading");
                result = CALIBRATION_SENSOR_ERROR;
                break;
            }

            /* Calculate the start time of the reading group */
            const uint32_t reading_start_ticks = sensor_reading.ticks - lroundf(stats_sensor->mean * MAX_ALS_COUNT);

            for (unsigned int i = 0; i < MAX_ALS_COUNT; i++) {
                /* Calculate the offset of the current reading within the group */
                float element_offset = stats_sensor->mean * (float) i;

                /* Calculate the start time of the current element */
                uint32_t element_start_ceil = reading_start_ticks + lroundf(ceilf(element_offset));

                /* Skip if this reading started before the time mark */
                if (element_start_ceil < time_relay_off) {
                    continue;
                }

                /* Record the reading time as the midpoint of the integration cycle */
                time_reading = reading_start_ticks + lroundf(element_offset + sensor_integration_mid);

                /* Break if the reading falls below the fully-on threshold */
                if (sensor_reading.reading[i].data < stats_on->min) {
                    time_fall_start = time_reading;
                    break;
                }
            }
            if (time_fall_start > 0) { break; }

            if (xTaskGetTickCount() - time_relay_off > MAX_LOOP_DURATION) {
                result = CALIBRATION_TIMEOUT;
                break;
            }
        } while (1);
        if (result != CALIBRATION_OK) { break; }

        /* Loop until the light level bottoms out */
        do {
            /* Get the next sensor reading */
            if (meter_probe_sensor_get_next_reading(handle, &sensor_reading, SENSOR_READING_TIMEOUT) != osOK) {
                log_w("Unable to get next sensor reading");
                result = CALIBRATION_SENSOR_ERROR;
                break;
            }

            /* Calculate the start time of the reading group */
            const uint32_t reading_start_ticks = sensor_reading.ticks - lroundf(stats_sensor->mean * MAX_ALS_COUNT);

            for (unsigned int i = 0; i < MAX_ALS_COUNT; i++) {
                /* Calculate the offset of the current reading within the group */
                float element_offset = stats_sensor->mean * (float) i;

                /* Record the reading time as the midpoint of the integration cycle */
                time_reading = reading_start_ticks + lroundf(element_offset + sensor_integration_mid);

                /* Integrate all readings during the fall period */
                integrated_fall += sensor_reading.reading[i].data;
                fall_counts++;

                /* Break once we've reached the fully-off state */
                if (sensor_reading.reading[i].data < falling_threshold) {
                    time_fall_end = time_reading;
                    break;
                }
            }
            if (time_fall_end > 0) { break; }

            if (xTaskGetTickCount() - time_fall_start > MAX_LOOP_DURATION) {
                result = CALIBRATION_TIMEOUT;
                break;
            }
        } while (1);
        if (result != CALIBRATION_OK) { break; }

        /* Wait for things to settle at the end */
        if (!delay_with_cancel(LIGHT_STABLIZE_WAIT_MS)) {
            result = CALIBRATION_CANCEL;
            break;
        }
    } while (0);

    if (result != CALIBRATION_OK) {
        log_w("Calibration did not finish");
        return result;
    }

    memset(timing_profile, 0, sizeof(enlarger_timing_t));
    timing_profile->turn_on_delay = (time_rise_start - time_relay_on) / portTICK_PERIOD_MS;
    timing_profile->rise_time = (time_rise_end - time_rise_start) / portTICK_PERIOD_MS;

    float rise_scale_factor = (float)integrated_rise / (stats_on->mean * rise_counts);
    timing_profile->rise_time_equiv = lroundf(timing_profile->rise_time * rise_scale_factor);

    timing_profile->turn_off_delay = (time_fall_start - time_relay_off) / portTICK_PERIOD_MS;
    timing_profile->fall_time = (time_fall_end - time_fall_start) / portTICK_PERIOD_MS;

    float fall_scale_factor = (float)integrated_fall / (stats_on->mean * fall_counts);
    timing_profile->fall_time_equiv = lroundf(timing_profile->fall_time * fall_scale_factor);

    log_i("Relay on delay: %ldms", timing_profile->turn_on_delay);
    log_i("Rise time: %ldms (full_equiv=%ldms, counts=%ld)", timing_profile->rise_time, timing_profile->rise_time_equiv, rise_counts);
    log_i("Relay off delay: %ldms", timing_profile->turn_off_delay);
    log_i("Fall time: %ldms (full_equiv=%ldms, counts=%ld)", timing_profile->fall_time, timing_profile->fall_time_equiv, fall_counts);

    return CALIBRATION_OK;
}

void calculate_reading_stats(reading_stats_t *stats, uint32_t *readings, size_t len)
{
    if (!stats || !readings) { return; }

    uint32_t reading_min = UINT16_MAX;
    uint32_t reading_max = 0;
    float reading_sum = 0;

    for (size_t i = 0; i < len; i++) {
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
    for (size_t i = 0; i < len; i++) {
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
