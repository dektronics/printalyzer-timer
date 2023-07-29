#include "tca8418.h"

#include <strings.h>

#define LOG_TAG "tca8418"
#include <elog.h>

/* I2C device address */
static const uint8_t TCA8418_ADDRESS = 0x34 << 1;

/* Registers */
#define TCA8418_CFG            0x01
#define TCA8418_INT_STAT       0x02
#define TCA8418_KEY_LCK_EC     0x03
#define TCA8418_KEY_EVENT_A    0x04
#define TCA8418_KEY_EVENT_B    0x05
#define TCA8418_KEY_EVENT_C    0x06
#define TCA8418_KEY_EVENT_D    0x07
#define TCA8418_KEY_EVENT_E    0x08
#define TCA8418_KEY_EVENT_F    0x09
#define TCA8418_KEY_EVENT_G    0x0A
#define TCA8418_KEY_EVENT_H    0x0B
#define TCA8418_KEY_EVENT_I    0x0C
#define TCA8418_KEY_EVENT_J    0x0D
#define TCA8418_KP_LCK_TIMER   0x0E
#define TCA8418_UNLOCK1        0x0F
#define TCA8418_UNLOCK2        0x10
#define TCA8418_GPIO_INT_STAT1 0x11
#define TCA8418_GPIO_INT_STAT2 0x12
#define TCA8418_GPIO_INT_STAT3 0x13
#define TCA8418_GPIO_DAT_STAT1 0x14
#define TCA8418_GPIO_DAT_STAT2 0x15
#define TCA8418_GPIO_DAT_STAT3 0x16
#define TCA8418_GPIO_DAT_OUT1  0x17
#define TCA8418_GPIO_DAT_OUT2  0x18
#define TCA8418_GPIO_DAT_OUT3  0x19
#define TCA8418_GPIO_INT_EN1   0x1A
#define TCA8418_GPIO_INT_EN2   0x1B
#define TCA8418_GPIO_INT_EN3   0x1C
#define TCA8418_KP_GPIO1       0x1D
#define TCA8418_KP_GPIO2       0x1E
#define TCA8418_KP_GPIO3       0x1F
#define TCA8418_GPI_EM1        0x20
#define TCA8418_GPI_EM2        0x21
#define TCA8418_GPI_EM3        0x22
#define TCA8418_GPIO_DIR1      0x23
#define TCA8418_GPIO_DIR2      0x24
#define TCA8418_GPIO_DIR3      0x25
#define TCA8418_GPIO_INT_LVL1  0x26
#define TCA8418_GPIO_INT_LVL2  0x27
#define TCA8418_GPIO_INT_LVL3  0x28
#define TCA8418_DEBOUNCE_DIS1  0x29
#define TCA8418_DEBOUNCE_DIS2  0x2A
#define TCA8418_DEBOUNCE_DIS3  0x2B
#define TCA8418_GPIO_PULL1     0x2C
#define TCA8418_GPIO_PULL2     0x2D
#define TCA8418_GPIO_PULL3     0x2E

HAL_StatusTypeDef tca8418_init(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    // Disable interrupts in case we didn't come up clean, so we're forced
    // to explicitly enable them.
    data = 0x00;
    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_CFG, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_GPIO_INT_EN1, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_GPIO_INT_EN2, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_GPIO_INT_EN3, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    // Read key events until the FIFO is clear
    do {
        ret = HAL_I2C_Mem_Read(hi2c, TCA8418_ADDRESS, TCA8418_KEY_EVENT_A, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
        if (ret != HAL_OK) {
            return ret;
        }
    } while (data != 0);

    // Clear any leftover interrupt flags
    data = 0x0F;
    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_INT_STAT, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    // Read the initial state of the controller
    ret = HAL_I2C_Mem_Read(hi2c, TCA8418_ADDRESS, TCA8418_CFG, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }
    log_i("CONFIG: %02X", data);

    ret = HAL_I2C_Mem_Read(hi2c, TCA8418_ADDRESS, TCA8418_INT_STAT, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }
    log_i("INT_STAT: %02X", data);

    ret = HAL_I2C_Mem_Read(hi2c, TCA8418_ADDRESS, TCA8418_KEY_LCK_EC, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }
    log_i("KEY_LCK_EC: %02X", data);

    return HAL_OK;
}

HAL_StatusTypeDef tca8148_set_config(I2C_HandleTypeDef *hi2c, uint8_t value)
{
    return HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_CFG, I2C_MEMADD_SIZE_8BIT, &value, 1, HAL_MAX_DELAY);
}

HAL_StatusTypeDef tca8148_get_interrupt_status(I2C_HandleTypeDef *hi2c, uint8_t *status)
{
    return HAL_I2C_Mem_Read(hi2c, TCA8418_ADDRESS, TCA8418_INT_STAT, I2C_MEMADD_SIZE_8BIT, status, 1, HAL_MAX_DELAY);
}

HAL_StatusTypeDef tca8148_set_interrupt_status(I2C_HandleTypeDef *hi2c, uint8_t status)
{
    return HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_INT_STAT, I2C_MEMADD_SIZE_8BIT, &status, 1, HAL_MAX_DELAY);
}

HAL_StatusTypeDef tca8148_get_key_event_count(I2C_HandleTypeDef *hi2c, uint8_t *count)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    ret = HAL_I2C_Mem_Read(hi2c, TCA8418_ADDRESS, TCA8418_KEY_LCK_EC, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    *count = (data & 0x0F);

    return HAL_OK;
}

HAL_StatusTypeDef tca8148_get_next_key_event(I2C_HandleTypeDef *hi2c, uint8_t *key, bool *pressed)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    ret = HAL_I2C_Mem_Read(hi2c, TCA8418_ADDRESS, TCA8418_KEY_EVENT_A, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    *key = (data & 0x7F);
    *pressed = (data & 0x80) != 0;

    return HAL_OK;
}

HAL_StatusTypeDef tca8148_get_gpio_interrupt_status(I2C_HandleTypeDef *hi2c, tca8418_pins_t *pins)
{
    HAL_StatusTypeDef ret;
    uint8_t data1;
    uint8_t data2;
    uint8_t data3;

    ret = HAL_I2C_Mem_Read(hi2c, TCA8418_ADDRESS, TCA8418_GPIO_INT_STAT1, I2C_MEMADD_SIZE_8BIT, &data1, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    ret = HAL_I2C_Mem_Read(hi2c, TCA8418_ADDRESS, TCA8418_GPIO_INT_STAT2, I2C_MEMADD_SIZE_8BIT, &data2, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    ret = HAL_I2C_Mem_Read(hi2c, TCA8418_ADDRESS, TCA8418_GPIO_INT_STAT3, I2C_MEMADD_SIZE_8BIT, &data3, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    pins->rows = data1;
    pins->cols_l = data2;
    pins->cols_h = data3 & 0x03;

    return HAL_OK;
}

HAL_StatusTypeDef tca8148_get_gpio_data_status(I2C_HandleTypeDef *hi2c, tca8418_pins_t *pins)
{
    HAL_StatusTypeDef ret;
    uint8_t data1;
    uint8_t data2;
    uint8_t data3;

    ret = HAL_I2C_Mem_Read(hi2c, TCA8418_ADDRESS, TCA8418_GPIO_DAT_STAT1, I2C_MEMADD_SIZE_8BIT, &data1, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    ret = HAL_I2C_Mem_Read(hi2c, TCA8418_ADDRESS, TCA8418_GPIO_DAT_STAT2, I2C_MEMADD_SIZE_8BIT, &data2, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    ret = HAL_I2C_Mem_Read(hi2c, TCA8418_ADDRESS, TCA8418_GPIO_DAT_STAT3, I2C_MEMADD_SIZE_8BIT, &data3, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    if (pins) {
        pins->rows = data1;
        pins->cols_l = data2;
        pins->cols_h = data3 & 0x03;
    }

    return HAL_OK;
}

HAL_StatusTypeDef tca8418_gpio_interrupt_enable(I2C_HandleTypeDef *hi2c, const tca8418_pins_t *pins)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    data = pins->rows;
    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_GPIO_INT_EN1, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    data = pins->cols_l;
    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_GPIO_INT_EN2, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    data = pins->cols_h;
    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_GPIO_INT_EN3, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    return HAL_OK;
}

HAL_StatusTypeDef tca8418_kp_gpio_select(I2C_HandleTypeDef *hi2c, const tca8418_pins_t *pins)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    data = pins->rows;
    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_KP_GPIO1, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    data = pins->cols_l;
    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_KP_GPIO2, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    data = pins->cols_h;
    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_KP_GPIO3, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    return HAL_OK;
}

HAL_StatusTypeDef tca8418_gpi_event_mode(I2C_HandleTypeDef *hi2c, const tca8418_pins_t *pins)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    data = pins->rows;
    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_GPI_EM1, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    data = pins->cols_l;
    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_GPI_EM2, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    data = pins->cols_h;
    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_GPI_EM3, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    return HAL_OK;
}

HAL_StatusTypeDef tca8418_gpio_data_direction(I2C_HandleTypeDef *hi2c, const tca8418_pins_t *pins)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    data = pins->rows;
    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_GPIO_DIR1, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    data = pins->cols_l;
    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_GPIO_DIR2, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    data = pins->cols_h;
    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_GPIO_DIR3, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    return HAL_OK;

}

HAL_StatusTypeDef tca8418_gpio_pullup_disable(I2C_HandleTypeDef *hi2c, const tca8418_pins_t *pins)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    data = pins->rows;
    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_GPIO_PULL1, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    data = pins->cols_l;
    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_GPIO_PULL2, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    data = pins->cols_h;
    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_GPIO_PULL3, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    return HAL_OK;
}

HAL_StatusTypeDef tca8418_gpio_data_status(I2C_HandleTypeDef *hi2c, tca8418_pins_t *pins)
{
    HAL_StatusTypeDef ret;
    uint8_t data1;
    uint8_t data2;
    uint8_t data3;

    ret = HAL_I2C_Mem_Read(hi2c, TCA8418_ADDRESS, TCA8418_GPIO_DAT_STAT1, I2C_MEMADD_SIZE_8BIT, &data1, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    ret = HAL_I2C_Mem_Read(hi2c, TCA8418_ADDRESS, TCA8418_GPIO_DAT_STAT2, I2C_MEMADD_SIZE_8BIT, &data2, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    ret = HAL_I2C_Mem_Read(hi2c, TCA8418_ADDRESS, TCA8418_GPIO_DAT_STAT3, I2C_MEMADD_SIZE_8BIT, &data3, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    pins->rows = data1;
    pins->cols_l = data2;
    pins->cols_h = data3 & 0x03;

    return HAL_OK;
}

HAL_StatusTypeDef tca8418_debounce_disable(I2C_HandleTypeDef *hi2c, const tca8418_pins_t *pins)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    data = pins->rows;
    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_DEBOUNCE_DIS1, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    data = pins->cols_l;
    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_DEBOUNCE_DIS2, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    data = pins->cols_h;
    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_DEBOUNCE_DIS3, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    return HAL_OK;
}

HAL_StatusTypeDef tca8418_clear_interrupt_status(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef ret;
    uint8_t data = 0x0F;
    ret = HAL_I2C_Mem_Write(hi2c, TCA8418_ADDRESS, TCA8418_INT_STAT, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    return ret;
}
