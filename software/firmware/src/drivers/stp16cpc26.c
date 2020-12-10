#include "stp16cpc26.h"

#include <esp_log.h>

static const char *TAG = "stp16cpc26";

HAL_StatusTypeDef stp16cpc26_set_leds(stp16cpc26_handle_t *handle, uint16_t setting)
{
    HAL_StatusTypeDef ret = HAL_OK;

    ret = HAL_SPI_Transmit(handle->hspi, (uint8_t *)(&setting), 2, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        ESP_LOGE(TAG, "HAL_SPI_Transmit error: %d", ret);
        return ret;
    }

    HAL_GPIO_WritePin(handle->le_gpio_port, handle->le_gpio_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(handle->le_gpio_port, handle->le_gpio_pin, GPIO_PIN_RESET);
    return ret;
}

HAL_StatusTypeDef stp16cpc26_set_brightness(stp16cpc26_handle_t *handle, uint16_t duty_cycle)
{
    HAL_StatusTypeDef ret;

    if (duty_cycle < UINT16_MAX) {
        __HAL_TIM_SET_COMPARE(handle->oe_tim, handle->oe_tim_channel, duty_cycle);

        if (handle->oe_tim->Instance->CR1 & TIM_CR1_CEN) {
            ret = HAL_OK;
        } else {
            ret = HAL_TIM_PWM_Start(handle->oe_tim, handle->oe_tim_channel);
        }
    } else {
        if (handle->oe_tim->Instance->CR1 & TIM_CR1_CEN) {
            ret = HAL_TIM_PWM_Stop(handle->oe_tim, handle->oe_tim_channel);
        } else {
            ret = HAL_OK;
        }
    }

    return ret;
}
