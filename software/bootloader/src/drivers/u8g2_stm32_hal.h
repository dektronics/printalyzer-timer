#ifndef __U8G2_STM32_HAL_H
#define __U8G2_STM32_HAL_H

#include "stm32f4xx_hal.h"
#include "u8g2.h"

typedef struct __u8g2_display_handle_t {
    SPI_HandleTypeDef *hspi;

    GPIO_TypeDef *reset_gpio_port;
    uint16_t reset_gpio_pin;

    GPIO_TypeDef *cs_gpio_port;
    uint16_t cs_gpio_pin;

    GPIO_TypeDef *dc_gpio_port;
    uint16_t dc_gpio_pin;
} u8g2_display_handle_t;

void u8g2_stm32_hal_init(u8g2_t *u8g2, const u8g2_display_handle_t *u8g2_display_handle);
uint8_t u8g2_stm32_spi_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
uint8_t u8g2_stm32_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

#endif /* __U8G2_STM32_HAL_H */
