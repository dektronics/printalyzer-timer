#include "aw9523.h"

#define LOG_TAG "aw9523"
#include <elog.h>

static const uint8_t AW9523_ADDRESS = 0x5B << 1;

/* Registers */
#define AW9523_INPUT_P0     0x00
#define AW9523_INPUT_P1     0x01
#define AW9523_OUTPUT_P0    0x02
#define AW9523_OUTPUT_P1    0x03
#define AW9523_CONFIG_P0    0x04
#define AW9523_CONFIG_P1    0x05
#define AW9523_INT_P0       0x06
#define AW9523_INT_P1       0x07
#define AW9523_ID           0x10
#define AW9523_CTL          0x11
#define AW9523_LED_MODE_P0  0x12
#define AW9523_LED_MODE_P1  0x13
#define AW9523_SW_RSTN      0x7F

static const uint8_t AW9523_LED_DIM[] = {
    0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
    0x20, 0x21, 0x22, 0x23, 0x2C, 0x2D, 0x2E, 0x2F
};

HAL_StatusTypeDef aw9523_init(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    if (!hi2c) { return HAL_ERROR; }

    /* Read ID register */
    ret = HAL_I2C_Mem_Read(hi2c, AW9523_ADDRESS, AW9523_ID, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        log_e("Unable to read ID register");
        return ret;
    }
    log_d("AW9523 ID = 0x%02X", data);

    /* Configure P0_7 to P0_0 as LED */
    data = 0x00;
    ret = HAL_I2C_Mem_Write(hi2c, AW9523_ADDRESS, AW9523_LED_MODE_P0, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) { return ret; }

    /* Configure P1_3 to P1_0 as LED */
    data = 0xF0;
    ret = HAL_I2C_Mem_Write(hi2c, AW9523_ADDRESS, AW9523_LED_MODE_P1, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) { return ret; }

    /* Set dimming range to 0 ~ (Imax * 1/4) */
    data = 0x03;
    ret = HAL_I2C_Mem_Write(hi2c, AW9523_ADDRESS, AW9523_CTL, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) { return ret; }

    log_i("AW9523 Initialized");

    return ret;
}

HAL_StatusTypeDef aw9523_set_led(I2C_HandleTypeDef *hi2c, uint8_t index, uint8_t value)
{
    if (!hi2c || index >= sizeof(AW9523_LED_DIM)) { return HAL_ERROR; }

    return HAL_I2C_Mem_Write(hi2c, AW9523_ADDRESS,
        AW9523_LED_DIM[index], I2C_MEMADD_SIZE_8BIT,
        &value, 1, HAL_MAX_DELAY);
}
