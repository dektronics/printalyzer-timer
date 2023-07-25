#include "dmx.h"

#include "stm32f4xx_hal.h"

#include <cmsis_os.h>
#include <stdbool.h>
#include <string.h>

#define LOG_TAG "dmx"
#include <elog.h>

#include "board_config.h"

/**
 * Sending a complete DMX512 frame takes about 22ms,
 * so this number should be set to round up from there to
 * whatever frame period is desired. This is otherwise known
 * as the "BREAK to BREAK Time" in the standard.
 */
#define DMX_FRAME_PERIOD_MS 25

/**
 * Because the DMX controller depends so tightly on the interaction
 * of specific peripherals, and switching them between alternate
 * functions, ownership of those peripherals is tightly coupled
 * to this module.
 */
extern TIM_HandleTypeDef htim4;
extern UART_HandleTypeDef huart6;

typedef enum {
    DMX_FRAME_IDLE = 0,
    DMX_FRAME_MARK_BEFORE_BREAK,
    DMX_FRAME_BREAK,
    DMX_FRAME_MARK_AFTER_BREAK,
    DMX_FRAME_DATA
} dmx_frame_state_t;

typedef enum {
    DMX_CONTROL_ENABLE,
    DMX_CONTROL_DISABLE,
    DMX_CONTROL_START,
    DMX_CONTROL_STOP,
    DMX_CONTROL_SET_FRAME
} dmx_control_event_type_t;

typedef struct {
    dmx_control_event_type_t event_type;
    osStatus_t *result;
} dmx_control_event_t;

/* Queue for DMX task control events */
static osMessageQueueId_t dmx_control_queue = NULL;
static const osMessageQueueAttr_t dmx_control_queue_attrs = {
    .name = "dmx_control_queue"
};

/* Semaphore to synchronize DMX task control calls */
static osSemaphoreId_t dmx_control_semaphore = NULL;
static const osSemaphoreAttr_t dmx_control_semaphore_attrs = {
    .name = "dmx_control_semaphore"
};

/* Semaphore to synchronize DMX frame sending */
static osSemaphoreId_t dmx_frame_semaphore = NULL;
static const osSemaphoreAttr_t dmx_frame_semaphore_attrs = {
    .name = "dmx_frame_semaphore"
};

/* Mutex to protect DMX frame updates */
static osMutexId_t dmx_frame_mutex;
static const osMutexAttr_t dmx_frame_mutex_attributes = {
    .name = "dmx_frame_mutex"
};

static bool dmx_initialized = false;
static dmx_port_state_t port_state = DMX_PORT_DISABLED;
static dmx_frame_state_t frame_state = DMX_FRAME_IDLE;
static uint8_t dmx_pending_frame[513] = {0};
static bool has_pending_frame = false;
static bool pending_frame_blocking = false;
static uint8_t dmx_frame[513] = {0};

static void dmx_task_loop();
static osStatus_t dmx_control_enable();
static osStatus_t dmx_control_disable();
static osStatus_t dmx_control_start();
static osStatus_t dmx_control_stop();
static osStatus_t dmx_control_set_frame();
static void dmx_send_frame();

void task_dmx_run(void *argument)
{
    osSemaphoreId_t task_start_semaphore = argument;
    log_d("dmx_task start");

    log_i("Initializing DMX512 controller");

    /* Set initial driver enable idle states */
    HAL_GPIO_WritePin(DMX512_TX_EN_GPIO_Port, DMX512_TX_EN_Pin, GPIO_PIN_RESET); /* TX_EN is active high */
    HAL_GPIO_WritePin(DMX512_RX_EN_GPIO_Port, DMX512_RX_EN_Pin, GPIO_PIN_SET); /* RX_EN is active low */

    /* Configure TX/RX pins as GPIO */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DMX512_TX_Pin | DMX512_RX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* Set initial TX/RX pin states */
    HAL_GPIO_WritePin(DMX512_TX_GPIO_Port, DMX512_TX_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DMX512_RX_GPIO_Port, DMX512_RX_Pin, GPIO_PIN_RESET);

    port_state = DMX_PORT_DISABLED;
    frame_state = DMX_FRAME_IDLE;

    /* Create the queue for DMX task control events */
    dmx_control_queue = osMessageQueueNew(20, sizeof(dmx_control_event_t), &dmx_control_queue_attrs);
    if (!dmx_control_queue) {
        log_e("Unable to create control queue");
        return;
    }

    /* Create the semaphore used to synchronize DMX task control */
    dmx_control_semaphore = osSemaphoreNew(1, 0, &dmx_control_semaphore_attrs);
    if (!dmx_control_semaphore) {
        log_e("Unable to create control semaphore");
        return;
    }

    /* Create the semaphore used to synchronize DMX frame sending */
    dmx_frame_semaphore = osSemaphoreNew(1, 0, &dmx_frame_semaphore_attrs);
    if (!dmx_frame_semaphore) {
        log_e("Unable to create frame semaphore");
        return;
    }

    /* Create the mutex used to protect DMX frame update */
    dmx_frame_mutex = osMutexNew(&dmx_frame_mutex_attributes);
    if (!dmx_frame_mutex) {
        log_e("dmx_frame_mutex create error");
        return;
    }

    dmx_initialized = true;

    /* Release the startup semaphore */
    if (osSemaphoreRelease(task_start_semaphore) != osOK) {
        log_e("Unable to release task_start_semaphore");
        return;
    }

    /* Start the DMX task loop */
    dmx_task_loop();
}

void dmx_task_loop()
{
    dmx_control_event_t control_event;
    osStatus_t ret;

    log_d("dmx_task start");

    for (;;) {
        if (port_state == DMX_PORT_ENABLED_TRANSMITTING) {
            uint32_t ticks_start = osKernelGetTickCount();
            /* Send the next frame */
            dmx_send_frame();

            /* Block until the frame is sent */
            osSemaphoreAcquire(dmx_frame_semaphore, portMAX_DELAY);

            /* Copy any pending frame updates */
            osMutexAcquire(dmx_frame_mutex, portMAX_DELAY);
            if (has_pending_frame && !pending_frame_blocking) {
                memcpy(dmx_frame, dmx_pending_frame, sizeof(dmx_frame));
                has_pending_frame = false;
            }
            osMutexRelease(dmx_frame_mutex);

            /* Add a delay to fill out the BREAK to BREAK time */
            osDelayUntil(ticks_start + DMX_FRAME_PERIOD_MS);

            ret = osMessageQueueGet(dmx_control_queue, &control_event, NULL, 0);
        } else {
            ret = osMessageQueueGet(dmx_control_queue, &control_event, NULL, portMAX_DELAY);
        }
        if (ret != osOK) { continue; }

        /* Process the next command */
        switch (control_event.event_type) {
        case DMX_CONTROL_ENABLE:
            ret = dmx_control_enable();
            break;
        case DMX_CONTROL_DISABLE:
            ret = dmx_control_disable();
            break;
        case DMX_CONTROL_START:
            ret = dmx_control_start();
            break;
        case DMX_CONTROL_STOP:
            ret = dmx_control_stop();
            break;
        case DMX_CONTROL_SET_FRAME:
            ret = dmx_control_set_frame();
            break;
        default:
            break;
        }

        if (control_event.result) {
            *(control_event.result) = ret;
        }
        if (osSemaphoreRelease(dmx_control_semaphore) != osOK) {
            log_e("Unable to release dmx_control_semaphore");
        }
    }
}

dmx_port_state_t dmx_get_port_state()
{
    return port_state;
}

osStatus_t dmx_enable()
{
    if (!dmx_initialized) { return osErrorResource; }

    osStatus_t result = osOK;
    dmx_control_event_t control_event = {
        .event_type = DMX_CONTROL_ENABLE,
        .result = &result
    };

    osMessageQueuePut(dmx_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(dmx_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t dmx_control_enable()
{
    log_d("dmx_control_enable");

    if (frame_state != DMX_FRAME_IDLE || port_state != DMX_PORT_DISABLED) {
        log_w("Invalid state");
        return osErrorResource;
    }

    /* Set TX state to high */
    HAL_GPIO_WritePin(DMX512_TX_GPIO_Port, DMX512_TX_Pin, GPIO_PIN_SET);

    /* Enable TX output */
    HAL_GPIO_WritePin(DMX512_TX_EN_GPIO_Port, DMX512_TX_EN_Pin, GPIO_PIN_SET);

    frame_state = DMX_FRAME_MARK_BEFORE_BREAK;
    port_state = DMX_PORT_ENABLED_IDLE;

    log_i("DMX512 output enabled");

    return osOK;
}

osStatus_t dmx_disable()
{
    if (!dmx_initialized) { return osErrorResource; }

    osStatus_t result = osOK;
    dmx_control_event_t control_event = {
        .event_type = DMX_CONTROL_DISABLE,
        .result = &result
    };

    osMessageQueuePut(dmx_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(dmx_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t dmx_control_disable()
{
    log_d("dmx_control_disable");

    if (frame_state != DMX_FRAME_MARK_BEFORE_BREAK || port_state != DMX_PORT_ENABLED_IDLE) {
        log_w("Invalid state");
        return osErrorResource;
    }

    /* Disable TX output */
    HAL_GPIO_WritePin(DMX512_TX_EN_GPIO_Port, DMX512_TX_EN_Pin, GPIO_PIN_RESET);

    frame_state = DMX_FRAME_IDLE;
    port_state = DMX_PORT_DISABLED;

    log_i("DMX512 output disabled");

    return osOK;
}

osStatus_t dmx_start()
{
    if (!dmx_initialized) { return osErrorResource; }

    osStatus_t result = osOK;
    dmx_control_event_t control_event = {
        .event_type = DMX_CONTROL_START,
        .result = &result
    };

    osMessageQueuePut(dmx_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(dmx_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t dmx_control_start()
{
    log_d("dmx_control_start");

    if (frame_state != DMX_FRAME_MARK_BEFORE_BREAK || port_state != DMX_PORT_ENABLED_IDLE) {
        log_w("Invalid state");
        return osErrorResource;
    }

    port_state = DMX_PORT_ENABLED_TRANSMITTING;

    log_i("DMX512 frame output started");

    return osOK;
}

osStatus_t dmx_stop()
{
    if (!dmx_initialized) { return osErrorResource; }

    osStatus_t result = osOK;
    dmx_control_event_t control_event = {
        .event_type = DMX_CONTROL_STOP,
        .result = &result
    };

    osMessageQueuePut(dmx_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(dmx_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t dmx_control_stop()
{
    log_d("dmx_control_stop");

    if (frame_state != DMX_FRAME_MARK_BEFORE_BREAK || port_state != DMX_PORT_ENABLED_TRANSMITTING) {
        log_w("Invalid state");
        return osErrorResource;
    }

    port_state = DMX_PORT_ENABLED_IDLE;

    log_i("DMX512 frame output stopped");

    return osOK;
}

osStatus_t dmx_set_frame(uint16_t offset, const uint8_t *frame, size_t len, bool blocking)
{
    osStatus_t result = osOK;

    if (!dmx_initialized) { return osErrorResource; }

    if (offset + len > 512) {
        return osErrorParameter;
    }

    osMutexAcquire(dmx_frame_mutex, portMAX_DELAY);
    memcpy(dmx_pending_frame + offset + 1, frame, len);
    has_pending_frame = true;
    pending_frame_blocking = blocking;
    osMutexRelease(dmx_frame_mutex);

    if (blocking) {
        dmx_control_event_t control_event = {
            .event_type = DMX_CONTROL_SET_FRAME,
            .result = &result
        };

        osMessageQueuePut(dmx_control_queue, &control_event, 0, portMAX_DELAY);
        osSemaphoreAcquire(dmx_control_semaphore, portMAX_DELAY);
    }

    return result;
}

osStatus_t dmx_control_set_frame()
{
    log_d("dmx_control_set_frame");

    if (frame_state != DMX_FRAME_MARK_BEFORE_BREAK) {
        log_w("Invalid state");
        return osErrorResource;
    }

    osMutexAcquire(dmx_frame_mutex, portMAX_DELAY);
    memcpy(dmx_frame, dmx_pending_frame, sizeof(dmx_frame));
    has_pending_frame = false;
    pending_frame_blocking = false;
    osMutexRelease(dmx_frame_mutex);

    return osOK;
}

void dmx_send_frame()
{
    if (frame_state != DMX_FRAME_MARK_BEFORE_BREAK) {
        log_w("Invalid state");
        return;
    }

    /* Set TX state to low */
    HAL_GPIO_WritePin(DMX512_TX_GPIO_Port, DMX512_TX_Pin, GPIO_PIN_RESET);

    frame_state = DMX_FRAME_BREAK;

    /* Timer settings for initial break period (~116us) */
    __HAL_TIM_SET_COUNTER(&htim4, 0);
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 100); /* Timer period 100us */
    HAL_TIM_OC_Start_IT(&htim4, TIM_CHANNEL_1);
}

void dmx_timer_notify()
{
    /* Stop the timer */
    HAL_TIM_OC_Stop_IT(&htim4, TIM_CHANNEL_1);

    if (frame_state == DMX_FRAME_BREAK) {
        /* Reconfigure TX pin as UART */
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = DMX512_TX_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
        HAL_GPIO_Init(DMX512_TX_GPIO_Port, &GPIO_InitStruct);

        frame_state = DMX_FRAME_MARK_AFTER_BREAK;

        /* Start timer for Mark-After-Break (~24us) */
        __HAL_TIM_SET_COUNTER(&htim4, 0);
        __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 7);
        HAL_TIM_OC_Start_IT(&htim4, TIM_CHANNEL_1);

    } else if (frame_state == DMX_FRAME_MARK_AFTER_BREAK) {
        frame_state = DMX_FRAME_DATA;
        /* Begin UART transmission of frame data */
        HAL_UART_Transmit_DMA(&huart6, dmx_frame, sizeof(dmx_frame));
    }
}

void dmx_uart_tx_cplt()
{
    if (frame_state == DMX_FRAME_DATA) {
        /* Set TX state to high */
        HAL_GPIO_WritePin(DMX512_TX_GPIO_Port, DMX512_TX_Pin, GPIO_PIN_SET);

        /* Reconfigure TX pin as GPIO */
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = DMX512_TX_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        frame_state = DMX_FRAME_MARK_BEFORE_BREAK;

        osSemaphoreRelease(dmx_frame_semaphore);
    }
}
