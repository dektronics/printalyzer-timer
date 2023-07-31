#ifndef EXPOSURE_TIMER_H
#define EXPOSURE_TIMER_H

#include <stm32f4xx_hal.h>
#include <stdbool.h>
#include "enlarger_config.h"

typedef enum {
    EXPOSURE_TIMER_START_TONE_NONE = 0,
    EXPOSURE_TIMER_START_TONE_COUNTDOWN
} exposure_timer_start_tone_t;

typedef enum {
    EXPOSURE_TIMER_END_TONE_NONE = 0,
    EXPOSURE_TIMER_END_TONE_SHORT,
    EXPOSURE_TIMER_END_TONE_REGULAR
} exposure_timer_end_tone_t;

typedef enum {
    EXPOSURE_TIMER_RATE_10_MS,
    EXPOSURE_TIMER_RATE_100_MS,
    EXPOSURE_TIMER_RATE_1_SEC
} exposure_timer_callback_rate_t;

typedef enum {
    EXPOSURE_TIMER_STATE_NONE = 0,
    EXPOSURE_TIMER_STATE_START = 1,
    EXPOSURE_TIMER_STATE_TICK = 2,
    EXPOSURE_TIMER_STATE_END = 3,
    EXPOSURE_TIMER_STATE_DONE = 4
} exposure_timer_state_t;

typedef bool (*exposure_timer_callback_t)(exposure_timer_state_t state, uint32_t time_ms, void *user_data);

typedef struct {
    /* Duration of the effective exposure (ms) */
    uint32_t exposure_time;

    /* Time delay between turning the enlarger on and the start of the timer period */
    uint16_t enlarger_on_delay;

    /* Time delay between turning the enlarger off and the end of the timer period */
    uint16_t enlarger_off_delay;

    /* Time delay between the end of the timer period and the completion of the timer process */
    uint16_t exposure_end_delay;

    /* Tone sequence to play at the start of the exposure sequence */
    exposure_timer_start_tone_t start_tone;

    /* Tone sequence to play at the end of the exposure sequence */
    exposure_timer_end_tone_t end_tone;

    /* The rate at which to invoke the callback function */
    exposure_timer_callback_rate_t callback_rate;

    /* The selected contrast grade, used for enlargers with contrast control */
    contrast_grade_t contrast_grade;

    /* Red channel value, if RGB-capable and contrast grade is unset */
    uint16_t channel_red;

    /* Green channel value, if RGB-capable and contrast grade is unset */
    uint16_t channel_green;

    /* Blue channel value, if RGB-capable and contrast grade is unset */
    uint16_t channel_blue;

    /* Callback function to be invoked at the specified rate */
    exposure_timer_callback_t timer_callback;

    /* Data to pass to the callback function */
    void *user_data;
} exposure_timer_config_t;

void exposure_timer_init(TIM_HandleTypeDef *htim);

/**
 * Set the time and delay fields in the timer configuration using
 * an enlarger configuration.
 */
void exposure_timer_set_config_time(exposure_timer_config_t *config,
    uint32_t exposure_time, const enlarger_config_t *enlarger_config);

/**
 * Set the provided configuration as the active configuration for
 * the exposure timer.
 */
void exposure_timer_set_config(const exposure_timer_config_t *config, const enlarger_control_t *control);

/**
 * Start the exposure timer process.
 * This function will block until the timer is complete, and provides all of
 * its state notifications via the configured callback.
 *
 * @return HAL_OK if the timer completed successfully, HAL_ERROR if the
 * timer could not be started, and HAL_TIMEOUT if the timer was canceled.
 */
HAL_StatusTypeDef exposure_timer_run();

/**
 * Call this function from the timer ISR at a 10ms interval
 */
void exposure_timer_notify();

#endif /* EXPOSURE_TIMER_H */
