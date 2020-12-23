#include "illum_controller.h"

#include <esp_log.h>

#include "relay.h"
#include "settings.h"

static const char *TAG = "illum_controller";

static illum_safelight_t illum_safelight = ILLUM_SAFELIGHT_HOME;
static bool illum_blackout = false;

void illum_controller_safelight_state(illum_safelight_t mode)
{
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
}

void illum_controller_blackout_state(bool enabled)
{
    if (illum_blackout != enabled) {
        ESP_LOGD(TAG, "blackout_state: %d", enabled);
        illum_blackout = enabled;
    }
    illum_controller_safelight_state(illum_safelight);
    //TODO
}
