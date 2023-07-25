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
#include "meter_probe.h"
#include "tcs3472.h"
#include "tsl2585.h"
#include "settings.h"
#include "illum_controller.h"
#include "densitometer.h"
#include "usb_host.h"
#include "dmx.h"
#include "util.h"

extern I2C_HandleTypeDef hi2c2;

static menu_result_t diagnostics_keypad();
static menu_result_t diagnostics_led();
static menu_result_t diagnostics_buzzer();
static menu_result_t diagnostics_relay();
static menu_result_t diagnostics_dmx512();
static menu_result_t diagnostics_meter_probe();
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
                "Meter Probe Test\n"
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
            menu_result = diagnostics_meter_probe();
        } else if (option == 7) {
            menu_result = diagnostics_densitometer();
        } else if (option == 8) {
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
        } else if (keypad_event.key == KEYPAD_USB_KEYBOARD
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
    uint16_t current_state = led_get_state();
    uint8_t current_brightness = led_get_brightness();
    char buf[256];

    // Start with all LEDs off
    led_set_state(0);

    bool illum_on = false;
    bool ind_on = false;
    int led_index = 0;
    uint8_t led_brightness = current_brightness;
    bool led_brightness_changed = false;

    for (;;) {
        sprintf(buf,
            "Illumination = %s\n"
            "  Indicators = %s\n"
            "       Index = %2d \n"
            "  Brightness = %3d",
            (illum_on ? "on " : "off"),
            (ind_on ? "on " : "off"),
            led_index,
            led_brightness);
        display_static_list("LED Test", buf);

        keypad_event_t keypad_event;
        if (keypad_wait_for_event(&keypad_event, -1) == HAL_OK) {
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_START)) {
                illum_on = !illum_on;
                led_index = 0;
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_FOCUS)) {
                ind_on = !ind_on;
                led_index = 0;
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_CONTRAST)) {
                illum_on = false;
                if (led_index <= 0) {
                    led_index = 14;
                } else {
                    led_index--;
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_CONTRAST)) {
                ind_on = false;
                if (led_index >= 14) {
                    led_index = 0;
                } else {
                    led_index++;
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_EXPOSURE)) {
                if ((255 - led_brightness) >= 10) {
                    led_brightness += 10;
                } else {
                    led_brightness = 255;
                }
                led_brightness_changed = true;
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_EXPOSURE)) {
                if (led_brightness >= 10) {
                    led_brightness -= 10;
                } else {
                    led_brightness = 0;
                }
                led_brightness_changed = true;
            } else if (keypad_event.key == KEYPAD_ENCODER_CCW && keypad_event.pressed) {
                if (led_brightness > 0) {
                    led_brightness--;
                    led_brightness_changed = true;
                }
            } else if (keypad_event.key == KEYPAD_ENCODER_CW && keypad_event.pressed) {
                if (led_brightness < 255) {
                    led_brightness++;
                    led_brightness_changed = true;
                }
            } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                break;
            } else if (keypad_event.key == KEYPAD_USB_KEYBOARD
                && keypad_usb_get_keypad_equivalent(&keypad_event) == KEYPAD_CANCEL) {
                break;
            }

            if (illum_on) {
                led_set_on(LED_ILLUM_ALL);
            } else {
                led_set_off(LED_ILLUM_ALL);
            }
            if (ind_on) {
                led_set_on(LED_IND_ALL);
            } else {
                led_set_off(LED_IND_ALL);
            }

            switch (led_index) {
            case 1:
                led_set_state(LED_START);
                break;
            case 2:
                led_set_state(LED_FOCUS);
                break;
            case 3:
                led_set_state(LED_DEC_CONTRAST);
                break;
            case 4:
                led_set_state(LED_DEC_EXPOSURE);
                break;
            case 5:
                led_set_state(LED_INC_CONTRAST);
                break;
            case 6:
                led_set_state(LED_INC_EXPOSURE);
                break;
            case 7:
                led_set_state(LED_ADD_ADJUSTMENT);
                break;
            case 8:
                led_set_state(LED_TEST_STRIP);
                break;
            case 9:
                led_set_state(LED_CANCEL);
                break;
            case 10:
                led_set_state(LED_MENU);
                break;
            case 11:
                led_set_state(LED_IND_ADD_ADJUSTMENT);
                break;
            case 12:
                led_set_state(LED_IND_TEST_STRIP);
                break;
            case 13:
                led_set_state(LED_IND_CANCEL);
                break;
            case 14:
                led_set_state(LED_IND_MENU);
                break;
            default:
                if (!illum_on && !ind_on) {
                    led_set_state(0);
                }
                break;
            }

            if (led_brightness_changed) {
                led_set_brightness(led_brightness);
                led_brightness_changed = false;
            }
        }
    }

    led_set_state(current_state);
    led_set_brightness(current_brightness);
    return MENU_OK;
}

menu_result_t diagnostics_buzzer()
{
    buzzer_volume_t current_volume = buzzer_get_volume();
    pam8904e_freq_t current_frequency = buzzer_get_frequency();

    char buf[256];

    pam8904e_freq_t freq = PAM8904E_FREQ_DEFAULT;
    bool freq_changed = false;
    buzzer_volume_t volume = settings_get_buzzer_volume();
    bool volume_changed = false;
    uint32_t duration = 200;

    buzzer_set_frequency(freq);
    buzzer_set_volume(volume);

    for (;;) {
        uint32_t freq_num;
        switch (freq) {
        case PAM8904E_FREQ_DEFAULT:
            freq_num = 1465;
            break;
        case PAM8904E_FREQ_500HZ:
            freq_num = 500;
            break;
        case PAM8904E_FREQ_1000HZ:
            freq_num = 1000;
            break;
        case PAM8904E_FREQ_1500HZ:
            freq_num = 1500;
            break;
        case PAM8904E_FREQ_2000HZ:
            freq_num = 2000;
            break;
        case PAM8904E_FREQ_4800HZ:
            freq_num = 4800;
            break;
        default:
            freq_num = 0;
        }

        sprintf(buf,
            "Frequency = %ldHz\n"
            "Duration = %ldms\n"
            "Volume = %d",
            freq_num,
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
                if (freq > PAM8904E_FREQ_DEFAULT) {
                    freq--;
                    freq_changed = true;
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_CONTRAST)) {
                if (freq < PAM8904E_FREQ_4800HZ) {
                    freq++;
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
            } else if (keypad_event.key == KEYPAD_USB_KEYBOARD
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
            } else if (keypad_event.key == KEYPAD_USB_KEYBOARD
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
            } else if (keypad_event.key == KEYPAD_USB_KEYBOARD
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

#if 0
menu_result_t diagnostics_meter_probe()
{
    HAL_StatusTypeDef ret = HAL_OK;
    char buf[512];
    bool sensor_initialized = false;
    bool sensor_error = false;
    bool freeze_data = false;
    bool use_calibration = true;
    tcs3472_channel_data_t channel_data;
    bool enlarger_enabled = relay_enlarger_is_enabled();

    float ga_factor = settings_get_tcs3472_ga_factor();

    memset(&channel_data, 0, sizeof(tcs3472_channel_data_t));

    for (;;) {
        if (!freeze_data && !sensor_initialized) {
            ret = tcs3472_init(&hi2c2);
            if (ret == HAL_OK) {
                sensor_error = false;
            } else {
                log_e("Error initializing TCS3472: %d", ret);
                sensor_error = true;
            }

            if (!sensor_error) {
                ret = tcs3472_enable(&hi2c2);
                if (ret != HAL_OK) {
                    log_e("Error enabling TCS3472: %d", ret);
                    sensor_error = true;
                }
            }
            sensor_initialized = true;
        }

        if (!freeze_data && sensor_initialized && !sensor_error) {
            memset(&channel_data, 0, sizeof(tcs3472_channel_data_t));
            ret = tcs3472_get_full_channel_data(&hi2c2, &channel_data);
            if (ret != HAL_OK) {
                log_e("Error getting TCS3472 channel data: %d", ret);
                sensor_error = true;
            }
        }

        if (sensor_initialized && !sensor_error) {
            uint16_t color_temp = tcs3472_calculate_color_temp(&channel_data);

            float lux = tcs3472_calculate_lux(&channel_data, use_calibration ? ga_factor : 0);

            uint16_t ir = (channel_data.red + channel_data.green + channel_data.blue > channel_data.clear)
                ? (channel_data.red + channel_data.green + channel_data.blue - channel_data.clear) / 2 : 0;
            uint16_t ir_pct = roundf(((float)ir / channel_data.clear) * 100.0F);

            sprintf(buf,
                "TCS3472 (%s, %s, GA=%.02f)\n"
                "Clear: %d\n"
                "R/G/B: %d / %d / %d\n"
                "Temp: %dK\n"
                "Lux: %04f / IR: %d%%",
                tcs3472_gain_str(channel_data.gain), tcs3472_atime_str(channel_data.integration),
                (use_calibration ? ga_factor : 1.0F),
                channel_data.clear, channel_data.red, channel_data.green, channel_data.blue,
                color_temp, lux, ir_pct);
        } else {
            sprintf(buf, "\n\n**** Sensor Unavailable ****");
        }
        if (freeze_data) {
            display_static_list("**** Meter Probe Test ****", buf);
        } else {
            display_static_list("Meter Probe Test", buf);
        }

        keypad_event_t keypad_event;
        if (keypad_wait_for_event(&keypad_event, 200) == HAL_OK) {
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_START)) {
                if (sensor_initialized && sensor_error) {
                    sensor_initialized = false;
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_FOCUS)) {
                if (!relay_enlarger_is_enabled()) {
                    log_i("Meter probe focus mode enabled");
                    relay_enlarger_enable(true);
                } else {
                    log_i("Meter probe focus mode disabled");
                    relay_enlarger_enable(false);
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_EXPOSURE)) {
                if (!freeze_data && sensor_initialized && !sensor_error) {
                    tcs3472_again_t again;

                    switch (channel_data.gain) {
                    case TCS3472_AGAIN_1X:
                        again = TCS3472_AGAIN_4X;
                        break;
                    case TCS3472_AGAIN_4X:
                        again = TCS3472_AGAIN_16X;
                        break;
                    case TCS3472_AGAIN_16X:
                        again = TCS3472_AGAIN_60X;
                        break;
                    case TCS3472_AGAIN_60X:
                    default:
                        again = channel_data.gain;
                        break;
                    }

                    if (again != channel_data.gain) {
                        tcs3472_set_gain(&hi2c2, again);
                    }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_EXPOSURE)) {
                if (!freeze_data && sensor_initialized && !sensor_error) {
                    tcs3472_again_t again;

                    switch (channel_data.gain) {
                        break;
                    case TCS3472_AGAIN_60X:
                        again = TCS3472_AGAIN_16X;
                        break;
                    case TCS3472_AGAIN_16X:
                        again = TCS3472_AGAIN_4X;
                        break;
                    case TCS3472_AGAIN_4X:
                        again = TCS3472_AGAIN_1X;
                        break;
                    case TCS3472_AGAIN_1X:
                    default:
                        again = channel_data.gain;
                        break;
                    }

                    if (again != channel_data.gain) {
                        tcs3472_set_gain(&hi2c2, again);
                    }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_CONTRAST)) {
                if (!freeze_data && sensor_initialized && !sensor_error) {
                    tcs3472_atime_t atime;

                    switch (channel_data.integration) {
                    case TCS3472_ATIME_2_4MS:
                        atime = TCS3472_ATIME_4_8MS;
                        break;
                    case TCS3472_ATIME_4_8MS:
                        atime = TCS3472_ATIME_24MS;
                        break;
                    case TCS3472_ATIME_24MS:
                        atime = TCS3472_ATIME_50MS;
                        break;
                    case TCS3472_ATIME_50MS:
                        atime = TCS3472_ATIME_101MS;
                        break;
                    case TCS3472_ATIME_101MS:
                        atime = TCS3472_ATIME_154MS;
                        break;
                    case TCS3472_ATIME_154MS:
                        atime = TCS3472_ATIME_614MS;
                        break;
                    case TCS3472_ATIME_614MS:
                    default:
                        atime = channel_data.integration;
                        break;
                    }

                    if (atime != channel_data.integration) {
                        tcs3472_set_time(&hi2c2, atime);
                    }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_CONTRAST)) {
                if (!freeze_data && sensor_initialized && !sensor_error) {
                    tcs3472_atime_t atime;

                    switch (channel_data.integration) {
                    case TCS3472_ATIME_614MS:
                        atime = TCS3472_ATIME_154MS;
                        break;
                    case TCS3472_ATIME_154MS:
                        atime = TCS3472_ATIME_101MS;
                        break;
                    case TCS3472_ATIME_101MS:
                        atime = TCS3472_ATIME_50MS;
                        break;
                    case TCS3472_ATIME_50MS:
                        atime = TCS3472_ATIME_24MS;
                        break;
                    case TCS3472_ATIME_24MS:
                        atime = TCS3472_ATIME_4_8MS;
                        break;
                    case TCS3472_ATIME_4_8MS:
                        atime = TCS3472_ATIME_2_4MS;
                        break;
                    case TCS3472_ATIME_2_4MS:
                    default:
                        atime = channel_data.integration;
                        break;
                    }

                    if (atime != channel_data.integration) {
                        tcs3472_set_time(&hi2c2, atime);
                    }
                }
            } else if (keypad_event.key == KEYPAD_TEST_STRIP && !keypad_event.pressed) {
                use_calibration = !use_calibration;
            } else if ((keypad_event.key == KEYPAD_MENU || keypad_event.key == KEYPAD_METER_PROBE) && !keypad_event.pressed) {
                freeze_data = !freeze_data;
            } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                break;
            } else if (keypad_event.key == KEYPAD_USB_KEYBOARD
                && keypad_usb_get_keypad_equivalent(&keypad_event) == KEYPAD_CANCEL) {
                break;
            }
        }
    }

    tcs3472_disable(&hi2c2);
    relay_enlarger_enable(enlarger_enabled);

    return MENU_OK;
}
#endif

menu_result_t diagnostics_meter_probe()
{
    HAL_StatusTypeDef ret = HAL_OK;
    char buf[512];
    bool sensor_initialized = false;
    bool sensor_error = false;
    bool enlarger_enabled = relay_enlarger_is_enabled();
    bool config_changed = false;
    bool agc_changed = false;
    tsl2585_gain_t gain = 0;
    uint16_t sample_time = 0;
    uint16_t sample_count = 0;
    bool agc_enabled = false;
    meter_probe_sensor_reading_t reading = {0};
    keypad_event_t keypad_event;
    bool key_changed = false;

    if (meter_probe_start() != osOK) {
        menu_result_t menu_result = MENU_OK;
        uint8_t option = display_message(
            "Meter Probe",
            NULL,
            "\n**** Not Detected ****", " OK ");
        if (option == UINT8_MAX) {
            menu_result = MENU_TIMEOUT;
        }
        return menu_result;
    }

    if (meter_probe_sensor_set_config(TSL2585_GAIN_256X, 716, 100) == osOK) {
        gain = TSL2585_GAIN_256X;
        sample_time = 716;
        sample_count = 100;
    }

    for (;;) {
        if (!sensor_initialized) {
            if (meter_probe_sensor_enable() == osOK) {
                sensor_initialized = true;
            } else {
                log_e("Error initializing TSL2585: %d", ret);
                sensor_error = true;
            }
        }

        if (key_changed) {
            key_changed = false;
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_FOCUS)) {
                if (!relay_enlarger_is_enabled()) {
                    log_i("Meter probe focus mode enabled");
                    relay_enlarger_enable(true);
                } else {
                    log_i("Meter probe focus mode disabled");
                    relay_enlarger_enable(false);
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_EXPOSURE)) {
                if (sensor_initialized && !sensor_error) {
                    if (gain > TSL2585_GAIN_0_5X) {
                        gain--;
                        config_changed = true;
                    }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_EXPOSURE)) {
                if (sensor_initialized && !sensor_error) {
                    if (gain < TSL2585_GAIN_4096X) {
                        gain++;
                        config_changed = true;
                    }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_CONTRAST)) {
                if (sensor_initialized && !sensor_error) {
                    if (sample_count > 10) {
                        sample_count -= 10;
                        config_changed = true;
                    }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_CONTRAST)) {
                if (sensor_initialized && !sensor_error) {
                    if (sample_count < (2047 - 10)) {
                        sample_count += 10;
                        config_changed = true;
                    }
                }
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_ADD_ADJUSTMENT)) {
                if (sensor_initialized && !sensor_error) {
                    agc_enabled = !agc_enabled;
                    agc_changed = true;
                }
            } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                break;
            } else if (keypad_event.key == KEYPAD_USB_KEYBOARD
                && keypad_usb_get_keypad_equivalent(&keypad_event) == KEYPAD_CANCEL) {
                break;
            }
        }

        if (config_changed) {
            if (meter_probe_sensor_set_config(gain, sample_time, sample_count) == osOK) {
                config_changed = false;
            }
        }
        if (agc_changed) {
            if (agc_enabled) {
                if (meter_probe_sensor_enable_agc() == osOK) {
                    agc_changed = false;
                }
            } else {
                if (meter_probe_sensor_disable_agc() == osOK) {
                    agc_changed = false;
                }
            }
        }

        if (meter_probe_sensor_get_next_reading(&reading, 1000) == osOK) {
            const float atime = tsl2585_integration_time_ms(reading.sample_time, reading.sample_count);
            const float gain_val = tsl2585_gain_value(reading.gain);

            float basic_result = reading.raw_result / (atime * gain_val);

            sprintf(buf,
                "TSL2585 (%s, %.2fms)\n"
                "Data: %ld\n"
                "Basic: %f\n"
                "[%s]",
                tsl2585_gain_str(reading.gain), atime,
                reading.raw_result, basic_result,
                agc_enabled ? "AGC" : "---");
            display_static_list("Meter Probe Test", buf);

            gain = reading.gain;
            sample_time = reading.sample_time;
            sample_count = reading.sample_count;
        }

        if (keypad_wait_for_event(&keypad_event, 100) == HAL_OK) {
            key_changed = true;
        }
    }

    meter_probe_sensor_disable();
    meter_probe_stop();
    relay_enlarger_enable(enlarger_enabled);

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

        dens_result = densitometer_reading_poll(&reading, 100);
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
