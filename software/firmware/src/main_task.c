#include "main_task.h"

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <cmsis_os.h>

#include <stdio.h>
#include <stdint.h>

#include <esp_log.h>

#include "usb_host.h"
#include "display.h"
#include "led.h"
#include "keypad.h"
#include "board_config.h"

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
    HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_ALL);

    /* Keypad controller */
    keypad_init(&hi2c1);

    /* GPIO queue task */
    gpio_event_queue = xQueueCreate(10, sizeof(uint16_t));
    gpio_queue_task_handle = osThreadNew(gpio_queue_task, NULL, &gpio_queue_task_attributes);

    /* Enable interrupts */
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

    for (;;) {
        keypad_event_t keypad_event;
        if (keypad_wait_for_event(&keypad_event, 0) == HAL_OK) {
            if (keypad_event.pressed && keypad_event.key == KEY_START) {
                /* Start pressed */
            } else if (keypad_event.pressed && keypad_event.key == KEY_FOCUS) {
                /* Focus pressed */
            }
        }
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
                ESP_LOGD(TAG, "Sensor interrupt");
            } else if(gpio_pin == KEY_INT_Pin) {
                /* Keypad controller interrupt */
                keypad_int_event_handler();
            } else {
                ESP_LOGI(TAG, "GPIO[%d] interrupt", gpio_pin);
            }
        }
    }
}

void main_task_notify_gpio_int(uint16_t gpio_pin)
{
    uint16_t foo = gpio_pin;
    xQueueSendFromISR(gpio_event_queue, &foo, NULL);
}
