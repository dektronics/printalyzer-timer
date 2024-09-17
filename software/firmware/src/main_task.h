#ifndef MAIN_TASK_H
#define MAIN_TASK_H

#include <stdbool.h>
#include <stdint.h>
#include <cmsis_os.h>

/**
 * Initialize the main FreeRTOS task.
 *
 * This task will run when the scheduler is started, and will start all
 * of the other tasks.
 */
osStatus_t main_task_init(void);

/**
 * Gets whether the main task has completed initialization.
 */
bool main_task_is_running();

void main_task_notify_countdown_timer();

/**
 * Shutdown the system hardware in preparation for a restart
 */
void main_task_shutdown();

#endif /* MAIN_TASK_H */
