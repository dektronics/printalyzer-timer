#ifndef DMX_H
#define DMX_H

#include "stm32f4xx_hal.h"
#include <cmsis_os.h>
#include <stdbool.h>

typedef enum {
    DMX_PORT_DISABLED = 0,        /*!< Port is not enabled */
    DMX_PORT_ENABLED_IDLE,        /*!< Port line drivers are enabled, but nothing is being sent */
    DMX_PORT_ENABLED_TRANSMITTING,/*!< Port is enabled and sending DMX frames */
} dmx_port_state_t;

/**
 * Initialize the DMX controller port and task
 */
void task_dmx_run(void *argument);

dmx_port_state_t dmx_get_port_state();

/**
 * Enable the DMX control port, putting it into an idle state.
 *
 * This will enable the transmit line drivers on the port, but will
 * not send any data.
 *
 * This function will block until it has taken effect.
 */
osStatus_t dmx_enable();

/**
 * Disable the DMX control port, releasing any control.
 *
 * This function will block until it has taken effect.
 */
osStatus_t dmx_disable();

/**
 * Start sending data frames out the DMX control port.
 *
 * This will begin the process of sending frames at a regular interval.
 *
 * This function will block until it has taken effect.
 */
osStatus_t dmx_start();

/**
 * Stop sending data frames out the DMX control port.
 *
 * This function will clear the active frame contents.
 *
 * This function will block until it has taken effect.
 */
osStatus_t dmx_stop();

/**
 * Pause automatically sending frames out the DMX control port.
 *
 * This function will not clear the active frame contents,
 * and sending can be resumed by calling 'dmx_start()'.
 *
 * This function will block until it has taken effect.
 */
osStatus_t dmx_pause();

/**
 * Set the value of the current DMX data frame.
 *
 * The underlying frame is up to 512 bytes long, so this function provides
 * an interface for altering a subset of that frame.
 *
 * @param offset Position within the frame to update.
 * @param frame Data within the frame to update
 * @param len Length of the data to be updated
 * @param blocking True to block until the update takes effect, false to return immediately.
 */
osStatus_t dmx_set_frame(uint16_t offset, const uint8_t *frame, size_t len, bool blocking);

/**
 * Set the value of specific slots in the current DMX data frame.
 *
 * The underlying frame is up to 512 bytes long, so this function provides
 * an interface for altering a subset of that frame.
 *
 * @param channels Array of channels to update
 * @param values Corresponding array of values to write
 * @param len Length of both arrays
 * @param blocking True to block until the update takes effect, false to return immediately.
 */
osStatus_t dmx_set_sparse_frame(const uint16_t *channels, const uint8_t *values, size_t len, bool blocking);

/**
 * Clear the current DMX frame.
 *
 * This is a convenience function to zero out the current frame from an unknown
 * state prior to setting it with an explicit new value.
 *
 * @param blocking True to block until the update takes effect, false to return immediately.
 */
osStatus_t dmx_clear_frame(bool blocking);

/**
 * Enable the direct updating of the active frame.
 *
 * This function enables the circumvention of any task synchronization,
 * and should only be used when in the IDLE state and intending to
 * explicitly send frames via the 'dmx_send_frame_from_isr()' function.
 *
 * A call to any function that changes the port state will disable
 * this feature.
 */
void dmx_enable_direct_frame_update();

/**
 * Explicitly send the next DMX frame.
 *
 * This function is intended for use when the timing of frame sending
 * is being controlled externally.
 * It may fail silently if called more frequently than the frame period.
 */
void dmx_send_frame_explicit();

/**
 * Call this function from the timer ISR
 */
void dmx_timer_notify();

/**
 * Call this function from the UART TX Complete callback
 */
void dmx_uart_tx_cplt();

#endif /* DMX_H */
