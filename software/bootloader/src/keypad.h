#ifndef KEYPAD_H
#define KEYPAD_H

#include <stm32f4xx_hal.h>
#include <stdint.h>

typedef enum {
    KEYPAD_START          = 0x00000100U,
    KEYPAD_FOCUS          = 0x00000200U,
    KEYPAD_INC_EXPOSURE   = 0x00000400U,
    KEYPAD_DEC_EXPOSURE   = 0x00000800U,
    KEYPAD_INC_CONTRAST   = 0x00001000U,
    KEYPAD_DEC_CONTRAST   = 0x00002000U,
    KEYPAD_ADD_ADJUSTMENT = 0x00008000U,
    KEYPAD_TEST_STRIP     = 0x00004000U,
    KEYPAD_CANCEL         = 0x00010000U,
    KEYPAD_MENU           = 0x00020000U,
    KEYPAD_ENCODER        = 0x00000001U,
    KEYPAD_BLACKOUT       = 0x00000002U,
    KEYPAD_FOOTSWITCH     = 0x00000004U,
    KEYPAD_METER_PROBE    = 0x00000008U
} keypad_key_t;

HAL_StatusTypeDef keypad_init(I2C_HandleTypeDef *hi2c);

keypad_key_t keypad_poll();

#endif /* KEYPAD_H */
