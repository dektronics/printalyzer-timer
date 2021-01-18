#include "menu_diagnostics.h"

#include <FreeRTOS.h>
#include <task.h>
#include <cmsis_os.h>

#include <stdio.h>
#include <string.h>
#include <math.h>

#include <esp_log.h>

#include "display.h"
#include "keypad.h"
#include "led.h"
#include "buzzer.h"
#include "relay.h"
#include "tcs3472.h"
#include "settings.h"
#include "illum_controller.h"

extern I2C_HandleTypeDef hi2c2;

static menu_result_t diagnostics_keypad();
static menu_result_t diagnostics_led();
static menu_result_t diagnostics_buzzer();
static menu_result_t diagnostics_relay();
static menu_result_t diagnostics_meter_probe();
static menu_result_t diagnostics_screenshot_mode();

static const char *TAG = "menu_diagnostics";

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
                "Meter Probe Test\n"
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
            menu_result = diagnostics_meter_probe();
        } else if (option == 6) {
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

menu_result_t diagnostics_meter_probe()
{
    HAL_StatusTypeDef ret = HAL_OK;
    char buf[512];
    bool sensor_initialized = false;
    bool sensor_error = false;
    bool freeze_data = false;
    tcs3472_channel_data_t channel_data;
    bool enlarger_enabled = relay_enlarger_is_enabled();

    memset(&channel_data, 0, sizeof(tcs3472_channel_data_t));

    for (;;) {
        if (!freeze_data && !sensor_initialized) {
            ret = tcs3472_init(&hi2c2);
            if (ret == HAL_OK) {
                sensor_error = false;
            } else {
                ESP_LOGE(TAG, "Error initializing TCS3472: %d", ret);
                sensor_error = true;
            }

            if (!sensor_error) {
                ret = tcs3472_enable(&hi2c2);
                if (ret != HAL_OK) {
                    ESP_LOGE(TAG, "Error enabling TCS3472: %d", ret);
                    sensor_error = true;
                }
            }
            sensor_initialized = true;
        }

        if (!freeze_data && sensor_initialized && !sensor_error) {
            memset(&channel_data, 0, sizeof(tcs3472_channel_data_t));
            ret = tcs3472_get_full_channel_data(&hi2c2, &channel_data);
            if (ret != HAL_OK) {
                ESP_LOGE(TAG, "Error getting TCS3472 channel data: %d", ret);
                sensor_error = true;
            }
        }

        if (sensor_initialized && !sensor_error) {
            uint16_t color_temp = tcs3472_calculate_color_temp(&channel_data);

            float lux = tcs3472_calculate_lux(&channel_data);

            uint16_t ir = (channel_data.red + channel_data.green + channel_data.blue > channel_data.clear)
                ? (channel_data.red + channel_data.green + channel_data.blue - channel_data.clear) / 2 : 0;
            uint16_t ir_pct = roundf(((float)ir / channel_data.clear) * 100.0F);

            sprintf(buf,
                    "TCS3472 (%s, %s)\n"
                    "Clear: %d\n"
                    "R/G/B: %d / %d / %d\n"
                    "Temp: %dK\n"
                    "Lux: %04f / IR: %d%%",
                    tcs3472_gain_str(channel_data.gain), tcs3472_atime_str(channel_data.integration),
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
                    ESP_LOGI(TAG, "Meter probe focus mode enabled");
                    relay_enlarger_enable(true);
                } else {
                    ESP_LOGI(TAG, "Meter probe focus mode disabled");
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
