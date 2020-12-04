#include "m24m01.h"

#include <esp_log.h>

#include <strings.h>

#include "i2c_util.h"

static const char *TAG = "m24m01";

/* I2C device base address */
static const uint8_t M24M01_ADDRESS = 0x50 << 1;

#define DEVICE_ADDRESS(x) (M24M01_ADDRESS | (uint8_t)((x & 0x10000) >> 15))
#define MEMORY_ADDRESS(x) ((uint16_t)(x & 0xFFFF))

HAL_StatusTypeDef m24m01_init(I2C_HandleTypeDef *hi2c)
{
//    HAL_StatusTypeDef ret;
//    uint8_t data[2];

    //FIXME change this quick and dirty test code into something useful

//    ESP_LOGI(TAG, "Testing M24M01...");
//
//    // Note: The read sequence for this chip might not be correct without testing.
//    // At this point, we really just care that it is responding to I2C commands.
//
//    data[0] = 0x00;
//    data[1] = 0x00;
//    ret = HAL_I2C_Master_Transmit(hi2c, M24M01_ADDRESS, data, 2, HAL_MAX_DELAY);
//    if (ret != HAL_OK) {
//        ESP_LOGE(TAG, "Unable to write byte address");
//        return ret;
//    }
//
//    ret = HAL_I2C_Master_Receive(hi2c, M24M01_ADDRESS, data, 1, HAL_MAX_DELAY);
//    if (ret != HAL_OK) {
//        ESP_LOGE(TAG, "Unable to read data");
//        return ret;
//    }
//
//    ESP_LOGI(TAG, "Data at first address: %02X", data[0]);

    return HAL_OK;
}

HAL_StatusTypeDef m24m01_read_byte(I2C_HandleTypeDef *hi2c, uint32_t address, uint8_t *data)
{
    HAL_StatusTypeDef ret = HAL_OK;

    ESP_LOGD(TAG, "m24m01_read_byte: address=0x%05lX", address);

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

    uint8_t device_addr = DEVICE_ADDRESS(address);
    uint16_t mem_addr = MEMORY_ADDRESS(address);

    ret = HAL_I2C_Mem_Write(hi2c, device_addr, mem_addr, I2C_MEMADD_SIZE_16BIT, &data, 1, HAL_MAX_DELAY);

    while(HAL_I2C_IsDeviceReady(hi2c, device_addr, 1, HAL_MAX_DELAY) != HAL_OK);

    return ret;
}

HAL_StatusTypeDef m24m01_write_buffer(I2C_HandleTypeDef *hi2c, uint32_t address, const uint8_t *data, size_t data_len)
{
    //TODO making this work as expected may require some page handling
    return HAL_ERROR;
}
