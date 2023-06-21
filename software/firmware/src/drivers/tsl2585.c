#include "tsl2585.h"

#include "stm32f4xx_hal.h"

#define LOG_TAG "tsl2585"
#include <elog.h>

/* I2C device address */
static const uint8_t TSL2585_ADDRESS = 0x39 << 1; // Use 8-bit address

/* Registers */
/* TODO: 0x08 .. 0x8F */
#define TSL2585_AUX_ID  0x90 /*!< Auxiliary identification */
#define TSL2585_REV_ID  0x91 /*!< Revision identification */
#define TSL2585_ID      0x92 /*!< Device identification */
#define TSL2585_STATUS  0x93 /*!< Device status information 1 */
/* TODO: 0x94 .. 0xFF */

HAL_StatusTypeDef tsl2585_init(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    log_i("Initializing TSL2585");

    ret = HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_ID, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    log_i("Device ID: %02X", data);

    if (data != 0x5C) {
        log_e("Invalid Device ID");
        return HAL_ERROR;
    }

    ret = HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_REV_ID, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    log_i("Revision ID: %02X", data);

    ret = HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_AUX_ID, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    log_i("Aux ID: %02X", data);

    ret = HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_STATUS, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    log_i("Status: %02X", data);

    //TODO: Power up sensor and set initial configuration

    log_i("TSL2585 Initialized");

    return HAL_OK;

}
