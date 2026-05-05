#include "menu_safelight.h"

#define LOG_TAG "menu_safelight"
#include <elog.h>

#include <stdio.h>
#include <string.h>

#include "display.h"
#include "illum_controller.h"
#include "settings.h"
#include "safelight_calibration.h"
#include "keypad.h"

static const char *safelight_control_str(safelight_control_t control);
static menu_result_t menu_settings_safelight_mode(safelight_config_t *safelight_config);
static menu_result_t menu_safelight_config_control_test(const safelight_config_t *safelight_config);

menu_result_t menu_safelight_config()
{
    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;
    safelight_config_t safelight_config;
    bool config_changed = false;
    const char *title = "Safelight Configuration";
    char buf[512];

    /* Load safelight configuration */
    if (!settings_get_safelight_config(&safelight_config)) {
        log_i("No valid config, using default");
    }

    do {
        size_t offset = 0;
        const char *val_str;
        const bool dmx_options = safelight_config.control == SAFELIGHT_CONTROL_DMX || safelight_config.control == SAFELIGHT_CONTROL_BOTH;

        switch(safelight_config.mode) {
        case SAFELIGHT_MODE_OFF:
            val_str = "Off";
            break;
        case SAFELIGHT_MODE_ON:
            val_str = "Enlarger";
            break;
        case SAFELIGHT_MODE_AUTO:
        default:
            val_str = "Meter/Print";
            break;
        case SAFELIGHT_MODE_METER:
            val_str = "Metering";
            break;
        }
        offset += menu_build_padded_str_row(buf + offset, "Safelight mode", val_str);

        offset += menu_build_padded_format_row(buf + offset, "Turn off delay", "%dms", safelight_config.turn_off_delay);

        offset += menu_build_padded_str_row(buf + offset, "Power control", safelight_control_str(safelight_config.control));

        if (dmx_options) {
            offset += sprintf(buf + offset, "DMX address                [%3d]\n",
                safelight_config.dmx_address + 1);

            if (safelight_config.dmx_wide_mode) {
                val_str = "16-bit";
            } else {
                val_str = "8-bit";
            }
            offset += menu_build_padded_str_row(buf + offset, "DMX resolution", val_str);

            val_str = "DMX brightness value";
            if (safelight_config.dmx_wide_mode) {
                offset += menu_build_padded_format_row(buf + offset, val_str, "%5d", safelight_config.dmx_on_value);
            } else {
                offset += menu_build_padded_format_row(buf + offset, val_str, "%3d", (uint8_t)safelight_config.dmx_on_value);
            }
        }

        offset += sprintf(buf + offset, "*** Test Safelights ***\n");
        sprintf(buf + offset, "*** Measure Off Delay ***");

        option = display_selection_list(title, option, buf);

        if (option == 1) {
            menu_result = menu_settings_safelight_mode(&safelight_config);
            if (menu_result == MENU_OK) { config_changed = true; }
        } else if (option == 2) {
            uint16_t value_sel = safelight_config.turn_off_delay;
            if (display_input_value_u16_inc(
                    "Turn Off Delay",
                    "Amount of time the safelights\n"
                    "take to fully turn off.\n",
                    "", &value_sel, 0, 10000, 5, "ms", 10, 100) == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                safelight_config.turn_off_delay = value_sel;
                config_changed = true;
            }
        } else if (option == 3) {
            if (safelight_config.control < SAFELIGHT_CONTROL_BOTH) {
                safelight_config.control++;
            } else {
                safelight_config.control = 0;
            }
            config_changed = true;
        } else if (dmx_options && option == 4) {
            uint16_t value_sel = safelight_config.dmx_address + 1;
            if (display_input_value_u16(
                "DMX Address",
                "Channel to which the safelights\n"
                "have been assigned.\n",
                "", &value_sel, 1, 512, 3, "") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                safelight_config.dmx_address = value_sel - 1;
                config_changed = true;
            }
        } else if (dmx_options && option == 5) {
            if (safelight_config.dmx_wide_mode) {
                safelight_config.dmx_wide_mode = false;
                safelight_config.dmx_on_value = (safelight_config.dmx_on_value & 0xFF00) >> 8;
            } else {
                safelight_config.dmx_wide_mode = true;
                if (safelight_config.dmx_on_value == 0xFF) {
                    safelight_config.dmx_on_value = 0xFFFF;
                } else {
                    safelight_config.dmx_on_value = (safelight_config.dmx_on_value & 0xFF) << 8;
                }
            }
            config_changed = true;
        } else if (dmx_options && option == 6) {
            if (safelight_config.dmx_wide_mode) {
                uint16_t value_sel = safelight_config.dmx_on_value;
                if (display_input_value_u16(
                    "DMX Brightness Value",
                    "Value to send when the\n"
                    "safelights are turned on.\n",
                    "", &value_sel, 0, 65535, 5, " / 65535") == UINT8_MAX) {
                    menu_result = MENU_TIMEOUT;
                    } else {
                        safelight_config.dmx_on_value = value_sel;
                        config_changed = true;
                    }
            } else {
                uint8_t value_sel = safelight_config.dmx_on_value;
                if (display_input_value(
                    "DMX Brightness Value",
                    "Value to send when the\n"
                    "safelights are turned on.\n",
                    "", &value_sel, 0, 255, 3, " / 255") == UINT8_MAX) {
                    menu_result = MENU_TIMEOUT;
                    } else {
                        safelight_config.dmx_on_value = value_sel;
                        config_changed = true;
                    }
            }
        } else if (option == (dmx_options ? 7 : 4)) {
            /* Test Safelights */
            menu_result_t sub_result = menu_safelight_config_control_test(&safelight_config);
            if (sub_result == MENU_OK) {
                config_changed = true;
            } else if (sub_result == MENU_TIMEOUT) {
                menu_result = MENU_TIMEOUT;
                break;
            }
        } else if (option == (dmx_options ? 8 : 5)) {
            menu_result_t sub_result = menu_safelight_calibration(&safelight_config);
            if (sub_result == MENU_OK) {
                config_changed = true;
            } else if (sub_result == MENU_TIMEOUT) {
                menu_result = MENU_TIMEOUT;
                break;
            }
        } else if (option == 0 && config_changed) {
            /* Skip this code if the original config is valid and unchanged */
            safelight_config_t orig_config;
            if (settings_get_safelight_config(&orig_config)) {
                if (safelight_config_compare(&orig_config, &safelight_config)) {
                    break;
                }
            }

            menu_result_t sub_result = menu_confirm_cancel(title);
            if (sub_result == MENU_SAVE) {
                menu_result = MENU_SAVE;
            } else if (sub_result == MENU_OK || sub_result == MENU_TIMEOUT) {
                menu_result = sub_result;
                break;
            } else if (sub_result == MENU_CANCEL) {
                option = 1;
            }
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }

    } while (option > 0 && menu_result != MENU_TIMEOUT);

    if (menu_result == MENU_SAVE) {
        if (safelight_config.control == SAFELIGHT_CONTROL_RELAY) {
            /* If DMX isn't enabled, reset all DMX fields to their defaults */
            safelight_config.dmx_address = 0;
            safelight_config.dmx_wide_mode = false;
            safelight_config.dmx_on_value = 255;
        }
        settings_set_safelight_config(&safelight_config);
    }

    illum_controller_refresh();
    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

    return menu_result;
}

const char *safelight_control_str(safelight_control_t control)
{
    switch(control) {
    case SAFELIGHT_CONTROL_DMX:
        return "DMX";
    case SAFELIGHT_CONTROL_BOTH:
        return "Relay+DMX";
    case SAFELIGHT_CONTROL_RELAY:
    default:
        return "Relay";
    }
}

menu_result_t menu_settings_safelight_mode(safelight_config_t *safelight_config)
{
    menu_result_t menu_result = MENU_OK;

    uint8_t option;
    switch (safelight_config->mode) {
    case SAFELIGHT_MODE_OFF:
        option = 1;
        break;
    case SAFELIGHT_MODE_ON:
        option = 2;
        break;
    case SAFELIGHT_MODE_AUTO:
    default:
        option = 3;
        break;
    case SAFELIGHT_MODE_METER:
        option = 4;
        break;
    }

    do {
        option = display_selection_list(
            "Safelight Mode", option,
            "Safelight always off\n"
            "Off when the enlarger is on\n"
            "Off for metering and printing\n"
            "Only off for metering");

        if (option == 1) {
            safelight_config->mode = SAFELIGHT_MODE_OFF;
            break;
        } else if (option == 2) {
            safelight_config->mode = SAFELIGHT_MODE_ON;
            break;
        } else if (option == 3) {
            safelight_config->mode = SAFELIGHT_MODE_AUTO;
            break;
        } else if (option == 4) {
            safelight_config->mode = SAFELIGHT_MODE_METER;
            break;
        } else if (option == 0) {
            menu_result = MENU_CANCEL;
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}

menu_result_t menu_safelight_config_control_test(const safelight_config_t *safelight_config)
{
    char buf[256];
    menu_result_t menu_result = MENU_OK;
    bool safelight_on = false;

    const uint32_t config_timeout = settings_get_menu_timeout();
    const int key_wait = (config_timeout == 0) ? -1 : (int)config_timeout;

    menu_safelight_test_enable(safelight_config, true);

    for (;;) {
        sprintf(buf,
            "\n\n"
            "%s control [%s]",
            safelight_control_str(safelight_config->control),
            safelight_on ? "**" : "  ");
        display_static_list("Safelight Test", buf);

        keypad_event_t keypad_event;
        HAL_StatusTypeDef ret = keypad_wait_for_event(&keypad_event, key_wait);
        if (ret == HAL_OK) {
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_FOCUS)) {
                safelight_on = !safelight_on;
                menu_safelight_test_toggle(safelight_config, safelight_on);
            } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                break;
            }

        } else if (ret == HAL_TIMEOUT) {
            menu_result = MENU_TIMEOUT;
            break;
        }
    }

    menu_safelight_test_enable(safelight_config, false);

    illum_controller_refresh();
    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

    return menu_result;
}
