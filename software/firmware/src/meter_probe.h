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
    uint32_t raw_result;   /*!< Raw sensor reading */
    meter_probe_sensor_result_t result_status; /*!< Sensor result status. */
    tsl2585_gain_t gain;   /*!< Sensor ADC gain */
    uint16_t sample_time;  /*!< Sensor integration sample time */
    uint16_t sample_count; /*!< Sensor integration sample count */
    uint32_t ticks;        /*!< Tick time when the integration cycle finished */
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
 *
 * @return
 */
osStatus_t meter_probe_start();

/**
 * Power down the meter probe.
 */
osStatus_t meter_probe_stop();

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
 * Set the meter probe sensor's gain and integration time
 *
 * These settings are specific to the TSL2585 sensor in the current
 * meter probe.
 * The sample time and count are combined to form the integration time,
 * according to the following formula:
 * TIME(μs) = (sample_count + 1) * (sample_time + 1) * 1.388889μs
 *
 * @param gain Sensor ADC gain
 * @param sample_time Duration of each sample in an integration cycle
 * @param sample_count Number of samples in an integration cycle
 */
osStatus_t meter_probe_sensor_set_config(tsl2585_gain_t gain, uint16_t sample_time, uint16_t sample_count);

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
 * Meter probe interrupt handler
 *
 * This function should only be called from within the ISR for the
 * meter probe's interrupt pin.
 */
void meter_probe_int_handler();

#endif /* METER_PROBE_H */
