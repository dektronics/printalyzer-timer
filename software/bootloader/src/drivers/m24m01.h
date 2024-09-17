/*
 * M24M01 1-Mbit serial I2C bus EEPROM
 */

#ifndef M24M01_H
#define M24M01_H

#include <stm32f4xx_hal.h>

/**
 * Read a byte from the specified address.
 *
 * @param hi2c Pointer to a handle for the I2C peripheral
 * @param address Address to read from
 * @param data Pointer to the variable to read the data into
 */
HAL_StatusTypeDef m24m01_read_byte(I2C_HandleTypeDef *hi2c, uint32_t address, uint8_t *data);

/**
 * Read a sequence of bytes from the specified address.
 *
 * If the read size exceeds the available size of the memory beyond the
 * starting address, then the read operation will wrap around and read
 * from the beginning of the memory space.
 *
 * @param hi2c Pointer to a handle for the I2C peripheral
 * @param address Address to read from
 * @param data Pointer to the buffer to read the data into
 * @param data_len Size of the data to be read
 */
HAL_StatusTypeDef m24m01_read_buffer(I2C_HandleTypeDef *hi2c, uint32_t address, uint8_t *data, size_t data_len);

#endif /* M24M01_H */
