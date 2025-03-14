#include "menu_step_wedge.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#define LOG_TAG "menu_step_wedge"
#include <elog.h>

#include "settings.h"
#include "step_wedge.h"
#include "display.h"
#include "util.h"
#include "usb_host.h"
#include "densitometer.h"

static int calibration_status(const step_wedge_t *wedge);
static int menu_step_wedge_list_selection();
static menu_result_t menu_step_wedge_calibration(step_wedge_t *wedge);
static uint16_t menu_step_wedge_densitometer_data_callback(uint8_t event_action, void *user_data);

menu_result_t menu_step_wedge()
{
    menu_result_t menu_result = MENU_OK;
    step_wedge_t *wedge = NULL;
    uint8_t option = 1;
    char buf[512];
    bool wedge_changed = false;

    /* Load step wedge configuration */
    if (!settings_get_step_wedge(&wedge)) {
        log_i("No saved step wedge, loading default");
        wedge = step_wedge_create_from_stock(DEFAULT_STOCK_WEDGE_INDEX);
        wedge_changed = true;
        if (!wedge) {
            log_e("Unable to load default wedge");
            return MENU_OK;
        }
    }

    do {
        size_t offset = 0;
        int cal_status = calibration_status(wedge);
        const char *value_str;

        offset += menu_build_padded_str_row(buf, "Name", wedge->name);

        offset += sprintf(buf + offset,
            "Steps                       [%2lu]\n"
            "Base density            [D=%0.02f]\n"
            "Density increment       [D=%0.02f]\n",
            wedge->step_count,
            wedge->base_density,
            wedge->density_increment);

        if (cal_status == 1) {
            value_str = "Calibrated";
        } else if (cal_status == 2) {
            value_str = "Partial";
        } else {
            value_str = "Uncalibrated";
        }
        offset += menu_build_padded_str_row(buf + offset, "Calibration", value_str);

        sprintf(buf + offset,
            "*** Save Changes ***\n"
            "*** Select From List ***");

        option = display_selection_list("Step Wedge Properties", option, buf);

        if (option == 1) {
            display_input_text("Step Wedge Name", wedge->name, PROFILE_NAME_LEN);
        } else if (option == 2) {
            uint8_t value_sel = wedge->step_count;
            if (display_input_value(
                "Step Count",
                "\nNumber of distinct patches\n"
                "on the step wedge.\n",
                "", &value_sel, MIN_STEP_WEDGE_STEP_COUNT, MAX_STEP_WEDGE_STEP_COUNT, 2, " steps") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            }
            if (value_sel != wedge->step_count) {
                log_i("Copying for step count change: %d != %lu", value_sel, wedge->step_count);
                step_wedge_t *updated_wedge = step_wedge_copy(wedge, value_sel);
                if (updated_wedge) {
                    step_wedge_free(wedge);
                    wedge = updated_wedge;
                }
            }
        } else if (option == 3) {
            uint16_t value_sel = lroundf(wedge->base_density * 100);
            if (display_input_value_f16(
                "Base Density",
                "Specified density of the base\n"
                "material of the step wedge.\n",
                "D=", &value_sel, 0, 999, 1, 2, "") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                wedge->base_density = (float)value_sel / 100.0F;
            }
        } else if (option == 4) {
            uint16_t value_sel = lroundf(wedge->density_increment * 100);
            if (display_input_value_f16(
                "Density Increment",
                "Specified density increase of\n"
                "each successive patch of the\n"
                "step wedge.\n",
                "D=", &value_sel, 1, 999, 1, 2, "") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                wedge->density_increment = (float)value_sel / 100.0F;
            }
        } else if (option == 5) {
            menu_result = menu_step_wedge_calibration(wedge);
        } else if (option == 6) {
            if (step_wedge_is_valid(wedge)) {
                if (settings_set_step_wedge(wedge)) {
                    log_i("Step wedge settings saved");
                    break;
                }
            } else {
                log_i("Step wedge properties not valid");
                uint8_t msg_option = display_message(
                        "Step Wedge Invalid",
                        NULL,
                        "Properties must describe a step\n"
                        "wedge with patches that increase\n"
                        "in density.", " OK ");
                if (msg_option == UINT8_MAX) {
                    menu_result = MENU_TIMEOUT;
                }
            }
        } else if (option == 7) {
            int index = menu_step_wedge_list_selection();
            if (index >= 0 && index < step_wedge_stock_count()) {
                log_i("Replacing with stock wedge: %d", index);
                step_wedge_t *updated_wedge = step_wedge_create_from_stock(index);
                if (updated_wedge) {
                    step_wedge_free(wedge);
                    wedge = updated_wedge;
                    option = 1;
                }
            } else if (index == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            }
        } else if (option == 0) {
            /*
             * Compare the updated wedge to the saved one, to avoid needing
             * to dirty-track the whole multi-screen editing process.
             */
            if (!wedge_changed) {
                step_wedge_t *saved_wedge = NULL;
                if (settings_get_step_wedge(&saved_wedge)) {
                    wedge_changed = !step_wedge_compare(wedge, saved_wedge);
                    step_wedge_free(saved_wedge);
                }
            }
            if (wedge_changed) {
                menu_result_t sub_result = menu_confirm_cancel("Step Wedge Properties");
                if (sub_result == MENU_SAVE) {
                    if (step_wedge_is_valid(wedge)) {
                        if (settings_set_step_wedge(wedge)) {
                            log_i("Step wedge settings saved");
                            break;
                        }
                    } else {
                        log_i("Step wedge properties not valid");
                        uint8_t msg_option = display_message(
                                "Step Wedge Invalid",
                                NULL,
                                "Properties must describe a step\n"
                                "wedge with patches that increase\n"
                                "in density.", " OK ");
                        if (msg_option == UINT8_MAX) {
                            menu_result = MENU_TIMEOUT;
                        }
                        option = 1;
                    }
                } else if (sub_result == MENU_OK || sub_result == MENU_TIMEOUT) {
                    menu_result = sub_result;
                    break;
                } else if (sub_result == MENU_CANCEL) {
                    option = 1;
                    continue;
                }
            }
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }

    } while (option > 0 && menu_result != MENU_TIMEOUT);

    step_wedge_free(wedge);

    return menu_result;
}

int calibration_status(const step_wedge_t *wedge)
{
    int valid_count = 0;
    for (size_t i = 0; i < wedge->step_count; i++) {
        if (isnormal(wedge->step_density[i]) || fpclassify(wedge->step_density[i]) == FP_ZERO) {
            valid_count++;
        }
    }

    if (valid_count == wedge->step_count) {
        return 1;
    } else if (valid_count > 0 && valid_count < wedge->step_count) {
        return 2;
    } else {
        return 0;
    }
}

int menu_step_wedge_list_selection()
{
    int option = 1;
    size_t offset = 0;
    char *buf = NULL;
    int count = step_wedge_stock_count();
    buf = pvPortMalloc(count * (sizeof(char) * 32));
    if (!buf) {
        return -1;
    }

    for (int i = 0; i < count; i++) {
        offset += sprintf(buf + offset, "%s (%d-Step)\n",
            step_wedge_stock_name(i),
            step_wedge_stock_step_count(i));
    }
    buf[offset - 1] = '\0';

    option = display_selection_list("Stock Step Wedges", option, buf);
    if (option != UINT8_MAX) {
        option--;
    }

    vPortFree(buf);
    return option;
}

menu_result_t menu_step_wedge_calibration(step_wedge_t *wedge)
{
    menu_result_t menu_result = MENU_OK;
    int option = 1;
    size_t offset = 0;
    char *buf = NULL;
    buf = pvPortMalloc((wedge->step_count * (sizeof(char) * 33)) + 64);
    if (!buf) {
        return MENU_OK;
    }

    float *prev_density = pvPortMalloc(sizeof(float) * wedge->step_count);
    if (!prev_density) {
        vPortFree(buf);
        return MENU_OK;
    }

    for (uint32_t i = 0; i < wedge->step_count; i++) {
        prev_density[i] = wedge->step_density[i];
    }

    do {
        offset = 0;
        for (uint32_t i = 0; i < wedge->step_count; i++) {
            bool is_calibrated;
            if (isnormal(wedge->step_density[i]) || fpclassify(wedge->step_density[i]) == FP_ZERO) {
                is_calibrated = true;
            } else {
                is_calibrated = false;
            }

            offset += sprintf(buf + offset,
                "Step %-2lu                 %cD=%0.02f%c\n",
                i + 1,
                (is_calibrated ? '[' : '{'),
                step_wedge_get_density(wedge, i),
                (is_calibrated ? ']' : '}'));
        }
        sprintf(buf + offset,
            "*** Accept Changes ***\n"
            "*** Reset Values ***");

        if (strlen(wedge->name) > 0) {
            option = display_selection_list(wedge->name, option, buf);
        } else {
            option = display_selection_list("Step Wedge Calibration", option, buf);
        }

        if (option > 0 && option <= wedge->step_count) {
            uint8_t patch_option;
            char patch_title_buf[32];
            char patch_buf[128];
            sprintf(patch_title_buf, "Step %d", option);
            sprintf(patch_buf,
                "\nMeasured density at patch %d\n"
                "of the step wedge.\n",
                option);
            uint16_t value_sel = lroundf(step_wedge_get_density(wedge, option - 1) * 100);

            bool dens_enable = usb_serial_is_attached();
            if (dens_enable) {
                usb_serial_clear_receive_buffer();
            }

            patch_option = display_input_value_f16_data_cb(
                patch_title_buf, patch_buf,
                "D=", &value_sel, 0, 999, 1, 2, "",
                menu_step_wedge_densitometer_data_callback, &dens_enable);
            if (patch_option == 1) {
                wedge->step_density[option - 1] = (float)value_sel / 100.0F;
            } else if (patch_option == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            }
        } else if (option == wedge->step_count + 1) {
            menu_result = MENU_SAVE;
            break;
        } else if (option == wedge->step_count + 2) {
            for (uint32_t i = 0; i < wedge->step_count; i++) {
                wedge->step_density[i] = NAN;
            }
            option = 1;
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }

    } while (option > 0 && menu_result != MENU_TIMEOUT);

    if (menu_result != MENU_SAVE) {
        for (uint32_t i = 0; i < wedge->step_count; i++) {
            wedge->step_density[i] = prev_density[i];
        }
    }

    if (menu_result == MENU_SAVE) {
        menu_result = MENU_OK;
    }

    vPortFree(buf);
    vPortFree(prev_density);
    return menu_result;
}

menu_result_t menu_step_wedge_show(const step_wedge_t *wedge)
{
    menu_result_t menu_result = MENU_OK;
    char buf[256];

    if (!wedge) {
        return MENU_OK;
    }

    size_t offset = 0;
    int cal_status = calibration_status(wedge);

    offset += sprintf(buf + offset,
        "Steps:                        %2lu\n"
        "Base density:             D=%0.02f\n"
        "Density increment:        D=%0.02f\n"
        "Calibration:        ",
        wedge->step_count,
        wedge->base_density,
        wedge->density_increment);

    if (cal_status == 1) {
        offset += sprintf(buf + offset,
            "  Calibrated\n");
    } else if (cal_status == 2) {
        offset += sprintf(buf + offset,
            "     Partial\n");
    } else {
        offset += sprintf(buf + offset,
            "Uncalibrated\n");
    }

    uint8_t option;
    if (strlen(wedge->name) > 0) {
        option = display_message(wedge->name, NULL, buf, " OK ");
    } else {
        option = display_message("Step Wedge", NULL, buf, " OK ");
    }

    if (option == UINT8_MAX) {
        menu_result = MENU_TIMEOUT;
    }
    return menu_result;
}

uint16_t menu_step_wedge_densitometer_data_callback(uint8_t event_action, void *user_data)
{
    bool dens_enable = *((bool *)user_data);
    if (dens_enable) {
        densitometer_reading_t reading;
        if (densitometer_reading_poll(&reading) == DENSITOMETER_RESULT_OK) {
            if ((reading.mode == DENSITOMETER_MODE_UNKNOWN || reading.mode == DENSITOMETER_MODE_TRANSMISSION)
                && !isnan(reading.visual) && reading.visual > 0.0F) {
                return lroundf(reading.visual * 100);
            }
        }
    }
    return UINT16_MAX;
}
