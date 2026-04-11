#include "menu_paper.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <arm_math.h>

#define LOG_TAG "menu_paper"
#include <elog.h>

#include "display.h"
#include "keypad.h"
#include "settings.h"
#include "paper_profile.h"
#include "util.h"
#include "densitometer.h"
#include "step_wedge.h"
#include "menu_step_wedge.h"
#include "menu_settings.h"
#include "exposure_state.h"

typedef struct {
    step_wedge_t *wedge;
    float paper_dmin;
    float paper_dmax;
    uint32_t calibration_pev;
    float *patch_density;
} wedge_calibration_params_t;

typedef struct {
    uint8_t paper_param; /* 0 = Dmin+0.04, 1 = Dmin+0.60, 2 = 90% Dnet */
    float paper_dmin;
    float paper_dmax;
    uint32_t calibration_pev;
    uint8_t patch_count;
    float patch_increment;
    float *patch_density;
} strip_calibration_params_t;

typedef struct {
    uint8_t min_option;
    uint8_t max_option;
    uint8_t alt_option;
    uint16_t reading;
} menu_paper_callback_data_t;

static const char *PAPER_PARAM_NAMES[] = {
    "Dmin+0.04", "Dmin+0.60", "90% Dnet"
};

static menu_result_t menu_paper_profile_edit(state_controller_t *controller, paper_profile_t *profile, uint8_t index);
static menu_result_t menu_paper_profile_check_save(paper_profile_t *profile);
static void menu_paper_delete_profile(uint8_t index, size_t profile_count);
static bool menu_paper_profile_delete_prompt(const paper_profile_t *profile, uint8_t index);
static menu_result_t menu_paper_profile_edit_grade(state_controller_t *controller, paper_profile_t *profile, uint8_t index, contrast_grade_t grade, menu_paper_callback_data_t *dens_data);
static menu_result_t menu_paper_profile_calibrate_grade(state_controller_t *controller, const char *title, paper_profile_grade_t *paper_grade, float paper_dmin, float paper_dmax, menu_paper_callback_data_t *dens_data);
static menu_result_t menu_paper_profile_calibrate_test_strip(state_controller_t *controller, const char *title, paper_profile_grade_t *paper_grade, float paper_dmin, float paper_dmax, menu_paper_callback_data_t *dens_data);
static uint8_t menu_paper_densitometer_input_poll_callback(uint8_t current_pos, uint8_t event_action, void *user_data);
static uint16_t menu_paper_densitometer_data_callback(uint8_t event_action, void *user_data);
static uint16_t menu_paper_densitometer_callback_impl(uint8_t event_action, menu_paper_callback_data_t *data);
static menu_result_t menu_paper_profile_calibrate_grade_validate(const wedge_calibration_params_t *params);
static menu_result_t menu_paper_profile_calibrate_grade_calculate(const char *title, const wedge_calibration_params_t *params, paper_profile_grade_t *paper_grade);
static menu_result_t menu_paper_profile_calibrate_test_strip_validate(const strip_calibration_params_t *params);
static menu_result_t menu_paper_profile_calibrate_test_strip_calculate(const char *title, const strip_calibration_params_t *params, paper_profile_grade_t *paper_grade);

menu_result_t menu_paper_profiles(state_controller_t *controller)
{
    menu_result_t menu_result = MENU_OK;

    paper_profile_t *profile_list;
    profile_list = pvPortMalloc(sizeof(paper_profile_t) * MAX_PAPER_PROFILES);
    if (!profile_list) {
        log_e("Unable to allocate memory for profile list");
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
            log_i("Loaded %d profiles, default is %d", profile_count, profile_default_index);
            reload_profiles = false;
        }

        for (size_t i = 0; i < profile_count; i++) {
            if (strlen(profile_list[i].name) > 0) {
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
            DISPLAY_MENU_ACCEPT_MENU | DISPLAY_MENU_ACCEPT_ENCODER | DISPLAY_MENU_ACCEPT_ADD_ADJUSTMENT);
        option = (uint8_t)(result & 0x00FF);
        keypad_key_t option_key = (uint8_t)((result & 0xFF00) >> 8);

        if (option == 0) {
            menu_result = MENU_CANCEL;
            break;
        } else if (option - 1 == profile_count) {
            paper_profile_t working_profile = {0};

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
                        log_i("New profile manually added at index: %d", profile_count);
                        memcpy(&profile_list[profile_count], &working_profile, sizeof(paper_profile_t));
                        profile_count++;
                    }
                }
            }
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        } else {
            if (option_key == KEYPAD_MENU || option_key == KEYPAD_ENCODER) {
                paper_profile_t working_profile;
                memcpy(&working_profile, &profile_list[option - 1], sizeof(paper_profile_t));
                menu_result = menu_paper_profile_edit(controller, &working_profile, option - 1);
                if (menu_result == MENU_SAVE) {
                    menu_result = MENU_OK;
                    if (settings_set_paper_profile(&working_profile, option - 1)) {
                        log_i("Profile saved at index: %d", option - 1);
                        memcpy(&profile_list[option - 1], &working_profile, sizeof(paper_profile_t));
                    }
                } else if (menu_result == MENU_DELETE) {
                    menu_result = MENU_OK;
                    menu_paper_delete_profile(option - 1, profile_count);
                    reload_profiles = true;
                }
            } else if (option_key == KEYPAD_ADD_ADJUSTMENT) {
                log_i("Set default profile at index: %d", option - 1);
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
    const bool valid = paper_profile_grade_is_valid(&profile->grade[grade]);
    const uint32_t iso_r = profile->grade[grade].hs_lev100 - profile->grade[grade].ht_lev100;
    if (!valid) {
        offset = sprintf(str,
            "Grade %-2s   [---][---][---][R---]\n",
            contrast_grade_str(grade));
    } else if (profile->grade[grade].hm_lev100 == 0) {
        offset = sprintf(str,
            "Grade %-2s   [%3lu][---][%3lu][R%3lu]\n",
            contrast_grade_str(grade),
            profile->grade[grade].ht_lev100,
            profile->grade[grade].hs_lev100,
            iso_r);
    } else {
        offset = sprintf(str,
            "Grade %-2s   [%3lu][%3lu][%3lu][R%3lu]\n",
            contrast_grade_str(grade),
            profile->grade[grade].ht_lev100,
            profile->grade[grade].hm_lev100,
            profile->grade[grade].hs_lev100,
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
    menu_paper_callback_data_t dens_data = {0};

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

        if (is_valid_number(profile->paper_dmin)) {
            offset += sprintf(buf + offset, "Paper Dmin              [D=%01.2f]\n", profile->paper_dmin);
        } else {
            offset += sprintf(buf + offset, "Paper Dmin              [------]\n");
        }

        if (isnormal(profile->paper_dmax) && profile->paper_dmax > 0.0F) {
            offset += sprintf(buf + offset, "Paper Dmax              [D=%01.2f]\n", profile->paper_dmax);
        } else {
            offset += sprintf(buf + offset, "Paper Dmax              [------]\n");
        }

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

        dens_data.min_option = 2;
        dens_data.max_option = 3;
        dens_data.alt_option = UINT8_MAX;
        option = display_selection_list_cb(buf_title, option, buf,
            menu_paper_densitometer_input_poll_callback, &dens_data);

        if (option == 1) {
            if (display_input_text("Paper Profile Name", profile->name, PROFILE_NAME_LEN) > 0) {
                profile_dirty = true;
            }
        } else if (option == 2) {
            uint16_t value_sel = lroundf(profile->paper_dmin * 100);
            uint16_t value_sel_prev = value_sel;

            if (dens_data.reading != UINT16_MAX) {
                if (dens_data.reading != value_sel_prev) {
                    profile->paper_dmin = (float) dens_data.reading / 100.0F;
                    profile_dirty = true;
                }
                option++;
            } else {
                if (display_input_value_f16_data_cb(
                        "Paper Dmin",
                        "Minimum density (Dmin) of the\n"
                        "paper, otherwise known as the\n"
                        "density of the paper base.\n",
                        "D=", &value_sel, 0, 999, 1, 2, "",
                        menu_paper_densitometer_data_callback, &dens_data) == UINT8_MAX) {
                    menu_result = MENU_TIMEOUT;
                } else {
                    if (value_sel != value_sel_prev) {
                        profile->paper_dmin = (float) value_sel / 100.0F;
                        profile_dirty = true;
                    }
                }
                densitometer_disable();
            }
        } else if (option == 3) {
            uint16_t value_sel = lroundf(profile->paper_dmax * 100);
            uint16_t value_sel_prev = value_sel;

            if (dens_data.reading != UINT16_MAX) {
                if (dens_data.reading != value_sel_prev) {
                    profile->paper_dmax = (float) dens_data.reading / 100.0F;
                    profile_dirty = true;
                }
                option++;
            } else {
                if (display_input_value_f16_data_cb(
                        "Paper Dmax",
                        "Maximum density (Dmax) of\n"
                        "the paper.\n",
                        "D=", &value_sel, 0, 999, 1, 2, "",
                        menu_paper_densitometer_data_callback, &dens_data) == UINT8_MAX) {
                    menu_result = MENU_TIMEOUT;
                } else {
                    if (value_sel != value_sel_prev) {
                        profile->paper_dmax = (float) value_sel / 100.0F;
                        profile_dirty = true;
                    }
                }
                densitometer_disable();
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
            menu_result_t sub_result = menu_paper_profile_edit_grade(controller, profile, index, grade, &dens_data);
            if (sub_result == MENU_SAVE) {
                profile_dirty = true;
            } else if (sub_result == MENU_TIMEOUT) {
                menu_result = MENU_TIMEOUT;
            }
        } else if (option == 11) {
            log_d("Save changes from profile editor");

            menu_result_t sub_result = menu_paper_profile_check_save(profile);
            if (sub_result == MENU_CANCEL) {
                option = 1;
                continue;
            } else {
                menu_result = sub_result;
            }

            break;
        } else if (option == 12) {
            log_d("Delete profile from profile editor");
            if (menu_paper_profile_delete_prompt(profile, index)) {
                menu_result = MENU_DELETE;
                break;
            }
        } else if (option == 0 && profile_dirty) {
            menu_result_t sub_result = menu_confirm_cancel(buf_title);

            /* Do extra pre-save validation */
            if (sub_result == MENU_SAVE) {
                sub_result = menu_paper_profile_check_save(profile);
            }

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

    densitometer_disable();

    return menu_result;
}

menu_result_t menu_paper_profile_check_save(paper_profile_t *profile)
{
    menu_result_t menu_result = MENU_CANCEL;

    if (!(isnormal(profile->paper_dmax) && profile->paper_dmax > 0.0F)) {
        uint8_t msg_option = display_message(
            "Max Density Missing\n",
            NULL,
            "Cannot save a paper profile\n"
            "without a Dmax value.", " OK ");
        if (msg_option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } else if (!isnan(profile->paper_dmin) && profile->paper_dmin >= profile->paper_dmax) {
        uint8_t msg_option = display_message(
            "Min Density Invalid\n",
            NULL,
            "Cannot save a paper profile\n"
            "if Dmin is greater than Dmax.", " OK ");
        if (msg_option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } else {
        menu_result = MENU_SAVE;
    }

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

    if (strlen(profile->name) > 0) {
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

menu_result_t menu_paper_profile_edit_grade(state_controller_t *controller, paper_profile_t *profile, uint8_t index, contrast_grade_t grade, menu_paper_callback_data_t *dens_data)
{
    char buf_title[36];
    char buf[512];
    char buf_ht[11], buf_hm[11], buf_hs[11], buf_isor[11];
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

    dens_data->min_option = UINT8_MAX;
    dens_data->max_option = UINT8_MAX;
    dens_data->alt_option = UINT8_MAX;

    if (working_grade.hm_lev100 <= working_grade.ht_lev100 || working_grade.hm_lev100 >= working_grade.hs_lev100) {
        working_grade.hm_lev100 = 0;
    }

    do {
        const uint32_t iso_r = working_grade.hs_lev100 - working_grade.ht_lev100;

        if (working_grade.ht_lev100 == 0) {
            strcpy(buf_ht, "---");
        } else {
            sprintf(buf_ht, "%3lu", working_grade.ht_lev100);
        }

        if (working_grade.hm_lev100 == 0) {
            strcpy(buf_hm, "---");
        } else {
            sprintf(buf_hm, "%3lu", working_grade.hm_lev100);
        }

        if (working_grade.hs_lev100 == 0) {
            strcpy(buf_hs, "---");
        } else {
            sprintf(buf_hs, "%3lu", working_grade.hs_lev100);
        }

        if (iso_r == 0) {
            strcpy(buf_isor, "---");
        } else {
            sprintf(buf_isor, "%3lu", iso_r);
        }

        sprintf(buf,
            "Base exposure (%s)  [%s]\n"
            "Speed point   (%s)  [%s]\n"
            "Dark exposure (%s)   [%s]\n"
            "ISO Range                  [%s]\n"
            "*** Measure From Step Wedge ***\n"
            "*** Measure From Test Strip ***\n"
            "*** Accept Changes ***\n"
            "*** Reset Values ***",
            PAPER_PARAM_NAMES[0], buf_ht,
            PAPER_PARAM_NAMES[1], buf_hm,
            PAPER_PARAM_NAMES[2], buf_hs,
            buf_isor);

        option = display_selection_list(buf_title, option, buf);

        if (option == 1) {
            uint16_t value_sel = working_grade.ht_lev100;
            if (display_input_value_u16(
                "Base Exposure",
                "Print exposure value necessary\n"
                "to achieve a density of D=0.04\n"
                "above the paper base (Dmin).\n",
                "", &value_sel, 0, 999, 3, "") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                working_grade.ht_lev100 = value_sel;
            }
        } else if (option == 2) {
            uint16_t value_sel = working_grade.hm_lev100;
            if (display_input_value_u16(
                "Speed Point (optional)",
                "Print exposure value necessary\n"
                "to achieve a density of D=0.60\n"
                "above the paper base (Dmin).\n",
                "", &value_sel, 0, 999, 3, "") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                working_grade.hm_lev100 = value_sel;
            }
        } else if (option == 3) {
            uint16_t value_sel = working_grade.hs_lev100;
            if (display_input_value_u16(
                    "Dark Exposure",
                    "Print exposure value necessary\n"
                    "to achieve 90% of the paper's\n"
                    "maximum net density.\n",
                    "", &value_sel, 0, 999, 3, "") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                working_grade.hs_lev100 = value_sel;
            }
        } else if (option == 4) {
            uint16_t value_sel = iso_r;
            if (display_input_value_u16(
                "ISO Range",
                "Print exposure range between the\n"
                "base exposure and an exposure\n"
                "that achieves 90% of Dnet\n",
                "", &value_sel, 1, 999, 3, "") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                working_grade.hs_lev100 = working_grade.ht_lev100 + value_sel;
            }
        } else if (option == 5 || option == 6) {
            if (!(isnormal(profile->paper_dmax) && profile->paper_dmax > 0.0F)) {
                uint8_t msg_option = display_message(
                    "Max Density Missing\n",
                    NULL,
                    "Cannot measure step wedge\n"
                    "exposure without a paper max\n"
                    "density value.", " OK ");
                if (msg_option == UINT8_MAX) {
                    menu_result = MENU_TIMEOUT;
                }
            } else if (!isnan(profile->paper_dmin) && profile->paper_dmin >= profile->paper_dmax) {
                uint8_t msg_option = display_message(
                    "Min Density Invalid\n",
                    NULL,
                    "Cannot measure step wedge\n"
                    "exposure if Dmin greater\n"
                    "than Dmax.", " OK ");
                if (msg_option == UINT8_MAX) {
                    menu_result = MENU_TIMEOUT;
                }
            } else {
                menu_result_t cal_result;
                if (option == 5) {
                    cal_result = menu_paper_profile_calibrate_grade(controller, buf_title, &working_grade,
                        profile->paper_dmin, profile->paper_dmax, dens_data);
                } else {
                    cal_result = menu_paper_profile_calibrate_test_strip(controller, buf_title, &working_grade,
                        profile->paper_dmin, profile->paper_dmax, dens_data);
                }
                if (cal_result == MENU_SAVE) {
                    /* If calibration results were accepted, move the cursor for convenience */
                    if (option == 5) {
                        option = 7;
                    } else {
                        option = 1;
                    }
                } else if (cal_result == MENU_TIMEOUT) {
                    menu_result = MENU_TIMEOUT;
                }
            }
        } else if (option == 7) {
            log_i("Accept: Grade %s, ht_lev100=%lu, hm_lev100=%lu, hs_lev100=%lu",
                contrast_grade_str(grade),
                working_grade.ht_lev100, working_grade.hm_lev100, working_grade.hs_lev100);
            if (!paper_profile_grade_is_valid(&working_grade)) {
                uint8_t msg_option = display_message(
                    "Invalid Grade Values\n",
                    NULL,
                    "Cannot accept invalid parameters\n"
                    "for this paper grade.\n",
                    " OK ");
                if (msg_option == UINT8_MAX) {
                    return MENU_TIMEOUT;
                }
                continue;
            } else {
                log_i("Accepting valid values");
            }
            memcpy(&profile->grade[grade], &working_grade, sizeof(paper_profile_grade_t));
            menu_result = MENU_SAVE;
            break;
        } else if (option == 8) {
            log_i("Clearing grade data");
            memset(&profile->grade[grade], 0, sizeof(paper_profile_grade_t));
            menu_result = MENU_SAVE;
            break;
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }

    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}

menu_result_t menu_paper_profile_calibrate_grade(state_controller_t *controller, const char *title, paper_profile_grade_t *paper_grade, float paper_dmin, float paper_dmax, menu_paper_callback_data_t *dens_data)
{
    menu_result_t menu_result = MENU_OK;
    char *buf = NULL;
    step_wedge_t *wedge = NULL;
    float *patch_density = NULL;
    uint8_t option = 1;
    uint32_t calibration_pev = 0;

    const float display_dmin = paper_dmin;
    const float display_dmax = paper_dmax;

    if (!is_valid_number(paper_dmin)) { paper_dmin = 0.0F; }
    if (!is_valid_number(paper_dmax)) { paper_dmax = 0.0F; }

    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);
    calibration_pev = exposure_get_calibration_pev(exposure_state);
    if (calibration_pev > 999) {
        calibration_pev = 0;
    }

    /* Load step wedge configuration */
    if (!settings_get_step_wedge(&wedge)) {
        log_i("No saved step wedge, loading default");
        wedge = step_wedge_create_from_stock(DEFAULT_STOCK_WEDGE_INDEX);
        if (!wedge) {
            log_e("Unable to load default wedge");
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

    dens_data->min_option = 5;
    dens_data->max_option = (5 + wedge->step_count) - 1;
    dens_data->alt_option = UINT8_MAX;

    do {
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

        if (isnormal(display_dmin) && display_dmin > 0.0F) {
            offset += sprintf(buf + offset,
                "Paper Dmin              {D=%0.02f}\n",
                display_dmin);
        } else {
            offset += sprintf(buf + offset,
                "Paper Dmin              {------}\n");
        }

        if (isnormal(display_dmax) && display_dmax > 0.0F) {
            offset += sprintf(buf + offset,
                "Paper Dmax              {D=%0.02f}\n",
                display_dmax);
        } else {
            offset += sprintf(buf + offset,
                "Paper Dmax              {------}\n");
        }

        if (calibration_pev > 0) {
            offset += sprintf(buf + offset,
                "Print Exposure Value       [%3lu]\n", calibration_pev);
        } else {
            offset += sprintf(buf + offset,
                "Print Exposure Value       [---]\n");
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

        option = display_selection_list_cb(title, option, buf,
            menu_paper_densitometer_input_poll_callback, dens_data);

        if (option == 1) {
            menu_result = menu_step_wedge_show(wedge);
        } else if (option == 2) {
            char msg_buf[256];
            sprintf(msg_buf,
                "Minimum density of the paper,\n"
                "used as the lower bound for\n"
                "measurements.\n"
                "D=%0.02f\n",
                paper_dmin);

            uint8_t msg_option = display_message("-- Paper Dmin --", NULL,
                msg_buf, " OK ");
            if (msg_option == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            }
        } else if (option == 3) {
            char msg_buf[256];
            sprintf(msg_buf,
                "Maximum density of the paper,\n"
                "used as the upper bound for\n"
                "measurements.\n"
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
                "Print Exposure Value",
                "Print exposure value used for\n"
                "the step wedge exposure being\n"
                "measured.\n",
                "", &value_sel, 0, 999, 3, "") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                calibration_pev = value_sel;
            }
        } else if (option >= 5 && option < 5 + wedge->step_count) {
            int step_index = option - 5;

            /* Constrain the input based on Dmin and Dmax */
            uint16_t min_value = lroundf(paper_dmin * 100);
            uint16_t max_value = lroundf(paper_dmax * 100);

            if (dens_data->reading != UINT16_MAX) {
                if (dens_data->reading <= min_value) {
                    patch_density[step_index] = paper_dmin;
                } else if (dens_data->reading >= max_value) {
                    patch_density[step_index] = paper_dmax;
                } else {
                    patch_density[step_index] = (float)dens_data->reading / 100.0F;
                }

                option++;
            } else {
                uint8_t patch_option;
                char patch_title_buf[32];
                char patch_buf[128];
                sprintf(patch_title_buf, "Step %d", step_index + 1);
                sprintf(patch_buf,
                    "\nMeasured density at patch %d\n"
                    "of the exposed paper.\n",
                    step_index + 1);

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

                patch_option = display_input_value_f16_data_cb(
                    patch_title_buf, patch_buf,
                    "D=", &value_sel, min_value, max_value, 1, 2, "",
                    menu_paper_densitometer_data_callback, dens_data);
                densitometer_disable();
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
            }
        } else if (option == 5 + wedge->step_count) {
            menu_result_t val_result;
            log_i("Calculate profile");

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
                log_i("Validation failed");
                continue;
            } else if (val_result == MENU_TIMEOUT) {
                menu_result = MENU_TIMEOUT;
                break;
            }
            log_i("Calibration parameters validated");

            /* Calculate the profile */
            val_result = menu_paper_profile_calibrate_grade_calculate(title, &params, paper_grade);
            if (val_result == MENU_OK || val_result == MENU_SAVE) {
                log_i("Calculation accepted");
                menu_result = MENU_SAVE;
                break;
            } else if (val_result == MENU_CANCEL) {
                log_i("Calculation failed");
                continue;
            } else if (val_result == MENU_TIMEOUT) {
                menu_result = MENU_TIMEOUT;
                break;
            }
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }

    } while (option > 0 && menu_result != MENU_TIMEOUT);

    densitometer_disable();
    vPortFree(buf);
    vPortFree(patch_density);
    vPortFree(wedge);

    return menu_result;
}

menu_result_t menu_paper_profile_calibrate_test_strip(state_controller_t *controller, const char *title, paper_profile_grade_t *paper_grade, float paper_dmin, float paper_dmax, menu_paper_callback_data_t *dens_data)
{
    constexpr int MAX_STEPS = 7;
    menu_result_t menu_result = MENU_OK;
    char *buf = NULL;
    uint8_t paper_param = 0;
    float *patch_density = NULL;
    uint8_t option = 1;
    uint32_t calibration_pev = 0;
    uint8_t patch_count;
    exposure_adjustment_increment_t patch_increment;

    const float display_dmin = paper_dmin;
    const float display_dmax = paper_dmax;

    if (!is_valid_number(paper_dmin)) { paper_dmin = 0.0F; }
    if (!is_valid_number(paper_dmax)) { paper_dmax = 0.0F; }

    const exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);
    calibration_pev = exposure_get_calibration_pev(exposure_state);
    if (calibration_pev > 999) {
        calibration_pev = 0;
    }

    switch (settings_get_teststrip_patches()) {
    case TESTSTRIP_PATCHES_5:
        patch_count = 5;
        break;
    case TESTSTRIP_PATCHES_7:
    default:
        patch_count = 7;
        break;
    }

    patch_increment = exposure_adj_increment_get(exposure_state);

    /* Allocate an array for the patch density measurements */
    patch_density = pvPortMalloc(sizeof(float) * MAX_STEPS);
    if (!patch_density) {
        return MENU_OK;
    }

    /* Allocate a buffer for the menu text */
    buf = pvPortMalloc((MAX_STEPS * (sizeof(char) * 33)) + (7 * 33));
    if (!buf) {
        vPortFree(patch_density);
        return MENU_OK;
    }

    /* Initialize the array of patch density measurements */
    for (uint32_t i = 0; i < MAX_STEPS; i++) {
        patch_density[i] = NAN;
    }

    dens_data->min_option = 7;
    dens_data->max_option = (7 + patch_count) - 1;
    dens_data->alt_option = UINT8_MAX;

    do {
        size_t offset = 0;
        const char *patch_increment_name;
        float patch_increment_value;

        offset += menu_build_padded_str_row(buf + offset, "Profile parameter", PAPER_PARAM_NAMES[paper_param]);

        if (isnormal(display_dmin) && display_dmin > 0.0F) {
            offset += sprintf(buf + offset,
                "Paper Dmin              {D=%0.02f}\n",
                display_dmin);
        } else {
            offset += sprintf(buf + offset,
                "Paper Dmin              {------}\n");
        }

        if (isnormal(display_dmax) && display_dmax > 0.0F) {
            offset += sprintf(buf + offset,
                "Paper Dmax              {D=%0.02f}\n",
                display_dmax);
        } else {
            offset += sprintf(buf + offset,
                "Paper Dmax              {------}\n");
        }

        offset += menu_build_padded_format_row(buf + offset, "Test Strip Size", "%d patches", patch_count);

        patch_increment_name = exposure_adjustment_increment_name(patch_increment);
        patch_increment_value = exposure_adjustment_increment_value(patch_increment);
        offset += menu_build_padded_format_row(buf + offset, "Exposure Increment", "%s stop", patch_increment_name);

        if (calibration_pev > 0) {
            offset += sprintf(buf + offset,
                "Center Exposure Value      [%3lu]\n", calibration_pev);
        } else {
            offset += sprintf(buf + offset,
                "Center Exposure Value      [---]\n");
        }

        for (uint8_t i = 0; i < patch_count; i++) {
            char prefix;
            const uint8_t num = abs(i - (patch_count / 2));
            if (i < (patch_count / 2)) {
                prefix = '-';
            } else if (i > (patch_count / 2)) {
                prefix = '+';
            } else {
                prefix = ' ';
            }

            if (is_valid_number(patch_density[i]) && patch_density[i] >= paper_dmin) {
                offset += sprintf(buf + offset,
                    "Patch %c%d                [D=%0.02f]\n",
                    prefix, num, patch_density[i]);
            } else {
                offset += sprintf(buf + offset,
                    "Patch %c%d                [------]\n",
                    prefix, num);
            }
        }

        sprintf(buf + offset, "*** Calculate Exp Point ***");

        option = display_selection_list_cb(title, option, buf,
            menu_paper_densitometer_input_poll_callback, dens_data);

        if (option == 1) {
            char list_buf[256];
            sprintf(list_buf,
                "Base exposure (%s)\n"
                "Speed point   (%s)\n"
                "Dark exposure (%s) ",
                PAPER_PARAM_NAMES[0], PAPER_PARAM_NAMES[1], PAPER_PARAM_NAMES[2]);

            uint8_t sub_option = display_selection_list("Parameter to Measure", paper_param + 1, list_buf);
            if (sub_option == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else if (sub_option != 0) {
                paper_param = sub_option - 1;
            }
        } else if (option == 2) {
            char msg_buf[256];
            sprintf(msg_buf,
                "Minimum density of the paper,\n"
                "used as the lower bound for\n"
                "measurements.\n"
                "D=%0.02f\n",
                paper_dmin);

            uint8_t msg_option = display_message("-- Paper Dmin --", NULL,
                msg_buf, " OK ");
            if (msg_option == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            }
        } else if (option == 3) {
            char msg_buf[256];
            sprintf(msg_buf,
                "Maximum density of the paper,\n"
                "used as the upper bound for\n"
                "measurements.\n"
                "D=%0.02f\n",
                paper_dmax);

            uint8_t msg_option = display_message("-- Paper Dmax --", NULL,
                msg_buf, " OK ");
            if (msg_option == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            }
        } else if (option == 4) {
            if (patch_count == 5) {
                /* Shift all readings down by one and NAN the start and end */
                patch_density[5] = patch_density[4];
                patch_density[4] = patch_density[3];
                patch_density[3] = patch_density[2];
                patch_density[2] = patch_density[1];
                patch_density[1] = patch_density[0];
                patch_density[0] = NAN;
                patch_density[6] = NAN;
                patch_count = 7;
            } else { /* patch_count == 7 */
                /* Shift all readings up by one and NAN the end */
                patch_density[0] = patch_density[1];
                patch_density[1] = patch_density[2];
                patch_density[2] = patch_density[3];
                patch_density[3] = patch_density[4];
                patch_density[4] = patch_density[5];
                patch_density[5] = NAN;
                patch_density[6] = NAN;
                patch_count = 5;
            }
        } else if (option == 5) {
            if (menu_settings_test_strip_step_size("Exposure Increment", &patch_increment) == MENU_TIMEOUT) {
                menu_result = MENU_TIMEOUT;
            }
        } else if (option == 6) {
            uint16_t value_sel = calibration_pev;
            if (display_input_value_u16(
                "Center Exposure Value",
                "Print exposure value used for\n"
                "the center of the test strip\n"
                "being measured.\n",
                "", &value_sel, 0, 999, 3, "") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                calibration_pev = value_sel;
            }
        } else if (option >= 7 && option < 7 + patch_count) {
            int patch_index = option - 7;

            /* Constrain the input based on Dmin and Dmax */
            uint16_t min_value = lroundf(paper_dmin * 100);
            uint16_t max_value = lroundf(paper_dmax * 100);

            if (dens_data->reading != UINT16_MAX) {
                if (dens_data->reading <= min_value) {
                    patch_density[patch_index] = paper_dmin;
                } else if (dens_data->reading >= max_value) {
                    patch_density[patch_index] = paper_dmax;
                } else {
                    patch_density[patch_index] = (float)dens_data->reading / 100.0F;
                }

                option++;
            } else {
                uint8_t patch_option;
                char patch_title_buf[32];
                char patch_buf[128];

                char num_prefix;
                const uint8_t num = abs(patch_index - (patch_count / 2));
                if (patch_index < (patch_count / 2)) {
                    num_prefix = '-';
                } else if (patch_index > (patch_count / 2)) {
                    num_prefix = '+';
                } else {
                    num_prefix = ' ';
                }

                if (num_prefix == ' ') {
                    sprintf(patch_title_buf, "Center Patch");
                    sprintf(patch_buf,
                        "\nMeasured density at the\n"
                        "center patch of the exposed\n"
                        "test strip.\n");
                } else {
                    sprintf(patch_title_buf, "Patch %c%d", num_prefix, num);
                    sprintf(patch_buf,
                        "\nMeasured density at the\n"
                        "%c%d patch of the exposed\n"
                        "test strip.\n",
                        num_prefix, num);
                }

                uint16_t value_sel = 0;
                if (isnormal(patch_density[patch_index]) && patch_density[patch_index] > 0.0F) {
                    value_sel = lroundf(patch_density[patch_index] * 100);
                }
                if (value_sel < min_value) {
                    value_sel = min_value;
                }
                if (value_sel > max_value) {
                    value_sel = max_value;
                }

                patch_option = display_input_value_f16_data_cb(
                    patch_title_buf, patch_buf,
                    "D=", &value_sel, min_value, max_value, 1, 2, "",
                    menu_paper_densitometer_data_callback, dens_data);
                densitometer_disable();
                if (patch_option == 1) {
                    if (value_sel <= min_value) {
                        patch_density[patch_index] = paper_dmin;
                    } else if (value_sel >= max_value) {
                        patch_density[patch_index] = paper_dmax;
                    } else {
                        patch_density[patch_index] = (float)value_sel / 100.0F;
                    }
                } else if (patch_option == UINT8_MAX) {
                    menu_result = MENU_TIMEOUT;
                }
            }
        } else if (option == 7 + patch_count) {
            menu_result_t val_result;
            log_i("Calculate profile");

            /* Collect the calibration parameters */
            strip_calibration_params_t params = {
                .paper_param = paper_param,
                .paper_dmin = paper_dmin,
                .paper_dmax = paper_dmax,
                .calibration_pev = calibration_pev,
                .patch_count = patch_count,
                .patch_increment = patch_increment_value,
                .patch_density = patch_density
            };

            /* Validate the parameters */
            val_result = menu_paper_profile_calibrate_test_strip_validate(&params);
            if (val_result == MENU_CANCEL) {
                log_i("Validation failed");
                continue;
            } else if (val_result == MENU_TIMEOUT) {
                menu_result = MENU_TIMEOUT;
                break;
            }
            log_i("Calibration parameters validated");

            /* Calculate the profile */
            val_result = menu_paper_profile_calibrate_test_strip_calculate(title, &params, paper_grade);
            if (val_result == MENU_OK || val_result == MENU_SAVE) {
                log_i("Calculation accepted");
                menu_result = MENU_SAVE;
                break;
            } else if (val_result == MENU_CANCEL) {
                log_i("Calculation failed");
                continue;
            } else if (val_result == MENU_TIMEOUT) {
                menu_result = MENU_TIMEOUT;
                break;
            }
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }

    } while (option > 0 && menu_result != MENU_TIMEOUT);

    densitometer_disable();
    vPortFree(buf);
    vPortFree(patch_density);

    return menu_result;
}

uint8_t menu_paper_densitometer_input_poll_callback(uint8_t current_pos, uint8_t event_action, void *user_data)
{
    menu_paper_callback_data_t *data = (menu_paper_callback_data_t *)user_data;

    if ((current_pos >= data->min_option && current_pos <= data->max_option) || current_pos == data->alt_option) {
        densitometer_enable(DENSITOMETER_MODE_REFLECTION);
    } else {
        densitometer_disable();
    }

    data->reading = menu_paper_densitometer_callback_impl(event_action, data);;

    if (data->reading != UINT16_MAX) {
        event_action = U8X8_MSG_GPIO_MENU_SELECT;
    }

    return event_action;
}

uint16_t menu_paper_densitometer_data_callback(uint8_t event_action, void *user_data)
{
    menu_paper_callback_data_t *data = (menu_paper_callback_data_t *)user_data;

    densitometer_enable(DENSITOMETER_MODE_REFLECTION);
    return menu_paper_densitometer_callback_impl(event_action, data);
}

uint16_t menu_paper_densitometer_callback_impl(uint8_t event_action, menu_paper_callback_data_t *data)
{
    if (event_action == 5 /*U8X8_MSG_GPIO_MENU_STICK_BTN*/) {
        densitometer_take_reading();
    }

    densitometer_reading_t reading;
    if (densitometer_get_reading(&reading)) {
        if (!isnan(reading.visual) && (fpclassify(reading.visual) == FP_ZERO || reading.visual > 0.0F)) {
            return lroundf(reading.visual * 100);
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
    float last_patch = NAN;
    uint32_t patch_count = 0;
    bool valid_sequence = true;
    for (uint32_t i = 0; i < params->wedge->step_count; i++) {
        if (!is_valid_number(params->patch_density[i])) {
            continue;
        }
        if (isnan(patch_min) || params->patch_density[i] < patch_min) {
            patch_min = params->patch_density[i];
        }
        if (isnan(patch_max) || params->patch_density[i] > patch_max) {
            patch_max = params->patch_density[i];
        }
        /*
         * Check measurement order to make sure it is descending.
         * Repeated measurements can be equal, which will be handled in
         * the interpolation code.
         */
        if (isnanf(last_patch) || params->patch_density[i] <= last_patch) {
            last_patch = params->patch_density[i];
        } else {
            valid_sequence = false;
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

    if (!valid_sequence) {
        uint8_t msg_option = display_message(
                "Invalid Measurements\n",
                NULL,
                "Paper patch measurements must\n"
                "be in descending order.\n",
                " OK ");
        if (msg_option == UINT8_MAX) {
            return MENU_TIMEOUT;
        }

        return MENU_CANCEL;
    }

    /*
     * Make sure the min value crosses Dmin+0.04 and
     * the max value crosses Dnet*0.90.
     * If the value range does not meet these requirements,
     * we can still estimate the ends, but the user should
     * be made aware.
     */
    const float patch_min_crossing = params->paper_dmin + 0.04F;
    const float paper_dnet = params->paper_dmax - params->paper_dmin;
    const float patch_max_crossing = params->paper_dmin + (paper_dnet * 0.90F);

    if (patch_min > patch_min_crossing || patch_max < patch_max_crossing) {
        char buf[128];
        sprintf(buf,
            "Paper patch measurements do not\n"
            "contain both D=%0.02f and D=%0.02f.\n"
            "Missing end values will\n"
            "be extrapolated.",
            patch_min_crossing, patch_max_crossing);

        uint8_t msg_option = display_message(
                "Insufficient Range\n",
                NULL, buf, " OK \n Cancel ");
        if (msg_option == 1) {
            return MENU_OK;
        } else if (msg_option == 2) {
            return MENU_CANCEL;
        } else if (msg_option == UINT8_MAX) {
            return MENU_TIMEOUT;
        }
    }

    return MENU_OK;
}

menu_result_t menu_paper_profile_calibrate_grade_calculate(const char *title, const wedge_calibration_params_t *params, paper_profile_grade_t *paper_grade)
{
    char buf[128];
    uint8_t msg_option = 0;
    size_t num_patches = 0;
    float32_t pev_min = NAN;
    float32_t pev_max = NAN;
    float32_t patch_d_min = NAN;
    float32_t patch_d_max = NAN;
    bool has_predicted_range = false;
    float32_t predicted_min = NAN;
    float32_t predicted_max = NAN;
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
    log_i("Found %d patches with valid measurements", num_patches);

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

    /* Output Y-axis is the corresponding calculated density values */
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
        for (int i = (int)params->wedge->step_count - 1; i >= 0; --i) {
            if (is_valid_number(params->patch_density[i])) {
                /* Overwrite repeated values */
                if (p > 0 && params->patch_density[i] == y_density_f32[p - 1]) {
                    p--;
                    num_patches--;
                }
                x_pev_f32[p] = roundf((float)params->calibration_pev - (step_wedge_get_density(params->wedge, i) * 100.0F));
                y_density_f32[p] = params->patch_density[i];
#if 0
                log_i("[i=%lu,p=%lu] %0.02f = %lu - (%0.02f * 100)",
                    i, p,
                    x_pev_f32[p], params->calibration_pev, step_wedge_get_density(params->wedge, i));
#endif
                p++;
            }
        }
        log_i("Using %d patches after removing duplicates", num_patches);

        /* Declare the reference point density values */
        const float32_t Ht_D = params->paper_dmin + 0.04F; /* paper base + 0.04 */
        const float32_t Hm_D = params->paper_dmin + 0.60F; /* paper base + 0.60 */
        const float32_t Hs_D = params->paper_dmin + ((params->paper_dmax - params->paper_dmin) * 0.90F); /* 90% of Dnet */

        /* Calculate range of input values */
        arm_min_f32(x_pev_f32, num_patches, &pev_min, NULL);
        arm_max_f32(x_pev_f32, num_patches, &pev_max, NULL);
        arm_min_f32(y_density_f32, num_patches, &patch_d_min, NULL);
        arm_max_f32(y_density_f32, num_patches, &patch_d_max, NULL);

        if (!is_valid_number(pev_min) || !is_valid_number(pev_max) || pev_min >= pev_max) {
            log_w("Cannot find min/max of interpolation range");
            break;
        }

        if (!is_valid_number(patch_d_min) || !is_valid_number(patch_d_max) || patch_d_min >= patch_d_max) {
            log_w("Cannot find min/max of density range");
            break;
        }

        if (patch_d_min > Ht_D || patch_d_max < Hs_D) {
            log_i("Expanding range due to insufficient patch values");

            /* Input X-axis containing the profile density values to find */
            float32_t xq_density_f32[3] = {0};

            /* Output Y-axis corresponding to the calculated PEV values */
            float32_t yq_pev_f32[3] = {0};

            float32_t yq_pev_min = NAN;
            float32_t yq_pev_max = NAN;

            /* Initialize the cubic spline interpolation (Density -> PEV) */
            arm_spline_init_f32(&S, ARM_SPLINE_NATURAL,
                y_density_f32, x_pev_f32, num_patches,
                coeffs, temp_buffer);

            xq_density_f32[0] = Ht_D;
            xq_density_f32[1] = Hm_D;
            xq_density_f32[2] = Hs_D;

            arm_spline_f32(&S, xq_density_f32, yq_pev_f32, 3);

            log_i("Predicted range:");
            log_i("Ht: D=%0.02f, PEV=%f", Ht_D, yq_pev_f32[0]);
            log_i("Hm: D=%0.02f, PEV=%f", Hm_D, yq_pev_f32[1]);
            log_i("Hs: D=%0.02f, PEV=%f", Hs_D, yq_pev_f32[2]);

            arm_min_f32(yq_pev_f32, 3, &yq_pev_min, NULL);
            arm_max_f32(yq_pev_f32, 3, &yq_pev_max, NULL);

            if (is_valid_number(yq_pev_min) && is_valid_number(yq_pev_max) && yq_pev_min < yq_pev_max) {
                predicted_min = floorf(yq_pev_min);
                predicted_max = ceilf(yq_pev_max);
                has_predicted_range = true;
                log_i("PEV min=%f, max=%f", lroundf(predicted_min), lroundf(predicted_max));
            }
        }

        /* Initialize the cubic spline interpolation (PEV -> Density) */
        arm_spline_init_f32(&S, ARM_SPLINE_NATURAL,
            x_pev_f32, y_density_f32, num_patches,
            coeffs, temp_buffer);

        /* Expand the range based on predicted values, if necessary */
        if (has_predicted_range) {
            log_i("Expanding range based on predictions");
            /* This adds a little buffer beyond the prediction range */
            if (predicted_min < pev_min) {
                pev_min = roundf(predicted_min) - 10.0F;
            }
            if (predicted_max > pev_max) {
                pev_max = roundf(predicted_max) + 10.0F;
            }
        }

        num_output = abs(lroundf(pev_max) - lroundf(pev_min));
        if (num_output > 500) {
            log_w("Interpolation range unusually large: %d", num_output);
            break;
        }
        float min_xq = MIN(pev_min, pev_max);
        log_i("Interpolating curve from PEV=%ld to %ld (%d steps)",
            lroundf(pev_min), lroundf(pev_max), num_output);

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

        /* Iterate across the output to find PEV values for each reference point */
        float32_t Ht_x = NAN;
        float32_t Ht_y = NAN;
        float32_t Hm_x = NAN;
        float32_t Hm_y = NAN;
        float32_t Hs_x = NAN;
        float32_t Hs_y = NAN;
        for (uint32_t i = 0; i < num_output; i++) {
            if (isnan(Ht_y) || fabsf(Ht_D - yq_density_f32[i]) < fabsf(Ht_D - Ht_y)) {
                Ht_y = yq_density_f32[i];
                Ht_x = xq_pev_f32[i];
            }
            if (isnan(Hm_y) || fabsf(Hm_D - yq_density_f32[i]) < fabsf(Hm_D - Hm_y)) {
                Hm_y = yq_density_f32[i];
                Hm_x = xq_pev_f32[i];
            }
            if (isnan(Hs_y) || fabsf(Hs_D - yq_density_f32[i]) < fabsf(Hs_D - Hs_y)) {
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
        log_i("Ht: PEV=%ld, D=%0.02f (%0.02f)", Ht_lev100, Ht_y, Ht_D);
        log_i("Hm: PEV=%ld, D=%0.02f (%0.02f)", Hm_lev100, Hm_y, Hm_D);
        log_i("Hs: PEV=%ld, D=%0.02f (%0.02f)", Hs_lev100, Hs_y, Hs_D);
        log_i("ISO(R) = %ld", iso_r);

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

        log_i("Preparing results display graph");

        /* Reallocate the result arrays for the display graph */
        size_t num_graph = 127;
        xq_pev_f32 = pvPortMalloc(sizeof(float32_t) * num_graph);
        if (!xq_pev_f32) { break; }

        yq_density_f32 = pvPortMalloc(sizeof(float32_t) * num_graph);
        if (!yq_density_f32) { break; }

        graph_points = pvPortMalloc(sizeof(uint8_t) * num_graph);
        if (!graph_points) { break; }

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
            if (isnan(yq_density_min) || yq_density_f32[i] < yq_density_min) {
                yq_density_min = yq_density_f32[i];
            }
            if (isnan(yq_density_max) || yq_density_f32[i] > yq_density_max) {
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

        log_i("Showing results display graph");

        /* Format the message text */
        if (Ht_lev100 > 0 && Hm_lev100 > 0 && Hs_lev100 > 0) {
            sprintf(buf,
                "Base exp  [%3lu] \n"
                "Speed pt  [%3lu] \n"
                "Dark exp  [%3lu] \n"
                "ISO Range [%3lu] ",
                Ht_lev100, Hm_lev100, Hs_lev100, iso_r);
        } else {
            sprintf(buf,
                "Base exp  [---] \n"
                "Speed pt  [---] \n"
                "Dark exp  [---] \n"
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
            "Unable to calculate the\n"
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
            log_i("Updating all grade values");
            paper_grade->ht_lev100 = Ht_lev100;
            paper_grade->hm_lev100 = Hm_lev100;
            paper_grade->hs_lev100 = Hs_lev100;
        } else {
            log_i("Updating only ISO(R) due to low PEV numbers");
            paper_grade->hs_lev100 = paper_grade->ht_lev100 + iso_r;
        }
        log_i("Calibration parameters updated");
        return MENU_OK;
    } else if (msg_option == UINT8_MAX) {
        return MENU_TIMEOUT;
    } else {
        log_i("Calibration parameters rejected");
        return MENU_CANCEL;
    }
}

static float paper_param_density_value(const strip_calibration_params_t *params)
{
    switch (params->paper_param) {
    case 0:
        return params->paper_dmin + 0.04F;
    case 1:
        return params->paper_dmin + 0.60F;
    case 2:
        return params->paper_dmin + ((params->paper_dmax - params->paper_dmin) * 0.90F);
    default:
        return NAN;
    }
}

menu_result_t menu_paper_profile_calibrate_test_strip_validate(const strip_calibration_params_t *params)
{
    /* Validate arguments that should never be in question */
    if (!params || !params->patch_density || params->patch_count < 3
        || !is_valid_number(params->patch_increment) || params->patch_increment <= 0.0F) {
        return MENU_CANCEL;
    }

    /* Validate the PEV, Dmin, and Dmax values */
    if (!(is_valid_number(params->paper_dmin) && is_valid_number(params->paper_dmax)
        && (params->paper_dmin < params->paper_dmax)
        && (params->calibration_pev > 0) && (params->calibration_pev <= 999))) {
        uint8_t msg_option = display_message(
                "Invalid Properties\n",
                NULL,
                "Test strip measurement\n"
                "properties are not valid.\n",
                " OK ");
        if (msg_option == UINT8_MAX) {
            return MENU_TIMEOUT;
        }

        return MENU_CANCEL;
    }

    /* Find the min and max patch values so they can be validated */
    float patch_min = NAN;
    float patch_max = NAN;
    float last_patch = NAN;
    uint32_t patch_count = 0;
    bool valid_sequence = true;
    for (uint32_t i = 0; i < params->patch_count; i++) {
        if (!is_valid_number(params->patch_density[i])) {
            continue;
        }
        if (isnan(patch_min) || params->patch_density[i] < patch_min) {
            patch_min = params->patch_density[i];
        }
        if (isnan(patch_max) || params->patch_density[i] > patch_max) {
            patch_max = params->patch_density[i];
        }
        /*
         * Check measurement order to make sure it is ascending.
         * Repeated measurements can be equal, which will be handled in
         * the interpolation code.
         */
        if (isnanf(last_patch) || params->patch_density[i] >= last_patch) {
            last_patch = params->patch_density[i];
        } else {
            valid_sequence = false;
        }
        patch_count++;
    }

    /* Make sure we actually found min and max patch values */
    if (!is_valid_number(patch_min) || !is_valid_number(patch_max)
        || fabsf(patch_max - patch_min) < 0.01F) {
        uint8_t msg_option = display_message(
                "Invalid Measurements\n",
                NULL,
                "Test strip measurements are\n"
                "missing or invalid.\n",
                " OK ");
        if (msg_option == UINT8_MAX) {
            return MENU_TIMEOUT;
        }

        return MENU_CANCEL;
    }

    /* Make sure we have enough patches */
    if (patch_count < 2) {
        uint8_t msg_option = display_message(
                "Invalid Measurements\n",
                NULL,
                "Must have at least two"
                "test strip measurements.\n",
                " OK ");
        if (msg_option == UINT8_MAX) {
            return MENU_TIMEOUT;
        }

        return MENU_CANCEL;
    }

    if (!valid_sequence) {
        uint8_t msg_option = display_message(
                "Invalid Measurements\n",
                NULL,
                "Test strip measurements must\n"
                "be in ascending order.\n",
                " OK ");
        if (msg_option == UINT8_MAX) {
            return MENU_TIMEOUT;
        }

        return MENU_CANCEL;
    }

    /*
     * Make sure patch densities cross one of the three
     * values necessary to define the paper grade profile.
     */
    const float param_density = paper_param_density_value(params);
    const bool crosses_param = patch_min <= param_density && param_density <= patch_max;

    if (!crosses_param) {
        char buf[128];
        uint8_t msg_option;

        if ((param_density < patch_min && patch_min - param_density > 0.60F)
            || (param_density > patch_max && param_density - patch_max > 0.60F)) {
            sprintf(buf,
                "Test strip range is too far\n"
                "from %s (D=%0.02f).\n"
                "Value cannot be be extrapolated.",
                PAPER_PARAM_NAMES[params->paper_param], param_density);
            msg_option = display_message(
                "Insufficient Range\n",
                NULL, buf, " OK ");
            if (msg_option == 1) { msg_option = 2; }
        } else {
            sprintf(buf,
                "Test strip range does not\n"
                "contain %s (D=%0.02f).\n"
                "Value will be extrapolated.",
                PAPER_PARAM_NAMES[params->paper_param], param_density);
            msg_option = display_message(
                "Insufficient Range\n",
                NULL, buf, " OK \n Cancel ");
        }

        if (msg_option == 1) {
            return MENU_OK;
        } else if (msg_option == 2) {
            return MENU_CANCEL;
        } else if (msg_option == UINT8_MAX) {
            return MENU_TIMEOUT;
        }
    }

    return MENU_OK;
}

menu_result_t menu_paper_profile_calibrate_test_strip_calculate(const char *title, const strip_calibration_params_t *params, paper_profile_grade_t *paper_grade)
{
    uint8_t msg_option = 0;
    size_t num_patches = 0;
    float32_t pev_min = NAN;
    float32_t pev_max = NAN;
    float32_t patch_d_min = NAN;
    float32_t patch_d_max = NAN;
    int32_t H_lev100 = 0;

    /* Validate arguments that should never be in question */
    if (!params || !params->patch_density || params->patch_count < 3
        || !is_valid_number(params->patch_increment) || params->patch_increment <= 0.0F) {
        return MENU_CANCEL;
    }

    /* Count the number of patches with valid data */
    for (uint32_t i = 0; i < params->patch_count; i++) {
        if (is_valid_number(params->patch_density[i])) {
            num_patches++;
        }
    }
    log_i("Found %d patches with valid measurements", num_patches);

    /* X-axis is the measured density values from the test strip */
    float32_t *x_density_f32 = NULL;

    /* Y-axis is the calculated PEV values for the test strip exposure */
    float32_t *y_pev_f32 = NULL;

    /* Coefficients array used for the interpolation */
    float32_t *coeffs = NULL;

    /* Buffer array used for the interpolation */
    float32_t *temp_buffer = NULL;

    /* Output X-axis is the density value being calculated */
    float32_t xq_density_f32 = NAN;

    /* Output Y-axis is the PEV value corresponding to that density value */
    float32_t yq_pev_f32 = NAN;

    do {
        arm_spline_instance_f32 S;

        /* Allocate the arrays needed for the cubic spline interpolation */
        x_density_f32 = pvPortMalloc(sizeof(float32_t) * num_patches);
        if (!x_density_f32) { break; }

        y_pev_f32 = pvPortMalloc(sizeof(float32_t) * num_patches);
        if (!y_pev_f32) { break; }

        coeffs = pvPortMalloc(sizeof(float32_t) * (3 * (num_patches - 1))); /* 3*(n-1) */
        if (!coeffs) { break; }

        temp_buffer = pvPortMalloc(sizeof(float32_t) * (num_patches + num_patches - 1)); /* n+n-1 */
        if (!temp_buffer) { break; }

        /* Apply wedge data to populate PEV and density values for each patch */

        const float base_lux_secs = powf(10.0F, (float)params->calibration_pev / 100.0F);

        uint32_t p = 0;
        for (uint8_t i = 0; i < params->patch_count; i++) {
            if (is_valid_number(params->patch_density[i])) {
                if (params->paper_param == 0 || params->paper_param == 1) {
                    /* Base and speed points use higher values in case of repeat */
                    if (p > 0 && params->patch_density[i] == x_density_f32[p - 1]) {
                        p--;
                        num_patches--;
                    }
                } else if (params->paper_param == 2) {
                    /* Dark point uses lower values in case of repeat */
                    if (p > 0 && params->patch_density[i] == x_density_f32[p - 1]) {
                        num_patches--;
                        continue;
                    }
                }

                const int pev_offset = i - (params->patch_count / 2);
                const float patch_lux_secs = base_lux_secs * powf(2.0F, (float)pev_offset * params->patch_increment);
                const float patch_pev = log10f(patch_lux_secs) * 100;

                x_density_f32[p] = params->patch_density[i];
                y_pev_f32[p] = roundf(patch_pev);
#if 0
                log_i("[i=%lu,p=%lu] D=%0.02f, PEV=%d",
                    i, p, x_density_f32[p], (int)y_pev_f32[p]);
#endif
                p++;
            }
        }
        log_i("Using %d patches of the test strip", num_patches);

        /* Calculate range of input values */
        arm_min_f32(x_density_f32, num_patches, &patch_d_min, NULL);
        arm_max_f32(x_density_f32, num_patches, &patch_d_max, NULL);
        arm_min_f32(y_pev_f32, num_patches, &pev_min, NULL);
        arm_max_f32(y_pev_f32, num_patches, &pev_max, NULL);

        if (!is_valid_number(pev_min) || !is_valid_number(pev_max) || pev_min >= pev_max) {
            log_w("Cannot find min/max of interpolation range");
            break;
        }

        if (!is_valid_number(patch_d_min) || !is_valid_number(patch_d_max) || patch_d_min >= patch_d_max) {
            log_w("Cannot find min/max of density range");
            break;
        }

        /* Initialize the cubic spline interpolation (Density -> PEV) */
        arm_spline_init_f32(&S, ARM_SPLINE_NATURAL,
            x_density_f32, y_pev_f32, num_patches,
            coeffs, temp_buffer);

        xq_density_f32 = paper_param_density_value(params);

        /* Run the cubic spline interpolation to get the characteristic curve */
        arm_spline_f32(&S, &xq_density_f32, &yq_pev_f32, 1);

        /* Assign the result, keeping signs */
        H_lev100 = lroundf(yq_pev_f32);

        /* Log the results */
        log_i("H(%s): PEV=%ld, D=%0.02f", PAPER_PARAM_NAMES[params->paper_param], H_lev100, xq_density_f32);

    } while (0);

    /* Free dynamically allocated arrays */
    vPortFree(x_density_f32);
    vPortFree(y_pev_f32);
    vPortFree(coeffs);
    vPortFree(temp_buffer);

    if (H_lev100 <= 0) {
        msg_option = display_message(
            "Calculation Error\n",
            NULL,
            "Unable to calculate the\n"
            "print exposure point.\n",
            " OK ");
        if (msg_option == UINT8_MAX) {
            return MENU_TIMEOUT;
        } else {
            return MENU_CANCEL;
        }
    }

    char buf[192];
    sprintf(buf,
        "\nExposure value for %s\n"
        "(D=%0.02f) has been calculated\n"
        "from the test strip to be %ld.",
        PAPER_PARAM_NAMES[params->paper_param],
        paper_param_density_value(params),
        H_lev100);

    msg_option = display_message(
        title,
        NULL, buf, " OK \n Cancel ");
    if (msg_option == 1) {
        if (params->paper_param == 0) {
            paper_grade->ht_lev100 = H_lev100;
        } else if (params->paper_param == 1) {
            paper_grade->hm_lev100 = H_lev100;
        } else if (params->paper_param == 2) {
            paper_grade->hs_lev100 = H_lev100;
        }
        return MENU_OK;
    } else if (msg_option == 2) {
        return MENU_CANCEL;
    } else if (msg_option == UINT8_MAX) {
        return MENU_TIMEOUT;
    } else {
        return MENU_OK;
    }
}
