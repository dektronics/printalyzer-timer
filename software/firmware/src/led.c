#include "led.h"

#include <string.h>
#include <stdint.h>
#include <math.h>

#define LOG_TAG "led"
#include <elog.h>

#include "stp16cpc26.h"

static uint16_t led_gamma_value(uint8_t input, uint16_t max_brightness);

static stp16cpc26_handle_t led_handle = {0};
static uint16_t led_state = 0;
static uint8_t led_brightness = 0;

HAL_StatusTypeDef led_init(const stp16cpc26_handle_t *handle)
{
    log_d("led_init");

    if (!handle) {
        log_e("LED handle not initialized");
        return HAL_ERROR;
    }

    memcpy(&led_handle, handle, sizeof(stp16cpc26_handle_t));

    led_state = 0;
    led_brightness = 0;
    stp16cpc26_set_leds(&led_handle, 0);
    stp16cpc26_set_brightness(&led_handle, 0);

    return HAL_OK;
}

HAL_StatusTypeDef led_set_state(uint16_t state)
{
    HAL_StatusTypeDef ret;
    ret = stp16cpc26_set_leds(&led_handle, state);
    if (ret == HAL_OK) {
        led_state = state;
    }
    return ret;
}

uint16_t led_get_state()
{
    return led_state;
}

HAL_StatusTypeDef led_set_on(led_t led)
{
    uint16_t updated_state = led_state | (uint16_t)led;
    if (updated_state == led_state) {
        return HAL_OK;
    }
    return led_set_state(updated_state);
}

HAL_StatusTypeDef led_set_off(led_t led)
{
    uint16_t updated_state = led_state & ~((uint16_t)led);
    if (updated_state == led_state) {
        return HAL_OK;
    }
    return led_set_state(updated_state);
}

HAL_StatusTypeDef led_set_brightness(uint8_t brightness)
{
    HAL_StatusTypeDef ret;

    const uint16_t max_duty_cycle = stp16cpc26_get_max_brightness(&led_handle);
    uint16_t duty_cycle;

    if (brightness > 0) {
        duty_cycle = led_gamma_value(brightness, max_duty_cycle);
    } else {
        duty_cycle = max_duty_cycle + 1;
    }

    ret = stp16cpc26_set_brightness(&led_handle, duty_cycle);
    if (ret == HAL_OK) {
        led_brightness = brightness;
    }
    return ret;
}

uint8_t led_get_brightness()
{
    return led_brightness;
}

uint16_t led_gamma_value(uint8_t input, uint16_t max_brightness)
{
    /*
     * This code is based on the following reference snippets:
     *
     * L* = 116(Y/Yn)^1/3 - 16 , Y/Yn > 0.008856
     * L* = 903.3(Y/Yn), Y/Yn <= 0.008856
     *
     * https://ledshield.wordpress.com/2012/11/13/led-brightness-to-your-eye-gamma-correction-no/
     * https://web.archive.org/web/20190122235608/http://forum.arduino.cc/index.php/topic,147810.0.html
     */

    float brightness_ratio = (input / (float)UINT8_MAX) * 100.0F;

    /* Calculate based on the CIE formula */
    float L;
    if (brightness_ratio > 7.9996F) {
        L = powf(((brightness_ratio + 16.0F) / 116.0F), 3.0F);
    } else {
        L = brightness_ratio / 903.3F;
    }

    /* Convert to the result we want */
    int result = max_brightness - lroundf(L * (float)max_brightness);

    /* Clamp the output */
    if (result < 0) { result = 0; }
    else if (result > max_brightness) { result = max_brightness; }

    return (uint16_t)result;
}
