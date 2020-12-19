#ifndef FATFS_H
#define FATFS_H

#include <stm32f4xx_hal.h>

void fatfs_init(void);

HAL_StatusTypeDef fatfs_mount();
void fatfs_unmount();

#endif /* FATFS_H */
