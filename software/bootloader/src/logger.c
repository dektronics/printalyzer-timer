#include "logger.h"

#include <stm32f4xx_hal.h>

extern UART_HandleTypeDef huart1;

void putchar_(char c)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&c, 1, 0xFFFF);
}
