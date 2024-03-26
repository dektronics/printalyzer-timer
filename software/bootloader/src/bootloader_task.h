#ifndef BOOTLOADER_TASK_H
#define BOOTLOADER_TASK_H

#include <cmsis_os.h>

/**
 * Initialize the bootloader FreeRTOS task.
 *
 * This task will run when the scheduler is started, and will be the
 * main task managing the interactive bootloader process.
 */
osStatus_t bootloader_task_init(void);

#endif /* BOOTLOADER_TASK_H */
