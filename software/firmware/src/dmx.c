#include "dmx.h"

#include "stm32f4xx_hal.h"

#include <stdbool.h>

#define LOG_TAG "dmx"
#include <elog.h>

#include "board_config.h"

extern TIM_HandleTypeDef htim4;
extern UART_HandleTypeDef huart6;

typedef enum {
    DMX_FRAME_IDLE = 0,
    DMX_FRAME_MARK_BEFORE_BREAK,
    DMX_FRAME_BREAK,
    DMX_FRAME_MARK_AFTER_BREAK,
    DMX_FRAME_DATA
} dmx_frame_state_t;

static bool dmx_initialized = false;
static dmx_frame_state_t frame_state = DMX_FRAME_IDLE;
static uint8_t dmx_frame[513] = {0};

void dmx_init()
{
    if (dmx_initialized) {
        log_e("DMX512 controller already initialized");
        return;
    }

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

    frame_state = DMX_FRAME_IDLE;

    // Test data to send
    dmx_frame[0] = 0x00;
    dmx_frame[1] = 0x80;
    dmx_frame[2] = 0x40;
    dmx_frame[3] = 0x20;
    dmx_frame[4] = 0x10;

    dmx_initialized = true;
}

void dmx_start()
{
    if (!dmx_initialized) {
        log_e("Not Initialized");
        return;
    }

    if (frame_state != DMX_FRAME_IDLE) {
        log_w("Invalid state");
        return;
    }

    log_i("Start DMX512 output");

    /* Set TX state to high */
    HAL_GPIO_WritePin(DMX512_TX_GPIO_Port, DMX512_TX_Pin, GPIO_PIN_SET);

    /* Enable TX output */
    HAL_GPIO_WritePin(DMX512_TX_EN_GPIO_Port, DMX512_TX_EN_Pin, GPIO_PIN_SET);

    frame_state = DMX_FRAME_MARK_BEFORE_BREAK;
}

void dmx_stop()
{
    if (!dmx_initialized) {
        log_e("Not Initialized");
        return;
    }

    if (frame_state != DMX_FRAME_MARK_BEFORE_BREAK) {
        log_w("Invalid state");
        return;
    }

    /* Disable TX output */
    HAL_GPIO_WritePin(DMX512_TX_EN_GPIO_Port, DMX512_TX_EN_Pin, GPIO_PIN_RESET);

    frame_state = DMX_FRAME_IDLE;
}

void dmx_send()
{
    if (!dmx_initialized) {
        log_e("Not Initialized");
        return;
    }

    if (frame_state != DMX_FRAME_IDLE && frame_state != DMX_FRAME_MARK_BEFORE_BREAK) {
        log_w("Invalid state");
        return;
    }

    if (frame_state == DMX_FRAME_IDLE) {
        dmx_start();
    }

    log_i("Send DMX512 frame");

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
    }
}
