#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stm32f4xx_hal.h>

extern UART_HandleTypeDef huart1;

void *__stack_chk_guard = (void *)0xdeadbeef;

void __stack_chk_fail(void)
{
    static const char *msg = "\nStack check failure!\n\n";
    HAL_UART_Transmit(&huart1, (uint8_t *)msg, 23, 0xFFFF);
    abort();
}
