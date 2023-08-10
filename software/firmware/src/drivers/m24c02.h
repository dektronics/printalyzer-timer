/*
 * M24C02(-D) 2-Kbit serial I2C bus EEPROM
 */

#ifndef M24C02_H
#define M24C02_H

#include <stm32f4xx_hal.h>

#define M24C02_PAGE_SIZE  0x10UL
#define M24C02_PAGE_LIMIT 0x100UL

/**
 * Read a byte from the specified address.
 *
 * @param hi2c Pointer to a handle for the I2C peripheral
 * @param address Address to read from
 * @param data Pointer to the variable to read the data into
 */
HAL_StatusTypeDef m24c02_read_byte(I2C_HandleTypeDef *hi2c, uint8_t address, uint8_t *data);

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
HAL_StatusTypeDef m24c02_read_buffer(I2C_HandleTypeDef *hi2c, uint8_t address, uint8_t *data, size_t data_len);

/**
 * Write a byte to the specified address.
 *
 * @param hi2c Pointer to a handle for the I2C peripheral
 * @param address Address to write to
 * @param data Data to be written
 */
HAL_StatusTypeDef m24c02_write_byte(I2C_HandleTypeDef *hi2c, uint8_t address, uint8_t data);

/**
 * Write a sequence of bytes to the specified address.
 *
 * Actual write operations may be broken up into several individual page
 * writes, and thus a failure may happen after a partial write was
 * completed.
 *
 * @param hi2c Pointer to a handle for the I2C peripheral
 * @param address Address to write from
 * @param data Pointer to the buffer to write the data from
 * @param data_len Size of the data to be written
 */
HAL_StatusTypeDef m24c02_write_buffer(I2C_HandleTypeDef *hi2c, uint8_t address, const uint8_t *data, size_t data_len);

/**
 * Write a sequence of bytes to the specified address within a memory page.
 *
 * Buffer write operations are limited to the 16-byte page of the starting
 * address, and will fail if they exceed these bounds. To write across
 * multiple pages, multiple write operations are necessary.
 *
 * @param hi2c Pointer to a handle for the I2C peripheral
 * @param address Address to write from
 * @param data Pointer to the buffer to write the data from
 * @param data_len Size of the data to be written
 */
HAL_StatusTypeDef m24c02_write_page(I2C_HandleTypeDef *hi2c, uint8_t address, const uint8_t *data, size_t data_len);

/**
 * Read the identification page
 *
 * @param hi2c Pointer to a handle for the I2C peripheral
 * @param data Pointer to a 16-byte buffer to read the data into
 */
HAL_StatusTypeDef m24c02_read_id_page(I2C_HandleTypeDef *hi2c, uint8_t *data);

/**
 * Write the identification page
 *
 * Note: This function explicitly avoids overwriting the device identification
 * code, which is stored in the first 3 bytes of the data buffer. Therefore,
 * the first 3 bytes of the provided buffer must match the first 3 bytes of
 * the existing identification page value.
 *
 * @param hi2c Pointer to a handle for the I2C peripheral
 * @param data Pointer to a 16-byte buffer to write the data from
 */
HAL_StatusTypeDef m24c02_write_id_page(I2C_HandleTypeDef *hi2c, const uint8_t *data);

#endif /* M24C02_H */
