#ifndef BUZZER_H
#define BUZZER_H

#include "pam8904e.h"

typedef enum {
    BUZZER_VOLUME_OFF,
    BUZZER_VOLUME_LOW,
    BUZZER_VOLUME_MEDIUM,
    BUZZER_VOLUME_HIGH
} buzzer_volume_t;

typedef enum {
    BUZZER_SEQUENCE_PROBE_WARNING,
    BUZZER_SEQUENCE_PROBE_ERROR
} buzzer_sequence_t;

HAL_StatusTypeDef buzzer_init(const pam8904e_handle_t *handle);

void buzzer_set_volume(buzzer_volume_t volume);
buzzer_volume_t buzzer_get_volume();

void buzzer_set_frequency(uint16_t freq);
uint16_t buzzer_get_frequency();

void buzzer_start();
void buzzer_stop();

void buzzer_sequence(buzzer_sequence_t sequence);

#endif /* BUZZER_H */
