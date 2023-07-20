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

typedef enum {
    METER_READING_OK = 0,
    METER_READING_LOW,
    METER_READING_HIGH,
    METER_READING_TIMEOUT,
    METER_READING_FAIL
} meter_probe_result_t;

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
 * Enable the meter probe sensor
 */
osStatus_t meter_probe_sensor_enable();

/**
 * Disable the meter probe sensor
 */
osStatus_t meter_probe_sensor_disable();

/**
 * Meter probe interrupt handler
 *
 * This function should only be called from within the ISR for the
 * meter probe's interrupt pin.
 */
void meter_probe_int_handler();

//XXX -----------------------------------------------------------------------

/**
 * Enable the meter probe sensor so that it is ready for measurements.
 *
 * This function will block while waiting for the sensor
 * to become ready.
 */
meter_probe_result_t meter_probe_sensor_enable_old();

/**
 * Take a stable measurement, returning Lux and CCT values.
 *
 * Since accuracy under low-light conditions is the main requirement
 * for this function's implementation, the longest usable sensor
 * integration time will be used.
 *
 * This function will block while doing auto-ranging and waiting
 * for a stable reading. Since accuracy under low-light conditions
 * is the main requirement, and the longest usable sensor integration
 * time is likely to be used, this may take several seconds.
 */
meter_probe_result_t meter_probe_measure_old(float *lux, uint32_t *cct);

/**
 * Disable the meter probe sensor after a reading cycle.
 */
void meter_probe_sensor_disable_old();

#endif /* METER_PROBE_H */
