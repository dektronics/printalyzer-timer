#include "menu_diagnostics.h"

#include <FreeRTOS.h>
#include <task.h>
#include <cmsis_os.h>

#include <stdio.h>
#include <string.h>
#include <math.h>

#define LOG_TAG "menu_diagnostics"
#include <elog.h>

#include "display.h"
#include "keypad.h"
#include "led.h"
#include "buzzer.h"
#include "relay.h"
#include "settings.h"
#include "illum_controller.h"
#include "enlarger_control.h"
#include "densitometer.h"
#include "usb_host.h"
#include "dmx.h"
#include "util.h"

static menu_result_t diagnostics_keypad();
static menu_result_t diagnostics_led();
static menu_result_t diagnostics_buzzer();
static menu_result_t diagnostics_relay();
static menu_result_t diagnostics_dmx512();
static menu_result_t diagnostics_densitometer();
static menu_result_t diagnostics_screenshot_mode();

menu_result_t menu_diagnostics()
{
    menu_result_t menu_result = MENU_OK;
    uint8_t option = 1;

    do {
        option = display_selection_list(
                "Diagnostics", option,
                "Keypad Test\n"
                "LED Test\n"
                "Buzzer Test\n"
                "Relay Test\n"
                "DMX512 Control Test\n"
                "Densitometer Test\n"
                "Screenshot Mode");

        if (option == 1) {
            menu_result = diagnostics_keypad();
        } else if (option == 2) {
            menu_result = diagnostics_led();
        } else if (option == 3) {
            menu_result = diagnostics_buzzer();
        } else if (option == 4) {
            menu_result = diagnostics_relay();
        } else if (option == 5) {
            menu_result = diagnostics_dmx512();
        } else if (option == 6) {
            menu_result = diagnostics_densitometer();
        } else if (option == 7) {
            menu_result = diagnostics_screenshot_mode();
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
    } while (option > 0 && menu_result != MENU_TIMEOUT);

    return menu_result;
}

menu_result_t diagnostics_keypad()
{
    char buf[512];

    bool keypad_pressed = false;
    keypad_event_t keypad_event;
    memset(&keypad_event, 0, sizeof(keypad_event_t));

    for (;;) {
        sprintf(buf,
               "|                       %c(%c)%c |\n"
               "|  [%c][%c]     [%c]     [%c][%c]  |\n"
               "|  [%c][%c]  [%c][%c][%c]  [%c][%c]  |\n"
               "|                             |\n"
               "|  (%c)     (%c)           {%c}  |",
            (keypad_event.key == KEYPAD_ENCODER_CCW) ? '<' : ' ',
            keypad_is_key_pressed(&keypad_event, KEYPAD_ENCODER) ? '*' : ' ',
            (keypad_event.key == KEYPAD_ENCODER_CW) ? '>' : ' ',
            keypad_is_key_pressed(&keypad_event, KEYPAD_START) ? '*' : ' ',
            keypad_is_key_pressed(&keypad_event, KEYPAD_FOCUS) ? '*' : ' ',
            keypad_is_key_pressed(&keypad_event, KEYPAD_INC_EXPOSURE) ? '*' : ' ',
            keypad_is_key_pressed(&keypad_event, KEYPAD_ADD_ADJUSTMENT) ? '*' : ' ',
            keypad_is_key_pressed(&keypad_event, KEYPAD_CANCEL) ? '*' : ' ',
            keypad_is_key_pressed(&keypad_event, KEYPAD_START) ? '*' : ' ',
            keypad_is_key_pressed(&keypad_event, KEYPAD_FOCUS) ? '*' : ' ',
            keypad_is_key_pressed(&keypad_event, KEYPAD_DEC_CONTRAST) ? '*' : ' ',
            keypad_is_key_pressed(&keypad_event, KEYPAD_DEC_EXPOSURE) ? '*' : ' ',
            keypad_is_key_pressed(&keypad_event, KEYPAD_INC_CONTRAST) ? '*' : ' ',
            keypad_is_key_pressed(&keypad_event, KEYPAD_TEST_STRIP) ? '*' : ' ',
            keypad_is_key_pressed(&keypad_event, KEYPAD_MENU) ? '*' : ' ',
            keypad_is_key_pressed(&keypad_event, KEYPAD_FOOTSWITCH) ? '*' : ' ',
            keypad_is_key_pressed(&keypad_event, KEYPAD_METER_PROBE) ? '*' : ' ',
            keypad_is_key_pressed(&keypad_event, KEYPAD_BLACKOUT) ? '*' : ' ');
        display_static_list("Keypad Test", buf);

        if (keypad_event.key == KEYPAD_CANCEL && keypad_event.repeated) {
            keypad_pressed = true;
            break;
        } else if (keypad_event.key == KEYPAD_USB_KEYBOARD && keypad_event.pressed
            && keypad_usb_get_keypad_equivalent(&keypad_event) == KEYPAD_CANCEL) {
            break;
        }

        keypad_event.key = 0;
        keypad_event.pressed = false;
        keypad_event.repeated = false;
        keypad_wait_for_event(&keypad_event, 200);
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

    return MENU_OK;
}

menu_result_t diagnostics_led()
{
    const uint16_t current_state = LED_ILLUM_ALL;
    const uint8_t current_brightness = settings_get_led_brightness();
    char buf[256];

    /* Start with all LEDs off */
    led_set_value(LED_ILLUM_ALL, 0);

    bool illum_on = false;
    int led_index = 0;
    uint8_t led_brightness = current_brightness;
    bool setting_changed = false;

    for (;;) {
        sprintf(buf,
            "Illumination = %s\n"
            "       Index = %2d \n"
            "  Brightness = %3d",
            (illum_on ? "on " : "off"),
            led_index,
            led_brightness);
        display_static_list("LED Test", buf);

        keypad_event_t keypad_event;
        if (keypad_wait_for_event(&keypad_event, -1) == HAL_OK) {
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_START)) {
                illum_on = !illum_on;
                setting_changed = true;
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_CONTRAST)) {
                if (led_index <= 0) {
                    led_index = 24;
                } else {
                    led_index--;
                }
                setting_changed = true;
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_CONTRAST)) {
                if (led_index >= 12) {
                    led_index = 0;
                } else {
                    led_index++;
                }
                setting_changed = true;
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_EXPOSURE)) {
                if ((255 - led_brightness) >= 10) {
                    led_brightness += 10;
                } else {
                    led_brightness = 255;
                }
                setting_changed = true;
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_EXPOSURE)) {
                if (led_brightness >= 10) {
                    led_brightness -= 10;
                } else {
                    led_brightness = 0;
                }
                setting_changed = true;
            } else if (keypad_event.key == KEYPAD_ENCODER_CCW && keypad_event.pressed) {
                if (led_brightness > 0) {
                    led_brightness--;
                    setting_changed = true;
                }
            } else if (keypad_event.key == KEYPAD_ENCODER_CW && keypad_event.pressed) {
                if (led_brightness < 255) {
                    led_brightness++;
                    setting_changed = true;
                }
            } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                break;
            } else if (keypad_event.key == KEYPAD_USB_KEYBOARD && keypad_event.pressed
                && keypad_usb_get_keypad_equivalent(&keypad_event) == KEYPAD_CANCEL) {
                break;
            }

            if (setting_changed) {
                led_t led_value;
                switch (led_index) {
                case 1:
                    led_value = LED_START_UPPER;
                    break;
                case 2:
                    led_value = LED_START_LOWER;
                    break;
                case 3:
                    led_value = LED_FOCUS_UPPER;
                    break;
                case 4:
                    led_value = LED_FOCUS_LOWER;
                    break;
                case 5:
                    led_value = LED_DEC_CONTRAST;
                    break;
                case 6:
                    led_value = LED_DEC_EXPOSURE;
                    break;
                case 7:
                    led_value = LED_INC_CONTRAST;
                    break;
                case 8:
                    led_value = LED_INC_EXPOSURE;
                    break;
                case 9:
                    led_value = LED_ADD_ADJUSTMENT;
                    break;
                case 10:
                    led_value = LED_TEST_STRIP;
                    break;
                case 11:
                    led_value = LED_CANCEL;
                    break;
                case 12:
                    led_value = LED_MENU;
                    break;
                default:
                    led_value = LED_ILLUM_ALL;
                    break;
                }

                led_set_value(LED_ILLUM_ALL, 0);
                led_set_value(led_value, illum_on ? led_brightness : 0);
                setting_changed = false;
            }
        }
    }

    led_set_value(current_state, current_brightness);

    return MENU_OK;
}

menu_result_t diagnostics_buzzer()
{
    buzzer_volume_t current_volume = buzzer_get_volume();
    uint16_t current_frequency = buzzer_get_frequency();

    char buf[256];

    uint16_t freq = 500;
    bool freq_changed = false;
    buzzer_volume_t volume = settings_get_buzzer_volume();
    bool volume_changed = false;
    uint32_t duration = 200;

    buzzer_set_frequency(freq);
    buzzer_set_volume(volume);

    for (;;) {

        sprintf(buf,
            "Frequency = %dHz\n"
            "Duration = %ldms\n"
            "Volume = %d",
            freq,
            duration,
            volume);
        display_static_list("Buzzer Test", buf);

        keypad_event_t keypad_event;
        if (keypad_wait_for_event(&keypad_event, -1) == HAL_OK) {
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_START)) {
                buzzer_start();
                osDelay(pdMS_TO_TICKS(duration));
                buzzer_stop();
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_CONTRAST)) {
                if (freq > 200) {
                    freq -= 100;
                    freq_changed = true;
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_CONTRAST)) {
                if (freq < 5000) {
                    freq += 100;
                    freq_changed = true;
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_EXPOSURE)) {
                if (volume < BUZZER_VOLUME_HIGH) {
                    volume++;
                    volume_changed = true;
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_EXPOSURE)) {
                if (volume > BUZZER_VOLUME_OFF) {
                    volume--;
                    volume_changed = true;
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_ADD_ADJUSTMENT)) {
                if (duration < 600) {
                    duration += 50;
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_TEST_STRIP)) {
                if (duration > 50) {
                    duration -= 50;
                }
            } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                break;
            } else if (keypad_event.key == KEYPAD_USB_KEYBOARD && keypad_event.pressed
                && keypad_usb_get_keypad_equivalent(&keypad_event) == KEYPAD_CANCEL) {
                break;
            }

            if (freq_changed) {
                buzzer_set_frequency(freq);
                freq_changed = false;
            }
            if (volume_changed) {
                buzzer_set_volume(volume);
                volume_changed = false;
            }
        }
    }

    buzzer_set_volume(current_volume);
    buzzer_set_frequency(current_frequency);
    return MENU_OK;
}

menu_result_t diagnostics_relay()
{
    bool current_enlarger = relay_enlarger_is_enabled();
    bool current_safelight = relay_safelight_is_enabled();
    relay_enlarger_enable(false);
    relay_safelight_enable(false);

    char buf[256];

    bool relay_enlg = false;
    bool relay_sflt = false;

    for (;;) {
        sprintf(buf,
            "\n"
            "Enlarger  [%s]\n"
            "Safelight [%s]",
            relay_enlg ? "**" : "  ",
            relay_sflt ? "**" : "  ");
        display_static_list("Relay Test", buf);

        keypad_event_t keypad_event;
        if (keypad_wait_for_event(&keypad_event, -1) == HAL_OK) {
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_START)) {
                relay_enlg = !relay_enlg;
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_FOCUS)) {
                relay_sflt = !relay_sflt;
            } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                break;
            } else if (keypad_event.key == KEYPAD_USB_KEYBOARD && keypad_event.pressed
                && keypad_usb_get_keypad_equivalent(&keypad_event) == KEYPAD_CANCEL) {
                break;
            }

            relay_enlarger_enable(relay_enlg);
            relay_safelight_enable(relay_sflt);
        }
    }

    relay_enlarger_enable(current_enlarger);
    relay_safelight_enable(current_safelight);

    return MENU_OK;
}

menu_result_t diagnostics_dmx512()
{
    char state_buf[32];
    char buf[256];
    uint16_t frame_offset = 0;
    uint8_t frame_val[8] = {0};
    bool wide_mode = false;
    uint8_t marker = 0;
    bool marker_all = false;
    dmx_port_state_t port_state = DMX_PORT_DISABLED;

    if (dmx_get_port_state() == DMX_PORT_ENABLED_TRANSMITTING) {
        dmx_clear_frame(false);
    }

    for (;;) {
        port_state = dmx_get_port_state();

        switch(port_state) {
        case DMX_PORT_DISABLED:
            sprintf(state_buf, "         [Disabled]");
            break;
        case DMX_PORT_ENABLED_IDLE:
            sprintf(state_buf, "   [Enabled (Idle)]");
            break;
        case DMX_PORT_ENABLED_TRANSMITTING:
            sprintf(state_buf, "     [Transmitting]");
            break;
        default:
            sprintf(state_buf, "                [?]");
            break;
        }

        if (wide_mode) {
            sprintf(buf,
                " Port state %s\n"
                " Resolution            [16-bit]\n"
                "    %03d    %03d    %03d    %03d    \n"
                "  [%05d][%05d][%05d][%05d]  \n"
                "     %c      %c      %c      %c     \n",
                state_buf,
                frame_offset + 1, frame_offset + 3, frame_offset + 5, frame_offset + 7,
                conv_array_u16(frame_val),
                conv_array_u16(frame_val + 2),
                conv_array_u16(frame_val + 4),
                conv_array_u16(frame_val + 6),
                ((marker == 0 || marker_all) ? '*' : ' '),
                ((marker == 1 || marker_all) ? '*' : ' '),
                ((marker == 2 || marker_all) ? '*' : ' '),
                ((marker == 3 || marker_all) ? '*' : ' '));
        } else {
            sprintf(buf,
                " Port state %s\n"
                " Resolution             [8-bit]\n"
                "  %03d  %03d  %03d  %03d  %03d  %03d  \n"
                " [%03d][%03d][%03d][%03d][%03d][%03d] \n"
                "   %c    %c    %c    %c    %c    %c   \n",
                state_buf,
                frame_offset + 1, frame_offset + 2, frame_offset + 3,
                frame_offset + 4, frame_offset + 5, frame_offset + 6,
                frame_val[0], frame_val[1], frame_val[2],
                frame_val[3], frame_val[4], frame_val[5],
                ((marker == 0 || marker_all) ? '*' : ' '),
                ((marker == 1 || marker_all) ? '*' : ' '),
                ((marker == 2 || marker_all) ? '*' : ' '),
                ((marker == 3 || marker_all) ? '*' : ' '),
                ((marker == 4 || marker_all) ? '*' : ' '),
                ((marker == 5 || marker_all) ? '*' : ' '));
        }

        display_static_list("DMX512 Control Test", buf);

        keypad_event_t keypad_event;
        if (keypad_wait_for_event(&keypad_event, -1) == HAL_OK) {
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_START)) {
                if (port_state == DMX_PORT_DISABLED) {
                    if (dmx_enable() == osOK) {
                        dmx_start();
                    }
                } else if (port_state == DMX_PORT_ENABLED_IDLE) {
                    dmx_start();
                } else if (port_state == DMX_PORT_ENABLED_TRANSMITTING) {
                    dmx_stop();
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_FOCUS)) {
                if (port_state == DMX_PORT_DISABLED) {
                    dmx_enable();
                } else if (port_state == DMX_PORT_ENABLED_IDLE) {
                    dmx_disable();
                } else if (port_state == DMX_PORT_ENABLED_TRANSMITTING) {
                    if (dmx_stop() == osOK) {
                        dmx_disable();
                    }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_EXPOSURE)) {
                if (marker_all) {
                    memset(frame_val, 0xFF, wide_mode ? 8 : 6);
                } else {
                    if (wide_mode) {
                        frame_val[marker * 2] = 0xFF;
                        frame_val[(marker * 2) + 1] = 0xFF;
                    } else {
                        frame_val[marker] = 0xFF;
                    }
                }
                dmx_set_frame(frame_offset, frame_val, sizeof(frame_val), false);
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_EXPOSURE)) {
                if (marker_all) {
                    memset(frame_val, 0x00, wide_mode ? 8 : 6);
                } else {
                    if (wide_mode) {
                        frame_val[marker * 2] = 0x00;
                        frame_val[(marker * 2) + 1] = 0x00;
                    } else {
                        frame_val[marker] = 0x00;
                    }
                }
                dmx_set_frame(frame_offset, frame_val, sizeof(frame_val), false);
            } else if (keypad_event.key == KEYPAD_ENCODER_CW) {
                if (wide_mode) {
                    if (marker_all) {
                        for (uint8_t i = 0; i < 8; i += 2) {
                            uint16_t temp = conv_array_u16(frame_val + i);
                            if (temp < 0xFFFF) { temp++; }
                            conv_u16_array(frame_val + i, temp);
                        }
                    } else {
                        size_t i = marker * 2;
                        uint16_t temp = conv_array_u16(frame_val + i);
                        if (temp < 0xFFFF) { temp++; }
                        conv_u16_array(frame_val + i, temp);
                    }
                } else {
                    if (marker_all) {
                        for (uint8_t i = 0; i < 6; i++) {
                            if (frame_val[i] < 0xFF) {
                                frame_val[i]++;
                            }
                        }
                    } else {
                        if (frame_val[marker] < 0xFF) {
                            frame_val[marker]++;
                        }
                    }
                }
                dmx_set_frame(frame_offset, frame_val, sizeof(frame_val), false);
            } else if (keypad_event.key == KEYPAD_ENCODER_CCW) {
                if (wide_mode) {
                    if (marker_all) {
                        for (uint8_t i = 0; i < 8; i += 2) {
                            uint16_t temp = conv_array_u16(frame_val + i);
                            if (temp > 0x0000) { temp--; }
                            conv_u16_array(frame_val + i, temp);
                        }
                    } else {
                        size_t i = marker * 2;
                        uint16_t temp = conv_array_u16(frame_val + i);
                        if (temp > 0x0000) { temp--; }
                        conv_u16_array(frame_val + i, temp);
                    }
                } else {
                    if (marker_all) {
                        for (uint8_t i = 0; i < 6; i++) {
                            if (frame_val[i] > 0x00) {
                                frame_val[i]--;
                            }
                        }
                    } else {
                        if (frame_val[marker] > 0x00) {
                            frame_val[marker]--;
                        }
                    }
                }
                dmx_set_frame(frame_offset, frame_val, sizeof(frame_val), false);
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_CONTRAST)) {
                if (marker < (wide_mode ? 3 : 5)) { marker++; }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_CONTRAST)) {
                if (marker > 0) { marker--; }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_ADD_ADJUSTMENT)) {
                marker_all = !marker_all;
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_TEST_STRIP)) {
                uint8_t option = 1;
                do {
                    sprintf(buf,
                        "Port offset                [%03d]\n"
                        "Resolution              %s",
                        frame_offset + 1,
                        wide_mode ? "[16-bit]" : " [8-bit]");
                    option = display_selection_list(
                        "DMX512 Control Options", option, buf);

                    if (option == 1) {
                        uint16_t value_sel = frame_offset + 1;
                        if (display_input_value_u16(
                            "Starting Channel",
                            "\n\n",
                            "", &value_sel, 1, 507, 3, "") == UINT8_MAX) {
                            continue;
                        }
                        if (value_sel - 1 != frame_offset) {
                            /* Zero out the current frame for simplicity */
                            memset(frame_val, 0x00, sizeof(frame_val));
                            dmx_set_frame(frame_offset, frame_val, sizeof(frame_val), false);

                            /* Reset the markers */
                            marker = 0;
                            marker_all = false;

                            /* Update the frame offset */
                            frame_offset = value_sel - 1;
                        }
                    } else if (option == 2) {
                        /* Zero out the current frame for simplicity */
                        memset(frame_val, 0x00, sizeof(frame_val));
                        dmx_set_frame(frame_offset, frame_val, sizeof(frame_val), false);

                        /* Reset the markers */
                        marker = 0;
                        marker_all = false;

                        /* Change the bit mode */
                        wide_mode = !wide_mode;
                    }
                } while (option > 0 && option != UINT8_MAX);
            } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                break;
            } else if (keypad_event.key == KEYPAD_USB_KEYBOARD && keypad_event.pressed
                && keypad_usb_get_keypad_equivalent(&keypad_event) == KEYPAD_CANCEL) {
                break;
            }
        }
    }

    /* Clear DMX output frame */
    port_state = dmx_get_port_state();
    if (port_state == DMX_PORT_ENABLED_TRANSMITTING) {
        dmx_stop();
    }

    /* Reset the DMX controller state */
    illum_controller_refresh();
    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

    return MENU_OK;
}

menu_result_t diagnostics_densitometer()
{
    char buf[512];
    densitometer_result_t dens_result = DENSITOMETER_RESULT_UNKNOWN;
    densitometer_reading_t reading;
    bool has_reading = false;

    usb_serial_clear_receive_buffer();

    for (;;) {
        if (has_reading) {
            const char *mode_str;
            switch (reading.mode) {
            case DENSITOMETER_MODE_TRANSMISSION:
                mode_str = "Transmission";
                break;
            case DENSITOMETER_MODE_REFLECTION:
                mode_str = "Reflection";
                break;
            case DENSITOMETER_MODE_UNKNOWN:
            default:
                mode_str = "Unknown";
                break;
            }
            sprintf(buf, "\n"
                "Mode: %s\n"
                "VIS=%0.02f\n"
                "R=%0.02f G=%0.02f B=%0.02f",
                mode_str, reading.visual,
                reading.red, reading.green, reading.blue);
        } else {
            if (usb_serial_is_attached()) {
                sprintf(buf, "\n\nConnected");
            } else {
                sprintf(buf, "\n\n** USB Serial Not Attached **");
            }
        }
        display_static_list("Densitometer Test", buf);

        keypad_event_t keypad_event;
        if (keypad_wait_for_event(&keypad_event, 100) == HAL_OK) {
            if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                break;
            }
        }

        dens_result = densitometer_reading_poll(&reading);
        if (dens_result == DENSITOMETER_RESULT_OK) {
            densitometer_log_reading(&reading);
            has_reading = true;
        }
    }
    return MENU_OK;
}

menu_result_t diagnostics_screenshot_mode()
{
    menu_result_t menu_result = MENU_OK;
    char buf[128];

    for (;;) {
        if (illum_controller_get_screenshot_mode()) {
            sprintf(buf, "Enabled\nBlackout takes a screenshot\n");
        } else {
            sprintf(buf, "Disabled\nBlackout behaves normally\n");
        }

        uint8_t option = display_message(
            "Screenshot Mode\n",
            NULL, buf,
            " Enable \n Disable ");

        if (option == 1) {
            illum_controller_set_screenshot_mode(true);
            break;
        } else if (option == 2) {
            illum_controller_set_screenshot_mode(false);
            break;
        } else if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
            break;
        } else if (option == 0) {
            break;
        }
    }

    return menu_result;
}
