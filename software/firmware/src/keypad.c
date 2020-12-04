#include "keypad.h"

#include <stm32f4xx_hal.h>
#include <FreeRTOS.h>
#include <queue.h>

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <esp_log.h>
#include "tca8418.h"

static const char *TAG = "keypad";

static I2C_HandleTypeDef *keypad_i2c;
static xQueueHandle keypad_event_queue = NULL;
static bool keypad_initialized = false;

HAL_StatusTypeDef keypad_init(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef ret = HAL_OK;
    ESP_LOGI(TAG, "Initializing keypad controller");

    if (!hi2c) {
        return HAL_ERROR;
    }
    keypad_i2c = hi2c;

    // Create the queue for key events
    keypad_event_queue = xQueueCreate(10, sizeof(keypad_event_t));
    if (!keypad_event_queue) {
        return HAL_ERROR;
    }

    do {
        if ((ret = tca8418_init(keypad_i2c)) != HAL_OK) {
            break;
        }

        const tca8418_pins_t pins_zero = { 0x00, 0x00, 0x00 };
        //const tca8418_pins_t pins_int = { 0x0F, 0xFF, 0x03 };
        const tca8418_pins_t pins_int = { 0xFF, 0xFF, 0xFF };

        // Enable pull-ups on all GPIO pins
        if ((ret = tca8418_gpio_pullup_disable(keypad_i2c, &pins_zero)) != HAL_OK) {
            break;
        }

        // Set pins to GPIO mode
        if ((ret = tca8418_kp_gpio_select(keypad_i2c, &pins_zero)) != HAL_OK) {
            break;
        }

        // Set the event FIFO (disabled for now)
        if ((ret = tca8418_gpi_event_mode(keypad_i2c, &pins_int)) != HAL_OK) {
            break;
        }

        // Set GPIO direction to input
        if ((ret = tca8418_gpio_data_direction(keypad_i2c, &pins_zero)) != HAL_OK) {
            break;
        }

        // Enable GPIO interrupts
        if ((ret = tca8418_gpio_interrupt_enable(keypad_i2c, &pins_int)) != HAL_OK) {
            break;
        }

        // Set the configuration register to enable GPIO interrupts
        if ((ret = tca8148_set_config(keypad_i2c, TCA8418_CFG_KE_IEN)) != HAL_OK) {
            break;
        }
    } while (0);

    if (ret != HAL_OK) {
        ESP_LOGE(TAG, "Keypad setup error: %d", ret);
        return ret;
    }

    keypad_initialized = true;
    ESP_LOGI(TAG, "Keypad controller configured");

    return HAL_OK;
}

HAL_StatusTypeDef keypad_inject_event(const keypad_event_t *event)
{
    if (!event) {
        return HAL_ERROR;
    }

    xQueueSend(keypad_event_queue, event, 0);
    return HAL_OK;
}

HAL_StatusTypeDef keypad_clear_events()
{
    xQueueReset(keypad_event_queue);
    return HAL_OK;
}

HAL_StatusTypeDef keypad_flush_events()
{
    keypad_event_t event;
    bzero(&event, sizeof(keypad_event_t));
    xQueueReset(keypad_event_queue);
    xQueueSend(keypad_event_queue, &event, 0);
    return HAL_OK;
}

HAL_StatusTypeDef keypad_wait_for_event(keypad_event_t *event, int msecs_to_wait)
{
    TickType_t ticks = msecs_to_wait < 0 ? portMAX_DELAY : (msecs_to_wait / portTICK_RATE_MS);
    if (!xQueueReceive(keypad_event_queue, event, ticks)) {
        if (msecs_to_wait > 0) {
            return HAL_TIMEOUT;
        } else {
            bzero(event, sizeof(keypad_event_t));
        }
    }
    return HAL_OK;
}

HAL_StatusTypeDef keypad_int_event_handler()
{
    HAL_StatusTypeDef ret = HAL_OK;

    if (!keypad_initialized) {
        return ret;
    }

    do {
        uint8_t int_status;

        // Read the INT_STAT (0x02) register to determine what asserted the
        // INT line. If GPI_INT or K_INT is set, then  a key event has
        // occurred, and the event is stored in the FIFO.
        ret = tca8148_get_interrupt_status(keypad_i2c, &int_status);
        if (ret != HAL_OK) {
            break;
        }
        ESP_LOGD(TAG, "INT_STAT: %02X (GPI=%d, K=%d)", int_status,
                (int_status & TCA8418_INT_STAT_GPI_INT) != 0,
                (int_status & TCA8418_INT_STAT_K_INT) != 0);

        // Read the KEY_LCK_EC (0x03) register, bits [3:0] to see how many
        // events are stored in FIFO.
        uint8_t count;
        ret = tca8148_get_key_event_count(keypad_i2c, &count);
        if (ret != HAL_OK) {
            break;
        }
        ESP_LOGD(TAG, "Key event count: %d", count);

        bool key_error = false;
        bool cb_error = false;
        do {
            uint8_t key;
            bool pressed;
            ret = tca8148_get_next_key_event(keypad_i2c, &key, &pressed);
            if (ret != HAL_OK) {
                key_error = true;
                break;
            }

            if (key == 0 && pressed == 0) {
                // Last key has been read, break the loop
                break;
            }

            ESP_LOGD(TAG, "Key event: key=%d, pressed=%d", key, pressed);

            keypad_event_t keypad_event = {
                .key = key,
                .pressed = pressed
            };
            xQueueSend(keypad_event_queue, &keypad_event, 0);
        } while (!key_error && !cb_error);

        if (key_error) {
            break;
        }

        // Read the GPIO INT STAT registers
        tca8418_pins_t int_pins;
        ret = tca8148_get_gpio_interrupt_status(keypad_i2c, &int_pins);
        if (ret != HAL_OK) {
            break;
        }
        ESP_LOGD(TAG, "GPIO_INT_STAT: %02X %02X %02X", int_pins.rows, int_pins.cols_l, int_pins.cols_h);

        // Reset the INT_STAT interrupt flag which was causing the interrupt
        // by writing a 1 to the specific bit.
        ret = tca8148_set_interrupt_status(keypad_i2c, int_status);
        if (ret != HAL_OK) {
            break;
        }
    } while (0);
    return ret;
}
