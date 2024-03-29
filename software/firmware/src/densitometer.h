/*
 * Common interface to externally attached densitometers.
 */

#ifndef DENSITOMETER_H
#define DENSITOMETER_H

#include <stdint.h>

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

typedef enum {
    DENSITOMETER_RESULT_OK = 0U,
    DENSITOMETER_RESULT_INVALID,
    DENSITOMETER_RESULT_TIMEOUT,
    DENSITOMETER_RESULT_NOT_CONNECTED,
    DENSITOMETER_RESULT_UNKNOWN
} densitometer_result_t;

/**
 * Poll for a densitometer reading from an attached USB serial device.
 *
 * This is a very simple implementation, and is intended for read-only
 * usage together with a UI loop that may also check for key events.
 * It is designed to detect any supported data format, so it does not
 * depend on device-specific configuration.
 *
 * Since this function is stateless, while the USB serial code very much
 * is not, it is advisable to clear the serial receive buffer prior
 * to entering into the loop where this function is called.
 *
 * @param reading Latest reading, if one was received.
 * @return 'DENSITOMETER_RESULT_OK' if a reading was received,
 *         otherwise an appropriate error code
 */
densitometer_result_t densitometer_reading_poll(densitometer_reading_t *reading);

/**
 * Log the provided reading for debugging purposes
 */
void densitometer_log_reading(const densitometer_reading_t *reading);

#endif /* DENSITOMETER_H */
