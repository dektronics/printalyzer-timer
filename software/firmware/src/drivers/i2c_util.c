#include "i2c_util.h"

#include <stm32f4xx_hal.h>

HAL_StatusTypeDef i2c_read_register(I2C_HandleTypeDef *hi2c, uint16_t device_id, uint8_t reg, uint8_t *data)
{
    return HAL_I2C_Mem_Read(hi2c, device_id, reg, I2C_MEMADD_SIZE_8BIT, data, 1, HAL_MAX_DELAY);
}

HAL_StatusTypeDef i2c_write_register(I2C_HandleTypeDef *hi2c, uint16_t device_id, uint8_t reg, uint8_t data)
{
    return HAL_I2C_Mem_Write(hi2c, device_id, reg, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
}
