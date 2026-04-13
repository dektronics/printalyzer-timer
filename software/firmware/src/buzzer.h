#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

#include "pam8904e.h"

typedef enum : uint8_t {
    BUZZER_VOLUME_OFF = 0,
    BUZZER_VOLUME_LOW,
    BUZZER_VOLUME_MEDIUM,
    BUZZER_VOLUME_HIGH
} buzzer_volume_t;

typedef enum : uint8_t {
    BUZZER_SEQUENCE_PROBE_START = 0,
    BUZZER_SEQUENCE_STICK_START,
    BUZZER_SEQUENCE_PROBE_SUCCESS,
    BUZZER_SEQUENCE_STICK_SUCCESS,
    BUZZER_SEQUENCE_PROBE_WARNING,
    BUZZER_SEQUENCE_PROBE_ERROR,
    BUZZER_SEQUENCE_ABORT_EXPOSURE,
    BUZZER_SEQUENCE_EXPOSURE_END_SHORT,
    BUZZER_SEQUENCE_EXPOSURE_END_REGULAR
} buzzer_sequence_t;

HAL_StatusTypeDef buzzer_init(const pam8904e_handle_t *handle);

/**
 * Start the buzzer task.
 *
 * @param argument The osSemaphoreId_t used to synchronize task startup.
 */
void task_buzzer_run(void *argument);

void buzzer_set_volume(buzzer_volume_t volume);
buzzer_volume_t buzzer_get_volume();
void buzzer_reset_volume();

void buzzer_set_frequency(uint16_t freq);
uint16_t buzzer_get_frequency();

void buzzer_start();
void buzzer_stop();

/**
 * Enable or disable the buzzer task.
 *
 * The buzzer task should only be disabled if there is a need to directly
 * call the start/stop/frequency functions from a dedicated timer interrupt,
 * such as in the case of exposure timer ticks.
 * Otherwise, the 'buzzer_beep' and 'buzzer_sequence' functions should
 * always be used.
 *
 * @param enable True to enable, false to disable
 */
void buzzer_task_enable(bool enable);

void buzzer_beep(uint16_t freq, uint16_t duration);
void buzzer_beep_blocking(uint16_t freq, uint16_t duration);

void buzzer_sequence(buzzer_sequence_t sequence);
void buzzer_sequence_blocking(buzzer_sequence_t sequence);

#endif /* BUZZER_H */
