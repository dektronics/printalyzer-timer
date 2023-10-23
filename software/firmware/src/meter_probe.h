#ifndef METER_PROBE_H
#define METER_PROBE_H

/**
 * Front-end interface to drive the meter probe sensor for standard
 * measurements. May also serve as an abstraction layer if different
 * sensor or meter types are used in the future.
 *
 * The button on the meter probe is handled through the keypad
 * interface, however it may not function unless the meter probe
 * has been powered up by the initialize function here.
 */

#include <stdint.h>
#include <stdbool.h>
#include <cmsis_os.h>

#include "tsl2585.h"
#include "meter_probe_settings.h"

typedef struct {
    meter_probe_id_t probe_id;
    uint8_t sensor_id[3];
} meter_probe_device_info_t;

typedef struct {
    meter_probe_type_t type;
    meter_probe_settings_tsl2585_t settings_tsl2585;
} meter_probe_settings_t;

typedef enum {
    METER_READING_OK = 0,
    METER_READING_LOW,
    METER_READING_HIGH,
    METER_READING_TIMEOUT,
    METER_READING_FAIL
} meter_probe_result_t;

typedef enum {
    METER_SENSOR_RESULT_INVALID = 0,
    METER_SENSOR_RESULT_VALID,
    METER_SENSOR_RESULT_SATURATED_ANALOG,
    METER_SENSOR_RESULT_SATURATED_DIGITAL
} meter_probe_sensor_result_t;

typedef struct {
    uint16_t raw_result;   /*!< Raw sensor reading */
    uint16_t scale;        /*!< Scale factor for the raw result */
    meter_probe_sensor_result_t result_status; /*!< Sensor result status. */
    tsl2585_gain_t gain;   /*!< Sensor ADC gain */
    uint16_t sample_time;  /*!< Sensor integration sample time */
    uint16_t sample_count; /*!< Sensor integration sample count */
    uint32_t ticks;        /*!< Tick time when the integration cycle finished */
    uint32_t elapsed_ticks;/*!< Elapsed ticks since the last sensor reading interrupt */
} meter_probe_sensor_reading_t;

/**
 * Start the meter probe task.
 *
 * This function simply initializes all of the task state variables
 * and starts the task event loop. It does not perform any meter probe
 * power up or initialization.
 *
 * @param argument The osSemaphoreId_t used to synchronize task startup.
 */
void task_meter_probe_run(void *argument);

/**
 * Gets whether the meter probe has been started
 */
bool meter_probe_is_started();

/**
 * Power up the meter probe and get it ready for use.
 *
 * This function will apply power to the meter probe port, and perform
 * various initialization functions to ensure that the meter probe is
 * connected and ready for use. It will not start the sensor's integration
 * cycle.
 */
osStatus_t meter_probe_start();

/**
 * Power down the meter probe.
 */
osStatus_t meter_probe_stop();

/**
 * Get the meter probe device info.
 *
 * The meter probe must be started for this function to work.
 *
 * @return osOK if data was returned
 */
osStatus_t meter_probe_get_device_info(meter_probe_device_info_t *info);

/**
 * Get whether meter probe settings were successfully loaded.
 *
 * The meter probe must be started for this function to work,
 * or it will return false.
 *
 * @return True if settings are available, false if settings are unavailable.
 */
bool meter_probe_has_settings();

/**
 * Get the meter probe settings.
 *
 * The meter probe must be started for this function to work.
 * If settings were not loaded, then this function will return
 * empty data.
 *
 * @return osOK if data was returned
 */
osStatus_t meter_probe_get_settings(meter_probe_settings_t *settings);

/**
 * Set the meter probe settings.
 *
 * The meter probe must be started for this function to work, with
 * the sensor in a disabled state. After calling this function,
 * the meter probe should be restarted to activate the new settings.
 *
 * @return osOK if the settings were successfully stored
 */
osStatus_t meter_probe_set_settings(const meter_probe_settings_t *settings);

/**
 * Enable oscillator calibration for the meter probe sensor.
 *
 * The meter probe must be started for this function to work, with
 * the sensor in a disabled state. This function sets an internal flag
 * which causes oscillator calibration to be performed as part of the
 * sensor enable process.
 *
 * @return osOK if the flag was successfully set
 */
osStatus_t meter_probe_sensor_enable_osc_calibration();

/**
 * Disable oscillator calibration for the meter probe sensor.
 */
osStatus_t meter_probe_sensor_disable_osc_calibration();

/**
 * Enable the meter probe sensor.
 *
 * In the normal enabled mode, the sensor will continuously run its
 * integration cycle and make results available as they come in.
 */
osStatus_t meter_probe_sensor_enable();

/**
 * Enable the meter probe sensor in single-shot mode.
 *
 * In single-shot mode, the sensor will only run its integration cycle
 * when explicitly triggered.
 */
osStatus_t meter_probe_sensor_enable_single_shot();

/**
 * Disable the meter probe sensor.
 */
osStatus_t meter_probe_sensor_disable();

/**
 * Set the meter probe sensor's gain
 *
 * @param gain Sensor ADC gain
 */
osStatus_t meter_probe_sensor_set_gain(tsl2585_gain_t gain);

/**
 * Set the meter probe sensor's integration time
 *
 * The sample time and count are combined to form the integration time,
 * according to the following formula:
 * TIME(μs) = (sample_count + 1) * (sample_time + 1) * 1.388889μs
 *
 * @param sample_time Duration of each sample in an integration cycle
 * @param sample_count Number of samples in an integration cycle
 */
osStatus_t meter_probe_sensor_set_integration(uint16_t sample_time, uint16_t sample_count);

/**
 * Enable the meter probe sensor's AGC function
 *
 * @param sample_count Number of samples in an AGC integration cycle
 */
osStatus_t meter_probe_sensor_enable_agc(uint16_t sample_count);

/**
 * Disable the meter probe sensor's AGC function
 */
osStatus_t meter_probe_sensor_disable_agc();

/**
 * Trigger the meter probe sensor's next sensing cycle.
 *
 * This function only works in single-shot mode, and only when
 * a cycle is not currently in progress.
 *
 * Note: There appears to be a 10-20ms delay, on top of the sensor's
 * integration time, between the trigger and a new reading becoming
 * available.
 */
osStatus_t meter_probe_sensor_trigger_next_reading();

/**
 * Clear the last sensor reading.
 *
 * This is so we can explicitly block on the next reading in
 * single shot mode.
 */
osStatus_t meter_probe_sensor_clear_last_reading();

/**
 * Get the next reading from the meter probe sensor.
 * If no reading is currently available, then this function will block
 * until the completion of the next sensor integration cycle.
 *
 * @param reading Sensor reading data
 * @param timeout Amount of time to wait for a reading to become available
 * @return osOK on success, osErrorTimeout on timeout
 */
osStatus_t meter_probe_sensor_get_next_reading(meter_probe_sensor_reading_t *reading, uint32_t timeout);

/**
 * High level function to get a light reading in lux.
 *
 * This samples a running sensor across several cycles,
 * wraps all the error and range handling behind a simpler
 * interface, and returns the result of the lux calculation.
 */
meter_probe_result_t meter_probe_measure(float *lux);

/**
 * Get the raw result with the scaling factor applied.
 * @param sensor_reading Sensor reading data
 * @return Scaled raw result
 */
uint32_t meter_probe_scaled_result(const meter_probe_sensor_reading_t *sensor_reading);

/**
 * Get the result in a gain and integration time adjusted format.
 *
 * This is the number that should be used for all calculations based on
 * a sensor reading, outside of specific diagnostic functions.
 *
 * @param sensor_reading Sensor reading data
 * @return Basic count result value
 */
float meter_probe_basic_result(const meter_probe_sensor_reading_t *sensor_reading);

/**
 * Get the result in lux units, after applying all relevant calibration.
 *
 * This is the number that should be used for all actual light measurements
 * in the print metering path.
 *
 * @param sensor_reading Sensor reading data
 * @return Calibrated lux value
 */
float meter_probe_lux_result(const meter_probe_sensor_reading_t *sensor_reading);

/**
 * Meter probe interrupt handler
 *
 * This function should only be called from within the ISR for the
 * meter probe's interrupt pin.
 */
void meter_probe_int_handler();

#endif /* METER_PROBE_H */
