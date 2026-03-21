#ifndef DENS_REMOTE_H
#define DENS_REMOTE_H

#include "densitometer.h"

typedef enum {
    DENS_REMOTE_RESULT_OK = 0U,
    DENS_REMOTE_RESULT_INVALID,
    DENS_REMOTE_RESULT_TIMEOUT,
    DENS_REMOTE_RESULT_NOT_CONNECTED,
    DENS_REMOTE_RESULT_UNKNOWN
} dens_remote_result_t;

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
 * @return 'DENS_REMOTE_RESULT_OK' if a reading was received,
 *         otherwise an appropriate error code
 */
dens_remote_result_t dens_remote_reading_poll(densitometer_reading_t *reading);

#endif /* DENS_REMOTE_H */
