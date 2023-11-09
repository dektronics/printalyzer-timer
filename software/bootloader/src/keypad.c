#include "keypad.h"

#include <stm32f4xx_hal.h>
#include <stdio.h>
#include <stdbool.h>

#include "tca8418.h"

/* Mask to only include internal keys */
#define KEY_STATE_MASK 0x0003FFF3

/* Flag to prevent duplicate initialization */
static bool keypad_initialized = false;

/* Handle to I2C peripheral used by the keypad controller */
static I2C_HandleTypeDef *keypad_i2c;

HAL_StatusTypeDef keypad_init(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef ret = HAL_OK;

    if (keypad_initialized) {
        return HAL_OK;
    }
    if (!hi2c) {
        return HAL_ERROR;
    }

    keypad_i2c = hi2c;

    do {
        /* Initialize the controller */
        if ((ret = tca8418_init(keypad_i2c)) != HAL_OK) {
            break;
        }

        const tca8418_pins_t pins_zero = { 0x00, 0x00, 0x00 };

        /* Enable pull-ups on all GPIO pins */
        if ((ret = tca8418_gpio_pullup_disable(keypad_i2c, &pins_zero)) != HAL_OK) {
            break;
        }

        /* Set pins to GPIO mode */
        if ((ret = tca8418_kp_gpio_select(keypad_i2c, &pins_zero)) != HAL_OK) {
            break;
        }

        /* Set the event FIFO to disabled */
        if ((ret = tca8418_gpi_event_mode(keypad_i2c, &pins_zero)) != HAL_OK) {
            break;
        }

        /* Set GPIO direction to input */
        if ((ret = tca8418_gpio_data_direction(keypad_i2c, &pins_zero)) != HAL_OK) {
            break;
        }
    } while (0);

    if (ret != HAL_OK) {
        printf("Keypad setup error: %d\r\n", ret);
        return ret;
    }

    keypad_initialized = true;
    printf("Keypad controller initialized\r\n");
    return HAL_OK;
}

keypad_key_t keypad_poll()
{
    if (!keypad_initialized) {
        return 0U;
    }

    tca8418_pins_t pins;
    if (tca8148_get_gpio_data_status(keypad_i2c, &pins) == HAL_OK) {
        uint32_t key_state =
            (uint32_t)pins.rows
            | ((uint32_t)pins.cols_l) << 8
            | ((uint32_t)pins.cols_h) << 16;
        return ~(key_state & KEY_STATE_MASK) & KEY_STATE_MASK;
    } else {
        return 0U;
    }
}
