#include "buzzer.h"

#include <string.h>
#include <esp_log.h>

#include "pam8904e.h"

static pam8904e_handle_t buzzer_handle = {0};

static const char *TAG = "buzzer";

HAL_StatusTypeDef buzzer_init(const pam8904e_handle_t *handle)
{
    ESP_LOGD(TAG, "buzzer_init");

    if (!handle) {
        ESP_LOGE(TAG, "Buzzer handle not initialized");
        return HAL_ERROR;
    }

    memcpy(&buzzer_handle, handle, sizeof(pam8904e_handle_t));

    pam8904e_set_gain(&buzzer_handle, PAM8904E_GAIN_SHUTDOWN);
    pam8904e_stop(&buzzer_handle);

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
    default:
        pam8904e_set_gain(&buzzer_handle, PAM8904E_GAIN_SHUTDOWN);
        break;
    }
}

void buzzer_start()
{
    pam8904e_start(&buzzer_handle);
}

void buzzer_stop()
{
    pam8904e_stop(&buzzer_handle);
}
