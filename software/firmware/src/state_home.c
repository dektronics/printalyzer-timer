#include "state_home.h"

#include <FreeRTOS.h>
#include <cmsis_os.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define LOG_TAG "state_home"
#include <elog.h>

#include "keypad.h"
#include "keypad_action.h"
#include "display.h"
#include "enlarger_config.h"
#include "enlarger_control.h"
#include "exposure_state.h"
#include "relay.h"
#include "illum_controller.h"
#include "meter_probe.h"
#include "buzzer.h"
#include "settings.h"
#include "util.h"

#define LIVE_TONE_TIMEOUT pdMS_TO_TICKS(2000)

typedef enum {
    ACTION_NONE = 0,
    ACTION_TIMER,
    ACTION_FOCUS,
    ACTION_INC_EXPOSURE,
    ACTION_DEC_EXPOSURE,
    ACTION_INC_CONTRAST,
    ACTION_DEC_CONTRAST,
    ACTION_EDIT_ADJUSTMENT,
    ACTION_LIST_ADJUSTMENTS,
    ACTION_TEST_STRIP,
    ACTION_SELECT_PROFILE,
    ACTION_ADJUST_FINE,
    ACTION_ADJUST_ABSOLUTE,
    ACTION_MENU,
    ACTION_CLEAR_READINGS,
    ACTION_SET_DEFAULTS,
    ACTION_TAKE_READING,
    ACTION_ENCODER_DEC,
    ACTION_ENCODER_INC,
    ACTION_CHANGE_TIME_INCREMENT,
    ACTION_CHANGE_MODE
} state_home_actions_t;

typedef struct {
    state_t base;
    uint32_t updated_tone_element;
    uint32_t live_tone_element;
    uint32_t live_tone_ticks;
    uint8_t highlight_element;
    bool display_dirty;
    bool tone_dirty;
    uint8_t dens_selected_mode;
    uint8_t dens_reading_type;
} state_home_t;

typedef struct {
    state_t base;
} state_home_change_time_increment_t;

typedef struct {
    state_t base;
    state_identifier_t prev_state;
    exposure_mode_t working_value;
    bool accepted;
} state_home_change_mode_t;

typedef struct {
    state_t base;
    int working_value;
    int min_value;
    int max_value;
    bool value_accepted;
} state_home_adjust_fine_t;

typedef struct {
    state_t base;
    uint32_t working_value;
    bool value_accepted;
} state_home_adjust_absolute_t;

static void state_home_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param);
static bool state_home_process(state_t *state_base, state_controller_t *controller);
static bool state_home_process_printing(state_home_t *state, state_controller_t *controller);
static bool state_home_process_calibration(state_home_t *state, state_controller_t *controller);
static void state_home_select_paper_profile(state_controller_t *controller);
static void state_home_check_meter_probe(state_home_t *state, const state_controller_t *controller);
static uint32_t state_home_take_reading(state_home_t *state, state_controller_t *controller);
static uint32_t state_home_take_live_reading(state_home_t *state, state_controller_t *controller);
static void state_home_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state);
static state_home_t state_home_data = {
    .base = {
        .state_entry = state_home_entry,
        .state_process = state_home_process,
        .state_exit = state_home_exit
    },
    .updated_tone_element = 0,
    .live_tone_element = 0,
    .live_tone_ticks = 0,
    .highlight_element = 0,
    .display_dirty = true,
    .tone_dirty = true,
    .dens_selected_mode = 0,
    .dens_reading_type = 0
};

static bool state_home_change_time_increment_process(state_t *state_base, state_controller_t *controller);
static state_home_change_time_increment_t state_home_change_time_increment_data = {
    .base = {
        .state_process = state_home_change_time_increment_process
    }
};

static void state_home_change_mode_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param);
static bool state_home_change_mode_process(state_t *state_base, state_controller_t *controller);
static void state_home_change_mode_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state);
static state_home_change_mode_t state_home_change_mode_data = {
    .base = {
        .state_entry = state_home_change_mode_entry,
        .state_process = state_home_change_mode_process,
        .state_exit = state_home_change_mode_exit
    }
};

static void state_home_adjust_fine_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param);
static bool state_home_adjust_fine_process(state_t *state_base, state_controller_t *controller);
static void state_home_adjust_fine_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state);
static state_home_adjust_fine_t state_home_adjust_fine_data = {
    .base = {
        .state_entry = state_home_adjust_fine_entry,
        .state_process = state_home_adjust_fine_process,
        .state_exit = state_home_adjust_fine_exit
    },
    .working_value = 0,
    .min_value = 0,
    .max_value = 0,
    .value_accepted = false
};

static void state_home_adjust_absolute_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param);
static bool state_home_adjust_absolute_process(state_t *state_base, state_controller_t *controller);
static void state_home_adjust_absolute_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state);
static state_home_adjust_absolute_t state_home_adjust_absolute_data = {
    .base = {
        .state_entry = state_home_adjust_absolute_entry,
        .state_process = state_home_adjust_absolute_process,
        .state_exit = state_home_adjust_absolute_exit
    },
    .working_value = 0,
    .value_accepted = false
};

static void update_display_highlight_element(display_main_printing_elements_t *elements, uint8_t highlight_element);

state_t *state_home()
{
    return (state_t *)&state_home_data;
}

void state_home_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param)
{
    state_home_t *state = (state_home_t *)state_base;
    const enlarger_config_t *enlarger = state_controller_get_enlarger_config(controller);
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);
    const exposure_mode_t mode = exposure_get_mode(exposure_state);
    const bool has_color_mode = enlarger_config_has_rgb(enlarger);

    state->updated_tone_element = 0;
    state->live_tone_element = 0;
    state->live_tone_ticks = 0;
    state->highlight_element = 0;
    state->display_dirty = true;
    state->tone_dirty = true;

    /* Configure keypad actions */
    keypad_action_clear();
    keypad_action_add(KEYPAD_START, ACTION_TIMER, 0, true);
    keypad_action_add(KEYPAD_FOOTSWITCH, ACTION_TIMER, 0, true);
    keypad_action_add(KEYPAD_FOCUS, ACTION_FOCUS, 0, true);
    keypad_action_add(KEYPAD_INC_EXPOSURE, ACTION_INC_EXPOSURE, 0, true);
    keypad_action_add(KEYPAD_DEC_EXPOSURE, ACTION_DEC_EXPOSURE, 0, true);
    keypad_action_add(KEYPAD_INC_CONTRAST, ACTION_INC_CONTRAST, 0, true);
    keypad_action_add(KEYPAD_DEC_CONTRAST, ACTION_DEC_CONTRAST, 0, true);
    keypad_action_add(KEYPAD_ADD_ADJUSTMENT, ACTION_EDIT_ADJUSTMENT, ACTION_LIST_ADJUSTMENTS, true);
    keypad_action_add(KEYPAD_TEST_STRIP, ACTION_TEST_STRIP, ACTION_SELECT_PROFILE, true);
    keypad_action_add(KEYPAD_ENCODER, ACTION_ADJUST_FINE, ACTION_ADJUST_ABSOLUTE, true);
    keypad_action_add(KEYPAD_MENU, ACTION_MENU, 0, false);
    keypad_action_add(KEYPAD_CANCEL, ACTION_CLEAR_READINGS, ACTION_SET_DEFAULTS, true);
    keypad_action_add(KEYPAD_METER_PROBE, ACTION_TAKE_READING, 0, false);
    keypad_action_add_encoder(ACTION_ENCODER_DEC, ACTION_ENCODER_INC);
    keypad_action_add_combo(KEYPAD_INC_EXPOSURE, KEYPAD_DEC_EXPOSURE, ACTION_CHANGE_TIME_INCREMENT);
    keypad_action_add_combo(KEYPAD_INC_CONTRAST, KEYPAD_DEC_CONTRAST, ACTION_CHANGE_MODE);

    if (mode == EXPOSURE_MODE_PRINTING_COLOR && !has_color_mode) {
        exposure_set_mode(exposure_state, EXPOSURE_MODE_PRINTING_BW);
    }
}

bool state_home_process(state_t *state_base, state_controller_t *controller)
{
    state_home_t *state = (state_home_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);
    exposure_mode_t mode = exposure_get_mode(exposure_state);

    state_home_check_meter_probe(state, controller);

    if (mode == EXPOSURE_MODE_PRINTING_BW || mode == EXPOSURE_MODE_PRINTING_COLOR) {
        return state_home_process_printing(state, controller);
    } else if (mode == EXPOSURE_MODE_CALIBRATION) {
        return state_home_process_calibration(state, controller);
    } else {
        return false;
    }
}

bool state_home_process_printing(state_home_t *state, state_controller_t *controller)
{
    const enlarger_config_t *enlarger = state_controller_get_enlarger_config(controller);
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);
    exposure_mode_t mode = exposure_get_mode(exposure_state);

    if (mode == EXPOSURE_MODE_PRINTING_BW
        && state_controller_is_enlarger_focus(controller)
        && meter_probe_is_started(meter_probe_handle())
        && exposure_get_tone_graph(exposure_state) > 0) {
        uint32_t live_tone = state_home_take_live_reading(state, controller);
        if (live_tone > 0) {
            state->live_tone_element = live_tone;
            state->live_tone_ticks = osKernelGetTickCount();
            state->tone_dirty = true;
        }
    }

    if (osKernelGetTickCount() - state->live_tone_ticks > LIVE_TONE_TIMEOUT) {
        state->live_tone_element = 0;
        state->live_tone_ticks = 0;
        state->tone_dirty = true;
    }

    /* Draw current exposure state */
    if (state->display_dirty) {
        display_main_printing_elements_t main_elements;
        if (state->updated_tone_element != 0) {
            /* Blink the most recently added tone graph element */
            uint32_t tone_graph = exposure_get_tone_graph(exposure_state);
            convert_exposure_to_display_printing(&main_elements, exposure_state, enlarger);
            update_display_highlight_element(&main_elements, state->highlight_element);
            display_draw_main_elements_printing(&main_elements);

            if ((tone_graph | state->updated_tone_element) == tone_graph) {
                osDelay(100);
                tone_graph ^= state->updated_tone_element;
                display_redraw_tone_graph(tone_graph, 0);
                osDelay(100);
                tone_graph ^= state->updated_tone_element;
                display_redraw_tone_graph(tone_graph, 0);
            }

            state->updated_tone_element = 0;
            if (main_elements.tone_graph && state->live_tone_element) {
                display_redraw_tone_graph(tone_graph, state->live_tone_element);
            }
        } else {
            convert_exposure_to_display_printing(&main_elements, exposure_state, enlarger);
            update_display_highlight_element(&main_elements, state->highlight_element);
            if (main_elements.tone_graph && state->live_tone_element) {
                main_elements.tone_graph_overlay = state->live_tone_element;
            }
            display_draw_main_elements_printing(&main_elements);
        }

        state->display_dirty = false;
        state->tone_dirty = false;
    } else if (state->tone_dirty) {
        uint32_t overlay_marks = 0;
        uint32_t tone_graph = exposure_get_tone_graph(exposure_state);
        if (tone_graph && state->live_tone_element) {
            overlay_marks = state->live_tone_element;
        }
        display_redraw_tone_graph(tone_graph, overlay_marks);
        state->tone_dirty = false;
    }

    /* Handle the next keypad action */
    keypad_action_t keypad_action;
    if (keypad_action_wait(&keypad_action, STATE_KEYPAD_WAIT) == osOK) {
        if (keypad_action.action_id == ACTION_CHANGE_TIME_INCREMENT) {
            state_controller_set_next_state(controller, STATE_HOME_CHANGE_TIME_INCREMENT, 0);
        } else if (keypad_action.action_id == ACTION_CHANGE_MODE) {
            state_controller_set_next_state(controller, STATE_HOME_CHANGE_MODE, 0);
        } else if (keypad_action.action_id == ACTION_TIMER) {
            state_controller_set_next_state(controller, STATE_TIMER, 0);
        } else if (keypad_action.action_id == ACTION_FOCUS) {
            if (!state_controller_is_enlarger_focus(controller)) {
                log_i("Focus mode enabled");
                illum_controller_safelight_state(ILLUM_SAFELIGHT_FOCUS);
                state_controller_set_enlarger_focus(controller, true);
                state_controller_start_focus_timeout(controller);
                if (mode != EXPOSURE_MODE_PRINTING_COLOR) {
                    state_controller_set_enable_meter_probe(controller, true);
                }
            } else {
                log_i("Focus mode disabled");
                state_controller_set_enlarger_focus(controller, false);
                illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);
                state_controller_stop_focus_timeout(controller);
                state_controller_set_enable_meter_probe(controller, false);
                state->live_tone_element = 0;
                state->live_tone_ticks = 0;
                state->tone_dirty = true;
            }
        } else if (keypad_action.action_id == ACTION_INC_EXPOSURE) {
            if (mode == EXPOSURE_MODE_PRINTING_COLOR) {
                if (state->highlight_element == 0) {
                    exposure_adj_increase(exposure_state);
                } else if (state->highlight_element < 5) {
                    exposure_channel_increase(exposure_state, state->highlight_element - 1, 1);
                }
                state->display_dirty = true;
            } else {
                exposure_adj_increase(exposure_state);
                state->display_dirty = true;
            }
        } else if (keypad_action.action_id == ACTION_DEC_EXPOSURE) {
            if (mode == EXPOSURE_MODE_PRINTING_COLOR) {
                if (state->highlight_element == 0) {
                    exposure_adj_decrease(exposure_state);
                } else if (state->highlight_element < 5) {
                    exposure_channel_decrease(exposure_state, state->highlight_element - 1, 1);
                }
                state->display_dirty = true;
            } else {
                exposure_adj_decrease(exposure_state);
                state->display_dirty = true;
            }
        } else if (keypad_action.action_id == ACTION_INC_CONTRAST) {
            if (mode == EXPOSURE_MODE_PRINTING_COLOR) {
                switch (state->highlight_element) {
                    case 0:
                        state->highlight_element = 4;
                        break;
                    case 1:
                        state->highlight_element = 0;
                        break;
                    case 2:
                        state->highlight_element = 1;
                        break;
                    case 3:
                        state->highlight_element = 2;
                        break;
                    case 4:
                        state->highlight_element = 3;
                        break;
                    default:
                        break;
                }
            } else {
                exposure_contrast_increase(exposure_state);
            }
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_DEC_CONTRAST) {
            if (mode == EXPOSURE_MODE_PRINTING_COLOR) {
                switch (state->highlight_element) {
                    case 0:
                        state->highlight_element = 1;
                        break;
                    case 1:
                        state->highlight_element = 2;
                        break;
                    case 2:
                        state->highlight_element = 3;
                        break;
                    case 3:
                        state->highlight_element = 4;
                        break;
                    case 4:
                        state->highlight_element = 0;
                        break;
                    default:
                        break;
                }
            } else {
                exposure_contrast_decrease(exposure_state);
            }
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_ENCODER_INC) {
            if (mode == EXPOSURE_MODE_PRINTING_COLOR && state->highlight_element > 0) {
                exposure_channel_increase(exposure_state, state->highlight_element - 1, keypad_action.count);
                state->display_dirty = true;
            }
        } else if (keypad_action.action_id == ACTION_ENCODER_DEC) {
            if (mode == EXPOSURE_MODE_PRINTING_COLOR && state->highlight_element > 0) {
                exposure_channel_decrease(exposure_state, state->highlight_element - 1, keypad_action.count);
                state->display_dirty = true;
            }
        } else if (keypad_action.action_id == ACTION_EDIT_ADJUSTMENT) {
            int burn_dodge_count = exposure_burn_dodge_count(exposure_state);
            if (burn_dodge_count < EXPOSURE_BURN_DODGE_MAX) {
                state_controller_set_next_state(controller, STATE_EDIT_ADJUSTMENT, burn_dodge_count);
            }
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_LIST_ADJUSTMENTS) {
            int burn_dodge_count = exposure_burn_dodge_count(exposure_state);
            if (burn_dodge_count > 0) {
                state_controller_set_next_state(controller, STATE_LIST_ADJUSTMENTS, 0);
            }
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_TEST_STRIP) {
            state_controller_set_next_state(controller, STATE_TEST_STRIP, 0);
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_SELECT_PROFILE) {
            if (mode == EXPOSURE_MODE_PRINTING_BW) {
                state_home_select_paper_profile(controller);
                state->display_dirty = true;
            }
        } else if (keypad_action.action_id == ACTION_ADJUST_FINE) {
            state_controller_set_next_state(controller, STATE_HOME_ADJUST_FINE, 0);
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_ADJUST_ABSOLUTE) {
            state_controller_set_next_state(controller, STATE_HOME_ADJUST_ABSOLUTE, 0);
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_MENU) {
            state_controller_set_next_state(controller, STATE_MENU, 0);
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_CLEAR_READINGS) {
            exposure_clear_meter_readings(exposure_state);
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_SET_DEFAULTS) {
            exposure_clear_meter_readings(exposure_state);
            exposure_state_defaults(exposure_state);
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_TAKE_READING) {
            if (mode != EXPOSURE_MODE_PRINTING_COLOR
                && state_controller_is_enlarger_focus(controller) && meter_probe_is_started(meter_probe_handle())) {
                state->updated_tone_element = state_home_take_reading(state, controller);
                state->display_dirty = true;
            }
        }
        return true;
    } else {
        return false;
    }
}

bool state_home_process_calibration(state_home_t *state, state_controller_t *controller)
{
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);

    /* Draw current exposure state */
    if (state->display_dirty) {
        display_main_calibration_elements_t main_elements;
        convert_exposure_to_display_calibration(&main_elements, exposure_state);
        display_draw_main_elements_calibration(&main_elements);

        state->display_dirty = false;
    }

    /* Handle the next keypad action */
    keypad_action_t keypad_action;
    if (keypad_action_wait(&keypad_action, STATE_KEYPAD_WAIT) == osOK) {
        if (keypad_action.action_id == ACTION_CHANGE_TIME_INCREMENT) {
            state_controller_set_next_state(controller, STATE_HOME_CHANGE_TIME_INCREMENT, 0);
        } else if (keypad_action.action_id == ACTION_CHANGE_MODE) {
            state_controller_set_next_state(controller, STATE_HOME_CHANGE_MODE, 0);
        } else if (keypad_action.action_id == ACTION_TIMER) {
            state_controller_set_next_state(controller, STATE_TIMER, 0);
        } else if (keypad_action.action_id == ACTION_FOCUS) {
            if (!state_controller_is_enlarger_focus(controller)) {
                log_i("Focus mode enabled");
                illum_controller_safelight_state(ILLUM_SAFELIGHT_FOCUS);
                state_controller_set_enlarger_focus(controller, true);
                state_controller_start_focus_timeout(controller);
                state_controller_set_enable_meter_probe(controller, true);
            } else {
                log_i("Focus mode disabled");
                state_controller_set_enlarger_focus(controller, false);
                illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);
                state_controller_stop_focus_timeout(controller);
                state_controller_set_enable_meter_probe(controller, false);
            }
        } else if (keypad_action.action_id == ACTION_INC_EXPOSURE) {
            exposure_adj_increase(exposure_state);
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_DEC_EXPOSURE) {
            exposure_adj_decrease(exposure_state);
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_INC_CONTRAST) {
            exposure_calibration_pev_increase(exposure_state);
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_DEC_CONTRAST) {
            exposure_calibration_pev_decrease(exposure_state);
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_EDIT_ADJUSTMENT) {
            exposure_pev_preset_t preset = exposure_calibration_pev_get_preset(exposure_state);
            if (preset == EXPOSURE_PEV_PRESET_BASE) {
                exposure_calibration_pev_set_preset(exposure_state, EXPOSURE_PEV_PRESET_STRIP);
            } else if (preset == EXPOSURE_PEV_PRESET_STRIP) {
                exposure_calibration_pev_set_preset(exposure_state, EXPOSURE_PEV_PRESET_BASE);
            }
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_TEST_STRIP) {
            state_controller_set_next_state(controller, STATE_TEST_STRIP, 0);
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_ADJUST_FINE) {
            state_controller_set_next_state(controller, STATE_HOME_ADJUST_FINE, 0);
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_ADJUST_ABSOLUTE) {
            state_controller_set_next_state(controller, STATE_HOME_ADJUST_ABSOLUTE, 0);
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_MENU) {
            state_controller_set_next_state(controller, STATE_MENU, 0);
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_CLEAR_READINGS) {
            exposure_clear_meter_readings(exposure_state);
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_SET_DEFAULTS) {
            exposure_clear_meter_readings(exposure_state);
            exposure_state_defaults(exposure_state);
            state->display_dirty = true;
        } else if (keypad_action.action_id == ACTION_TAKE_READING) {
            if (state_controller_is_enlarger_focus(controller) && meter_probe_is_started(meter_probe_handle())) {
                state->updated_tone_element = state_home_take_reading(state, controller);
                state->display_dirty = true;
            }
        }
        return true;
    } else {
        return false;
    }
}

void state_home_select_paper_profile(state_controller_t *controller)
{
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);
    paper_profile_t *profile_list;
    profile_list = pvPortMalloc(sizeof(paper_profile_t) * MAX_PAPER_PROFILES);
    if (!profile_list) {
        log_e("Unable to allocate memory for profile list");
        return;
    }

    char buf[640];
    size_t offset;
    uint8_t profile_index = 0;
    size_t profile_count = 0;
    uint8_t option = 1;

    profile_count = 0;
    profile_index = exposure_get_active_paper_profile_index(exposure_state);
    for (size_t i = 0; i < MAX_PAPER_PROFILES; i++) {
        if (!settings_get_paper_profile(&profile_list[i], i)) {
            break;
        } else {
            profile_count = i + 1;
        }
    }
    log_i("Loaded %d profiles, selected is %d", profile_count, profile_index);
    if (profile_count == 0) {
        log_w("No profiles available");
        return;
    }

    if (profile_index >= profile_count) {
        option = 1;
    } else {
        option = profile_index + 1;
    }

    offset = 0;
    for (size_t i = 0; i < profile_count; i++) {
        if (strlen(profile_list[i].name) > 0) {
            sprintf(buf + offset, "%s %s",
                ((i == profile_index) ? "-->" : "   "),
                profile_list[i].name);
        } else {
            sprintf(buf + offset, "%s Paper profile %d",
                ((i == profile_index) ? "-->" : "   "),
                i + 1);
        }
        offset += pad_str_to_length(buf + offset, ' ', DISPLAY_MENU_ROW_LENGTH);
        if (i < profile_count) {
            buf[offset++] = '\n';
            buf[offset] = '\0';
        }
    }

    do {

        option = display_selection_list("Paper Profiles", option, buf);
        if (option > 0 && option <= profile_count) {
            exposure_set_active_paper_profile_index(exposure_state, option - 1);
            break;
        }
    } while (option != 0 && option != UINT8_MAX);

    vPortFree(profile_list);
}

void state_home_check_meter_probe(state_home_t *state, const state_controller_t *controller)
{
    /* Start with an integration time of 100ms */
    static uint16_t sample_time = 719;
    static uint16_t sample_count = 99;

    meter_probe_handle_t *handle = meter_probe_handle();

    if (state_controller_is_enable_meter_probe(controller)) {
        if (meter_probe_is_attached(handle) && !meter_probe_is_started(handle)) {
            if (meter_probe_start(handle) == osOK) {
                meter_probe_sensor_set_gain(handle, TSL2585_GAIN_256X);
                meter_probe_sensor_set_integration(handle, sample_time, sample_count);
                meter_probe_sensor_set_mod_calibration(handle, 1);
                meter_probe_sensor_enable_agc(handle, sample_count);
                meter_probe_sensor_enable(handle);
            }
        }
    } else {
        if (meter_probe_is_started(handle)) {
            meter_probe_sensor_disable(handle);
            meter_probe_stop(handle);
        }
    }
}

uint32_t state_home_take_reading(state_home_t *state, state_controller_t *controller)
{
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);
    meter_probe_result_t result = METER_READING_OK;
    float lux = 0;
    uint32_t updated_tone_element = 0;

    display_draw_mode_text("Measuring");
    buzzer_sequence(BUZZER_SEQUENCE_PROBE_START);

    do {
        illum_controller_safelight_state(ILLUM_SAFELIGHT_MEASUREMENT);
        osDelay(SAFELIGHT_OFF_DELAY / 2);

        result = meter_probe_measure(meter_probe_handle(), &lux);
        if (result != METER_READING_OK) {
            break;
        }

    } while (0);

    illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);

    if (result == METER_READING_OK) {
        buzzer_sequence(BUZZER_SEQUENCE_PROBE_SUCCESS);
        updated_tone_element = exposure_add_meter_reading(exposure_state, lux);
        log_i("Measured PEV=%lu (Lux=%f)", exposure_get_calibration_pev(exposure_state), lux);
    } else if (result == METER_READING_LOW) {
        display_draw_mode_text("Light Low");
        buzzer_sequence(BUZZER_SEQUENCE_PROBE_WARNING);
        osDelay(pdMS_TO_TICKS(2000));
    } else if (result == METER_READING_HIGH) {
        display_draw_mode_text("Light High");
        buzzer_sequence(BUZZER_SEQUENCE_PROBE_WARNING);
        osDelay(pdMS_TO_TICKS(2000));
    } else if (result == METER_READING_TIMEOUT) {
        display_draw_mode_text("Timeout");
        buzzer_sequence(BUZZER_SEQUENCE_PROBE_ERROR);
        osDelay(pdMS_TO_TICKS(2000));
    } else if (result == METER_READING_FAIL) {
        display_draw_mode_text("Meter Error");
        buzzer_sequence(BUZZER_SEQUENCE_PROBE_ERROR);
        osDelay(pdMS_TO_TICKS(2000));
    }
    return updated_tone_element;
}

uint32_t state_home_take_live_reading(state_home_t *state, state_controller_t *controller)
{
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);
    meter_probe_result_t result = METER_READING_OK;
    float lux = 0;
    uint32_t live_tone_element = 0;

    result = meter_probe_try_measure(meter_probe_handle(), &lux);

    if (result == METER_READING_OK) {
        live_tone_element = exposure_get_meter_reading_tone(exposure_state, lux);
    }

    return live_tone_element;
}

void state_home_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state)
{
    state_home_t *state = (state_home_t *)state_base;

    if (next_state != STATE_HOME_CHANGE_TIME_INCREMENT
        && next_state != STATE_HOME_ADJUST_FINE
        && next_state != STATE_HOME_ADJUST_ABSOLUTE
        && next_state != STATE_EDIT_ADJUSTMENT
        && next_state != STATE_LIST_ADJUSTMENTS) {
        if (state_controller_is_enlarger_focus(controller)) {
            log_i("Focus mode disabled due to state change");
            illum_controller_safelight_state(ILLUM_SAFELIGHT_HOME);
            state_controller_set_enlarger_focus(controller, false);
            state_controller_stop_focus_timeout(controller);
            state_controller_set_enable_meter_probe(controller, false);
            state->live_tone_element = 0;
            state->live_tone_ticks = 0;
            state->tone_dirty = true;
            state_home_check_meter_probe(state, controller);
        }
    }
}

state_t *state_home_change_time_increment()
{
    return (state_t *)&state_home_change_time_increment_data;
}

bool state_home_change_time_increment_process(state_t *state_base, state_controller_t *controller)
{
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);

    // Draw current stop increment
    uint8_t stop_inc_den = exposure_adj_increment_get_denominator(exposure_state);
    display_draw_stop_increment(stop_inc_den);

    // Handle the next keypad event
    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, STATE_KEYPAD_WAIT) == HAL_OK) {
        if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_EXPOSURE)) {
            exposure_adj_increment_increase(exposure_state);
        } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_EXPOSURE)) {
            exposure_adj_increment_decrease(exposure_state);
        } else if ((keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed)
                   || (keypad_usb_get_keypad_equivalent(&keypad_event) == KEYPAD_CANCEL && keypad_event.pressed)) {
            state_controller_set_next_state(controller, STATE_HOME, 0);
        }
        return true;
    } else {
        return false;
    }
}

state_t *state_home_change_mode()
{
    return (state_t *)&state_home_change_mode_data;
}

void state_home_change_mode_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param)
{
    state_home_change_mode_t *state = (state_home_change_mode_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);

    state->prev_state = prev_state;
    state->working_value = (prev_state == STATE_DENSITOMETER) ? EXPOSURE_MODE_DENSITOMETER : exposure_get_mode(exposure_state);
    state->accepted = false;
}

bool state_home_change_mode_process(state_t *state_base, state_controller_t *controller)
{
    state_home_change_mode_t *state = (state_home_change_mode_t *)state_base;
    const enlarger_config_t *enlarger = state_controller_get_enlarger_config(controller);

    const bool has_color_mode = enlarger_config_has_rgb(enlarger);

    /* Draw the current mode */
    switch (state->working_value) {
    case EXPOSURE_MODE_PRINTING_BW:
        display_draw_mode_text(has_color_mode ? "B&W Printing" : "Printing");
        break;
    case EXPOSURE_MODE_PRINTING_COLOR:
        display_draw_mode_text("Color Printing");
        break;
    case EXPOSURE_MODE_DENSITOMETER:
        display_draw_mode_text("Densitometer");
        break;
    case EXPOSURE_MODE_CALIBRATION:
        display_draw_mode_text("Calibration");
        break;
    default:
        display_draw_mode_text("Unknown");
        break;
    }

    /* Handle the next keypad event */
    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, STATE_KEYPAD_WAIT) == HAL_OK) {
        if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_CONTRAST)) {
            switch (state->working_value) {
            case EXPOSURE_MODE_PRINTING_BW:
                state->working_value = has_color_mode ? EXPOSURE_MODE_PRINTING_COLOR : EXPOSURE_MODE_DENSITOMETER;
                break;
            case EXPOSURE_MODE_PRINTING_COLOR:
                state->working_value = EXPOSURE_MODE_DENSITOMETER;
                break;
            case EXPOSURE_MODE_DENSITOMETER:
                state->working_value = EXPOSURE_MODE_CALIBRATION;
                break;
            case EXPOSURE_MODE_CALIBRATION:
                state->working_value = EXPOSURE_MODE_PRINTING_BW;
                break;
            default:
                state->working_value = EXPOSURE_MODE_PRINTING_BW;
                break;
            }
        } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_CONTRAST)) {
            switch (state->working_value) {
            case EXPOSURE_MODE_CALIBRATION:
                state->working_value = EXPOSURE_MODE_DENSITOMETER;
                break;
            case EXPOSURE_MODE_DENSITOMETER:
                state->working_value = has_color_mode ? EXPOSURE_MODE_PRINTING_COLOR : EXPOSURE_MODE_PRINTING_BW;
                break;
            case EXPOSURE_MODE_PRINTING_COLOR:
                state->working_value = EXPOSURE_MODE_PRINTING_BW;
                break;
            case EXPOSURE_MODE_PRINTING_BW:
                state->working_value = EXPOSURE_MODE_CALIBRATION;
                break;
            default:
                state->working_value = EXPOSURE_MODE_PRINTING_BW;
                break;
            }
        } else if (keypad_event.key == KEYPAD_MENU && !keypad_event.pressed) {
            state->accepted = true;
            if (state->working_value == EXPOSURE_MODE_DENSITOMETER) {
                state_controller_set_next_state(controller, STATE_DENSITOMETER, 0);
            } else {
                state_controller_set_next_state(controller, STATE_HOME, 0);
            }
        } else if ((keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed)
                   || (keypad_usb_get_keypad_equivalent(&keypad_event) == KEYPAD_CANCEL && keypad_event.pressed)) {
            state_controller_set_next_state(controller, state->prev_state, 0);
        }
        return true;
    } else {
        return false;
    }
}

void state_home_change_mode_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state)
{
    state_home_change_mode_t *state = (state_home_change_mode_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);

    if (state->accepted && state->working_value != EXPOSURE_MODE_DENSITOMETER) {
        exposure_set_mode(exposure_state, state->working_value);
    }
}

state_t *state_home_adjust_fine()
{
    return (state_t *)&state_home_adjust_fine_data;
}

void state_home_adjust_fine_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param)
{
    state_home_adjust_fine_t *state = (state_home_adjust_fine_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);

    state->working_value = exposure_adj_get(exposure_state);
    state->min_value = exposure_adj_min(exposure_state);
    state->max_value = exposure_adj_max(exposure_state);
    state->value_accepted = false;
}

bool state_home_adjust_fine_process(state_t *state_base, state_controller_t *controller)
{
    state_home_adjust_fine_t *state = (state_home_adjust_fine_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);
    uint32_t tone_graph = exposure_get_adjusted_tone_graph(exposure_state, state->working_value);

    display_draw_exposure_adj(state->working_value, tone_graph);

    // Handle the next keypad event
    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, STATE_KEYPAD_WAIT) == HAL_OK) {
        if (keypad_event.key == KEYPAD_ENCODER_CW) {
            if (state->working_value <= state->max_value) {
                state->working_value = MIN(state->working_value + keypad_event.count, state->max_value);
            }
        } else if (keypad_event.key == KEYPAD_ENCODER_CCW) {
            if (state->working_value >= state->min_value) {
                state->working_value = MAX(state->working_value - keypad_event.count, state->min_value);
            }
        } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_ENCODER)) {
            state->value_accepted = true;
            state_controller_set_next_state(controller, STATE_HOME, 0);
        } else if ((keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed)
                   || (keypad_usb_get_keypad_equivalent(&keypad_event) == KEYPAD_CANCEL && keypad_event.pressed)) {
            state_controller_set_next_state(controller, STATE_HOME, 0);
        }

        return true;
    } else {
        return false;
    }
}

void state_home_adjust_fine_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state)
{
    state_home_adjust_fine_t *state = (state_home_adjust_fine_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);

    if (state->value_accepted) {
        exposure_adj_set(exposure_state, state->working_value);
    }
}

state_t *state_home_adjust_absolute()
{
    return (state_t *)&state_home_adjust_absolute_data;
}

void state_home_adjust_absolute_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param)
{
    state_home_adjust_absolute_t *state = (state_home_adjust_absolute_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);

    state->working_value = rounded_exposure_time_ms(exposure_get_exposure_time(exposure_state));
    state->value_accepted = false;
}

bool state_home_adjust_absolute_process(state_t *state_base, state_controller_t *controller)
{
    state_home_adjust_absolute_t *state = (state_home_adjust_absolute_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);
    uint32_t tone_graph = exposure_get_absolute_tone_graph(exposure_state, state->working_value / 1000.0f);

    display_exposure_timer_t elements;
    convert_exposure_to_display_timer(&elements, state->working_value);

    display_draw_timer_adj(&elements, tone_graph);

    // Handle the next keypad event
    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, STATE_KEYPAD_WAIT) == HAL_OK) {
        if (keypad_event.key == KEYPAD_ENCODER_CW) {
            do {
                if (state->working_value < 10000) {
                    state->working_value += 10;
                    if (state->working_value > 10000) { state->working_value = 10000; }
                } else if (state->working_value < 100000) {
                    state->working_value += 100;
                    if (state->working_value > 100000) { state->working_value = 100000; }
                } else if (state->working_value < 999000) {
                    state->working_value += 1000;
                    if (state->working_value > 999000) { state->working_value = 999000; }
                }
                keypad_event.count--;
            } while (keypad_event.count > 0);
        } else if (keypad_event.key == KEYPAD_ENCODER_CCW) {
            do {
                if (state->working_value <= 10000) {
                    state->working_value -= 10;
                    if (state->working_value < 10 || state->working_value > 1000000) { state->working_value = 10; }
                } else if (state->working_value <= 100000) {
                    state->working_value -= 100;
                    if (state->working_value < 10000) { state->working_value = 10000; }
                } else if (state->working_value <= 999000) {
                    state->working_value -= 1000;
                    if (state->working_value < 100000) { state->working_value = 100000; }
                }
                keypad_event.count--;
            } while (keypad_event.count > 0);
        } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_EXPOSURE)) {
            if (state->working_value < 10000) {
                state->working_value += 100;
                if (state->working_value > 10000) { state->working_value = 10000; }
            } else if (state->working_value < 100000) {
                state->working_value += 1000;
                if (state->working_value > 100000) { state->working_value = 100000; }
            } else if (state->working_value < 999000) {
                state->working_value += 10000;
                if (state->working_value > 999000) { state->working_value = 999000; }
            }
        } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_EXPOSURE)) {
            if (state->working_value <= 10000) {
                state->working_value -= 100;
                if (state->working_value < 10 || state->working_value > 1000000) { state->working_value = 10; }
            } else if (state->working_value <= 100000) {
                state->working_value -= 1000;
                if (state->working_value < 10000) { state->working_value = 10000; }
            } else if (state->working_value <= 999000) {
                state->working_value -= 10000;
                if (state->working_value < 100000) { state->working_value = 100000; }
            }
        } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_ENCODER)) {
            state->value_accepted = true;
            state_controller_set_next_state(controller, STATE_HOME, 0);
        } else if ((keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed)
                   || (keypad_usb_get_keypad_equivalent(&keypad_event) == KEYPAD_CANCEL && keypad_event.pressed)) {
            state_controller_set_next_state(controller, STATE_HOME, 0);
        }

        return true;
    } else {
        return false;
    }
}

void state_home_adjust_absolute_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state)
{
    state_home_adjust_absolute_t *state = (state_home_adjust_absolute_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);

    if (state->value_accepted) {
        exposure_set_base_time(exposure_state, state->working_value / 1000.0f);
    }
}

void update_display_highlight_element(display_main_printing_elements_t *elements, uint8_t highlight_element)
{
    if (!elements) { return; }

    if (elements->printing_type == DISPLAY_MAIN_PRINTING_COLOR) {
        if (highlight_element == 0) {
            elements->color.ch_highlight = 0;
            elements->time_icon_highlight = true;
        } else if (highlight_element < 5) {
            elements->color.ch_highlight = highlight_element;
            elements->time_icon_highlight = false;
        } else {
            elements->color.ch_highlight = 0;
            elements->time_icon_highlight = true;
        }
    } else {
        elements->time_icon_highlight = false;
    }
}
