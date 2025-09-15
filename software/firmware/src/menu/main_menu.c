#include "main_menu.h"

#include <FreeRTOS.h>
#include <task.h>

#define LOG_TAG "main_menu"
#include <elog.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "display.h"
#include "keypad.h"
#include "led.h"
#include "util.h"
#include "app_descriptor.h"

#include "menu_settings.h"
#include "menu_enlarger.h"
#include "menu_safelight.h"
#include "menu_paper.h"
#include "menu_step_wedge.h"
#include "menu_import_export.h"
#include "menu_meter_probe.h"
#include "menu_diagnostics.h"
#include "menu_firmware.h"

#ifdef ENABLE_EMC_TEST
#include "meter_probe.h"
#include "relay.h"
#include "dmx.h"
#include "buzzer.h"
#include "math.h"
#endif


static menu_result_t menu_about();
#ifdef ENABLE_EMC_TEST
static menu_result_t menu_emc_test();
#endif

menu_result_t main_menu_start(state_controller_t *controller)
{
    log_i("Main menu");

    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;

    do {
        option = display_selection_list(
                "Main Menu", option,
                "Settings\n"
                "Enlarger Configuration\n"
                "Safelight Configuration\n"
                "Paper Profiles\n"
                "Step Wedge Properties\n"
                "Import / Export\n"
                "Meter Probe\n"
                "DensiStick\n"
                "Diagnostics\n"
                "Update Firmware\n"
                "About"
#ifdef ENABLE_EMC_TEST
                "\nEMC Test Cycle"
#endif
        );

        if (option == 1) {
            menu_result = menu_settings();
        } else if (option == 2) {
            menu_result = menu_enlarger_configs(controller);
        } else if (option == 3) {
            menu_result = menu_safelight_config();
        } else if (option == 4) {
            menu_result = menu_paper_profiles(controller);
        } else if (option == 5) {
            menu_result = menu_step_wedge();
        } else if (option == 6) {
            menu_result = menu_import_export(controller);
        } else if (option == 7) {
            menu_result = menu_meter_probe();
        } else if (option == 8) {
            menu_result = menu_densistick();
        } else if (option == 9) {
            menu_result = menu_diagnostics();
        } else if (option == 10) {
            menu_result = menu_firmware();
        } else if (option == 11) {
            menu_result = menu_about();
#ifdef ENABLE_EMC_TEST
        } else if (option == 12) {
            menu_result = menu_emc_test();
#endif
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}

menu_result_t menu_confirm_cancel(const char *title)
{
    uint8_t option;
    char buf[128];

    if (title && strlen(title) > 0) {
        sprintf(buf, "%s\n", title);
    } else {
        sprintf(buf, "\n");
    }

    option = display_message(buf, "Would you like to save changes?", "\n",
        " Yes \n No \n Cancel ");

    if (option == 1) {
        return MENU_SAVE;
    } else if (option == 2) {
        return MENU_OK;
    } else if (option == UINT8_MAX) {
        return MENU_TIMEOUT;
    } else {
        return MENU_CANCEL;
    }
}

size_t menu_build_padded_str_row(char *buf, const char *label, const char *value)
{
    size_t offset = 0;
    size_t label_len = strlen(label);
    size_t value_len = strlen(value);
    size_t value_display_len = MIN(value_len, DISPLAY_MENU_ROW_LENGTH - (label_len + 3));

    strcpy(buf, label);
    strcat(buf, " ");

    offset = pad_str_to_length(buf, ' ', DISPLAY_MENU_ROW_LENGTH - (value_display_len + 2));
    buf[offset++] = '[';
    strncpy(buf + offset, value, value_display_len + 1);

    if (value_display_len < value_len) {
        buf[DISPLAY_MENU_ROW_LENGTH - 2] = '\x7F';
    }

    buf[DISPLAY_MENU_ROW_LENGTH - 1] = ']';
    buf[DISPLAY_MENU_ROW_LENGTH] = '\n';
    buf[DISPLAY_MENU_ROW_LENGTH + 1] = '\0';

    return DISPLAY_MENU_ROW_LENGTH + 1;
}

size_t menu_build_padded_format_row(char *buf, const char *label, const char *format, ...)
{
    va_list ap;
    char val_buf[DISPLAY_MENU_ROW_LENGTH];

    va_start(ap, format);
    vsnprintf(val_buf, sizeof(val_buf), format, ap);
    va_end(ap);

    return menu_build_padded_str_row(buf, label, val_buf);
}

menu_result_t menu_about()
{
    const app_descriptor_t *app_descriptor = app_descriptor_get();
    menu_result_t menu_result = MENU_OK;
    char buf[384];

    sprintf(buf,
        "Enlarging Timer & Exposure Meter\n"
        "%s\n"
        "%s\n"
        "%s\n",
        app_descriptor->version,
        app_descriptor->build_describe,
        app_descriptor->build_date);

    uint8_t option = display_message(
        app_descriptor->project_name,
        NULL,
        buf, " OK ");
    if (option == UINT8_MAX) {
        menu_result = MENU_TIMEOUT;
    }
    return menu_result;
}

#ifdef ENABLE_EMC_TEST
menu_result_t menu_emc_test()
{
    bool keypad_pressed = false;
    bool mp_active = false;
    bool mp_start_error = false;
    bool mp_running = false;
    bool mp_run_error = false;
    meter_probe_sensor_reading_t mp_reading = {0};
    bool ds_active = false;
    bool ds_start_error = false;
    bool ds_running = false;
    bool ds_run_error = false;
    bool emc_cycle_running = false;
    uint8_t emc_cycle_index = 0;
    uint32_t emc_cycle_time = 0;
    uint8_t dmx_frame_val[8] = {0};
    meter_probe_sensor_reading_t ds_reading = {0};
    keypad_event_t keypad_event = {0};
    display_emc_elements_t emc_elements = {0};

    const bool current_enlarger = relay_enlarger_is_enabled();
    const bool current_safelight = relay_safelight_is_enabled();
    const buzzer_volume_t current_volume = buzzer_get_volume();
    const uint16_t current_frequency = buzzer_get_frequency();

    meter_probe_handle_t *mp_handle = meter_probe_handle();
    meter_probe_handle_t *ds_handle = densistick_handle();

    if (meter_probe_start(mp_handle) == osOK) {
        mp_active = true;
    }
    if (meter_probe_start(ds_handle) == osOK) {
        ds_active = true;
    }

    for (;;) {
        const uint32_t loop_ticks = osKernelGetTickCount();

        if (mp_running && !meter_probe_is_started(mp_handle)) {
            mp_running = false;
            mp_active = false;
        }

        if (ds_running && !meter_probe_is_started(ds_handle)) {
            ds_running = false;
            ds_active = false;
        }

        /* Start the Meter Probe if attached and not started */
        if (!mp_start_error && !mp_active && meter_probe_is_attached(mp_handle)) {
            if (meter_probe_start(mp_handle) == osOK) {
                mp_active = true;
                mp_run_error = false;
            } else {
                mp_start_error = true;
            }
        }

        /* Start the DensiStick if attached and not started */
        if (!ds_start_error && !ds_active && meter_probe_is_attached(ds_handle)) {
            if (meter_probe_start(ds_handle) == osOK) {
                ds_active = true;
                ds_run_error = false;
            } else {
                ds_start_error = true;
            }
        }

        /* Start the Meter Probe sensor read process */
        if (!mp_running && !mp_run_error) {
            meter_probe_sensor_set_gain(mp_handle, TSL2585_GAIN_256X);
            meter_probe_sensor_set_integration(mp_handle, 719, 99);
            meter_probe_sensor_set_mod_calibration(mp_handle, 1);
            meter_probe_sensor_enable_agc(mp_handle, 19);
            if (meter_probe_sensor_enable(mp_handle) == osOK) {
                mp_running = true;
            } else {
                mp_run_error = true;
            }
        }

        /* Start the DensiStick sensor read process */
        if (!ds_running && !ds_run_error) {
            meter_probe_sensor_set_gain(ds_handle, TSL2585_GAIN_256X);
            meter_probe_sensor_set_integration(ds_handle, 719, 99);
            meter_probe_sensor_set_mod_calibration(ds_handle, 1);
            meter_probe_sensor_enable_agc(ds_handle, 19);
            if (meter_probe_sensor_enable(ds_handle) == osOK) {
                ds_running = true;
            } else {
                ds_run_error = true;
            }
        }

        emc_elements.mp_connected = mp_active;
        emc_elements.ds_connected = ds_active;

        if (mp_running) {
            if (meter_probe_sensor_get_next_reading(mp_handle, &mp_reading, 0) == osOK) {
                emc_elements.mp_reading = meter_probe_basic_result(mp_handle, &mp_reading);
            }
        } else {
            emc_elements.mp_reading = NAN;
        }

        if (ds_running) {
            if (meter_probe_sensor_get_next_reading(ds_handle, &ds_reading, 0) == osOK) {
                emc_elements.ds_reading = meter_probe_basic_result(ds_handle, &ds_reading);
            }
        } else {
            emc_elements.ds_reading = NAN;
        }

        /* Handle EMC test cycle */
        if (emc_cycle_running && (loop_ticks - emc_cycle_time) >= 1000) {
            if (emc_cycle_index == 0) {
                /* Initial setup state for EMC cycle */

                /* Relays both off */
                relay_enlarger_enable(false);
                relay_safelight_enable(false);

                /* DMX frame clear but transmitting */
                dmx_clear_frame(false);
                dmx_enable();
                dmx_start();

                /* Set initial buzzer frequency and volume */
                buzzer_set_frequency(500);
                buzzer_set_volume(BUZZER_VOLUME_HIGH);
                buzzer_stop();

                /* Set initial DensiStick light value */
                densistick_set_light_enable(ds_handle, false);
                densistick_set_light_brightness(ds_handle, 0);
            } else if (emc_cycle_index == 1) {
                /* Simulate focus state */

                relay_enlarger_enable(false);
                relay_safelight_enable(true);

                dmx_frame_val[0] = 0xFF;
                dmx_frame_val[3] = 0x00;
                dmx_set_frame(0, dmx_frame_val, sizeof(dmx_frame_val), true);
            } else if (emc_cycle_index == 2) {
                densistick_set_light_enable(ds_handle, true);
            } else if (emc_cycle_index == 3) {
                buzzer_set_frequency(750);
                buzzer_start();
                osDelay(pdMS_TO_TICKS(50));
                buzzer_stop();
                osDelay(pdMS_TO_TICKS(100));
                buzzer_set_frequency(1000);
                buzzer_start();
                osDelay(pdMS_TO_TICKS(50));
                buzzer_stop();
                densistick_set_light_brightness(ds_handle, 127);
            } else if (emc_cycle_index == 4) {
                densistick_set_light_enable(ds_handle, false);
            } else if (emc_cycle_index == 5) {
                /* Simulate expose state */

                relay_enlarger_enable(true);
                relay_safelight_enable(false);

                dmx_frame_val[0] = 0x00;
                dmx_frame_val[3] = 0xFF;
                dmx_set_frame(0, dmx_frame_val, sizeof(dmx_frame_val), true);
            } else if (emc_cycle_index >= 6 && emc_cycle_index <= 10) {
                buzzer_set_frequency(500);
                buzzer_start();
                osDelay(pdMS_TO_TICKS(40));
                buzzer_stop();
            } else if (emc_cycle_index == 11) {
                buzzer_set_frequency(1000);
                buzzer_start();
                osDelay(pdMS_TO_TICKS(50));
                buzzer_set_frequency(2000);
                osDelay(pdMS_TO_TICKS(50));
                buzzer_set_frequency(1500);
                osDelay(pdMS_TO_TICKS(50));
                buzzer_stop();
            }

            /* Advance to next cycle index */
            emc_cycle_index++;
            emc_cycle_time = loop_ticks;
            if (emc_cycle_index > 11) { emc_cycle_index = 1; }
        }

        /* Populate various display elements */
        emc_elements.enlarger_on = relay_enlarger_is_enabled();
        emc_elements.safelight_on = relay_safelight_is_enabled();
        emc_elements.dmx_ch_a = dmx_frame_val[0];
        emc_elements.dmx_ch_b = dmx_frame_val[3];
        emc_elements.ds_light = densistick_get_light_enable(ds_handle);
        emc_elements.ds_light_value = densistick_get_light_brightness(ds_handle);

        display_emc_elements(&keypad_event, &emc_elements);

        if (keypad_event.key == KEYPAD_START && keypad_event.repeated && !emc_cycle_running) {
            emc_cycle_running = true;
            emc_cycle_index = 0;
            emc_cycle_time = osKernelGetTickCount();
        }

        if (keypad_event.key == KEYPAD_CANCEL && keypad_event.repeated) {
            keypad_pressed = true;
            break;
        }

        keypad_event.key = 0;
        keypad_event.pressed = false;
        keypad_event.repeated = false;
        const uint32_t elapsed_ticks = osKernelGetTickCount() - loop_ticks;
        int wait_ticks;
        if (elapsed_ticks > 200) {
            wait_ticks = 0;
        } else {
            wait_ticks = 200 - (int)elapsed_ticks;
        }
        keypad_wait_for_event(&keypad_event, wait_ticks);
    }

    if (keypad_pressed) {
        for (;;) {
            if (keypad_wait_for_event(&keypad_event, -1) == HAL_OK) {
                if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                    break;
                }
            } else {
                break;
            }
        }
    }

    /* Stop the Meter Probe */
    if (mp_active) {
        meter_probe_sensor_disable(mp_handle);
        meter_probe_stop(mp_handle);
    }

    /* Stop the DensiStick */
    if (ds_active) {
        meter_probe_sensor_disable(ds_handle);
        meter_probe_stop(ds_handle);
    }

    /* Revert the relay state */
    relay_enlarger_enable(current_enlarger);
    relay_safelight_enable(current_safelight);

    /* Clear the DMX state */
    if (dmx_get_port_state() == DMX_PORT_ENABLED_TRANSMITTING) {
        dmx_stop();
    }

    /* Revert the buzzer state */
    buzzer_set_volume(current_volume);
    buzzer_set_frequency(current_frequency);

    return MENU_OK;
}
#endif
