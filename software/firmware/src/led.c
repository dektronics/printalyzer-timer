#include "led.h"

#include <string.h>
#include <stdint.h>
#include <math.h>

#define LOG_TAG "led"
#include <elog.h>

#include "aw9523.h"

/* Handle to I2C peripheral used by the keypad controller */
static I2C_HandleTypeDef *led_i2c = NULL;
static osMutexId_t led_i2c_mutex = NULL;

static bool led_initialized = false;
static uint8_t led_state[12];

static const uint16_t led_map[] = {
    LED_START_LOWER, LED_FOCUS_LOWER, LED_INC_EXPOSURE, LED_DEC_EXPOSURE,
    LED_INC_CONTRAST, LED_DEC_CONTRAST,
    LED_TEST_STRIP, LED_ADD_ADJUSTMENT, LED_CANCEL, LED_MENU,
    LED_START_UPPER, LED_FOCUS_UPPER
};

HAL_StatusTypeDef led_init(I2C_HandleTypeDef *hi2c, osMutexId_t i2c_mutex)
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("led_init");

    if (!hi2c || !i2c_mutex) {
        return HAL_ERROR;
    }

    if (led_initialized) {
        return HAL_ERROR;
    }

    led_i2c = hi2c;
    led_i2c_mutex = i2c_mutex;

    memset(led_state, 0, sizeof(led_state));

    osMutexAcquire(led_i2c_mutex, portMAX_DELAY);
    ret = aw9523_init(led_i2c);
    osMutexRelease(led_i2c_mutex);

    if (ret != HAL_OK) {
        log_e("Unable to initialize LED driver");
        return ret;
    }
    led_initialized = true;

    /* Try to set all LEDs to the off state */
    osMutexAcquire(led_i2c_mutex, portMAX_DELAY);
    for (uint8_t i = 0; i < 12; i++) {
        aw9523_set_led(led_i2c, i, 0);
    }
    osMutexRelease(led_i2c_mutex);

    return ret;
}

HAL_StatusTypeDef led_set_value(led_t led, uint8_t value)
{
    HAL_StatusTypeDef ret = HAL_OK;
    if (!led_initialized) { return HAL_ERROR; }

    osMutexAcquire(led_i2c_mutex, portMAX_DELAY);
    for (uint8_t i = 0; i < 12; i++) {
        if ((led & led_map[i]) != 0) {
            if (led_state[i] != value) {
                ret = aw9523_set_led(led_i2c, i, value);
                if (ret != HAL_OK) { break; }
                led_state[i] = value;
            }
        }
    }
    osMutexRelease(led_i2c_mutex);

    return ret;
}
