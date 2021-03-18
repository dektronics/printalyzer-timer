#include "menu_paper.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <arm_math.h>

#include <esp_log.h>

#include "display.h"
#include "keypad.h"
#include "settings.h"
#include "paper_profile.h"
#include "util.h"
#include "usb_host.h"
#include "densitometer.h"
#include "step_wedge.h"
#include "menu_step_wedge.h"
#include "exposure_state.h"

static const char *TAG = "menu_paper";

typedef struct {
    step_wedge_t *wedge;
    float paper_dmin;
    float paper_dmax;
    uint32_t calibration_pev;
    float *patch_density;
} wedge_calibration_params_t;

static menu_result_t menu_paper_profile_edit(state_controller_t *controller, paper_profile_t *profile, uint8_t index);
static void menu_paper_delete_profile(uint8_t index, size_t profile_count);
static bool menu_paper_profile_delete_prompt(const paper_profile_t *profile, uint8_t index);
static menu_result_t menu_paper_profile_edit_grade(state_controller_t *controller, paper_profile_t *profile, uint8_t index, contrast_grade_t grade);
static menu_result_t menu_paper_profile_calibrate_grade(state_controller_t *controller, const char *title, paper_profile_grade_t *paper_grade, float max_net_density);
static uint16_t menu_paper_densitometer_data_callback(void *user_data);
static menu_result_t menu_paper_profile_calibrate_grade_validate(const wedge_calibration_params_t *params);
static menu_result_t menu_paper_profile_calibrate_grade_calculate(const char *title, const wedge_calibration_params_t *params, paper_profile_grade_t *paper_grade);

menu_result_t menu_paper_profiles(state_controller_t *controller)
{
    menu_result_t menu_result = MENU_OK;

    paper_profile_t *profile_list;
    profile_list = pvPortMalloc(sizeof(paper_profile_t) * MAX_PAPER_PROFILES);
    if (!profile_list) {
        ESP_LOGE(TAG, "Unable to allocate memory for profile list");
        return MENU_OK;
    }

    char buf[640];
    size_t offset;
    bool reload_profiles = true;
    bool default_profile_changed = false;
    uint8_t profile_default_index = 0;
    size_t profile_count = 0;
    uint8_t option = 1;
    do {
        offset = 0;
        if (reload_profiles) {
            profile_count = 0;
            profile_default_index = settings_get_default_paper_profile_index();
            for (size_t i = 0; i < MAX_PAPER_PROFILES; i++) {
                if (!settings_get_paper_profile(&profile_list[i], i)) {
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
                sprintf(buf + offset, "%c%02d%c Paper profile %d",
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
        if (profile_count < MAX_PAPER_PROFILES) {
            sprintf(buf + offset, "*** Add New Profile ***");
        }

        uint16_t result = display_selection_list_params("Paper Profiles", option, buf,
            DISPLAY_MENU_ACCEPT_MENU | DISPLAY_MENU_ACCEPT_ADD_ADJUSTMENT);
        option = (uint8_t)(result & 0x00FF);
        keypad_key_t option_key = (uint8_t)((result & 0xFF00) >> 8);

        if (option == 0) {
            menu_result = MENU_CANCEL;
            break;
        } else if (option - 1 == profile_count) {
            paper_profile_t working_profile;
            memset(&working_profile, 0, sizeof(paper_profile_t));

            uint8_t add_option = display_selection_list("Add New Profile", 1,
                "*** Add Profile Manually ***\n"
                "*** Add Default Profile ***");

            bool show_editor = false;

            if (add_option == 1) {
                show_editor = true;
            } else if (add_option == 2) {
                paper_profile_set_defaults(&working_profile);
                working_profile.name[0] = '\0';
                show_editor = true;
            }

            if (show_editor) {
                menu_result = menu_paper_profile_edit(controller, &working_profile, UINT8_MAX);
                if (menu_result == MENU_SAVE) {
                    menu_result = MENU_OK;
                    if (settings_set_paper_profile(&working_profile, profile_count)) {
                        ESP_LOGI(TAG, "New profile manually added at index: %d", profile_count);
                        memcpy(&profile_list[profile_count], &working_profile, sizeof(paper_profile_t));
                        profile_count++;
                    }
                }
            }
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        } else {
            if (option_key == KEYPAD_MENU) {
                paper_profile_t working_profile;
                memcpy(&working_profile, &profile_list[option - 1], sizeof(paper_profile_t));
                menu_result = menu_paper_profile_edit(controller, &working_profile, option - 1);
                if (menu_result == MENU_SAVE) {
                    menu_result = MENU_OK;
                    if (settings_set_paper_profile(&working_profile, option - 1)) {
                        ESP_LOGI(TAG, "Profile saved at index: %d", option - 1);
                        memcpy(&profile_list[option - 1], &working_profile, sizeof(paper_profile_t));
                    }
                } else if (menu_result == MENU_DELETE) {
                    menu_result = MENU_OK;
                    menu_paper_delete_profile(option - 1, profile_count);
                    reload_profiles = true;
                }
            } else if (option_key == KEYPAD_ADD_ADJUSTMENT) {
                ESP_LOGI(TAG, "Set default profile at index: %d", option - 1);
                settings_set_default_paper_profile_index(option - 1);
                profile_default_index = option - 1;
                default_profile_changed = true;
            }
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    vPortFree(profile_list);

    // There are many paths in this menu that can change profile settings,
    // so its easiest to just reload the state controller's active profile
    // whenever exiting from this menu.
    state_controller_reload_paper_profile(controller, default_profile_changed);

    return menu_result;
}

size_t menu_paper_profile_edit_append_grade(char *str, const paper_profile_t *profile, contrast_grade_t grade)
{
    size_t offset;
    bool valid = paper_profile_grade_is_valid(&profile->grade[grade]);
    uint32_t iso_r = profile->grade[grade].hs_lev100 - profile->grade[grade].ht_lev100;
    if (!valid) {
        offset = sprintf(str,
            "Grade %-2s         [---][---][---]\n",
            contrast_grade_str(grade));
    } else if (profile->grade[grade].hm_lev100 == 0) {
        offset = sprintf(str,
            "Grade %-2s         [%3lu][---][%3lu]\n",
            contrast_grade_str(grade),
            profile->grade[grade].ht_lev100,
            iso_r);
    } else {
        offset = sprintf(str,
            "Grade %-2s         [%3lu][%3lu][%3lu]\n",
            contrast_grade_str(grade),
            profile->grade[grade].ht_lev100,
            profile->grade[grade].hm_lev100,
            iso_r);
    }
    return offset;
}

menu_result_t menu_paper_profile_edit(state_controller_t *controller, paper_profile_t *profile, uint8_t index)
{
    char buf_title[32];
    char buf[512];
    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;
    bool profile_dirty;

    if (index == UINT8_MAX) {
        sprintf(buf_title, "New Paper Profile");
        profile_dirty = true;
    } else {
        sprintf(buf_title, "Paper Profile %d", index + 1);
        profile_dirty = false;
    }

    do {
        size_t offset = 0;

        offset = menu_build_padded_str_row(buf, "Name", profile->name);

        if (isnormal(profile->max_net_density) && profile->max_net_density > 0.0F) {
            offset += sprintf(buf + offset, "Max net density         [D=%01.2f]\n", profile->max_net_density);
        } else {
            offset += sprintf(buf + offset, "Max net density         [------]\n");
        }

        const char *filter_str;
        switch (profile->contrast_filter) {
        case CONTRAST_FILTER_REGULAR:
            filter_str = "Regular";
            break;
        case CONTRAST_FILTER_DURST_170M:
            filter_str = "Durst (170M)";
            break;
        case CONTRAST_FILTER_DURST_130M:
            filter_str = "Durst (130M)";
            break;
        case CONTRAST_FILTER_KODAK:
            filter_str = "Kodak";
            break;
        case CONTRAST_FILTER_LEITZ_FOCOMAT_V35:
            filter_str = "Focomat V35";
            break;
        case CONTRAST_FILTER_MEOPTA:
            filter_str = "Meopta";
            break;
        default:
            filter_str = "";
            break;
        }
        offset += menu_build_padded_str_row(buf + offset, "Contrast filter", filter_str);

        offset += menu_paper_profile_edit_append_grade(buf + offset, profile, CONTRAST_GRADE_00);
        offset += menu_paper_profile_edit_append_grade(buf + offset, profile, CONTRAST_GRADE_0);
        offset += menu_paper_profile_edit_append_grade(buf + offset, profile, CONTRAST_GRADE_1);
        offset += menu_paper_profile_edit_append_grade(buf + offset, profile, CONTRAST_GRADE_2);
        offset += menu_paper_profile_edit_append_grade(buf + offset, profile, CONTRAST_GRADE_3);
        offset += menu_paper_profile_edit_append_grade(buf + offset, profile, CONTRAST_GRADE_4);
        offset += menu_paper_profile_edit_append_grade(buf + offset, profile, CONTRAST_GRADE_5);

        sprintf(buf + offset, "*** Save Changes ***");

        if (index != UINT8_MAX) {
            strcat(buf, "\n*** Delete Profile ***");
        }

        option = display_selection_list(buf_title, option, buf);

        if (option == 1) {
            if (display_input_text("Paper Profile Name", profile->name, 32) > 0) {
                profile_dirty = true;
            }
        } else if (option == 2) {
            uint16_t value_sel = lroundf(profile->max_net_density * 100);
            uint16_t value_sel_prev = value_sel;
            bool dens_enable = usb_serial_is_attached();
            if (dens_enable) {
                usb_serial_clear_receive_buffer();
            }
            if (display_input_value_f16_data_cb(
                "Max Net Density",
                "Maximum density (Dmax) of the\n"
                "paper, measured relative to the\n"
                "paper base (Dmin).\n",
                "D=", &value_sel, 0, 999, 1, 2, "",
                menu_paper_densitometer_data_callback, &dens_enable) == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                if (value_sel != value_sel_prev) {
                    profile->max_net_density = (float)value_sel / 100.0F;
                    profile_dirty = true;
                }
            }
        } else if (option == 3) {
            uint8_t sub_option = display_selection_list(
                "Contrast filter", profile->contrast_filter + 1,
                "Regular\n"
                "Durst (max 170M)\n"
                "Durst (max 130M)\n"
                "Kodak\n"
                "Leitz Focomat V35\n"
                "Meopta");
            if (sub_option == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                if (sub_option - 1 != profile->contrast_filter) {
                    profile->contrast_filter = sub_option - 1;
                    profile_dirty = true;
                }
            }
        } else if (option >= 4 && option <= 10) {
            contrast_grade_t grade;
            switch (option) {
            case 4:
                grade = CONTRAST_GRADE_00;
                break;
            case 5:
                grade = CONTRAST_GRADE_0;
                break;
            case 6:
                grade = CONTRAST_GRADE_1;
                break;
            case 7:
                grade = CONTRAST_GRADE_2;
                break;
            case 8:
                grade = CONTRAST_GRADE_3;
                break;
            case 9:
                grade = CONTRAST_GRADE_4;
                break;
            case 10:
                grade = CONTRAST_GRADE_5;
                break;
            default:
                grade = CONTRAST_GRADE_2;
                break;
            }
            menu_result_t sub_result = menu_paper_profile_edit_grade(controller, profile, index, grade);
            if (sub_result == MENU_SAVE) {
                profile_dirty = true;
            } else if (sub_result == MENU_TIMEOUT) {
                menu_result = MENU_TIMEOUT;
            }
        } else if (option == 11) {
            ESP_LOGD(TAG, "Save changes from profile editor");
            menu_result = MENU_SAVE;
            break;
        } else if (option == 12) {
            ESP_LOGD(TAG, "Delete profile from profile editor");
            if (menu_paper_profile_delete_prompt(profile, index)) {
                menu_result = MENU_DELETE;
                break;
            }
        } else if (option == 0 && profile_dirty) {
            menu_result_t sub_result = menu_confirm_cancel(buf_title);
            if (sub_result == MENU_SAVE) {
                menu_result = MENU_SAVE;
                break;
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

void menu_paper_delete_profile(uint8_t index, size_t profile_count)
{
    for (size_t i = index; i < MAX_PAPER_PROFILES; i++) {
        if (i < profile_count - 1) {
            paper_profile_t profile;
            if (!settings_get_paper_profile(&profile, i + 1)) {
                return;
            }
            if (!settings_set_paper_profile(&profile, i)) {
                return;
            }
        } else {
            settings_clear_paper_profile(i);
            break;
        }
    }

    uint8_t default_profile_index = settings_get_default_paper_profile_index();
    if (default_profile_index == index) {
        settings_set_default_paper_profile_index(0);
    } else if (default_profile_index > index) {
        settings_set_default_paper_profile_index(default_profile_index - 1);
    }
}

bool menu_paper_profile_delete_prompt(const paper_profile_t *profile, uint8_t index)
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

menu_result_t menu_paper_profile_edit_grade(state_controller_t *controller, paper_profile_t *profile, uint8_t index, contrast_grade_t grade)
{
    char buf_title[36];
    char buf[512];
    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;

    memset(buf_title, 0, sizeof(buf_title));

    if (strlen(profile->name) > 0) {
        strncpy(buf_title, profile->name, 22);
    } else {
        if (index == UINT8_MAX) {
            sprintf(buf_title, "New Paper Profile");
        } else {
            sprintf(buf_title, "Paper Profile %d", index + 1);
        }
    }

    pad_str_to_length(buf_title, ' ', 31 - (grade == CONTRAST_GRADE_00 ? 8 : 7));
    strcat(buf_title, "Grade ");
    strcat(buf_title, contrast_grade_str(grade));

    paper_profile_grade_t working_grade;
    memcpy(&working_grade, &profile->grade[grade], sizeof(paper_profile_grade_t));

    float working_max_net_density = profile->max_net_density;

    do {
        if (working_grade.hm_lev100 <= working_grade.ht_lev100 || working_grade.hm_lev100 >= working_grade.hs_lev100) {
            working_grade.hm_lev100 = 0;
        }

        uint32_t iso_r = working_grade.hs_lev100 - working_grade.ht_lev100;

        if (working_grade.hm_lev100 == 0) {
            sprintf(buf,
                "Base exposure (Dmin+0.04)  [%3lu]\n"
                "Speed point   (Dmin+0.60)  [---]\n"
                "ISO Range                  [%3lu]\n"
                "*** Measure From Step Wedge ***\n"
                "*** Accept Changes ***",
                working_grade.ht_lev100, iso_r);
        } else {
            sprintf(buf,
                "Base exposure (Dmin+0.04)  [%3lu]\n"
                "Speed point   (Dmin+0.60)  [%3lu]\n"
                "ISO Range                  [%3lu]\n"
                "*** Measure From Step Wedge ***\n"
                "*** Accept Changes ***",
                working_grade.ht_lev100, working_grade.hm_lev100, iso_r);
        }

        option = display_selection_list(buf_title, option, buf);

        if (option == 1) {
            uint16_t value_sel = working_grade.ht_lev100;
            if (display_input_value_u16(
                "Base Exposure",
                "Paper exposure value necessary\n"
                "to achieve a density of D=0.04\n"
                "above the paper base (Dmin).\n",
                "", &value_sel, 0, 999, 3, "") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                working_grade.ht_lev100 = value_sel;
                working_grade.hs_lev100 = working_grade.ht_lev100 + iso_r;
            }
        } else if (option == 2) {
            uint16_t value_sel = working_grade.hm_lev100;
            if (display_input_value_u16(
                "Speed Point (optional)",
                "Paper exposure value necessary\n"
                "to achieve a density of D=0.60\n"
                "above the paper base (Dmin).\n",
                "", &value_sel, 0, 999, 3, "") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                working_grade.hm_lev100 = value_sel;
            }
        } else if (option == 3) {
            uint16_t value_sel = iso_r;
            if (display_input_value_u16(
                "ISO Range",
                "Paper exposure range between the\n"
                "base exposure and an exposure\n"
                "that achieves 90% of Dnet\n",
                "", &value_sel, 1, 999, 3, "") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                working_grade.hs_lev100 = working_grade.ht_lev100 + value_sel;
            }
        } else if (option == 4) {
            if (isnormal(working_max_net_density) && working_max_net_density > 0.0F) {
                menu_result_t cal_result = menu_paper_profile_calibrate_grade(controller, buf_title, &working_grade, working_max_net_density);
                if (cal_result == MENU_SAVE) {
                    /* If calibration results were accepted, move the cursor down for convenience */
                    option = 5;
                } else if (cal_result == MENU_TIMEOUT) {
                    menu_result = MENU_TIMEOUT;
                }
            } else {
                uint8_t msg_option = display_message(
                        "Max Net Density Missing\n",
                        NULL,
                        "Cannot measure step wedge\n"
                        "exposure without a max net\n"
                        "density value.", " OK ");
                if (msg_option == UINT8_MAX) {
                    menu_result = MENU_TIMEOUT;
                }
            }
        } else if (option == 5) {
            ESP_LOGI(TAG, "Accept: Grade %s, ht_lev100=%lu, hm_lev100=%lu, hs_lev100=%lu",
                contrast_grade_str(grade),
                working_grade.ht_lev100, working_grade.hm_lev100, working_grade.hs_lev100);
            if (!paper_profile_grade_is_valid(&working_grade)) {
                ESP_LOGI(TAG, "Clearing grade data because invalid values were accepted");
                memset(&working_grade, 0, sizeof(paper_profile_grade_t));
            } else {
                ESP_LOGI(TAG, "Accepting valid values");
            }
            memcpy(&profile->grade[grade], &working_grade, sizeof(paper_profile_grade_t));
            profile->max_net_density = working_max_net_density;
            menu_result = MENU_SAVE;
            break;
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }

    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}

menu_result_t menu_paper_profile_calibrate_grade(state_controller_t *controller, const char *title, paper_profile_grade_t *paper_grade, float max_net_density)
{
    menu_result_t menu_result = MENU_OK;
    char *buf = NULL;
    step_wedge_t *wedge = NULL;
    float *patch_density = NULL;
    uint8_t option = 1;
    float paper_dmin = 0.0F;
    float paper_dmax = 0.0F;
    uint32_t calibration_pev = 0;

    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);
    calibration_pev = exposure_get_calibration_pev(exposure_state);
    if (calibration_pev > 999) {
        calibration_pev = 0;
    }

    /* Load step wedge configuration */
    if (!settings_get_step_wedge(&wedge)) {
        ESP_LOGI(TAG, "No saved step wedge, loading default");
        wedge = step_wedge_create_from_stock(DEFAULT_STOCK_WEDGE_INDEX);
        if (!wedge) {
            ESP_LOGE(TAG, "Unable to load default wedge");
            return MENU_OK;
        }
    }

    /* Allocate an array for the patch density measurements */
    patch_density = pvPortMalloc(sizeof(float) * wedge->step_count);
    if (!patch_density) {
        vPortFree(wedge);
        return MENU_OK;
    }

    /* Allocate a buffer for the menu text */
    buf = pvPortMalloc((wedge->step_count * (sizeof(char) * 33)) + (5 * 33));
    if (!buf) {
        vPortFree(patch_density);
        vPortFree(wedge);
        return MENU_OK;
    }

    /* Initialize the array of patch density measurements */
    for (uint32_t i = 0; i < wedge->step_count; i++) {
        patch_density[i] = NAN;
    }

    do {
        paper_dmax = paper_dmin + max_net_density;

        size_t offset = 0;
        size_t name_len = MIN(strlen(wedge->name), 20);

        strcpy(buf, "Step wedge ");
        offset = pad_str_to_length(buf, ' ', DISPLAY_MENU_ROW_LENGTH - (name_len + 2));
        buf[offset++] = '{';
        strncpy(buf + offset, wedge->name, name_len + 1);
        offset += name_len;
        buf[offset++] = '}';
        buf[offset++] = '\n';
        buf[offset] = '\0';

        offset += sprintf(buf + offset,
            "Paper Dmin              [D=%0.02f]\n",
            paper_dmin);

        if (isnormal(paper_dmax) && paper_dmax > 0.0F) {
            offset += sprintf(buf + offset,
                "Paper Dmax              {D=%0.02f}\n",
                paper_dmax);
        } else {
            offset += sprintf(buf + offset,
                "Paper Dmax              {------}\n");
        }

        if (calibration_pev > 0) {
            offset += sprintf(buf + offset,
                "Paper Exposure Value       [%3lu]\n", calibration_pev);
        } else {
            offset += sprintf(buf + offset,
                "Paper Exposure Value       [---]\n");
        }

        for (uint32_t i = 0; i < wedge->step_count; i++) {
            if (is_valid_number(patch_density[i]) && patch_density[i] >= paper_dmin) {
                offset += sprintf(buf + offset,
                    "Step %-2lu        {D=%0.02f} [D=%0.02f]\n",
                    i + 1,
                    step_wedge_get_density(wedge, i),
                    patch_density[i]);
            } else {
                offset += sprintf(buf + offset,
                    "Step %-2lu        {D=%0.02f} [------]\n",
                    i + 1,
                    step_wedge_get_density(wedge, i));
            }
        }
        sprintf(buf + offset, "*** Calculate Profile ***");

        option = display_selection_list(title, option, buf);

        if (option == 1) {
            menu_result = menu_step_wedge_show(wedge);
        } else if (option == 2) {
            uint16_t value_sel = lroundf(paper_dmin * 100);
            bool dens_enable = usb_serial_is_attached();
            if (dens_enable) {
                usb_serial_clear_receive_buffer();
            }
            if (display_input_value_f16_data_cb(
                "Paper Dmin (optional)",
                "Minimum density of the paper,\n"
                "used as an offset for the\n"
                "step measurements.\n",
                "D=", &value_sel, 0, 999, 1, 2, "",
                menu_paper_densitometer_data_callback, &dens_enable) == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                paper_dmin = (float)value_sel / 100.0F;
            }
        } else if (option == 3) {
            char msg_buf[256];
            sprintf(msg_buf,
                "Maximum density of the paper,\n"
                "based on Dmin and the paper\n"
                "profile's max net density.\n"
                "D=%0.02f\n",
                paper_dmax);

            uint8_t msg_option = display_message("-- Paper Dmax --", NULL,
                msg_buf, " OK ");
            if (msg_option == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            }
        } else if (option == 4) {
            uint16_t value_sel = calibration_pev;
            if (display_input_value_u16(
                "Paper Exposure Value",
                "Paper exposure value used for\n"
                "the step wedge exposure being\n"
                "measured.\n",
                "", &value_sel, 0, 999, 3, "") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                calibration_pev = value_sel;
            }
        } else if (option >= 5 && option < 5 + wedge->step_count) {
            int step_index = option - 5;

            uint8_t patch_option;
            char patch_title_buf[32];
            char patch_buf[128];
            sprintf(patch_title_buf, "Step %d", step_index + 1);
            sprintf(patch_buf,
                "\nMeasured density at patch %d\n"
                "of the exposed paper.\n",
                step_index + 1);

            /* Constrain the input based on Dmin and Dmax */
            uint16_t min_value = lroundf(paper_dmin * 100);
            uint16_t max_value = lroundf(paper_dmax * 100);

            uint16_t value_sel = 0;
            if (isnormal(patch_density[step_index]) && patch_density[step_index] > 0.0F) {
                value_sel = lroundf(patch_density[step_index] * 100);
            }
            if (value_sel < min_value) {
                value_sel = min_value;
            }
            if (value_sel > max_value) {
                value_sel = max_value;
            }

            bool dens_enable = usb_serial_is_attached();
            if (dens_enable) {
                usb_serial_clear_receive_buffer();
            }

            patch_option = display_input_value_f16_data_cb(
                patch_title_buf, patch_buf,
                "D=", &value_sel, min_value, max_value, 1, 2, "",
                menu_paper_densitometer_data_callback, &dens_enable);
            if (patch_option == 1) {
                if (value_sel <= min_value) {
                    patch_density[step_index] = paper_dmin;
                } else if (value_sel >= max_value) {
                    patch_density[step_index] = paper_dmax;
                } else {
                    patch_density[step_index] = (float)value_sel / 100.0F;
                }
            } else if (patch_option == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            }
        } else if (option == 5 + wedge->step_count) {
            menu_result_t val_result;
            ESP_LOGI(TAG, "Calculate profile");

            /* Collect the calibration parameters */
            wedge_calibration_params_t params = {
                .wedge = wedge,
                .paper_dmin = paper_dmin,
                .paper_dmax = paper_dmax,
                .calibration_pev = calibration_pev,
                .patch_density = patch_density
            };

            /* Validate the parameters */
            val_result = menu_paper_profile_calibrate_grade_validate(&params);
            if (val_result == MENU_CANCEL) {
                ESP_LOGI(TAG, "Validation failed");
                continue;
            } else if (val_result == MENU_TIMEOUT) {
                menu_result = MENU_TIMEOUT;
                break;
            }
            ESP_LOGI(TAG, "Calibration parameters validated");

            /* Calculate the profile */
            val_result = menu_paper_profile_calibrate_grade_calculate(title, &params, paper_grade);
            if (val_result == MENU_OK || val_result == MENU_SAVE) {
                ESP_LOGI(TAG, "Calculation accepted");
                menu_result = MENU_SAVE;
                break;
            } else if (val_result == MENU_CANCEL) {
                ESP_LOGI(TAG, "Calculation failed");
                continue;
            } else if (val_result == MENU_TIMEOUT) {
                menu_result = MENU_TIMEOUT;
                break;
            }
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }

    } while (option > 0 && menu_result != MENU_TIMEOUT);

    vPortFree(buf);
    vPortFree(patch_density);
    vPortFree(wedge);

    return menu_result;
}

uint16_t menu_paper_densitometer_data_callback(void *user_data)
{
    bool dens_enable = *((bool *)user_data);
    if (dens_enable) {
        densitometer_reading_t reading;
        if (densitometer_reading_poll(&reading, 50) == DENSITOMETER_RESULT_OK) {
            if ((reading.mode == DENSITOMETER_MODE_UNKNOWN || reading.mode == DENSITOMETER_MODE_REFLECTION)
                && !isnanf(reading.visual) && reading.visual > 0.0F) {
                return lroundf(reading.visual * 100);
            }
        }
    }
    return UINT16_MAX;
}

menu_result_t menu_paper_profile_calibrate_grade_validate(const wedge_calibration_params_t *params)
{
    /* Validate arguments that should never be in question */
    if (!params || !params->wedge || !params->patch_density) {
        return MENU_CANCEL;
    }

    /* Make sure we have a valid step wedge */
    if (!step_wedge_is_valid(params->wedge)) {
        uint8_t msg_option = display_message(
                "Invalid Step Wedge\n",
                NULL,
                "Cannot calculate without valid\n"
                "step wedge properties.\n",
                " OK ");
        if (msg_option == UINT8_MAX) {
            return MENU_TIMEOUT;
        }

        return MENU_CANCEL;
    }

    /* Validate the PEV, Dmin, and Dmax values */
    if (!(is_valid_number(params->paper_dmin) && is_valid_number(params->paper_dmax)
        && (params->paper_dmin < params->paper_dmax)
        && (params->calibration_pev > 0) && (params->calibration_pev <= 999))) {
        uint8_t msg_option = display_message(
                "Invalid Properties\n",
                NULL,
                "Paper measurement properties\n"
                "are not valid.\n",
                " OK ");
        if (msg_option == UINT8_MAX) {
            return MENU_TIMEOUT;
        }

        return MENU_CANCEL;
    }

    /* Find the min and max patch values so they can be validated */
    float patch_min = NAN;
    float patch_max = NAN;
    uint32_t patch_count = 0;
    for (uint32_t i = 0; i < params->wedge->step_count; i++) {
        if (!is_valid_number(params->patch_density[i])) {
            continue;
        }
        if (isnanf(patch_min) || params->patch_density[i] < patch_min) {
            patch_min = params->patch_density[i];
        }
        if (isnanf(patch_max) || params->patch_density[i] > patch_max) {
            patch_max = params->patch_density[i];
        }
        patch_count++;
    }

    /* Make sure we actually found min and max patch values */
    if (!is_valid_number(patch_min) || !is_valid_number(patch_max)
        || fabsf(patch_max - patch_min) < 0.01F) {
        uint8_t msg_option = display_message(
                "Invalid Measurements\n",
                NULL,
                "Paper patch measurements are\n"
                "missing or invalid.\n",
                " OK ");
        if (msg_option == UINT8_MAX) {
            return MENU_TIMEOUT;
        }

        return MENU_CANCEL;
    }

    /* Make sure we have enough patches */
    if (patch_count < 3) {
        uint8_t msg_option = display_message(
                "Invalid Measurements\n",
                NULL,
                "Must have at least 3 paper"
                "patch measurements.\n",
                " OK ");
        if (msg_option == UINT8_MAX) {
            return MENU_TIMEOUT;
        }

        return MENU_CANCEL;
    }

    /* Make sure the min value crosses Dmin+0.04 */
    float patch_min_crossing = params->paper_dmin + 0.04F;
    if (patch_min > patch_min_crossing) {
        char buf[128];
        sprintf(buf,
            "Paper patch measurements must\n"
            "go below D=%0.02f.\n",
            patch_min_crossing);
        uint8_t msg_option = display_message(
                "Invalid Measurements\n",
                NULL, buf, " OK ");
        if (msg_option == UINT8_MAX) {
            return MENU_TIMEOUT;
        }

        return MENU_CANCEL;
    }

    /* Make sure the max value crosses Dnet*0.90 */
    float paper_dnet = params->paper_dmax - params->paper_dmin;
    float patch_max_crossing = params->paper_dmin + (paper_dnet * 0.90F);
    if (patch_max < patch_max_crossing) {
        char buf[128];
        sprintf(buf,
            "Paper patch measurements must\n"
            "go above D=%0.02f.\n",
            patch_max_crossing);
        uint8_t msg_option = display_message(
                "Invalid Measurements\n",
                NULL, buf, " OK ");
        if (msg_option == UINT8_MAX) {
            return MENU_TIMEOUT;
        }

        return MENU_CANCEL;
    }

    return MENU_OK;
}

menu_result_t menu_paper_profile_calibrate_grade_calculate(const char *title, const wedge_calibration_params_t *params, paper_profile_grade_t *paper_grade)
{
    char buf[128];
    uint8_t msg_option = 0;
    size_t num_patches = 0;
    size_t num_output = 0;
    int32_t Ht_lev100 = 0;
    int32_t Hm_lev100 = 0;
    int32_t Hs_lev100 = 0;
    uint32_t iso_r = 0;
    bool calculation_completed = false;

    /* Validate arguments that should never be in question */
    if (!params || !params->wedge || !params->patch_density) {
        return MENU_CANCEL;
    }

    /* Count the number of patches with valid data */
    for (uint32_t i = 0; i < params->wedge->step_count; i++) {
        if (is_valid_number(params->patch_density[i])) {
            num_patches++;
        }
    }
    ESP_LOGI(TAG, "Found %d patches with valid measurements", num_patches);

    /* X-axis is the calculated PEV values for the step wedge exposure */
    float32_t *x_pev_f32 = NULL;

    /* Y-axis is the measured density values from the paper */
    float32_t *y_density_f32 = NULL;

    /* Coefficients array used for the interpolation */
    float32_t *coeffs = NULL;

    /* Buffer array used for the interpolation */
    float32_t *temp_buffer = NULL;

    /* Output X-axis is the full range of PEV values from min to max */
    float32_t *xq_pev_f32 = NULL;

    /* Output Y-axis is the coresponding calculated density values */
    float32_t *yq_density_f32 = NULL;

    /* Output Y-axis scaled for the display graph */
    uint8_t *graph_points = NULL;

    do {
        arm_spline_instance_f32 S;

        /* Allocate the arrays needed for the cubic spline interpolation */
        x_pev_f32 = pvPortMalloc(sizeof(float32_t) * num_patches);
        if (!x_pev_f32) { break; }

        y_density_f32 = pvPortMalloc(sizeof(float32_t) * num_patches);
        if (!y_density_f32) { break; }

        coeffs = pvPortMalloc(sizeof(float32_t) * (3 * (num_patches - 1))); /* 3*(n-1) */
        if (!coeffs) { break; }

        temp_buffer = pvPortMalloc(sizeof(float32_t) * (num_patches + num_patches - 1)); /* n+n-1 */
        if (!temp_buffer) { break; }

        /* Apply wedge data to populate PEV and density values for each patch */
        uint32_t p = 0;
        for (int i = params->wedge->step_count - 1; i >= 0; --i) {
            if (is_valid_number(params->patch_density[i])) {
                x_pev_f32[p] = roundf((float)params->calibration_pev - (params->wedge->step_density[i] * 100.0F));
#if 0
                ESP_LOGI(TAG, "[i=%lu,p=%lu] %0.02f = %lu - (%0.02f * 100)",
                    i, p,
                    x_pev_f32[p], params->calibration_pev, params->wedge->step_density[i]);
#endif
                y_density_f32[p] = params->patch_density[i];
                p++;
            }
        }

        /* Initialize the cubic spline interpolation */
        arm_spline_init_f32(&S, ARM_SPLINE_NATURAL,
            x_pev_f32, y_density_f32, num_patches,
            coeffs, temp_buffer);

        num_output = abs(lroundf(x_pev_f32[num_patches - 1]) - lroundf(x_pev_f32[0]));
        if (num_output > 500) {
            ESP_LOGW(TAG, "Interpolation range unusually large: %d", num_output);
            break;
        }
        float min_xq = MIN(x_pev_f32[0], x_pev_f32[num_patches - 1]);
        ESP_LOGI(TAG, "Interpolating curve from PEV=%ld to %ld (%d steps)",
            lroundf(x_pev_f32[0]), lroundf(x_pev_f32[num_patches - 1]), num_output);

        /* Allocate the arrays needed for interpolation results */
        xq_pev_f32 = pvPortMalloc(sizeof(float32_t) * num_output);
        if (!xq_pev_f32) { break; }

        yq_density_f32 = pvPortMalloc(sizeof(float32_t) * num_output);
        if (!yq_density_f32) { break; }

        /* Fill the interpolation output X-value array */
        for (uint32_t i = 0; i < num_output; i++) {
            xq_pev_f32[i] = min_xq + i;
        }

        /* Run the cubic spline interpolation to get the characteristic curve */
        arm_spline_f32(&S, xq_pev_f32, yq_density_f32, num_output);

        /* Declare the reference point density values */
        const float32_t Ht_D = params->paper_dmin + 0.04F; /* paper base + 0.04 */
        const float32_t Hm_D = params->paper_dmin + 0.60F; /* paper base + 0.60 */
        const float32_t Hs_D = params->paper_dmin + ((params->paper_dmax - params->paper_dmin) * 0.90F); /* 90% of Dnet */

        /* Iterate across the output to find PEV values for each reference point */
        float32_t Ht_x = NAN;
        float32_t Ht_y = NAN;
        float32_t Hm_x = NAN;
        float32_t Hm_y = NAN;
        float32_t Hs_x = NAN;
        float32_t Hs_y = NAN;
        for (uint32_t i = 0; i < num_output; i++) {
            if (isnanf(Ht_y) || fabsf(Ht_D - yq_density_f32[i]) < fabsf(Ht_D - Ht_y)) {
                Ht_y = yq_density_f32[i];
                Ht_x = xq_pev_f32[i];
            }
            if (isnanf(Hm_y) || fabsf(Hm_D - yq_density_f32[i]) < fabsf(Hm_D - Hm_y)) {
                Hm_y = yq_density_f32[i];
                Hm_x = xq_pev_f32[i];
            }
            if (isnanf(Hs_y) || fabsf(Hs_D - yq_density_f32[i]) < fabsf(Hs_D - Hs_y)) {
                Hs_y = yq_density_f32[i];
                Hs_x = xq_pev_f32[i];
            }
        }

        /* Assign the results, keeping signs */
        Ht_lev100 = lroundf(Ht_x);
        Hm_lev100 = lroundf(Hm_x);
        Hs_lev100 = lroundf(Hs_x);
        iso_r = abs(lroundf(Hs_lev100 - Ht_lev100));

        /* Log the results */
        ESP_LOGI(TAG, "Ht: PEV=%ld, D=%0.02f (%0.02f)", Ht_lev100, Ht_y, Ht_D);
        ESP_LOGI(TAG, "Hm: PEV=%ld, D=%0.02f (%0.02f)", Hm_lev100, Hm_y, Hm_D);
        ESP_LOGI(TAG, "Hs: PEV=%ld, D=%0.02f (%0.02f)", Hs_lev100, Hs_y, Hs_D);
        ESP_LOGI(TAG, "ISO(R) = %ld", iso_r);

        /*
         * Sanity check the numbers and abort if wildly out of range.
         * Unlike the regular profile validation, we don't reject negative
         * values here. If the values go negative, we can still use the
         * calculated ISO(R) even if everything else is not usable.
         */
        if (Ht_lev100 > 999 && Hm_lev100 > 999 && Hs_lev100 > 999) {
            return false;
        }
        if (Ht_lev100 >= Hs_lev100) {
            return false;
        }
        if (Hm_lev100 <= Ht_lev100 || Hm_lev100 >= Hs_lev100) {
            return false;
        }

        /* Free the interpolation result arrays */
        vPortFree(xq_pev_f32);
        vPortFree(yq_density_f32);
        xq_pev_f32 = NULL;
        yq_density_f32 = NULL;

        ESP_LOGI(TAG, "Preparing results display graph");

        /* Reallocate the result arrays for the display graph */
        size_t num_graph = 127;
        xq_pev_f32 = pvPortMalloc(sizeof(float32_t) * num_graph);
        if (!xq_pev_f32) { break; }

        yq_density_f32 = pvPortMalloc(sizeof(float32_t) * num_graph);
        if (!yq_density_f32) { break; }

        graph_points = pvPortMalloc(sizeof(uint8_t) * num_graph);
        if (!yq_density_f32) { break; }

        /* Fill the interpolation display graph output X-value array */
        float xq_increment = (float)num_output / (float)num_graph;
        for (uint32_t i = 0; i < num_graph; i++) {
            xq_pev_f32[i] = min_xq + (i * xq_increment);
        }

        /* Run the cubic spline interpolation to get the display graph curve */
        arm_spline_f32(&S, xq_pev_f32, yq_density_f32, num_graph);

        /* Find the minimum and maximum values in the display graph curve */
        float yq_density_min = NAN;
        float yq_density_max = NAN;
        for (uint32_t i = 0; i < num_graph; i++) {
            if (isnanf(yq_density_min) || yq_density_f32[i] < yq_density_min) {
                yq_density_min = yq_density_f32[i];
            }
            if (isnanf(yq_density_max) || yq_density_f32[i] > yq_density_max) {
                yq_density_max = yq_density_f32[i];
            }
        }

        /* Make sure the display values cannot go negative */
        if (yq_density_min < 0.0F) {
            yq_density_min += fabsf(yq_density_min);
            yq_density_max += fabsf(yq_density_min);
        }

        /* Scale the display graph curve values */
        for (uint32_t i = 0; i < num_graph; i++) {
            graph_points[i] = lroundf(42.0F * ((yq_density_f32[i] - yq_density_min) / (yq_density_max - yq_density_min))) + 4;
        }

        ESP_LOGI(TAG, "Showing results display graph");

        /* Format the message text */
        if (Ht_lev100 > 0 && Hm_lev100 > 0 && Hs_lev100 > 0) {
            sprintf(buf,
                "Base exp  [%3lu] \n"
                "Speed pt  [%3lu] \n"
                "ISO Range [%3lu] ",
                Ht_lev100, Hm_lev100, iso_r);
        } else {
            sprintf(buf,
                "Base exp  [---] \n"
                "Speed pt  [---] \n"
                "ISO Range [%3lu] ",
                iso_r);
        }

        calculation_completed = true;

        msg_option = display_message_graph(title, buf, " OK \n Cancel ",
            graph_points, num_graph);

    } while (0);

    /* Free dynamically allocated arrays */
    vPortFree(x_pev_f32);
    vPortFree(y_density_f32);
    vPortFree(coeffs);
    vPortFree(temp_buffer);
    vPortFree(xq_pev_f32);
    vPortFree(yq_density_f32);
    vPortFree(graph_points);

    if (!calculation_completed) {
        msg_option = display_message(
            "Calculation Error\n",
            NULL,
            "Unable to calculate the"
            "paper profile.\n",
            " OK ");
        if (msg_option == UINT8_MAX) {
            return MENU_TIMEOUT;
        } else {
            return MENU_CANCEL;
        }
    }

    if (msg_option == 1) {
        if (Ht_lev100 > 0 && Hm_lev100 > 0 && Hs_lev100 > 0) {
            ESP_LOGI(TAG, "Updating all grade values");
            paper_grade->ht_lev100 = Ht_lev100;
            paper_grade->hm_lev100 = Hm_lev100;
            paper_grade->hs_lev100 = Hs_lev100;
        } else {
            ESP_LOGI(TAG, "Updating only ISO(R) due to low PEV numbers");
            paper_grade->hs_lev100 = paper_grade->ht_lev100 + iso_r;
        }
        ESP_LOGI(TAG, "Calibration parameters updated");
        return MENU_OK;
    } else if (msg_option == UINT8_MAX) {
        return MENU_TIMEOUT;
    } else {
        ESP_LOGI(TAG, "Calibration parameters rejected");
        return MENU_CANCEL;
    }
}
