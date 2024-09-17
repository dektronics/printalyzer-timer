#include "m24m01.h"

#include <strings.h>

#include "logger.h"

/* I2C device base address */
static const uint8_t M24M01_ADDRESS = 0x50 << 1;

#define DEVICE_ADDRESS(x) (M24M01_ADDRESS | (uint8_t)((x & 0x10000) >> 15))
#define MEMORY_ADDRESS(x) ((uint16_t)(x & 0xFFFF))

HAL_StatusTypeDef m24m01_read_byte(I2C_HandleTypeDef *hi2c, uint32_t address, uint8_t *data)
{
    HAL_StatusTypeDef ret = HAL_OK;

    BL_PRINTF("m24m01_read_byte: address=0x%05lX\r\n", address);

    if (address > 0x1FFFF) {
        BL_PRINTF("Cannot read from invalid address\r\n");
        return HAL_ERROR;
    }

    uint8_t device_addr = DEVICE_ADDRESS(address);
    uint16_t mem_addr = MEMORY_ADDRESS(address);

    ret = HAL_I2C_Mem_Read(hi2c, device_addr, mem_addr, I2C_MEMADD_SIZE_16BIT, data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        BL_PRINTF("HAL_I2C_Mem_Read error: %d\r\n", ret);
    }

    return ret;
}

HAL_StatusTypeDef m24m01_read_buffer(I2C_HandleTypeDef *hi2c, uint32_t address, uint8_t *data, size_t data_len)
{
    HAL_StatusTypeDef ret = HAL_OK;

    BL_PRINTF("m24m01_read_buffer: address=0x%05lX, size=%d\r\n", address, data_len);

    if (address > 0x1FFFF) {
        BL_PRINTF("Cannot read from invalid address\r\n");
        return HAL_ERROR;
    }

    uint8_t device_addr = DEVICE_ADDRESS(address);
    uint16_t mem_addr = MEMORY_ADDRESS(address);

    ret = HAL_I2C_Mem_Read(hi2c, device_addr, mem_addr, I2C_MEMADD_SIZE_16BIT, data, data_len, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        BL_PRINTF("HAL_I2C_Mem_Read error: %d\r\n", ret);
    }

    return ret;
}
