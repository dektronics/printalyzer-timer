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

typedef enum {
    METER_READING_OK = 0,
    METER_READING_LOW,
    METER_READING_HIGH,
    METER_READING_TIMEOUT,
    METER_READING_FAIL
} meter_probe_result_t;

/**
 * Initialize the meter probe so that it is ready for measurements.
 *
 * This function will apply power to the meter probe port, read the
 * device identification and calibration information, and verify that
 * a supported meter probe is connected.
 */
meter_probe_result_t meter_probe_initialize();

/**
 * Shutdown the meter probe.
 *
 * This function will remove power from the meter probe.
 */
void meter_probe_shutdown();

/**
 * Gets whether the meter probe has been initialized
 */
bool meter_probe_is_initialized();

/**
 * Enable the meter probe sensor so that it is ready for measurements.
 *
 * This function will block while waiting for the sensor
 * to become ready.
 */
meter_probe_result_t meter_probe_sensor_enable();

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
meter_probe_result_t meter_probe_measure(float *lux, uint32_t *cct);

/**
 * Disable the meter probe sensor after a reading cycle.
 */
void meter_probe_sensor_disable();

#endif /* METER_PROBE_H */
