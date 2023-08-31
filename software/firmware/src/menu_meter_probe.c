#include "menu_meter_probe.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#define LOG_TAG "menu_meter_probe"
#include <elog.h>

#include "display.h"
#include "keypad.h"

#include "meter_probe.h"
#include "tsl2585.h"
#include "settings.h"
#include "illum_controller.h"
#include "enlarger_control.h"
#include "util.h"

static menu_result_t meter_probe_device_info();
static menu_result_t meter_probe_sensor_calibration();
static menu_result_t meter_probe_diagnostics();

menu_result_t menu_meter_probe()
{
    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;

    /* Attempt to start the meter probe */
    if (meter_probe_start() != osOK) {
        option = display_message(
            "Meter Probe",
            NULL,
            "\n**** Not Detected ****\n", " OK ");
        if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
        return menu_result;
    }

    /* Show the meter probe menu */
    do {
        option = display_selection_list(
            "Meter Probe", option,
            "Device Info\n"
            "Sensor Calibration\n"
            "Diagnostics");

        if (option == 1) {
            menu_result = meter_probe_device_info();
        } else if (option == 2) {
            menu_result = meter_probe_sensor_calibration();
        } else if (option == 3) {
            menu_result = meter_probe_diagnostics();
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    /* Stop the meter probe before returning */
    meter_probe_sensor_disable();
    meter_probe_stop();

    return menu_result;
}

menu_result_t meter_probe_device_info()
{
    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;
    char buf[512];

    meter_probe_device_info_t info;
    if (meter_probe_get_device_info(&info) != osOK) {
        return MENU_OK;
    }

    bool has_settings = meter_probe_has_settings();

    do {
        size_t offset = 0;
        offset += menu_build_padded_format_row(buf + offset, "Revision", "%d", info.revision);
        offset += menu_build_padded_format_row(buf + offset, "Serial Number", "%03d", info.serial);
        offset += menu_build_padded_str_row(buf + offset, "Sensor Type",
            ((info.type == METER_PROBE_TYPE_TSL2585) ? "TSL2585" : "Unknown"));
        offset += menu_build_padded_format_row(buf + offset, "Sensor ID", "%02X%02X%02X",
            info.sensor_id[0], info.sensor_id[1], info.sensor_id[2]);
        offset += menu_build_padded_format_row(buf + offset, "Memory ID", "%02X%02X%02X",
            info.memory_id[0], info.memory_id[1], info.memory_id[2]);
        offset += menu_build_padded_str_row(buf + offset, "Calibration",
            (has_settings ? "Loaded" : "Missing"));
        buf[offset - 1] = '\0';

        option = display_selection_list("Device Info", option, buf);

        if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}

menu_result_t meter_probe_sensor_calibration()
{
    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;
    char buf[512];

    meter_probe_settings_t settings;
    if (meter_probe_get_settings(&settings) != osOK) {
        return MENU_OK;
    }

    if (settings.type != METER_PROBE_TYPE_TSL2585) {
        /* Unknown meter probe type */
        return MENU_OK;
    }

    do {
        size_t offset = 0;

        for (tsl2585_gain_t gain = 0; gain < TSL2585_GAIN_MAX; gain++) {
            offset += menu_build_padded_format_row(buf + offset,
                tsl2585_gain_str(gain), "%f][%f",
                settings.settings_tsl2585.gain_cal[gain].slope,
                settings.settings_tsl2585.gain_cal[gain].offset);
        }

        buf[offset - 1] = '\0';

        option = display_selection_list("Sensor Calibration", option, buf);

        if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}

menu_result_t meter_probe_diagnostics()
{
    HAL_StatusTypeDef ret = HAL_OK;
    char buf[512];
    enlarger_config_t enlarger_config = {0};
    bool sensor_initialized = false;
    bool sensor_error = false;
    bool enlarger_enabled = false;
    bool config_changed = false;
    bool agc_changed = false;
    tsl2585_gain_t gain = 0;
    uint16_t sample_time = 0;
    uint16_t sample_count = 0;
    bool agc_enabled = false;
    bool single_shot = false;
    meter_probe_sensor_reading_t reading = {0};
    keypad_event_t keypad_event;
    bool key_changed = false;

    /* Load active enlarger config */
    uint8_t config_index = settings_get_default_enlarger_config_index();
    bool result = settings_get_enlarger_config(&enlarger_config, config_index);
    if (!(result && enlarger_config_is_valid(&enlarger_config))) {
        enlarger_config_set_defaults(&enlarger_config);
    }

    /* Make sure enlarger starts in a known state */
    illum_controller_refresh();
    enlarger_control_set_state_off(&(enlarger_config.control), false);

    if (meter_probe_sensor_set_config(TSL2585_GAIN_256X, 716, 100) == osOK) {
        gain = TSL2585_GAIN_256X;
        sample_time = 716;
        sample_count = 100;
    }

    for (;;) {
        if (!sensor_initialized) {
            if (meter_probe_sensor_enable() == osOK) {
                sensor_initialized = true;
            } else {
                log_e("Error initializing TSL2585: %d", ret);
                sensor_error = true;
            }
        }

        if (key_changed) {
            key_changed = false;
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_FOCUS)) {
                if (!enlarger_enabled) {
                    if (enlarger_control_set_state_focus(&(enlarger_config.control), false) == osOK) {
                        log_i("Meter probe focus mode enabled");
                        enlarger_enabled = true;
                    }
                } else {
                    if (enlarger_control_set_state_off(&(enlarger_config.control), false) == osOK) {
                        log_i("Meter probe focus mode disabled");
                        enlarger_enabled = false;
                    }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_EXPOSURE)) {
                if (sensor_initialized && !sensor_error) {
                    if (gain > TSL2585_GAIN_0_5X) {
                        gain--;
                        config_changed = true;
                    }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_EXPOSURE)) {
                if (sensor_initialized && !sensor_error) {
                    if (gain < TSL2585_GAIN_4096X) {
                        gain++;
                        config_changed = true;
                    }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_CONTRAST)) {
                if (sensor_initialized && !sensor_error) {
                    if (sample_count > 10) {
                        sample_count -= 10;
                        config_changed = true;
                    }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_CONTRAST)) {
                if (sensor_initialized && !sensor_error) {
                    if (sample_count < (2047 - 10)) {
                        sample_count += 10;
                        config_changed = true;
                    }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_ADD_ADJUSTMENT)) {
                if (sensor_initialized && !sensor_error) {
                    agc_enabled = !agc_enabled;
                    agc_changed = true;
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_TEST_STRIP)) {
                if (meter_probe_sensor_disable() == osOK) {
                    if (single_shot) {
                        if (meter_probe_sensor_enable() == osOK) {
                            single_shot = false;
                        } else {
                            sensor_error = true;
                        }
                    } else {
                        if (meter_probe_sensor_enable_single_shot() == osOK) {
                            single_shot = true;
                        } else {
                            sensor_error = true;
                        }
                    }
                } else {
                    sensor_error = true;
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_METER_PROBE)
                || keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_ENCODER)) {
                if (single_shot) {
                    meter_probe_sensor_trigger_next_reading();
                }
            } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                break;
            } else if (keypad_event.key == KEYPAD_USB_KEYBOARD
                && keypad_usb_get_keypad_equivalent(&keypad_event) == KEYPAD_CANCEL) {
                break;
            }
        }

        if (config_changed) {
            if (meter_probe_sensor_set_config(gain, sample_time, sample_count) == osOK) {
                config_changed = false;
            }
        }
        if (agc_changed) {
            if (agc_enabled) {
                if (meter_probe_sensor_enable_agc(sample_count) == osOK) {
                    agc_changed = false;
                }
            } else {
                if (meter_probe_sensor_disable_agc() == osOK) {
                    agc_changed = false;
                }
            }
        }

        if (meter_probe_sensor_get_next_reading(&reading, single_shot ? 10 : 1000) == osOK) {
            const uint32_t scaled_result = meter_probe_scaled_result(&reading);
            const float basic_result = meter_probe_basic_result(&reading);
            const float atime = tsl2585_integration_time_ms(reading.sample_time, reading.sample_count);
            const float lux_result = meter_probe_lux_result(&reading);

            if (reading.result_status == METER_SENSOR_RESULT_VALID) {
                sprintf(buf,
                    "TSL2585 (%s, %.2fms)\n"
                    "Data: %ld [%d]\n"
                    "Basic: %f, Lux: %f\n"
                    "[%s][%s]\n"
                    "%s",
                    tsl2585_gain_str(reading.gain), atime,
                    scaled_result, reading.raw_result,
                    basic_result, lux_result,
                    (enlarger_enabled ? "**" : "--"),
                    (agc_enabled ? "AGC" : "---"),
                    (single_shot ? "Single Shot" : "Continuous"));
            } else {
                const char *status_str;
                switch (reading.result_status) {
                case METER_SENSOR_RESULT_INVALID:
                    status_str = "Invalid";
                    break;
                case METER_SENSOR_RESULT_SATURATED_ANALOG:
                    status_str = "Analog Saturation";
                    break;
                case METER_SENSOR_RESULT_SATURATED_DIGITAL:
                    status_str = "Digital Saturation";
                    break;
                default:
                    status_str = "";
                    break;
                }
                sprintf(buf,
                    "TSL2585 (%s, %.2fms)\n"
                    "%s\n"
                    "\n"
                    "[%s][%s]\n"
                    "%s",
                    tsl2585_gain_str(reading.gain), atime,
                    status_str,
                    (enlarger_enabled ? "**" : "--"),
                    (agc_enabled ? "AGC" : "---"),
                    (single_shot ? "Single Shot" : "Continuous"));
            }
            display_static_list("Meter Probe Diagnostics", buf);

            gain = reading.gain;
            sample_time = reading.sample_time;
            sample_count = reading.sample_count;
        }

        if (keypad_wait_for_event(&keypad_event, 100) == HAL_OK) {
            key_changed = true;
        }
    }

    meter_probe_sensor_disable();

    enlarger_control_set_state_off(&(enlarger_config.control), false);

    return MENU_OK;
}

