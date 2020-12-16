#ifndef MAIN_TASK_H
#define MAIN_TASK_H

#include <stdint.h>

/**
 * Initialize the main FreeRTOS task
 */
void main_task_init(void);

void main_task_notify_gpio_int(uint16_t gpio_pin);
void main_task_notify_countdown_timer();

#endif /* MAIN_TASK_H */
