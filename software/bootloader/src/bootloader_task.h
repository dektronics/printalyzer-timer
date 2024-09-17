#ifndef BOOTLOADER_TASK_H
#define BOOTLOADER_TASK_H

#include <cmsis_os.h>

typedef enum {
    TRIGGER_USER_BUTTON = 0,
    TRIGGER_FIRMWARE,
    TRIGGER_CHECKSUM_FAIL
} bootloader_trigger_t;

/**
 * Initialize the bootloader FreeRTOS task.
 *
 * This task will run when the scheduler is started, and will be the
 * main task managing the interactive bootloader process.
 */
osStatus_t bootloader_task_init(bootloader_trigger_t trigger);

#endif /* BOOTLOADER_TASK_H */
