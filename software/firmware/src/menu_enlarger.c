#include "menu_enlarger.h"

#include <FreeRTOS.h>
#include <task.h>
#include <cmsis_os.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#define LOG_TAG "menu_enlarger"
#include <elog.h>

#include "display.h"
#include "keypad.h"
#include "relay.h"
#include "tcs3472.h"
#include "settings.h"
#include "enlarger_config.h"
#include "enlarger_control.h"
#include "illum_controller.h"
#include "dmx.h"
#include "util.h"

#define REFERENCE_READING_ITERATIONS 100
#define PROFILE_ITERATIONS 5
#define MAX_LOOP_DURATION pdMS_TO_TICKS(10000)

/*
 * The enlarger calibration process works by measuring the light output
 * of the enlarger over time, while running through a series of simulated
 * exposure cycles. The result of this process is a series of numbers that
 * define how the light output of the enlarger behaves on either end of
 * the controllable exposure process.
 *
 * The sensor polling loop used for calibration involves a channel read,
 * followed by a 5ms delay. The sensor itself is configured for an
 * integration time of 4.8ms, and the channel read operation has been
 * measured to take 0.14ms. There does not appear to be a way to synchronize
 * ourselves to the exact integration state of the sensor, so this approach
 * is hopefully sufficient for the data we are trying to collect.
 */

extern I2C_HandleTypeDef hi2c2;

typedef struct {
    float mean;
    uint16_t min;
    uint16_t max;
    float stddev;
} reading_stats_t;

typedef enum {
    CALIBRATION_OK,
    CALIBRATION_CANCEL,
    CALIBRATION_SENSOR_ERROR,
    CALIBRATION_ZERO_READING,
    CALIBRATION_SENSOR_SATURATED,
    CALIBRATION_INVALID_REFERENCE_STATS,
    CALIBRATION_FAIL
} calibration_result_t;

static menu_result_t menu_enlarger_config_edit(enlarger_config_t *config, uint8_t index);
static menu_result_t menu_enlarger_config_control_edit(enlarger_control_t *enlarger_control);
static menu_result_t menu_enlarger_config_control_contrast_edit(enlarger_control_t *enlarger_control);
static menu_result_t menu_enlarger_config_control_contrast_entry_edit(enlarger_control_t *enlarger_control, contrast_grade_t grade);
static menu_result_t menu_enlarger_config_control_exposure_entry_edit(enlarger_control_t *enlarger_control);
static menu_result_t menu_enlarger_config_control_test_relay(const enlarger_control_t *enlarger_control);
static menu_result_t menu_enlarger_config_control_test_dmx(const enlarger_control_t *enlarger_control);
static uint16_t dmx_adjust_value(uint16_t value, bool wide_mode);
static menu_result_t menu_enlarger_config_timing_edit(enlarger_timing_t *timing_profile);
static void menu_enlarger_delete_config(uint8_t index, size_t config_count);
static bool menu_enlarger_config_delete_prompt(const enlarger_config_t *config, uint8_t index);

static menu_result_t menu_enlarger_calibration(enlarger_config_t *result_config);
static calibration_result_t enlarger_calibration_start(enlarger_config_t *result_config);
static void enlarger_calibration_preview_result(const enlarger_config_t *profile);
static void enlarger_calibration_show_error(calibration_result_t result_error);
static calibration_result_t calibration_initialize_sensor();
static calibration_result_t calibration_collect_reference_stats(reading_stats_t *stats_on, reading_stats_t *stats_off, reading_stats_t *stats_color);
static calibration_result_t calibration_validate_reference_stats(reading_stats_t *stats_on, reading_stats_t *stats_off);
static calibration_result_t calibration_build_timing_profile(enlarger_timing_t *timing_profile, const reading_stats_t *stats_on, const reading_stats_t *stats_off);

static void calculate_reading_stats(reading_stats_t *stats, uint16_t *readings, size_t len);
static bool delay_with_cancel(uint32_t time_ms);

menu_result_t menu_enlarger_configs(state_controller_t *controller)
{
    menu_result_t menu_result = MENU_OK;

    char *config_name_list;
    config_name_list = pvPortMalloc(PROFILE_NAME_LEN * MAX_ENLARGER_CONFIGS);
    if (!config_name_list) {
        log_e("Unable to allocate memory for config name list");
        return MENU_OK;
    }

    char buf[640];
    size_t offset;
    bool reload_configs = true;
    uint8_t config_default_index = 0;
    size_t config_count = 0;
    uint8_t option = 1;
    do {
        offset = 0;
        if (reload_configs) {
            config_count = 0;
            config_default_index = settings_get_default_enlarger_config_index();
            for (size_t i = 0; i < MAX_ENLARGER_CONFIGS; i++) {
                if (!settings_get_enlarger_config_name(config_name_list + (i * PROFILE_NAME_LEN), i)) {
                    break;
                } else {
                    config_count = i + 1;
                }
            }
            log_i("Loaded %d configs, default is %d", config_count, config_default_index);
            reload_configs = false;
        }

        for (size_t i = 0; i < config_count; i++) {
            const char *config_name = config_name_list + (i * PROFILE_NAME_LEN);
            if (config_name && strlen(config_name) > 0) {
                sprintf(buf + offset, "%c%02d%c %s",
                    ((i == config_default_index) ? '<' : '['),
                    i + 1,
                    ((i == config_default_index) ? '>' : ']'),
                    config_name);
            } else {
                sprintf(buf + offset, "%c%02d%c Enlarger profile %d",
                    ((i == config_default_index) ? '<' : '['),
                    i + 1,
                    ((i == config_default_index) ? '>' : ']'),
                    i + 1);
            }
            offset += pad_str_to_length(buf + offset, ' ', DISPLAY_MENU_ROW_LENGTH);
            if (i < config_count) {
                buf[offset++] = '\n';
                buf[offset] = '\0';
            }
        }
        if (config_count < MAX_ENLARGER_CONFIGS) {
            sprintf(buf + offset, "*** Add New Profile ***");
        }

        uint16_t result = display_selection_list_params("Enlarger Configurations", option, buf,
            DISPLAY_MENU_ACCEPT_MENU | DISPLAY_MENU_ACCEPT_ADD_ADJUSTMENT);
        option = (uint8_t)(result & 0x00FF);
        keypad_key_t option_key = (uint8_t)((result & 0xFF00) >> 8);

        if (option == 0) {
            menu_result = MENU_CANCEL;
            break;
        } else if (option - 1 == config_count) {
            /* Add new configuration */
            enlarger_config_t working_config;
            enlarger_config_set_defaults(&working_config);
            working_config.name[0] = '\0';
            menu_result = menu_enlarger_config_edit(&working_config, UINT8_MAX);
            if (menu_result == MENU_SAVE) {
                menu_result = MENU_OK;
                if (settings_set_enlarger_config(&working_config, config_count)) {
                    log_i("New config added at index: %d", config_count);
                    strncpy(config_name_list + (config_count * PROFILE_NAME_LEN), working_config.name, PROFILE_NAME_LEN);
                    config_name_list[((config_count * PROFILE_NAME_LEN) + PROFILE_NAME_LEN) - 1] = '\0';
                    config_count++;
                }
            }
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        } else {
            /* Edit existing configuration */
            uint8_t config_index = option - 1;
            if (option_key == KEYPAD_MENU) {
                enlarger_config_t working_config;

                if (!settings_get_enlarger_config(&working_config, config_index)) {
                    log_w("Unable to load config at index: %d", config_index);
                    enlarger_config_set_defaults(&working_config);
                }

                menu_result = menu_enlarger_config_edit(&working_config, config_index);
                if (menu_result == MENU_SAVE) {
                    menu_result = MENU_OK;
                    if (settings_set_enlarger_config(&working_config, config_index)) {
                        log_i("Config saved at index: %d", config_index);

                        strncpy(config_name_list + (config_index * PROFILE_NAME_LEN), working_config.name, PROFILE_NAME_LEN);
                        config_name_list[((config_index * PROFILE_NAME_LEN) + PROFILE_NAME_LEN) - 1] = '\0';
                    }
                } else if (menu_result == MENU_DELETE) {
                    menu_result = MENU_OK;
                    menu_enlarger_delete_config(config_index, config_count);
                    reload_configs = true;
                }
            } else if (option_key == KEYPAD_ADD_ADJUSTMENT) {
                log_i("Set default config at index: %d", config_index);
                settings_set_default_enlarger_config_index(config_index);
                config_default_index = option - 1;
            }
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    vPortFree(config_name_list);

    // There are many paths in this menu that can change profile settings,
    // so its easiest to just reload the state controller's active profile
    // whenever exiting from this menu.
    state_controller_reload_enlarger_config(controller);

    /* Refresh the illumination controller, as it controls the DMX task */
    illum_controller_refresh();
    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

    return menu_result;
}

void menu_enlarger_delete_config(uint8_t index, size_t config_count)
{
    for (size_t i = index; i < MAX_ENLARGER_CONFIGS; i++) {
        if (i < config_count - 1) {
            enlarger_config_t config;
            if (!settings_get_enlarger_config(&config, i + 1)) {
                return;
            }
            if (!settings_set_enlarger_config(&config, i)) {
                return;
            }
        } else {
            settings_clear_enlarger_config(i);
            break;
        }
    }

    uint8_t default_config_index = settings_get_default_enlarger_config_index();
    if (default_config_index == index) {
        settings_set_default_enlarger_config_index(0);
    } else if (default_config_index > index) {
        settings_set_default_enlarger_config_index(default_config_index - 1);
    }
}

menu_result_t menu_enlarger_config_edit(enlarger_config_t *config, uint8_t index)
{
    char buf_title[32];
    char buf[512];
    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;
    bool config_dirty;

    if (index == UINT8_MAX) {
        sprintf(buf_title, "New Enlarger Profile");
        config_dirty = true;
    } else {
        sprintf(buf_title, "Enlarger Profile %d", index + 1);
        config_dirty = false;
    }

    do {
        size_t offset = 0;

        offset += menu_build_padded_str_row(buf + offset, "Name", config->name);
        offset += menu_build_padded_str_row(buf + offset, "Power control", config->control.dmx_control ? "DMX" : "Relay");
        offset += menu_build_padded_str_row(buf + offset, "Timing profile", "\xB7\xB7\xB7");

        if (!config->control.dmx_control) {
            offset += menu_build_padded_str_row(buf + offset, "Contrast filters",
                contrast_filter_name_str(config->contrast_filter));
        }

        offset += sprintf(buf + offset, "*** Test Enlarger ***\n");
        offset += sprintf(buf + offset, "*** Calibrate Timing ***");

        if (index != UINT8_MAX) {
            offset += sprintf(buf + offset, "\n*** Delete Profile ***");
        }

        option = display_selection_list(buf_title, option, buf);

        if (option == 1) {
            char working_text[PROFILE_NAME_LEN];
            strncpy(working_text, config->name, PROFILE_NAME_LEN);
            if (display_input_text("Enlarger Name", working_text, PROFILE_NAME_LEN) > 0) {
                if (strncmp(config->name, working_text, PROFILE_NAME_LEN) != 0) {
                    strncpy(config->name, working_text, PROFILE_NAME_LEN);
                    config_dirty = true;
                }
            }
        } else if (option == 2) {
            /* Edit enlarger power control settings */
            enlarger_control_t enlarger_control;
            memcpy(&enlarger_control, &(config->control), sizeof(enlarger_control_t));
            menu_result_t sub_result = menu_enlarger_config_control_edit(&enlarger_control);
            if (sub_result == MENU_SAVE) {
                memcpy(&(config->control), &enlarger_control, sizeof(enlarger_control_t));
                config_dirty = true;
            } else if (sub_result == MENU_TIMEOUT) {
                menu_result = MENU_TIMEOUT;
            }
        } else if (option == 3) {
            /* Edit timing profile */
            enlarger_timing_t timing_profile;
            memcpy(&timing_profile, &(config->timing), sizeof(enlarger_timing_t));
            menu_result_t sub_result = menu_enlarger_config_timing_edit(&timing_profile);
            if (sub_result == MENU_SAVE) {
                memcpy(&(config->timing), &timing_profile, sizeof(enlarger_timing_t));
                config_dirty = true;
            } else if (sub_result == MENU_TIMEOUT) {
                menu_result = MENU_TIMEOUT;
            }
        } else if (!config->control.dmx_control && option == 4) {
            /* Contrast filtration type */
            offset = 0;
            for (contrast_filter_t i = CONTRAST_FILTER_REGULAR; i < CONTRAST_FILTER_MAX; i++) {
                offset += sprintf(buf + offset, "%s\n", contrast_filter_name_str(i));
            }
            buf[offset - 1] = '\0';
            uint8_t sub_option = (uint8_t)config->contrast_filter;
            if (sub_option >= (uint8_t)CONTRAST_FILTER_MAX) { sub_option = 1; }
            else { sub_option = sub_option + 1; }

            sub_option = display_selection_list(
                "Contrast filters", sub_option, buf);
            if (sub_option == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                if (sub_option - 1 != (uint8_t)config->contrast_filter) {
                    config->contrast_filter = (contrast_filter_t)(sub_option - 1);
                    config_dirty = true;
                }
            }
        } else if (option == (config->control.dmx_control ? 4 : 5)) {
            /* Test Enlarger */
            menu_result_t sub_result;
            if (config->control.dmx_control) {
                sub_result = menu_enlarger_config_control_test_dmx(&(config->control));
            } else {
                sub_result = menu_enlarger_config_control_test_relay(&(config->control));
            }
            if (sub_result == MENU_TIMEOUT) {
                menu_result = MENU_TIMEOUT;
            }
        } else if (option == (config->control.dmx_control ? 5 : 6)) {
            log_d("Run calibration from profile editor");
            uint8_t sub_option = display_message(
                "Run Timing Calibration?\n",
                NULL,
                "This will overwrite the timing\n"
                "profile values with results\n"
                "from the calibration process.",
                " Start \n Cancel ");

            if (sub_option == 1) {
                menu_result_t sub_result = menu_enlarger_calibration(config);
                if (sub_result == MENU_OK) {
                    config_dirty = true;
                } else if (sub_result == MENU_TIMEOUT) {
                    menu_result = MENU_TIMEOUT;
                    break;
                }
            } else if (sub_option == 2) {
                /* Cancel selected */
                continue;
            } else if (sub_option == UINT8_MAX) {
                /* Return assuming timeout */
                menu_result = MENU_TIMEOUT;
                break;
            } else {
                /* Return assuming no timeout */
                continue;
            }
        } else if (option == (config->control.dmx_control ? 6 : 7)) {
            log_d("Delete config from config editor");
            if (menu_enlarger_config_delete_prompt(config, index)) {
                menu_result = MENU_DELETE;
                break;
            }
        } else if (option == 0 && config_dirty) {
            menu_result_t sub_result = menu_confirm_cancel(buf_title);
            if (sub_result == MENU_SAVE) {
                menu_result = MENU_SAVE;
            } else if (sub_result == MENU_OK || sub_result == MENU_TIMEOUT) {
                menu_result = sub_result;
                break;
            } else if (sub_result == MENU_CANCEL) {
                option = 1;
                continue;
            }
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }

    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}

menu_result_t menu_enlarger_config_control_edit(enlarger_control_t *enlarger_control)
{
    char buf[512];
    const char *value_str;
    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;
    bool has_rgb_channels = false;
    bool has_contrast_grades = false;
    bool config_dirty = false;

    do {
        size_t offset = 0;
        offset += menu_build_padded_str_row(buf + offset, "Control method",
            enlarger_control->dmx_control ? "DMX" : "Relay");

        if (enlarger_control->dmx_control) {
            switch (enlarger_control->channel_set) {
            case ENLARGER_CHANNEL_SET_RGB:
                value_str = "RGB";
                has_rgb_channels = true;
                break;
            case ENLARGER_CHANNEL_SET_RGBW:
                value_str = "RGB+W";
                has_rgb_channels = true;
                break;
            case ENLARGER_CHANNEL_SET_WHITE:
            default:
                value_str = "White";
                has_rgb_channels = false;
                break;
            }

            offset += menu_build_padded_str_row(buf + offset, "DMX channel set", value_str);

            offset += menu_build_padded_str_row(buf + offset, "DMX resolution",
                enlarger_control->dmx_wide_mode ? "16-bit" : "8-bit");

            switch (enlarger_control->channel_set) {
            case ENLARGER_CHANNEL_SET_RGB:
                offset += menu_build_padded_format_row(buf + offset,
                    "Channel IDs", "%3d][%3d][%3d",
                    enlarger_control->dmx_channel_red + 1,
                    enlarger_control->dmx_channel_green + 1,
                    enlarger_control->dmx_channel_blue + 1);
                break;
            case ENLARGER_CHANNEL_SET_RGBW:
                offset += menu_build_padded_format_row(buf + offset,
                    "Channel IDs", "%3d][%3d][%3d][%3d",
                    enlarger_control->dmx_channel_red + 1,
                    enlarger_control->dmx_channel_green + 1,
                    enlarger_control->dmx_channel_blue + 1,
                    enlarger_control->dmx_channel_white + 1);
                break;
            case ENLARGER_CHANNEL_SET_WHITE:
            default:
                offset += menu_build_padded_format_row(buf + offset,
                    "Channel ID", "%3d",
                    enlarger_control->dmx_channel_white + 1);
                break;
            }

            if (has_rgb_channels) {
                offset += menu_build_padded_str_row(buf + offset, "Contrast control",
                    (enlarger_control->contrast_mode == ENLARGER_CONTRAST_MODE_GREEN_BLUE)
                    ? "Green+Blue" : "None");
            }

            value_str = "Focus brightness";
            if (enlarger_control->dmx_wide_mode) {
                offset += menu_build_padded_format_row(buf + offset,
                    value_str, "%5d", enlarger_control->focus_value);
            } else {
                offset += menu_build_padded_format_row(buf + offset,
                    value_str, "%3d", (uint8_t)enlarger_control->focus_value);
            }

            if (has_rgb_channels) {
                value_str = "Safe (Red) brightness";
                if (enlarger_control->dmx_wide_mode) {
                    offset += menu_build_padded_format_row(buf + offset,
                        value_str, "%5d", enlarger_control->safe_value);
                } else {
                    offset += menu_build_padded_format_row(buf + offset,
                        value_str, "%3d", (uint8_t)enlarger_control->safe_value);
                }
            }

            if (has_rgb_channels && enlarger_control->contrast_mode == ENLARGER_CONTRAST_MODE_GREEN_BLUE) {
                offset += menu_build_padded_str_row(buf + offset, "Contrast grades", "\xB7\xB7\xB7");
                has_contrast_grades = true;
            } else {
                value_str = "Exposure brightness";
                if (has_rgb_channels) {
                    offset += menu_build_padded_str_row(buf + offset, value_str, "\xB7\xB7\xB7");
                } else {
                    if (enlarger_control->dmx_wide_mode) {
                        offset += menu_build_padded_format_row(buf + offset,
                            value_str, "%5d", enlarger_control->grade_values[CONTRAST_GRADE_2].channel_white);
                    } else {
                        offset += menu_build_padded_format_row(buf + offset,
                            value_str, "%3d", (uint8_t)enlarger_control->grade_values[CONTRAST_GRADE_2].channel_white);
                    }
                }
                has_contrast_grades = false;
            }
        } else {
            has_rgb_channels = false;
            has_contrast_grades = false;
        }
        buf[offset - 1] = '\0';

        option = display_selection_list("Enlarger Power Control", option, buf);

        if (option == 1) {
            enlarger_control->dmx_control = !enlarger_control->dmx_control;
            config_dirty = true;
        } else if (option == 2) {
            enlarger_channel_set_t channel_set = enlarger_control->channel_set;
            if (channel_set > ENLARGER_CHANNEL_SET_RGBW) { channel_set = 0; }

            uint8_t sub_option = display_selection_list(
                "DMX Channel Set", channel_set + 1,
                "White\n"
                "RGB\n"
                "RGB+W");

            if (sub_option > 0 && sub_option < UINT8_MAX) {
                channel_set = sub_option - 1;
                if (channel_set != enlarger_control->channel_set && channel_set <= ENLARGER_CHANNEL_SET_RGBW) {
                    enlarger_control->channel_set = channel_set;
                    config_dirty = true;
                }
            } else if (sub_option == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
                break;
            } else {
                continue;
            }
        } else if (option == 3) {
            enlarger_control->dmx_wide_mode = !enlarger_control->dmx_wide_mode;

            if (has_rgb_channels) {
                const bool has_white = (enlarger_control->channel_set == ENLARGER_CHANNEL_SET_RGBW);
                /* Check if the channel IDs are consecutive, and adjust them accordingly */
                if (enlarger_control->dmx_wide_mode) {
                    /*
                     * 8-bit -> 16-bit
                     * e.g.
                     * 0, 1, 2, 3 -> 0, 2, 4, 6
                     * 508, 509, 510, 511 -> 504, 506, 508, 510
                     */
                    if (enlarger_control->dmx_channel_green == enlarger_control->dmx_channel_red + 1
                        && enlarger_control->dmx_channel_blue == enlarger_control->dmx_channel_red + 2
                        && (!has_white || enlarger_control->dmx_channel_white == enlarger_control->dmx_channel_red + 3)
                        && enlarger_control->dmx_channel_red + (has_white ? 8 : 6) <= 512) {

                        enlarger_control->dmx_channel_green = enlarger_control->dmx_channel_red + 2;
                        enlarger_control->dmx_channel_blue = enlarger_control->dmx_channel_red + 4;
                        if (has_white) {
                            enlarger_control->dmx_channel_white = enlarger_control->dmx_channel_red + 6;
                        }
                    }
                } else {
                    /*
                     * 16-bit -> 8-bit
                     * 0, 2, 4, 6 -> 0, 1, 2, 3
                     */
                    if (enlarger_control->dmx_channel_green == enlarger_control->dmx_channel_red + 2
                        && enlarger_control->dmx_channel_blue == enlarger_control->dmx_channel_red + 4
                        && (!has_white || enlarger_control->dmx_channel_white == enlarger_control->dmx_channel_red + 6)) {
                        enlarger_control->dmx_channel_green = enlarger_control->dmx_channel_red + 1;
                        enlarger_control->dmx_channel_blue = enlarger_control->dmx_channel_red + 2;
                        if (has_white) {
                            enlarger_control->dmx_channel_white = enlarger_control->dmx_channel_red + 3;
                        }
                    }
                }
            }

            /* Scale the values for focus and safe modes */
            enlarger_control->focus_value = dmx_adjust_value(enlarger_control->focus_value, enlarger_control->dmx_wide_mode);
            enlarger_control->safe_value = dmx_adjust_value(enlarger_control->safe_value, enlarger_control->dmx_wide_mode);

            /* Scale the values for the contrast grades */
            for (size_t i = 0; i < CONTRAST_GRADE_MAX; i++) {
                enlarger_control->grade_values[i].channel_red = dmx_adjust_value(enlarger_control->grade_values[i].channel_red, enlarger_control->dmx_wide_mode);
                enlarger_control->grade_values[i].channel_green = dmx_adjust_value(enlarger_control->grade_values[i].channel_green, enlarger_control->dmx_wide_mode);
                enlarger_control->grade_values[i].channel_blue = dmx_adjust_value(enlarger_control->grade_values[i].channel_blue, enlarger_control->dmx_wide_mode);
                enlarger_control->grade_values[i].channel_white = dmx_adjust_value(enlarger_control->grade_values[i].channel_white, enlarger_control->dmx_wide_mode);
            }
            config_dirty = true;
        } else if (option == 4) {
            if (has_rgb_channels) {
                uint16_t ch_red = enlarger_control->dmx_channel_red + 1;
                uint16_t ch_green = enlarger_control->dmx_channel_green + 1;
                uint16_t ch_blue = enlarger_control->dmx_channel_blue + 1;
                uint16_t ch_white = enlarger_control->dmx_channel_white + 1;
                uint8_t sub_option = 0;

                do {
                    offset = 0;
                    offset += menu_build_padded_format_row(buf + offset, "Red channel", "%3d", ch_red);
                    offset += menu_build_padded_format_row(buf + offset, "Green channel", "%3d", ch_green);
                    offset += menu_build_padded_format_row(buf + offset, "Blue channel", "%3d", ch_blue);
                    if (enlarger_control->channel_set == ENLARGER_CHANNEL_SET_RGBW) {
                        offset += menu_build_padded_format_row(buf + offset, "White channel", "%3d", ch_white);
                    }
                    buf[offset - 1] = '\0';

                    sub_option = display_selection_list("DMX Channel IDs", sub_option, buf);

                    if (sub_option >= 1 && sub_option <= 4) {
                        const char *title;
                        uint16_t *value_sel;

                        switch (sub_option) {
                        case 1:
                            title = "Red Channel ID";
                            value_sel = &ch_red;
                            break;
                        case 2:
                            title = "Green Channel ID";
                            value_sel = &ch_green;
                            break;
                        case 3:
                            title = "Blue Channel ID";
                            value_sel = &ch_blue;
                            break;
                        case 4:
                        default:
                            title = "White Channel ID";
                            value_sel = &ch_white;
                            break;
                        }

                        if (display_input_value_u16(
                            title,
                            "Channel for the selected\n"
                            "enlarger light.\n",
                            "", value_sel, 1, 512, 3, "") == UINT8_MAX) {
                            menu_result = MENU_TIMEOUT;
                        }
                    } else if (sub_option == UINT8_MAX) {
                        menu_result = MENU_TIMEOUT;
                    } else {
                        continue;
                    }
                } while (sub_option > 0 && menu_result != MENU_TIMEOUT);

                if (sub_option == 0 &&
                    (ch_red != enlarger_control->dmx_channel_red + 1
                        || ch_green != enlarger_control->dmx_channel_green + 1
                        || ch_blue != enlarger_control->dmx_channel_blue + 1
                        || ch_white != enlarger_control->dmx_channel_white + 1)) {
                    enlarger_control->dmx_channel_red = ch_red - 1;
                    enlarger_control->dmx_channel_green = ch_green - 1;
                    enlarger_control->dmx_channel_blue = ch_blue - 1;
                    enlarger_control->dmx_channel_white = ch_white - 1;
                    config_dirty = true;
                }
            } else {
                uint16_t value_sel = enlarger_control->dmx_channel_white + 1;
                if (display_input_value_u16(
                    "White Channel ID",
                    "Channel for the enlarger's\n"
                    "white light.\n",
                    "", &value_sel, 1, 512, 3, "") == UINT8_MAX) {
                    menu_result = MENU_TIMEOUT;
                } else {
                    if (value_sel != enlarger_control->dmx_channel_white + 1) {
                        enlarger_control->dmx_channel_white = value_sel - 1;
                        config_dirty = true;
                    }
                }
            }
        } else if (has_rgb_channels && option == 5) {
            enlarger_contrast_mode_t contrast_mode = enlarger_control->contrast_mode;
            if (contrast_mode > ENLARGER_CONTRAST_MODE_GREEN_BLUE) { contrast_mode = 0; }

            uint8_t sub_option = display_selection_list(
                "Contrast Control", contrast_mode + 1,
                "None (Filtered White)\n"
                "Green + Blue");

            if (sub_option > 0 && sub_option < UINT8_MAX) {
                contrast_mode = sub_option - 1;
                if (contrast_mode != enlarger_control->contrast_mode && contrast_mode <= ENLARGER_CONTRAST_MODE_GREEN_BLUE) {
                    enlarger_control->contrast_mode = contrast_mode;
                    config_dirty = true;
                }
            } else if (sub_option == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
                break;
            } else {
                continue;
            }
        } else if (option == (has_rgb_channels ? 6 : 5)) {
            const char *title = "Focus Brightness Value";
            const char *msg =
                "Brightness when the enlarger\n"
                "is turned on for focusing.\n";

            if (enlarger_control->dmx_wide_mode) {
                uint16_t value_sel = enlarger_control->focus_value;
                if (display_input_value_u16(
                    title, msg,
                    "", &value_sel, 0, 65535, 5, " / 65535") == UINT8_MAX) {
                    menu_result = MENU_TIMEOUT;
                } else {
                    if (value_sel != enlarger_control->focus_value) {
                        enlarger_control->focus_value = value_sel;
                        config_dirty = true;
                    }
                }
            } else {
                uint8_t value_sel = enlarger_control->focus_value;
                if (display_input_value(
                    title, msg,
                    "", &value_sel, 0, 255, 3, " / 255") == UINT8_MAX) {
                    menu_result = MENU_TIMEOUT;
                } else {
                    if (value_sel != enlarger_control->focus_value) {
                        enlarger_control->focus_value = value_sel;
                        config_dirty = true;
                    }
                }
            }
        } else if (has_rgb_channels && option == 7) {
            const char *title = "Safe (Red) Brightness Value";
            const char *msg =
                "Brightness when the enlarger's\n"
                "red light is turned on between\n"
                "exposure steps.\n";

            if (enlarger_control->dmx_wide_mode) {
                uint16_t value_sel = enlarger_control->safe_value;
                if (display_input_value_u16(
                    title, msg,
                    "", &value_sel, 0, 65535, 5, " / 65535") == UINT8_MAX) {
                    menu_result = MENU_TIMEOUT;
                } else {
                    if (value_sel != enlarger_control->safe_value) {
                        enlarger_control->safe_value = value_sel;
                        config_dirty = true;
                    }
                }
            } else {
                uint8_t value_sel = enlarger_control->safe_value;
                if (display_input_value(
                    title, msg,
                    "", &value_sel, 0, 255, 3, " / 255") == UINT8_MAX) {
                    menu_result = MENU_TIMEOUT;
                } else {
                    if (value_sel != enlarger_control->safe_value) {
                        enlarger_control->safe_value = value_sel;
                        config_dirty = true;
                    }
                }
            }
        } else if (has_contrast_grades && option == 8) {
            menu_result_t sub_result = menu_enlarger_config_control_contrast_edit(enlarger_control);
            if (sub_result == MENU_SAVE) {
                config_dirty = true;
            } else if (sub_result == MENU_TIMEOUT) {
                menu_result = MENU_TIMEOUT;
            }
        } else if (!has_contrast_grades && has_rgb_channels && option == 8) {
            menu_result_t sub_result = menu_enlarger_config_control_exposure_entry_edit(enlarger_control);
            if (sub_result == MENU_SAVE) {
                config_dirty = true;
            } else if (sub_result == MENU_TIMEOUT) {
                menu_result = MENU_TIMEOUT;
            }
        } else if (!has_contrast_grades && !has_rgb_channels && option == 6) {
            const char *title = "Exposure Brightness Value";
            const char *msg =
                "Brightness when the enlarger's\n"
                "white light is turned on\n"
                "for paper exposure.\n";
            if (enlarger_control->dmx_wide_mode) {
                uint16_t value_sel = enlarger_control->grade_values[CONTRAST_GRADE_2].channel_white;
                if (display_input_value_u16(
                    title, msg,
                    "", &value_sel, 0, 65535, 5, " / 65535") == UINT8_MAX) {
                    menu_result = MENU_TIMEOUT;
                } else {
                    if (value_sel != enlarger_control->grade_values[CONTRAST_GRADE_2].channel_white) {
                        enlarger_control->grade_values[CONTRAST_GRADE_2].channel_white = value_sel;
                        config_dirty = true;
                    }
                }
            } else {
                uint8_t value_sel = enlarger_control->grade_values[CONTRAST_GRADE_2].channel_white;
                if (display_input_value(
                    title, msg,
                    "", &value_sel, 0, 255, 3, " / 255") == UINT8_MAX) {
                    menu_result = MENU_TIMEOUT;
                } else {
                    if (value_sel != enlarger_control->grade_values[CONTRAST_GRADE_2].channel_white) {
                        enlarger_control->grade_values[CONTRAST_GRADE_2].channel_white = value_sel;
                        config_dirty = true;
                    }
                }
            }
        } else if (option == 0 && config_dirty) {
            menu_result = MENU_SAVE;
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }

    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}

menu_result_t menu_enlarger_config_control_contrast_edit(enlarger_control_t *enlarger_control)
{
    char buf[256];
    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;
    bool contrast_dirty = false;

    const char *format_str = enlarger_control->dmx_wide_mode ? "%5d][%5d" : "%3d][%3d";
    const uint16_t format_mask = enlarger_control->dmx_wide_mode ? 0xFFFF : 0x00FF;

    do {
        size_t offset = 0;
        for (size_t i = 0; i < CONTRAST_WHOLE_GRADE_COUNT; i++) {
            char label_buf[10];
            strcpy(label_buf, "Grade ");
            strcat(label_buf, contrast_grade_str(CONTRAST_WHOLE_GRADES[i]));

            offset += menu_build_padded_format_row(buf + offset,
                label_buf, format_str,
                enlarger_control->grade_values[CONTRAST_WHOLE_GRADES[i]].channel_green & format_mask,
                enlarger_control->grade_values[CONTRAST_WHOLE_GRADES[i]].channel_blue & format_mask);
        }
        buf[offset - 1] = '\0';
        option = display_selection_list("Contrast Grades", option, buf);

        if (option >= 1 && option <= CONTRAST_WHOLE_GRADE_COUNT) {
            menu_result_t sub_result = menu_enlarger_config_control_contrast_entry_edit(enlarger_control, CONTRAST_WHOLE_GRADES[option - 1]);
            if (sub_result == MENU_SAVE) {
                contrast_dirty = true;
            } else if (sub_result == MENU_TIMEOUT) {
                menu_result = MENU_TIMEOUT;
            }
        } else if (option == 0 && contrast_dirty) {
            menu_result = MENU_SAVE;
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}

menu_result_t menu_enlarger_config_control_contrast_entry_edit(enlarger_control_t *enlarger_control, contrast_grade_t grade)
{
    char buf[256];
    char title_buf[10];
    menu_result_t menu_result = MENU_OK;
    uint8_t row = 0;

    const uint16_t max_val = enlarger_control->dmx_wide_mode ? UINT16_MAX : UINT8_MAX;
    uint16_t green_val = enlarger_control->grade_values[grade].channel_green;
    uint16_t blue_val = enlarger_control->grade_values[grade].channel_blue;

    strcpy(title_buf, "Grade ");
    strcat(title_buf, contrast_grade_str(grade));

    for (;;) {
        size_t offset = 0;
        offset += sprintf(buf + offset, "\n");
        if (enlarger_control->dmx_wide_mode) {
            offset += sprintf(buf + offset,
                " %c Green       [%5d / 65535]  \n"
                " %c Blue        [%5d / 65535]  \n",
                (row == 0 ? '*' : ' '), green_val,
                (row == 1 ? '*' : ' '), blue_val);
        } else {
            offset += sprintf(buf + offset,
                " %c Green           [%3d / 255]  \n"
                " %c Blue            [%3d / 255]  \n",
                (row == 0 ? '*' : ' '), (uint8_t)green_val,
                (row == 1 ? '*' : ' '), (uint8_t)blue_val);
        }
        display_static_list(title_buf, buf);

        keypad_event_t keypad_event;
        HAL_StatusTypeDef ret = keypad_wait_for_event(&keypad_event, MENU_TIMEOUT_MS);
        if (ret == HAL_OK) {
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_EXPOSURE) || keypad_event.key == KEYPAD_ENCODER_CW) {
                /* Increment active row with rollover */
                if (row == 0) {
                    if (green_val < max_val) { green_val++; }
                    else { green_val = 0; }
                } else if (row == 1) {
                    if (blue_val < max_val) { blue_val++; }
                    else { blue_val = 0; }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_EXPOSURE) || keypad_event.key == KEYPAD_ENCODER_CCW) {
                /* Decrement active row with rollover */
                if (row == 0) {
                    if (green_val > 0) { green_val--; }
                    else { green_val = max_val; }
                } else if (row == 1) {
                    if (blue_val > 0) { blue_val--; }
                    else { blue_val = max_val; }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_CONTRAST)) {
                /* Skip-increment active row */
                if (row == 0) {
                    if (green_val > max_val - 10) {
                        green_val = max_val;
                    } else {
                        green_val += 10;
                    }
                } else if (row == 1) {
                    if (blue_val > max_val - 10) {
                        blue_val = max_val;
                    } else {
                        blue_val += 10;
                    }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_CONTRAST)) {
                /* Skip-decrement active row */
                if (row == 0) {
                    if (green_val < 10) {
                        green_val = 0;
                    } else {
                        green_val -= 10;
                    }
                } else if (row == 1) {
                    if (blue_val < 10) {
                        blue_val = 0;
                    } else {
                        blue_val -= 10;
                    }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_ADD_ADJUSTMENT)) {
                /* Toggle active row */
                if (row == 0) { row = 1; }
                else { row = 0; }
            } else if (keypad_event.key == KEYPAD_MENU && !keypad_event.pressed) {
                /* Accept values, if changed, and return */
                if (green_val != enlarger_control->grade_values[grade].channel_green
                    || blue_val != enlarger_control->grade_values[grade].channel_blue) {
                    enlarger_control->grade_values[grade].channel_green = green_val;
                    enlarger_control->grade_values[grade].channel_blue = blue_val;
                    menu_result = MENU_SAVE;
                }
                break;
            } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                /* Reject values and return */
                break;
            }
        } else if (ret == HAL_TIMEOUT) {
            menu_result = MENU_TIMEOUT;
            break;
        }
    }

    return menu_result;
}

menu_result_t menu_enlarger_config_control_exposure_entry_edit(enlarger_control_t *enlarger_control)
{
    char buf[256];
    char title_buf[32];
    menu_result_t menu_result = MENU_OK;
    uint8_t row = 0;

    const contrast_grade_t grade = CONTRAST_GRADE_2;
    const uint16_t max_val = enlarger_control->dmx_wide_mode ? UINT16_MAX : UINT8_MAX;
    const bool has_white = enlarger_control->channel_set == ENLARGER_CHANNEL_SET_RGBW;
    uint16_t row_val[] = {
        enlarger_control->grade_values[grade].channel_red,
        enlarger_control->grade_values[grade].channel_green,
        enlarger_control->grade_values[grade].channel_blue,
        enlarger_control->grade_values[grade].channel_white
    };

    strcpy(title_buf, "Exposure Brightness");

    for (;;) {
        size_t offset = 0;
        offset += sprintf(buf + offset, "\n");
        if (enlarger_control->dmx_wide_mode) {
            offset += sprintf(buf + offset,
                " %c Red         [%5d / 65535]  \n"
                " %c Green       [%5d / 65535]  \n"
                " %c Blue        [%5d / 65535]  \n",
                (row == 0 ? '*' : ' '), row_val[0],
                (row == 1 ? '*' : ' '), row_val[1],
                (row == 2 ? '*' : ' '), row_val[2]);
            if (has_white) {
                offset += sprintf(buf + offset,
                    " %c White       [%5d / 65535]  ",
                    (row == 3 ? '*' : ' '), row_val[3]);
            }
        } else {
            offset += sprintf(buf + offset,
                " %c Red             [%3d / 255]  \n"
                " %c Green           [%3d / 255]  \n"
                " %c Blue            [%3d / 255]  \n",
                (row == 0 ? '*' : ' '), (uint8_t)row_val[0],
                (row == 1 ? '*' : ' '), (uint8_t)row_val[1],
                (row == 2 ? '*' : ' '), (uint8_t)row_val[2]);
            if (has_white) {
                offset += sprintf(buf + offset,
                    " %c White           [%3d / 255]  ",
                    (row == 3 ? '*' : ' '), (uint8_t)row_val[3]);
            }
        }
        display_static_list(title_buf, buf);

        keypad_event_t keypad_event;
        HAL_StatusTypeDef ret = keypad_wait_for_event(&keypad_event, MENU_TIMEOUT_MS);
        if (ret == HAL_OK) {
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_EXPOSURE) || keypad_event.key == KEYPAD_ENCODER_CW) {
                /* Increment active row with rollover */
                if (row_val[row] < max_val) { row_val[row]++; }
                else { row_val[row] = 0; }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_EXPOSURE) || keypad_event.key == KEYPAD_ENCODER_CCW) {
                /* Decrement active row with rollover */
                if (row_val[row] > 0) { row_val[row]--; }
                else { row_val[row] = max_val; }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_CONTRAST)) {
                /* Skip-increment active row */
                if (row_val[row] > max_val - 10) {
                    row_val[row] = max_val;
                } else {
                    row_val[row] += 10;
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_CONTRAST)) {
                /* Skip-decrement active row */
                if (row_val[row] < 10) {
                    row_val[row] = 0;
                } else {
                    row_val[row] -= 10;
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_ADD_ADJUSTMENT)) {
                /* Toggle active row */
                row++;
                if (row == 3 && !has_white) { row = 0; }
                else if (row > 3) { row = 0; }
            } else if (keypad_event.key == KEYPAD_MENU && !keypad_event.pressed) {
                /* Accept values, if changed, and return */
                if (row_val[0] != enlarger_control->grade_values[grade].channel_red
                    || row_val[1] != enlarger_control->grade_values[grade].channel_green
                    || row_val[2] != enlarger_control->grade_values[grade].channel_blue
                    || row_val[3] != enlarger_control->grade_values[grade].channel_white) {
                    enlarger_control->grade_values[grade].channel_red = row_val[0];
                    enlarger_control->grade_values[grade].channel_green = row_val[1];
                    enlarger_control->grade_values[grade].channel_blue = row_val[2];
                    enlarger_control->grade_values[grade].channel_white = row_val[3];
                    menu_result = MENU_SAVE;
                }
                break;
            } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                /* Reject values and return */
                break;
            }
        } else if (ret == HAL_TIMEOUT) {
            menu_result = MENU_TIMEOUT;
            break;
        }
    }

    return menu_result;
}

menu_result_t menu_enlarger_config_control_test_relay(const enlarger_control_t *enlarger_control)
{
    char buf[256];
    menu_result_t menu_result = MENU_OK;
    bool enlarger_on = false;

    enlarger_control_set_state(enlarger_control,
        ENLARGER_CONTROL_STATE_OFF, CONTRAST_GRADE_MAX, false);

    for (;;) {
        sprintf(buf,
            "\n\n"
            "Relay control [%s]",
            enlarger_on ? "**" : "  ");
        display_static_list("Enlarger Test", buf);

        keypad_event_t keypad_event;
        HAL_StatusTypeDef ret = keypad_wait_for_event(&keypad_event, MENU_TIMEOUT_MS);
        if (ret == HAL_OK) {
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_FOCUS)) {
                enlarger_on = !enlarger_on;

                enlarger_control_set_state(enlarger_control,
                    (enlarger_on ? ENLARGER_CONTROL_STATE_EXPOSURE : ENLARGER_CONTROL_STATE_OFF),
                    CONTRAST_GRADE_MAX, false);
            } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                break;
            }

        } else if (ret == HAL_TIMEOUT) {
            menu_result = MENU_TIMEOUT;
            break;
        }
    }

    enlarger_control_set_state(enlarger_control,
        ENLARGER_CONTROL_STATE_OFF, CONTRAST_GRADE_MAX, false);

    return menu_result;
}

menu_result_t menu_enlarger_config_control_test_dmx(const enlarger_control_t *enlarger_control)
{
    char buf[256];
    menu_result_t menu_result = MENU_OK;
    bool enlarger_on = false;
    int test_mode = 0;
    contrast_grade_t test_grade = CONTRAST_GRADE_MAX;

    const bool has_rgb =
        enlarger_control->channel_set == ENLARGER_CHANNEL_SET_RGB
        || enlarger_control->channel_set == ENLARGER_CHANNEL_SET_RGBW;
    const bool has_contrast = enlarger_control->contrast_mode != ENLARGER_CONTRAST_MODE_WHITE;

    /* Make sure the DMX port is running and the frame is clear */
    if (dmx_get_port_state() == DMX_PORT_DISABLED) {
        dmx_enable();
    }
    if (dmx_get_port_state() == DMX_PORT_ENABLED_IDLE) {
        dmx_start();
    }
    if (dmx_get_port_state() == DMX_PORT_ENABLED_TRANSMITTING) {
        dmx_clear_frame(false);
    }

    if (has_contrast) {
        test_grade = CONTRAST_GRADE_2;
    } else {
        test_grade = CONTRAST_GRADE_MAX;
    }

    for (;;) {
        size_t offset = 0;
        offset += sprintf(buf, "\n");
        offset += sprintf(buf + offset, "  Enlarger power          [%s]  \n", enlarger_on ? "**" : "  ");

        offset += sprintf(buf + offset, "  Mode              ");
        if (test_mode == 0) {
            offset += sprintf(buf + offset, "   [Focus]  \n");
        } else if (test_mode == 1) {
            offset += sprintf(buf + offset, "    [Safe]  \n");
        } else {
            offset += sprintf(buf + offset, "[Exposure]  \n");
        }

        offset += sprintf(buf + offset, "  Contrast grade    ");
        if (test_grade >= CONTRAST_GRADE_0 && test_grade <= CONTRAST_GRADE_5) {
            buf[offset++] = ' ';
            buf[offset] = '\0';
        }

        if (test_grade < CONTRAST_GRADE_MAX) {
            offset += sprintf(buf + offset, "[Grade %s]  \n", contrast_grade_str(test_grade));
        } else {
            offset += sprintf(buf + offset, "     [N/A]  \n");
        }

        display_static_list("Enlarger Test", buf);

        keypad_event_t keypad_event;
        HAL_StatusTypeDef ret = keypad_wait_for_event(&keypad_event, MENU_TIMEOUT_MS);
        if (ret == HAL_OK) {
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_FOCUS)) {
                enlarger_on = !enlarger_on;
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_EXPOSURE)) {
                if (test_mode == 0) {
                    if (has_rgb && enlarger_control->safe_value > 0) {
                        test_mode = 1;
                    } else {
                        test_mode = 2;
                    }
                } else if (test_mode == 1) {
                    test_mode = 2;
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_EXPOSURE)) {
                if (test_mode == 2) {
                    if (has_rgb && enlarger_control->safe_value > 0) {
                        test_mode = 1;
                    } else {
                        test_mode = 0;
                    }
                } else if (test_mode == 1) {
                    test_mode = 0;
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_CONTRAST) && has_contrast) {
                if (test_grade == CONTRAST_GRADE_00) {
                    test_grade++;
                } else if (test_grade <= CONTRAST_GRADE_4) {
                    test_grade += 2;
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_CONTRAST) && has_contrast) {
                if (test_grade == CONTRAST_GRADE_0) {
                    test_grade--;
                } else if (test_grade >= CONTRAST_GRADE_1) {
                    test_grade -= 2;
                }
            } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                break;
            }
        } else if (ret == HAL_TIMEOUT) {
            menu_result = MENU_TIMEOUT;
            break;
        }

        if (enlarger_on) {
            if (test_mode == 0) {
                enlarger_control_set_state(enlarger_control, ENLARGER_CONTROL_STATE_FOCUS, test_grade, false);
            } else if (test_mode == 1) {
                enlarger_control_set_state(enlarger_control, ENLARGER_CONTROL_STATE_SAFE, test_grade, false);
            } else if (test_mode == 2) {
                enlarger_control_set_state(enlarger_control, ENLARGER_CONTROL_STATE_EXPOSURE, test_grade, false);
            }
        } else {
            enlarger_control_set_state(enlarger_control, ENLARGER_CONTROL_STATE_OFF, test_grade, false);
        }
    }

    /* Clear DMX output frame */
    if (dmx_get_port_state() == DMX_PORT_ENABLED_TRANSMITTING) {
        dmx_stop();
    }

    /* Refresh the illumination controller, to return to our previous DMX state */
    illum_controller_refresh();
    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

    return menu_result;
}

uint16_t dmx_adjust_value(uint16_t value, bool wide_mode)
{
    if (wide_mode) {
        if (value == 0xFF) {
            return 0xFFFF;
        } else {
            return (value & 0xFF) << 8;
        }
    } else {
        return (value & 0xFF00) >> 8;
    }
}

menu_result_t menu_enlarger_config_timing_edit(enlarger_timing_t *timing_profile)
{
    char buf[512];
    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;
    bool profile_dirty = false;
    const char *title = "Enlarger Timing Profile";

    do {
        sprintf(buf,
            "Turn on delay          [%5lums]\n"
            "Rise time              [%5lums]\n"
            "Rise time equivalent   [%5lums]\n"
            "Turn off delay         [%5lums]\n"
            "Fall time              [%5lums]\n"
            "Fall time equivalent   [%5lums]\n"
            "Color temperature      [ %5luK]",
            timing_profile->turn_on_delay,
            timing_profile->rise_time,
            timing_profile->rise_time_equiv,
            timing_profile->turn_off_delay,
            timing_profile->fall_time,
            timing_profile->fall_time_equiv,
            timing_profile->color_temperature);

        option = display_selection_list(title, option, buf);

        if (option == 1) {
            uint16_t value_sel = timing_profile->turn_on_delay;
            if (display_input_value_u16(
                "Turn On Delay",
                "Time period from when the\n"
                "enlarger is activated until its\n"
                "light level starts to rise.\n",
                "", &value_sel, 0, UINT16_MAX, 5, " ms") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                if (timing_profile->turn_on_delay != value_sel) {
                    timing_profile->turn_on_delay = value_sel;
                    profile_dirty = true;
                }
            }
        } else if (option == 2) {
            uint16_t value_sel = timing_profile->rise_time;
            if (display_input_value_u16(
                "Rise Time",
                "Time period from when the\n"
                "enlarger light level starts to\n"
                "rise until it peaks.\n",
                "", &value_sel, 0, UINT16_MAX, 5, " ms") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                if (timing_profile->rise_time != value_sel) {
                    timing_profile->rise_time = value_sel;
                    profile_dirty = true;
                }
            }
        } else if (option == 3) {
            uint16_t value_sel = timing_profile->rise_time_equiv;
            if (display_input_value_u16(
                "Rise Time Equivalent",
                "Time period at peak output\n"
                "for an exposure equivalent to\n"
                "the rise time period.\n",
                "", &value_sel, 0, UINT16_MAX, 5, " ms") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                if (timing_profile->rise_time_equiv != value_sel) {
                    timing_profile->rise_time_equiv = value_sel;
                    profile_dirty = true;
                }
            }
        } else if (option == 4) {
            uint16_t value_sel = timing_profile->turn_off_delay;
            if (display_input_value_u16(
                "Turn Off Delay",
                "Time period from when the\n"
                "enlarger is deactivated until\n"
                "its light level starts to fall.\n",
                "", &value_sel, 0, UINT16_MAX, 5, " ms") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                if (timing_profile->turn_off_delay != value_sel) {
                    timing_profile->turn_off_delay = value_sel;
                    profile_dirty = true;
                }
            }
        } else if (option == 5) {
            uint16_t value_sel = timing_profile->fall_time;
            if (display_input_value_u16(
                "Fall Time",
                "Time period from when the\n"
                "enlarger light level starts to\n"
                "fall until it is completely off.\n",
                "", &value_sel, 0, UINT16_MAX, 5, " ms") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                if (timing_profile->fall_time != value_sel) {
                    timing_profile->fall_time = value_sel;
                    profile_dirty = true;
                }
            }
        } else if (option == 6) {
            uint16_t value_sel = timing_profile->fall_time_equiv;
            if (display_input_value_u16(
                "Fall Time Equivalent",
                "Time period at peak output\n"
                "for an exposure equivalent to\n"
                "the fall time period.\n",
                "", &value_sel, 0, UINT16_MAX, 5, " ms") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                if (timing_profile->fall_time_equiv != value_sel) {
                    timing_profile->fall_time_equiv = value_sel;
                    profile_dirty = true;
                }
            }
        } else if (option == 7) {
            uint16_t value_sel = timing_profile->color_temperature;
            if (display_input_value_u16(
                "Color Temperature",
                "The unfiltered color temperature\n"
                "of the enlarger lamp.\n",
                "", &value_sel, 0, UINT16_MAX, 5, " K") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                if (timing_profile->color_temperature != value_sel) {
                    timing_profile->color_temperature = value_sel;
                    profile_dirty = true;
                }
            }
        } else if (option == 0 && profile_dirty) {
            menu_result = MENU_SAVE;
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }

    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}

bool menu_enlarger_config_delete_prompt(const enlarger_config_t *config, uint8_t index)
{
    char buf_title[32];
    char buf[256];

    sprintf(buf_title, "Delete Configuration %d?", index + 1);

    if (config->name && strlen(config->name) > 0) {
        sprintf(buf, "\n%s\n", config->name);
    } else {
        sprintf(buf, "\n\n\n");
    }

    uint8_t option = display_message(buf_title, NULL, buf, " Yes \n No ");
    if (option == 1) {
        return true;
    } else {
        return false;
    }
}

menu_result_t menu_enlarger_calibration(enlarger_config_t *result_config)
{
    //TODO Enlarger calibration needs major rework to support the new meter probe sensor
    menu_result_t menu_result = MENU_CANCEL;

    illum_controller_safelight_state(ILLUM_SAFELIGHT_EXPOSURE);

    uint8_t option = display_message(
            "Enlarger Calibration",
            NULL,
            "\nMake sure your safelight is off,\n"
            "the meter probe is connected,\n"
            "and your enlarger is ready.",
            " Start ");

    if (option == 1) {
        calibration_result_t calibration_result = enlarger_calibration_start(result_config);
        if (calibration_result == CALIBRATION_OK) {
            enlarger_calibration_preview_result(result_config);
            menu_result = MENU_OK;
        } else if (calibration_result == CALIBRATION_CANCEL) {
            menu_result = MENU_CANCEL;
        } else {
            enlarger_calibration_show_error(calibration_result);
            menu_result = MENU_CANCEL;
        }
        relay_enlarger_enable(false);
    } else if (option == UINT8_MAX) {
        menu_result = MENU_TIMEOUT;
    }

    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

    return menu_result;
}

calibration_result_t enlarger_calibration_start(enlarger_config_t *result_config)
{
    calibration_result_t calibration_result;
    log_i("Starting enlarger calibration process");

    display_static_list("Enlarger Calibration", "\n\nInitializing...");

    // Turn everything off, just in case it isn't already off
    relay_safelight_enable(false);
    relay_enlarger_enable(false);

    calibration_result = calibration_initialize_sensor();
    if (calibration_result != CALIBRATION_OK) {
        log_e("Could not initialize sensor");
        return calibration_result;
    }

    if (!delay_with_cancel(1000)) {
        tcs3472_disable(&hi2c2);
        return CALIBRATION_CANCEL;
    }

    display_static_list("Enlarger Calibration", "\n\nCollecting Reference Points");

    reading_stats_t enlarger_on_stats;
    reading_stats_t enlarger_off_stats;
    reading_stats_t enlarger_color_stats;
    calibration_result = calibration_collect_reference_stats(&enlarger_on_stats, &enlarger_off_stats, &enlarger_color_stats);
    if (calibration_result != CALIBRATION_OK) {
        log_e("Could not collect reference stats");
        tcs3472_disable(&hi2c2);
        if (relay_enlarger_is_enabled()) {
            relay_enlarger_enable(false);
        }
        return calibration_result;
    }

    // Log statistics used for reference points
    log_i("Enlarger on");
    log_i("Mean = %.1f, Min = %d, Max = %d, StdDev = %.1f",
        enlarger_on_stats.mean, enlarger_on_stats.min, enlarger_on_stats.max,
        enlarger_on_stats.stddev);
    log_i("Enlarger off");
    log_i("Mean = %.1f, Min = %d, Max = %d, StdDev = %.1f",
        enlarger_off_stats.mean, enlarger_off_stats.min, enlarger_off_stats.max,
        enlarger_off_stats.stddev);
    log_i("Enlarger color temperature");
    log_i("Mean = %.1f, Min = %d, Max = %d, StdDev = %.1f",
        enlarger_color_stats.mean, enlarger_color_stats.min, enlarger_color_stats.max,
        enlarger_color_stats.stddev);

    calibration_result = calibration_validate_reference_stats(&enlarger_on_stats, &enlarger_off_stats);
    if (calibration_result != CALIBRATION_OK) {
        log_w("Reference stats are not usable for calibration");
        tcs3472_disable(&hi2c2);
        illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);
        return calibration_result;
    }

    if (!delay_with_cancel(1000)) {
        tcs3472_disable(&hi2c2);
        return CALIBRATION_CANCEL;
    }

    // Do the profiling process
    display_static_list("Enlarger Calibration", "\n\nProfiling enlarger...");

    enlarger_timing_t timing_profile;
    enlarger_timing_t timing_profile_sum;
    enlarger_timing_t timing_profile_inc[PROFILE_ITERATIONS];

    memset(&timing_profile, 0, sizeof(enlarger_timing_t));
    memset(&timing_profile_sum, 0, sizeof(enlarger_timing_t));

    for (int i = 0; i < PROFILE_ITERATIONS; i++) {
        log_i("Profile run %d...", i + 1);

        calibration_result = calibration_build_timing_profile(&timing_profile_inc[i], &enlarger_on_stats, &enlarger_off_stats);
        if (calibration_result != CALIBRATION_OK) {
            log_e("Could not build profile");
            tcs3472_disable(&hi2c2);
            illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

            if (relay_enlarger_is_enabled()) {
                relay_enlarger_enable(false);
            }

            return calibration_result;
        }

        timing_profile_sum.turn_on_delay += timing_profile_inc[i].turn_on_delay;
        timing_profile_sum.rise_time += timing_profile_inc[i].rise_time;
        timing_profile_sum.rise_time_equiv += timing_profile_inc[i].rise_time_equiv;
        timing_profile_sum.turn_off_delay += timing_profile_inc[i].turn_off_delay;
        timing_profile_sum.fall_time += timing_profile_inc[i].fall_time;
        timing_profile_sum.fall_time_equiv += timing_profile_inc[i].fall_time_equiv;
    }
    log_i("Profile runs complete");

    // Average across all the runs
    timing_profile.turn_on_delay = roundf((float)timing_profile_sum.turn_on_delay / PROFILE_ITERATIONS);
    timing_profile.rise_time = roundf((float)timing_profile_sum.rise_time / PROFILE_ITERATIONS);
    timing_profile.rise_time_equiv = roundf((float)timing_profile_sum.rise_time_equiv / PROFILE_ITERATIONS);
    timing_profile.turn_off_delay = roundf((float)timing_profile_sum.turn_off_delay / PROFILE_ITERATIONS);
    timing_profile.fall_time = roundf((float)timing_profile_sum.fall_time / PROFILE_ITERATIONS);
    timing_profile.fall_time_equiv = roundf((float)timing_profile_sum.fall_time_equiv / PROFILE_ITERATIONS);
    timing_profile.color_temperature = roundf(enlarger_color_stats.mean);

    tcs3472_disable(&hi2c2);
    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

    log_i("Relay on delay: %ldms", timing_profile.turn_on_delay);
    log_i("Rise time: %ldms (full_equiv=%ldms)", timing_profile.rise_time, timing_profile.rise_time_equiv);
    log_i("Relay off delay: %ldms", timing_profile.turn_off_delay);
    log_i("Fall time: %ldms (full_equiv=%ldms)", timing_profile.fall_time, timing_profile.fall_time_equiv);
    log_i("Color temperature: %ldK", timing_profile.color_temperature);

    if (result_config) {
        result_config->timing.turn_on_delay = timing_profile.turn_on_delay;
        result_config->timing.rise_time = timing_profile.rise_time;
        result_config->timing.rise_time_equiv = timing_profile.rise_time_equiv;
        result_config->timing.turn_off_delay = timing_profile.turn_off_delay;
        result_config->timing.fall_time = timing_profile.fall_time;
        result_config->timing.fall_time_equiv = timing_profile.fall_time_equiv;
        result_config->timing.color_temperature = timing_profile.color_temperature;
    }

    return CALIBRATION_OK;
}

void enlarger_calibration_preview_result(const enlarger_config_t *config)
{
    char buf[256];
    sprintf(buf, "\n"
        "Enlarger on: %ldms\n"
        "Rise time: %ldms (%ldms)\n"
        "Enlarger off: %ldms\n"
        "Fall time: %ldms (%ldms)",
        config->timing.turn_on_delay,
        config->timing.rise_time, config->timing.rise_time_equiv,
        config->timing.turn_off_delay,
        config->timing.fall_time, config->timing.fall_time_equiv);
    do {
        uint8_t option = display_message(
            "Enlarger Calibration Done",
            NULL, buf,
            " Done ");
        if (option == 1) {
            break;
        }
    } while (1);
}

void enlarger_calibration_show_error(calibration_result_t result_error)
{
    const char *msg = NULL;

    switch (result_error) {
    case CALIBRATION_SENSOR_ERROR:
        msg =
            "The meter probe is either\n"
            "disconnected or not working\n"
            "correctly.";
        break;
    case CALIBRATION_ZERO_READING:
        msg =
            "The meter probe is incorrectly\n"
            "positioned or the enlarger is\n"
            "not turning on.";
        break;
    case CALIBRATION_SENSOR_SATURATED:
        msg =
            "The enlarger is too bright\n"
            "for calibration.";
        break;
    case CALIBRATION_INVALID_REFERENCE_STATS:
        msg =
            "Could not detect a large enough\n"
            "brightness difference between\n"
            "the enlarger being on and off.";
        break;
    case CALIBRATION_FAIL:
    default:
        msg =
            "Unable to complete\n"
            "enlarger calibration.";
        break;
    }

    do {
        uint8_t option = display_message(
            "Enlarger Calibration Failed\n",
            NULL, msg, " OK ");
        if (option == 1) { break; }
    } while (1);
}

calibration_result_t calibration_initialize_sensor()
{
    HAL_StatusTypeDef ret = HAL_OK;
    do {
        log_i("Initializing sensor");
        ret = tcs3472_init(&hi2c2);
        if (ret != HAL_OK) {
            log_e("Error initializing TCS3472: %d", ret);
            break;
        }

        ret = tcs3472_enable(&hi2c2);
        if (ret != HAL_OK) {
            log_e("Error enabling TCS3472: %d", ret);
            break;
        }

        ret = tcs3472_set_gain(&hi2c2, TCS3472_AGAIN_60X);
        if (ret != HAL_OK) {
            log_e("Error setting TCS3472 gain: %d", ret);
            break;
        }

        ret = tcs3472_set_time(&hi2c2, TCS3472_ATIME_4_8MS);
        if (ret != HAL_OK) {
            log_e("Error setting TCS3472 time: %d", ret);
            break;
        }

        bool valid = false;
        do {
            ret = tcs3472_get_status_valid(&hi2c2, &valid);
            if (ret != HAL_OK) {
                log_e("Error getting TCS3472 status: %d", ret);
                break;
            }
        } while (!valid);

        log_i("Sensor initialized");
    } while(0);

    if (ret != HAL_OK) {
        tcs3472_disable(&hi2c2);
        return CALIBRATION_SENSOR_ERROR;
    }
    return CALIBRATION_OK;
}

calibration_result_t calibration_collect_reference_stats(reading_stats_t *stats_on, reading_stats_t *stats_off, reading_stats_t *stats_color)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint16_t readings[REFERENCE_READING_ITERATIONS];
    uint16_t color_readings[REFERENCE_READING_ITERATIONS];

    if (!stats_on || !stats_off) { return false; }

    log_i("Turning enlarger on for baseline reading");
    relay_enlarger_enable(true);

    log_i("Waiting for light to stabilize");
    if (!delay_with_cancel(5000)) {
        return CALIBRATION_CANCEL;
    }

    log_i("Finding appropriate gain setting");
    bool gain_selected = false;
    for (tcs3472_again_t gain = TCS3472_AGAIN_60X; gain >= TCS3472_AGAIN_1X; --gain) {
        tcs3472_channel_data_t channel_data;

        ret = tcs3472_set_gain(&hi2c2, gain);
        if (ret != HAL_OK) {
            log_e("Error setting TCS3472 gain: %d (gain=%d)", ret, gain);
            return CALIBRATION_SENSOR_ERROR;
        }

        osDelay(pdMS_TO_TICKS(20));

        ret = tcs3472_get_full_channel_data(&hi2c2, &channel_data);
        if (ret != HAL_OK) {
            log_e("Error getting TCS3472 channel data: %d", ret);
            return CALIBRATION_SENSOR_ERROR;
        }

        if (channel_data.clear == 0) {
            log_w("No reading on clear channel");
            return CALIBRATION_ZERO_READING;
        }
        if (tcs3472_calculate_color_temp(&channel_data) > 0) {
            log_i("Selected gain: %s", tcs3472_gain_str(gain));
            gain_selected = true;
            break;
        }
    }
    if (!gain_selected) {
        log_w("Could not find a gain setting with a valid unsaturated reading");
        return CALIBRATION_SENSOR_SATURATED;
    }

    log_i("Collecting data with enlarger on");
    for (int i = 0; i < REFERENCE_READING_ITERATIONS; i++) {
        tcs3472_channel_data_t channel_data;
        ret = tcs3472_get_full_channel_data(&hi2c2, &channel_data);
        if (ret != HAL_OK) {
            log_e("Error getting TCS3472 channel data: %d", ret);
            return CALIBRATION_SENSOR_ERROR;
        }
        readings[i] = channel_data.clear;
        color_readings[i] = tcs3472_calculate_color_temp(&channel_data);
        osDelay(pdMS_TO_TICKS(5));
    }

    relay_enlarger_enable(false);

    log_i("Computing baseline enlarger reading statistics");
    calculate_reading_stats(stats_on, readings, REFERENCE_READING_ITERATIONS);
    if (stats_color) {
        calculate_reading_stats(stats_color, color_readings, REFERENCE_READING_ITERATIONS);
    }

    log_i("Waiting for light to stabilize");
    if (!delay_with_cancel(2000)) {
        return false;
    }

    log_i("Collecting data with enlarger off");
    for (int i = 0; i < REFERENCE_READING_ITERATIONS; i++) {
        uint16_t channel_data;
        ret = tcs3472_get_clear_channel_data(&hi2c2, &channel_data);
        if (ret != HAL_OK) {
            log_e("Error getting TCS3472 channel data: %d", ret);
            return false;
        }
        readings[i] = channel_data;
        osDelay(pdMS_TO_TICKS(5));
    }

    log_i("Computing baseline darkness reading statistics");
    calculate_reading_stats(stats_off, readings, REFERENCE_READING_ITERATIONS);

    return CALIBRATION_OK;
}

calibration_result_t calibration_validate_reference_stats(reading_stats_t *stats_on, reading_stats_t *stats_off)
{
    if (!stats_on || !stats_off) { return CALIBRATION_FAIL; }

    if (stats_on->min <= stats_off->max) {
        log_w("On and off ranges overlap");
        return CALIBRATION_INVALID_REFERENCE_STATS;
    }

    if ((stats_on->min - stats_off->max) < 10) {
        log_w("Insufficient separation between on and off ranges");
        return CALIBRATION_INVALID_REFERENCE_STATS;
    }

    if ((stats_on->mean - stats_off->mean) < 20) {
        log_w("Insufficient separation between on and off mean values");
        return CALIBRATION_INVALID_REFERENCE_STATS;
    }

    return CALIBRATION_OK;
}

calibration_result_t calibration_build_timing_profile(enlarger_timing_t *timing_profile, const reading_stats_t *stats_on, const reading_stats_t *stats_off)
{
    if (!timing_profile || !stats_on || !stats_off) {
        return CALIBRATION_FAIL;
    }

    HAL_StatusTypeDef ret = HAL_OK;
    uint16_t rising_threshold = stats_off->max;
    if (rising_threshold < 2) {
        rising_threshold = 2;
    }

    uint16_t falling_threshold = roundf(stats_off->mean + stats_off->stddev);
    if (falling_threshold < 2) {
        falling_threshold = 2;
    }

    uint16_t channel_data = 0;

    log_i("Collecting profile data...");

    TaskHandle_t current_task_handle = xTaskGetCurrentTaskHandle();
    UBaseType_t current_task_priority = uxTaskPriorityGet(current_task_handle);

    TickType_t time_mark = 0;
    uint32_t integrated_rise = 0;
    uint32_t rise_counts = 0;
    uint32_t integrated_fall = 0;
    uint32_t fall_counts = 0;

    vTaskPrioritySet(current_task_handle, osPriorityRealtime);
    TickType_t time_relay_on = xTaskGetTickCount();
    time_mark = time_relay_on;
    relay_enlarger_enable(true);
    do {
        ret = tcs3472_get_clear_channel_data(&hi2c2, &channel_data);
        if (ret != HAL_OK) {
            log_e("Error getting TCS3472 channel data: %d", ret);
            vTaskPrioritySet(current_task_handle, current_task_priority);
            return CALIBRATION_SENSOR_ERROR;
        }
        if (channel_data > rising_threshold) { break; }

        time_mark += pdMS_TO_TICKS(5);
        osDelayUntil(time_mark);
        if (time_mark - time_relay_on > MAX_LOOP_DURATION) { return false; }
    } while (1);

    TickType_t time_rise_start = xTaskGetTickCount();
    time_mark = time_rise_start;
    do {
        ret = tcs3472_get_clear_channel_data(&hi2c2, &channel_data);
        if (ret != HAL_OK) {
            log_e("Error getting TCS3472 channel data: %d", ret);
            vTaskPrioritySet(current_task_handle, current_task_priority);
            return CALIBRATION_SENSOR_ERROR;
        }
        integrated_rise += channel_data;
        rise_counts++;
        if (channel_data >= (uint16_t)roundf(stats_on->mean - stats_on->stddev)) { break; }

        time_mark += pdMS_TO_TICKS(5);
        osDelayUntil(time_mark);
        if (time_mark - time_rise_start > MAX_LOOP_DURATION) { return false; }
    } while (1);

    TickType_t time_rise_end = xTaskGetTickCount();

    vTaskPrioritySet(current_task_handle, current_task_priority);

    if (!delay_with_cancel(5000)) {
        return CALIBRATION_CANCEL;
    }

    vTaskPrioritySet(current_task_handle, osPriorityRealtime);

    TickType_t time_relay_off = xTaskGetTickCount();
    time_mark = time_relay_off;
    relay_enlarger_enable(false);
    do {
        ret = tcs3472_get_clear_channel_data(&hi2c2, &channel_data);
        if (ret != HAL_OK) {
            log_e("Error getting TCS3472 channel data: %d", ret);
            vTaskPrioritySet(current_task_handle, current_task_priority);
            return CALIBRATION_SENSOR_ERROR;
        }
        if (channel_data < stats_on->min) { break; }

        time_mark += pdMS_TO_TICKS(5);
        osDelayUntil(time_mark);
        if (time_mark - time_relay_off > MAX_LOOP_DURATION) { return false; }
    } while (1);

    TickType_t time_fall_start = xTaskGetTickCount();
    time_mark = time_fall_start;
    do {
        ret = tcs3472_get_clear_channel_data(&hi2c2, &channel_data);
        if (ret != HAL_OK) {
            log_e("Error getting TCS3472 channel data: %d", ret);
            vTaskPrioritySet(current_task_handle, current_task_priority);
            return CALIBRATION_SENSOR_ERROR;
        }
        integrated_fall += channel_data;
        fall_counts++;
        if (channel_data < falling_threshold) { break; }

        time_mark += pdMS_TO_TICKS(5);
        osDelayUntil(time_mark);
        if (time_mark - time_fall_start > MAX_LOOP_DURATION) { return false; }
    } while (1);

    TickType_t time_fall_end = xTaskGetTickCount();

    vTaskPrioritySet(current_task_handle, current_task_priority);

    if (!delay_with_cancel(5000)) {
        return CALIBRATION_CANCEL;
    }

    memset(timing_profile, 0, sizeof(enlarger_timing_t));
    timing_profile->turn_on_delay = (time_rise_start - time_relay_on) / portTICK_PERIOD_MS;
    timing_profile->rise_time = (time_rise_end - time_rise_start) / portTICK_PERIOD_MS;

    float rise_scale_factor = (float)integrated_rise / (stats_on->mean * rise_counts);
    timing_profile->rise_time_equiv = roundf(timing_profile->rise_time * rise_scale_factor);

    timing_profile->turn_off_delay = (time_fall_start - time_relay_off) / portTICK_PERIOD_MS;
    timing_profile->fall_time = (time_fall_end - time_fall_start) / portTICK_PERIOD_MS;

    float fall_scale_factor = (float)integrated_fall / (stats_on->mean * fall_counts);
    timing_profile->fall_time_equiv = roundf(timing_profile->fall_time * fall_scale_factor);

    log_i("Relay on delay: %ldms", timing_profile->turn_on_delay);
    log_i("Rise time: %ldms (full_equiv=%ldms)", timing_profile->rise_time, timing_profile->rise_time_equiv);
    log_i("Relay off delay: %ldms", timing_profile->turn_off_delay);
    log_i("Fall time: %ldms (full_equiv=%ldms)", timing_profile->fall_time, timing_profile->fall_time_equiv);

    return CALIBRATION_OK;
}

void calculate_reading_stats(reading_stats_t *stats, uint16_t *readings, size_t len)
{
    if (!stats || !readings) { return; }

    uint16_t reading_min = UINT16_MAX;
    uint16_t reading_max = 0;
    float reading_sum = 0;

    for (int i = 0; i < len; i++) {
        reading_sum += readings[i];
        if (readings[i] > reading_max) {
            reading_max = readings[i];
        }
        if (readings[i] < reading_min) {
            reading_min = readings[i];
        }
    }
    float reading_mean = reading_sum / len;

    float mean_dist_sum = 0;
    for (int i = 0; i < len; i++) {
        mean_dist_sum += powf(fabsf(readings[i] - reading_mean), 2);
    }

    float reading_stddev = sqrtf((mean_dist_sum / len));

    stats->mean = reading_mean;
    stats->min = reading_min;
    stats->max = reading_max;
    stats->stddev = reading_stddev;
}

bool delay_with_cancel(uint32_t time_ms)
{
    keypad_event_t keypad_event;
    TickType_t tick_start = xTaskGetTickCount();
    TickType_t tick_time = pdMS_TO_TICKS(time_ms);

    do {
        if (keypad_wait_for_event(&keypad_event, 10) == HAL_OK) {
            if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                log_w("Canceling enlarger calibration");
                return false;
            }
        }
    } while ((xTaskGetTickCount() - tick_start) < tick_time);

    return true;
}
