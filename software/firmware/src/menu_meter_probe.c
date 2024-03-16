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
//#include "usb_host.h"
#include "core_json.h"
#include "file_picker.h"

#define HEADER_EXPORT_VERSION     1
#define MAX_CALIBRATION_FILE_SIZE 16384

static menu_result_t meter_probe_device_info();
static menu_result_t meter_probe_sensor_calibration();

static menu_result_t meter_probe_sensor_calibration_import();
static bool import_calibration_file(const char *filename, const meter_probe_device_info_t *info, meter_probe_settings_t *settings);
static bool validate_section_header(const char *buf, size_t len, const meter_probe_device_info_t *info);
static bool import_section_sensor_cal(const char *buf, size_t len, meter_probe_settings_t *settings);
static bool parse_section_sensor_cal_gain(const char *buf, size_t len, meter_probe_settings_tsl2585_cal_gain_t *cal_gain);
static bool parse_section_sensor_cal_slope(const char *buf, size_t len, meter_probe_settings_tsl2585_cal_slope_t *cal_slope);
static bool parse_section_sensor_cal_target(const char *buf, size_t len, meter_probe_settings_tsl2585_cal_target_t *cal_target);

//static void parse_section_sensor_cal_gain_entry(const char *buf, size_t len, meter_probe_settings_tsl2585_gain_cal_t *gain_cal);

static menu_result_t meter_probe_sensor_calibration_export();
static bool export_calibration_file(const char *filename, const meter_probe_device_info_t *info, const meter_probe_settings_t *settings);
static bool write_section_header(FIL *fp, const meter_probe_device_info_t *info);
static bool write_section_sensor_cal(FIL *fp, const meter_probe_settings_t *settings);

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

    /* Enable oscillator calibration */
    meter_probe_sensor_enable_osc_calibration();

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
    meter_probe_sensor_disable_osc_calibration();
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
        offset += menu_build_padded_str_row(buf + offset, "Probe Type",
            meter_probe_type_str(info.probe_id.probe_type));
        offset += menu_build_padded_format_row(buf + offset, "Revision", "%d", info.probe_id.probe_revision);
        offset += menu_build_padded_format_row(buf + offset, "Serial Number", "%03d", info.probe_id.probe_serial);
        offset += menu_build_padded_format_row(buf + offset, "Sensor ID", "%02X%02X%02X",
            info.sensor_id[0], info.sensor_id[1], info.sensor_id[2]);
        offset += menu_build_padded_format_row(buf + offset, "Memory ID", "%02X%02X%02X",
            info.probe_id.memory_id[0], info.probe_id.memory_id[1], info.probe_id.memory_id[2]);
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
    char buf[640];

    meter_probe_settings_t settings;
    if (meter_probe_get_settings(&settings) != osOK) {
        return MENU_OK;
    }

    if (settings.type != METER_PROBE_TYPE_TSL2585 && settings.type != METER_PROBE_TYPE_TSL2521) {
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
            menu_result = meter_probe_sensor_calibration_import();
            if (menu_result == MENU_SAVE) {
                meter_probe_stop();
                if (meter_probe_start() != osOK) {
                    menu_result = MENU_TIMEOUT;
                } else {
                    menu_result = MENU_OK;
                    break;
                }
            }
        } else if (option == option_offset + 2) {
            menu_result = meter_probe_sensor_calibration_export();
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}

menu_result_t meter_probe_sensor_calibration_import()
{
    char buf[256];
    char path_buf[256];
    uint8_t option;
    meter_probe_settings_t imported_settings = {0};

    //TODO Rewrite for new USB host stack
    if (true /*XXX !usb_msc_is_mounted()*/) {
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
    if (meter_probe_get_device_info(&info) != osOK) {
        return MENU_OK;
    }

    imported_settings.type = info.probe_id.probe_type;

    option = file_picker_show("Select Calibration File", path_buf, sizeof(path_buf));
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
                if (meter_probe_set_settings(&imported_settings) == osOK) {
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
            if (info->probe_id.probe_type == METER_PROBE_TYPE_TSL2585) {
                has_type_match = (strncmp(pair.value, "TSL2585", pair.valueLength) == 0);
            } else if (info->probe_id.probe_type == METER_PROBE_TYPE_TSL2521) {
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

    if (revision != info->probe_id.probe_revision) {
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

menu_result_t meter_probe_sensor_calibration_export()
{
    char buf[256];
    char filename[32];
    uint8_t option;

    //TODO Rewrite for new USB host stack
    if (true /*XXX !usb_msc_is_mounted()*/) {
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
    if (meter_probe_get_device_info(&info) != osOK) {
        return MENU_OK;
    }

    if ((info.probe_id.probe_type != METER_PROBE_TYPE_TSL2585 && info.probe_id.probe_type != METER_PROBE_TYPE_TSL2521)
        || !meter_probe_has_settings()) {
        return MENU_OK;
    }

    meter_probe_settings_t settings;
    if (meter_probe_get_settings(&settings) != osOK) {
        return MENU_OK;
    }

    if (settings.type != info.probe_id.probe_type) {
        return MENU_OK;
    }

    sprintf(filename, "mp-cal-%03ld.dat", info.probe_id.probe_serial);
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
    json_write_int(fp, 4, "revision", info->probe_id.probe_revision, true);
    json_write_int(fp, 4, "serial", (int)info->probe_id.probe_serial, false);
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

menu_result_t meter_probe_diagnostics()
{
    HAL_StatusTypeDef ret = HAL_OK;
    char buf[512];
    enlarger_config_t enlarger_config = {0};
    bool sensor_initialized = false;
    bool sensor_error = false;
    bool enlarger_enabled = false;
    bool gain_changed = false;
    bool time_changed = false;
    bool agc_changed = false;
    tsl2585_gain_t gain = 0;
    uint16_t sample_time = 0;
    uint16_t sample_count = 0;
    bool agc_enabled = false;
    bool single_shot = false;
    meter_probe_sensor_reading_t reading = {0};
    keypad_event_t keypad_event;
    bool key_changed = false;
    uint32_t elapsed_tick_buf[10] = {0};
    bool elapsed_tick_buf_full = false;
    size_t elapsed_tick_buf_pos = 0;
    const size_t elapsed_tick_buf_len = sizeof(elapsed_tick_buf) / sizeof(uint32_t);

    /* Get meter probe device info */
    meter_probe_device_info_t info;
    if (meter_probe_get_device_info(&info) != osOK) {
        return MENU_OK;
    }

    /* Load active enlarger config */
    uint8_t config_index = settings_get_default_enlarger_config_index();
    bool result = settings_get_enlarger_config(&enlarger_config, config_index);
    if (!(result && enlarger_config_is_valid(&enlarger_config))) {
        enlarger_config_set_defaults(&enlarger_config);
    }

    /* Make sure enlarger starts in a known state */
    illum_controller_refresh();
    enlarger_control_set_state_off(&(enlarger_config.control), false);

    if (meter_probe_sensor_set_gain(TSL2585_GAIN_256X) == osOK) {
        gain = TSL2585_GAIN_256X;
    }

    if (meter_probe_sensor_set_integration(719, 99) == osOK) {
        sample_time = 719;
        sample_count = 99;
    }

    /* Enable internal calibration on every sequencer round */
    meter_probe_sensor_set_mod_calibration(1);

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
                    if (sample_count > 10) {
                        sample_count -= 10;
                        time_changed = true;
                    }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_CONTRAST)) {
                if (sensor_initialized && !sensor_error) {
                    if (sample_count < (2047 - 10)) {
                        sample_count += 10;
                        time_changed = true;
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

        if (gain_changed || time_changed) {
            memset(elapsed_tick_buf, 0, sizeof(elapsed_tick_buf));
            elapsed_tick_buf_full = false;
            elapsed_tick_buf_pos = 0;

            if (gain_changed) {
                if (meter_probe_sensor_set_gain(gain) == osOK) {
                    gain_changed = false;
                }
            }

            if (time_changed) {
                if (meter_probe_sensor_set_integration(sample_time, sample_count) == osOK) {
                    time_changed = false;
                }
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
            const float basic_result = meter_probe_basic_result(&reading);
            const float atime = tsl2585_integration_time_ms(reading.sample_time, reading.sample_count);
            const float lux_result = meter_probe_lux_result(&reading);
            float elapsed_tick_avg = 0;

            if (reading.elapsed_ticks < lroundf(atime * 2)) {
                /* Track the moving average of measured integration time */
                elapsed_tick_buf[elapsed_tick_buf_pos] = reading.elapsed_ticks;
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

            if (reading.result_status == METER_SENSOR_RESULT_VALID) {
                sprintf(buf,
                    "%s (%s, %.2fms)\n"
                    "Data: %lu\n"
                    "Basic: %f, Lux: %f\n"
                    "[%s][%s] {%.2f}\n"
                    "%s",
                    meter_probe_type_str(info.probe_id.probe_type),
                    tsl2585_gain_str(reading.gain), atime,
                    reading.als_data,
                    basic_result, lux_result,
                    (enlarger_enabled ? "**" : "--"),
                    (agc_enabled ? "AGC" : "---"), elapsed_tick_avg,
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
                    "%s (%s, %.2fms)\n"
                    "%s\n"
                    "\n"
                    "[%s][%s]\n"
                    "%s",
                    meter_probe_type_str(info.probe_id.probe_type),
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

