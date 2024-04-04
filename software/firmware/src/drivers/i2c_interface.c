#include "i2c_interface.h"

#include "stm32f4xx.h"

extern I2C_HandleTypeDef hi2c1;

HAL_StatusTypeDef i2c_transmit(i2c_handle_t *hi2c, uint8_t dev_address, const uint8_t *data, uint16_t len, uint32_t timeout)
{
    return hi2c->transmit(hi2c, dev_address, data, len, timeout);
}

HAL_StatusTypeDef i2c_receive(i2c_handle_t *hi2c, uint8_t dev_address, uint8_t *data, uint16_t len, uint32_t timeout)
{
    return hi2c->receive(hi2c, dev_address, data, len, timeout);
}

HAL_StatusTypeDef i2c_mem_write(i2c_handle_t *hi2c, uint8_t dev_address, uint16_t mem_address, uint16_t mem_addr_size, const uint8_t *data, uint16_t len, uint32_t timeout)
{
    return hi2c->mem_write(hi2c, dev_address, mem_address, mem_addr_size, data, len, timeout);
}

HAL_StatusTypeDef i2c_mem_read(i2c_handle_t *hi2c, uint8_t dev_address, uint16_t mem_address, uint16_t mem_addr_size, uint8_t *data, uint16_t len, uint32_t timeout)
{
    return hi2c->mem_read(hi2c, dev_address, mem_address, mem_addr_size, data, len, timeout);
}

HAL_StatusTypeDef i2c_is_device_ready(i2c_handle_t *hi2c, uint8_t dev_address, uint32_t timeout)
{
    return hi2c->is_device_ready(hi2c, dev_address, timeout);
}

HAL_StatusTypeDef i2c_reset(i2c_handle_t *hi2c)
{
    return hi2c->reset(hi2c);
}

static HAL_StatusTypeDef hal_i2c_transmit(i2c_handle_t *hi2c, uint8_t dev_address, const uint8_t *data, uint16_t len, uint32_t timeout)
{
    return HAL_I2C_Master_Transmit(&hi2c1, dev_address << 1, (uint8_t *)data, len, timeout);
}

static HAL_StatusTypeDef hal_i2c_receive(i2c_handle_t *hi2c, uint8_t dev_address, uint8_t *data, uint16_t len, uint32_t timeout)
{
    return HAL_I2C_Master_Receive(&hi2c1, dev_address << 1, data, len, timeout);
}

static HAL_StatusTypeDef hal_i2c_mem_write(i2c_handle_t *hi2c, uint8_t dev_address, uint16_t mem_address, uint16_t mem_addr_size, const uint8_t *data, uint16_t len, uint32_t timeout)
{
    return HAL_I2C_Mem_Write(&hi2c1, dev_address << 1, mem_address, mem_addr_size, (uint8_t *)data, len, timeout);
}

static HAL_StatusTypeDef hal_i2c_mem_read(i2c_handle_t *hi2c, uint8_t dev_address, uint16_t mem_address, uint16_t mem_addr_size, uint8_t *data, uint16_t len, uint32_t timeout)
{
    return HAL_I2C_Mem_Read(&hi2c1, dev_address << 1, mem_address, mem_addr_size, data, len, timeout);
}

static HAL_StatusTypeDef hal_i2c_is_device_ready(i2c_handle_t *hi2c, uint8_t dev_address, uint32_t timeout)
{
    return HAL_I2C_IsDeviceReady(&hi2c1, dev_address << 1, 1, timeout);
}

static HAL_StatusTypeDef hal_i2c_reset(i2c_handle_t *hi2c)
{
    HAL_StatusTypeDef ret;
    do {
        ret = HAL_I2C_DeInit(&hi2c1);
        if (ret != HAL_OK) {
            break;
        }

        ret = HAL_I2C_Init(&hi2c1);
        if (ret != HAL_OK) {
            break;
        }
    } while (0);
    return ret;
}

static i2c_handle_t i2c_handle_hal_i2c1 = {
    .transmit = hal_i2c_transmit,
    .receive = hal_i2c_receive,
    .mem_write = hal_i2c_mem_write,
    .mem_read = hal_i2c_mem_read,
    .is_device_ready = hal_i2c_is_device_ready,
    .reset = hal_i2c_reset,
    .priv = NULL
};

i2c_handle_t *i2c_interface_get_hal_i2c1()
{
    return &i2c_handle_hal_i2c1;
}
