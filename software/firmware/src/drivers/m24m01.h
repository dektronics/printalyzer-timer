/*
 * M24M01 1-Mbit serial I2C bus EEPROM
 */

#ifndef M24M01_H
#define M24M01_H

#include <stm32f4xx_hal.h>

HAL_StatusTypeDef m24m01_init(I2C_HandleTypeDef *hi2c);

HAL_StatusTypeDef m24m01_read_byte(I2C_HandleTypeDef *hi2c, uint32_t address, uint8_t *data);
HAL_StatusTypeDef m24m01_read_buffer(I2C_HandleTypeDef *hi2c, uint32_t address, uint8_t *data, size_t data_len);

HAL_StatusTypeDef m24m01_write_byte(I2C_HandleTypeDef *hi2c, uint32_t address, uint8_t data);
HAL_StatusTypeDef m24m01_write_buffer(I2C_HandleTypeDef *hi2c, uint32_t address, const uint8_t *data, size_t data_len);

#endif /* M24M01_H */
