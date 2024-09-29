#include "buzzer.h"

#include <string.h>
#include <cmsis_os.h>

#define LOG_TAG "buzzer"
#include <elog.h>

#include "pam8904e.h"
#include "settings.h"

static pam8904e_handle_t buzzer_handle = {0};
static buzzer_volume_t buzzer_volume = 0;
static uint16_t buzzer_frequency = 0;

HAL_StatusTypeDef buzzer_init(const pam8904e_handle_t *handle)
{
    log_d("buzzer_init");

    if (!handle) {
        log_e("Buzzer handle not initialized");
        return HAL_ERROR;
    }

    memcpy(&buzzer_handle, handle, sizeof(pam8904e_handle_t));

    pam8904e_set_gain(&buzzer_handle, PAM8904E_GAIN_SHUTDOWN);
    pam8904e_stop(&buzzer_handle);
    buzzer_volume = BUZZER_VOLUME_OFF;
    buzzer_frequency = PAM8904E_FREQ_DEFAULT;

    return HAL_OK;
}

void buzzer_set_volume(buzzer_volume_t volume)
{
    switch(volume) {
    case BUZZER_VOLUME_LOW:
        pam8904e_set_gain(&buzzer_handle, PAM8904E_GAIN_1X);
        break;
    case BUZZER_VOLUME_MEDIUM:
        pam8904e_set_gain(&buzzer_handle, PAM8904E_GAIN_2X);
        break;
    case BUZZER_VOLUME_HIGH:
        pam8904e_set_gain(&buzzer_handle, PAM8904E_GAIN_3X);
        break;
    case BUZZER_VOLUME_OFF:
        pam8904e_set_gain(&buzzer_handle, PAM8904E_GAIN_SHUTDOWN);
        break;
    default:
        return;
    }
    buzzer_volume = volume;
}

buzzer_volume_t buzzer_get_volume()
{
    return buzzer_volume;
}

void buzzer_set_frequency(uint16_t freq)
{
    pam8904e_set_frequency(&buzzer_handle, freq);
    buzzer_frequency = freq;
}

uint16_t buzzer_get_frequency()
{
    return buzzer_frequency;
}

void buzzer_start()
{
    pam8904e_start(&buzzer_handle);
}

void buzzer_stop()
{
    pam8904e_stop(&buzzer_handle);
}

void buzzer_sequence(buzzer_sequence_t sequence)
{
    buzzer_volume_t current_volume = buzzer_get_volume();
    uint16_t current_frequency = buzzer_get_frequency();

    buzzer_set_volume(settings_get_buzzer_volume());

    if (sequence == BUZZER_SEQUENCE_PROBE_WARNING) {
        buzzer_set_frequency(2000);
        buzzer_start();
        osDelay(pdMS_TO_TICKS(50));
        buzzer_stop();
        osDelay(pdMS_TO_TICKS(100));
        buzzer_start();
        osDelay(pdMS_TO_TICKS(50));
        buzzer_stop();
    } else if (sequence == BUZZER_SEQUENCE_PROBE_ERROR) {
        buzzer_set_frequency(500);
        buzzer_start();
        osDelay(pdMS_TO_TICKS(100));
        buzzer_stop();
        osDelay(pdMS_TO_TICKS(100));
        buzzer_start();
        osDelay(pdMS_TO_TICKS(100));
        buzzer_stop();
    }

    buzzer_set_volume(current_volume);
    buzzer_set_frequency(current_frequency);
}