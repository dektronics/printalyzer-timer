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

#define DENSITY_BUF_SIZE 5

static menu_result_t menu_meter_probe_impl(const char *title, meter_probe_handle_t *handle);
static menu_result_t meter_probe_device_info(meter_probe_handle_t *handle);
static menu_result_t meter_probe_show_calibration(const meter_probe_handle_t *handle);
static menu_result_t densistick_reflection_calibration(meter_probe_handle_t *handle);
static menu_result_t densistick_reflection_calibration_measure(meter_probe_handle_t *handle, const peripheral_cal_density_target_t *cal_target);
static menu_result_t densistick_show_calibration(meter_probe_handle_t *handle);
void format_density_value(char *buf, float value);

static menu_result_t meter_probe_diagnostics(const char *title, meter_probe_handle_t *handle, bool fast_mode);
static menu_result_t densistick_test_reading(meter_probe_handle_t *handle);

menu_result_t menu_meter_probe()
{
    meter_probe_handle_t *handle = meter_probe_handle();
    return menu_meter_probe_impl("Meter Probe", handle);
}

menu_result_t menu_densistick()
{
    meter_probe_handle_t *handle = densistick_handle();
    return menu_meter_probe_impl("DensiStick", handle);
}

menu_result_t menu_meter_probe_impl(const char *title, meter_probe_handle_t *handle)
{
    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;

    /* Attempt to start the meter probe */
    if (meter_probe_start(handle) != osOK) {
        option = display_message(
            title,
            NULL,
            "\n**** Not Detected ****\n", " OK ");
        if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
        return menu_result;
    }

    /* Show the meter probe menu */
    do {
        if (handle == densistick_handle()) {
            option = display_selection_list(
                title, option,
                "Device Info\n"
                "Reflection Calibration\n"
                "Show Calibration Data\n"
                "Diagnostics\n"
                "Test Density Reading");

            if (option == 1) {
                menu_result = meter_probe_device_info(handle);
            } else if (option == 2) {
                menu_result = densistick_reflection_calibration(handle);
            } else if (option == 3) {
                menu_result = densistick_show_calibration(handle);
            } else if (option == 4) {
                menu_result = meter_probe_diagnostics(title, handle, false);
            } else if (option == 5) {
                menu_result = densistick_test_reading(handle);
            } else if (option == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            }
        } else {
            option = display_selection_list(
                title, option,
                "Device Info\n"
                "Show Calibration Data\n"
                "Diagnostics (Normal Mode)\n"
                "Diagnostics (Fast Mode)");

            if (option == 1) {
                menu_result = meter_probe_device_info(handle);
            } else if (option == 2) {
                menu_result = meter_probe_show_calibration(handle);
            } else if (option == 3) {
                menu_result = meter_probe_diagnostics(title, handle, false);
            } else if (option == 4) {
                menu_result = meter_probe_diagnostics(title, handle, true);
            } else if (option == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            }
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    /* Stop the meter probe before returning */
    meter_probe_sensor_disable(handle);
    meter_probe_stop(handle);

    return menu_result;
}

menu_result_t meter_probe_device_info(meter_probe_handle_t *handle)
{
    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;
    char buf[512];

    meter_probe_device_info_t info;
    if (meter_probe_get_device_info(handle, &info) != osOK) {
        return MENU_OK;
    }

    bool has_settings = meter_probe_has_settings(handle);

    do {
        size_t offset = 0;
        if (handle == meter_probe_handle()) {
            offset += menu_build_padded_str_row(buf + offset, "Peripheral Type",
                meter_probe_type_str(info.peripheral_type));

        } else if (handle == densistick_handle()) {
            offset += menu_build_padded_str_row(buf + offset, "Peripheral Type",
                densistick_type_str(info.peripheral_type));

        }
        offset += menu_build_padded_format_row(buf + offset, "Revision", "%d.%d",
            info.peripheral_id.rev_major,
            info.peripheral_id.rev_minor);
        offset += menu_build_padded_format_row(buf + offset, "Serial Number", "%s",
            info.peripheral_id.serial);
        offset += menu_build_padded_format_row(buf + offset, "Sensor Type", "%s",
            tsl2585_sensor_type_str(info.sensor_type));
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

menu_result_t meter_probe_show_calibration(const meter_probe_handle_t *handle)
{
    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;
    char buf[640];

    meter_probe_settings_t settings;
    if (meter_probe_get_settings(handle, &settings) != osOK) {
        return MENU_OK;
    }

    if (settings.type != METER_PROBE_TYPE_BASELINE) {
        /* Unknown meter probe type */
        return MENU_OK;
    }

    do {
        size_t offset = 0;

        for (tsl2585_gain_t gain = 0; gain <= TSL2585_GAIN_256X; gain++) {
            offset += menu_build_padded_format_row(buf + offset,
                tsl2585_gain_str(gain), "%f",
                settings.cal_gain.values[gain]);
        }

        offset += menu_build_padded_format_row(buf + offset,
            "Lux slope", "%f",
            settings.cal_target.lux_slope);

        offset += menu_build_padded_format_row(buf + offset,
            "Lux intercept", "%f",
            settings.cal_target.lux_intercept);

        buf[offset - 1] = '\0';

        option = display_selection_list("Sensor Calibration Data", option, buf);

        if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}

menu_result_t densistick_reflection_calibration(meter_probe_handle_t *handle)
{
    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;
    char buf[256];
    char buf_lo[DENSITY_BUF_SIZE];
    char buf_hi[DENSITY_BUF_SIZE];
    peripheral_cal_density_target_t *cal_target;

    densistick_settings_t settings;
    if (densistick_get_settings(handle, &settings) != osOK) {
        return MENU_OK;
    }

    if (settings.type != DENSISTICK_TYPE_BASELINE) {
        /* Unknown meter probe type */
        return MENU_OK;
    }

    cal_target = &settings.cal_target;

    do {
        size_t offset = 0;

        format_density_value(buf_lo, cal_target->lo_density);
        format_density_value(buf_hi, cal_target->hi_density);

        offset += menu_build_padded_format_row(buf + offset,
            "CAL-LO (White)", "%s", buf_lo);
        offset += menu_build_padded_format_row(buf + offset,
            "CAL-HI (Black)", "%s", buf_hi);

        sprintf(buf + offset, "*** Measure Reference ***");

        option = display_selection_list("Reflection Calibration", option, buf);

        if (option == 1) {
            uint16_t working_value;
            if (is_valid_number(cal_target->lo_density) && cal_target->lo_density >= 0.0F) {
                working_value = lroundf(cal_target->lo_density * 100);
            } else {
                working_value = 8;
            }

            uint8_t input_option = display_input_value_f1_2(
                "CAL-LO (White)\n",
                "D=", &working_value,
                0, 250, '.', NULL);
            if (input_option == 1) {
                cal_target->lo_density = (float)working_value / 100.0F;
            } else if (input_option == UINT8_MAX) {
                option = UINT8_MAX;
            }

        } else if (option == 2) {
            uint16_t working_value;
            if (is_valid_number(cal_target->hi_density) && cal_target->hi_density >= 0.0F) {
                working_value = lroundf(cal_target->hi_density * 100);
            } else {
                working_value = 150;
            }

            uint8_t input_option = display_input_value_f1_2(
                "CAL-HI (Black)\n",
                "D=", &working_value,
                0, 250, '.', NULL);
            if (input_option == 1) {
                cal_target->hi_density = (float)working_value / 100.0F;
            } else if (input_option == UINT8_MAX) {
                option = UINT8_MAX;
            }

        } else if (option == 3) {
            menu_result = densistick_reflection_calibration_measure(handle, cal_target);
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}

menu_result_t densistick_reflection_calibration_measure(meter_probe_handle_t *handle, const peripheral_cal_density_target_t *cal_target)
{
    meter_probe_result_t meas_result = METER_READING_OK;
    uint8_t option = 1;
    float cal_lo_reading = NAN;
    float cal_hi_reading = NAN;

    do {
        /* Validate the target densities, just in case */
        if (!is_valid_number(cal_target->lo_density) || !is_valid_number(cal_target->hi_density)) {
            meas_result = METER_READING_FAIL;
            break;
        }
        if (cal_target->lo_density < 0.00F || cal_target->lo_density > 2.5F) {
            meas_result = METER_READING_FAIL;
            break;
        }
        if (cal_target->hi_density < 0.00F || cal_target->hi_density > 2.5F) {
            meas_result = METER_READING_FAIL;
            break;
        }
        if (cal_target->lo_density >= cal_target->hi_density) {
            meas_result = METER_READING_FAIL;
            break;
        }

        /* Activate the idle light at minimum brightness */
        densistick_set_light_brightness(handle, 127);
        densistick_set_light_enable(handle, true);

        do {
            option = display_message_params(
                "Place the sensor on top of the\n"
                "CAL-LO reference patch and push\n"
                "the measurement button",
                "", NULL, " Measure ",
                DISPLAY_MENU_ACCEPT_STICK);
        } while (option != 0 && option != 1 && option != UINT8_MAX);

        if (option == 1) {
            display_draw_mode_text("Measuring");
            buzzer_sequence(BUZZER_SEQUENCE_STICK_START);

            meas_result = densistick_measure(densistick_handle(), NULL, &cal_lo_reading);

            if (meas_result == METER_READING_OK) {
                buzzer_sequence(BUZZER_SEQUENCE_STICK_SUCCESS);
            } else if (meas_result == METER_READING_TIMEOUT) {
                display_draw_mode_text("Timeout");
                buzzer_sequence(BUZZER_SEQUENCE_PROBE_ERROR);
                osDelay(pdMS_TO_TICKS(2000));
            } else  {
                display_draw_mode_text("Reading Error");
                buzzer_sequence(BUZZER_SEQUENCE_PROBE_ERROR);
                osDelay(pdMS_TO_TICKS(2000));
            }
        } else {
            meas_result = METER_READING_FAIL;
            break;
        }
        if (meas_result != METER_READING_OK) { break; }

        /* Set light back to idle */
        densistick_set_light_brightness(handle, 127);
        densistick_set_light_enable(handle, true);

        do {
            option = display_message_params(
                "Place the sensor on top of the\n"
                "CAL-HI reference patch and push\n"
                "the measurement button",
                "", NULL, " Measure ",
                DISPLAY_MENU_ACCEPT_STICK);
        } while (option != 0 && option != 1 && option != UINT8_MAX);

        if (option == 1) {
            display_draw_mode_text("Measuring");
            buzzer_sequence(BUZZER_SEQUENCE_STICK_START);

            meas_result = densistick_measure(densistick_handle(), NULL, &cal_hi_reading);

            if (meas_result == METER_READING_OK) {
                buzzer_sequence(BUZZER_SEQUENCE_STICK_SUCCESS);
            } else if (meas_result == METER_READING_TIMEOUT) {
                display_draw_mode_text("Timeout");
                buzzer_sequence(BUZZER_SEQUENCE_PROBE_ERROR);
                osDelay(pdMS_TO_TICKS(2000));
            } else  {
                display_draw_mode_text("Reading Error");
                buzzer_sequence(BUZZER_SEQUENCE_PROBE_ERROR);
                osDelay(pdMS_TO_TICKS(2000));
            }
        } else {
            meas_result = METER_READING_FAIL;
            break;
        }
        if (meas_result != METER_READING_OK) { break; }

        /* Validate reading values */
        if (isnan(cal_lo_reading) || isinf(cal_lo_reading)) {
            meas_result = METER_READING_FAIL;
            break;
        }
        if (isnan(cal_hi_reading) || isinf(cal_hi_reading)) {
            meas_result = METER_READING_FAIL;
            break;
        }
        if (cal_lo_reading < 0.0F || cal_hi_reading >= cal_lo_reading) {
            meas_result = METER_READING_FAIL;
            break;
        }

        /*
         * Basic check that LO and HI are far enough apart.
         * This check has a very generous margin of error and should only catch
         * extreme errors likely to be caused by obvious user error in
         * measuring the calibration targets.
         */
        float d_delta_expected = cal_target->hi_density - cal_target->lo_density;
        float d_delta_actual = -1.0F * log10f(cal_hi_reading / cal_lo_reading);
        log_d("LO-vs-HI d-diff expected=%f, actual=%f", d_delta_expected, d_delta_actual);
        if (fabsf(d_delta_expected - d_delta_actual) > 1.0F) {
            log_d("Reading out of bounds");
            meas_result = METER_READING_FAIL;
            break;
        }

    } while (0);

    if (meas_result == METER_READING_OK) {
        peripheral_cal_density_target_t result_target = {0};
        result_target.lo_density = cal_target->lo_density;
        result_target.lo_reading = cal_lo_reading;
        result_target.hi_density = cal_target->hi_density;
        result_target.hi_reading = cal_hi_reading;

        if (densistick_set_settings_target(handle, &result_target) != osOK) {
            option = display_message("Unable to save calibration", "", NULL, " OK ");
        } else {
            option = display_message("Reflection calibration\ncomplete", "", NULL, " OK ");
        }
    } else if (option != UINT8_MAX) {
        option = display_message("Reflection calibration\nfailed", "", NULL, " OK ");
    }

    /* Deactivate the idle light */
    densistick_set_light_enable(handle, false);

    /* Try to restart the device to reload any changed settings */
    if (meas_result == METER_READING_OK) {
        meter_probe_stop(handle);
        meter_probe_start(handle);
    }

    return option == UINT8_MAX ? MENU_TIMEOUT : MENU_OK;
}

void format_density_value(char *buf, float value)
{
    if (is_valid_number(value) && value >= 0.0F) {
        snprintf(buf, DENSITY_BUF_SIZE, "%.2f", value);
    } else {
        strncpy(buf, "-.--", DENSITY_BUF_SIZE);
    }
}

menu_result_t densistick_show_calibration(meter_probe_handle_t *handle)
{
    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;
    char buf[640];

    densistick_settings_t settings;
    if (densistick_get_settings(handle, &settings) != osOK) {
        return MENU_OK;
    }

    if (settings.type != DENSISTICK_TYPE_BASELINE) {
        /* Unknown meter probe type */
        return MENU_OK;
    }

    do {
        size_t offset = 0;

        for (tsl2585_gain_t gain = 0; gain <= TSL2585_GAIN_256X; gain++) {
            offset += menu_build_padded_format_row(buf + offset,
                tsl2585_gain_str(gain), "%f",
                settings.cal_gain.values[gain]);
        }

#if 0
        /* Not showing slope values since they are not used */
        offset += menu_build_padded_format_row(buf + offset,
            "B0", "%f",
            settings.settings_tsl2585.cal_slope.b0);

        offset += menu_build_padded_format_row(buf + offset,
            "B1", "%f",
            settings.settings_tsl2585.cal_slope.b1);

        offset += menu_build_padded_format_row(buf + offset,
            "B2", "%f",
            settings.settings_tsl2585.cal_slope.b2);
#endif

        offset += menu_build_padded_format_row(buf + offset,
            "CAL-LO Density", "%.2f",
            settings.cal_target.lo_density);

        offset += menu_build_padded_format_row(buf + offset,
            "CAL-LO Reading", "%f",
            settings.cal_target.lo_reading);

        offset += menu_build_padded_format_row(buf + offset,
            "CAL-HI Density", "%.2f",
            settings.cal_target.hi_density);

        offset += menu_build_padded_format_row(buf + offset,
            "CAL-HI Reading", "%f",
            settings.cal_target.hi_reading);

        buf[offset - 1] = '\0';

        option = display_selection_list("Sensor Calibration", option, buf);

        if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}

menu_result_t meter_probe_diagnostics(const char *title, meter_probe_handle_t *handle, bool fast_mode)
{
    HAL_StatusTypeDef ret = HAL_OK;
    char title_buf[32];
    char buf[512];
    enlarger_config_t enlarger_config = {0};
    bool sensor_initialized = false;
    bool sensor_error = false;
    bool light_source_enabled = false;
    bool gain_changed = false;
    bool time_changed = false;
    bool agc_changed = false;
    uint8_t stick_light_val_changed = false;
    tsl2585_gain_t gain = 0;
    uint16_t sample_time = 0;
    uint16_t sample_count = 0;
    bool agc_enabled = false;
    uint8_t stick_light_val = 0;
    bool single_shot = false;
    meter_probe_sensor_reading_t sensor_reading = {0};
    keypad_event_t keypad_event;
    bool key_changed = false;
    uint32_t elapsed_tick_buf[10] = {0};
    bool elapsed_tick_buf_full = false;
    size_t elapsed_tick_buf_pos = 0;
    const size_t elapsed_tick_buf_len = sizeof(elapsed_tick_buf) / sizeof(uint32_t);
    float atime;
    uint32_t expected_reading_time;
    const bool is_stick = (handle == densistick_handle()) ? true : false;

    sprintf(title_buf, "%s Diagnostics", title);

    /* Get meter probe device info */
    meter_probe_device_info_t info;
    if (meter_probe_get_device_info(handle, &info) != osOK) {
        return MENU_OK;
    }

    if (!is_stick) {
        /* Load active enlarger config */
        uint8_t config_index = settings_get_default_enlarger_config_index();
        bool result = settings_get_enlarger_config(&enlarger_config, config_index);
        if (!(result && enlarger_config_is_valid(&enlarger_config))) {
            enlarger_config_set_defaults(&enlarger_config);
        }
        /* Make sure enlarger starts in a known state */
        illum_controller_refresh();
        enlarger_control_set_state_off(&(enlarger_config.control), false);
    } else {
        densistick_set_light_enable(handle, false);
        stick_light_val = densistick_get_light_brightness(handle);
    }

    if (meter_probe_sensor_set_gain(handle, TSL2585_GAIN_256X) == osOK) {
        gain = TSL2585_GAIN_256X;
    }

    sample_time = fast_mode ? 359 : 719;
    sample_count = fast_mode ? 59 : 99;

    if (meter_probe_sensor_set_integration(handle, sample_time, sample_count) != osOK) {
        log_w("Unable to set integration time");
    }

    /* Enable internal calibration on every sequencer round */
    meter_probe_sensor_set_mod_calibration(handle, fast_mode ? 0xFF : 1);

    for (;;) {
        if (!sensor_initialized) {
            if ((fast_mode ? meter_probe_sensor_enable_fast_mode(handle) : meter_probe_sensor_enable(handle)) == osOK) {
                sensor_initialized = true;
            } else {
                log_e("Error initializing TSL2585: %d", ret);
                sensor_error = true;
            }
        }

        if (key_changed) {
            key_changed = false;
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_FOCUS)) {
                if (is_stick) {
                    if (!light_source_enabled) {
                        densistick_set_light_enable(handle, true);
                        light_source_enabled = true;
                    } else {
                        densistick_set_light_enable(handle, false);
                        light_source_enabled = false;
                    }
                } else {
                    if (!light_source_enabled) {
                        if (enlarger_control_set_state_focus(&(enlarger_config.control), false) == osOK) {
                            log_i("Meter probe focus mode enabled");
                            light_source_enabled = true;
                        }
                    } else {
                        if (enlarger_control_set_state_off(&(enlarger_config.control), false) == osOK) {
                            log_i("Meter probe focus mode disabled");
                            light_source_enabled = false;
                        }
                    }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_EXPOSURE)) {
                if (sensor_initialized && !sensor_error) {
                    if (gain > TSL2585_GAIN_0_5X) {
                        gain--;
                        gain_changed = true;
                    }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_EXPOSURE)) {
                if (sensor_initialized && !sensor_error) {
                    if (gain < TSL2585_GAIN_256X) {
                        gain++;
                        gain_changed = true;
                    }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_CONTRAST)) {
                if (sensor_initialized && !sensor_error) {
                    if (fast_mode) {
                        if (sample_count > 10) {
                            sample_count -= 10;
                            time_changed = true;
                        } else if (sample_count > 4) {
                            sample_count--;
                            time_changed = true;
                        }
                    } else {
                        if (sample_count > 30) {
                            sample_count -= 10;
                            time_changed = true;
                        }
                    }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_CONTRAST)) {
                if (sensor_initialized && !sensor_error) {
                    if (fast_mode) {
                        if (sample_count < 9) {
                            sample_count++;
                            time_changed = true;
                        } else if (sample_count < 50) {
                            sample_count += 10;
                            time_changed = true;
                        }
                    } else {
                        if (sample_count < (2047 - 10)) {
                            sample_count += 10;
                            time_changed = true;
                        }
                    }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_ADD_ADJUSTMENT) && !fast_mode) {
                if (sensor_initialized && !sensor_error) {
                    agc_enabled = !agc_enabled;
                    agc_changed = true;
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_TEST_STRIP) && !fast_mode) {
                if (meter_probe_sensor_disable(handle) == osOK) {
                    if (single_shot) {
                        if (meter_probe_sensor_enable(handle) == osOK) {
                            single_shot = false;
                        } else {
                            sensor_error = true;
                        }
                    } else {
                        if (meter_probe_sensor_enable_single_shot(handle) == osOK) {
                            single_shot = true;
                        } else {
                            sensor_error = true;
                        }
                    }
                } else {
                    sensor_error = true;
                }
            } else if (keypad_event.key == KEYPAD_ENCODER_CW && keypad_event.pressed && is_stick) {
                stick_light_val = (stick_light_val + keypad_event.count) & 0x7F;
                stick_light_val_changed = true;
            } else if (keypad_event.key == KEYPAD_ENCODER_CCW && keypad_event.pressed && is_stick) {
                stick_light_val = (stick_light_val - keypad_event.count) & 0x7F;
                stick_light_val_changed = true;
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_METER_PROBE)
                || keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DENSISTICK)
                || keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_ENCODER)) {
                if (single_shot) {
                    meter_probe_sensor_trigger_next_reading(handle);
                }
            } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                break;
            } else if (keypad_event.key == KEYPAD_USB_KEYBOARD && keypad_event.pressed
                && keypad_usb_get_keypad_equivalent(&keypad_event) == KEYPAD_CANCEL) {
                break;
            }
        }

        if (gain_changed || time_changed) {
            memset(elapsed_tick_buf, 0, sizeof(elapsed_tick_buf));
            elapsed_tick_buf_full = false;
            elapsed_tick_buf_pos = 0;

            if (gain_changed) {
                if (meter_probe_sensor_set_gain(handle, gain) == osOK) {
                    gain_changed = false;
                }
            }

            if (time_changed) {
                if (meter_probe_sensor_set_integration(handle, sample_time, sample_count) == osOK) {
                    time_changed = false;
                }
            }
        }
        if (agc_changed) {
            if (agc_enabled) {
                if (meter_probe_sensor_enable_agc(handle, sample_count) == osOK) {
                    agc_changed = false;
                }
            } else {
                if (meter_probe_sensor_disable_agc(handle) == osOK) {
                    agc_changed = false;
                }
            }
        }

        if (stick_light_val_changed) {
            if (densistick_set_light_brightness(handle, stick_light_val) == osOK) {
                stick_light_val_changed = false;
            }
        }

        atime = tsl2585_integration_time_ms(sensor_reading.sample_time, sensor_reading.sample_count);
        if (fast_mode) {
            expected_reading_time = lroundf(atime * MAX_ALS_COUNT);
        } else {
            expected_reading_time = lroundf(atime);
        }
        if (meter_probe_sensor_get_next_reading(handle, &sensor_reading, single_shot ? 10 : (expected_reading_time * 2)) == osOK) {
            float elapsed_tick_avg = 0;

            if (sensor_reading.elapsed_ticks < (expected_reading_time * 2)) {
                /* Track the moving average of measured integration time */
                elapsed_tick_buf[elapsed_tick_buf_pos] = sensor_reading.elapsed_ticks;
                elapsed_tick_buf_pos++;

                float elapsed_tick_sum = 0;
                size_t elapsed_tick_sum_len = (elapsed_tick_buf_full ? elapsed_tick_buf_len : elapsed_tick_buf_pos);
                for (size_t i = 0; i < elapsed_tick_sum_len; i++) {
                    elapsed_tick_sum += (float)elapsed_tick_buf[i];
                }
                elapsed_tick_avg = elapsed_tick_sum / (float)elapsed_tick_sum_len;

                if (elapsed_tick_buf_pos >= elapsed_tick_buf_len) {
                    elapsed_tick_buf_full = true;
                    elapsed_tick_buf_pos = 0;
                }
            }

            if (fast_mode) {
                size_t offset;

                offset = sprintf(buf, "%s (%s, %.2fms)\n",
                    tsl2585_sensor_type_str(info.sensor_type),
                    tsl2585_gain_str(sensor_reading.reading[0].gain), atime);

                for (uint8_t i = 0; i < MAX_ALS_COUNT; i++) {
                    if (i > 0) {
                        offset += sprintf(buf + offset, ",");
                        if ((i % 4) == 0) { offset += sprintf(buf + offset, "\n"); }
                    }

                    switch (sensor_reading.reading[i].status) {
                    case METER_SENSOR_RESULT_INVALID:
                        offset += sprintf(buf + offset, "INVL");
                        break;
                    case METER_SENSOR_RESULT_SATURATED_ANALOG:
                        offset += sprintf(buf + offset, "ASAT");
                        break;
                    case METER_SENSOR_RESULT_SATURATED_DIGITAL:
                        offset += sprintf(buf + offset, "DSAT");
                        break;
                    default:
                        offset += sprintf(buf + offset, "%lu", sensor_reading.reading[i].data);
                        break;
                    }
                }

                sprintf(buf + offset, "\n[%s] {%.2f}",
                    (light_source_enabled ? "**" : "--"),
                    elapsed_tick_avg);
            }
            else {
                if (sensor_reading.reading[0].status == METER_SENSOR_RESULT_VALID) {
                    const float basic_result = meter_probe_basic_result(handle, &sensor_reading);
                    if (is_stick) {
                        sprintf(buf,
                            "%s (%s, %.2fms)\n"
                            "Data: %lu\n"
                            "Basic: %f\n"
                            "[%s][%3d][%s] {%.2f}\n"
                            "%s",
                            tsl2585_sensor_type_str(info.sensor_type),
                            tsl2585_gain_str(sensor_reading.reading[0].gain), atime,
                            sensor_reading.reading[0].data,
                            basic_result,
                            (light_source_enabled ? "**" : "--"), stick_light_val,
                            (agc_enabled ? "AGC" : "---"), elapsed_tick_avg,
                            (single_shot ? "Single Shot" : "Continuous"));
                    } else {
                        const float lux_result = meter_probe_lux_result(handle, &sensor_reading);
                        sprintf(buf,
                            "%s (%s, %.2fms)\n"
                            "Data: %lu\n"
                            "Basic: %f, Lux: %f\n"
                            "[%s][%s] {%.2f}\n"
                            "%s",
                            tsl2585_sensor_type_str(info.sensor_type),
                            tsl2585_gain_str(sensor_reading.reading[0].gain), atime,
                            sensor_reading.reading[0].data,
                            basic_result, lux_result,
                            (light_source_enabled ? "**" : "--"),
                            (agc_enabled ? "AGC" : "---"), elapsed_tick_avg,
                            (single_shot ? "Single Shot" : "Continuous"));
                    }
                } else {
                    const char *status_str;
                    switch (sensor_reading.reading[0].status) {
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
                    if (is_stick) {
                        sprintf(buf,
                            "%s (%s, %.2fms)\n"
                            "%s\n"
                            "\n"
                            "[%s][%3d][%s]\n"
                            "%s",
                            tsl2585_sensor_type_str(info.sensor_type),
                            tsl2585_gain_str(sensor_reading.reading[0].gain), atime,
                            status_str,
                            (light_source_enabled ? "**" : "--"), stick_light_val,
                            (agc_enabled ? "AGC" : "---"),
                            (single_shot ? "Single Shot" : "Continuous"));
                    } else {
                        sprintf(buf,
                            "%s (%s, %.2fms)\n"
                            "%s\n"
                            "\n"
                            "[%s][%s]\n"
                            "%s",
                            tsl2585_sensor_type_str(info.sensor_type),
                            tsl2585_gain_str(sensor_reading.reading[0].gain), atime,
                            status_str,
                            (light_source_enabled ? "**" : "--"),
                            (agc_enabled ? "AGC" : "---"),
                            (single_shot ? "Single Shot" : "Continuous"));
                    }
                }
            }
            display_static_list(title_buf, buf);

            gain = sensor_reading.reading[0].gain;
            sample_time = sensor_reading.sample_time;
            sample_count = sensor_reading.sample_count;
        } else {
            if (!meter_probe_is_started(handle)) {
                sprintf(buf, "\n\n**** Detached ****");
                display_static_list(title_buf, buf);
                if (meter_probe_is_attached(handle)) {
                    bool probe_restarted = false;
                    do {
                        if (meter_probe_start(handle) != osOK) { break; }
                        if (meter_probe_get_device_info(handle, &info) != osOK) { break; }
                        if (meter_probe_sensor_set_gain(handle, gain) != osOK) { break; }
                        if (meter_probe_sensor_set_integration(handle, sample_time, sample_count) != osOK) { break; }
                        if (meter_probe_sensor_set_mod_calibration(handle, fast_mode ? 0xFF : 1) != osOK) { break; }
                        if (agc_enabled) {
                            if (meter_probe_sensor_enable_agc(handle, sample_count) != osOK) { break; }
                        }
                        if (fast_mode) {
                            if (meter_probe_sensor_enable_fast_mode(handle) != osOK) { break; }
                        } else if (single_shot) {
                            if (meter_probe_sensor_enable_single_shot(handle) != osOK) { break; }
                        } else {
                            if (meter_probe_sensor_enable(handle) != osOK) { break; }
                        }
                        if (is_stick) {
                            if (densistick_set_light_enable(handle, light_source_enabled) != osOK) { break; }
                        }
                        probe_restarted = true;
                    } while (0);
                    if (!probe_restarted) {
                        log_w("Unable to restart %s", title);
                        meter_probe_stop(handle);
                    }
                }
            }
        }

        if (keypad_wait_for_event(&keypad_event, 100) == HAL_OK) {
            key_changed = true;
        }
    }

    meter_probe_sensor_disable(handle);

    if (is_stick) {
        densistick_set_light_enable(handle, false);
    } else {
        enlarger_control_set_state_off(&(enlarger_config.control), false);
    }

    return MENU_OK;
}

menu_result_t densistick_test_reading(meter_probe_handle_t *handle)
{
    char buf[512];
    float reading = NAN;
    float density = NAN;
    meter_probe_result_t result = METER_READING_OK;
    bool has_reading = false;

    densistick_set_light_brightness(handle, 127);
    densistick_set_light_enable(handle, true);

    for (;;) {
        if (has_reading) {
            sprintf(buf, "\n"
                "Raw Reading=%f\n"
                "Density=%0.02f",
                reading, density);
        } else {
            switch (result) {
            case METER_READING_LOW:
                sprintf(buf, "\n\nReading Low");
                break;
            case METER_READING_HIGH:
                sprintf(buf, "\n\nReading High");
                break;
            case METER_READING_TIMEOUT:
                sprintf(buf, "\n\nReading Timeout");
                break;
            case METER_READING_FAIL:
                sprintf(buf, "\n\nReading Failed");
                break;
            case METER_READING_OK:
            default:
                sprintf(buf, "\n\nNo Reading");
                break;
            }
        }
        display_static_list("DensiStick Reading Test", buf);

        keypad_event_t keypad_event;
        if (keypad_wait_for_event(&keypad_event, -1) == HAL_OK) {
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DENSISTICK)) {
                log_i("Taking sample reading...");
                sprintf(buf, "\n\nTaking reading...");
                display_static_list("DensiStick Reading Test", buf);
                result = densistick_measure(handle, &density, &reading);
                if (result == METER_READING_OK) {
                    has_reading = true;
                } else {
                    has_reading = false;
                }
                /* Return light to idle state */
                densistick_set_light_brightness(handle, 127);
                densistick_set_light_enable(handle, true);
            } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                break;
            } else if (keypad_event.key == KEYPAD_USB_KEYBOARD && keypad_event.pressed
                && keypad_usb_get_keypad_equivalent(&keypad_event) == KEYPAD_CANCEL) {
                break;
            }
        }
    }

    densistick_set_light_enable(handle, false);

    return MENU_OK;
}