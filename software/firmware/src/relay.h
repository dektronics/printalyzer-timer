#ifndef RELAY_H
#define RELAY_H

#include <stdbool.h>
#include "stm32f4xx_hal.h"

typedef struct __relay_handle_t {
    GPIO_TypeDef *enlarger_gpio_port;
    uint16_t enlarger_gpio_pin;
    GPIO_TypeDef *safelight_gpio_port;
    uint16_t safelight_gpio_pin;
} relay_handle_t;

HAL_StatusTypeDef relay_init(const relay_handle_t *handle);

void relay_enlarger_enable(bool enabled);
void relay_safelight_enable(bool enabled);

#endif /* RELAY_H */
