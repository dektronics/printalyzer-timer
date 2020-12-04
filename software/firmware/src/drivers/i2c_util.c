#include "i2c_util.h"

#include <stm32f4xx_hal.h>

HAL_StatusTypeDef i2c_read_register(I2C_HandleTypeDef *hi2c, uint16_t device_id, uint8_t reg, uint8_t *data)
{
    HAL_StatusTypeDef ret = HAL_OK;

    if (!data) {
        return HAL_ERROR;
    }

    do {
        ret = HAL_I2C_Master_Transmit(hi2c, device_id, &reg, 1, HAL_MAX_DELAY);
        if (ret != HAL_OK) {
            break;
        }

        ret = HAL_I2C_Master_Receive(hi2c, device_id, data, 1, HAL_MAX_DELAY);

    } while (0);

    return ret;
}

HAL_StatusTypeDef i2c_write_register(I2C_HandleTypeDef *hi2c, uint16_t device_id, uint8_t reg, uint8_t data)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t buf[2];

    buf[0] = reg;
    buf[1] = data;

    ret = HAL_I2C_Master_Transmit(hi2c, device_id, buf, 2, HAL_MAX_DELAY);

    return ret;
}
