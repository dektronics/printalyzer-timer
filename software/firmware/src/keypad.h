#ifndef KEYPAD_H
#define KEYPAD_H

#include <stm32f4xx_hal.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    KEY_START = 105,
    KEY_FOCUS = 106,
    KEY_INC_EXPOSURE = 107,
    KEY_DEC_EXPOSURE = 108,
    KEY_INC_CONTRAST = 109,
    KEY_DEC_CONTRAST = 110,
    KEY_ADD_ADJUSTMENT = 112,
    KEY_TEST_STRIP = 111,
    KEY_CANCEL = 0, /* not working */
    KEY_MENU = 0, /* not working */
    KEY_ENCODER = 97,
    KEY_BLACKOUT = 98,
    KEY_FOOTSWITCH = 99,
    KEY_METER_PROBE = 0, /* not working */
} keypad_key_t;

/**
 * Represents a key press or release event
 *
 * Key codes 1..80 are from the keypad array
 * Key codes 97..104 are for Row GPI key events
 * Key codes 105..114 are for Column GPI key events
 * Key code 200 is the capacitive touch pad
 */
typedef struct {
    uint8_t key;
    bool pressed;
} keypad_event_t;

HAL_StatusTypeDef keypad_init(I2C_HandleTypeDef *hi2c);

HAL_StatusTypeDef keypad_inject_event(const keypad_event_t *event);
HAL_StatusTypeDef keypad_clear_events();
HAL_StatusTypeDef keypad_flush_events();
HAL_StatusTypeDef keypad_wait_for_event(keypad_event_t *event, int msecs_to_wait);

HAL_StatusTypeDef keypad_int_event_handler();

#endif /* KEYPAD_H */
