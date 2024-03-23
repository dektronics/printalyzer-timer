/*
 * M24C08 8-Kbit serial I2C bus EEPROM
 */

#ifndef M24C08_H
#define M24C08_H

#include <stm32f4xx_hal.h>

typedef struct i2c_handle_t i2c_handle_t;

#define M24C08_PAGE_SIZE  0x10UL
#define M24C08_MEM_SIZE   0x200UL

/**
 * Read a byte from the specified address.
 *
 * @param hi2c Pointer to a handle for the I2C peripheral
 * @param address Address to read from
 * @param data Pointer to the variable to read the data into
 */
HAL_StatusTypeDef m24c08_read_byte(i2c_handle_t *hi2c, uint16_t address, uint8_t *data);

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
HAL_StatusTypeDef m24c08_read_buffer(i2c_handle_t *hi2c, uint16_t address, uint8_t *data, size_t data_len);

/**
 * Write a byte to the specified address.
 *
 * @param hi2c Pointer to a handle for the I2C peripheral
 * @param address Address to write to
 * @param data Data to be written
 */
HAL_StatusTypeDef m24c08_write_byte(i2c_handle_t *hi2c, uint16_t address, uint8_t data);

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
HAL_StatusTypeDef m24c08_write_buffer(i2c_handle_t *hi2c, uint16_t address, const uint8_t *data, size_t data_len);

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
HAL_StatusTypeDef m24c08_write_page(i2c_handle_t *hi2c, uint16_t address, const uint8_t *data, size_t data_len);

#endif /* M24C08_H */
