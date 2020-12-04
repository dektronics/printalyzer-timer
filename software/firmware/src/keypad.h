#ifndef KEYPAD_H
#define KEYPAD_H

#include <stm32f4xx_hal.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    KEYPAD_START = 105,
    KEYPAD_FOCUS = 106,
    KEYPAD_INC_EXPOSURE = 107,
    KEYPAD_DEC_EXPOSURE = 108,
    KEYPAD_INC_CONTRAST = 109,
    KEYPAD_DEC_CONTRAST = 110,
    KEYPAD_ADD_ADJUSTMENT = 112,
    KEYPAD_TEST_STRIP = 111,
    KEYPAD_CANCEL = 113,
    KEYPAD_MENU = 114,
    KEYPAD_ENCODER = 97,
    KEYPAD_BLACKOUT = 98,
    KEYPAD_FOOTSWITCH = 99,
    KEYPAD_METER_PROBE = 100
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
    keypad_key_t key;
    bool pressed;
} keypad_event_t;

HAL_StatusTypeDef keypad_init(I2C_HandleTypeDef *hi2c);

HAL_StatusTypeDef keypad_inject_event(const keypad_event_t *event);
HAL_StatusTypeDef keypad_clear_events();
HAL_StatusTypeDef keypad_flush_events();
HAL_StatusTypeDef keypad_wait_for_event(keypad_event_t *event, int msecs_to_wait);

HAL_StatusTypeDef keypad_int_event_handler();

#endif /* KEYPAD_H */
