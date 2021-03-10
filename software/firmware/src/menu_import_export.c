#include "menu_import_export.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>

#include <esp_log.h>
#include <ff.h>

#include "display.h"
#include "settings.h"
#include "usb_host.h"
#include "core_json.h"
#include "util.h"
#include "file_picker.h"
#include "app_descriptor.h"

static const char *TAG = "menu_import_export";

/*
 * Fixed filename for configuration import/export.
 * Once a file browser is available, this should simply become the
 * default choice for export.
 */
#define CONF_FILENAME "printalyzer-conf.dat"

/*
 * Maximum allowed size of the configuration file, since the entire file
 * is loaded into memory for parsing. If anything bigger is required, then
 * the parser needs to be redesigned to be able to work incrementally
 * off the file.
 */
#define MAX_CONF_FILE_SIZE 32768

#define HEADER_EXPORT_VERSION     1
#define CONFIG_EXPORT_VERSION     1
#define ENLARGER_EXPORT_VERSION   1
#define PAPER_EXPORT_VERSION      1
#define STEP_WEDGE_EXPORT_VERSION 1

static menu_result_t menu_import_config(state_controller_t *controller);
static bool import_config_file(const char *filename, bool *reload_paper);
static bool validate_section_header(const char *buf, size_t len);
static menu_result_t import_section_prompt(bool *has_settings, int *has_enlargers, int *has_papers, bool *has_step_wedge);
static int parse_section_version(const char *buf, size_t len);
static bool import_section_settings(const char *buf, size_t len, int *enlarger_profile_index, int *paper_profile_index);
static int import_section_enlargers(const char *buf, size_t len);
static bool parse_section_enlarger(const char *buf, size_t len, enlarger_profile_t *profile);
static int import_section_papers(const char *buf, size_t len);
static bool parse_section_paper(const char *buf, size_t len, paper_profile_t *profile);
static void parse_section_paper_grades(const char *buf, size_t len, paper_profile_t *profile);
static void parse_section_paper_grade_entry(const char *buf, size_t len, paper_profile_grade_t *grade);
static bool import_section_step_wedge(const char *buf, size_t len);
static int json_count_elements(const char *buf, size_t len);
static int json_parse_int(const char *buf, size_t len, int def_value);
static float json_parse_float(const char *buf, size_t len, float def_value);

static menu_result_t menu_export_config();
static bool export_config_file(const char *filename);
static bool write_section_header(FIL *fp);
static bool write_section_config(FIL *fp);
static bool write_section_enlargers(FIL *fp);
static bool write_section_papers(FIL *fp);
static bool write_section_step_wedge(FIL *fp);
static void json_write_string(FIL *fp, int indent, const char *key, const char *val, bool has_more);
static void json_write_int(FIL *fp, int indent, const char *key, int val, bool has_more);
static void json_write_float02(FIL *fp, int indent, const char *key, float val, bool has_more);
static bool scrub_export_filename(char *filename);

menu_result_t menu_import_export(state_controller_t *controller)
{
    ESP_LOGI(TAG, "Import/Export menu");

    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;

    do {
        option = display_selection_list(
                "Import / Export Configuration", option,
                "Import from USB device\n"
                "Export to USB device");

        if (option == 1) {
            menu_result = menu_import_config(controller);
        } else if (option == 2) {
            menu_result = menu_export_config();
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}

menu_result_t menu_import_config(state_controller_t *controller)
{
    char buf[256];
    char path_buf[256];
    uint8_t option;
    bool reload_paper = false;

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

    option = file_picker_show("Select Configuration File", path_buf, sizeof(path_buf));
    if (option == MENU_TIMEOUT) {
        return MENU_TIMEOUT;
    } else if (option != MENU_OK) {
        return MENU_OK;
    }

    if (import_config_file(path_buf, &reload_paper)) {
        char filename[33];
        if (file_picker_expand_filename(filename, 33, path_buf)) {
            filename[32] = '\0';
        } else {
            sprintf(filename, "----");
        }
        sprintf(buf,
            "\n"
            "Configuration loaded from file:\n"
            "%s\n", filename);
        option = display_message(
            "Import from USB device",
            NULL, buf, " OK ");
    } else {
        option = display_message(
            "Import from USB device",
            NULL,
            "\n"
            "Configuration was not loaded\n", " OK ");
    }

    /* Reload active profiles that may have changed */
    state_controller_reload_enlarger_profile(controller);
    if (reload_paper) {
        state_controller_reload_paper_profile(controller, true);
    }

    if (option == UINT8_MAX) {
        return MENU_TIMEOUT;
    } else {
        return MENU_OK;
    }
}

bool import_config_file(const char *filename, bool *reload_paper)
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
    bool has_settings = false;
    int has_enlargers = 0;
    int has_papers = 0;
    bool has_step_wedge = false;
    int enlarger_profile_index = -1;
    int paper_profile_index = -1;
    int imported_enlargers = -1;
    int imported_papers = -1;
    menu_result_t menu_result;

    do {
        memset(&fp, 0, sizeof(FIL));

        res = f_open(&fp, filename, FA_READ | FA_OPEN_EXISTING);
        if (res != FR_OK) {
            ESP_LOGE(TAG, "Error opening config file: %d", res);
            break;
        }
        file_open = true;

        if (f_size(&fp) > MAX_CONF_FILE_SIZE) {
            ESP_LOGE(TAG, "File is too large: %lu > %d", f_size(&fp), MAX_CONF_FILE_SIZE);
            break;
        }

        ESP_LOGI(TAG, "Config file opened: %s", filename);

        /* Allocate buffer for file */
        file_buf = pvPortMalloc(f_size(&fp));
        if (!file_buf) {
            ESP_LOGE(TAG, "Unable to allocate buffer for file: %lu", f_size(&fp));
            break;
        }

        /* Read file into buffer */
        if (f_read(&fp, file_buf, f_size(&fp), &bytes_read) != FR_OK) {
            ESP_LOGE(TAG, "Error reading file");
            break;
        }
        if (bytes_read != f_size(&fp)) {
            ESP_LOGE(TAG, "Short read: %d != %lu", bytes_read, f_size(&fp));
            break;
        }

        /* Close file */
        f_close(&fp);
        file_open = false;

        ESP_LOGI(TAG, "Config file loaded into buffer");

        /* Validate the JSON */
        if (JSON_Validate(file_buf, bytes_read) != JSONSuccess) {
            ESP_LOGW(TAG, "JSON invalid");
            break;
        }

        ESP_LOGI(TAG, "Config file validated as JSON");

        /* Traverse the top level to see what sections are in the file */
        start = 0;
        next = 0;
        status = JSON_Iterate(file_buf, bytes_read, &start, &next, &pair);
        while (status == JSONSuccess) {
            if (pair.key) {
                if (strncmp("header", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONObject) {
                    has_valid_header = validate_section_header(pair.value, pair.valueLength);
                    if (!has_valid_header) {
                        break;
                    }
                } else if (strncmp("settings", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONObject) {
                    has_settings = json_count_elements(pair.value, pair.valueLength) > 0;
                } else if (strncmp("enlargers", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONArray) {
                    has_enlargers = json_count_elements(pair.value, pair.valueLength);
                } else if (strncmp("papers", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONArray) {
                    has_papers = json_count_elements(pair.value, pair.valueLength);
                } else if (strncmp("step_wedge", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONObject) {
                    has_step_wedge = json_count_elements(pair.value, pair.valueLength) > 0;
                }
            }

            status = JSON_Iterate(file_buf, bytes_read, &start, &next, &pair);
        }

        if (!has_valid_header) {
            ESP_LOGW(TAG, "File does not contain valid header");
            break;
        }
        ESP_LOGI(TAG, "Found valid header");

        ESP_LOGI(TAG, "Found sections: settings = %d, enlargers = %d, papers = %d, step_wedge = %d",
            has_settings, has_enlargers, has_papers, has_step_wedge);

        /* Show user prompt of sections to import */
        menu_result = import_section_prompt(&has_settings, &has_enlargers, &has_papers, &has_step_wedge);
        if (menu_result != MENU_SAVE) {
            /* No sections were selected */
            break;
        }
        if (!has_settings && !has_enlargers && !has_papers && !has_step_wedge) {
            /* No sections were found */
            break;
        }

        ESP_LOGI(TAG, "Selected sections: settings = %d, enlargers = %d, papers = %d, step_wedge = %d",
            has_settings, has_enlargers, has_papers, has_step_wedge);

        /* Traverse the top level and import any selected sections */
        start = 0;
        next = 0;
        status = JSON_Iterate(file_buf, bytes_read, &start, &next, &pair);
        while (status == JSONSuccess) {
            if (!pair.key) { continue; }

            if (strncmp("settings", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONObject) {
                if (has_settings) {
                    import_section_settings(pair.value, pair.valueLength, &enlarger_profile_index, &paper_profile_index);
                }
            } else if (strncmp("enlargers", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONArray) {
                if (has_enlargers) {
                    imported_enlargers = import_section_enlargers(pair.value, pair.valueLength);
                }
            } else if (strncmp("papers", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONArray) {
                if (has_papers) {
                    imported_papers = import_section_papers(pair.value, pair.valueLength);
                }
            } else if (strncmp("step_wedge", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONObject) {
                if (has_step_wedge) {
                    import_section_step_wedge(pair.value, pair.valueLength);
                }
            }

            status = JSON_Iterate(file_buf, bytes_read, &start, &next, &pair);
        }

        success = true;
    } while (0);

    /* Set default indices for enlarger and paper sections */
    if (has_enlargers && imported_enlargers > 0 && enlarger_profile_index >= 0) {
        if (enlarger_profile_index < imported_enlargers) {
            settings_set_default_enlarger_profile_index(enlarger_profile_index);
        } else {
            settings_set_default_enlarger_profile_index(0);
        }
    }
    if (has_papers && imported_papers > 0 && paper_profile_index >= 0) {
        if (paper_profile_index < imported_papers) {
            settings_set_default_paper_profile_index(paper_profile_index);
        } else {
            settings_set_default_paper_profile_index(0);
        }
        if (reload_paper) { *reload_paper = true; }
    }

    if (file_open) {
        f_close(&fp);
    }
    if (file_buf) {
        vPortFree(file_buf);
    }

    return success;
}

bool validate_section_header(const char *buf, size_t len)
{
    JSONStatus_t status;
    size_t start = 0;
    size_t next = 0;
    JSONPair_t pair = {0};
    int version = -1;
    bool has_device = false;

    status = JSON_Iterate(buf, len, &start, &next, &pair);
    while (status == JSONSuccess) {
        if (!pair.key) { continue; }

        if (strncmp("version", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
            version = json_parse_int(pair.value, pair.valueLength, 0);
        } else if (strncmp("device", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONString) {
            has_device = (strncasecmp(pair.value, "Printalyzer", pair.valueLength) == 0);
        }

        status = JSON_Iterate(buf, len, &start, &next, &pair);
    }

    return (version == HEADER_EXPORT_VERSION && has_device);
}

menu_result_t import_section_prompt(bool *has_settings, int *has_enlargers, int *has_papers, bool *has_step_wedge)
{
    char buf[256];
    size_t offset = 0;
    uint8_t option = 0;
    menu_result_t menu_result = MENU_OK;
    bool select_settings = true;
    bool select_enlargers = true;
    bool select_papers = true;
    bool select_step_wedge = true;

    if (!has_settings || !has_enlargers || !has_papers || !has_step_wedge) {
        return MENU_CANCEL;
    }

    if (!(*has_settings) && *has_enlargers == 0 && *has_papers == 0 && !(*has_step_wedge)) {
        option = display_message(
            "Import from USB device",
            NULL,
            "\n"
            "File does not contain any\n"
            "configurations.\n", " OK ");
        if (option == UINT8_MAX) {
            return MENU_TIMEOUT;
        } else {
            return MENU_CANCEL;
        }
    }

    do {
        offset = 0;
        if (*has_settings) {
            offset += sprintf(buf + offset, select_settings ? "[*]" : "[ ]");
            offset += sprintf(buf + offset, " Settings                    \n");
        }
        if (*has_enlargers > 0) {
            offset += sprintf(buf + offset, select_enlargers ? "[*]" : "[ ]");
            sprintf(buf + offset, " Enlarger profiles (%d)", *has_enlargers);
            offset += pad_str_to_length(buf + offset, ' ', DISPLAY_MENU_ROW_LENGTH - 3);
            offset += sprintf(buf + offset, "\n");
        }
        if (*has_papers > 0) {
            offset += sprintf(buf + offset, select_papers ? "[*]" : "[ ]");
            sprintf(buf + offset, " Paper profiles (%d)", *has_papers);
            offset += pad_str_to_length(buf + offset, ' ', DISPLAY_MENU_ROW_LENGTH - 3);
            offset += sprintf(buf + offset, "\n");
        }
        if (*has_step_wedge) {
            offset += sprintf(buf + offset, select_step_wedge ? "[*]" : "[ ]");
            offset += sprintf(buf + offset, " Step wedge properties       \n");
        }
        offset += sprintf(buf + offset, "*** Import selected ***");

        option = display_selection_list("Import from USB device", option, buf);
        if (option == 1) {
            select_settings = !select_settings;
        } else if (option == 2) {
            select_enlargers = !select_enlargers;
        } else if (option == 3) {
            select_papers = !select_papers;
        } else if (option == 4) {
            select_step_wedge = !select_step_wedge;
        } else if (option == 5) {
            if (!select_settings && !select_enlargers && !select_papers && !select_step_wedge) {
                continue;
            }
            if (*has_settings) {
                *has_settings = select_settings;
            }
            if (*has_enlargers > 0) {
                *has_enlargers = select_enlargers ? *has_enlargers : 0;
            }
            if (*has_papers > 0) {
                *has_papers = select_papers ? *has_papers : 0;
            }
            if (*has_step_wedge) {
                *has_step_wedge = select_step_wedge;
            }
            menu_result = MENU_SAVE;
            break;
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
            break;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}

int parse_section_version(const char *buf, size_t len)
{
    JSONStatus_t status;
    size_t start = 0;
    size_t next = 0;
    JSONPair_t pair = {0};
    int version = -1;

    status = JSON_Iterate(buf, len, &start, &next, &pair);
    while (status == JSONSuccess) {
        if (pair.key) {
            if (strncmp("version", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                version = json_parse_int(pair.value, pair.valueLength, 0);
                break;
            }
        }
        status = JSON_Iterate(buf, len, &start, &next, &pair);
    }

    return version;
}

bool import_section_settings(const char *buf, size_t len, int *enlarger_profile_index, int *paper_profile_index)
{
    JSONStatus_t status;
    size_t start = 0;
    size_t next = 0;
    JSONPair_t pair = {0};
    int version = -1;

    /* Validate the section version up front */
    version = parse_section_version(buf, len);
    if (version != CONFIG_EXPORT_VERSION) {
        ESP_LOGW(TAG, "Settings section has invalid version: %d", version);
        return false;
    }

    /*
     * Iterate across the section, and import keys as they're found.
     * This code only does the most basic of validation, relying on the
     * settings API to implement stricter validation before actually saving
     * the values.
     */
    status = JSON_Iterate(buf, len, &start, &next, &pair);
    while (status == JSONSuccess) {
        if (pair.key) {
            if (strncmp("exposure_time", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                int num = json_parse_int(pair.value, pair.valueLength, -1);
                if (num > 0) {
                    settings_set_default_exposure_time(num);
                }
            } else if (strncmp("contrast_grade", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                int num = json_parse_int(pair.value, pair.valueLength, -1);
                if (num >= 0 && num <= UINT8_MAX) {
                    settings_set_default_contrast_grade(num);
                }
            } else if (strncmp("step_size", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                int num = json_parse_int(pair.value, pair.valueLength, -1);
                if (num >= 0 && num <= UINT8_MAX) {
                    settings_set_default_step_size(num);
                }
            } else if (strncmp("safelight_mode", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                int num = json_parse_int(pair.value, pair.valueLength, -1);
                if (num >= 0 && num <= UINT8_MAX) {
                    settings_set_safelight_mode(num);
                }
            } else if (strncmp("focus_timeout", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                int num = json_parse_int(pair.value, pair.valueLength, -1);
                if (num >= 0) {
                    settings_set_enlarger_focus_timeout(num);
                }
            } else if (strncmp("display_brightness", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                int num = json_parse_int(pair.value, pair.valueLength, -1);
                if (num >= 0 && num <= UINT8_MAX) {
                    settings_set_display_brightness(num);
                }
            } else if (strncmp("led_brightness", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                int num = json_parse_int(pair.value, pair.valueLength, -1);
                if (num >= 0 && num <= UINT8_MAX) {
                    settings_set_led_brightness(num);
                }
            } else if (strncmp("buzzer_volume", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                int num = json_parse_int(pair.value, pair.valueLength, -1);
                if (num >= 0 && num <= UINT8_MAX) {
                    settings_set_buzzer_volume(num);
                }
            } else if (strncmp("teststrip_mode", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                int num = json_parse_int(pair.value, pair.valueLength, -1);
                if (num >= 0 && num <= UINT8_MAX) {
                    settings_set_teststrip_mode(num);
                }
            } else if (strncmp("teststrip_patches", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                int num = json_parse_int(pair.value, pair.valueLength, -1);
                if (num >= 0 && num <= UINT8_MAX) {
                    settings_set_teststrip_patches(num);
                }
            } else if (strncmp("enlarger_profile_index", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                int num = json_parse_int(pair.value, pair.valueLength, -1);
                if (num >= 0 && num < MAX_ENLARGER_PROFILES) {
                    if (enlarger_profile_index) {
                        *enlarger_profile_index = num;
                    }
                }
            } else if (strncmp("paper_profile_index", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                int num = json_parse_int(pair.value, pair.valueLength, -1);
                if (num >= 0 && num < MAX_PAPER_PROFILES) {
                    if (paper_profile_index) {
                        *paper_profile_index = num;
                    }
                }
            } else if (strncmp("tcs3472_ga_factor", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                float num = json_parse_float(pair.value, pair.valueLength, NAN);
                if (!isnanf(num) && num >= 0.0F && num < 10.0F) {
                    settings_set_tcs3472_ga_factor(num);
                }
            }
        }
        status = JSON_Iterate(buf, len, &start, &next, &pair);
    }

    return true;
}

int import_section_enlargers(const char *buf, size_t len)
{
    JSONStatus_t status;
    size_t start = 0;
    size_t next = 0;
    JSONPair_t pair = {0};
    int count = 0;

    status = JSON_Iterate(buf, len, &start, &next, &pair);
    while (status == JSONSuccess) {
        if (!pair.key && pair.jsonType == JSONObject) {
            int version = parse_section_version(pair.value, pair.valueLength);
            if (version == ENLARGER_EXPORT_VERSION) {
                enlarger_profile_t profile;
                enlarger_profile_t existing_profile;
                if (parse_section_enlarger(pair.value, pair.valueLength, &profile)) {
                    /*
                     * Check if the loaded profile matches the already-saved
                     * profile at its index, and only store if it is different.
                     * However, increment the count in either case.
                     */
                    if (settings_get_enlarger_profile(&existing_profile, count)
                        && enlarger_profile_compare(&existing_profile, &profile)) {
                        ESP_LOGI(TAG, "Skipping unchanged profile at index: %d", count);
                        count++;
                    } else if (settings_set_enlarger_profile(&profile, count)) {
                        ESP_LOGI(TAG, "Updated profile at index: %d", count);
                        count++;
                    }
                } else {
                    ESP_LOGW(TAG, "Parsed profile is not valid");
                }
            } else {
                ESP_LOGW(TAG, "Enlarger element has invalid version: %d", version);
            }
        }

        status = JSON_Iterate(buf, len, &start, &next, &pair);
    }

    /*
     * If we imported a non-zero number of profiles, then remove profiles
     * beyond the last index we actually imported.
     */
    if (count > 0) {
        for (int i = count; i < MAX_ENLARGER_PROFILES; i++) {
            settings_clear_enlarger_profile(i);
        }
    }

    return count;
}

bool parse_section_enlarger(const char *buf, size_t len, enlarger_profile_t *profile)
{
    JSONStatus_t status;
    size_t start = 0;
    size_t next = 0;
    JSONPair_t pair = {0};

    memset(profile, 0, sizeof(enlarger_profile_t));

    status = JSON_Iterate(buf, len, &start, &next, &pair);
    while (status == JSONSuccess) {
        if (pair.key) {
            if (strncmp("name", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONString) {
                strncpy(profile->name, pair.value, MIN(pair.valueLength, 32));
                profile->name[31] = '\0';
            } else if (strncmp("turn_on_delay", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                profile->turn_on_delay = json_parse_int(pair.value, pair.valueLength, 0);
            } else if (strncmp("rise_time", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                profile->rise_time = json_parse_int(pair.value, pair.valueLength, 0);
            } else if (strncmp("rise_time_equiv", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                profile->rise_time_equiv = json_parse_int(pair.value, pair.valueLength, 0);
            } else if (strncmp("turn_off_delay", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                profile->turn_off_delay = json_parse_int(pair.value, pair.valueLength, 0);
            } else if (strncmp("fall_time", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                profile->fall_time = json_parse_int(pair.value, pair.valueLength, 0);
            } else if (strncmp("fall_time_equiv", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                profile->fall_time_equiv = json_parse_int(pair.value, pair.valueLength, 0);
            } else if (strncmp("color_temperature", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                profile->color_temperature = json_parse_int(pair.value, pair.valueLength, 0);
            }
        }
        status = JSON_Iterate(buf, len, &start, &next, &pair);
    }

    return enlarger_profile_is_valid(profile);
}

int import_section_papers(const char *buf, size_t len)
{
    JSONStatus_t status;
    size_t start = 0;
    size_t next = 0;
    JSONPair_t pair = {0};
    int count = 0;

    status = JSON_Iterate(buf, len, &start, &next, &pair);
    while (status == JSONSuccess) {
        if (!pair.key && pair.jsonType == JSONObject) {
            int version = parse_section_version(pair.value, pair.valueLength);
            if (version == PAPER_EXPORT_VERSION) {
                paper_profile_t profile;
                paper_profile_t existing_profile;
                if (parse_section_paper(pair.value, pair.valueLength, &profile)) {
                    /*
                     * Check if the loaded profile matches the already-saved
                     * profile at its index, and only store if it is different.
                     * However, increment the count in either case.
                     */
                    if (settings_get_paper_profile(&existing_profile, count)
                        && paper_profile_compare(&existing_profile, &profile)) {
                        ESP_LOGI(TAG, "Skipping unchanged profile at index: %d", count);
                        count++;
                    } else if (settings_set_paper_profile(&profile, count)) {
                        ESP_LOGI(TAG, "Updated profile at index: %d", count);
                        count++;
                    }
                } else {
                    ESP_LOGW(TAG, "Parsed profile is not valid");
                }
            } else {
                ESP_LOGW(TAG, "Paper element has invalid version: %d", version);
            }
        }

        status = JSON_Iterate(buf, len, &start, &next, &pair);
    }

    /*
     * If we imported a non-zero number of profiles, then remove profiles
     * beyond the last index we actually imported.
     */
    if (count > 0) {
        for (int i = count; i < MAX_PAPER_PROFILES; i++) {
            settings_clear_paper_profile(i);
        }
    }

    return count;
}

bool parse_section_paper(const char *buf, size_t len, paper_profile_t *profile)
{
    JSONStatus_t status;
    size_t start = 0;
    size_t next = 0;
    JSONPair_t pair = {0};

    memset(profile, 0, sizeof(paper_profile_t));

    status = JSON_Iterate(buf, len, &start, &next, &pair);
    while (status == JSONSuccess) {
        if (pair.key) {
            if (strncmp("name", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONString) {
                strncpy(profile->name, pair.value, MIN(pair.valueLength, 32));
                profile->name[31] = '\0';
            } else if (strncmp("grades", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONArray) {
                parse_section_paper_grades(pair.value, pair.valueLength, profile);
            } else if (strncmp("max_net_density", pair.key, pair.keyLength) == 0) {
                if (pair.jsonType == JSONNumber) {
                    profile->max_net_density = json_parse_float(pair.value, pair.valueLength, NAN);
                } else {
                    profile->max_net_density = NAN;
                }
            }
        }
        status = JSON_Iterate(buf, len, &start, &next, &pair);
    }

    return paper_profile_is_valid(profile);
}

void parse_section_paper_grades(const char *buf, size_t len, paper_profile_t *profile)
{
    JSONStatus_t status;
    size_t start = 0;
    size_t next = 0;
    JSONPair_t pair = {0};
    int count = 0;

    status = JSON_Iterate(buf, len, &start, &next, &pair);
    while (status == JSONSuccess && count < CONTRAST_GRADE_MAX) {
        if (!pair.key && pair.jsonType == JSONObject) {
            parse_section_paper_grade_entry(pair.value, pair.valueLength, &(profile->grade[count]));
            count++;
        }

        status = JSON_Iterate(buf, len, &start, &next, &pair);
    }
}

void parse_section_paper_grade_entry(const char *buf, size_t len, paper_profile_grade_t *grade)
{
    JSONStatus_t status;
    size_t start = 0;
    size_t next = 0;
    JSONPair_t pair = {0};

    status = JSON_Iterate(buf, len, &start, &next, &pair);
    while (status == JSONSuccess) {
        if (pair.key) {
            if (strncmp("ht_lev100", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                grade->ht_lev100 = json_parse_int(pair.value, pair.valueLength, 0);
            } else if (strncmp("hm_lev100", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                grade->hm_lev100 = json_parse_int(pair.value, pair.valueLength, 0);
            } else if (strncmp("hs_lev100", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                grade->hs_lev100 = json_parse_int(pair.value, pair.valueLength, 0);
            }
        }
        status = JSON_Iterate(buf, len, &start, &next, &pair);
    }
}

bool import_section_step_wedge(const char *buf, size_t len)
{
    JSONStatus_t status;
    size_t start = 0;
    size_t next = 0;
    JSONPair_t pair = {0};
    int version = -1;
    const char *name_buf = NULL;
    size_t name_len = 0;
    float base_density = NAN;
    float density_increment = NAN;
    int step_count = 0;
    const char *density_array_buf = NULL;
    size_t density_array_len = 0;
    step_wedge_t *wedge = NULL;
    step_wedge_t *existing_wedge = NULL;
    int count = 0;
    bool save_wedge = false;

    /* Validate the section version up front */
    version = parse_section_version(buf, len);
    if (version != STEP_WEDGE_EXPORT_VERSION) {
        ESP_LOGW(TAG, "Step wedge section has invalid version: %d", version);
        return false;
    }

    /*
     * Iterate across the section, taking note of top-level values as
     * they are found.
     */
    status = JSON_Iterate(buf, len, &start, &next, &pair);
    while (status == JSONSuccess) {
        if (pair.key) {
            if (strncmp("name", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONString) {
                name_buf = pair.value;
                name_len = pair.valueLength;
            } else if (strncmp("base_density", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                base_density = json_parse_float(pair.value, pair.valueLength, NAN);
            } else if (strncmp("density_increment", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                density_increment = json_parse_float(pair.value, pair.valueLength, NAN);
            } else if (strncmp("step_count", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONNumber) {
                step_count = json_parse_int(pair.value, pair.valueLength, 0);
            } else if (strncmp("step_densities", pair.key, pair.keyLength) == 0 && pair.jsonType == JSONArray) {
                density_array_buf = pair.value;
                density_array_len = pair.valueLength;
            }
        }
        status = JSON_Iterate(buf, len, &start, &next, &pair);
    }

    if (step_count < MIN_STEP_WEDGE_STEP_COUNT || step_count > MAX_STEP_WEDGE_STEP_COUNT
        || !is_valid_number(base_density) || !is_valid_number(density_increment)) {
        ESP_LOGW(TAG, "Step wedge section has invalid properties");
        return false;
    }

    wedge = step_wedge_create(step_count);
    if (!wedge) {
        ESP_LOGW(TAG, "Unable to allocate step wedge");
        return false;
    }

    if (name_buf && name_len > 0) {
        strncpy(wedge->name, name_buf, MIN(name_len, 32));
        wedge->name[31] = '\0';
    }

    wedge->base_density = base_density;
    wedge->density_increment = density_increment;

    if (density_array_buf && density_array_len > 0) {
        start = 0;
        next = 0;
        status = JSON_Iterate(density_array_buf, density_array_len, &start, &next, &pair);
        while (status == JSONSuccess && count < step_count) {
            if (!pair.key) {
                if (pair.jsonType == JSONNumber) {
                    wedge->step_density[count] = json_parse_float(pair.value, pair.valueLength, NAN);
                    count++;
                } else if (pair.jsonType == JSONNull) {
                    wedge->step_density[count] = NAN;
                    count++;
                }
            }
            status = JSON_Iterate(density_array_buf, density_array_len, &start, &next, &pair);
        }
    }

    /* Compare to existing saved wedge and only save if different */
    if (step_wedge_is_valid(wedge)) {
        if (settings_get_step_wedge(&existing_wedge)) {
            save_wedge = !step_wedge_compare(existing_wedge, wedge);
        } else {
            save_wedge = true;
        }
    } else {
        ESP_LOGW(TAG, "Step wedge data is invalid");
    }

    if (save_wedge) {
        settings_set_step_wedge(wedge);
    } else {
        ESP_LOGI(TAG, "Not saving step wedge");
    }

    step_wedge_free(wedge);
    step_wedge_free(existing_wedge);

    return true;
}

int json_count_elements(const char *buf, size_t len)
{
    JSONStatus_t status;
    size_t start = 0;
    size_t next = 0;
    JSONPair_t pair = {0};
    int count = 0;

    status = JSON_Iterate(buf, len, &start, &next, &pair);
    while (status == JSONSuccess) {
        count++;
        status = JSON_Iterate(buf, len, &start, &next, &pair);
    }

    return count;
}

int json_parse_int(const char *buf, size_t len, int def_value)
{
    char num_buf[12];
    char *endptr;
    long result;

    if (!buf || len == 0 || len > sizeof(num_buf) - 1) {
        return def_value;
    }

    memset(num_buf, 0, sizeof(num_buf));
    memcpy(num_buf, buf, len);

    errno = 0;
    result = strtol(num_buf, &endptr, 10);
    if (endptr == num_buf || *endptr != '\0' ||
        ((result == LONG_MIN || result == LONG_MAX) && errno == ERANGE)) {
        result = def_value;
    }
    return result;
}

float json_parse_float(const char *buf, size_t len, float def_value)
{
    char num_buf[32];
    char *endptr;
    float result;

    if (!buf || len == 0 || len > sizeof(num_buf) - 1) {
        return def_value;
    }

    memset(num_buf, 0, sizeof(num_buf));
    memcpy(num_buf, buf, len);

    errno = 0;
    result = strtof(num_buf, &endptr);
    if (endptr == num_buf || *endptr != '\0' ||
        ((result == -HUGE_VALF || result == HUGE_VALF) && errno == ERANGE)) {
        result = def_value;
    }
    return result;
}

menu_result_t menu_export_config()
{
    char buf[256];
    char filename[32];
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

    strcpy(filename, CONF_FILENAME);
    do {
        if (display_input_text("Configuration File Name", filename, sizeof(filename)) == 0) {
            return MENU_OK;
        }
    } while (scrub_export_filename(filename));

    if (export_config_file(filename)) {
        sprintf(buf,
            "\n"
            "Configuration saved to file:\n"
            "%s\n", filename);
        option = display_message(
            "Export to USB device",
            NULL, buf, " OK ");
    } else {
        option = display_message(
            "Export to USB device",
            NULL,
            "\n"
            "Unable to save configuration!\n", " OK ");
    }

    if (option == UINT8_MAX) {
        return MENU_TIMEOUT;
    } else {
        return MENU_OK;
    }
}

bool export_config_file(const char *filename)
{
    FRESULT res;
    FIL fp;
    bool file_open = false;
    bool success = false;

    do {
        memset(&fp, 0, sizeof(FIL));

        res = f_open(&fp, filename, FA_WRITE | FA_CREATE_ALWAYS);
        if (res != FR_OK) {
            ESP_LOGE(TAG, "Error opening config file: %d", res);
            break;
        }
        file_open = true;

        f_printf(&fp, "{\n");
        write_section_header(&fp);
        f_printf(&fp, ",\n");
        write_section_config(&fp);
        f_printf(&fp, ",\n");
        write_section_enlargers(&fp);
        f_printf(&fp, ",\n");
        write_section_papers(&fp);
        f_printf(&fp, ",\n");
        write_section_step_wedge(&fp);
        f_printf(&fp, "\n");
        f_printf(&fp, "}");

        ESP_LOGD(TAG, "Config written to file: %s", filename);
        success = true;
    } while (0);

    if (file_open) {
        f_close(&fp);
    }

    return success;
}

bool write_section_header(FIL *fp)
{
    const app_descriptor_t *app_descriptor = app_descriptor_get();
    f_printf(fp, "  \"header\": {\n");
    json_write_int(fp, 4, "version", HEADER_EXPORT_VERSION, true);
    json_write_string(fp, 4, "device", app_descriptor->project_name, true);
    json_write_string(fp, 4, "build_describe", app_descriptor->build_describe, true);
    json_write_string(fp, 4, "build_date", app_descriptor->build_date, false);
    f_printf(fp, "\n  }");
    return true;
}

bool write_section_config(FIL *fp)
{
    f_printf(fp, "  \"settings\": {\n");
    json_write_int(fp, 4, "version", CONFIG_EXPORT_VERSION, true);
    json_write_int(fp, 4, "exposure_time", settings_get_default_exposure_time(), true);
    json_write_int(fp, 4, "contrast_grade", (uint8_t)settings_get_default_contrast_grade(), true);
    json_write_int(fp, 4, "step_size", (uint8_t)settings_get_default_step_size(), true);
    json_write_int(fp, 4, "safelight_mode", (uint8_t)settings_get_safelight_mode(), true);
    json_write_int(fp, 4, "focus_timeout", settings_get_enlarger_focus_timeout(), true);
    json_write_int(fp, 4, "display_brightness", settings_get_display_brightness(), true);
    json_write_int(fp, 4, "led_brightness", settings_get_led_brightness(), true);
    json_write_int(fp, 4, "buzzer_volume", (uint8_t)settings_get_buzzer_volume(), true);
    json_write_int(fp, 4, "teststrip_mode", (uint8_t)settings_get_teststrip_mode(), true);
    json_write_int(fp, 4, "teststrip_patches", (uint8_t)settings_get_teststrip_patches(), true);
    json_write_int(fp, 4, "enlarger_profile_index", settings_get_default_enlarger_profile_index(), true);
    json_write_int(fp, 4, "paper_profile_index", settings_get_default_paper_profile_index(), true);
    json_write_float02(fp, 4, "tcs3472_ga_factor", settings_get_tcs3472_ga_factor(), false);
    f_printf(fp, "\n  }");
    return true;
}

bool write_section_enlargers(FIL *fp)
{
    int i;
    enlarger_profile_t profile;

    f_printf(fp, "  \"enlargers\": [\n");
    for (i = 0; i < MAX_ENLARGER_PROFILES; i++) {
        if (!settings_get_enlarger_profile(&profile, i)) {
            break;
        }
        if (i > 0) {
            f_printf(fp, ",\n");
        }
        f_printf(fp, "    {\n");
        json_write_int(fp, 6, "version", ENLARGER_EXPORT_VERSION, true);
        json_write_string(fp, 6, "name", profile.name, true);
        json_write_int(fp, 6, "turn_on_delay", profile.turn_on_delay, true);
        json_write_int(fp, 6, "rise_time", profile.rise_time, true);
        json_write_int(fp, 6, "rise_time_equiv", profile.rise_time_equiv, true);
        json_write_int(fp, 6, "turn_off_delay", profile.turn_off_delay, true);
        json_write_int(fp, 6, "fall_time", profile.fall_time, true);
        json_write_int(fp, 6, "fall_time_equiv", profile.fall_time_equiv, true);
        json_write_int(fp, 6, "color_temperature", profile.color_temperature, false);
        f_printf(fp, "\n    }");
    }
    if (i > 0) {
        f_printf(fp, "\n");
    }
    f_printf(fp, "  ]");

    return true;
}

bool write_section_papers(FIL *fp)
{
    int i;
    int j;
    paper_profile_t profile;

    f_printf(fp, "  \"papers\": [\n");
    for (i = 0; i < MAX_PAPER_PROFILES; i++) {
        if (!settings_get_paper_profile(&profile, i)) {
            break;
        }
        if (i > 0) {
            f_printf(fp, ",\n");
        }
        f_printf(fp, "    {\n");
        json_write_int(fp, 6, "version", PAPER_EXPORT_VERSION, true);
        json_write_string(fp, 6, "name", profile.name, true);

        f_printf(fp, "      \"grades\": [\n");

        for (j = 0; j < CONTRAST_GRADE_MAX; j++) {
            if (j > 0) {
                f_printf(fp, ",\n");
            }
            f_printf(fp, "        {\"ht_lev100\": %lu, \"hm_lev100\": %lu, \"hs_lev100\": %lu}",
                profile.grade[j].ht_lev100, profile.grade[j].hm_lev100, profile.grade[j].hs_lev100);
        }
        f_printf(fp, "\n      ],\n");
        json_write_float02(fp, 6, "max_net_density", profile.max_net_density, false);
        f_printf(fp, "\n    }");
    }
    if (i > 0) {
        f_printf(fp, "\n");
    }
    f_printf(fp, "  ]");

    return true;
}

bool write_section_step_wedge(FIL *fp)
{
    int i;
    char buf[32];
    step_wedge_t *wedge = NULL;

    if (!settings_get_step_wedge(&wedge)) {
        return false;
    }

    f_printf(fp, "  \"step_wedge\": {\n");
    json_write_int(fp, 4, "version", STEP_WEDGE_EXPORT_VERSION, true);
    json_write_string(fp, 4, "name", wedge->name, true);
    json_write_float02(fp, 4, "base_density", wedge->base_density, true);
    json_write_float02(fp, 4, "density_increment", wedge->density_increment, true);
    json_write_int(fp, 4, "step_count", wedge->step_count, true);
    f_printf(fp, "    \"step_densities\": [\n      ");
    for (i = 0; i < wedge->step_count; i++) {
        if (i > 0) {
            f_printf(fp, ", ");
        }
        if (is_valid_number(wedge->step_density[i])) {
            sprintf(buf, "%0.2f", wedge->step_density[i]);
            f_printf(fp, buf);
        } else {
            f_printf(fp, "null");
        }
    }
    f_printf(fp, "\n    ]\n");
    f_printf(fp, "  }");

    step_wedge_free(wedge);
    return true;
}

void json_write_string(FIL *fp, int indent, const char *key, const char *val, bool has_more)
{
    for (int i = 0; i < indent; i++) {
        f_putc(' ', fp);
    }

    f_printf(fp, "\"%s\": \"%s\"", key, val);

    if (has_more) {
        f_puts(",\n", fp);
    }
}

void json_write_int(FIL *fp, int indent, const char *key, int val, bool has_more)
{
    for (int i = 0; i < indent; i++) {
        f_putc(' ', fp);
    }

    f_printf(fp, "\"%s\": %d", key, val);

    if (has_more) {
        f_puts(",\n", fp);
    }
}

void json_write_float02(FIL *fp, int indent, const char *key, float val, bool has_more)
{
    char buf[32];
    if (!is_valid_number(val)) {
        sprintf(buf, "null");
    } else {
        sprintf(buf, "%0.2f", val);
    }

    for (int i = 0; i < indent; i++) {
        f_putc(' ', fp);
    }

    f_printf(fp, "\"%s\": %s", key, buf);

    if (has_more) {
        f_puts(",\n", fp);
    }
}

static const char* const VALID_NONALPHA_CHARS = "!#$%&'()-@^_`{}~+,;=[]";

bool scrub_export_filename(char *filename)
{
    char buf[32];
    size_t len;
    char ch;
    char *p;
    bool changed = false;

    /* Copy the filename to a working buffer, keeping only legal characters */
    len = MIN(strlen(filename), 30);
    memset(buf, 0, sizeof(buf));
    p = buf;
    for (size_t i = 0; i < len; i++) {
        ch = filename[i];
        if (isalnum(ch) || ch == ' ' || ch == '.' || ch > 0x80
            || strchr(VALID_NONALPHA_CHARS, ch)) {
            *p = ch;
            p++;
        }
    }

    /* Trim any trailing dots or spaces */
    while (p > buf && (*(p - 1) == '.' || *(p - 1) == ' ')) { p--; *p = '\0'; }

    /* Strip the file extension */
    p = strrchr(buf, '.');
    if (p && p > buf) {
        *p = '\0';
    }

    /* Truncate the name */
    buf[31] = '\0';
    buf[30] = '\0';
    buf[29] = '\0';
    buf[28] = '\0';
    buf[27] = '\0';
    buf[26] = '\0';

    /* Make sure the file ends in the correct extension */
    strcat(buf, ".dat");

    /* If the file actually changed, copy back and take note */
    if (strcmp(filename, buf) != 0) {
        strcpy(filename, buf);
        changed = true;
    }

    return changed;
}
