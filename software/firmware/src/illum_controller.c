#include "illum_controller.h"

#include <FreeRTOS.h>
#include <semphr.h>
#include <cmsis_os.h>

#define LOG_TAG "illum_controller"
#include <elog.h>

#include "relay.h"
#include "display.h"
#include "led.h"
#include "settings.h"

static osMutexId_t illum_mutex;
static const osMutexAttr_t illum_mutex_attributes = {
  .name = "illum_mutex",
  .attr_bits = osMutexRecursive,
};

static illum_safelight_t illum_safelight = ILLUM_SAFELIGHT_HOME;
static bool illum_blackout = false;
static bool illum_screenshot_mode = false;

void illum_controller_init()
{
    illum_mutex = osMutexNew(&illum_mutex_attributes);
    if (!illum_mutex) {
        log_e("osMutexNew error");
        return;
    }
}

void illum_controller_safelight_state(illum_safelight_t mode)
{
    osMutexAcquire(illum_mutex, portMAX_DELAY);

    if (illum_safelight != mode) {
        log_d("safelight_state: %d", mode);
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

void illum_controller_set_screenshot_mode(bool enabled)
{
    osMutexAcquire(illum_mutex, portMAX_DELAY);
    illum_screenshot_mode = enabled;
    osMutexRelease(illum_mutex);
}

bool illum_controller_get_screenshot_mode()
{
    bool result;
    osMutexAcquire(illum_mutex, portMAX_DELAY);
    result = illum_screenshot_mode;
    osMutexRelease(illum_mutex);
    return result;
}

void illum_controller_keypad_blackout_callback(bool enabled, void *user_data)
{
    log_d("illum_controller_keypad_blackout_callback: %d", enabled);

    osMutexAcquire(illum_mutex, portMAX_DELAY);

    if (!illum_blackout && illum_screenshot_mode) {
        if (enabled) {
            log_d("Triggering display screenshot");
            display_save_screenshot();
        }
    } else {
        if (illum_blackout != enabled) {
            log_d("blackout_state: %d", enabled);
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
    }

    osMutexRelease(illum_mutex);
}
