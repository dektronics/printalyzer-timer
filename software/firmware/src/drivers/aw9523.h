/*
 * AW9523 Multi-Function LED driver and GPIO controller
 */

#ifndef AW9523_H
#define AW9523_H

#include <stm32f4xx_hal.h>

HAL_StatusTypeDef aw9523_init(I2C_HandleTypeDef *hi2c);

HAL_StatusTypeDef aw9523_set_led(I2C_HandleTypeDef *hi2c, uint8_t index, uint8_t value);

#endif /* AW9523_H */
