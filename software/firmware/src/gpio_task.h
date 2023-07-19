/*
 * Task to handle GPIO interrupts
 */

#ifndef GPIO_TASK_H
#define GPIO_TASK_H

#include <stdint.h>

/**
 * Start the GPIO task.
 *
 * @param argument The osSemaphoreId_t used to synchronize task startup.
 */
void gpio_task_run(void *argument);

void gpio_task_notify_gpio_int(uint16_t gpio_pin);

#endif /* GPIO_TASK_H */
