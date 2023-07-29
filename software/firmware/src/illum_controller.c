#include "illum_controller.h"

#include <FreeRTOS.h>
#include <semphr.h>
#include <cmsis_os.h>

#define LOG_TAG "illum_controller"
#include <elog.h>

#include "relay.h"
#include "display.h"
#include "led.h"
#include "dmx.h"
#include "settings.h"
#include "util.h"

static osMutexId_t illum_mutex;
static const osMutexAttr_t illum_mutex_attributes = {
  .name = "illum_mutex",
  .attr_bits = osMutexRecursive,
};

static safelight_config_t illum_safelight_config;
static illum_safelight_t illum_safelight = ILLUM_SAFELIGHT_HOME;
static bool illum_blackout = false;
static bool illum_screenshot_mode = false;

static void illum_controller_set_safelight(bool enabled);

void illum_controller_init()
{
    illum_mutex = osMutexNew(&illum_mutex_attributes);
    if (!illum_mutex) {
        log_e("osMutexNew error");
        return;
    }
}

void illum_controller_refresh()
{
    bool safelight_dmx = false;
    bool enlarger_dmx = false;

    osMutexAcquire(illum_mutex, portMAX_DELAY);

    settings_get_safelight_config(&illum_safelight_config);

    /* Check if DMX is needed for safelight control */
    if (illum_safelight_config.control == SAFELIGHT_CONTROL_DMX || illum_safelight_config.control == SAFELIGHT_CONTROL_BOTH) {
        safelight_dmx = true;
    }

    /* Check if DMX is needed for enlarger control */
    uint8_t index = settings_get_default_enlarger_config_index();
    settings_get_enlarger_config_dmx_control(&enlarger_dmx, index);

    /* Enable or disable the DMX controller accordingly */
    if (safelight_dmx || enlarger_dmx) {
        if (dmx_get_port_state() == DMX_PORT_DISABLED) {
            dmx_enable();
        }
        if (dmx_get_port_state() == DMX_PORT_ENABLED_IDLE) {
            dmx_start();
        }
        dmx_clear_frame(false);
    } else {
        if (dmx_get_port_state() == DMX_PORT_ENABLED_TRANSMITTING) {
            dmx_stop();
        }
        if (dmx_get_port_state() == DMX_PORT_ENABLED_IDLE) {
            dmx_disable();
        }
    }

    osMutexRelease(illum_mutex);
}

void illum_controller_safelight_state(illum_safelight_t mode)
{
    osMutexAcquire(illum_mutex, portMAX_DELAY);

    if (illum_safelight != mode) {
        log_d("safelight_state: %d", mode);
        illum_safelight = mode;
    }

    if (!illum_blackout) {
        const safelight_mode_t setting = illum_safelight_config.mode;
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

        illum_controller_set_safelight(safelight_enabled);
    } else {
        illum_controller_set_safelight(false);
    }

    osMutexRelease(illum_mutex);
}

void illum_controller_set_safelight(bool enabled)
{
    bool toggle_relay = false;
    bool toggle_dmx = false;

    switch (illum_safelight_config.control) {
    case SAFELIGHT_CONTROL_DMX:
        toggle_dmx = true;
        break;
    case SAFELIGHT_CONTROL_BOTH:
        toggle_relay = true;
        toggle_dmx = true;
        break;
    case SAFELIGHT_CONTROL_RELAY:
    default:
        toggle_relay = true;
        break;
    }

    /* Make sure the safelight relay is turned off if not enabled */
    if (!toggle_relay && relay_safelight_is_enabled()) {
        relay_safelight_enable(false);
    }

    /* Toggle the relay, if configured */
    if (toggle_relay) {
        relay_safelight_enable(enabled);
    }

    /* Toggle the DMX output channel, if configured */
    if (toggle_dmx) {
        uint8_t frame[2] = {0};
        if (illum_safelight_config.dmx_wide_mode) {
            if (enabled) {
                conv_u16_array(frame, illum_safelight_config.dmx_on_value);
            }
            dmx_set_frame(illum_safelight_config.dmx_address, frame, 2, false);
        } else {
            if (enabled) {
                frame[0] = (uint8_t)illum_safelight_config.dmx_on_value;
            }
            dmx_set_frame(illum_safelight_config.dmx_address, frame, 1, false);
        }
    }
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
