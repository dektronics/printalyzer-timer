#ifndef DMX_H
#define DMX_H

#include "stm32f4xx_hal.h"
#include <cmsis_os.h>

/**
 * Initialize the DMX controller port and task
 */
void task_dmx_run(void *argument);

osStatus_t dmx_enable();
osStatus_t dmx_disable();
osStatus_t dmx_start();
osStatus_t dmx_stop();
osStatus_t dmx_set_frame(uint8_t offset, const uint8_t *frame, size_t len);

/**
 * Call this function from the timer ISR
 */
void dmx_timer_notify();

/**
 * Call this function from the UART TX Complete callback
 */
void dmx_uart_tx_cplt();

#endif /* DMX_H */
