#include "main_task.h"

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <cmsis_os.h>

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <machine/endian.h>

#define LOG_TAG "main_task"
#include <elog.h>

#include "exposure_timer.h"
#include "usb_host.h"
#include "display.h"
#include "led.h"
#include "gpio_task.h"
#include "keypad.h"
#include "buzzer.h"
#include "relay.h"
#include "board_config.h"
#include "state_controller.h"
#include "illum_controller.h"
#include "settings.h"
#include "app_descriptor.h"
#include "meter_probe.h"
#include "dmx.h"

/* Peripheral handles initialized before this task is started */
extern UART_HandleTypeDef huart1;
extern I2C_HandleTypeDef hi2c1;
extern SMBUS_HandleTypeDef hsmbus2;
extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi3;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim9;
extern TIM_HandleTypeDef htim10;

static void main_task_run(void *argument);
static void main_task_startup_log();
static void main_task_display_init();
static void main_task_led_init();
static void main_task_buzzer_init();
static void main_task_relay_init();
static void main_task_exposure_timer_init();
static void main_task_keypad_init();
static void main_task_enable_interrupts();
static void main_task_disable_interrupts();
extern void deinit_peripherals();

typedef struct {
    const osThreadFunc_t task_func;
    const osThreadAttr_t task_attrs;
    osThreadId_t task_handle;
} task_params_t;

static osSemaphoreId_t task_start_semaphore = NULL;
static const osSemaphoreAttr_t task_start_semaphore_attributes = {
    .name = "task_start_semaphore"
};

#define TASK_MAIN_STACK_SIZE        (8192U)
#define TASK_GPIO_STACK_SIZE        (2048U)
#define TASK_KEYPAD_STACK_SIZE      (2048U)
#define TASK_DMX_STACK_SIZE         (2048U)
#define TASK_METER_PROBE_STACK_SIZE (2048U)

static task_params_t task_list[] = {
    {
        .task_func = main_task_run,
        .task_attrs = {
            .name = "main",
            .stack_size = TASK_MAIN_STACK_SIZE,
            .priority = osPriorityNormal
        }
    },
    {
        .task_func = gpio_task_run,
        .task_attrs = {
            .name = "gpio",
            .stack_size = TASK_GPIO_STACK_SIZE,
            .priority = osPriorityNormal
        }
    },
    {
        .task_func = task_keypad_run,
        .task_attrs = {
            .name = "keypad",
            .stack_size = TASK_KEYPAD_STACK_SIZE,
            .priority = osPriorityNormal
        }
    },
    {
        .task_func = task_dmx_run,
        .task_attrs = {
            .name = "dmx",
            .stack_size = TASK_DMX_STACK_SIZE,
            .priority = osPriorityAboveNormal
        }
    },
    {
        .task_func = task_meter_probe_run,
        .task_attrs = {
            .name = "meter_probe",
            .stack_size = TASK_METER_PROBE_STACK_SIZE,
            .priority = osPriorityNormal
        }
    }
};

static osMutexId_t i2c1_mutex;
static const osMutexAttr_t i2c1_mutex_attributes = {
    .name = "i2c1_mutex",
    .attr_bits = osMutexRecursive
};

static bool main_task_running = false;

osStatus_t main_task_init()
{
    /* Create the mutex used to protect the I2C1 peripheral */
    i2c1_mutex = osMutexNew(&i2c1_mutex_attributes);
    if (!i2c1_mutex) {
        log_e("i2c1_mutex create error");
        return osErrorNoMemory;
    }

    /* Create the semaphore used to synchronize task startup */
    task_start_semaphore = osSemaphoreNew(1, 0, &task_start_semaphore_attributes);
    if (!task_start_semaphore) {
        log_e("task_start_semaphore create error");
        return osErrorNoMemory;
    }

    /* Create the main task */
    task_list[0].task_handle = osThreadNew(task_list[0].task_func, NULL, &task_list[0].task_attrs);
    if (!task_list[0].task_handle) {
        log_e("main_task create error");
        return osErrorNoMemory;
    }
    return osOK;
}

bool task_main_is_running()
{
    return main_task_running;
}

void main_task_run(void *argument)
{
    UNUSED(argument);
    const uint8_t task_count = sizeof(task_list) / sizeof(task_params_t);
    uint32_t logo_ticks = 0;

    log_d("main_task start");

    /* Print various startup log messages */
    main_task_startup_log();

    /*
     * All hardware that does not have dedicated management tasks,
     * or which has initialization functions that need to be called
     * prior to task startup, is handled here.
     */

    /* Initialize the system settings */
    settings_init(&hi2c1, i2c1_mutex);

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

    /* Piezo buzzer */
    main_task_buzzer_init();

    /* Relay driver */
    main_task_relay_init();

    /* Countdown timer init */
    main_task_exposure_timer_init();

    /* Keypad hardware configuration */
    main_task_keypad_init();

    /*
     * Hardware that does depend on dedicated management tasks
     * is initialized here as part of the startup of those tasks.
     */

    /* Loop through the remaining tasks and create them */
    log_d("Starting tasks...");
    for (uint8_t i = 1; i < task_count; i++) {
        log_d("Task: %s", task_list[i].task_attrs.name);

        /* Create the next task */
        task_list[i].task_handle = osThreadNew(task_list[i].task_func, task_start_semaphore, &task_list[i].task_attrs);
        if (!task_list[i].task_handle) {
            log_e("%s create error", task_list[i].task_attrs.name);
            continue;
        }

        /* Wait for the semaphore set once the task initializes */
        if (osSemaphoreAcquire(task_start_semaphore, portMAX_DELAY) != osOK) {
            log_e("Unable to acquire task_start_semaphore");
            return;
        }
    }
    log_d("Task start loop complete");

    /* Keypad controller post-init */
    keypad_set_blackout_callback(illum_controller_keypad_blackout_callback, NULL);

    /* Enable interrupts */
    main_task_enable_interrupts();

    /* Set the startup safelight state */
    illum_controller_refresh();
    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

    /*
     * Initialize the USB host framework.
     * This framework does have a dedicated management task, but it is
     * not handled as part of the application code.
     */
    if (!usb_host_init()) {
        log_e("Unable to initialize USB host");
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

    main_task_running = true;

    /* Main state controller loop, which should never exit */
    log_i("Starting controller loop");
    state_controller_loop();
}

void main_task_startup_log()
{
    const app_descriptor_t *app_descriptor = app_descriptor_get();

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
}

void main_task_display_init()
{
    const u8g2_display_handle_t display_handle = {
        .hspi = &hspi1,
        .reset_gpio_port = DISP_RESET_GPIO_Port,
        .reset_gpio_pin = DISP_RESET_Pin,
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
    //FIXME Replace with updated LED Driver code
#if 0
    const stp16cpc26_handle_t led_handle = {
        .hspi = &hspi3,
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
#endif
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

void main_task_keypad_init()
{
    /* Make sure the keypad controller is in reset */
    HAL_GPIO_WritePin(KEY_RESET_GPIO_Port, KEY_RESET_Pin, GPIO_PIN_RESET);
    osDelay(1);

    /* Bring the keypad controller out of reset */
    HAL_GPIO_WritePin(KEY_RESET_GPIO_Port, KEY_RESET_Pin, GPIO_PIN_SET);
    osDelay(1);

    keypad_init(&hi2c1, i2c1_mutex);
}

void main_task_enable_interrupts()
{
    /*
     * This enables all the GPIO-related interrupts, which can cause issues
     * if they fire before everything else is initialized.
     *
     * Timer, UART, and DMA interrupts are still enabled earlier in the
     * startup process, since the processes that lead them being fired
     * should depend on code that will not run until initialization is
     * complete.
     */
    HAL_NVIC_SetPriority(EXTI1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI1_IRQn);
    HAL_NVIC_SetPriority(EXTI2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI2_IRQn);
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

void main_task_disable_interrupts()
{
    HAL_NVIC_DisableIRQ(EXTI1_IRQn);
    HAL_NVIC_DisableIRQ(EXTI2_IRQn);
    HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);
}

void main_task_notify_countdown_timer()
{
    exposure_timer_notify();
}

void main_task_shutdown()
{
    log_d("Shutting down components...");

    /* Shutdown the USB host stack */
    usb_host_deinit();

    /* Disable GPIO interrupts */
    main_task_disable_interrupts();

    /* Turn off panel LEDs */
    led_set_brightness(0);

    /* Put the keypad controller into the reset state */
    HAL_GPIO_WritePin(KEY_RESET_GPIO_Port, KEY_RESET_Pin, GPIO_PIN_RESET);

    /* Wait for things to settle */
    osDelay(200);

    log_d("Stopping scheduler...");

    /* Suspend the FreeRTOS scheduler */
    vTaskSuspendAll();

    log_d("Deinitializing and restarting...");

    /* Deinitialize the peripherals */
    deinit_peripherals();

    /* Trigger a restart */
    NVIC_SystemReset();
}
