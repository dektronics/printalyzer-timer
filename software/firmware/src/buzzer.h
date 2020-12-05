#ifndef BUZZER_H
#define BUZZER_H

#include "pam8904e.h"

typedef enum {
    BUZZER_VOLUME_OFF,
    BUZZER_VOLUME_LOW,
    BUZZER_VOLUME_MEDIUM,
    BUZZER_VOLUME_HIGH
} buzzer_volume_t;

HAL_StatusTypeDef buzzer_init(const pam8904e_handle_t *handle);

void buzzer_set_volume(buzzer_volume_t volume);
void buzzer_start();
void buzzer_stop();

#endif /* BUZZER_H */