#include "menu_settings.h"

#include <FreeRTOS.h>
#include <cmsis_os.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "display.h"
#include "led.h"
#include "settings.h"
#include "exposure_state.h"
#include "illum_controller.h"

static menu_result_t menu_settings_default_exposure();
static menu_result_t menu_settings_default_step_size();
static menu_result_t menu_settings_test_strip_mode();
static menu_result_t menu_settings_safelight_mode();
static menu_result_t menu_settings_enlarger_auto_shutoff();
static menu_result_t menu_settings_display_brightness();
static menu_result_t menu_settings_buzzer_volume();
static menu_result_t menu_settings_sensor_adjustment();

menu_result_t menu_settings()
{
    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;

    do {
        option = display_selection_list(
            "Settings", option,
            "Default Exposure\n"
            "Default Step Size\n"
            "Test Strip Mode\n"
            "Safelight Mode\n"
            "Enlarger Auto-Shutoff\n"
            "Display Brightness\n"
            "Buzzer Volume\n"
            "Sensor Adjustment");

        if (option == 1) {
            menu_result = menu_settings_default_exposure();
        } else if (option == 2) {
            menu_result = menu_settings_default_step_size();
        } else if (option == 3) {
            menu_result = menu_settings_test_strip_mode();
        } else if (option == 4) {
            menu_result = menu_settings_safelight_mode();
        } else if (option == 5) {
            menu_result = menu_settings_enlarger_auto_shutoff();
        } else if (option == 6) {
            menu_result = menu_settings_display_brightness();
        } else if (option == 7) {
            menu_result = menu_settings_buzzer_volume();
        } else if (option == 8) {
            menu_result = menu_settings_sensor_adjustment();
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}

menu_result_t menu_settings_default_exposure_contrast(contrast_grade_t *grade_setting)
{
    char buf[512];
    menu_result_t menu_result = MENU_OK;

    contrast_grade_t setting = *grade_setting;

    uint8_t option;
    switch (setting) {
    case CONTRAST_GRADE_00:
        option = 1;
        break;
    case CONTRAST_GRADE_0:
        option = 2;
        break;
    case CONTRAST_GRADE_0_HALF:
        option = 3;
        break;
    case CONTRAST_GRADE_1:
        option = 4;
        break;
    case CONTRAST_GRADE_1_HALF:
        option = 5;
        break;
    case CONTRAST_GRADE_2:
        option = 6;
        break;
    case CONTRAST_GRADE_2_HALF:
        option = 7;
        break;
    case CONTRAST_GRADE_3:
        option = 8;
        break;
    case CONTRAST_GRADE_3_HALF:
        option = 9;
        break;
    case CONTRAST_GRADE_4:
        option = 10;
        break;
    case CONTRAST_GRADE_4_HALF:
        option = 11;
        break;
    case CONTRAST_GRADE_5:
        option = 12;
        break;
    default:
        option = 6;
        break;
    }

    do {
        sprintf(buf,
            "Grade %s\n"
            "Grade %s\n"
            "Grade %s\n"
            "Grade %s\n"
            "Grade %s\n"
            "Grade %s\n"
            "Grade %s\n"
            "Grade %s\n"
            "Grade %s\n"
            "Grade %s\n"
            "Grade %s\n"
            "Grade %s",
            contrast_grade_str(CONTRAST_GRADE_00),
            contrast_grade_str(CONTRAST_GRADE_0),
            contrast_grade_str(CONTRAST_GRADE_0_HALF),
            contrast_grade_str(CONTRAST_GRADE_1),
            contrast_grade_str(CONTRAST_GRADE_1_HALF),
            contrast_grade_str(CONTRAST_GRADE_2),
            contrast_grade_str(CONTRAST_GRADE_2_HALF),
            contrast_grade_str(CONTRAST_GRADE_3),
            contrast_grade_str(CONTRAST_GRADE_3_HALF),
            contrast_grade_str(CONTRAST_GRADE_4),
            contrast_grade_str(CONTRAST_GRADE_4_HALF),
            contrast_grade_str(CONTRAST_GRADE_5));

        option = display_selection_list(
            "Default Contrast Grade", option, buf);

        if (option == 1) {
            setting = CONTRAST_GRADE_00;
            break;
        } else if (option == 2) {
            setting = CONTRAST_GRADE_0;
            break;
        } else if (option == 3) {
            setting = CONTRAST_GRADE_0_HALF;
            break;
        } else if (option == 4) {
            setting = CONTRAST_GRADE_1;
            break;
        } else if (option == 5) {
            setting = CONTRAST_GRADE_1_HALF;
            break;
        } else if (option == 6) {
            setting = CONTRAST_GRADE_2;
            break;
        } else if (option == 7) {
            setting = CONTRAST_GRADE_2_HALF;
            break;
        } else if (option == 8) {
            setting = CONTRAST_GRADE_3;
            break;
        } else if (option == 9) {
            setting = CONTRAST_GRADE_3_HALF;
            break;
        } else if (option == 10) {
            setting = CONTRAST_GRADE_4;
            break;
        } else if (option == 11) {
            setting = CONTRAST_GRADE_4_HALF;
            break;
        } else if (option == 12) {
            setting = CONTRAST_GRADE_5;
            break;
        } else if (option == 0) {
            menu_result = MENU_CANCEL;
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    if (menu_result == MENU_OK) {
        *grade_setting = setting;
    }

    return menu_result;
}

menu_result_t menu_settings_default_exposure()
{
    menu_result_t menu_result = MENU_OK;
    char buf1[64];
    char buf2[64];

    uint32_t time_setting = settings_get_default_exposure_time();
    contrast_grade_t grade_setting = settings_get_default_contrast_grade();

    bool accepted = false;

    for (;;) {
        sprintf(buf1, "%ld seconds", time_setting / 1000);
        sprintf(buf2, "Grade %s\n", contrast_grade_str(grade_setting));

        uint8_t option = display_message("Default Exposure\n", buf1, buf2, " OK \n Time \n Grade ");

        if (option == 1) {
            accepted = true;
            break;
        } else if (option == 2) {
            uint8_t value_sel = time_setting / 1000;
            if (display_input_value("Default Exposure Time", "\n", "", &value_sel, 5, 30, 2, " seconds") == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            }
            time_setting = value_sel * 1000;
        } else if (option == 3) {
            if (menu_settings_default_exposure_contrast(&grade_setting) == MENU_TIMEOUT) {
                menu_result = MENU_TIMEOUT;
            }
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
            break;
        } else if (option == 0) {
            if (time_setting != settings_get_default_exposure_time()
                || grade_setting != settings_get_default_contrast_grade()) {
                menu_result = menu_confirm_cancel("Default Exposure");
                if (menu_result == MENU_SAVE) {
                    menu_result = MENU_OK;
                    accepted = true;
                } else if (menu_result == MENU_OK || menu_result == MENU_TIMEOUT) {
                    break;
                } else if (menu_result == MENU_CANCEL) {
                    continue;
                }
            }
            break;
        }
    }

    if (accepted) {
        settings_set_default_exposure_time(time_setting);
        settings_set_default_contrast_grade(grade_setting);
    }

    return menu_result;
}

menu_result_t menu_settings_default_step_size()
{
    menu_result_t menu_result = MENU_OK;

    exposure_adjustment_increment_t setting = settings_get_default_step_size();

    uint8_t option;
    switch (setting) {
    case EXPOSURE_ADJ_WHOLE:
        option = 1;
        break;
    case EXPOSURE_ADJ_HALF:
        option = 2;
        break;
    case EXPOSURE_ADJ_THIRD:
        option = 3;
        break;
    case EXPOSURE_ADJ_QUARTER:
        option = 4;
        break;
    case EXPOSURE_ADJ_SIXTH:
        option = 5;
        break;
    case EXPOSURE_ADJ_TWELFTH:
        option = 6;
        break;
    default:
        option = 4;
        break;
    }

    do {
        option = display_selection_list(
            "Default Step Size", option,
            "1 stop\n"
            "1/2 stop\n"
            "1/3 stop\n"
            "1/4 stop\n"
            "1/6 stop\n"
            "1/12 stop");

        if (option == 1) {
            setting = EXPOSURE_ADJ_WHOLE;
            break;
        } else if (option == 2) {
            setting = EXPOSURE_ADJ_HALF;
            break;
        } else if (option == 3) {
            setting = EXPOSURE_ADJ_THIRD;
            break;
        } else if (option == 4) {
            setting = EXPOSURE_ADJ_QUARTER;
            break;
        } else if (option == 5) {
            setting = EXPOSURE_ADJ_SIXTH;
            break;
        } else if (option == 6) {
            setting = EXPOSURE_ADJ_TWELFTH;
            break;
        } else if (option == 0) {
            menu_result = MENU_CANCEL;
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    if (menu_result == MENU_OK) {
        settings_set_default_step_size(setting);
    }

    return menu_result;
}

menu_result_t menu_settings_test_strip_mode()
{
    menu_result_t menu_result = MENU_OK;
    char buf1[64];
    char buf2[64];

    teststrip_mode_t mode_setting = settings_get_teststrip_mode();
    teststrip_patches_t patch_setting = settings_get_teststrip_patches();

    bool accepted = false;

    for (;;) {
        if (mode_setting == TESTSTRIP_MODE_SEPARATE) {
            sprintf(buf1, "Separate exposures");
        } else {
            sprintf(buf1, "Incremental exposures");
        }
        if (patch_setting == TESTSTRIP_PATCHES_5) {
            sprintf(buf2, "5 patches\n");
        } else {
            sprintf(buf2, "7 patches\n");
        }
        uint8_t option = display_message("Test Strip Mode\n", buf1, buf2, " OK \n Mode \n Patches ");

        if (option == 1) {
            accepted = true;
            break;
        } else if (option == 2) {
            if (mode_setting == TESTSTRIP_MODE_SEPARATE) {
                mode_setting = TESTSTRIP_MODE_INCREMENTAL;
            } else {
                mode_setting = TESTSTRIP_MODE_SEPARATE;
            }
        } else if (option == 3) {
            if (patch_setting == TESTSTRIP_PATCHES_5) {
                patch_setting = TESTSTRIP_PATCHES_7;
            } else {
                patch_setting = TESTSTRIP_PATCHES_5;
            }
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
            break;
        } else if (option == 0) {
            if (mode_setting != settings_get_teststrip_mode()
                || patch_setting != settings_get_teststrip_patches()) {
                menu_result = menu_confirm_cancel("Test Strip Mode");
                if (menu_result == MENU_SAVE) {
                    menu_result = MENU_OK;
                    accepted = true;
                } else if (menu_result == MENU_OK || menu_result == MENU_TIMEOUT) {
                    break;
                } else if (menu_result == MENU_CANCEL) {
                    continue;
                }
            }
            break;
        }
    }

    if (accepted) {
        settings_set_teststrip_mode(mode_setting);
        settings_set_teststrip_patches(patch_setting);
    }

    return menu_result;
}

menu_result_t menu_settings_safelight_mode()
{
    menu_result_t menu_result = MENU_OK;

    safelight_mode_t setting = settings_get_safelight_mode();

    uint8_t option;
    switch (setting) {
    case SAFELIGHT_MODE_OFF:
        option = 1;
        break;
    case SAFELIGHT_MODE_ON:
        option = 2;
        break;
    case SAFELIGHT_MODE_AUTO:
    default:
        option = 3;
        break;
    }

    do {
        option = display_selection_list(
            "Safelight Mode", option,
            "Off\n"
            "On\n"
            "Auto");

        if (option == 1) {
            setting = SAFELIGHT_MODE_OFF;
            break;
        } else if (option == 2) {
            setting = SAFELIGHT_MODE_ON;
            break;
        } else if (option == 3) {
            setting = SAFELIGHT_MODE_AUTO;
            break;
        } else if (option == 0) {
            menu_result = MENU_CANCEL;
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    if (menu_result == MENU_OK) {
        settings_set_safelight_mode(setting);
        illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);
    }

    return menu_result;
}

menu_result_t menu_settings_enlarger_auto_shutoff()
{
    menu_result_t menu_result = MENU_OK;

    uint32_t setting = settings_get_enlarger_focus_timeout();

    uint8_t option;
    if (setting == 0) {
        option = 1;
    } else if (setting == 1 * 60000) {
        option = 2;
    } else if (setting == 2 * 60000) {
        option = 3;
    } else if (setting == 5 * 60000) {
        option = 4;
    } else if (setting == 10 * 60000) {
        option = 5;
    } else {
        option = 4;
    }

    do {
        option = display_selection_list(
            "Enlarger Auto-Shutoff", option,
            "Disabled\n"
            "1 minute\n"
            "2 minutes\n"
            "5 minutes\n"
            "10 minutes");

        if (option == 1) {
            setting = 0;
            break;
        } else if (option == 2) {
            setting = 1 * 60000;
            break;
        } else if (option == 3) {
            setting = 2 * 60000;
            break;
        } else if (option == 4) {
            setting = 5 * 60000;
            break;
        } else if (option == 5) {
            setting = 10 * 60000;
            break;
        } else if (option == 0) {
            menu_result = MENU_CANCEL;
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    if (menu_result == MENU_OK) {
        settings_set_enlarger_focus_timeout(setting);
    }

    return menu_result;
}

static void screen_brightness_adjust_callback(uint8_t value, void *user_data)
{
    display_set_brightness(value & 0x0F);
}

static void panel_brightness_adjust_callback(uint8_t value, void *user_data)
{
    led_set_brightness(value);
}

menu_result_t menu_settings_display_brightness()
{
    menu_result_t menu_result = MENU_OK;
    char buf1[64];
    char buf2[64];
    uint8_t display_setting = settings_get_display_brightness();
    uint8_t led_setting = settings_get_led_brightness();
    bool accepted = false;

    for (;;) {
        sprintf(buf1, "Screen =  %02d/15 ", display_setting);
        sprintf(buf2, " Panel = %03d/255\n", led_setting);
        uint8_t option = display_message("Display Brightness\n", buf1, buf2, " OK \n Screen \n Panel ");

        if (option == 1) {
            accepted = true;
            break;
        } else if (option == 2) {
            uint8_t value_sel = display_setting & 0x0F;
            if (display_input_value_cb("Screen Brightness", "\n", "", &value_sel, 0, 0x0F, 2, "",
                screen_brightness_adjust_callback, NULL) == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                display_setting = value_sel & 0x0F;
            }
            display_set_brightness(display_setting);
        } else if (option == 3) {
            uint8_t value_sel = led_setting;
            if (display_input_value_cb("Panel Brightness", "\n", "", &value_sel, 0, 0xFF, 3, "",
                panel_brightness_adjust_callback, NULL) == UINT8_MAX) {
                menu_result = MENU_TIMEOUT;
            } else {
                led_setting = value_sel;
            }
            led_set_brightness(led_setting);
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
            break;
        } else if (option == 0) {
            if (display_setting != settings_get_display_brightness()
                || led_setting != settings_get_led_brightness()) {
                menu_result = menu_confirm_cancel("Display Brightness");
                if (menu_result == MENU_SAVE) {
                    menu_result = MENU_OK;
                    accepted = true;
                } else if (menu_result == MENU_OK || menu_result == MENU_TIMEOUT) {
                    break;
                } else if (menu_result == MENU_CANCEL) {
                    continue;
                }
            }
            break;
        }
    }

    if (accepted) {
        settings_set_display_brightness(display_setting);
        settings_set_led_brightness(led_setting);
    }

    display_set_brightness(settings_get_display_brightness());
    led_set_brightness(settings_get_led_brightness());

    return menu_result;
}

menu_result_t menu_settings_buzzer_volume()
{
    menu_result_t menu_result = MENU_OK;

    buzzer_volume_t setting = settings_get_buzzer_volume();

    uint8_t option;
    switch (setting) {
    case BUZZER_VOLUME_OFF:
        option = 1;
        break;
    case BUZZER_VOLUME_LOW:
        option = 2;
        break;
    case BUZZER_VOLUME_MEDIUM:
        option = 3;
        break;
    case BUZZER_VOLUME_HIGH:
        option = 4;
        break;
    default:
        option = 3;
        break;
    }

    do {
        option = display_selection_list(
            "Buzzer Volume", option,
            "Off\n"
            "Low\n"
            "Medium\n"
            "High");

        if (option == 1) {
            setting = BUZZER_VOLUME_OFF;
            break;
        } else if (option == 2) {
            setting = BUZZER_VOLUME_LOW;
            break;
        } else if (option == 3) {
            setting = BUZZER_VOLUME_MEDIUM;
            break;
        } else if (option == 4) {
            setting = BUZZER_VOLUME_HIGH;
            break;
        } else if (option == 0) {
            menu_result = MENU_CANCEL;
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    if (menu_result == MENU_OK) {
        settings_set_buzzer_volume(setting);

        buzzer_set_frequency(PAM8904E_FREQ_DEFAULT);
        buzzer_set_volume(setting);
        buzzer_start();
        osDelay(100);
        buzzer_stop();
        buzzer_set_volume(BUZZER_VOLUME_OFF);
    }

    return menu_result;
}

menu_result_t menu_settings_sensor_adjustment()
{
    menu_result_t menu_result = MENU_OK;

    /*
     * In future versions of the meter probe, calibration constants may
     * be stored on the probe itself. However, the initial meter probe
     * does not have any memory. Therefore, this will be configured in
     * device settings. Hopefully the same values should work across all
     * similar meter probes, but this is being made adjustable just
     * in case.
     */

    uint16_t value_sel = lroundf(settings_get_tcs3472_ga_factor() * 100);
    if (display_input_value_f16(
        "TCS3472 GA Factor",
        "Ratio between a calibrated lux\n"
        "reading and an uncalibrated\n"
        "reading from the meter probe.\n",
        "", &value_sel, 0, 999, 1, 2, "") == UINT8_MAX) {
        menu_result = MENU_TIMEOUT;
    } else {
        settings_set_tcs3472_ga_factor((float)value_sel / 100.0F);
    }

    return menu_result;
}
