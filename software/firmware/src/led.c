#include "led.h"

#include <string.h>
#include <esp_log.h>

#include "stp16cpc26.h"

static stp16cpc26_handle_t led_handle = {0};

static const char *TAG = "led";

HAL_StatusTypeDef led_init(const stp16cpc26_handle_t *handle)
{
    ESP_LOGD(TAG, "led_init");

    if (!handle) {
        ESP_LOGE(TAG, "LED handle not initialized");
        return HAL_ERROR;
    }

    memcpy(&led_handle, handle, sizeof(stp16cpc26_handle_t));

    stp16cpc26_set_leds(&led_handle, 0);
    stp16cpc26_set_brightness(&led_handle, 0);

    return HAL_OK;
}

HAL_StatusTypeDef led_set_enabled(led_t led)
{
    return stp16cpc26_set_leds(&led_handle, (uint16_t)led);
}

HAL_StatusTypeDef led_set_brightness(uint8_t brightness)
{
    return stp16cpc26_set_brightness(&led_handle, brightness);
}
