/*
 * Common interface to externally attached densitometers.
 */

#ifndef DENSITOMETER_H
#define DENSITOMETER_H

#include <stdbool.h>

typedef enum {
    DENSITOMETER_MODE_UNKNOWN = 0U,
    DENSITOMETER_MODE_TRANSMISSION,
    DENSITOMETER_MODE_REFLECTION
} densitometer_mode_t;

typedef struct {
    densitometer_mode_t mode;
    float visual;
    float red;
    float green;
    float blue;
} densitometer_reading_t;

/**
 * Enable receiving densitometer readings.
 *
 * This function does the necessary setup for receiving densitometer readings,
 * and is designed so that it can be called repeatedly without undesired
 * behavior.
 *
 * If the mode is set to transmission, then only remote densitometers will be
 * enabled. Received readings may be of either the transmission or unknown types.
 *
 * If the mode is set to reflection, then both remote densitometers and the
 * DensiStick will be enabled. Received readings may be of either the reflection
 * or unknown types.
 *
 * If the mode is set to unknown, then both remote densitometers and the DensiStick
 * will be enabled, and received readings may be of any type.
 *
 * @param mode The types of readings to enable.
 */
void densitometer_enable(densitometer_mode_t mode);

/**
 * Enable or disable the densitometer idle light.
 *
 * This currently only works with the DensiStick, and is for use in cases where we
 * want to turn the light on and off without actually having to disable or re-enable
 * the device.
 */
void densitometer_idle_light(bool enabled);

/**
 * Disable receiving densitometer readings.
 */
void densitometer_disable();

/**
 * Explicitly take a densitometer reading from the DensiStick.
 *
 * This function is necessary because DensiStick readings happen in response to
 * button presses that are controlled from outside of this module.
 */
void densitometer_take_reading();

/**
 * Clear any previous readings or buffered remote data.
 */
void densitometer_clear_reading();

/**
 * Get the most recent densitometer reading.
 *
 * This will return the most recent DensiStick reading, if available, and then
 * poll for a recent remote densitometer reading. Readings that don't match the
 * enabled types will not be returned.
 *
 * @return True if a reading was returned, false otherwise.
 */
bool densitometer_get_reading(densitometer_reading_t *reading);

/**
 * Log the provided reading for debugging purposes
 */
void densitometer_log_reading(const densitometer_reading_t *reading);

#endif /* DENSITOMETER_H */
