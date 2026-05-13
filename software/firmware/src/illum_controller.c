#include "illum_controller.h"

#include <FreeRTOS.h>
#include <semphr.h>
#include <cmsis_os.h>

#define LOG_TAG "illum_controller"
#include <elog.h>

#include "keypad.h"
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

static uint16_t panel_led_normal = LED_ILLUM_ALL;
static uint16_t panel_led_bright = 0;
static uint16_t panel_led_dim = 0;

static bool panel_led_normal_changed = true;
static bool panel_led_bright_changed = true;
static bool panel_led_dim_changed = true;

static void illum_controller_set_safelight(bool enabled);
static uint8_t panel_brightness_value(illum_panel_brightness_t panel_brightness);
static void panel_brightness_update();

void illum_controller_init()
{
    illum_mutex = osMutexNew(&illum_mutex_attributes);
    if (!illum_mutex) {
        log_e("osMutexNew error");
        return;
    }

    illum_blackout = keypad_is_blackout_enabled();
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

static bool mode_safelight_enabled(illum_safelight_t mode)
{
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
    } else if (setting == SAFELIGHT_MODE_METER) {
        switch (mode) {
        case ILLUM_SAFELIGHT_HOME:
            safelight_enabled = true;
            break;
        case ILLUM_SAFELIGHT_FOCUS:
            safelight_enabled = true;
            break;
        case ILLUM_SAFELIGHT_EXPOSURE:
            safelight_enabled = true;
            break;
        case ILLUM_SAFELIGHT_MEASUREMENT:
            safelight_enabled = false;
            break;
        default:
            safelight_enabled = true;
            break;
        }
    }
    return safelight_enabled;
}

void illum_controller_safelight_state(illum_safelight_t mode)
{
    bool safelight_prev_enabled;
    bool safelight_enabled;
    uint32_t turn_off_delay;

    osMutexAcquire(illum_mutex, portMAX_DELAY);

    if (illum_safelight != mode) {
        log_d("safelight_state: %d", mode);
    }

    safelight_prev_enabled = !illum_blackout && mode_safelight_enabled(illum_safelight);
    safelight_enabled = !illum_blackout && mode_safelight_enabled(mode);
    turn_off_delay = illum_safelight_config.turn_off_delay;
    illum_safelight = mode;

    illum_controller_set_safelight(safelight_enabled);

    osMutexRelease(illum_mutex);

    if (safelight_prev_enabled && !safelight_enabled
        && (mode == ILLUM_SAFELIGHT_EXPOSURE || mode == ILLUM_SAFELIGHT_MEASUREMENT)
        && turn_off_delay > 0) {
        osDelay(turn_off_delay);
    }
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

void illum_controller_set_panel(led_t panel_led, illum_panel_brightness_t panel_brightness)
{
    osMutexAcquire(illum_mutex, portMAX_DELAY);

    uint16_t updated_led_normal = panel_led_normal;
    uint16_t updated_led_bright = panel_led_bright;
    uint16_t updated_led_dim = panel_led_dim;

    if (panel_brightness == ILLUM_BUTTON_BRIGHT) {
        updated_led_normal &= ~panel_led;
        updated_led_bright |= panel_led;
        updated_led_dim &= ~panel_led;

    } else if (panel_brightness == ILLUM_BUTTON_DIM) {
        updated_led_normal &= ~panel_led;
        updated_led_bright &= ~panel_led;
        updated_led_dim |= panel_led;
    } else {
        updated_led_normal |= panel_led;
        updated_led_bright &= ~panel_led;
        updated_led_dim &= ~panel_led;
    }

    if (panel_led_normal != updated_led_normal) {
        panel_led_normal = updated_led_normal;
        panel_led_normal_changed = true;
    }
    if (panel_led_bright != updated_led_bright) {
        panel_led_bright = updated_led_bright;
        panel_led_bright_changed = true;
    }
    if (panel_led_dim != updated_led_dim) {
        panel_led_dim = updated_led_dim;
        panel_led_dim_changed = true;
    }
    panel_brightness_update();
    osMutexRelease(illum_mutex);
}

uint8_t panel_brightness_value(illum_panel_brightness_t panel_brightness)
{
    uint8_t brightness = settings_get_led_brightness();
    if (panel_brightness == ILLUM_BUTTON_BRIGHT) {
        brightness = (brightness >= 64) ? 255 : brightness * 4;
    } else if (panel_brightness == ILLUM_BUTTON_DIM) {
        brightness = (brightness <= 4) ? 1 : brightness / 4;
    }
    return brightness;
}

void panel_brightness_update()
{
    osMutexAcquire(illum_mutex, portMAX_DELAY);
    if (!illum_blackout) {
        if (panel_led_normal_changed) {
            led_set_value(panel_led_normal, panel_brightness_value(ILLUM_BUTTON_NORMAL));
            panel_led_normal_changed = false;
        }
        if (panel_led_bright_changed) {
            led_set_value(panel_led_bright, panel_brightness_value(ILLUM_BUTTON_BRIGHT));
            panel_led_bright_changed = false;
        }
        if (panel_led_dim_changed) {
            led_set_value(panel_led_dim, panel_brightness_value(ILLUM_BUTTON_DIM));
            panel_led_dim_changed = false;
        }
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
    log_d("illum_controller_keypad_blackout_callback: %d", enabled);

    osMutexAcquire(illum_mutex, portMAX_DELAY);

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
        led_set_value(LED_ILLUM_ALL, 0);
    } else {
        if (panel_led_normal != 0) { panel_led_normal_changed = true; }
        if (panel_led_bright != 0) { panel_led_bright_changed = true; }
        if (panel_led_dim != 0) { panel_led_dim_changed = true; }
        panel_brightness_update();
    }

    osMutexRelease(illum_mutex);
}
