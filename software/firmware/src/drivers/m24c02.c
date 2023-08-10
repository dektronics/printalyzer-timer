#include "m24c02.h"

#include <strings.h>

#define LOG_TAG "m24c02"
#include <elog.h>

#include "util.h"

/* I2C device memory address */
static const uint8_t M24C02_ADDRESS = 0x50 << 1;

/* I2C device identification page address */
static const uint8_t M24C02_ID_ADDRESS = 0x58 << 1;

HAL_StatusTypeDef m24c02_read_byte(I2C_HandleTypeDef *hi2c, uint8_t address, uint8_t *data)
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("m24c02_read_byte: address=0x%02X", address);

    ret = HAL_I2C_Mem_Read(hi2c, M24C02_ADDRESS, address, I2C_MEMADD_SIZE_8BIT, data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        log_e("HAL_I2C_Mem_Read error: %d", ret);
    }

    return ret;
}

HAL_StatusTypeDef m24c02_read_buffer(I2C_HandleTypeDef *hi2c, uint8_t address, uint8_t *data, size_t data_len)
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("m24c02_read_buffer: address=0x%02X, size=%d", address, data_len);

    ret = HAL_I2C_Mem_Read(hi2c, M24C02_ADDRESS, address, I2C_MEMADD_SIZE_8BIT, data, data_len, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        log_e("HAL_I2C_Mem_Read error: %d", ret);
    }

    return ret;
}

HAL_StatusTypeDef m24c02_write_byte(I2C_HandleTypeDef *hi2c, uint8_t address, uint8_t data)
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("m24c02_write_byte: address=0x%02X", address);

    ret = HAL_I2C_Mem_Write(hi2c, M24C02_ADDRESS, address, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        log_e("HAL_I2C_Mem_Write error: %d", ret);
    }

    while(HAL_I2C_IsDeviceReady(hi2c, M24C02_ADDRESS, 1, HAL_MAX_DELAY) != HAL_OK);

    return ret;
}

HAL_StatusTypeDef m24c02_write_buffer(I2C_HandleTypeDef *hi2c, uint8_t address, const uint8_t *data, size_t data_len)
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("m24c02_write_buffer: address=0x%02X, len=%d", address, data_len);

    if (!data || data_len == 0) {
        log_e("Data is null or empty");
        return HAL_ERROR;
    }

    if (data_len > 0xFF - address) {
        log_e("Buffer exceeds memory boundary: 0x%05lX+%d > 0x%05lX",
            address, data_len, 0xFF - address);
        return HAL_ERROR;
    }

    //FIXME Need to test the behavior of this code
    uint32_t offset = 0;
    do {
        size_t page_len = 0x10 - (uint8_t)((address + offset) & 0x0F);
        size_t write_len = MIN(page_len, data_len - offset);
        ret = m24c02_write_page(hi2c, address + offset, data + offset, write_len);
        offset += write_len;
    } while (ret == HAL_OK && offset < data_len);

    return ret;
}

HAL_StatusTypeDef m24c02_write_page(I2C_HandleTypeDef *hi2c, uint8_t address, const uint8_t *data, size_t data_len)
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("m24c02_write_page: address=0x%02X, len=%d", address, data_len);

    if (!data || data_len == 0) {
        log_e("Data is null or empty");
        return HAL_ERROR;
    }

    //FIXME Need to test the behavior of this code

    uint8_t page_offset = (uint8_t)(address & 0x0F);
    if (data_len > 0x10 - page_offset) {
        log_e("Buffer exceeds page boundary: 0x%05lX+%d > 0x%05lX",
            address, data_len, ((address & 0xF0) + 0x0F));
        return HAL_ERROR;
    }

    ret = HAL_I2C_Mem_Write(hi2c, M24C02_ADDRESS, address, I2C_MEMADD_SIZE_8BIT,
        (uint8_t *)data, data_len, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        log_e("HAL_I2C_Mem_Write error: %d", ret);
    }

    while(HAL_I2C_IsDeviceReady(hi2c, M24C02_ADDRESS, 1, HAL_MAX_DELAY) != HAL_OK);

    return ret;
}

HAL_StatusTypeDef m24c02_read_id_page(I2C_HandleTypeDef *hi2c, uint8_t *data)
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("m24c02_read_id_page");

    ret = HAL_I2C_Mem_Read(hi2c, M24C02_ID_ADDRESS, 0x00, I2C_MEMADD_SIZE_8BIT, data, M24C02_PAGE_SIZE, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        log_e("HAL_I2C_Mem_Read error: %d", ret);
    }

    return ret;
}

HAL_StatusTypeDef m24c02_write_id_page(I2C_HandleTypeDef *hi2c, const uint8_t *data)
{
    HAL_StatusTypeDef ret = HAL_OK;

    uint8_t buf[M24C02_PAGE_SIZE];

    /* Read the existing ID page value */
    ret = m24c02_read_id_page(hi2c, buf);
    if (ret != HAL_OK) { return ret; }

    /* Validate the first 3 bytes */
    if (buf[0] != data[0] || buf[1] != data[1] || buf[2] != data[2]) {
        log_e("Device identification mismatch: %02X%02X%02X != %02X%02X%02X",
            buf[0], buf[1], buf[2],
            data[0], data[1], data[2]);
        return HAL_ERROR;
    }

    /* Write the updated identification page */
    ret = HAL_I2C_Mem_Write(hi2c, M24C02_ID_ADDRESS, 0x00, I2C_MEMADD_SIZE_8BIT,
        (uint8_t *)data, M24C02_PAGE_SIZE, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        log_e("HAL_I2C_Mem_Write error: %d", ret);
    }

    return ret;
}
