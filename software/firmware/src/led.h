#ifndef LEDS_H
#define LEDS_H

#include "stm32f4xx_hal.h"
#include "stp16cpc26.h"

typedef enum {
    LED_START = 0x0100,
    LED_FOCUS = 0x0200,
    LED_INC_EXPOSURE = 0x0400,
    LED_DEC_EXPOSURE = 0x0800,
    LED_INC_CONTRAST = 0x1000,
    LED_DEC_CONTRAST = 0x2000,
    LED_ADD_ADJUSTMENT = 0x8000,
    LED_TEST_STRIP = 0x4000,
    LED_CANCEL = 0x0001,
    LED_MENU = 0x0002,
    LED_IND_ADD_ADJUSTMENT = 0x0008,
    LED_IND_TEST_STRIP = 0x0004,
    LED_IND_CANCEL = 0x0010,
    LED_IND_MENU = 0x0020,

    LED_ILLUM_CONTROL = LED_START | LED_FOCUS,
    LED_ILLUM_ADJUST = LED_INC_EXPOSURE | LED_DEC_EXPOSURE | LED_INC_CONTRAST | LED_DEC_CONTRAST,
    LED_ILLUM_COMMANDS = LED_ADD_ADJUSTMENT | LED_TEST_STRIP | LED_CANCEL | LED_MENU,
    LED_ILLUM_ALL = LED_ILLUM_CONTROL | LED_ILLUM_ADJUST | LED_ILLUM_COMMANDS,
    LED_IND_ALL = LED_IND_ADD_ADJUSTMENT | LED_IND_TEST_STRIP | LED_IND_CANCEL | LED_IND_MENU

} led_t;

HAL_StatusTypeDef led_init(const stp16cpc26_handle_t *handle);

HAL_StatusTypeDef led_set_state(uint16_t state);
uint16_t led_get_state();

HAL_StatusTypeDef led_set_on(led_t led);
HAL_StatusTypeDef led_set_off(led_t led);

HAL_StatusTypeDef led_set_brightness(uint8_t brightness);
uint8_t led_get_brightness();

#endif /* LEDS_H */
