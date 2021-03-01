#include "m24m01.h"

#include <esp_log.h>

#include <strings.h>

#include "i2c_util.h"
#include "util.h"

static const char *TAG = "m24m01";

/* I2C device base address */
static const uint8_t M24M01_ADDRESS = 0x50 << 1;

#define DEVICE_ADDRESS(x) (M24M01_ADDRESS | (uint8_t)((x & 0x10000) >> 15))
#define MEMORY_ADDRESS(x) ((uint16_t)(x & 0xFFFF))

HAL_StatusTypeDef m24m01_read_byte(I2C_HandleTypeDef *hi2c, uint32_t address, uint8_t *data)
{
    HAL_StatusTypeDef ret = HAL_OK;

    ESP_LOGD(TAG, "m24m01_read_byte: address=0x%05lX", address);

    if (address > 0x1FFFF) {
        ESP_LOGE(TAG, "Cannot read from invalid address");
        return HAL_ERROR;
    }

    uint8_t device_addr = DEVICE_ADDRESS(address);
    uint16_t mem_addr = MEMORY_ADDRESS(address);

    ret = HAL_I2C_Mem_Read(hi2c, device_addr, mem_addr, I2C_MEMADD_SIZE_16BIT, data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        ESP_LOGE(TAG, "HAL_I2C_Mem_Read error: %d", ret);
    }

    return ret;
}

HAL_StatusTypeDef m24m01_read_buffer(I2C_HandleTypeDef *hi2c, uint32_t address, uint8_t *data, size_t data_len)
{
    HAL_StatusTypeDef ret = HAL_OK;

    ESP_LOGD(TAG, "m24m01_read_buffer: address=0x%05lX, size=%d", address, data_len);

    if (address > 0x1FFFF) {
        ESP_LOGE(TAG, "Cannot read from invalid address");
        return HAL_ERROR;
    }

    uint8_t device_addr = DEVICE_ADDRESS(address);
    uint16_t mem_addr = MEMORY_ADDRESS(address);

    ret = HAL_I2C_Mem_Read(hi2c, device_addr, mem_addr, I2C_MEMADD_SIZE_16BIT, data, data_len, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        ESP_LOGE(TAG, "HAL_I2C_Mem_Read error: %d", ret);
    }

    return ret;
}

HAL_StatusTypeDef m24m01_write_byte(I2C_HandleTypeDef *hi2c, uint32_t address, uint8_t data)
{
    HAL_StatusTypeDef ret = HAL_OK;

    ESP_LOGD(TAG, "m24m01_write_byte: address=0x%05lX", address);

    if (address > 0x1FFFF) {
        ESP_LOGE(TAG, "Cannot write to invalid address");
        return HAL_ERROR;
    }

    uint8_t device_addr = DEVICE_ADDRESS(address);
    uint16_t mem_addr = MEMORY_ADDRESS(address);

    ret = HAL_I2C_Mem_Write(hi2c, device_addr, mem_addr, I2C_MEMADD_SIZE_16BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        ESP_LOGE(TAG, "HAL_I2C_Mem_Write error: %d", ret);
    }

    while(HAL_I2C_IsDeviceReady(hi2c, device_addr, 1, HAL_MAX_DELAY) != HAL_OK);

    return ret;
}

HAL_StatusTypeDef m24m01_write_buffer(I2C_HandleTypeDef *hi2c, uint32_t address, const uint8_t *data, size_t data_len)
{
    HAL_StatusTypeDef ret = HAL_OK;

    ESP_LOGD(TAG, "m24m01_write_buffer: address=0x%05lX, len=%d", address, data_len);

    if (!data || data_len == 0) {
        ESP_LOGE(TAG, "Data is null or empty");
        return HAL_ERROR;
    }

    if (data_len > 0x1FFFF - address) {
        ESP_LOGE(TAG, "Buffer exceeds memory boundary: 0x%05lX+%d > 0x%05lX",
            address, data_len, 0x1FFFF - address);
        return HAL_ERROR;
    }

    uint32_t offset = 0;
    do {
        size_t page_len = 0x100 - (uint8_t)((address + offset) & 0x000FF);
        size_t write_len = MIN(page_len, data_len - offset);
        ret = m24m01_write_page(hi2c, address + offset, data + offset, write_len);
        offset += write_len;
    } while (ret == HAL_OK && offset < data_len);

    return ret;
}

HAL_StatusTypeDef m24m01_write_page(I2C_HandleTypeDef *hi2c, uint32_t address, const uint8_t *data, size_t data_len)
{
    HAL_StatusTypeDef ret = HAL_OK;

    ESP_LOGD(TAG, "m24m01_write_page: address=0x%05lX, len=%d", address, data_len);

    if (address > 0x1FFFF) {
        ESP_LOGE(TAG, "Cannot write to invalid address");
        return HAL_ERROR;
    }

    uint8_t device_addr = DEVICE_ADDRESS(address);
    uint16_t mem_addr = MEMORY_ADDRESS(address);

    if (!data || data_len == 0) {
        ESP_LOGE(TAG, "Data is null or empty");
        return HAL_ERROR;
    }

    uint8_t page_offset = (uint8_t)(mem_addr & 0x00FF);
    if (data_len > 0x100 - page_offset) {
        ESP_LOGE(TAG, "Buffer exceeds page boundary: 0x%05lX+%d > 0x%05lX",
            address, data_len, ((address & 0x1FF00) + 0xFF));
        return HAL_ERROR;
    }

    ret = HAL_I2C_Mem_Write(hi2c, device_addr, mem_addr, I2C_MEMADD_SIZE_16BIT,
        (uint8_t *)data, data_len, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        ESP_LOGE(TAG, "HAL_I2C_Mem_Write error: %d", ret);
    }

    while(HAL_I2C_IsDeviceReady(hi2c, device_addr, 1, HAL_MAX_DELAY) != HAL_OK);

    return ret;
}
