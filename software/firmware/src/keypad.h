#ifndef KEYPAD_H
#define KEYPAD_H

#include <stm32f4xx_hal.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * Mapping of key codes
 *
 * Key codes 1..80 are from the keypad array
 * Key codes 97..104 are for Row GPI key events
 * Key codes 105..114 are for Column GPI key events
 * Key codes 200..255 are injected from other sources
 */
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
    KEYPAD_METER_PROBE = 100,
    KEYPAD_ENCODER_CCW = 200,
    KEYPAD_ENCODER_CW = 201,
    KEYPAD_USB_KEYBOARD = 254
} keypad_key_t;

typedef struct {
    keypad_key_t key;
    bool pressed;
    bool repeated;
    uint16_t keypad_state;
} keypad_event_t;

typedef void (*keypad_blackout_callback_t)(bool enabled, void *user_data);

HAL_StatusTypeDef keypad_init(I2C_HandleTypeDef *hi2c);
void keypad_set_blackout_callback(keypad_blackout_callback_t callback, void *user_data);

HAL_StatusTypeDef keypad_inject_event(const keypad_event_t *event);
HAL_StatusTypeDef keypad_clear_events();
HAL_StatusTypeDef keypad_flush_events();
HAL_StatusTypeDef keypad_wait_for_event(keypad_event_t *event, int msecs_to_wait);

bool keypad_is_key_pressed(const keypad_event_t *event, keypad_key_t key);
bool keypad_is_key_released_or_repeated(const keypad_event_t *event, keypad_key_t key);
bool keypad_is_key_combo_pressed(const keypad_event_t *event, keypad_key_t key1, keypad_key_t key2);
char keypad_usb_get_ascii(const keypad_event_t *event);
uint8_t keypad_usb_get_keycode(const keypad_event_t *event);
keypad_key_t keypad_usb_get_keypad_equivalent(const keypad_event_t *event);

HAL_StatusTypeDef keypad_int_event_handler();

#endif /* KEYPAD_H */
