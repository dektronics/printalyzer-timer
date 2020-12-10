#include "menu_diagnostics.h"

#include <FreeRTOS.h>
#include <task.h>
#include <cmsis_os.h>

#include <stdio.h>
#include <string.h>

#include "display.h"
#include "keypad.h"
#include "led.h"
#include "buzzer.h"
#include "relay.h"

static menu_result_t diagnostics_keypad();
static menu_result_t diagnostics_led();
static menu_result_t diagnostics_buzzer();
static menu_result_t diagnostics_relay();

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
                "Meter Probe Test");

        if (option == 1) {
            menu_result = diagnostics_keypad();
        } else if (option == 2) {
            menu_result = diagnostics_led();
        } else if (option == 3) {
            menu_result = diagnostics_buzzer();
        } else if (option == 4) {
            menu_result = diagnostics_relay();
        } else if (option == 5) {
            //menu_result = diagnostics_meter_probe();
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
    keypad_wait_for_event(&keypad_event, 0);

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

    sprintf(buf, "\n");
    display_static_list("LED Test", buf);

    for (;;) {
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

    pam8904e_freq_t freq = current_frequency;
    bool freq_changed = false;
    buzzer_volume_t volume = current_volume;
    bool volume_changed = false;
    uint32_t duration = 200;

    sprintf(buf, "\n");
    display_static_list("Buzzer Test", buf);

    for (;;) {
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
                if (freq < PAM8904E_FREQ_2000HZ) {
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
            }

            if (freq_changed) {
                buzzer_set_frequency(freq);
                freq_changed = false;
            }
            if (volume_changed) {
                buzzer_set_volume(volume);
                volume_changed = false;
            }

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

    sprintf(buf, "\n");
    display_static_list("Relay Test", buf);

    for (;;) {
        keypad_event_t keypad_event;
        if (keypad_wait_for_event(&keypad_event, -1) == HAL_OK) {
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_START)) {
                relay_enlg = !relay_enlg;
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_FOCUS)) {
                relay_sflt = !relay_sflt;
            } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                break;
            }

            relay_enlarger_enable(relay_enlg);
            relay_safelight_enable(relay_sflt);

            sprintf(buf,
                "\n"
                "Enlarger  [%s]\n"
                "Safelight [%s]",
                relay_enlg ? "**" : "  ",
                relay_sflt ? "**" : "  ");
            display_static_list("Relay Test", buf);
        }
    }

    relay_enlarger_enable(current_enlarger);
    relay_safelight_enable(current_safelight);

    return MENU_OK;
}
