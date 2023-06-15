#include "relay.h"

#include <string.h>

#define LOG_TAG "relay"
#include <elog.h>

static relay_handle_t relay_handle = {0};

HAL_StatusTypeDef relay_init(const relay_handle_t *handle)
{
    log_d("relay_init");

    if (!handle) {
        log_e("Relay handle not initialized");
        return HAL_ERROR;
    }

    memcpy(&relay_handle, handle, sizeof(relay_handle_t));

    /* Start with both relays in a known disengaged state */
    HAL_GPIO_WritePin(relay_handle.enlarger_gpio_port, relay_handle.enlarger_gpio_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(relay_handle.safelight_gpio_port, relay_handle.safelight_gpio_pin, GPIO_PIN_RESET);

    return HAL_OK;
}

void relay_enlarger_enable(bool enabled)
{
    HAL_GPIO_WritePin(relay_handle.enlarger_gpio_port, relay_handle.enlarger_gpio_pin,
        enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool relay_enlarger_is_enabled()
{
    if (relay_handle.enlarger_gpio_port->ODR & relay_handle.enlarger_gpio_pin) {
        return true;
    } else {
        return false;
    }
}

void relay_safelight_enable(bool enabled)
{
    HAL_GPIO_WritePin(relay_handle.safelight_gpio_port, relay_handle.safelight_gpio_pin,
        enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool relay_safelight_is_enabled()
{
    if (relay_handle.safelight_gpio_port->ODR & relay_handle.safelight_gpio_pin) {
        return true;
    } else {
        return false;
    }
}
