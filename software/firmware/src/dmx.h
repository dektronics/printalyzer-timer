#ifndef DMX_H
#define DMX_H

#include "stm32f4xx_hal.h"
#include <cmsis_os.h>

typedef enum {
    DMX_PORT_DISABLED = 0,        /*!< Port is not enabled */
    DMX_PORT_ENABLED_IDLE,        /*!< Port line drivers are enabled, but nothing is being sent */
    DMX_PORT_ENABLED_TRANSMITTING /*!< Port is enabled and sending DMX frames */
} dmx_port_state_t;

/**
 * Initialize the DMX controller port and task
 */
void task_dmx_run(void *argument);

dmx_port_state_t dmx_get_port_state();

osStatus_t dmx_enable();
osStatus_t dmx_disable();
osStatus_t dmx_start();
osStatus_t dmx_stop();
osStatus_t dmx_set_frame(uint16_t offset, const uint8_t *frame, size_t len);

/**
 * Call this function from the timer ISR
 */
void dmx_timer_notify();

/**
 * Call this function from the UART TX Complete callback
 */
void dmx_uart_tx_cplt();

#endif /* DMX_H */
