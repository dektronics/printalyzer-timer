#include "gpio_task.h"

#include "stm32f4xx_hal.h"
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <cmsis_os.h>

#define LOG_TAG "gpio_task"
#include <elog.h>

#include "board_config.h"
#include "keypad.h"

static osMessageQueueId_t gpio_event_queue = NULL;
static const osMessageQueueAttr_t gpio_event_queue_attributes = {
    .name = "gpio_event_queue"
};


void gpio_task_run(void *argument)
{
    osSemaphoreId_t task_start_semaphore = argument;
    uint16_t gpio_pin;

    log_d("gpio_task start");

    gpio_event_queue = osMessageQueueNew(16, sizeof(uint16_t), &gpio_event_queue_attributes);
    if (!gpio_event_queue) {
        log_e("Unable to create GPIO event queue");
        return;
    }

    /* Release the startup semaphore */
    if (osSemaphoreRelease(task_start_semaphore) != osOK) {
        log_e("Unable to release task_start_semaphore");
        return;
    }

    /* Start the GPIO interrupt handling event loop */
    for (;;) {
        if(osMessageQueueGet(gpio_event_queue, &gpio_pin, NULL, portMAX_DELAY) == osOK) {
            if (gpio_pin == USB_VBUS_OC_Pin) {
                /* USB VBUS OverCurrent */
                log_d("USB VBUS OverCurrent interrupt");
            } else if(gpio_pin == KEY_INT_Pin) {
                /* Keypad controller interrupt */
                keypad_int_event_handler();
            } else if (gpio_pin == ENC_CH1_Pin) {
                /* Encoder counter interrupt, counter-clockwise rotation */
                keypad_event_t keypad_event = {
                    .key = KEYPAD_ENCODER_CCW,
                    .pressed = true
                };
                keypad_inject_event(&keypad_event);
            } else if (gpio_pin == ENC_CH2_Pin) {
                /* Encoder counter interrupt, clockwise rotation */
                keypad_event_t keypad_event = {
                    .key = KEYPAD_ENCODER_CW,
                    .pressed = true
                };
                keypad_inject_event(&keypad_event);
            } else {
                log_i("GPIO[%d] interrupt", gpio_pin);
            }
        }
    }
}

void gpio_task_notify_gpio_int(uint16_t gpio_pin)
{
    osMessageQueuePut(gpio_event_queue, &gpio_pin, 0, 0);
}

