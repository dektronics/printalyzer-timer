#include "main_task.h"

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <cmsis_os.h>

#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include <esp_log.h>

#include "usb_host.h"
#include "display.h"
#include "led.h"
#include "keypad.h"
#include "buzzer.h"
#include "relay.h"
#include "board_config.h"
#include "exposure_state.h"

osThreadId_t main_task_handle;
osThreadId_t gpio_queue_task_handle;
static xQueueHandle gpio_event_queue = NULL;

/* Peripheral handles initialized before this task is started */
extern UART_HandleTypeDef huart1;
extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;
extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim9;

static void main_task_start(void *argument);
static void gpio_queue_task(void *argument);
static void convert_exposure_to_display(display_main_elements_t *elements, const exposure_state_t *exposure);

const osThreadAttr_t main_task_attributes = {
    .name = "main_task",
    .priority = (osPriority_t) osPriorityNormal,
    .stack_size = 2048 * 4
};

const osThreadAttr_t gpio_queue_task_attributes = {
    .name = "gpio_queue_task",
    .priority = (osPriority_t) osPriorityNormal,
    .stack_size = 2048
};

static const char *TAG = "main_task";

void main_task_init(void)
{
    main_task_handle = osThreadNew(main_task_start, NULL, &main_task_attributes);
}

void main_task_display_init()
{
    const u8g2_display_handle_t display_handle = {
        .hspi = &hspi1,
        .reset_gpio_port = DISP_RES_GPIO_Port,
        .reset_gpio_pin = DISP_RES_Pin,
        .cs_gpio_port = DISP_CS_GPIO_Port,
        .cs_gpio_pin = DISP_CS_Pin,
        .dc_gpio_port = DISP_DC_GPIO_Port,
        .dc_gpio_pin = DISP_DC_Pin
    };
    display_init(&display_handle);
}

void main_task_led_init()
{
    const stp16cpc26_handle_t led_handle = {
        .hspi = &hspi2,
        .le_gpio_port = LED_LE_GPIO_Port,
        .le_gpio_pin = LED_LE_Pin,
        .oe_tim = &htim3,
        .oe_tim_channel = TIM_CHANNEL_1,
    };

    led_init(&led_handle);
}

void main_task_buzzer_init()
{
    const pam8904e_handle_t buzzer_handle = {
        .en1_gpio_port = BUZZ_EN1_GPIO_Port,
        .en1_gpio_pin = BUZZ_EN1_Pin,
        .en2_gpio_port = BUZZ_EN2_GPIO_Port,
        .en2_gpio_pin = BUZZ_EN2_Pin,
        .din_tim = &htim9,
        .din_tim_channel = TIM_CHANNEL_1
    };
    buzzer_init(&buzzer_handle);
}

void main_task_relay_init()
{
    const relay_handle_t relay_handle = {
        .enlarger_gpio_port = RELAY_ENLG_GPIO_Port,
        .enlarger_gpio_pin = RELAY_ENLG_Pin,
        .safelight_gpio_port = RELAY_SFLT_GPIO_Port,
        .safelight_gpio_pin = RELAY_SFLT_Pin
    };
    relay_init(&relay_handle);
}

void main_task_start(void *argument)
{
    UNUSED(argument);

    /* Print various startup log messages */
    printf("---- STM32 Startup ----\r\n");
    uint32_t hal_ver = HAL_GetHalVersion();
    uint8_t hal_ver_code = ((uint8_t)(hal_ver)) & 0x0F;
    uint32_t hal_sysclock = HAL_RCC_GetSysClockFreq();
    printf("HAL Version: %d.%d.%d%c\r\n",
        ((uint8_t)(hal_ver >> 24)) & 0x0F,
        ((uint8_t)(hal_ver >> 16)) & 0x0F,
        ((uint8_t)(hal_ver >> 8)) & 0x0F,
        hal_ver_code > 0 ? (char)hal_ver_code : ' ');
    printf("Revision ID: %ld\r\n", HAL_GetREVID());
    printf("Device ID: 0x%lX\r\n", HAL_GetDEVID());
    printf("FreeRTOS: %s\r\n", tskKERNEL_VERSION_NUMBER);
    printf("SysClock: %ldMHz\r\n", hal_sysclock / 1000000);

    uint8_t *uniqueId = (uint8_t*)0x1FFF7A10;
    printf("Unique ID: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\r\n",
            uniqueId[0], uniqueId[1], uniqueId[2], uniqueId[3], uniqueId[4],
            uniqueId[5], uniqueId[6], uniqueId[7], uniqueId[8], uniqueId[9],
            uniqueId[10], uniqueId[11]);

    printf("-----------------------\r\n");
    fflush(stdout);

    /* Initialize the USB host framework */
    if (usb_host_init() != USBH_OK) {
        ESP_LOGE(TAG, "Unable to initialize USB host\r\n");
    }

    /* Initialize the display */
    main_task_display_init();
    display_draw_logo();

    main_task_led_init();

    led_set_enabled(LED_ILLUM_ALL);
    led_set_brightness(1);

    /* Rotary encoder */
    HAL_TIM_Encoder_Start_IT(&htim1, TIM_CHANNEL_ALL);

    /* Keypad controller */
    keypad_init(&hi2c1);

    /* Piezo buzzer */
    main_task_buzzer_init();

    /* Relay driver */
    main_task_relay_init();

    /* GPIO queue task */
    gpio_event_queue = xQueueCreate(10, sizeof(uint16_t));
    gpio_queue_task_handle = osThreadNew(gpio_queue_task, NULL, &gpio_queue_task_attributes);

    /* Enable interrupts */
    HAL_NVIC_SetPriority(EXTI1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI1_IRQn);
    HAL_NVIC_SetPriority(EXTI2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI2_IRQn);
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

    /* Startup beep */
    buzzer_set_volume(BUZZER_VOLUME_MEDIUM);
    buzzer_start();
    osDelay(100);
    buzzer_stop();
    buzzer_set_volume(BUZZER_VOLUME_OFF);

    ESP_LOGI(TAG, "Startup complete");
    osDelay(500);

    /* Startup display elements */
    display_main_elements_t elements = {
        .tone_graph = 0,
        .contrast_grade = DISPLAY_GRADE_2_HALF,
        .time_seconds = 15,
        .time_milliseconds = 0,
        .fraction_digits = 1
    };
    exposure_state_t exposure_state;
    exposure_state_defaults(&exposure_state);
    convert_exposure_to_display(&elements, &exposure_state);
    display_draw_main_elements(&elements);

    bool adj_inc_mode = false;
    bool adj_inc_mode_swallow_release_up = false;
    bool adj_inc_mode_swallow_release_down = false;
    for (;;) {
        keypad_event_t keypad_event;
        if (keypad_wait_for_event(&keypad_event, -1) == HAL_OK) {
            //TODO Create something that takes key events and states and creates enumerated actions

            // Swallow release events from button combos that put us into
            // an alternate UI mode.
            if (adj_inc_mode_swallow_release_up && keypad_event.key == KEYPAD_INC_EXPOSURE && !keypad_event.pressed) {
                adj_inc_mode_swallow_release_up = false;
                continue;
            }
            if (adj_inc_mode_swallow_release_down && keypad_event.key == KEYPAD_DEC_EXPOSURE && !keypad_event.pressed) {
                adj_inc_mode_swallow_release_down = false;
                continue;
            }

            if (adj_inc_mode) {
                if (keypad_event.key == KEYPAD_INC_EXPOSURE
                    && (!keypad_event.pressed || keypad_event.repeated)) {
                    ESP_LOGI(TAG, "--> Inc adjustment increment");
                }
                else if (keypad_event.key == KEYPAD_DEC_EXPOSURE
                    && (!keypad_event.pressed || keypad_event.repeated)) {
                    ESP_LOGI(TAG, "--> Dec adjustment increment");
                }
                else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                    adj_inc_mode = false;
                    adj_inc_mode_swallow_release_up = false;
                    adj_inc_mode_swallow_release_down = false;
                    ESP_LOGI(TAG, "--> Adjust increment mode finished");
                }
            } else {
                if (keypad_event.key == KEYPAD_INC_EXPOSURE
                    && (!keypad_event.pressed || keypad_event.repeated)) {
                        exposure_adj_increase(&exposure_state);
                }
                else if (keypad_event.key == KEYPAD_DEC_EXPOSURE
                    && (!keypad_event.pressed || keypad_event.repeated)) {
                        exposure_adj_decrease(&exposure_state);
                }
                else if (keypad_event.key == KEYPAD_INC_CONTRAST
                    && (!keypad_event.pressed || keypad_event.repeated)) {
                    exposure_contrast_increase(&exposure_state);
                }
                else if (keypad_event.key == KEYPAD_DEC_CONTRAST
                    && (!keypad_event.pressed || keypad_event.repeated)) {
                    exposure_contrast_decrease(&exposure_state);
                }
                else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                    exposure_state_defaults(&exposure_state);
                }
                else if (((keypad_event.key == KEYPAD_INC_EXPOSURE && keypad_is_key_pressed(&keypad_event, KEYPAD_DEC_EXPOSURE))
                    || (keypad_event.key == KEYPAD_DEC_EXPOSURE && keypad_is_key_pressed(&keypad_event, KEYPAD_INC_EXPOSURE)))
                    && keypad_event.pressed && !adj_inc_mode) {
                    adj_inc_mode = true;
                    adj_inc_mode_swallow_release_up = true;
                    adj_inc_mode_swallow_release_down = true;
                    ESP_LOGI(TAG, "--> Adjust increment mode");
                }
            }
        }
        convert_exposure_to_display(&elements, &exposure_state);
        display_draw_main_elements(&elements);
    }
}

void convert_exposure_to_display(display_main_elements_t *elements, const exposure_state_t *exposure)
{
    switch (exposure->contrast_grade) {
    case CONTRAST_GRADE_00:
        elements->contrast_grade = DISPLAY_GRADE_00;
        break;
    case CONTRAST_GRADE_0:
        elements->contrast_grade = DISPLAY_GRADE_0;
        break;
    case CONTRAST_GRADE_0_HALF:
        elements->contrast_grade = DISPLAY_GRADE_0_HALF;
        break;
    case CONTRAST_GRADE_1:
        elements->contrast_grade = DISPLAY_GRADE_1;
        break;
    case CONTRAST_GRADE_1_HALF:
        elements->contrast_grade = DISPLAY_GRADE_1_HALF;
        break;
    case CONTRAST_GRADE_2:
        elements->contrast_grade = DISPLAY_GRADE_2;
        break;
    case CONTRAST_GRADE_2_HALF:
        elements->contrast_grade = DISPLAY_GRADE_2_HALF;
        break;
    case CONTRAST_GRADE_3:
        elements->contrast_grade = DISPLAY_GRADE_3;
        break;
    case CONTRAST_GRADE_3_HALF:
        elements->contrast_grade = DISPLAY_GRADE_3_HALF;
        break;
    case CONTRAST_GRADE_4:
        elements->contrast_grade = DISPLAY_GRADE_4;
        break;
    case CONTRAST_GRADE_4_HALF:
        elements->contrast_grade = DISPLAY_GRADE_4_HALF;
        break;
    case CONTRAST_GRADE_5:
        elements->contrast_grade = DISPLAY_GRADE_5;
        break;
    default:
        elements->contrast_grade = DISPLAY_GRADE_NONE;
        break;
    }

    float seconds;
    float fractional;
    fractional = modff(exposure->adjusted_time, &seconds);
    elements->time_seconds = seconds;
    elements->time_milliseconds = fractional * 1000.0f;

    if (exposure->adjusted_time < 10) {
        elements->fraction_digits = 2;

    } else if (exposure->adjusted_time < 100) {
        elements->fraction_digits = 1;
    } else {
        elements->fraction_digits = 0;
    }
}

void gpio_queue_task(void *argument)
{
    uint16_t gpio_pin;
    for (;;) {
        if(xQueueReceive(gpio_event_queue, &gpio_pin, portMAX_DELAY)) {
            if (gpio_pin == USB_VBUS_OC_Pin) {
                /* USB VBUS OverCurrent */
                ESP_LOGD(TAG, "USB VBUS OverCurrent interrupt");
            } else if (gpio_pin == SENSOR_INT_Pin) {
                /* Sensor interrupt */
                GPIO_PinState state = HAL_GPIO_ReadPin(SENSOR_INT_GPIO_Port, SENSOR_INT_Pin);
                if (state == GPIO_PIN_RESET) {
                    ESP_LOGD(TAG, "Sensor interrupt");
                }
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
                ESP_LOGI(TAG, "GPIO[%d] interrupt", gpio_pin);
            }
        }
    }
}

void main_task_notify_gpio_int(uint16_t gpio_pin)
{
    xQueueSendFromISR(gpio_event_queue, &gpio_pin, NULL);
}
