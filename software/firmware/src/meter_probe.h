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
#include "densistick_settings.h"

/* Maximum number of ALS readings that can be in a result */
#define MAX_ALS_COUNT 8

typedef struct {
    meter_probe_id_t probe_id;
    uint8_t sensor_id[3];
} meter_probe_device_info_t;

typedef struct {
    meter_probe_sensor_type_t type;
    meter_probe_settings_tsl2585_t settings_tsl2585;
} meter_probe_settings_t;

typedef struct {
    meter_probe_sensor_type_t type;
    densistick_settings_tsl2585_t settings_tsl2585;
} densistick_settings_t;

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

/*
 * This structure contains a single ALS sensor result, with all of its
 * associated properties. It is separated out to allow sensor results
 * to be returned in batches in certain modes.
 */
typedef struct {
    uint32_t data; /*!< Raw ALS sensor reading value */
    meter_probe_sensor_result_t status; /*!< Sensor result status */
    tsl2585_gain_t gain; /*!< Sensor ADC gain for the reading */
} meter_probe_als_result_t;

/**
 * Meter probe reading structure.
 * Normally, it only contains a single ALS reading and its properties.
 * However, when "fast mode" is enabled, it will return MAX_ALS_COUNT
 * readings. For this set of readings, many of the properties are
 * common, and the tick time is based on when the complete set was
 * read out.
 */
typedef struct {
    meter_probe_als_result_t reading[MAX_ALS_COUNT]; /*!< Full ALS sensor reading(s) */
    uint16_t sample_time;  /*!< Sensor integration sample time */
    uint16_t sample_count; /*!< Sensor integration sample count */
    uint32_t ticks;        /*!< Tick time when the integration cycle finished */
    uint32_t elapsed_ticks;/*!< Elapsed ticks since the last sensor reading interrupt */
} meter_probe_sensor_reading_t;

typedef struct __meter_probe_handle_t meter_probe_handle_t;

/**
* Get the handle to the meter probe instance
*/
meter_probe_handle_t *meter_probe_handle();

/**
* Get the handle to the DensiStick instance
*/
meter_probe_handle_t *densistick_handle();

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
 * Gets whether the meter probe is attached
 */
bool meter_probe_is_attached(const meter_probe_handle_t *handle);

/**
 * Gets whether the meter probe has been started
 */
bool meter_probe_is_started(const meter_probe_handle_t *handle);

/**
 * Power up the meter probe and get it ready for use.
 *
 * This function will apply power to the meter probe port, and perform
 * various initialization functions to ensure that the meter probe is
 * connected and ready for use. It will not start the sensor's integration
 * cycle.
 */
osStatus_t meter_probe_start(meter_probe_handle_t *handle);

/**
 * Power down the meter probe.
 */
osStatus_t meter_probe_stop(meter_probe_handle_t *handle);

/**
 * Get the meter probe device info.
 *
 * The meter probe must be started for this function to work.
 *
 * @return osOK if data was returned
 */
osStatus_t meter_probe_get_device_info(const meter_probe_handle_t *handle, meter_probe_device_info_t *info);

/**
 * Get whether meter probe settings were successfully loaded.
 *
 * The meter probe must be started for this function to work,
 * or it will return false.
 *
 * @return True if settings are available, false if settings are unavailable.
 */
bool meter_probe_has_settings(const meter_probe_handle_t *handle);

/**
 * Get the meter probe settings.
 *
 * The meter probe must be started for this function to work.
 * If settings were not loaded, then this function will return
 * empty data.
 *
 * @return osOK if data was returned
 */
osStatus_t meter_probe_get_settings(const meter_probe_handle_t *handle, meter_probe_settings_t *settings);

/**
 * Set the meter probe settings.
 *
 * The meter probe must be started for this function to work, with
 * the sensor in a disabled state. After calling this function,
 * the meter probe should be restarted to activate the new settings.
 *
 * @return osOK if the settings were successfully stored
 */
osStatus_t meter_probe_set_settings(meter_probe_handle_t *handle, const meter_probe_settings_t *settings);

/**
 * Get the DensiStick settings.
 *
 * The DensiStick must be started for this function to work.
 * If settings were not loaded, then this function will return
 * empty data.
 *
 * @return osOK if data was returned
 */
osStatus_t densistick_get_settings(const meter_probe_handle_t *handle, densistick_settings_t *settings);

/**
 * Set the DensiStick settings.
 *
 * The DensiStick must be started for this function to work, with
 * the sensor in a disabled state. After calling this function,
 * the DensiStick should be restarted to activate the new settings.
 *
 * @return osOK if the settings were successfully stored
 */
osStatus_t densistick_set_settings(meter_probe_handle_t *handle, const densistick_settings_t *settings);

/**
 * Set the DensiStick target calibration settings.
 *
 * The DensiStick must be started for this function to work, with
 * the sensor in a disabled state. After calling this function,
 * the DensiStick should be restarted to activate the new settings.
 *
 * @return osOK if the settings were successfully stored
 */
osStatus_t densistick_set_settings_target(meter_probe_handle_t *handle, const densistick_settings_tsl2585_cal_target_t *cal_target);

/**
 * Enable the meter probe sensor.
 *
 * In the normal enabled mode, the sensor will continuously run its
 * integration cycle and make results available as they come in.
 */
osStatus_t meter_probe_sensor_enable(meter_probe_handle_t *handle);

/**
 * Enable the meter probe sensor in fast mode.
 *
 * In fast mode, sensor interrupt handling behavior is changed to allow for
 * faster integration times by collecting readings in batches and by reducing
 * the number of device transactions necessary to receive those batches.
 *
 * In this mode, the reading structure will contain multiple actual sensor
 * readings and will be timestamped based on when the whole batch was read
 * out.
 *
 * This mode may also result in less robust handling of sensor errors,
 * and is primarily intended for dedicated time-critical measurements.
 */
osStatus_t meter_probe_sensor_enable_fast_mode(meter_probe_handle_t *handle);

/**
 * Enable the meter probe sensor in single-shot mode.
 *
 * In single-shot mode, the sensor will only run its integration cycle
 * when explicitly triggered.
 */
osStatus_t meter_probe_sensor_enable_single_shot(meter_probe_handle_t *handle);

/**
 * Disable the meter probe sensor.
 */
osStatus_t meter_probe_sensor_disable(meter_probe_handle_t *handle);

/**
 * Set the meter probe sensor's gain
 *
 * @param gain Sensor ADC gain
 */
osStatus_t meter_probe_sensor_set_gain(meter_probe_handle_t *handle, tsl2585_gain_t gain);

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
osStatus_t meter_probe_sensor_set_integration(meter_probe_handle_t *handle, uint16_t sample_time, uint16_t sample_count);

/**
 * Set the modulator calibration repetition rate.
 *
 * The following values are supported:
 * - 0x00 - Never
 * - 0x01-0xFE - Every nth time
 * - 0xFF - Only once at startup
 *
 * Note that modulator calibration adds approximately 8ms to the length of
 * an integration cycle, so it should only be used when fast timing is
 * not required.
 *
 * @param value Repetition rate to set
 * @return osOK if the value was successfully set
 */
osStatus_t meter_probe_sensor_set_mod_calibration(meter_probe_handle_t *handle, uint8_t value);

/**
 * Enable the meter probe sensor's AGC function
 *
 * @param sample_count Number of samples in an AGC integration cycle
 */
osStatus_t meter_probe_sensor_enable_agc(meter_probe_handle_t *handle, uint16_t sample_count);

/**
 * Disable the meter probe sensor's AGC function
 */
osStatus_t meter_probe_sensor_disable_agc(meter_probe_handle_t *handle);

/**
 * Enable the DensiStick light source
 */
osStatus_t densistick_set_light_enable(meter_probe_handle_t *handle, bool enable);

/**
 * Get whether the DensiStick light source is enabled
 */
bool densistick_get_light_enable(meter_probe_handle_t *handle);

/**
 * Change the DensiStick light source brightness
 *
 * @value Brightness value, where 0 is maximum and 127 is minimum.
 */
osStatus_t densistick_set_light_brightness(meter_probe_handle_t *handle, uint8_t value);

/**
 * Get the current DensiStick light source brightness
 */
uint8_t densistick_get_light_brightness(const meter_probe_handle_t *handle);

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
osStatus_t meter_probe_sensor_trigger_next_reading(meter_probe_handle_t *handle);

/**
 * Clear the last sensor reading.
 *
 * This is so we can explicitly block on the next reading in
 * single shot mode.
 */
osStatus_t meter_probe_sensor_clear_last_reading(meter_probe_handle_t *handle);

/**
 * Get the next reading from the meter probe sensor.
 * If no reading is currently available, then this function will block
 * until the completion of the next sensor integration cycle.
 *
 * @param reading Sensor reading data
 * @param timeout Amount of time to wait for a reading to become available
 * @return osOK on success, osErrorTimeout on timeout
 */
osStatus_t meter_probe_sensor_get_next_reading(meter_probe_handle_t *handle, meter_probe_sensor_reading_t *reading, uint32_t timeout);

/**
 * High level function to get a light reading in lux.
 *
 * This samples a running sensor across several cycles,
 * wraps all the error and range handling behind a simpler
 * interface, and returns the result of the lux calculation.
 */
meter_probe_result_t meter_probe_measure(meter_probe_handle_t *handle, float *lux);

/**
 * High level function to get a quick light reading in lux.
 *
 * This returns a similar result to `meter_probe_measure`
 * except that it only returns data from the most recent
 * sensor cycle and does not block.
 */
meter_probe_result_t meter_probe_try_measure(meter_probe_handle_t *handle, float *lux);

/**
* Get a reflection density reading from the DensiStick
*
* This function runs the standard measurement cycle for taking density
* measurements.
*/
meter_probe_result_t densistick_measure(meter_probe_handle_t *handle, float *density, float *raw_reading);

/**
 * Get the result in a gain and integration time adjusted format.
 *
 * This is the number that should be used for all calculations based on
 * a sensor reading, outside of specific diagnostic functions.
 *
 * @param sensor_reading Sensor reading data
 * @return Basic count result value
 */
float meter_probe_basic_result(const meter_probe_handle_t *handle, const meter_probe_sensor_reading_t *sensor_reading);

/**
 * Get the result in lux units, after applying all relevant calibration.
 *
 * This is the number that should be used for all actual light measurements
 * in the print metering path.
 *
 * @param sensor_reading Sensor reading data
 * @return Calibrated lux value
 */
float meter_probe_lux_result(const meter_probe_handle_t *handle, const meter_probe_sensor_reading_t *sensor_reading);

#endif /* METER_PROBE_H */
