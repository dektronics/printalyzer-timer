#include "keypad.h"

#include <stm32f4xx_hal.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <timers.h>
#include <cmsis_os.h>

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <usbh_hid_keybd.h>

#define LOG_TAG "keypad"
#define LOG_LVL ELOG_LVL_INFO
#include <elog.h>

#include "tca8418.h"

#define KEYPAD_INDEX_MAX       14
#define KEYPAD_REPEAT_DELAY_MS 600
#define KEYPAD_REPEAT_RATE_S   25

/* Internal raw keypad event data */
typedef struct {
    keypad_key_t keycode;
    bool pressed;
    TickType_t ticks;
    bool repeated;
} keypad_raw_event_t;

/* Handle to I2C peripheral used by the keypad controller */
static I2C_HandleTypeDef *keypad_i2c = NULL;
static osMutexId_t keypad_i2c_mutex = NULL;

/* Queue for raw keypad events, which come from the controller or timers */
static osMessageQueueId_t keypad_raw_event_queue = NULL;
static const osMessageQueueAttr_t keypad_raw_event_queue_attributes = {
    .name = "keypad_raw_event_queue"
};

/* Queue for emitted keypad events, which are handled by the application */
static osMessageQueueId_t keypad_event_queue = NULL;
static const osMessageQueueAttr_t keypad_event_queue_attributes = {
    .name = "keypad_event_queue"
};

static osMessageQueueId_t keypad_encoder_event_queue = NULL;
static const osMessageQueueAttr_t keypad_encoder_event_queue_attributes = {
    .name = "keypad_encoder_event_queue"
};

static osMutexId_t keypad_event_mutex;
static const osMutexAttr_t keypad_event_mutex_attributes = {
    .name = "keypad_event_mutex"
};

static QueueSetHandle_t keypad_event_queue_set = NULL;

/* Currently known state of all keypad buttons */
static uint16_t button_state = 0;

/* Timer for button repeat events */
static TimerHandle_t button_repeat_timer;

/* Whether timer is in initial delay or repeating event mode */
static UBaseType_t button_repeat_timer_reload;

/* Callback to be notified of changes to the blackout switch state */
static keypad_blackout_callback_t blackout_callback = NULL;

/* User data for the blackout callback */
static void *blackout_callback_user_data = NULL;

/* Current blackout state */
static bool blackout_state = false;

/* Flag to track initialization state */
static bool keypad_initialized = false;

static HAL_StatusTypeDef keypad_controller_init();
static void keypad_task_loop();
static void keypad_handle_key_event(uint8_t keycode, bool pressed, TickType_t ticks);
static void keypad_handle_key_repeat(uint8_t keycode, TickType_t ticks);
static void keypad_button_repeat_timer_callback(TimerHandle_t xTimer);
static uint8_t keypad_keycode_to_index(keypad_key_t keycode);
static uint16_t keypad_pins_to_mask(const tca8418_pins_t *pins);
static bool keypad_keycode_can_repeat(keypad_key_t keycode);

void keypad_init(I2C_HandleTypeDef *hi2c, osMutexId_t i2c_mutex)
{
    if (!hi2c || !i2c_mutex) {
        return;
    }

    if (keypad_initialized) {
        return;
    }

    keypad_i2c = hi2c;
    keypad_i2c_mutex = i2c_mutex;
}

void task_keypad_run(void *argument)
{
    osSemaphoreId_t task_start_semaphore = argument;
    log_d("keypad_task start");

    if (!keypad_i2c || !keypad_i2c_mutex) {
        log_e("Keypad hardware config is not set");
        return;
    }

    /* Create the queues for key events */
    keypad_raw_event_queue = osMessageQueueNew(20, sizeof(keypad_raw_event_t), &keypad_raw_event_queue_attributes);
    if (!keypad_raw_event_queue) {
        log_e("Unable to create raw event queue");
        return;
    }

    keypad_event_queue = osMessageQueueNew(20, sizeof(keypad_event_t), &keypad_event_queue_attributes);
    if (!keypad_event_queue) {
        log_e("Unable to create event queue");
        return;
    }

    keypad_encoder_event_queue = osMessageQueueNew(1, sizeof(keypad_event_t), &keypad_encoder_event_queue_attributes);
    if (!keypad_event_queue) {
        log_e("Unable to create encoder event queue");
        return;
    }

    keypad_event_mutex = osMutexNew(&keypad_event_mutex_attributes);
    if (!keypad_event_mutex) {
        log_e("Unable to create event mutex");
        return;
    }

    keypad_event_queue_set = xQueueCreateSet(21);
    if (!keypad_event_queue_set) {
        log_e("Unable to create event queue set");
        return;
    }

    xQueueAddToSet(keypad_event_queue, keypad_event_queue_set);
    xQueueAddToSet(keypad_encoder_event_queue, keypad_event_queue_set);

    /* Clear the button state */
    button_state = 0;

    /* Create the timer to handle key repeat events */
    button_repeat_timer = xTimerCreate(
        "keypad_repeat", 1, pdFALSE, (void *)0,
        keypad_button_repeat_timer_callback);
    if (!button_repeat_timer) {
        log_e("Unable to create repeat timer");
        return;
    }
    button_repeat_timer_reload = pdFALSE;

    /* Initialize the keypad controller */
    if (keypad_controller_init() != HAL_OK) {
        log_e("Unable to initialize the keypad controller");
        return;
    }

    /* Set the initialized flag */
    keypad_initialized = true;

    /* Release the startup semaphore */
    if (osSemaphoreRelease(task_start_semaphore) != osOK) {
        log_e("Unable to release task_start_semaphore");
        return;
    }

    /* Start the main task loop */
    keypad_task_loop();
}

HAL_StatusTypeDef keypad_controller_init()
{
    HAL_StatusTypeDef ret = HAL_OK;
    tca8418_pins_t pins_initial = {0};

    log_i("Initializing keypad controller");

    osMutexAcquire(keypad_i2c_mutex, portMAX_DELAY);
    do {
        if ((ret = tca8418_init(keypad_i2c)) != HAL_OK) {
            break;
        }

        const tca8418_pins_t pins_zero = { 0x00, 0x00, 0x00 };
        const tca8418_pins_t pins_int = { 0x0F, 0xFF, 0x03 };

        /* Enable pull-ups on all GPIO pins */
        if ((ret = tca8418_gpio_pullup_disable(keypad_i2c, &pins_zero)) != HAL_OK) {
            break;
        }

        /* Set pins to GPIO mode */
        if ((ret = tca8418_kp_gpio_select(keypad_i2c, &pins_zero)) != HAL_OK) {
            break;
        }

        /* Set the event FIFO (disabled for now) */
        if ((ret = tca8418_gpi_event_mode(keypad_i2c, &pins_int)) != HAL_OK) {
            break;
        }

        /* Set GPIO direction to input */
        if ((ret = tca8418_gpio_data_direction(keypad_i2c, &pins_zero)) != HAL_OK) {
            break;
        }

        /* Disable debouncing to read startup values */
        if ((ret = tca8418_debounce_disable(keypad_i2c, &pins_int)) != HAL_OK) {
            break;
        }

        /* Get the initial pin state */
        if ((ret = tca8148_get_gpio_data_status(keypad_i2c, &pins_initial)) != HAL_OK) {
            break;
        }

        /* Enable debouncing for normal operation */
        if ((ret = tca8418_debounce_disable(keypad_i2c, &pins_zero)) != HAL_OK) {
            break;
        }

        /* Enable GPIO interrupts */
        if ((ret = tca8418_gpio_interrupt_enable(keypad_i2c, &pins_int)) != HAL_OK) {
            break;
        }

        /* Set the configuration register to enable GPIO interrupts */
        if ((ret = tca8148_set_config(keypad_i2c, TCA8418_CFG_KE_IEN)) != HAL_OK) {
            break;
        }

    } while (0);
    osMutexRelease(keypad_i2c_mutex);

    if (ret != HAL_OK) {
        log_e("Keypad setup error: %d", ret);
        return ret;
    }

    button_state = keypad_pins_to_mask(&pins_initial);

    log_i("Keypad controller initialized");

    return HAL_OK;
}

void keypad_task_loop()
{
    keypad_raw_event_t raw_event;
    for (;;) {
        if(osMessageQueueGet(keypad_raw_event_queue, &raw_event, NULL, portMAX_DELAY) == osOK) {
            if (raw_event.keycode == KEYPAD_BLACKOUT) {
                if (blackout_state != raw_event.pressed) {
                    if (blackout_callback) {
                        blackout_callback(raw_event.pressed, blackout_callback_user_data);
                    }
                    blackout_state = raw_event.pressed;
                }
            }

            if (!raw_event.repeated) {
                keypad_handle_key_event(raw_event.keycode, raw_event.pressed, raw_event.ticks);
            } else {
                keypad_handle_key_repeat(raw_event.keycode, raw_event.ticks);
            }
        }
    }
}

void keypad_set_blackout_callback(keypad_blackout_callback_t callback, void *user_data)
{
    blackout_callback = callback;
    blackout_callback_user_data = user_data;
    if (callback) {
        const int index = keypad_keycode_to_index(KEYPAD_BLACKOUT);
        blackout_state = (button_state & (1 << index)) != 0;
        callback(blackout_state, user_data);
    }
}

HAL_StatusTypeDef keypad_inject_event(const keypad_event_t *event)
{
    if (!event) {
        return HAL_ERROR;
    }

    osMutexAcquire(keypad_event_mutex, portMAX_DELAY);

    if (event->key == KEYPAD_ENCODER_CW || event->key == KEYPAD_ENCODER_CCW) {
        /* Extract the count as a signed value from the previous event */
        keypad_event_t old_event = {0};
        int encoder_count = 0;
        if (xQueuePeek(keypad_encoder_event_queue, &old_event, 0) == pdTRUE) {
            if (old_event.key == KEYPAD_ENCODER_CW) {
                encoder_count = old_event.count;
            } else if (old_event.key == KEYPAD_ENCODER_CCW) {
                encoder_count = -1 * old_event.count;
            }
        }

        /* Update the count based on the current event */
        if (event->key == KEYPAD_ENCODER_CW) {
            encoder_count++;
        } else {
            encoder_count--;
        }

        /* Generate a new event based on the new count */
        if (encoder_count == 0) {
            osMessageQueueReset(keypad_encoder_event_queue);
        } else {
            keypad_event_t updated_event = {0};
            updated_event.pressed = true;
            if (encoder_count > 0) {
                updated_event.key = KEYPAD_ENCODER_CW;
                updated_event.count = MIN(UINT8_MAX, encoder_count);
            } else {
                updated_event.key = KEYPAD_ENCODER_CCW;
                updated_event.count = MIN(UINT8_MAX, -1 * encoder_count);
            }
            xQueueOverwrite(keypad_encoder_event_queue, &updated_event);
        }
    } else {
        osMessageQueuePut(keypad_event_queue, event, 0, 0);
    }

    osMutexRelease(keypad_event_mutex);

    return HAL_OK;
}

HAL_StatusTypeDef keypad_clear_events()
{
    osMutexAcquire(keypad_event_mutex, portMAX_DELAY);
    osMessageQueueReset(keypad_event_queue);
    osMessageQueueReset(keypad_encoder_event_queue);
    osMutexRelease(keypad_event_mutex);
    return HAL_OK;
}

HAL_StatusTypeDef keypad_flush_events()
{
    keypad_event_t event;
    bzero(&event, sizeof(keypad_event_t));
    osMutexAcquire(keypad_event_mutex, portMAX_DELAY);
    osMessageQueueReset(keypad_event_queue);
    osMessageQueueReset(keypad_encoder_event_queue);
    osMessageQueuePut(keypad_event_queue, &event, 0, 0);
    osMutexRelease(keypad_event_mutex);
    return HAL_OK;
}

HAL_StatusTypeDef keypad_wait_for_event(keypad_event_t *event, int msecs_to_wait)
{
    HAL_StatusTypeDef ret = HAL_OK;
    TickType_t ticks = msecs_to_wait < 0 ? portMAX_DELAY : (msecs_to_wait / portTICK_RATE_MS);

    QueueSetMemberHandle_t queue = xQueueSelectFromSet(keypad_event_queue_set, ticks);

    osMutexAcquire(keypad_event_mutex, portMAX_DELAY);
    if (queue == keypad_event_queue) {
        if (osMessageQueueGet(keypad_event_queue, event, NULL, 0) != osOK) {
            bzero(event, sizeof(keypad_event_t));
        }
    } else if (queue == keypad_encoder_event_queue) {
        if (osMessageQueueGet(keypad_encoder_event_queue, event, NULL, 0) != osOK) {
            bzero(event, sizeof(keypad_event_t));
        }
    } else {
        ret = HAL_TIMEOUT;
    }
    osMutexRelease(keypad_event_mutex);
    return ret;
}

HAL_StatusTypeDef keypad_int_event_handler()
{
    HAL_StatusTypeDef ret = HAL_OK;
    TickType_t ticks = xTaskGetTickCount();

    if (!keypad_initialized) {
        return ret;
    }

    osMutexAcquire(keypad_i2c_mutex, portMAX_DELAY);
    do {
        uint8_t int_status;

        /*
         * Read the INT_STAT (0x02) register to determine what asserted the
         * INT line. If GPI_INT or K_INT is set, then  a key event has
         * occurred, and the event is stored in the FIFO.
         */
        ret = tca8148_get_interrupt_status(keypad_i2c, &int_status);
        if (ret != HAL_OK) {
            break;
        }
        log_d("INT_STAT: %02X (GPI=%d, K=%d)", int_status,
            (int_status & TCA8418_INT_STAT_GPI_INT) != 0,
            (int_status & TCA8418_INT_STAT_K_INT) != 0);

        /*
         * Read the KEY_LCK_EC (0x03) register, bits [3:0] to see how many
         * events are stored in FIFO.
         */
        uint8_t count;
        ret = tca8148_get_key_event_count(keypad_i2c, &count);
        if (ret != HAL_OK) {
            break;
        }
        log_d("Key event count: %d", count);

        bool key_error = false;
        bool cb_error = false;
        do {
            uint8_t keycode;
            bool pressed;
            ret = tca8148_get_next_key_event(keypad_i2c, &keycode, &pressed);
            if (ret != HAL_OK) {
                key_error = true;
                break;
            }

            if (keycode == 0 && pressed == 0) {
                /* Last key has been read, break the loop */
                break;
            }

            /*
             * Send the raw event to the queue used by the keypad
             * event processing task
             */
            keypad_raw_event_t raw_event = {
                .keycode = keycode,
                .pressed = pressed,
                .ticks = ticks,
                .repeated = false
            };
            osMessageQueuePut(keypad_raw_event_queue, &raw_event, 0, 0);

        } while (!key_error && !cb_error);

        if (key_error) {
            break;
        }

        /* Read the GPIO INT STAT registers */
        tca8418_pins_t int_pins;
        ret = tca8148_get_gpio_interrupt_status(keypad_i2c, &int_pins);
        if (ret != HAL_OK) {
            break;
        }
        log_d("GPIO_INT_STAT: %02X %02X %02X", int_pins.rows, int_pins.cols_l, int_pins.cols_h);

        /*
         * Reset the INT_STAT interrupt flag which was causing the interrupt
         * by writing a 1 to the specific bit.
         */
        ret = tca8148_set_interrupt_status(keypad_i2c, int_status);
        if (ret != HAL_OK) {
            break;
        }
    } while (0);
    osMutexRelease(keypad_i2c_mutex);

    return ret;
}

void keypad_handle_key_event(uint8_t keycode, bool pressed, TickType_t ticks)
{
    /* Update the button state information */
    TickType_t tick_duration = 0;
    uint8_t index = keypad_keycode_to_index(keycode);
    if (index < KEYPAD_INDEX_MAX) {
        uint16_t mask = 1 << index;
        if (pressed) {
            button_state |= mask;
        } else {
            button_state &= ~mask;
        }
    }

    /* Limit the max duration we can report */
    if (tick_duration > UINT16_MAX) {
        tick_duration = UINT16_MAX;
    }

    /* Handle keys that can repeat */
    if (keypad_keycode_can_repeat(keycode)) {
        if (pressed) {
            /* Pressing a repeatable key should reset and restart the repeat timer. */
            if (xTimerIsTimerActive(button_repeat_timer) == pdTRUE) {
                xTimerStop(button_repeat_timer, portMAX_DELAY);
            }

            uint32_t keycode_id = keycode;
            xTimerChangePeriod(button_repeat_timer, pdMS_TO_TICKS(KEYPAD_REPEAT_DELAY_MS), portMAX_DELAY);
            vTimerSetReloadMode(button_repeat_timer, pdFALSE);
            button_repeat_timer_reload = pdFALSE;
            vTimerSetTimerID(button_repeat_timer, (void *)keycode_id);
            xTimerStart(button_repeat_timer, portMAX_DELAY);
        } else {
            /* Releasing a repeatable key should stop the repeat timer */
            if (xTimerIsTimerActive(button_repeat_timer) == pdTRUE) {
                xTimerStop(button_repeat_timer, portMAX_DELAY);
            }
        }
    }

    /* Generate the keypad event */
    keypad_event_t keypad_event = {
        .key = keycode,
        .pressed = pressed,
        .repeated = false,
        .keypad_state = button_state
    };
    log_d("Key event: key=%d, pressed=%d, state=%04X", keycode, pressed, button_state);

    osMutexAcquire(keypad_event_mutex, portMAX_DELAY);
    osMessageQueuePut(keypad_event_queue, &keypad_event, 0, 0);
    osMutexRelease(keypad_event_mutex);
}

void keypad_handle_key_repeat(uint8_t keycode, TickType_t ticks)
{
    /*
     * Make sure the repeated key is still pressed, and shortcut out if
     * it is not.
     */
    int index = keypad_keycode_to_index(keycode);
    if (index < KEYPAD_INDEX_MAX && !(button_state & (1 << index))) {
        xTimerStop(button_repeat_timer, portMAX_DELAY);
        return;
    }

    if (button_repeat_timer_reload == pdFALSE) {
        /* The initial period elapsed, so reconfigure for key repeat */
        xTimerStop(button_repeat_timer, portMAX_DELAY);
        xTimerChangePeriod(button_repeat_timer, pdMS_TO_TICKS(1000) / KEYPAD_REPEAT_RATE_S, portMAX_DELAY);
        vTimerSetReloadMode(button_repeat_timer, pdTRUE);
        button_repeat_timer_reload = pdTRUE;
        xTimerStart(button_repeat_timer, portMAX_DELAY);
    }

    /* Generate the keypad event */
    keypad_event_t keypad_event = {
        .key = keycode,
        .pressed = true,
        .repeated = true,
        .keypad_state = button_state
    };
    log_d("Key event: key=%d, pressed=1, state=%04X (repeat)", keycode, button_state);

    osMutexAcquire(keypad_event_mutex, portMAX_DELAY);
    osMessageQueuePut(keypad_event_queue, &keypad_event, 0, 0);
    osMutexRelease(keypad_event_mutex);
}

void keypad_button_repeat_timer_callback(TimerHandle_t xTimer)
{
    TickType_t ticks = xTaskGetTickCount();
    uint32_t keycode_id = (uint32_t)pvTimerGetTimerID(xTimer);

    keypad_raw_event_t raw_event = {
        .keycode = (uint16_t)keycode_id,
        .pressed = true,
        .ticks = ticks,
        .repeated = true
    };

    osMessageQueuePut(keypad_raw_event_queue, &raw_event, 0, 0);
}

bool keypad_is_key_pressed(const keypad_event_t *event, keypad_key_t key)
{
    if (!event) { return false; }

    int index = keypad_keycode_to_index(key);
    if (index < KEYPAD_INDEX_MAX && event && event->keypad_state & (1 << index)) {
        return true;
    } else {
        return false;
    }
}

bool keypad_is_key_released_or_repeated(const keypad_event_t *event, keypad_key_t key)
{
    if (!event) { return false; }

    if (event->key == key && (!event->pressed || event->repeated)) {
        return true;
    } else {
        return false;
    }
}

bool keypad_is_key_combo_pressed(const keypad_event_t *event, keypad_key_t key1, keypad_key_t key2)
{
    if (!event) { return false; }

    if (((event->key == key1 && keypad_is_key_pressed(event, key2))
        || (event->key == key2 && keypad_is_key_pressed(event, key1)))
        && event->pressed) {
        return true;
    } else {
        return false;
    }
}

char keypad_usb_get_ascii(const keypad_event_t *event)
{
    if (!event || event->key != KEYPAD_USB_KEYBOARD) { return '\0'; }
    return (char)(event->keypad_state & 0x00FF);
}

uint8_t keypad_usb_get_keycode(const keypad_event_t *event)
{
    if (!event || event->key != KEYPAD_USB_KEYBOARD) { return 0; }
    return (uint8_t)((event->keypad_state & 0xFF00) >> 8);
}

keypad_key_t keypad_usb_get_keypad_equivalent(const keypad_event_t *event)
{
    if (!event || event->key != KEYPAD_USB_KEYBOARD) { return 0; }
    uint8_t keycode = (uint8_t)((event->keypad_state & 0xFF00) >> 8);
    keypad_key_t keypad_key = 0;

    switch (keycode) {
    case KEY_UPARROW:
    case KEY_KEYPAD_8_UP_ARROW:
        keypad_key = KEYPAD_INC_EXPOSURE;
        break;
    case KEY_DOWNARROW:
    case KEY_KEYPAD_2_DOWN_ARROW:
        keypad_key = KEYPAD_DEC_EXPOSURE;
        break;
    case KEY_LEFTARROW:
    case KEY_KEYPAD_4_LEFT_ARROW:
        keypad_key = KEYPAD_DEC_CONTRAST;
        break;
    case KEY_RIGHTARROW:
    case KEY_KEYPAD_6_RIGHT_ARROW:
        keypad_key = KEYPAD_INC_CONTRAST;
        break;
    case KEY_A:
    case KEY_EQUAL_PLUS:
    case KEY_KEYPAD_PLUS:
        keypad_key = KEYPAD_ADD_ADJUSTMENT;
        break;
    case KEY_F1:
        keypad_key = KEYPAD_MENU;
        break;
    case KEY_ESCAPE:
        keypad_key = KEYPAD_CANCEL;
        break;
    default:
        break;
    }

    return keypad_key;
}

uint8_t keypad_keycode_to_index(keypad_key_t keycode)
{
    switch (keycode) {
    case KEYPAD_START:
        return 0;
    case KEYPAD_FOCUS:
        return 1;
    case KEYPAD_INC_EXPOSURE:
        return 2;
    case KEYPAD_DEC_EXPOSURE:
        return 3;
    case KEYPAD_INC_CONTRAST:
        return 4;
    case KEYPAD_DEC_CONTRAST:
        return 5;
    case KEYPAD_ADD_ADJUSTMENT:
        return 6;
    case KEYPAD_TEST_STRIP:
        return 7;
    case KEYPAD_CANCEL:
        return 8;
    case KEYPAD_MENU:
        return 9;
    case KEYPAD_ENCODER:
        return 10;
    case KEYPAD_BLACKOUT:
        return 11;
    case KEYPAD_FOOTSWITCH:
        return 12;
    case KEYPAD_METER_PROBE:
        return 13;
    default:
        return KEYPAD_INDEX_MAX;
    }
}

uint16_t keypad_pins_to_mask(const tca8418_pins_t *pins)
{
    uint16_t mask = 0;
    if (!pins) { return mask; }

    if ((pins->rows & 0x01) == 0) {
        mask |= 1 << keypad_keycode_to_index(KEYPAD_ENCODER);
    }
    if ((pins->rows & 0x02) == 0) {
        mask |= 1 << keypad_keycode_to_index(KEYPAD_BLACKOUT);
    }
    if ((pins->rows & 0x04) == 0) {
        mask |= 1 << keypad_keycode_to_index(KEYPAD_FOOTSWITCH);
    }
    if ((pins->rows & 0x08) == 0) {
        mask |= 1 << keypad_keycode_to_index(KEYPAD_METER_PROBE);
    }

    if ((pins->cols_l & 0x01) == 0) {
        mask |= 1 << keypad_keycode_to_index(KEYPAD_START);
    }
    if ((pins->cols_l & 0x02) == 0) {
        mask |= 1 << keypad_keycode_to_index(KEYPAD_FOCUS);
    }
    if ((pins->cols_l & 0x04) == 0) {
        mask |= 1 << keypad_keycode_to_index(KEYPAD_INC_EXPOSURE);
    }
    if ((pins->cols_l & 0x08) == 0) {
        mask |= 1 << keypad_keycode_to_index(KEYPAD_DEC_EXPOSURE);
    }
    if ((pins->cols_l & 0x10) == 0) {
        mask |= 1 << keypad_keycode_to_index(KEYPAD_INC_CONTRAST);
    }
    if ((pins->cols_l & 0x20) == 0) {
        mask |= 1 << keypad_keycode_to_index(KEYPAD_DEC_CONTRAST);
    }
    if ((pins->cols_l & 0x40) == 0) {
        mask |= 1 << keypad_keycode_to_index(KEYPAD_TEST_STRIP);
    }
    if ((pins->cols_l & 0x80) == 0) {
        mask |= 1 << keypad_keycode_to_index(KEYPAD_ADD_ADJUSTMENT);
    }

    if ((pins->cols_h & 0x01) == 0) {
        mask |= 1 << keypad_keycode_to_index(KEYPAD_CANCEL);
    }
    if ((pins->cols_h & 0x02) == 0) {
        mask |= 1 << keypad_keycode_to_index(KEYPAD_MENU);
    }

    return mask;
}

bool keypad_keycode_can_repeat(keypad_key_t keycode)
{
    switch (keycode) {
    case KEYPAD_START:
    case KEYPAD_FOCUS:
    case KEYPAD_INC_EXPOSURE:
    case KEYPAD_DEC_EXPOSURE:
    case KEYPAD_INC_CONTRAST:
    case KEYPAD_DEC_CONTRAST:
    case KEYPAD_ADD_ADJUSTMENT:
    case KEYPAD_TEST_STRIP:
    case KEYPAD_CANCEL:
    case KEYPAD_MENU:
    case KEYPAD_ENCODER:
    case KEYPAD_METER_PROBE:
        return true;
    case KEYPAD_BLACKOUT:
    case KEYPAD_FOOTSWITCH:
    default:
        return false;
    }
}
