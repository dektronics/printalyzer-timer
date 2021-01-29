#include "menu_paper.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include <esp_log.h>

#include "display.h"
#include "keypad.h"
#include "settings.h"
#include "paper_profile.h"
#include "util.h"

#define MIN(a,b) (((a)<(b))?(a):(b))

static const char *TAG = "menu_paper";

static menu_result_t menu_paper_profile_edit(paper_profile_t *profile, uint8_t index);
static void menu_paper_delete_profile(uint8_t index, size_t profile_count);
static bool menu_paper_profile_delete_prompt(const paper_profile_t *profile, uint8_t index);
static menu_result_t menu_paper_profile_edit_grade(paper_profile_t *profile, uint8_t index, exposure_contrast_grade_t grade);

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
            //TODO Add ability to set from step wedge measurements

            bool show_editor = false;

            if (add_option == 1) {
                show_editor = true;
            } else if (add_option == 2) {
                paper_profile_set_defaults(&working_profile);
                working_profile.name[0] = '\0';
                show_editor = true;
            }

            if (show_editor) {
                menu_result = menu_paper_profile_edit(&working_profile, UINT8_MAX);
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
                menu_result = menu_paper_profile_edit(&working_profile, option - 1);
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
            }
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    vPortFree(profile_list);

    // There are many paths in this menu that can change profile settings,
    // so its easiest to just reload the state controller's active profile
    // whenever exiting from this menu.
    state_controller_reload_paper_profile(controller);

    return menu_result;
}

size_t menu_paper_profile_edit_append_grade(char *str, const paper_profile_t *profile, exposure_contrast_grade_t grade)
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

menu_result_t menu_paper_profile_edit(paper_profile_t *profile, uint8_t index)
{
    char buf_title[32];
    char buf[512];
    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;

    if (index == UINT8_MAX) {
        sprintf(buf_title, "New Paper Profile");
    } else {
        sprintf(buf_title, "Paper Profile %d", index + 1);
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

        offset += menu_paper_profile_edit_append_grade(buf + offset, profile, CONTRAST_GRADE_00);
        offset += menu_paper_profile_edit_append_grade(buf + offset, profile, CONTRAST_GRADE_0);
        offset += menu_paper_profile_edit_append_grade(buf + offset, profile, CONTRAST_GRADE_1);
        offset += menu_paper_profile_edit_append_grade(buf + offset, profile, CONTRAST_GRADE_2);
        offset += menu_paper_profile_edit_append_grade(buf + offset, profile, CONTRAST_GRADE_3);
        offset += menu_paper_profile_edit_append_grade(buf + offset, profile, CONTRAST_GRADE_4);
        offset += menu_paper_profile_edit_append_grade(buf + offset, profile, CONTRAST_GRADE_5);

        if (isnormal(profile->max_net_density) && profile->max_net_density > 0.0F) {
            offset += sprintf(buf + offset, "Max net density           [%01.2f]\n", profile->max_net_density);
        } else {
            offset += sprintf(buf + offset, "Max net density           [----]\n");
        }

        sprintf(buf + offset, "*** Save Changes ***");

        if (index != UINT8_MAX) {
            strcat(buf, "\n*** Delete Profile ***");
        }

        option = display_selection_list(buf_title, option, buf);

        if (option == 1) {
            display_input_text("Enlarger Name", profile->name, 32);
        } else if (option >= 2 && option <= 8) {
            exposure_contrast_grade_t grade;
            switch (option) {
            case 2:
                grade = CONTRAST_GRADE_00;
                break;
            case 3:
                grade = CONTRAST_GRADE_0;
                break;
            case 4:
                grade = CONTRAST_GRADE_1;
                break;
            case 5:
                grade = CONTRAST_GRADE_2;
                break;
            case 6:
                grade = CONTRAST_GRADE_3;
                break;
            case 7:
                grade = CONTRAST_GRADE_4;
                break;
            case 8:
                grade = CONTRAST_GRADE_5;
                break;
            default:
                grade = CONTRAST_GRADE_2;
                break;
            }
            if (menu_paper_profile_edit_grade(profile, index, grade) == MENU_TIMEOUT) {
                menu_result = MENU_TIMEOUT;
            }
        } else if (option == 9) {
            uint16_t value_sel = lroundf(profile->max_net_density * 100);
            if (display_input_value_f16(
                "-- Max Net Density --\n"
                "Maximum density (Dmax) of the\n"
                "paper, measured relative to the\n"
                "paper base (Dmin).\n",
                "", &value_sel, 0, 999, 1, 2, "") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                profile->max_net_density = (float)value_sel / 100.0F;
            }
        } else if (option == 10) {
            ESP_LOGD(TAG, "Save changes from profile editor");
            menu_result = MENU_SAVE;
            break;
        } else if (option == 11) {
            ESP_LOGD(TAG, "Delete profile from profile editor");
            if (menu_paper_profile_delete_prompt(profile, index)) {
                menu_result = MENU_DELETE;
                break;
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

menu_result_t menu_paper_profile_edit_grade(paper_profile_t *profile, uint8_t index, exposure_contrast_grade_t grade)
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
                "*** Accept Changes ***",
                working_grade.ht_lev100, iso_r);
        } else {
            sprintf(buf,
                "Base exposure (Dmin+0.04)  [%3lu]\n"
                "Speed point   (Dmin+0.60)  [%3lu]\n"
                "ISO Range                  [%3lu]\n"
                "*** Accept Changes ***",
                working_grade.ht_lev100, working_grade.hm_lev100, iso_r);
        }
        //TODO Add ability to set from step wedge measurements

        option = display_selection_list(buf_title, option, buf);

        if (option == 1) {
            uint16_t value_sel = working_grade.ht_lev100;
            if (display_input_value_u16(
                "-- Base Exposure --\n"
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
                "-- Speed Point (optional) --\n"
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
                "-- ISO Range --\n"
                "Paper exposure range between the\n"
                "base exposure and an exposure\n"
                "that achieves 90% of Dnet\n",
                "", &value_sel, 1, 999, 3, "") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                working_grade.hs_lev100 = working_grade.ht_lev100 + value_sel;
            }
        } else if (option == 4) {
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
            menu_result = MENU_SAVE;
            break;
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }

    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}
