#ifndef I2C_INTERFACE_H
#define I2C_INTERFACE_H

#include <stdint.h>
#include "stm32f4xx.h"

typedef struct i2c_handle_t i2c_handle_t;

struct i2c_handle_t {
    HAL_StatusTypeDef (*transmit)(i2c_handle_t *hi2c, uint8_t dev_address, const uint8_t *data, uint16_t len, uint32_t timeout);
    HAL_StatusTypeDef (*receive)(i2c_handle_t *hi2c, uint8_t dev_address, uint8_t *data, uint16_t len, uint32_t timeout);
    HAL_StatusTypeDef (*mem_write)(i2c_handle_t *hi2c, uint8_t dev_address, uint16_t mem_address, uint16_t mem_addr_size, const uint8_t *data, uint16_t len, uint32_t timeout);
    HAL_StatusTypeDef (*mem_read)(i2c_handle_t *hi2c, uint8_t dev_address, uint16_t mem_address, uint16_t mem_addr_size, uint8_t *data, uint16_t len, uint32_t timeout);
    HAL_StatusTypeDef (*is_device_ready)(i2c_handle_t *hi2c, uint8_t dev_address, uint32_t timeout);
    HAL_StatusTypeDef (*reset)(i2c_handle_t *hi2c);
    void *priv;
};

HAL_StatusTypeDef i2c_transmit(i2c_handle_t *hi2c, uint8_t dev_address, const uint8_t *data, uint16_t len, uint32_t timeout);
HAL_StatusTypeDef i2c_receive(i2c_handle_t *hi2c, uint8_t dev_address, uint8_t *data, uint16_t len, uint32_t timeout);
HAL_StatusTypeDef i2c_mem_write(i2c_handle_t *hi2c, uint8_t dev_address, uint16_t mem_address, uint16_t mem_addr_size, const uint8_t *data, uint16_t len, uint32_t timeout);
HAL_StatusTypeDef i2c_mem_read(i2c_handle_t *hi2c, uint8_t dev_address, uint16_t mem_address, uint16_t mem_addr_size, uint8_t *data, uint16_t len, uint32_t timeout);
HAL_StatusTypeDef i2c_is_device_ready(i2c_handle_t *hi2c, uint8_t dev_address, uint32_t timeout);
HAL_StatusTypeDef i2c_reset(i2c_handle_t *hi2c);

i2c_handle_t *i2c_interface_get_hal_i2c1();

#endif /* I2C_INTERFACE_H */
