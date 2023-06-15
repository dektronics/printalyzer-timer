#include "main_task.h"

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <cmsis_os.h>

#include <stdio.h>
#include <stdint.h>
#include <math.h>

#define LOG_TAG "main_task"
#include <elog.h>

#include "exposure_timer.h"
#include "usb_host.h"
#include "display.h"
#include "led.h"
#include "keypad.h"
#include "buzzer.h"
#include "relay.h"
#include "board_config.h"
#include "state_controller.h"
#include "illum_controller.h"
#include "settings.h"
#include "app_descriptor.h"

osThreadId_t main_task_handle;
osThreadId_t gpio_queue_task_handle;

static osMessageQueueId_t gpio_event_queue = NULL;
static const osMessageQueueAttr_t gpio_event_queue_attributes = {
  .name = "gpio_event_queue"
};

/* Peripheral handles initialized before this task is started */
extern UART_HandleTypeDef huart1;
extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;
extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim9;
extern TIM_HandleTypeDef htim10;

static void main_task_start(void *argument);
static void gpio_queue_task(void *argument);

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
    display_set_brightness(settings_get_display_brightness());

    if (illum_controller_is_blackout()) {
        display_enable(false);
    } else {
        display_enable(true);
    }
}

void main_task_led_init()
{
    const stp16cpc26_handle_t led_handle = {
        .hspi = &hspi2,
        .le_gpio_port = LED_LE_GPIO_Port,
        .le_gpio_pin = LED_LE_Pin,
        .oe_tim = &htim3,
        .oe_tim_channel = TIM_CHANNEL_3,
    };

    led_init(&led_handle);
    led_set_state(LED_ILLUM_ALL);
    if (illum_controller_is_blackout()) {
        led_set_brightness(0);
    } else {
        led_set_brightness(settings_get_led_brightness());
    }
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

void main_task_exposure_timer_init()
{
    exposure_timer_init(&htim10);
}

void main_task_start(void *argument)
{
    const app_descriptor_t *app_descriptor = app_descriptor_get();
    uint32_t logo_ticks = 0;
    UNUSED(argument);

    /* Print various startup log messages */
    printf("---- %s Startup ----\r\n", app_descriptor->project_name);
    uint32_t hal_ver = HAL_GetHalVersion();
    uint8_t hal_ver_code = ((uint8_t)(hal_ver)) & 0x0F;
    uint16_t *flash_size = (uint16_t*)(FLASHSIZE_BASE);

    printf("HAL Version: %d.%d.%d%c\r\n",
        ((uint8_t)(hal_ver >> 24)) & 0x0F,
        ((uint8_t)(hal_ver >> 16)) & 0x0F,
        ((uint8_t)(hal_ver >> 8)) & 0x0F,
        hal_ver_code > 0 ? (char)hal_ver_code : ' ');
    printf("Device ID: 0x%lX\r\n", HAL_GetDEVID());
    printf("Revision ID: %ld\r\n", HAL_GetREVID());
    printf("Flash size: %dk\r\n", *flash_size);
    printf("FreeRTOS: %s\r\n", tskKERNEL_VERSION_NUMBER);
    printf("SysClock: %ldMHz\r\n", HAL_RCC_GetSysClockFreq() / 1000000);

    printf("Unique ID: %08lX%08lX%08lX\r\n",
        __bswap32(HAL_GetUIDw0()),
        __bswap32(HAL_GetUIDw1()),
        __bswap32(HAL_GetUIDw2()));

    printf("App version: %s\r\n", app_descriptor->version);
    printf("Build date: %s\r\n", app_descriptor->build_date);
    printf("Build describe: %s\r\n", app_descriptor->build_describe);
    printf("Build checksum: %08lX\r\n", __bswap32(app_descriptor->crc32));

    printf("-----------------------\r\n");
    fflush(stdout);

    /* Initialize the system settings */
    settings_init(&hi2c1);

    /* Initialize the illumination controller */
    illum_controller_init();

    /* Initialize the display */
    main_task_display_init();
    display_draw_logo();
    logo_ticks = osKernelGetTickCount();

    /* Initialize the LED driver */
    main_task_led_init();

    /* Rotary encoder */
    HAL_TIM_Encoder_Start_IT(&htim1, TIM_CHANNEL_ALL);

    /* Keypad controller */
    keypad_init(&hi2c1);
    keypad_set_blackout_callback(illum_controller_keypad_blackout_callback, NULL);

    /* Piezo buzzer */
    main_task_buzzer_init();

    /* Relay driver */
    main_task_relay_init();

    /* Countdown timer init */
    main_task_exposure_timer_init();

    /* GPIO queue task */
    gpio_event_queue = osMessageQueueNew(16, sizeof(uint16_t), &gpio_event_queue_attributes);
    gpio_queue_task_handle = osThreadNew(gpio_queue_task, NULL, &gpio_queue_task_attributes);

    /* Enable interrupts */
    HAL_NVIC_SetPriority(EXTI1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI1_IRQn);
    HAL_NVIC_SetPriority(EXTI2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI2_IRQn);
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

    /* Set the startup safelight state */
    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

    /* Initialize the USB host framework */
    if (usb_host_init() != USBH_OK) {
        log_e("Unable to initialize USB host\r\n");
    }

    /* Startup beep */
    buzzer_set_frequency(PAM8904E_FREQ_DEFAULT);
    buzzer_set_volume(settings_get_buzzer_volume());
    buzzer_start();
    osDelay(100);
    buzzer_stop();
    buzzer_set_volume(BUZZER_VOLUME_OFF);

    log_i("Startup complete");
    osDelayUntil(logo_ticks + 750);

    /* Initialize state controller */
    state_controller_init();

    /* Main state controller loop, which should never exit */
    state_controller_loop();
}

void gpio_queue_task(void *argument)
{
    uint16_t gpio_pin;
    for (;;) {
        if(osMessageQueueGet(gpio_event_queue, &gpio_pin, NULL, portMAX_DELAY) == osOK) {
            if (gpio_pin == USB_VBUS_OC_Pin) {
                /* USB VBUS OverCurrent */
                log_d("USB VBUS OverCurrent interrupt");
            } else if (gpio_pin == SENSOR_INT_Pin) {
                /* Sensor interrupt */
                GPIO_PinState state = HAL_GPIO_ReadPin(SENSOR_INT_GPIO_Port, SENSOR_INT_Pin);
                if (state == GPIO_PIN_RESET) {
                    log_d("Sensor interrupt");
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
                log_i("GPIO[%d] interrupt", gpio_pin);
            }
        }
    }
}

void main_task_notify_gpio_int(uint16_t gpio_pin)
{
    osMessageQueuePut(gpio_event_queue, &gpio_pin, 0, 0);
}

void main_task_notify_countdown_timer()
{
    exposure_timer_notify();
}
