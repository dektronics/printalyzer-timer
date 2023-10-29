#include "state_adjustment.h"

#include <FreeRTOS.h>
#include <cmsis_os.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#define LOG_TAG "state_adjustment"
#include <elog.h>

#include "exposure_state.h"
#include "display.h"
#include "keypad.h"
#include "util.h"

typedef struct {
    state_t base;
    uint8_t working_index;
    exposure_burn_dodge_t working_value;
    uint8_t stop_inc_den;
    bool from_list;
    bool value_accepted;
} state_edit_adjustment_t;

typedef struct {
    state_t base;
    uint8_t starting_index;
} state_list_adjustments_t;

static void state_edit_adjustment_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param);
static bool state_edit_adjustment_process(state_t *state_base, state_controller_t *controller);
static void state_edit_adjustment_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state);
static state_edit_adjustment_t state_edit_adjustment_data = {
    .base = {
        .state_entry = state_edit_adjustment_entry,
        .state_process = state_edit_adjustment_process,
        .state_exit = state_edit_adjustment_exit
    }
};

static void state_list_adjustments_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param);
static bool state_list_adjustments_process(state_t *state_base, state_controller_t *controller);
static void state_list_adjustments_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state);

static int state_list_adjustments_append_entry(char *buf, const exposure_burn_dodge_t *entry, int index);
static bool state_list_adjustments_delete_prompt(exposure_state_t *exposure_state, int index);

static state_list_adjustments_t state_list_adjustments_data = {
    .base = {
        .state_entry = state_list_adjustments_entry,
        .state_process = state_list_adjustments_process,
        .state_exit = state_list_adjustments_exit
    }
};

state_t *state_edit_adjustment()
{
    return (state_t *)&state_edit_adjustment_data;
}

void state_edit_adjustment_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param)
{
    state_edit_adjustment_t *state = (state_edit_adjustment_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);
    log_i("Edit Exposure Adjustment");

    state->stop_inc_den = exposure_adj_increment_get_denominator(exposure_state);

    uint32_t param_index = param & 0x7FFFFFFF;
    bool param_from_list = (param & 0x80000000) != 0;

    const exposure_burn_dodge_t *state_entry = exposure_burn_dodge_get(exposure_state, param_index);
    if (state_entry) {
        memcpy(&state->working_value, state_entry, sizeof(exposure_burn_dodge_t));
    } else {
        memset(&state->working_value, 0, sizeof(exposure_burn_dodge_t));
    }

    if (param_index < EXPOSURE_BURN_DODGE_MAX) {
        state->working_index = param_index;
    } else {
        state->working_index = UINT8_MAX;
    }

    if (state->working_value.numerator == 0) {
        state->working_value.denominator = state->stop_inc_den;
        state->working_value.contrast_grade = CONTRAST_GRADE_MAX;
    }

    state->from_list = (prev_state == STATE_LIST_ADJUSTMENTS) || param_from_list;
    state->value_accepted = false;
}

bool state_edit_adjustment_process(state_t *state_base, state_controller_t *controller)
{
    state_edit_adjustment_t *state = (state_edit_adjustment_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);
    char buf_adj_title1[32];
    char buf_adj_title2[32];
    char buf_base_title2[32];
    uint32_t tone_graph;
    bool time_too_short = false;

    if (state->working_value.numerator == 0) {
        sprintf(buf_adj_title1, "Burn/Dodge");
        sprintf(buf_adj_title2, "Area %d", state->working_index + 1);
    } else {
        float adj_stops = (float)state->working_value.numerator / (float)state->working_value.denominator;
        float adj_time = exposure_get_exposure_time(exposure_state) * powf(2.0f, adj_stops);
        float min_exposure_time = exposure_get_min_exposure_time(exposure_state);

        if (state->working_value.numerator > 0) {
            float burn_time = adj_time - exposure_get_exposure_time(exposure_state);
            sprintf(buf_adj_title1, "Burn Area %d", state->working_index + 1);
            size_t offset = sprintf(buf_adj_title2, "+");
            offset += append_exposure_time(buf_adj_title2 + offset, burn_time);
            if (state->working_value.contrast_grade < CONTRAST_GRADE_MAX) {
                sprintf(buf_adj_title2 + offset, " [%s]", contrast_grade_str(state->working_value.contrast_grade));
            } else {
                sprintf(buf_adj_title2 + offset, " [B]");
            }
            time_too_short = (min_exposure_time > 0) && (burn_time < min_exposure_time);
        } else {
            float dodge_time = fabsf(exposure_get_exposure_time(exposure_state) - adj_time);
            float adj_base_time = exposure_get_exposure_time(exposure_state) - dodge_time;
            sprintf(buf_adj_title1, "Dodge Area %d", state->working_index + 1);
            size_t offset = sprintf(buf_adj_title2, "-");
            append_exposure_time(buf_adj_title2 + offset, dodge_time);
            time_too_short = (min_exposure_time > 0)
                && ((dodge_time < min_exposure_time) || (adj_base_time < min_exposure_time));
        }
    }

    size_t offset = append_exposure_time(buf_base_title2, exposure_get_exposure_time(exposure_state));
    sprintf(buf_base_title2 + offset, " [%s]", contrast_grade_str(exposure_get_contrast_grade(exposure_state)));

    tone_graph = exposure_get_burn_dodge_tone_graph(exposure_state, &(state->working_value));

    display_edit_adjustment_elements_t elements = {
        .tone_graph = tone_graph,
        .adj_title1 = buf_adj_title1,
        .adj_title2 = buf_adj_title2,
        .base_title1 = "Base Exposure",
        .base_title2 = buf_base_title2,
        .tip_up = state->working_value.numerator == 0 ? "Burn" : 0,
        .tip_down = (state->working_value.numerator == 0 && state->working_index == 0) ? "Dodge" : 0,
        .adj_num = state->working_value.numerator,
        .adj_den = state->working_value.denominator,
        .time_too_short = time_too_short
    };

    display_draw_edit_adjustment_elements(&elements);

    keypad_event_t keypad_event;
    if (keypad_wait_for_event(&keypad_event, -1) == HAL_OK) {
        if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_EXPOSURE)) {
            // Check if the main stops increment has changed
            if (state->working_value.denominator != state->stop_inc_den) {
                // Convert the setting to a floating point stops value
                float adj_stops = (float)state->working_value.numerator / (float)state->working_value.denominator;

                // Get the numerator for the new increment, rounding up if necessary
                int8_t updated_numerator = (int8_t)ceilf(adj_stops * state->stop_inc_den);

                // Check if the value actually changed, and increment if it did not
                float updated_stops = (float)updated_numerator / (float)state->stop_inc_den;
                if (fabsf(adj_stops - updated_stops) > 0.001f
                    && updated_numerator < state->stop_inc_den * 9) {
                    updated_numerator++;
                }
                state->working_value.numerator = updated_numerator;
                state->working_value.denominator = state->stop_inc_den;
            } else {
                if (state->working_value.numerator < state->working_value.denominator * 9) {
                    state->working_value.numerator++;
                }
            }
        } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_EXPOSURE)) {
            if (state->working_index == 0 || state->working_value.numerator > 0) {
                // Check if the main stops increment has changed
                if (state->working_value.denominator != state->stop_inc_den) {
                    // Convert the setting to a floating point stops value
                    float adj_stops = (float)state->working_value.numerator / (float)state->working_value.denominator;

                    // Get the numerator for the new increment, rounding down if necessary
                    int8_t updated_numerator = (int8_t)floorf(adj_stops * state->stop_inc_den);

                    // Check if the value actually changed, and decrement if it did not
                    float updated_stops = (float)updated_numerator / (float)state->stop_inc_den;
                    if (fabsf(adj_stops - updated_stops) > 0.001f
                        && updated_numerator > state->stop_inc_den * -9) {
                        updated_numerator--;
                    }
                    state->working_value.numerator = updated_numerator;
                    state->working_value.denominator = state->stop_inc_den;
                } else {
                    if (state->working_value.numerator > state->working_value.denominator * -9) {
                        state->working_value.numerator--;
                    }
                }
            }
        } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_INC_CONTRAST)) {
            if (state->working_value.numerator > 0) {
                if (state->working_value.contrast_grade == CONTRAST_GRADE_MAX) {
                    state->working_value.contrast_grade = CONTRAST_GRADE_00;
                } else {
                    state->working_value.contrast_grade++;
                }
            }
        } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_DEC_CONTRAST)) {
            if (state->working_value.numerator > 0) {
                if (state->working_value.contrast_grade == CONTRAST_GRADE_00) {
                    state->working_value.contrast_grade = CONTRAST_GRADE_MAX;
                } else {
                    state->working_value.contrast_grade--;
                }
            }
        } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_ADD_ADJUSTMENT)) {
            if (state->working_value.numerator != 0) {
                state->value_accepted = true;
                if (state->working_index + 1 < EXPOSURE_BURN_DODGE_MAX) {
                    uint32_t param = state->working_index + 1;
                    if (state->from_list) {
                        param |= 0x80000000;
                    }
                    state_controller_set_next_state(controller, STATE_EDIT_ADJUSTMENT, param);
                } else {
                    state_controller_set_next_state(controller, STATE_HOME, 0);
                }
            }
        } else if (keypad_is_key_released_or_repeated(&keypad_event, KEYPAD_MENU)) {
            if (state->working_value.numerator != 0) {
                state->value_accepted = true;
                if (state->from_list) {
                    state_controller_set_next_state(controller, STATE_LIST_ADJUSTMENTS, state->working_index);
                } else {
                    state_controller_set_next_state(controller, STATE_HOME, 0);
                }
            }
        } else if (keypad_event.key == KEYPAD_CANCEL && !keypad_event.pressed) {
            state->value_accepted = false;
            state_controller_set_next_state(controller, STATE_HOME, 0);
        }
    }

    return true;
}

void state_edit_adjustment_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state)
{
    state_edit_adjustment_t *state = (state_edit_adjustment_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);
    if (state->value_accepted && state->working_value.numerator != 0) {
        // Only allow contrast selection for burn exposures
        if (state->working_value.numerator < 0) {
            state->working_value.contrast_grade = CONTRAST_GRADE_MAX;
        }

        // Copy the working value to the exposure state.
        exposure_burn_dodge_set(exposure_state, &state->working_value, state->working_index);
    }
}

state_t *state_list_adjustments()
{
    return (state_t *)&state_list_adjustments_data;
}

void state_list_adjustments_entry(state_t *state_base, state_controller_t *controller, state_identifier_t prev_state, uint32_t param)
{
    state_list_adjustments_t *state = (state_list_adjustments_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);

    log_i("List Exposure Adjustments");

    // Logging printout of adjustment list
    for (int i = 0; i < exposure_burn_dodge_count(exposure_state); i++) {
        const exposure_burn_dodge_t *entry = exposure_burn_dodge_get(exposure_state, i);
        log_i("[%d] %d/%d [%s]", i + 1,
            entry->numerator, entry->denominator,
            contrast_grade_str(entry->contrast_grade));
    }
    state->starting_index = param;
}

int state_list_adjustments_append_entry(char *buf, const exposure_burn_dodge_t *entry, int index)
{
    int offset = 0;
    bool num_positive = entry->numerator >= 0;

    offset += sprintf(buf + offset, "[%d] %s",
        index + 1,
        num_positive ? "Burn  " : "Dodge ");

    append_signed_fraction(buf + offset, entry->numerator, entry->denominator);

    // Pad the fraction out to its max possible length for alignment
    offset += pad_str_to_length(buf + offset, ' ', 8);

    if (entry->contrast_grade != CONTRAST_GRADE_MAX) {
        offset += sprintf(buf + offset, " [Grade %s]",
            contrast_grade_str(entry->contrast_grade));
    }

    return offset;
}

bool state_list_adjustments_process(state_t *state_base, state_controller_t *controller)
{
    state_list_adjustments_t *state = (state_list_adjustments_t *)state_base;
    exposure_state_t *exposure_state = state_controller_get_exposure_state(controller);
    char buf[512];
    int offset = 0;
    int count = exposure_burn_dodge_count(exposure_state);

    for (int i = 0; i < count; i++) {
        const exposure_burn_dodge_t *entry = exposure_burn_dodge_get(exposure_state, i);

        state_list_adjustments_append_entry(buf + offset, entry, i);

        // Pad the end out to max length for alignment
        offset += pad_str_to_length(buf + offset, ' ', DISPLAY_MENU_ROW_LENGTH);

        // Add a line break if this is not the last item
        if (i < count - 1) {
            offset += sprintf(buf + offset, "\n");
        }
    }

    uint8_t starting_pos;
    if (state->starting_index < exposure_burn_dodge_count(exposure_state)) {
        starting_pos = state->starting_index + 1;
    } else {
        starting_pos = 1;
    }

    uint16_t result = display_selection_list_params("Burn/Dodge Adjustments",
        starting_pos, buf,
        DISPLAY_MENU_ACCEPT_MENU | DISPLAY_MENU_ACCEPT_ADD_ADJUSTMENT);
    uint8_t option = (uint8_t)(result & 0x00FF);
    keypad_key_t option_key = (uint8_t)((result & 0xFF00) >> 8);

    if (result == UINT16_MAX) {
        state_controller_set_next_state(controller, STATE_HOME, 0);
        return false;
    } else if (option > 0) {
        if (option_key == KEYPAD_MENU) {
            bool result = state_list_adjustments_delete_prompt(exposure_state, option - 1);
            if (!result || exposure_burn_dodge_count(exposure_state) == 0) {
                state_controller_set_next_state(controller, STATE_HOME, 0);
            }
            return result;
        } else if (option_key == KEYPAD_ADD_ADJUSTMENT) {
            state_controller_set_next_state(controller, STATE_EDIT_ADJUSTMENT, option - 1);
        }
    } else {
        state_controller_set_next_state(controller, STATE_HOME, 0);
    }
    return true;
}

bool state_list_adjustments_delete_prompt(exposure_state_t *exposure_state, int index)
{
    const exposure_burn_dodge_t *entry = exposure_burn_dodge_get(exposure_state, index);
    char buf1[64];
    char buf2[64];

    if (entry->numerator < 0) {
        sprintf(buf1, "Dodge Area %d", index + 1);
    } else {
        sprintf(buf1, "Burn Area %d", index + 1);
    }

    size_t frac_len = append_signed_fraction(buf2, entry->numerator, entry->denominator);

    if (entry->contrast_grade == CONTRAST_GRADE_MAX) {
        sprintf(buf2 + frac_len, " [Base Grade]\n");
    } else {
        sprintf(buf2 + frac_len, " [Grade %s]\n", contrast_grade_str(entry->contrast_grade));
    }

    uint8_t option = display_message("Delete Adjustment?\n", buf1, buf2, " Yes \n No \n Delete All ");

    if (option == 1) {
        exposure_burn_dodge_delete(exposure_state, index);
        return true;
    } else if (option == 2) {
        // Return assuming no timeout
        return true;
    } else if (option == 3) {
        exposure_burn_dodge_delete_all(exposure_state);
        return true;
    } else if (option == UINT8_MAX) {
        // Return assuming timeout
        return false;
    } else {
        // Return assuming no timeout
        return true;
    }
}

void state_list_adjustments_exit(state_t *state_base, state_controller_t *controller, state_identifier_t next_state)
{

}
