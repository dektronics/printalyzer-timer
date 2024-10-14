#include "m24c08.h"

#include <strings.h>

#define LOG_TAG "m24c02"
#include <elog.h>

#include "i2c_interface.h"
#include "util.h"

/* I2C device memory address */
static const uint8_t M24C08_ADDRESS = 0x50;

HAL_StatusTypeDef m24c08_read_byte(i2c_handle_t *hi2c, uint16_t address, uint8_t *data)
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("m24c08_read_byte: address=0x%02X", address);

    const uint8_t dev_address = (uint16_t)M24C08_ADDRESS | ((address & 0x0300) >> 8);
    const uint8_t mem_address = address & 0x00FF;

    ret = i2c_mem_read(hi2c, dev_address, mem_address, I2C_MEMADD_SIZE_8BIT, data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        log_e("i2c_mem_read error: %d", ret);
    }

    return ret;
}

HAL_StatusTypeDef m24c08_read_buffer(i2c_handle_t *hi2c, uint16_t address, uint8_t *data, size_t data_len)
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("m24c08_read_buffer: address=0x%02X, size=%d", address, data_len);

    const uint8_t dev_address = (uint16_t)M24C08_ADDRESS | ((address & 0x0300) >> 8);
    const uint8_t mem_address = address & 0x00FF;

    ret = i2c_mem_read(hi2c, dev_address, mem_address, I2C_MEMADD_SIZE_8BIT, data, data_len, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        log_e("i2c_mem_read error: %d", ret);
    }

    return ret;
}

HAL_StatusTypeDef m24c08_write_byte(i2c_handle_t *hi2c, uint16_t address, uint8_t data)
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("m24c08_write_byte: address=0x%02X", address);

    const uint8_t dev_address = (uint16_t)M24C08_ADDRESS | ((address & 0x0300) >> 8);
    const uint8_t mem_address = address & 0x00FF;

    ret = i2c_mem_write(hi2c, dev_address, mem_address, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        log_e("i2c_mem_write error: %d", ret);
    }

    while(i2c_is_device_ready(hi2c, M24C08_ADDRESS, HAL_MAX_DELAY) != HAL_OK);

    return ret;
}

HAL_StatusTypeDef m24c08_write_buffer(i2c_handle_t *hi2c, uint16_t address, const uint8_t *data, size_t data_len)
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("m24c08_write_buffer: address=0x%02X, len=%d", address, data_len);

    if (!data || data_len == 0) {
        log_e("Data is null or empty");
        return HAL_ERROR;
    }

    if (data_len > 0xFFU - address) {
        log_e("Buffer exceeds memory boundary: 0x%05lX+%d > 0x%05lX",
            address, data_len, 0xFF - address);
        return HAL_ERROR;
    }

    //FIXME Need to test the behavior of this code
    uint32_t offset = 0;
    do {
        size_t page_len = 0x10 - (uint8_t)((address + offset) & 0x0F);
        size_t write_len = MIN(page_len, data_len - offset);
        ret = m24c08_write_page(hi2c, address + offset, data + offset, write_len);
        offset += write_len;
    } while (ret == HAL_OK && offset < data_len);

    return ret;
}

HAL_StatusTypeDef m24c08_write_page(i2c_handle_t *hi2c, uint16_t address, const uint8_t *data, size_t data_len)
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("m24c08_write_page: address=0x%02X, len=%d", address, data_len);

    if (!data || data_len == 0) {
        log_e("Data is null or empty");
        return HAL_ERROR;
    }

    //FIXME Need to test the behavior of this code

    uint8_t page_offset = (uint8_t)(address & 0x0F);
    if (data_len > 0x10U - page_offset) {
        log_e("Buffer exceeds page boundary: 0x%05lX+%d > 0x%05lX",
            address, data_len, ((address & 0xF0) + 0x0F));
        return HAL_ERROR;
    }

    const uint8_t dev_address = (uint16_t)M24C08_ADDRESS | ((address & 0x0300) >> 8);
    const uint8_t mem_address = address & 0x00FF;

    ret = i2c_mem_write(hi2c, dev_address, mem_address, I2C_MEMADD_SIZE_8BIT,
        (uint8_t *)data, data_len, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        log_e("i2c_mem_write error: %d", ret);
    }

    //TODO Need to prevent this loop from running infinitely under unrecoverable errors
    while(i2c_is_device_ready(hi2c, M24C08_ADDRESS, HAL_MAX_DELAY) != HAL_OK);

    return ret;
}
