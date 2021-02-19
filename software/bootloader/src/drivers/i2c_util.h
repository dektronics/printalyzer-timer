#ifndef I2C_UTIL_H
#define I2C_UTIL_H

#include <stm32f4xx_hal.h>

HAL_StatusTypeDef i2c_read_register(I2C_HandleTypeDef *hi2c, uint16_t device_id, uint8_t reg, uint8_t *data);
HAL_StatusTypeDef i2c_write_register(I2C_HandleTypeDef *hi2c, uint16_t device_id, uint8_t reg, uint8_t data);

#endif /* I2C_UTIL_H */
