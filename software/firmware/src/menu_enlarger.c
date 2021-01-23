#include "menu_enlarger.h"

#include <FreeRTOS.h>
#include <task.h>
#include <cmsis_os.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include <esp_log.h>

#include "display.h"
#include "keypad.h"
#include "relay.h"
#include "tcs3472.h"
#include "settings.h"
#include "enlarger_profile.h"
#include "illum_controller.h"
#include "util.h"

#define MIN(a,b) (((a)<(b))?(a):(b))

#define REFERENCE_READING_ITERATIONS 100
#define PROFILE_ITERATIONS 5
#define MAX_LOOP_DURATION pdMS_TO_TICKS(10000)

static const char *TAG = "menu_enlarger";

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

static menu_result_t menu_enlarger_profile_edit(enlarger_profile_t *profile, uint8_t index);
static void menu_enlarger_delete_profile(uint8_t index, size_t profile_count);
static bool menu_enlarger_profile_delete_prompt(const enlarger_profile_t *profile, uint8_t index);

static menu_result_t menu_enlarger_calibration(enlarger_profile_t *result_profile);
static calibration_result_t enlarger_calibration_start(enlarger_profile_t *result_profile);
static void enlarger_calibration_preview_result(const enlarger_profile_t *profile);
static void enlarger_calibration_show_error(calibration_result_t result_error);
static calibration_result_t calibration_initialize_sensor();
static calibration_result_t calibration_collect_reference_stats(reading_stats_t *stats_on, reading_stats_t *stats_off, reading_stats_t *stats_color);
static calibration_result_t calibration_validate_reference_stats(reading_stats_t *stats_on, reading_stats_t *stats_off);
static calibration_result_t calibration_build_profile(enlarger_profile_t *profile, const reading_stats_t *stats_on, const reading_stats_t *stats_off);

static void calculate_reading_stats(reading_stats_t *stats, uint16_t *readings, size_t len);
static bool delay_with_cancel(uint32_t time_ms);

menu_result_t menu_enlarger_profiles(state_controller_t *controller)
{
    menu_result_t menu_result = MENU_OK;

    enlarger_profile_t *profile_list;
    profile_list = malloc(sizeof(enlarger_profile_t) * MAX_ENLARGER_PROFILES);
    if (!profile_list) {
        ESP_LOGE(TAG, "Unable to allocate memory for profile list");
        return MENU_OK;
    }

    char buf[640];
    size_t offset;
    bool reload_profiles = true;
    uint8_t profile_default_index = 0;
    size_t profile_count = 0;
    uint8_t option = 1;
    do {
        offset = 0;
        if (reload_profiles) {
            profile_count = 0;
            profile_default_index = settings_get_default_enlarger_profile_index();
            for (size_t i = 0; i < MAX_ENLARGER_PROFILES; i++) {
                if (!settings_get_enlarger_profile(&profile_list[i], i)) {
                    break;
                } else {
                    profile_count = i + 1;
                }
            }
            ESP_LOGI(TAG, "Loaded %d profiles, default is %d", profile_count, profile_default_index);
            reload_profiles = false;
        }

        for (size_t i = 0; i < profile_count; i++) {
            if (profile_list[i].name && strlen(profile_list[i].name) > 0) {
                sprintf(buf + offset, "%c%02d%c %s",
                    (i == profile_default_index) ? '<' : '[',
                    i + 1,
                    (i == profile_default_index) ? '>' : ']',
                    profile_list[i].name);
            } else {
                sprintf(buf + offset, "%c%02d%c Enlarger profile %d",
                    (i == profile_default_index) ? '<' : '[',
                    i + 1,
                    (i == profile_default_index) ? '>' : ']',
                    i + 1);
            }
            offset += pad_str_to_length(buf + offset, ' ', DISPLAY_MENU_ROW_LENGTH);
            if (i < profile_count) {
                buf[offset++] = '\n';
                buf[offset] = '\0';
            }
        }
        if (profile_count < MAX_ENLARGER_PROFILES) {
            sprintf(buf + offset, "*** Add New Profile ***");
        }

        uint16_t result = display_selection_list_params("Enlarger Profiles", option, buf,
            DISPLAY_MENU_ACCEPT_MENU | DISPLAY_MENU_ACCEPT_ADD_ADJUSTMENT);
        option = (uint8_t)(result & 0x00FF);
        keypad_key_t option_key = (uint8_t)((result & 0xFF00) >> 8);

        if (option == 0) {
            menu_result = MENU_CANCEL;
            break;
        } else if (option - 1 == profile_count) {
            uint8_t add_option = display_selection_list("Add New Profile", 1,
                "*** Run Calibration ***\n"
                "*** Add Profile Manually ***");

            if (add_option == 1) {
                enlarger_profile_t working_profile;
                memset(&working_profile, 0, sizeof(enlarger_profile_t));
                ESP_LOGI(TAG, "Add new profile at index: %d", profile_count);
                if (menu_enlarger_calibration(&working_profile) == MENU_OK
                    && enlarger_profile_is_valid(&working_profile)) {
                    if (settings_set_enlarger_profile(&working_profile, profile_count)) {
                        ESP_LOGI(TAG, "New profile added at index: %d", profile_count);
                        memcpy(&profile_list[profile_count], &working_profile, sizeof(enlarger_profile_t));
                        profile_count++;
                    }
                }
            } else if (add_option == 2) {
                enlarger_profile_t working_profile;
                enlarger_profile_set_defaults(&working_profile);
                working_profile.name[0] = '\0';
                menu_result = menu_enlarger_profile_edit(&working_profile, UINT8_MAX);
                if (menu_result == MENU_SAVE) {
                    menu_result = MENU_OK;
                    if (settings_set_enlarger_profile(&working_profile, profile_count)) {
                        ESP_LOGI(TAG, "New profile manually added at index: %d", profile_count);
                        memcpy(&profile_list[profile_count], &working_profile, sizeof(enlarger_profile_t));
                        profile_count++;
                    }
                }
            }
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        } else {
            if (option_key == KEYPAD_MENU) {
                enlarger_profile_t working_profile;
                memcpy(&working_profile, &profile_list[option - 1], sizeof(enlarger_profile_t));
                menu_result = menu_enlarger_profile_edit(&working_profile, option - 1);
                if (menu_result == MENU_SAVE) {
                    menu_result = MENU_OK;
                    if (settings_set_enlarger_profile(&working_profile, option - 1)) {
                        ESP_LOGI(TAG, "Profile saved at index: %d", option - 1);
                        memcpy(&profile_list[option - 1], &working_profile, sizeof(enlarger_profile_t));
                    }
                } else if (menu_result == MENU_DELETE) {
                    menu_result = MENU_OK;
                    menu_enlarger_delete_profile(option - 1, profile_count);
                    reload_profiles = true;
                }
            } else if (option_key == KEYPAD_ADD_ADJUSTMENT) {
                ESP_LOGI(TAG, "Set default profile at index: %d", option - 1);
                settings_set_default_enlarger_profile_index(option - 1);
                profile_default_index = option - 1;
            }
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    free(profile_list);

    // There are many paths in this menu that can change profile settings,
    // so its easiest to just reload the state controller's active profile
    // whenever exiting from this menu.
    state_controller_reload_enlarger_profile(controller);

    return menu_result;
}

void menu_enlarger_delete_profile(uint8_t index, size_t profile_count)
{
    for (size_t i = index; i < MAX_ENLARGER_PROFILES; i++) {
        if (i < profile_count - 1) {
            enlarger_profile_t profile;
            if (!settings_get_enlarger_profile(&profile, i + 1)) {
                return;
            }
            if (!settings_set_enlarger_profile(&profile, i)) {
                return;
            }
        } else {
            settings_clear_enlarger_profile(i);
            break;
        }
    }

    uint8_t default_profile_index = settings_get_default_enlarger_profile_index();
    if (default_profile_index == index) {
        settings_set_default_enlarger_profile_index(0);
    } else if (default_profile_index > index) {
        settings_set_default_enlarger_profile_index(default_profile_index - 1);
    }
}

menu_result_t menu_enlarger_profile_edit(enlarger_profile_t *profile, uint8_t index)
{
    char buf_title[32];
    char buf[512];
    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;

    if (index == UINT8_MAX) {
        sprintf(buf_title, "New Enlarger Profile");
    } else {
        sprintf(buf_title, "Enlarger Profile %d", index + 1);
    }

    do {
        size_t offset = 0;

        size_t name_len = MIN(strlen(profile->name), 25);

        strcpy(buf, "Name ");
        offset = pad_str_to_length(buf, ' ', DISPLAY_MENU_ROW_LENGTH - (name_len + 2));
        buf[offset++] = '[';
        strncpy(buf + offset, profile->name, name_len + 1);
        offset += name_len;
        buf[offset++] = ']';
        buf[offset++] = '\n';
        buf[offset] = '\0';

        sprintf(buf + offset,
            "Turn on delay          [%5lums]\n"
            "Rise time              [%5lums]\n"
            "Rise time equivalent   [%5lums]\n"
            "Turn off delay         [%5lums]\n"
            "Fall time              [%5lums]\n"
            "Fall time equivalent   [%5lums]\n"
            "Color temperature      [ %5luK]\n"
            "*** Run Calibration ***\n"
            "*** Save Changes ***",
            profile->turn_on_delay,
            profile->rise_time,
            profile->rise_time_equiv,
            profile->turn_off_delay,
            profile->fall_time,
            profile->fall_time_equiv,
            profile->color_temperature);

        if (index != UINT8_MAX) {
            strcat(buf, "\n*** Delete Profile ***");
        }

        option = display_selection_list(buf_title, option, buf);

        if (option == 1) {
            display_input_text("Enlarger Name", profile->name, 32);
        } else if (option == 2) {
            uint16_t value_sel = profile->turn_on_delay;
            if (display_input_value_u16(
                "-- Turn On Delay --\n"
                "Time period from when the\n"
                "enlarger is activated until its\n"
                "light level starts to rise.\n",
                "", &value_sel, 0, UINT16_MAX, 5, " ms") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                profile->turn_on_delay = value_sel;
            }
        } else if (option == 3) {
            uint16_t value_sel = profile->rise_time;
            if (display_input_value_u16(
                "-- Rise Time --\n"
                "Time period from when the\n"
                "enlarger light level starts to\n"
                "rise until it peaks.\n",
                "", &value_sel, 0, UINT16_MAX, 5, " ms") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                profile->rise_time = value_sel;
            }
        } else if (option == 4) {
            uint16_t value_sel = profile->rise_time_equiv;
            if (display_input_value_u16(
                "-- Rise Time Equivalent --\n"
                "Time period at peak output\n"
                "for an exposure equivalent to\n"
                "the rise time period.\n",
                "", &value_sel, 0, UINT16_MAX, 5, " ms") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                profile->rise_time_equiv = value_sel;
            }
        } else if (option == 5) {
            uint16_t value_sel = profile->turn_off_delay;
            if (display_input_value_u16(
                "-- Turn Off Delay --\n"
                "Time period from when the\n"
                "enlarger is deactivated until\n"
                "its light level starts to fall.\n",
                "", &value_sel, 0, UINT16_MAX, 5, " ms") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                profile->turn_off_delay = value_sel;
            }
        } else if (option == 6) {
            uint16_t value_sel = profile->fall_time;
            if (display_input_value_u16(
                "-- Fall Time --\n"
                "Time period from when the\n"
                "enlarger light level starts to\n"
                "fall until it is completely off.\n",
                "", &value_sel, 0, UINT16_MAX, 5, " ms") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                profile->fall_time = value_sel;
            }
        } else if (option == 7) {
            uint16_t value_sel = profile->fall_time_equiv;
            if (display_input_value_u16(
                "-- Fall Time Equivalent --\n"
                "Time period at peak output\n"
                "for an exposure equivalent to\n"
                "the fall time period.\n",
                "", &value_sel, 0, UINT16_MAX, 5, " ms") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                profile->fall_time_equiv = value_sel;
            }
        } else if (option == 8) {
            uint16_t value_sel = profile->color_temperature;
            if (display_input_value_u16(
                "-- Color Temperature --\n"
                "The unfiltered color temperature\n"
                "of the enlarger lamp.\n",
                "", &value_sel, 0, UINT16_MAX, 5, " K") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                profile->color_temperature = value_sel;
            }
        } else if (option == 9) {
            ESP_LOGD(TAG, "Run calibration from profile editor");
            uint8_t sub_option = display_message(
                "Run Calibration?\n",
                NULL,
                "This will replace all the\n"
                "profile values with results from\n"
                "the calibration process.",
                " Start \n Cancel ");

            if (sub_option == 1) {
                if (menu_enlarger_calibration(profile) == MENU_TIMEOUT) {
                    menu_result = MENU_TIMEOUT;
                    break;
                }
            } else if (sub_option == 2) {
                // Cancel selected
                continue;
            } else if (sub_option == UINT8_MAX) {
                // Return assuming timeout
                menu_result = MENU_TIMEOUT;
                break;
            } else {
                // Return assuming no timeout
                continue;
            }
        } else if (option == 10) {
            ESP_LOGD(TAG, "Save changes from profile editor");
            menu_result = MENU_SAVE;
            break;
        } else if (option == 11) {
            ESP_LOGD(TAG, "Delete profile from profile editor");
            if (menu_enlarger_profile_delete_prompt(profile, index)) {
                menu_result = MENU_DELETE;
                break;
            }
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }

    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}

bool menu_enlarger_profile_delete_prompt(const enlarger_profile_t *profile, uint8_t index)
{
    char buf_title[32];
    char buf[256];

    sprintf(buf_title, "Delete Profile %d?", index + 1);

    if (profile->name && strlen(profile->name) > 0) {
        sprintf(buf, "\n%s\n", profile->name);
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

menu_result_t menu_enlarger_calibration(enlarger_profile_t *result_profile)
{
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
        calibration_result_t calibration_result = enlarger_calibration_start(result_profile);
        if (calibration_result == CALIBRATION_OK) {
            enlarger_calibration_preview_result(result_profile);
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

calibration_result_t enlarger_calibration_start(enlarger_profile_t *result_profile)
{
    calibration_result_t calibration_result;
    ESP_LOGI(TAG, "Starting enlarger calibration process");

    display_static_list("Enlarger Calibration", "\n\nInitializing...");

    // Turn everything off, just in case it isn't already off
    relay_safelight_enable(false);
    relay_enlarger_enable(false);

    calibration_result = calibration_initialize_sensor();
    if (calibration_result != CALIBRATION_OK) {
        ESP_LOGE(TAG, "Could not initialize sensor");
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
        ESP_LOGE(TAG, "Could not collect reference stats");
        tcs3472_disable(&hi2c2);
        if (relay_enlarger_is_enabled()) {
            relay_enlarger_enable(false);
        }
        return calibration_result;
    }

    // Log statistics used for reference points
    ESP_LOGI(TAG, "Enlarger on");
    ESP_LOGI(TAG, "Mean = %.1f, Min = %d, Max = %d, StdDev = %.1f",
        enlarger_on_stats.mean, enlarger_on_stats.min, enlarger_on_stats.max,
        enlarger_on_stats.stddev);
    ESP_LOGI(TAG, "Enlarger off");
    ESP_LOGI(TAG, "Mean = %.1f, Min = %d, Max = %d, StdDev = %.1f",
        enlarger_off_stats.mean, enlarger_off_stats.min, enlarger_off_stats.max,
        enlarger_off_stats.stddev);
    ESP_LOGI(TAG, "Enlarger color temperature");
    ESP_LOGI(TAG, "Mean = %.1f, Min = %d, Max = %d, StdDev = %.1f",
        enlarger_color_stats.mean, enlarger_color_stats.min, enlarger_color_stats.max,
        enlarger_color_stats.stddev);

    calibration_result = calibration_validate_reference_stats(&enlarger_on_stats, &enlarger_off_stats);
    if (calibration_result != CALIBRATION_OK) {
        ESP_LOGW(TAG, "Reference stats are not usable for calibration");
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

    enlarger_profile_t profile;
    enlarger_profile_t profile_sum;
    enlarger_profile_t profile_inc[PROFILE_ITERATIONS];

    memset(&profile, 0, sizeof(enlarger_profile_t));
    memset(&profile_sum, 0, sizeof(enlarger_profile_t));

    for (int i = 0; i < PROFILE_ITERATIONS; i++) {
        ESP_LOGI(TAG, "Profile run %d...", i + 1);

        calibration_result = calibration_build_profile(&profile_inc[i], &enlarger_on_stats, &enlarger_off_stats);
        if (calibration_result != CALIBRATION_OK) {
            ESP_LOGE(TAG, "Could not build profile");
            tcs3472_disable(&hi2c2);
            illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

            if (relay_enlarger_is_enabled()) {
                relay_enlarger_enable(false);
            }

            return calibration_result;
        }

        profile_sum.turn_on_delay += profile_inc[i].turn_on_delay;
        profile_sum.rise_time += profile_inc[i].rise_time;
        profile_sum.rise_time_equiv += profile_inc[i].rise_time_equiv;
        profile_sum.turn_off_delay += profile_inc[i].turn_off_delay;
        profile_sum.fall_time += profile_inc[i].fall_time;
        profile_sum.fall_time_equiv += profile_inc[i].fall_time_equiv;
    }
    ESP_LOGI(TAG, "Profile runs complete");

    // Average across all the runs
    profile.turn_on_delay = roundf((float)profile_sum.turn_on_delay / PROFILE_ITERATIONS);
    profile.rise_time = roundf((float)profile_sum.rise_time / PROFILE_ITERATIONS);
    profile.rise_time_equiv = roundf((float)profile_sum.rise_time_equiv / PROFILE_ITERATIONS);
    profile.turn_off_delay = roundf((float)profile_sum.turn_off_delay / PROFILE_ITERATIONS);
    profile.fall_time = roundf((float)profile_sum.fall_time / PROFILE_ITERATIONS);
    profile.fall_time_equiv = roundf((float)profile_sum.fall_time_equiv / PROFILE_ITERATIONS);
    profile.color_temperature = roundf(enlarger_color_stats.mean);

    tcs3472_disable(&hi2c2);
    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

    ESP_LOGI(TAG, "Relay on delay: %ldms", profile.turn_on_delay);
    ESP_LOGI(TAG, "Rise time: %ldms (full_equiv=%ldms)", profile.rise_time, profile.rise_time_equiv);
    ESP_LOGI(TAG, "Relay off delay: %ldms", profile.turn_off_delay);
    ESP_LOGI(TAG, "Fall time: %ldms (full_equiv=%ldms)", profile.fall_time, profile.fall_time_equiv);
    ESP_LOGI(TAG, "Color temperature: %ldK", profile.color_temperature);

    if (result_profile) {
        result_profile->turn_on_delay = profile.turn_on_delay;
        result_profile->rise_time = profile.rise_time;
        result_profile->rise_time_equiv = profile.rise_time_equiv;
        result_profile->turn_off_delay = profile.turn_off_delay;
        result_profile->fall_time = profile.fall_time;
        result_profile->fall_time_equiv = profile.fall_time_equiv;
        result_profile->color_temperature = profile.color_temperature;
    }

    return CALIBRATION_OK;
}

void enlarger_calibration_preview_result(const enlarger_profile_t *profile)
{
    char buf[256];
    sprintf(buf, "\n"
        "Enlarger on: %ldms\n"
        "Rise time: %ldms (%ldms)\n"
        "Enlarger off: %ldms\n"
        "Fall time: %ldms (%ldms)",
        profile->turn_on_delay,
        profile->rise_time, profile->rise_time_equiv,
        profile->turn_off_delay,
        profile->fall_time, profile->fall_time_equiv);
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
        ESP_LOGI(TAG, "Initializing sensor");
        ret = tcs3472_init(&hi2c2);
        if (ret != HAL_OK) {
            ESP_LOGE(TAG, "Error initializing TCS3472: %d", ret);
            break;
        }

        ret = tcs3472_enable(&hi2c2);
        if (ret != HAL_OK) {
            ESP_LOGE(TAG, "Error enabling TCS3472: %d", ret);
            break;
        }

        ret = tcs3472_set_gain(&hi2c2, TCS3472_AGAIN_60X);
        if (ret != HAL_OK) {
            ESP_LOGE(TAG, "Error setting TCS3472 gain: %d", ret);
            break;
        }

        ret = tcs3472_set_time(&hi2c2, TCS3472_ATIME_4_8MS);
        if (ret != HAL_OK) {
            ESP_LOGE(TAG, "Error setting TCS3472 time: %d", ret);
            break;
        }

        bool valid = false;
        do {
            ret = tcs3472_get_status_valid(&hi2c2, &valid);
            if (ret != HAL_OK) {
                ESP_LOGE(TAG, "Error getting TCS3472 status: %d", ret);
                break;
            }
        } while (!valid);

        ESP_LOGI(TAG, "Sensor initialized");
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

    ESP_LOGI(TAG, "Turning enlarger on for baseline reading");
    relay_enlarger_enable(true);

    ESP_LOGI(TAG, "Waiting for light to stabilize");
    if (!delay_with_cancel(5000)) {
        return CALIBRATION_CANCEL;
    }

    ESP_LOGI(TAG, "Finding appropriate gain setting");
    bool gain_selected = false;
    for (tcs3472_again_t gain = TCS3472_AGAIN_60X; gain >= TCS3472_AGAIN_1X; --gain) {
        tcs3472_channel_data_t channel_data;

        ret = tcs3472_set_gain(&hi2c2, gain);
        if (ret != HAL_OK) {
            ESP_LOGE(TAG, "Error setting TCS3472 gain: %d (gain=%d)", ret, gain);
            return CALIBRATION_SENSOR_ERROR;
        }

        osDelay(pdMS_TO_TICKS(20));

        ret = tcs3472_get_full_channel_data(&hi2c2, &channel_data);
        if (ret != HAL_OK) {
            ESP_LOGE(TAG, "Error getting TCS3472 channel data: %d", ret);
            return CALIBRATION_SENSOR_ERROR;
        }

        if (channel_data.clear == 0) {
            ESP_LOGW(TAG, "No reading on clear channel");
            return CALIBRATION_ZERO_READING;
        }
        if (tcs3472_calculate_color_temp(&channel_data) > 0) {
            ESP_LOGI(TAG, "Selected gain: %s", tcs3472_gain_str(gain));
            gain_selected = true;
            break;
        }
    }
    if (!gain_selected) {
        ESP_LOGW(TAG, "Could not find a gain setting with a valid unsaturated reading");
        return CALIBRATION_SENSOR_SATURATED;
    }

    ESP_LOGI(TAG, "Collecting data with enlarger on");
    for (int i = 0; i < REFERENCE_READING_ITERATIONS; i++) {
        tcs3472_channel_data_t channel_data;
        ret = tcs3472_get_full_channel_data(&hi2c2, &channel_data);
        if (ret != HAL_OK) {
            ESP_LOGE(TAG, "Error getting TCS3472 channel data: %d", ret);
            return CALIBRATION_SENSOR_ERROR;
        }
        readings[i] = channel_data.clear;
        color_readings[i] = tcs3472_calculate_color_temp(&channel_data);
        osDelay(pdMS_TO_TICKS(5));
    }

    relay_enlarger_enable(false);

    ESP_LOGI(TAG, "Computing baseline enlarger reading statistics");
    calculate_reading_stats(stats_on, readings, REFERENCE_READING_ITERATIONS);
    if (stats_color) {
        calculate_reading_stats(stats_color, color_readings, REFERENCE_READING_ITERATIONS);
    }

    ESP_LOGI(TAG, "Waiting for light to stabilize");
    if (!delay_with_cancel(2000)) {
        return false;
    }

    ESP_LOGI(TAG, "Collecting data with enlarger off");
    for (int i = 0; i < REFERENCE_READING_ITERATIONS; i++) {
        uint16_t channel_data;
        ret = tcs3472_get_clear_channel_data(&hi2c2, &channel_data);
        if (ret != HAL_OK) {
            ESP_LOGE(TAG, "Error getting TCS3472 channel data: %d", ret);
            return false;
        }
        readings[i] = channel_data;
        osDelay(pdMS_TO_TICKS(5));
    }

    ESP_LOGI(TAG, "Computing baseline darkness reading statistics");
    calculate_reading_stats(stats_off, readings, REFERENCE_READING_ITERATIONS);

    return CALIBRATION_OK;
}

calibration_result_t calibration_validate_reference_stats(reading_stats_t *stats_on, reading_stats_t *stats_off)
{
    if (!stats_on || !stats_off) { return CALIBRATION_FAIL; }

    if (stats_on->min <= stats_off->max) {
        ESP_LOGW(TAG, "On and off ranges overlap");
        return CALIBRATION_INVALID_REFERENCE_STATS;
    }

    if ((stats_on->min - stats_off->max) < 10) {
        ESP_LOGW(TAG, "Insufficient separation between on and off ranges");
        return CALIBRATION_INVALID_REFERENCE_STATS;
    }

    if ((stats_on->mean - stats_off->mean) < 20) {
        ESP_LOGW(TAG, "Insufficient separation between on and off mean values");
        return CALIBRATION_INVALID_REFERENCE_STATS;
    }

    return CALIBRATION_OK;
}

calibration_result_t calibration_build_profile(enlarger_profile_t *profile, const reading_stats_t *stats_on, const reading_stats_t *stats_off)
{
    if (!profile || !stats_on || !stats_off) {
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

    ESP_LOGI(TAG, "Collecting profile data...");

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
            ESP_LOGE(TAG, "Error getting TCS3472 channel data: %d", ret);
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
            ESP_LOGE(TAG, "Error getting TCS3472 channel data: %d", ret);
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
            ESP_LOGE(TAG, "Error getting TCS3472 channel data: %d", ret);
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
            ESP_LOGE(TAG, "Error getting TCS3472 channel data: %d", ret);
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

    memset(profile, 0, sizeof(enlarger_profile_t));
    profile->turn_on_delay = (time_rise_start - time_relay_on) / portTICK_PERIOD_MS;
    profile->rise_time = (time_rise_end - time_rise_start) / portTICK_PERIOD_MS;

    float rise_scale_factor = (float)integrated_rise / (stats_on->mean * rise_counts);
    profile->rise_time_equiv = roundf(profile->rise_time * rise_scale_factor);


    profile->turn_off_delay = (time_fall_start - time_relay_off) / portTICK_PERIOD_MS;
    profile->fall_time = (time_fall_end - time_fall_start) / portTICK_PERIOD_MS;

    float fall_scale_factor = (float)integrated_fall / (stats_on->mean * fall_counts);
    profile->fall_time_equiv = roundf(profile->fall_time * fall_scale_factor);

    ESP_LOGI(TAG, "Relay on delay: %ldms", profile->turn_on_delay);
    ESP_LOGI(TAG, "Rise time: %ldms (full_equiv=%ldms)", profile->rise_time, profile->rise_time_equiv);
    ESP_LOGI(TAG, "Relay off delay: %ldms", profile->turn_off_delay);
    ESP_LOGI(TAG, "Fall time: %ldms (full_equiv=%ldms)", profile->fall_time, profile->fall_time_equiv);

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
                ESP_LOGW(TAG, "Canceling enlarger calibration");
                return false;
            }
        }
    } while ((xTaskGetTickCount() - tick_start) < tick_time);

    return true;
}
