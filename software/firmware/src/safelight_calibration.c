#include "safelight_calibration.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <FreeRTOS.h>
#include <task.h>
#include <cmsis_os.h>

#define LOG_TAG "safelight_calibration"
#include <elog.h>

#include "main_menu.h"
#include "display.h"
#include "keypad.h"
#include "settings.h"
#include "meter_probe.h"
#include "tsl2585.h"
#include "illum_controller.h"
#include "relay.h"
#include "dmx.h"

#define LIGHT_STABLIZE_WAIT_MS       (5000U)
#define REFERENCE_READING_ITERATIONS (12U)
#define PROFILE_ITERATIONS           (5U)
#define MAX_LOOP_DURATION            pdMS_TO_TICKS(10000)

/* Sensor settings for a 50ms integration time */
#define SENSOR_SAMPLE_TIME 719
#define SENSOR_NUM_SAMPLES 49

#define SENSOR_READING_TIMEOUT (200U)

/* Minimum value we'll accept for a non-zero reading */
#define SENSOR_ZERO_THRESHOLD (5U)

/* Number of cycles we must record the light as off to accept the result */
#define FALL_COUNT_THRESHOLD 5

static const char *DISPLAY_TITLE = "Safelight Calibration";

/*
 * The safelight calibration process works by measuring the light output
 * of the safelights over time, while running through a series of simulated
 * on/off cycles. This is to measure how long it actually takes for the
 * safelights to fully turn off once commanded to do so.
 */

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

static calibration_result_t safelight_calibration_start(const safelight_config_t *config, reading_stats_t *turn_off_delay);
static void safelight_calibration_preview_result(const reading_stats_t *turn_off_delay, uint32_t safe_value);
static void safelight_calibration_show_error(calibration_result_t result_error);
static calibration_result_t calibration_collect_reference_stats(const safelight_config_t *config,
    reading_stats_t *stats_on, reading_stats_t *stats_off);
static calibration_result_t calibration_validate_reference_stats(
    const reading_stats_t *stats_on, const reading_stats_t *stats_off);
static calibration_result_t calibration_build_timing_profile(const safelight_config_t *config,
    const reading_stats_t *stats_on, const reading_stats_t *stats_off,
    uint32_t *turn_off_delay);

static void calculate_reading_stats(reading_stats_t *stats, uint32_t *readings, size_t len);
static bool delay_with_cancel(uint32_t time_ms);

menu_result_t menu_safelight_calibration(safelight_config_t *config)
{
    menu_result_t menu_result = MENU_CANCEL;

    menu_safelight_test_enable(config, true);
    menu_safelight_test_toggle(config, true);

    /* If we are calibrating DMX controlled safelights, then make sure the DMX task is ready */
    if (config->control == SAFELIGHT_CONTROL_DMX || config->control == SAFELIGHT_CONTROL_BOTH) {
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
        "\nMake sure your safelight is on\n"
        "and your meter probe is\n"
        "connected with a clear view of\n"
        "the safelight.",
        " Start ");

    if (option == 1) {
        reading_stats_t turn_off_delay = {};
        calibration_result_t calibration_result = safelight_calibration_start(config, &turn_off_delay);
        if (calibration_result == CALIBRATION_OK) {
            /* Round max delay up to the nearest multiple of 10ms and add 50ms for comfort */
            const uint32_t safe_value = (((turn_off_delay.max + 10 - 1) / 10) * 10) + 50;;
            safelight_calibration_preview_result(&turn_off_delay, safe_value);
            config->turn_off_delay = safe_value;
            menu_result = MENU_OK;
        } else if (calibration_result == CALIBRATION_CANCEL) {
            menu_result = MENU_CANCEL;
        } else {
            safelight_calibration_show_error(calibration_result);
            menu_result = MENU_CANCEL;
        }
        menu_safelight_test_toggle(config, false);
    } else if (option == UINT8_MAX) {
        menu_result = MENU_TIMEOUT;
    }

    /* If we are calibrating DMX controlled safelights, then make sure the DMX frame is clean afterwards */
    if (config->control == SAFELIGHT_CONTROL_DMX || config->control == SAFELIGHT_CONTROL_BOTH) {
        /* Clear DMX output frame */
        if (dmx_get_port_state() == DMX_PORT_ENABLED_TRANSMITTING) {
            dmx_stop();
        }
    }

    menu_safelight_test_enable(config, false);

    illum_controller_refresh();
    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

    return menu_result;
}

calibration_result_t safelight_calibration_start(const safelight_config_t *config, reading_stats_t *turn_off_delay)
{
    calibration_result_t calibration_result;
    log_i("Starting safelight calibration process");

    meter_probe_handle_t *handle = meter_probe_handle();

    display_static_list(DISPLAY_TITLE, "\n\nInitializing...");

    /* Turn everything off, just in case it isn't already off */
    menu_safelight_test_toggle(config, false);

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

    reading_stats_t safelight_on_stats;
    reading_stats_t safelight_off_stats;
    calibration_result = calibration_collect_reference_stats(config,
        &safelight_on_stats, &safelight_off_stats);
    if (calibration_result != CALIBRATION_OK) {
        log_e("Could not collect reference stats");
        meter_probe_stop(handle);
        menu_safelight_test_toggle(config, false);
        return calibration_result;
    }

    /* Log statistics used for reference points */
    log_i("Safelight on");
    log_i("Mean = %.1f, Min = %d, Max = %d, StdDev = %.1f",
        safelight_on_stats.mean, safelight_on_stats.min, safelight_on_stats.max,
        safelight_on_stats.stddev);
    log_i("Safelight off");
    log_i("Mean = %.1f, Min = %d, Max = %d, StdDev = %.1f",
        safelight_off_stats.mean, safelight_off_stats.min, safelight_off_stats.max,
        safelight_off_stats.stddev);

    calibration_result = calibration_validate_reference_stats(&safelight_on_stats, &safelight_off_stats);
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
    float turn_off_delay_sum = 0;
    uint32_t turn_off_delay_min = 0;
    uint32_t turn_off_delay_max = 0;

    for (unsigned int i = 0; i < PROFILE_ITERATIONS; i++) {
        char buf[64];
        uint32_t turn_off_delay_inc;
        log_i("Profile run %d...", i + 1);

        sprintf(buf, "\nProfiling safelight...\n\nCycle %d of %d", i + 1, PROFILE_ITERATIONS);
        display_static_list(DISPLAY_TITLE, buf);

        calibration_result = calibration_build_timing_profile(config,
            &safelight_on_stats, &safelight_off_stats,
            &turn_off_delay_inc);
        if (calibration_result != CALIBRATION_OK) {
            log_e("Could not build profile");
            meter_probe_stop(handle);
            illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);
            return calibration_result;
        }

        turn_off_delay_sum += logf((float)turn_off_delay_inc);
        if (i == 0) {
            turn_off_delay_min = turn_off_delay_inc;
            turn_off_delay_max = turn_off_delay_inc;
        } else {
            if (turn_off_delay_inc < turn_off_delay_min) { turn_off_delay_min = turn_off_delay_inc; }
            if (turn_off_delay_inc > turn_off_delay_max) { turn_off_delay_max = turn_off_delay_inc; }
        }
    }
    log_i("Profile runs complete");

    meter_probe_stop(handle);
    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

    /* Geometric mean across all the runs */
    turn_off_delay->mean = expf(turn_off_delay_sum / (float)PROFILE_ITERATIONS);
    turn_off_delay->min = turn_off_delay_min;
    turn_off_delay->max = turn_off_delay_max;

    log_i("Turn off delay: %ldms (%ldms..%ldms)", lroundf(turn_off_delay->mean), turn_off_delay->min, turn_off_delay->max);

    return CALIBRATION_OK;
}

void safelight_calibration_preview_result(const reading_stats_t *turn_off_delay, uint32_t safe_value)
{
    char buf[256];
    sprintf(buf, "\n"
        "Turn off delay:\n"
        "Average: %ldms\n"
        "Range: %ldms..%ldms\n"
        "Safe value: %ldms",
        lroundf(turn_off_delay->mean),
        turn_off_delay->min, turn_off_delay->max,
        safe_value);
    do {
        uint8_t option = display_message(
            "Safelight Calibration Done",
            NULL, buf,
            " Done ");
        if (option == 1) {
            break;
        }
    } while (1);
}

void safelight_calibration_show_error(calibration_result_t result_error)
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
            "positioned or the safelight is\n"
            "not turning on.";
        break;
    case CALIBRATION_SENSOR_SATURATED:
        msg =
            "The safelight is too bright\n"
            "for calibration.";
        break;
    case CALIBRATION_INVALID_REFERENCE_STATS:
        msg =
            "Could not detect a large enough\n"
            "brightness difference between\n"
            "the safelight being on and off.";
        break;
    case CALIBRATION_TIMEOUT:
        msg =
            "The safelight took too long to\n"
            "respond while being measured.";
        break;
    case CALIBRATION_FAIL:
    default:
        msg =
            "Unable to complete\n"
            "safelight calibration.";
        break;
    }

    do {
        uint8_t option = display_message(
            "Safelight Calibration Failed\n",
            NULL, msg, " OK ");
        if (option == 1) { break; }
    } while (1);
}

calibration_result_t calibration_collect_reference_stats(const safelight_config_t *config,
    reading_stats_t *stats_on, reading_stats_t *stats_off)
{
    meter_probe_sensor_reading_t sensor_reading = {0};
    tsl2585_gain_t sensor_gain;
    uint32_t readings[REFERENCE_READING_ITERATIONS];
    uint32_t index;
    meter_probe_handle_t *handle = meter_probe_handle();

    if (!stats_on || !stats_off) { return CALIBRATION_FAIL; }

    log_i("Turning safelight on for baseline reading");
    menu_safelight_test_toggle(config, true);

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
    if (meter_probe_sensor_enable(handle) != osOK) {
        return CALIBRATION_SENSOR_ERROR;
    }

    log_i("Waiting for light to stabilize");
    if (!delay_with_cancel(LIGHT_STABLIZE_WAIT_MS)) {
        return CALIBRATION_CANCEL;
    }

    log_i("Finding appropriate gain setting");
    if (meter_probe_sensor_get_next_reading(handle, &sensor_reading, SENSOR_READING_TIMEOUT * 4) != osOK) {
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
    if (meter_probe_sensor_enable(handle) != osOK) {
        return CALIBRATION_SENSOR_ERROR;
    }

    /* Skip a few integration cycles, just to be safe */
    osDelay(100);

    log_i("Collecting data with safelight on");
    index = 0;
    for (unsigned int i = 0; i < REFERENCE_READING_ITERATIONS; i++) {
        if (meter_probe_sensor_get_next_reading(handle, &sensor_reading, SENSOR_READING_TIMEOUT) != osOK) {
            return CALIBRATION_SENSOR_ERROR;
        }
        readings[index++] = sensor_reading.reading[0].data;
    }

    /* Turn the safelight off */
    menu_safelight_test_toggle(config, false);

    log_i("Computing baseline safelight reading statistics");
    calculate_reading_stats(stats_on, readings, REFERENCE_READING_ITERATIONS);

    log_i("Waiting for light to stabilize");
    if (!delay_with_cancel(LIGHT_STABLIZE_WAIT_MS / 2)) {
        return CALIBRATION_TIMEOUT;
    }

    log_i("Collecting data with safelight off");

    /* Capture a reading as the starting point for sensor cycle measurement */
    if (meter_probe_sensor_get_next_reading(handle, &sensor_reading, SENSOR_READING_TIMEOUT) != osOK) {
        return CALIBRATION_SENSOR_ERROR;
    }

    index = 0;
    for (unsigned int i = 0; i < REFERENCE_READING_ITERATIONS; i++) {
        if (meter_probe_sensor_get_next_reading(handle, &sensor_reading, SENSOR_READING_TIMEOUT) != osOK) {
            return CALIBRATION_SENSOR_ERROR;
        }
        readings[index++] = sensor_reading.reading[0].data;
    }

    log_i("Computing baseline darkness reading statistics");
    calculate_reading_stats(stats_off, readings, REFERENCE_READING_ITERATIONS);

    return CALIBRATION_OK;
}

calibration_result_t calibration_validate_reference_stats(
    const reading_stats_t *stats_on, const reading_stats_t *stats_off)
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

calibration_result_t calibration_build_timing_profile(const safelight_config_t *config,
    const reading_stats_t *stats_on, const reading_stats_t *stats_off,
    uint32_t *turn_off_delay)
{
    if (!config || !turn_off_delay || !stats_on || !stats_off) {
        return CALIBRATION_FAIL;
    }
    meter_probe_handle_t *handle = meter_probe_handle();

    /* Set the rising threshold to a point part-way between fully off and fully on */
    uint32_t rising_threshold = stats_off->max + lroundf((float)(stats_on->min - stats_off->max) * 0.20F);
    if (rising_threshold < 2) {
        rising_threshold = 2;
    }

    /* Set the falling threshold to one standard deviation above the mean off value */
    uint32_t falling_threshold = lroundf(stats_off->mean + stats_off->stddev);
    if (falling_threshold < 2) {
        falling_threshold = 2;
    }

    calibration_result_t result = CALIBRATION_OK;
    meter_probe_sensor_reading_t sensor_reading = {0};
    TickType_t time_toggle_on = 0;
    TickType_t time_toggle_off = 0;
    TickType_t time_fall_end = 0;
    uint8_t fall_counter = 0;
    uint8_t fall_reset_count = 0;

    log_i("Collecting profile data...");

    do {
        /* Activate the safelights */
        time_toggle_on = xTaskGetTickCount();
        menu_safelight_test_toggle(config, true);

        /* Loop until the light level starts to rise */
        do {
            /* Get the next sensor reading */
            if (meter_probe_sensor_get_next_reading(handle, &sensor_reading, SENSOR_READING_TIMEOUT) != osOK) {
                log_w("Unable to get next sensor reading");
                result = CALIBRATION_SENSOR_ERROR;
                break;
            }

            /* Break if the reading exceeds the rising threshold */
            if (sensor_reading.reading[0].data > rising_threshold) {
                break;
            }

            /* Check if we've been waiting in this loop for too long */
            if (xTaskGetTickCount() - time_toggle_on > MAX_LOOP_DURATION) {
                log_w("Took too long for the light to turn on");
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

        /* Deactivate the safelights */
        time_toggle_off = xTaskGetTickCount();
        menu_safelight_test_toggle(config, false);

        /* Loop until the light level bottoms out */
        do {
            /* Get the next sensor reading */
            if (meter_probe_sensor_get_next_reading(handle, &sensor_reading, SENSOR_READING_TIMEOUT) != osOK) {
                log_w("Unable to get next sensor reading");
                result = CALIBRATION_SENSOR_ERROR;
                break;
            }

            /* Track each cycle where we're now in the fully-off state */
            if (sensor_reading.reading[0].data < falling_threshold) {
                if (fall_counter == 0) {
                    time_fall_end = sensor_reading.ticks;
                }
                fall_counter++;
            } else {
                fall_counter = 0;
                fall_reset_count++;
            }

            /* Break once we've been in the fully-off state for several integration cycles */
            if (fall_counter >= FALL_COUNT_THRESHOLD) {
                break;
            }

            if (xTaskGetTickCount() - time_toggle_off > MAX_LOOP_DURATION) {
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

    } while (0);

    if (result != CALIBRATION_OK) {
        log_w("Calibration did not finish");
        return result;
    }

    *turn_off_delay = (time_fall_end - time_toggle_off) / portTICK_PERIOD_MS;
    log_i("Turn off delay: %ldms", *turn_off_delay);
    log_i("Reset fall counter %d times", fall_reset_count);

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
                log_w("Canceling safelight calibration");
                return false;
            }
        }
    } while ((xTaskGetTickCount() - tick_start) < tick_time);

    return true;
}

void menu_safelight_test_enable(const safelight_config_t *config, bool enable)
{
    const bool has_dmx = config->control == SAFELIGHT_CONTROL_DMX || config->control == SAFELIGHT_CONTROL_BOTH;

    if (enable) {
        if (has_dmx) {
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
        menu_safelight_test_toggle(config, false);
    } else {
        menu_safelight_test_toggle(config, false);

        if (has_dmx) {
            /* Clear DMX output frame */
            if (dmx_get_port_state() == DMX_PORT_ENABLED_TRANSMITTING) {
                dmx_stop();
            }
        }
    }
}

void menu_safelight_test_toggle(const safelight_config_t *config, bool on)
{
    /*
     * This is mostly a duplicate of an illum_controller function,
     * modified to work off an unsaved configuration.
     */

    const bool toggle_relay = config->control == SAFELIGHT_CONTROL_RELAY || config->control == SAFELIGHT_CONTROL_BOTH;
    const bool toggle_dmx = config->control == SAFELIGHT_CONTROL_DMX || config->control == SAFELIGHT_CONTROL_BOTH;

    /* Make sure the safelight relay is turned off if not enabled */
    if (!toggle_relay && relay_safelight_is_enabled()) {
        relay_safelight_enable(false);
    }

    /* Toggle the relay, if configured */
    if (toggle_relay) {
        relay_safelight_enable(on);
    }

    /* Toggle the DMX output channel, if configured */
    if (toggle_dmx) {
        uint8_t frame[2] = {0};
        if (config->dmx_wide_mode) {
            if (on) {
                conv_u16_array(frame, config->dmx_on_value);
            }
            dmx_set_frame(config->dmx_address, frame, 2, false);
        } else {
            if (on) {
                frame[0] = (uint8_t)config->dmx_on_value;
            }
            dmx_set_frame(config->dmx_address, frame, 1, false);
        }
    }
}
