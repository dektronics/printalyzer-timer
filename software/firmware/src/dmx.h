#ifndef DMX_H
#define DMX_H

#include "stm32f4xx_hal.h"

void dmx_init();
void dmx_start();
void dmx_stop();
void dmx_send();

/**
 * Call this function from the timer ISR
 */
void dmx_timer_notify();

/**
 * Call this function from the UART TX Complete callback
 */
void dmx_uart_tx_cplt();

#endif /* DMX_H */
