#include "state_controller.h"

#include <FreeRTOS.h>
#include <cmsis_os.h>
#include <task.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <esp_log.h>

#include "keypad.h"
#include "display.h"
#include "exposure_state.h"
#include "main_menu.h"
#include "buzzer.h"

static const char *TAG = "state_controller";

typedef enum {
    STATE_HOME,
    STATE_HOME_CHANGE_TIME_INCREMENT,
    STATE_TIMER,
    STATE_FOCUS,
    STATE_TEST_STRIP,
    STATE_ADD_ADJUSTMENT,
    STATE_MENU
} state_identifier_t;

typedef struct {
    bool change_inc_pending;
    bool change_inc_swallow_release_up;
    bool change_inc_swallow_release_down;
} state_home_data_t;

/* State data used by the home screen and related screens */
exposure_state_t exposure_state;

void state_home_init(state_home_data_t *state_data);
static state_identifier_t state_home(state_home_data_t *state_data);

static state_identifier_t state_home_change_time_increment();
static state_identifier_t state_timer();
static state_identifier_t state_focus();
static state_identifier_t state_test_strip();
static state_identifier_t state_add_adjustment();
static state_identifier_t state_menu();

static void convert_exposure_to_display(display_main_elements_t *elements, const exposure_state_t *exposure);
static void convert_exposure_to_display_timer(display_exposure_timer_t *elements, const exposure_state_t *exposure);
static void update_display_timer(display_exposure_timer_t *elements, uint32_t exposure_ms);

void state_controller_init()
{
    exposure_state_defaults(&exposure_state);
}

void state_controller_loop()
{
    // State data initialization
    state_home_data_t home_data;
    state_home_init(&home_data);

    state_identifier_t current_state = STATE_HOME;
    state_identifier_t next_state = current_state;
    for (;;) {
        switch (current_state) {
        case STATE_HOME:
            next_state = state_home(&home_data);
            break;
        case STATE_HOME_CHANGE_TIME_INCREMENT:
            next_state = state_home_change_time_increment();
            break;
        case STATE_TIMER:
            next_state = state_timer();
            break;
        case STATE_FOCUS:
            next_state = state_focus();
            break;
        case STATE_TEST_STRIP:
            next_state = state_test_strip();
            break;
        case STATE_ADD_ADJUSTMENT:
            next_state = state_add_adjustment();
            break;
        case STATE_MENU:
            next_state = state_menu();
            break;
        default:
            break;
        }

        if (next_state != current_state) {
            ESP_LOGI(TAG, "State transition: %d -> %d", current_state, next_state);
            current_state = next_state;
        }
    }
}

void state_home_init(state_home_data_t *state_data)
{
    state_data->change_inc_pending = false;
    state_data->change_inc_swallow_release_up = false;
    state_data->change_inc_swallow_release_down = false;
}

state_identifier_t state_home(state_home_data_t *state_data)
{
    state_identifier_t next_state = STATE_HOME;

    // Draw current exposure state
    display_main_elements_t main_elements;
    convert_exposure_to_display(&main_elements, &exposure_state);
    display_draw_main_elements(&main_elements);

    // Handle the next keypad event
    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, -1) == HAL_OK) {
        if (state_data->change_inc_pending) {
            if (keypad_event.key == KEYPAD_INC_EXPOSURE && !keypad_event.pressed) {
                state_data->change_inc_swallow_release_up = false;
            } else if (keypad_event.key == KEYPAD_DEC_EXPOSURE && !keypad_event.pressed) {
                state_data->change_inc_swallow_release_down = false;
            }

            if (!state_data->change_inc_swallow_release_up && !state_data->change_inc_swallow_release_down) {
                state_data->change_inc_pending = false;
                next_state = STATE_HOME_CHANGE_TIME_INCREMENT;
            }
        } else {
            if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_START)) {
                next_state = STATE_TIMER;
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_EXPOSURE)) {
                exposure_adj_increase(&exposure_state);
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_EXPOSURE)) {
                exposure_adj_decrease(&exposure_state);
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_CONTRAST)) {
                exposure_contrast_increase(&exposure_state);
            } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_CONTRAST)) {
                exposure_contrast_decrease(&exposure_state);
            } else if (keypad_event.key == KEYPAD_MENU && !keypad_event.pressed) {
                next_state = STATE_MENU;
            } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                exposure_state_defaults(&exposure_state);
            } else if (keypad_is_key_combo_pressed(&keypad_event, KEYPAD_INC_EXPOSURE, KEYPAD_DEC_EXPOSURE)) {
                state_data->change_inc_pending = true;
                state_data->change_inc_swallow_release_up = true;
                state_data->change_inc_swallow_release_down = true;
            }
        }
    }

    return next_state;
}

state_identifier_t state_home_change_time_increment()
{
    state_identifier_t next_state = STATE_HOME_CHANGE_TIME_INCREMENT;

    // Draw current stop increment
    uint8_t stop_inc_den = exposure_adj_increment_get_denominator(&exposure_state);
    display_draw_stop_increment(stop_inc_den);

    // Handle the next keypad event
    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, -1) == HAL_OK) {
        if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_EXPOSURE)) {
            exposure_adj_increment_increase(&exposure_state);
        } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_EXPOSURE)) {
            exposure_adj_increment_decrease(&exposure_state);
        } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
            next_state = STATE_HOME;
        }
    }

    return next_state;
}

state_identifier_t state_timer()
{
    //TODO Change design to use complex timer arrangement for more consistent behavior
    //TODO Add code to toggle the relays on and off
    state_identifier_t next_state = STATE_HOME;

    TickType_t exposure_start_ticks;
    TickType_t exposure_elapsed_ticks;
    uint32_t exposure_time_start_ms;
    uint32_t exposure_time_ms;
    display_exposure_timer_t elements;
    display_exposure_timer_t prev_elements;
    TickType_t buzz_start_ticks;
    TickType_t buzz_stop_ticks;

    buzzer_volume_t current_volume = buzzer_get_volume();
    pam8904e_freq_t current_frequency = buzzer_get_frequency();

    exposure_time_start_ms = (uint32_t)(exposure_state.adjusted_time * 1000.0f);
    exposure_time_ms = exposure_time_start_ms;
    convert_exposure_to_display_timer(&elements, &exposure_state);
    display_draw_exposure_timer(&elements, 0);

    ESP_LOGI(TAG, "Starting exposure timer at %ldms", exposure_time_ms);
    exposure_start_ticks = xTaskGetTickCount();
    exposure_elapsed_ticks = 0;

    buzzer_set_volume(BUZZER_VOLUME_MEDIUM);
    buzzer_set_frequency(PAM8904E_FREQ_500HZ);

    if ((exposure_time_ms % 1000) == 0) {
        buzzer_start();
        buzz_stop_ticks = pdMS_TO_TICKS(40);
        buzz_start_ticks = pdMS_TO_TICKS(1000);
    } else {
        buzz_stop_ticks = 0;
        buzz_start_ticks = pdMS_TO_TICKS(exposure_time_ms % 1000);
    }

    do {
        memcpy(&prev_elements, &elements, sizeof(display_exposure_timer_t));
        exposure_elapsed_ticks = xTaskGetTickCount() - exposure_start_ticks;
        if ((exposure_elapsed_ticks * portTICK_PERIOD_MS) > exposure_time_start_ms) {
            exposure_time_ms = 0;
        } else {
            exposure_time_ms = exposure_time_start_ms - (exposure_elapsed_ticks * portTICK_PERIOD_MS);
        }

        if (buzz_start_ticks > 0 && exposure_elapsed_ticks >= buzz_start_ticks) {
            buzzer_start();
            buzz_stop_ticks = buzz_start_ticks + 40;
            buzz_start_ticks += pdMS_TO_TICKS(1000);
        }
        else if (buzz_stop_ticks > 0 && exposure_elapsed_ticks >= buzz_stop_ticks) {
            buzzer_stop();
            buzz_stop_ticks = 0;
        }

        update_display_timer(&elements, exposure_time_ms);
        display_draw_exposure_timer(&elements, &prev_elements);

        // Handle the next keypad event
        keypad_event_t keypad_event;
        if (keypad_wait_for_event(&keypad_event, 5) == HAL_OK) {
            if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
                ESP_LOGI(TAG, "Canceling exposure timer at %ldms", exposure_time_ms);
                next_state = STATE_HOME;
                break;
            }
        }
    } while (exposure_time_ms <= exposure_time_start_ms && exposure_time_ms > 0);

    // Make sure we wait out the last tick
    exposure_elapsed_ticks = xTaskGetTickCount() - exposure_start_ticks;
    if (buzz_stop_ticks > exposure_elapsed_ticks) {
        osDelay(buzz_stop_ticks - exposure_elapsed_ticks);
    }
    buzzer_stop();

    ESP_LOGI(TAG, "Exposure timer complete");

    osDelay(pdMS_TO_TICKS(200));

    buzzer_set_frequency(PAM8904E_FREQ_1000HZ);
    buzzer_start();
    osDelay(pdMS_TO_TICKS(100));
    buzzer_stop();

    buzzer_set_frequency(PAM8904E_FREQ_2000HZ);
    buzzer_start();
    osDelay(pdMS_TO_TICKS(100));
    buzzer_stop();

    buzzer_set_frequency(PAM8904E_FREQ_1500HZ);
    buzzer_start();
    osDelay(pdMS_TO_TICKS(100));
    buzzer_stop();

    osDelay(pdMS_TO_TICKS(500));

    buzzer_set_volume(current_volume);
    buzzer_set_frequency(current_frequency);

    return next_state;
}

state_identifier_t state_focus()
{
    //TODO
    return STATE_HOME;
}

state_identifier_t state_test_strip()
{
    //TODO
    return STATE_HOME;
}

state_identifier_t state_add_adjustment()
{
    //TODO
    return STATE_HOME;
}

state_identifier_t state_menu()
{
    main_menu_start();
    return STATE_HOME;
}

void convert_exposure_to_display(display_main_elements_t *elements, const exposure_state_t *exposure)
{
    //TODO Fix this once the exposure state contains tone graph data
    elements->tone_graph = 0;

    switch (exposure->contrast_grade) {
    case CONTRAST_GRADE_00:
        elements->contrast_grade = DISPLAY_GRADE_00;
        break;
    case CONTRAST_GRADE_0:
        elements->contrast_grade = DISPLAY_GRADE_0;
        break;
    case CONTRAST_GRADE_0_HALF:
        elements->contrast_grade = DISPLAY_GRADE_0_HALF;
        break;
    case CONTRAST_GRADE_1:
        elements->contrast_grade = DISPLAY_GRADE_1;
        break;
    case CONTRAST_GRADE_1_HALF:
        elements->contrast_grade = DISPLAY_GRADE_1_HALF;
        break;
    case CONTRAST_GRADE_2:
        elements->contrast_grade = DISPLAY_GRADE_2;
        break;
    case CONTRAST_GRADE_2_HALF:
        elements->contrast_grade = DISPLAY_GRADE_2_HALF;
        break;
    case CONTRAST_GRADE_3:
        elements->contrast_grade = DISPLAY_GRADE_3;
        break;
    case CONTRAST_GRADE_3_HALF:
        elements->contrast_grade = DISPLAY_GRADE_3_HALF;
        break;
    case CONTRAST_GRADE_4:
        elements->contrast_grade = DISPLAY_GRADE_4;
        break;
    case CONTRAST_GRADE_4_HALF:
        elements->contrast_grade = DISPLAY_GRADE_4_HALF;
        break;
    case CONTRAST_GRADE_5:
        elements->contrast_grade = DISPLAY_GRADE_5;
        break;
    default:
        elements->contrast_grade = DISPLAY_GRADE_NONE;
        break;
    }

    float seconds;
    float fractional;
    fractional = modff(exposure->adjusted_time, &seconds);
    elements->time_seconds = seconds;
    elements->time_milliseconds = fractional * 1000.0f;

    if (exposure->adjusted_time < 10) {
        elements->fraction_digits = 2;

    } else if (exposure->adjusted_time < 100) {
        elements->fraction_digits = 1;
    } else {
        elements->fraction_digits = 0;
    }
}

void convert_exposure_to_display_timer(display_exposure_timer_t *elements, const exposure_state_t *exposure)
{
    update_display_timer(elements, (uint32_t)(exposure->adjusted_time * 1000.0f));

    if (exposure->adjusted_time < 10) {
        elements->fraction_digits = 2;

    } else if (exposure->adjusted_time < 100) {
        elements->fraction_digits = 1;
    } else {
        elements->fraction_digits = 0;
    }
}

void update_display_timer(display_exposure_timer_t *elements, uint32_t exposure_ms)
{
    elements->time_seconds = exposure_ms / 1000;
    elements->time_milliseconds = exposure_ms % 1000;
}
