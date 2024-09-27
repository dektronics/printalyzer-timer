#include "menu_meter_probe.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <ff.h>

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
#include "json_util.h"
#include "usb_host.h"
#include "core_json.h"
#include "file_picker.h"

#define HEADER_EXPORT_VERSION     1
#define MAX_CALIBRATION_FILE_SIZE 16384

static menu_result_t menu_meter_probe_impl(const char *title, meter_probe_handle_t *handle);
static menu_result_t meter_probe_device_info(meter_probe_handle_t *handle);
static menu_result_t meter_probe_sensor_calibration(meter_probe_handle_t *handle);
static menu_result_t densistick_sensor_calibration(meter_probe_handle_t *handle);

static menu_result_t meter_probe_sensor_calibration_import(meter_probe_handle_t *handle);
static bool import_calibration_file(const char *filename, const meter_probe_device_info_t *info, meter_probe_settings_t *settings);
static bool validate_section_header(const char *buf, size_t len, const meter_probe_device_info_t *info);
static bool import_section_sensor_cal(const char *buf, size_t len, meter_probe_settings_t *settings);
static bool parse_section_sensor_cal_gain(const char *buf, size_t len, meter_probe_settings_tsl2585_cal_gain_t *cal_gain);
static bool parse_section_sensor_cal_slope(const char *buf, size_t len, meter_probe_settings_tsl2585_cal_slope_t *cal_slope);
static bool parse_section_sensor_cal_target(const char *buf, size_t len, meter_probe_settings_tsl2585_cal_target_t *cal_target);

//static void parse_section_sensor_cal_gain_entry(const char *buf, size_t len, meter_probe_settings_tsl2585_gain_cal_t *gain_cal);

static menu_result_t meter_probe_sensor_calibration_export(meter_probe_handle_t *handle);
static bool export_calibration_file(const char *filename, const meter_probe_device_info_t *info, const meter_probe_settings_t *settings);
static bool write_section_header(FIL *fp, const meter_probe_device_info_t *info);
static bool write_section_sensor_cal(FIL *fp, const meter_probe_settings_t *settings);

static menu_result_t meter_probe_diagnostics(const char *title, meter_probe_handle_t *handle, bool fast_mode);


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
                "Sensor Calibration\n"
                "Diagnostics");

            if (option == 1) {
                menu_result = meter_probe_device_info(handle);
            } else if (option == 2) {
                menu_result = densistick_sensor_calibration(handle);
            } else if (option == 3) {
                menu_result = meter_probe_diagnostics(title, handle, false);
            } else if (option == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            }
        } else {
            option = display_selection_list(
                title, option,
                "Device Info\n"
                "Sensor Calibration\n"
                "Diagnostics (Normal Mode)\n"
                "Diagnostics (Fast Mode)");

            if (option == 1) {
                menu_result = meter_probe_device_info(handle);
            } else if (option == 2) {
                menu_result = meter_probe_sensor_calibration(handle);
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
        offset += menu_build_padded_str_row(buf + offset, "Probe Type",
            meter_probe_type_str(info.probe_id.probe_type));
        offset += menu_build_padded_format_row(buf + offset, "Revision", "%d", info.probe_id.probe_rev_major);
        offset += menu_build_padded_format_row(buf + offset, "Serial Number", "%s", info.probe_id.probe_serial);
        offset += menu_build_padded_format_row(buf + offset, "Sensor ID", "%02X%02X%02X",
            info.sensor_id[0], info.sensor_id[1], info.sensor_id[2]);
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

menu_result_t meter_probe_sensor_calibration(meter_probe_handle_t *handle)
{
    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;
    char buf[640];

    meter_probe_settings_t settings;
    if (meter_probe_get_settings(handle, &settings) != osOK) {
        return MENU_OK;
    }

    if (settings.type != METER_PROBE_SENSOR_TSL2585 && settings.type != METER_PROBE_SENSOR_TSL2521) {
        /* Unknown meter probe type */
        return MENU_OK;
    }

    do {
        size_t option_offset = 0;
        size_t offset = 0;

        for (tsl2585_gain_t gain = 0; gain <= TSL2585_GAIN_256X; gain++) {
            offset += menu_build_padded_format_row(buf + offset,
                tsl2585_gain_str(gain), "%f",
                settings.settings_tsl2585.cal_gain.values[gain]);
            option_offset++;
        }

        offset += menu_build_padded_format_row(buf + offset,
            "B0", "%f",
            settings.settings_tsl2585.cal_slope.b0);
        option_offset++;

        offset += menu_build_padded_format_row(buf + offset,
            "B1", "%f",
            settings.settings_tsl2585.cal_slope.b1);
        option_offset++;

        offset += menu_build_padded_format_row(buf + offset,
            "B2", "%f",
            settings.settings_tsl2585.cal_slope.b2);
        option_offset++;

        offset += menu_build_padded_format_row(buf + offset,
            "Lux slope", "%f",
            settings.settings_tsl2585.cal_target.lux_slope);
        option_offset++;

        offset += menu_build_padded_format_row(buf + offset,
            "Lux intercept", "%f",
            settings.settings_tsl2585.cal_target.lux_intercept);
        option_offset++;

        offset += sprintf(buf + offset, "*** Import from USB device ***\n");
        offset += sprintf(buf + offset, "*** Export to USB device ***");

        option = display_selection_list("Sensor Calibration", option, buf);

        if (option == option_offset + 1) {
            menu_result = meter_probe_sensor_calibration_import(handle);
            if (menu_result == MENU_SAVE) {
                meter_probe_stop(handle);
                if (meter_probe_start(handle) != osOK) {
                    menu_result = MENU_TIMEOUT;
                } else {
                    menu_result = MENU_OK;
                    break;
                }
            }
        } else if (option == option_offset + 2) {
            menu_result = meter_probe_sensor_calibration_export(handle);
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}

menu_result_t densistick_sensor_calibration(meter_probe_handle_t *handle)
{
    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;
    char buf[640];

    densistick_settings_t settings;
    if (densistick_get_settings(handle, &settings) != osOK) {
        return MENU_OK;
    }

    if (settings.type != METER_PROBE_SENSOR_TSL2585 && settings.type != METER_PROBE_SENSOR_TSL2521) {
        /* Unknown meter probe type */
        return MENU_OK;
    }

    do {
        size_t option_offset = 0;
        size_t offset = 0;

        for (tsl2585_gain_t gain = 0; gain <= TSL2585_GAIN_256X; gain++) {
            offset += menu_build_padded_format_row(buf + offset,
                tsl2585_gain_str(gain), "%f",
                settings.settings_tsl2585.cal_gain.values[gain]);
            option_offset++;
        }

#if 0
        /* Not showing slope values since they are not used */
        offset += menu_build_padded_format_row(buf + offset,
            "B0", "%f",
            settings.settings_tsl2585.cal_slope.b0);
        option_offset++;

        offset += menu_build_padded_format_row(buf + offset,
            "B1", "%f",
            settings.settings_tsl2585.cal_slope.b1);
        option_offset++;

        offset += menu_build_padded_format_row(buf + offset,
            "B2", "%f",
            settings.settings_tsl2585.cal_slope.b2);
        option_offset++;
#endif

        offset += menu_build_padded_format_row(buf + offset,
            "CAL-LO Density", "%.2f",
            settings.settings_tsl2585.cal_target.lo_density);
        option_offset++;

        offset += menu_build_padded_format_row(buf + offset,
            "CAL-LO Reading", "%f",
            settings.settings_tsl2585.cal_target.lo_reading);
        option_offset++;

        offset += menu_build_padded_format_row(buf + offset,
            "CAL-HI Density", "%.2f",
            settings.settings_tsl2585.cal_target.hi_density);
        option_offset++;

        offset += menu_build_padded_format_row(buf + offset,
            "CAL-HI Reading", "%f",
            settings.settings_tsl2585.cal_target.hi_reading);
        option_offset++;

        buf[offset - 1] = '\0';
#if 0
        offset += sprintf(buf + offset, "*** Import from USB device ***\n");
        offset += sprintf(buf + offset, "*** Export to USB device ***");
#endif

        option = display_selection_list("Sensor Calibration", option, buf);

#if 0
        if (option == option_offset + 1) {
            menu_result = meter_probe_sensor_calibration_import(handle);
            if (menu_result == MENU_SAVE) {
                meter_probe_stop(handle);
                if (meter_probe_start(handle) != osOK) {
                    menu_result = MENU_TIMEOUT;
                } else {
                    menu_result = MENU_OK;
                    break;
                }
            }
        } else if (option == option_offset + 2) {
            menu_result = meter_probe_sensor_calibration_export(handle);
        } else
#endif
        if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}

menu_result_t meter_probe_sensor_calibration_import(meter_probe_handle_t *handle)
{
    char buf[256];
    char path_buf[256];
    uint8_t option;
    meter_probe_settings_t imported_settings = {0};

    if (!usb_msc_is_mounted()) {
        option = display_message(
                "Import from USB device",
                NULL,
                "\n"
                "Please insert a USB storage\n"
                "device and try again.\n", " OK ");
        if (option == UINT8_MAX) {
            return MENU_TIMEOUT;
        } else {
            return MENU_OK;
        }
    }

    meter_probe_device_info_t info;
    if (meter_probe_get_device_info(handle, &info) != osOK) {
        return MENU_OK;
    }

    imported_settings.type = info.probe_id.probe_type;

    option = file_picker_show("Select Calibration File", path_buf, sizeof(path_buf), NULL);
    if (option == MENU_TIMEOUT) {
        return MENU_TIMEOUT;
    } else if (option != MENU_OK) {
        return MENU_OK;
    }

    if (import_calibration_file(path_buf, &info, &imported_settings)) {
        char filename[33];
        if (file_picker_expand_filename(filename, 33, path_buf)) {
            filename[32] = '\0';
        } else {
            sprintf(filename, "----");
        }
        sprintf(buf,
            "\n"
            "Calibration loaded from file:\n"
            "%s\n", filename);
        option = display_message(
            "Import from USB device",
            NULL, buf, " Close \n Save ");

        if (option == 2) {
            option = display_message(
                "Overwrite meter probe\n"
                "calibration with values from\n"
                "loaded file?\n", NULL, NULL,
                " NO \n YES ");
            if (option == 2) {
                if (meter_probe_set_settings(handle, &imported_settings) == osOK) {
                    option = display_message(
                        "Meter probe calibration saved\n",
                        NULL, NULL, " Close ");
                    if (option != UINT8_MAX) {
                        return MENU_SAVE;
                    }
                } else {
                    option = display_message(
                        "Unable to save meter probe\n"
                        "calibration\n",
                        NULL, NULL, " Close ");
                }
            }
        }
    } else {
        option = display_message(
            "Import from USB device",
            NULL,
            "\n"
            "Calibration was not loaded\n", " OK ");
    }

    if (option == UINT8_MAX) {
        return MENU_TIMEOUT;
    } else {
        return MENU_OK;
    }
}

bool import_calibration_file(const char *filename, const meter_probe_device_info_t *info, meter_probe_settings_t *settings)
{
    FRESULT res;
    FIL fp;
    bool file_open = false;
    bool success = false;
    char *file_buf = NULL;
    UINT bytes_read = 0;
    JSONStatus_t status;
    size_t start = 0;
    size_t next = 0;
    JSONPair_t pair = {0};
    bool has_valid_header = false;
    bool has_sensor_cal = false;
    bool has_valid_sensor_cal = false;

    do {
        memset(&fp, 0, sizeof(FIL));

        res = f_open(&fp, filename, FA_READ | FA_OPEN_EXISTING);
        if (res != FR_OK) {
            log_e("Error opening cal file: %d", res);
            break;
        }
        file_open = true;

        if (f_size(&fp) > MAX_CALIBRATION_FILE_SIZE) {
            log_e("File is too large: %lu > %d", f_size(&fp), MAX_CALIBRATION_FILE_SIZE);
            break;
        }

        log_i("Cal file opened: %s", filename);

        /* Allocate buffer for file */
        file_buf = pvPortMalloc(f_size(&fp));
        if (!file_buf) {
            log_e("Unable to allocate buffer for file: %lu", f_size(&fp));
            break;
        }

        /* Read file into buffer */
        if (f_read(&fp, file_buf, f_size(&fp), &bytes_read) != FR_OK) {
            log_e("Error reading file");
            break;
        }
        if (bytes_read != f_size(&fp)) {
            log_e("Short read: %d != %lu", bytes_read, f_size(&fp));
            break;
        }

        /* Close file */
        f_close(&fp);
        file_open = false;

        log_i("Cal file loaded into buffer");

        /* Validate the JSON */
        if (JSON_Validate(file_buf, bytes_read) != JSONSuccess) {
            log_w("JSON invalid");
            break;
        }

        log_i("Config file validated as JSON");

        /* Traverse the top level to see what sections are in the file */
        start = 0;
        next = 0;
        status = JSON_Iterate(file_buf, bytes_read, &start, &next, &pair);
        while (status == JSONSuccess) {
            if (pair.key) {
                if (strncmp("header", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONObject) {
                    has_valid_header = validate_section_header(pair.value, pair.valueLength, info);
                    if (!has_valid_header) {
                        break;
                    }
                } else if (strncmp("sensor_cal", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONObject) {
                    has_sensor_cal = json_count_elements(pair.value, pair.valueLength) > 0;
                }
            }

            status = JSON_Iterate(file_buf, bytes_read, &start, &next, &pair);
        }

        if (!has_valid_header) {
            log_w("File does not contain valid header");
            break;
        }
        log_i("Found valid header");
        log_i("Found sections: sensor_cal = %d", has_sensor_cal);

        if (!has_sensor_cal) { break; }

        /* Traverse the top level and import */
        start = 0;
        next = 0;
        status = JSON_Iterate(file_buf, bytes_read, &start, &next, &pair);
        while (status == JSONSuccess) {
            if (!pair.key) { continue; }

            if (strncmp("sensor_cal", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONObject) {
                has_valid_sensor_cal = import_section_sensor_cal(pair.value, pair.valueLength, settings);
            }
            status = JSON_Iterate(file_buf, bytes_read, &start, &next, &pair);
        }

        success = true;
    } while (0);

    if (file_open) {
        f_close(&fp);
    }
    if (file_buf) {
        vPortFree(file_buf);
    }

    return success && has_valid_sensor_cal;
}

bool validate_section_header(const char *buf, size_t len, const meter_probe_device_info_t *info)
{
    JSONStatus_t status;
    size_t start = 0;
    size_t next = 0;
    JSONPair_t pair = {0};
    int version = -1;
    int revision = -1;
    bool has_device = false;
    bool has_type_match = false;

    status = JSON_Iterate(buf, len, &start, &next, &pair);
    while (status == JSONSuccess) {
        if (!pair.key) { continue; }

        if (strncmp("version", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
            version = json_parse_int(pair.value, pair.valueLength, 0);
        } else if (strncmp("device", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONString) {
            has_device = (strncasecmp(pair.value, "Printalyzer Meter Probe", pair.valueLength) == 0);
        } else if (strncmp("type", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONString) {
            if (info->probe_id.probe_type == METER_PROBE_SENSOR_TSL2585) {
                has_type_match = (strncmp(pair.value, "TSL2585", pair.valueLength) == 0);
            } else if (info->probe_id.probe_type == METER_PROBE_SENSOR_TSL2521) {
                has_type_match = (strncmp(pair.value, "TSL2521", pair.valueLength) == 0);
            }
        } else if (strncmp("revision", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
            revision = json_parse_int(pair.value, pair.valueLength, 0);
        }

        status = JSON_Iterate(buf, len, &start, &next, &pair);
    }

    if (version != HEADER_EXPORT_VERSION) {
        log_w("Export version mismatch");
        return false;
    }

    if (!has_device) {
        log_w("Device name mismatch");
        return false;
    }

    if (!has_type_match) {
        log_w("Device type mismatch");
        return false;
    }

    if (revision != info->probe_id.probe_rev_major) {
        log_w("Device revision mismatch");
        return false;
    }

    return true;
}

bool import_section_sensor_cal(const char *buf, size_t len, meter_probe_settings_t *settings)
{
    JSONStatus_t status;
    size_t start = 0;
    size_t next = 0;
    JSONPair_t pair = {0};
    bool has_gain = false;
    bool has_slope = false;
    bool has_target = false;

    /*
     * Iterate across the section, and import keys as they're found.
     * This code only does the most basic of validation, relying on the
     * settings API to implement stricter validation before actually saving
     * the values.
     */
    status = JSON_Iterate(buf, len, &start, &next, &pair);
    while (status == JSONSuccess) {
        if (pair.key) {
            if (strncmp("gain", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONArray) {
                has_gain = parse_section_sensor_cal_gain(pair.value, pair.valueLength,
                    &settings->settings_tsl2585.cal_gain);
            } else if (strncmp("slope", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONObject) {
                has_slope = parse_section_sensor_cal_slope(pair.value, pair.valueLength,
                    &settings->settings_tsl2585.cal_slope);
            } else if (strncmp("target", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONObject) {
                has_target = parse_section_sensor_cal_target(pair.value, pair.valueLength,
                    &settings->settings_tsl2585.cal_target);
            }
        }
        status = JSON_Iterate(buf, len, &start, &next, &pair);
    }

    return has_gain && has_slope && has_target;
}

bool parse_section_sensor_cal_gain(const char *buf, size_t len, meter_probe_settings_tsl2585_cal_gain_t *cal_gain)
{
    JSONStatus_t status;
    size_t start = 0;
    size_t next = 0;
    JSONPair_t pair = {0};
    tsl2585_gain_t gain = TSL2585_GAIN_0_5X;

    status = JSON_Iterate(buf, len, &start, &next, &pair);
    while (status == JSONSuccess && gain <= TSL2585_GAIN_256X) {
        if (!pair.key && pair.jsonType == JSONNumber) {
            cal_gain->values[gain] = json_parse_float(pair.value, pair.valueLength, NAN);
            gain++;
        }

        status = JSON_Iterate(buf, len, &start, &next, &pair);
    }

    /* Validate the loaded gain */
    if (gain < TSL2585_GAIN_256X) {
        return false;
    }

    for (gain = TSL2585_GAIN_1X; gain <= TSL2585_GAIN_256X; gain++) {
        if (cal_gain->values[gain] <= cal_gain->values[gain - 1]) {
            return false;
        }
    }

    return true;
}

bool parse_section_sensor_cal_slope(const char *buf, size_t len, meter_probe_settings_tsl2585_cal_slope_t *cal_slope)
{
    JSONStatus_t status;
    size_t start = 0;
    size_t next = 0;
    JSONPair_t pair = {0};

    cal_slope->b0 = NAN;
    cal_slope->b1 = NAN;
    cal_slope->b2 = NAN;

    status = JSON_Iterate(buf, len, &start, &next, &pair);
    while (status == JSONSuccess) {
        if (pair.key) {
            if (strncmp("b0", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                cal_slope->b0 =json_parse_float(pair.value, pair.valueLength, NAN);
            } else if (strncmp("b1", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                cal_slope->b1 =json_parse_float(pair.value, pair.valueLength, NAN);
            } else if (strncmp("b2", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                cal_slope->b2 =json_parse_float(pair.value, pair.valueLength, NAN);
            }
        }
        status = JSON_Iterate(buf, len, &start, &next, &pair);
    }

    if (is_valid_number(cal_slope->b0)
        && is_valid_number(cal_slope->b1)
        && is_valid_number(cal_slope->b2)) {
        return true;
    } else {
        return false;
    }
}

bool parse_section_sensor_cal_target(const char *buf, size_t len, meter_probe_settings_tsl2585_cal_target_t *cal_target)
{
    JSONStatus_t status;
    size_t start = 0;
    size_t next = 0;
    JSONPair_t pair = {0};

    cal_target->lux_slope = NAN;
    cal_target->lux_intercept = NAN;

    status = JSON_Iterate(buf, len, &start, &next, &pair);
    while (status == JSONSuccess) {
        if (pair.key) {
            if (strncmp("lux_slope", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                cal_target->lux_slope =json_parse_float(pair.value, pair.valueLength, NAN);
            } else if (strncmp("lux_intercept", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                cal_target->lux_intercept =json_parse_float(pair.value, pair.valueLength, NAN);
            }
        }
        status = JSON_Iterate(buf, len, &start, &next, &pair);
    }

    if (is_valid_number(cal_target->lux_slope)
        && is_valid_number(cal_target->lux_intercept)) {
        return true;
    } else {
        return false;
    }
}

menu_result_t meter_probe_sensor_calibration_export(meter_probe_handle_t *handle)
{
    char buf[256];
    char filename[64];
    uint8_t option;

    if (!usb_msc_is_mounted()) {
        option = display_message(
                "Export to USB device",
                NULL,
                "\n"
                "Please insert a USB storage\n"
                "device and try again.\n", " OK ");
        if (option == UINT8_MAX) {
            return MENU_TIMEOUT;
        } else {
            return MENU_OK;
        }
    }

    meter_probe_device_info_t info;
    if (meter_probe_get_device_info(handle, &info) != osOK) {
        return MENU_OK;
    }

    if ((info.probe_id.probe_type != METER_PROBE_SENSOR_TSL2585 && info.probe_id.probe_type != METER_PROBE_SENSOR_TSL2521)
        || !meter_probe_has_settings(handle)) {
        return MENU_OK;
    }

    meter_probe_settings_t settings;
    if (meter_probe_get_settings(handle, &settings) != osOK) {
        return MENU_OK;
    }

    if (settings.type != info.probe_id.probe_type) {
        return MENU_OK;
    }

    sprintf(filename, "mp-cal-%s.dat", info.probe_id.probe_serial);
    do {
        if (display_input_text("Calibration File Name", filename, sizeof(filename)) == 0) {
            return MENU_OK;
        }
    } while (scrub_export_filename(filename, ".dat"));

    if (export_calibration_file(filename, &info, &settings)) {
        sprintf(buf,
            "\n"
            "Calibration saved to file:\n"
            "%s\n", filename);
        option = display_message(
            "Export to USB device",
            NULL, buf, " OK ");
    } else {
        option = display_message(
            "Export to USB device",
            NULL,
            "\n"
            "Unable to save calibration!\n", " OK ");
    }

    if (option == UINT8_MAX) {
        return MENU_TIMEOUT;
    } else {
        return MENU_OK;
    }
}

bool export_calibration_file(const char *filename, const meter_probe_device_info_t *info, const meter_probe_settings_t *settings)
{
    FRESULT res;
    FIL fp;
    bool file_open = false;
    bool success = false;

    do {
        memset(&fp, 0, sizeof(FIL));

        res = f_open(&fp, filename, FA_WRITE | FA_CREATE_ALWAYS);
        if (res != FR_OK) {
            log_e("Error opening cal file: %d", res);
            break;
        }
        file_open = true;

        f_printf(&fp, "{\n");
        write_section_header(&fp, info);
        f_printf(&fp, ",\n");
        write_section_sensor_cal(&fp, settings);
        f_printf(&fp, "\n");
        f_printf(&fp, "}\n");

        log_d("Cal written to file: %s", filename);
        success = true;
    } while (0);

    if (file_open) {
        f_close(&fp);
    }

    return success;
}

bool write_section_header(FIL *fp, const meter_probe_device_info_t *info)
{
    f_printf(fp, "  \"header\": {\n");
    json_write_int(fp, 4, "version", HEADER_EXPORT_VERSION, true);
    json_write_string(fp, 4, "device", "Printalyzer Meter Probe", true);
    json_write_string(fp, 4, "type", meter_probe_type_str(info->probe_id.probe_type), true);
    json_write_int(fp, 4, "revision", info->probe_id.probe_rev_major, true);
    json_write_string(fp, 4, "serial", info->probe_id.probe_serial, false);
    f_printf(fp, "\n  }");
    return true;
}

bool write_section_sensor_cal(FIL *fp, const meter_probe_settings_t *settings)
{
    char buf[32];

    f_printf(fp, "  \"sensor_cal\": {\n");
    f_printf(fp, "    \"gain\": [\n");
    for (tsl2585_gain_t gain = 0; gain <= TSL2585_GAIN_256X; gain++) {
        const float gain_val = settings->settings_tsl2585.cal_gain.values[gain];

        if (is_valid_number(gain_val)) {
            sprintf(buf, "%0.6f", gain_val);
        } else {
            sprintf(buf, "null");
        }

        f_printf(fp, "      %s", buf);
        if (gain < TSL2585_GAIN_256X) {
            f_putc(',', fp);
        }
        f_putc('\n', fp);
    }
    f_printf(fp, "    ],\n");

    f_printf(fp, "    \"slope\": {\n");
    json_write_float06(fp, 6, "b0", settings->settings_tsl2585.cal_slope.b0, true);
    json_write_float06(fp, 6, "b1", settings->settings_tsl2585.cal_slope.b1, true);
    json_write_float06(fp, 6, "b2", settings->settings_tsl2585.cal_slope.b2, false);
    f_printf(fp, "\n    },\n");

    f_printf(fp, "    \"target\": {\n");
    json_write_float06(fp, 6, "lux_slope",
        settings->settings_tsl2585.cal_target.lux_slope, true);
    json_write_float06(fp, 6, "lux_intercept",
        settings->settings_tsl2585.cal_target.lux_intercept, false);
    f_printf(fp, "\n    }\n");
    f_printf(fp, "  }");
    return true;
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
                    meter_probe_type_str(info.probe_id.probe_type),
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
                            meter_probe_type_str(info.probe_id.probe_type),
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
                            meter_probe_type_str(info.probe_id.probe_type),
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
                            meter_probe_type_str(info.probe_id.probe_type),
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
                            meter_probe_type_str(info.probe_id.probe_type),
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

