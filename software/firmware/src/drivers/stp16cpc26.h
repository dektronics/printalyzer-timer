/*
 * STP16CPC26 Low voltage 16-bit constant current LED sink driver
 */

#ifndef STP16CPC26_H
#define STP16CPC26_H

#include "stm32f4xx_hal.h"

typedef struct __stp16cpc26_handle_t {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *le_gpio_port;
    uint16_t le_gpio_pin;
    TIM_HandleTypeDef *oe_tim;
    uint32_t oe_tim_channel;
} stp16cpc26_handle_t;

HAL_StatusTypeDef stp16cpc26_set_leds(stp16cpc26_handle_t *handle, uint16_t setting);
HAL_StatusTypeDef stp16cpc26_set_brightness(stp16cpc26_handle_t *handle, uint16_t duty_cycle);
uint16_t stp16cpc26_get_max_brightness(stp16cpc26_handle_t *handle);

#endif /* STP16CPC26_H */
