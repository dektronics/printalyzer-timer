#include "illum_controller.h"

#include <FreeRTOS.h>
#include <semphr.h>
#include <cmsis_os.h>
#include <esp_log.h>

#include "relay.h"
#include "display.h"
#include "led.h"
#include "settings.h"

static const char *TAG = "illum_controller";

//static SemaphoreHandle_t illum_mutex = NULL;
static osMutexId_t illum_mutex;
static const osMutexAttr_t illum_mutex_attributes = {
  .name = "illum_mutex",
  .attr_bits = osMutexRecursive,
};

static illum_safelight_t illum_safelight = ILLUM_SAFELIGHT_HOME;
static bool illum_blackout = false;

void illum_controller_init()
{
    illum_mutex = osMutexNew(&illum_mutex_attributes);
    if (!illum_mutex) {
        ESP_LOGE(TAG, "osMutexNew error");
        return;
    }
}

void illum_controller_safelight_state(illum_safelight_t mode)
{
    osMutexAcquire(illum_mutex, portMAX_DELAY);

    if (illum_safelight != mode) {
        ESP_LOGD(TAG, "safelight_state: %d", mode);
        illum_safelight = mode;
    }

    if (!illum_blackout) {
        safelight_mode_t setting = settings_get_safelight_mode();
        bool safelight_enabled = true;
        if (setting == SAFELIGHT_MODE_OFF) {
            safelight_enabled = false;
        } else if (setting == SAFELIGHT_MODE_ON) {
            switch (mode) {
            case ILLUM_SAFELIGHT_HOME:
                safelight_enabled = true;
                break;
            case ILLUM_SAFELIGHT_FOCUS:
                safelight_enabled = false;
                break;
            case ILLUM_SAFELIGHT_EXPOSURE:
                safelight_enabled = false;
                break;
            case ILLUM_SAFELIGHT_MEASUREMENT:
                safelight_enabled = false;
                break;
            default:
                safelight_enabled = true;
                break;
            }
        } else if (setting == SAFELIGHT_MODE_AUTO) {
            switch (mode) {
            case ILLUM_SAFELIGHT_HOME:
                safelight_enabled = true;
                break;
            case ILLUM_SAFELIGHT_FOCUS:
                safelight_enabled = true;
                break;
            case ILLUM_SAFELIGHT_EXPOSURE:
                safelight_enabled = false;
                break;
            case ILLUM_SAFELIGHT_MEASUREMENT:
                safelight_enabled = false;
                break;
            default:
                safelight_enabled = true;
                break;
            }
        }

        relay_safelight_enable(safelight_enabled);
    } else {
        relay_safelight_enable(false);
    }

    osMutexRelease(illum_mutex);
}

bool illum_controller_is_blackout()
{
    bool result;
    osMutexAcquire(illum_mutex, portMAX_DELAY);
    result = illum_blackout;
    osMutexRelease(illum_mutex);
    return result;
}

void illum_controller_keypad_blackout_callback(bool enabled, void *user_data)
{
    ESP_LOGD(TAG, "illum_controller_keypad_blackout_callback: %d", enabled);

    osMutexAcquire(illum_mutex, portMAX_DELAY);

    if (illum_blackout != enabled) {
        ESP_LOGD(TAG, "blackout_state: %d", enabled);
        illum_blackout = enabled;
    }
    illum_controller_safelight_state(illum_safelight);

    if (enabled) {
        display_enable(false);
    } else {
        display_enable(true);
    }

    if (enabled) {
        led_set_brightness(0);
    } else {
        led_set_brightness(settings_get_led_brightness());
    }

    osMutexRelease(illum_mutex);
}
