#include "menu_safelight.h"

#define LOG_TAG "menu_safelight"
#include <elog.h>

#include <stdio.h>
#include <string.h>

#include "display.h"
#include "illum_controller.h"
#include "settings.h"
#include "util.h"

static menu_result_t menu_settings_safelight_mode(safelight_config_t *safelight_config);

menu_result_t menu_safelight_config()
{
    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;
    safelight_config_t safelight_config;
    bool config_changed = false;
    char buf[512];

    /* Load safelight configuration */
    if (!settings_get_safelight_config(&safelight_config)) {
        log_i("No valid config, using default");
    }

    do {
        size_t offset = 0;
        const char *val_str;
        switch(safelight_config.mode) {
        case SAFELIGHT_MODE_OFF:
            val_str = "Off";
            break;
        case SAFELIGHT_MODE_ON:
            val_str = "On";
            break;
        case SAFELIGHT_MODE_AUTO:
        default:
            val_str = "Auto";
            break;
        }
        offset += menu_build_padded_str_row(buf + offset, "Safelight mode", val_str);

        switch(safelight_config.control) {
        case SAFELIGHT_CONTROL_DMX:
            val_str = "DMX";
            break;
        case SAFELIGHT_CONTROL_BOTH:
            val_str = "Relay+DMX";
            break;
        case SAFELIGHT_CONTROL_RELAY:
        default:
            val_str = "Relay";
            break;
        }
        offset += menu_build_padded_str_row(buf + offset, "Power control", val_str);

        if (safelight_config.control == SAFELIGHT_CONTROL_DMX || safelight_config.control == SAFELIGHT_CONTROL_BOTH) {
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
        buf[offset - 1] = '\0';

        option = display_selection_list("Safelight Configuration", option, buf);

        if (option == 1) {
            menu_result = menu_settings_safelight_mode(&safelight_config);
            if (menu_result == MENU_OK) { config_changed = true; }
        } else if (option == 2) {
            if (safelight_config.control < SAFELIGHT_CONTROL_BOTH) {
                safelight_config.control++;
            } else {
                safelight_config.control = 0;
            }
            config_changed = true;
        } else if (option == 3) {
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
        } else if (option == 4) {
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
        } else if (option == 5) {
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
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }

    } while (option > 0 && menu_result != MENU_TIMEOUT);

    if (config_changed) {
        settings_set_safelight_config(&safelight_config);
        illum_controller_refresh();
        illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);
    }

    return menu_result;
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
    }

    do {
        option = display_selection_list(
            "Safelight Mode", option,
            "Off\n"
            "On\n"
            "Auto");

        if (option == 1) {
            safelight_config->mode = SAFELIGHT_MODE_OFF;
            break;
        } else if (option == 2) {
            safelight_config->mode = SAFELIGHT_MODE_ON;
            break;
        } else if (option == 3) {
            safelight_config->mode = SAFELIGHT_MODE_AUTO;
            break;
        } else if (option == 0) {
            menu_result = MENU_CANCEL;
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}
