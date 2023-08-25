#include "exposure_state.h"

#include <FreeRTOS.h>
#include <string.h>
#include <math.h>

#define LOG_TAG "exposure_state"
#include <elog.h>

#include "settings.h"
#include "util.h"
#include "paper_profile.h"

/**
 * Maximum number of light readings used to calculate exposure
 * and to create the tone graph.
 */
#define MAX_LUX_READINGS 8

/**
 * Number of tick marks along the tone graph.
 * The number of visible elements in the graph should be one less
 * than this number, plus the limit arrows on either end.
 */
#define TONE_GRAPH_MARKS_SIZE 16

/**
 * Approximate PEV value to use when recommending a base
 * exposure time in calibration mode. This should help get
 * a starting point that is close enough for test strips
 * to easily narrow down the correct values for a paper.
 */
#define CALIBRATION_BASE_PEV 30

/**
 * Approximate PEV value to use when recommending a step wedge
 * exposure time in calibration mode. This should help get a
 * starting point that is close enough for step wedge exposures
 * when attempting to determine a paper's characteristic curve.
 */
#define CALIBRATION_STRIP_PEV 220

/**
 * Minimum exposure time that may be offered by calculation
 * from light readings.
 */
#define EXPOSURE_TIME_LOWER_BOUND (1.0F)

typedef struct __exposure_state_t {
    exposure_mode_t mode;
    exposure_mode_t last_printing_mode;
    contrast_grade_t contrast_grade;
    uint16_t channel_default_values[3];
    uint16_t channel_values[3];
    bool channel_wide_mode;
    float base_time;
    float adjusted_time;
    float min_exposure_time;
    int adjustment_value;
    int adjustment_increment;
    float lux_readings[MAX_LUX_READINGS];
    int lux_reading_count;
    uint32_t calibration_pev;
    exposure_pev_preset_t calibration_pev_preset;
    int paper_profile_index;
    paper_profile_t paper_profile;
    float tone_graph_marks[TONE_GRAPH_MARKS_SIZE];
    uint32_t tone_graph;
    exposure_burn_dodge_t burn_dodge_entry[EXPOSURE_BURN_DODGE_MAX];
    int burn_dodge_count;
} exposure_state_t;

static float exposure_base_time_for_calibration_pev(float lux, uint32_t pev);
static void exposure_recalculate(exposure_state_t *state);
static void exposure_recalculate_tone_graph_marks(exposure_state_t *state);
static void exposure_recalculate_tone_graph_marks_impl(const exposure_state_t *state,
    contrast_grade_t contrast_grade, float *tone_graph_marks);
static void exposure_recalculate_base_time(exposure_state_t *state);
static void exposure_populate_tone_graph(exposure_state_t *state);
static uint32_t exposure_calculate_tone_graph(const exposure_state_t *state, float adjusted_time);
static uint32_t exposure_calculate_tone_graph_impl(const exposure_state_t *state,
    const float *tone_graph_marks, float adjusted_time);
static uint32_t exposure_calculate_tone_graph_element_impl(float lux_reading,
    const float *tone_graph_marks, float adjusted_time);
static uint32_t exposure_pev_for_preset(exposure_pev_preset_t preset);

exposure_state_t *exposure_state_create()
{
    exposure_state_t *state = pvPortMalloc(sizeof(exposure_state_t));
    if (!state) {
        log_e("Unable to allocate exposure state");
        return NULL;
    }
    memset(state, 0, sizeof(exposure_state_t));
    state->last_printing_mode = EXPOSURE_MODE_PRINTING_BW;
    exposure_state_defaults(state);

    // Initialize fields that aren't reset every time the user
    // clears state.
    state->paper_profile_index = settings_get_default_paper_profile_index();
    if (!settings_get_paper_profile(&state->paper_profile, state->paper_profile_index)) {
        exposure_clear_active_paper_profile(state);
        log_i("Set default paper profile");
    } else {
        log_i("Loaded paper profile: [%d] => \"%s\"", state->paper_profile_index + 1, state->paper_profile.name);
    }
    exposure_recalculate_tone_graph_marks(state);

    return state;
}

void exposure_state_free(exposure_state_t *state)
{
    vPortFree(state);
}

void exposure_state_defaults(exposure_state_t *state)
{
    if (!state) { return; }

    if (state->last_printing_mode == EXPOSURE_MODE_PRINTING_BW || state->last_printing_mode == EXPOSURE_MODE_PRINTING_COLOR) {
        state->mode = state->last_printing_mode;
    } else {
        state->mode = EXPOSURE_MODE_PRINTING_BW;
    }

    state->contrast_grade = settings_get_default_contrast_grade();
    state->base_time = settings_get_default_exposure_time() / 1000.0f;
    state->adjusted_time = state->base_time;
    state->adjustment_value = 0;
    state->adjustment_increment = settings_get_default_step_size();

    for (size_t i = 0; i < EXPOSURE_BURN_DODGE_MAX; i++) {
        memset(&state->burn_dodge_entry[i], 0, sizeof(exposure_burn_dodge_t));
    }
    state->burn_dodge_count = 0;

    for (size_t i = 0; i < MAX_LUX_READINGS; i++) {
        state->lux_readings[i] = NAN;
    }
    state->lux_reading_count = 0;
    state->calibration_pev = 0;
    state->calibration_pev_preset = EXPOSURE_PEV_PRESET_BASE;

    for (size_t i = 0; i < 3; i++) {
        state->channel_values[i] = state->channel_default_values[i];
    }
}

exposure_mode_t exposure_get_mode(const exposure_state_t *state)
{
    if (!state) { return EXPOSURE_MODE_PRINTING_BW; }
    return state->mode;
}

void exposure_set_mode(exposure_state_t *state, exposure_mode_t mode)
{
    if (!state) { return; }
    if (mode < EXPOSURE_MODE_PRINTING_BW || mode > EXPOSURE_MODE_CALIBRATION) { return; }

    if (state->mode != mode) {
        if (mode == EXPOSURE_MODE_PRINTING_BW || mode == EXPOSURE_MODE_PRINTING_COLOR) {
            state->last_printing_mode = mode;
        }

        state->mode = mode;

        if (state->mode == EXPOSURE_MODE_PRINTING_BW || state->mode == EXPOSURE_MODE_PRINTING_COLOR) {
            // Reset the base exposure and light readings if entering printing or densitometer mode
            state->base_time = settings_get_default_exposure_time() / 1000.0f;
            state->adjusted_time = state->base_time;
            state->adjustment_value = 0;

            for (int i = 0; i < MAX_LUX_READINGS; i++) {
                state->lux_readings[i] = NAN;
            }
            state->lux_reading_count = 0;
        } else if (state->mode == EXPOSURE_MODE_DENSITOMETER) {
            // Reset the base exposure
            state->base_time = settings_get_default_exposure_time() / 1000.0f;
            state->adjusted_time = state->base_time;
            state->adjustment_value = 0;

            // Select just the lowest light reading
            float lowest_lux = NAN;
            for (int i = 0; i < state->lux_reading_count; i++) {
                if (isnanf(lowest_lux) || state->lux_readings[i] < lowest_lux) {
                    lowest_lux = state->lux_readings[i];
                }
            }

            for (int i = 0; i < MAX_LUX_READINGS; i++) {
                state->lux_readings[i] = NAN;
            }

            if (!isnanf(lowest_lux)) {
                state->lux_readings[0] = lowest_lux;
                state->lux_reading_count = 1;
            } else {
                state->lux_reading_count = 0;
            }

            if (state->burn_dodge_count > 0) {
                exposure_burn_dodge_delete_all(state);
            }
        } else if (state->mode == EXPOSURE_MODE_CALIBRATION) {
            // Clear any burn/dodge adjustments if entering calibration mode
            if (state->burn_dodge_count > 0) {
                exposure_burn_dodge_delete_all(state);
            }

            state->calibration_pev_preset = EXPOSURE_PEV_PRESET_BASE;
        }
    }
}

void exposure_set_base_time(exposure_state_t *state, float value)
{
    if (!state) { return; }

    if (value < 0.01f) {
        value = 0.01f;
    } else if (value > 999.0f) {
        value = 999.0f;
    }

    state->base_time = value;
    state->adjustment_value = 0;
    exposure_recalculate(state);
}

void exposure_set_min_exposure_time(exposure_state_t *state, float value)
{
    if (!state) { return; }
    if (isnormal(value) && value > 0.01F) {
        state->min_exposure_time = value;

        if (state->lux_reading_count > 0 && state->base_time < MAX(state->min_exposure_time, EXPOSURE_TIME_LOWER_BOUND)) {
            exposure_recalculate_base_time(state);
        }
    }
}

float exposure_get_min_exposure_time(const exposure_state_t *state)
{
    if (!state) { return 0; }

    if (isnormal(state->min_exposure_time) && state->min_exposure_time > 0.01F) {
        return state->min_exposure_time;
    } else {
        return 0;
    }
}

float exposure_get_exposure_time(const exposure_state_t *state)
{
    if (!state) { return 0; }
    return state->adjusted_time;
}

float exposure_get_relative_density(const exposure_state_t *state)
{
    if (!state) { return NAN; }

    if (state->lux_reading_count == 0) {
        return NAN;
    } else if (state->lux_reading_count == 1) {
        return 0.0F;
    } else if (state->lux_reading_count == 2) {
        if (isnormal(state->lux_readings[0]) && state->lux_readings[0] >= 0.01F
            && isnormal(state->lux_readings[1]) && state->lux_readings[1] > state->lux_readings[0]) {
            return log10f(state->lux_readings[1] / state->lux_readings[0]);
        } else {
            return 0.0F;
        }
    } else {
        return NAN;
    }
}

int exposure_get_active_paper_profile_index(const exposure_state_t *state)
{
    if (!state) { return -1; }
    return state->paper_profile_index;
}

bool exposure_set_active_paper_profile_index(exposure_state_t *state, int index)
{
    if (!state) { return false; }
    if (index < 16) {
        if (settings_get_paper_profile(&state->paper_profile, index)) {
            log_i("Loaded paper profile: [%d] => \"%s\"", index + 1, state->paper_profile.name);
            state->paper_profile_index = index;
            exposure_recalculate_tone_graph_marks(state);
            exposure_recalculate_base_time(state);
            exposure_recalculate(state);
            return true;
        }
    }
    return false;
}

void exposure_clear_active_paper_profile(exposure_state_t *state)
{
    if (!state) { return; }
    state->paper_profile_index = -1;
    paper_profile_set_defaults(&state->paper_profile);
}

float exposure_base_time_for_calibration_pev(float lux, uint32_t pev)
{
    float base_time = 0;
    if (isnormal(lux) && lux > 0) {
        base_time = powf(10, pev / 100.0F) / lux;
    }
    if (base_time < 0.10F) {
        base_time = 0;
    }
    return base_time;
}

uint32_t exposure_add_meter_reading(exposure_state_t *state, float lux)
{
    bool place_added_reading = false;
    if (!state) { return 0; }

    if (state->mode == EXPOSURE_MODE_PRINTING_BW || state->mode == EXPOSURE_MODE_PRINTING_COLOR) {
        //TODO consider filtering out "new" readings that are very close to old readings, but unsure of tolerance
        if (state->lux_reading_count < MAX_LUX_READINGS) {
            state->lux_readings[state->lux_reading_count++] = lux;
            place_added_reading = true;
        }
        exposure_recalculate_base_time(state);
    } else if (state->mode == EXPOSURE_MODE_DENSITOMETER) {
        //TODO consider using a tolerance when deciding to add/replace readings
        if (state->lux_reading_count == 0) {
            state->lux_readings[0] = lux;
            state->lux_reading_count = 1;
        } else if (state->lux_reading_count == 1) {
            if (lux < state->lux_readings[0]) {
                state->lux_readings[0] = lux;
            } else {
                state->lux_readings[1] = lux;
                state->lux_reading_count = 2;
            }
        } else {
            if (lux < state->lux_readings[0]) {
                state->lux_readings[0] = lux;
                state->lux_readings[1] = NAN;
                state->lux_reading_count = 1;
            } else {
                state->lux_readings[1] = lux;
                state->lux_reading_count = 2;
            }
        }
    } else if (state->mode == EXPOSURE_MODE_CALIBRATION) {
        float updated_base_time = exposure_base_time_for_calibration_pev(lux, exposure_pev_for_preset(state->calibration_pev_preset));
        if (isnormal(updated_base_time) && updated_base_time > 0) {
            state->base_time = updated_base_time;
        } else {
            state->base_time = settings_get_default_exposure_time() / 1000.0F;
        }
        state->adjustment_value = 0;

        if (state->lux_reading_count > 1) {
            for (int i = 1; i < MAX_LUX_READINGS; i++) {
                state->lux_readings[i] = NAN;
            }
        }
        state->lux_readings[0] = lux;
        state->lux_reading_count = 1;
    }

    exposure_recalculate(state);

    if (place_added_reading) {
        return exposure_calculate_tone_graph_element_impl(lux, state->tone_graph_marks, state->adjusted_time);
    } else {
        return 0;
    }
}

void exposure_clear_meter_readings(exposure_state_t *state)
{
    if (!state) { return; }
    for (int i = 0; i < MAX_LUX_READINGS; i++) {
        state->lux_readings[i] = NAN;
    }
    state->lux_reading_count = 0;
    exposure_recalculate(state);
}

uint32_t exposure_get_tone_graph(const exposure_state_t *state)
{
    if (!state) { return 0; }
    return state->tone_graph;
}

uint32_t exposure_get_adjusted_tone_graph(const exposure_state_t *state, int adjustment)
{
    if (!state) { return 0; }
    float stops = adjustment / 12.0f;
    float adjusted_time = state->base_time * powf(2.0f, stops);
    return exposure_calculate_tone_graph(state, adjusted_time);
}

uint32_t exposure_get_absolute_tone_graph(const exposure_state_t *state, float exposure_time)
{
    if (!state) { return 0; }
    return exposure_calculate_tone_graph(state, exposure_time);
}

uint32_t exposure_get_burn_dodge_tone_graph(const exposure_state_t *state, const exposure_burn_dodge_t *burn_dodge)
{
    if (!state || !burn_dodge) { return 0; }

    if (burn_dodge->contrast_grade != CONTRAST_GRADE_MAX && burn_dodge->contrast_grade != state->contrast_grade) {
        float tone_graph_marks[TONE_GRAPH_MARKS_SIZE];
        exposure_recalculate_tone_graph_marks_impl(state, burn_dodge->contrast_grade, tone_graph_marks);
        float stops = (float)burn_dodge->numerator / (float)burn_dodge->denominator;
        float adjusted_time = state->base_time * powf(2.0f, stops);
        return exposure_calculate_tone_graph_impl(state, tone_graph_marks, adjusted_time);
    } else {
        float stops = (float)burn_dodge->numerator / (float)burn_dodge->denominator;
        float adjusted_time = state->base_time * powf(2.0f, stops);
        return exposure_calculate_tone_graph(state, adjusted_time);
    }
}

uint32_t exposure_get_calibration_pev(const exposure_state_t *state)
{
    if (!state) { return 0; }
    return state->calibration_pev;
}

void exposure_adj_increase(exposure_state_t *state)
{
    if (!state) { return; }

    /* Clamp adjustment at +/- 12 stops */
    if (state->adjustment_value >= 144) { return; }

    /* Prevent adjusted times beyond 999 seconds */
    if (state->adjusted_time >= 999.0f) { return; }

    state->adjustment_value += state->adjustment_increment;
    exposure_recalculate(state);
}

void exposure_adj_decrease(exposure_state_t *state)
{
    if (!state) { return; }

    /* Clamp adjustment at +/- 12 stops */
    if (state->adjustment_value <= -144) { return; }

    /* Prevent adjusted times below 0.01 seconds */
    if (state->adjusted_time <= 0.01f) { return; }

    state->adjustment_value -= state->adjustment_increment;
    exposure_recalculate(state);
}

int exposure_adj_get(const exposure_state_t *state)
{
    if (!state) { return 0; }
    return state->adjustment_value;
}

void exposure_adj_set(exposure_state_t *state, int value)
{
    if (!state) { return; }

    /* Clamp adjustment at +/- 12 stops */
    if (value < -144 || value > 144) { return; }

    state->adjustment_value = value;
    exposure_recalculate(state);
}

int exposure_adj_min(exposure_state_t *state)
{
    // Adjustment to current base time that gets close to 0.01 seconds

    if (!state || state->base_time <= 0) { return 0; }

    float min_stops = logf(0.01f / state->base_time) / logf(2.0f);
    int min_adj = ceilf(min_stops * 12.0f);
    if (min_adj < -144) {
        min_adj = -144;
    }

    return min_adj;
}

int exposure_adj_max(exposure_state_t *state)
{
    // Adjustment to current base time that gets close to 999 seconds

    if (!state || state->base_time <= 0) { return 0; }

    float max_stops = logf(999.0f / state->base_time) / logf(2.0f);
    int max_adj = floorf(max_stops * 12.0f);
    if (max_adj > 144) {
        max_adj = 144;
    }

    return max_adj;
}

contrast_grade_t exposure_get_contrast_grade(const exposure_state_t *state)
{
    if (!state) { return CONTRAST_GRADE_MAX; }
    return state->contrast_grade;
}

void exposure_contrast_increase(exposure_state_t *state)
{
    if (!state) { return; }

    if (state->contrast_grade < CONTRAST_GRADE_5) {
        state->contrast_grade++;
        exposure_recalculate_tone_graph_marks(state);
        exposure_recalculate_base_time(state);
        exposure_recalculate(state);
    }
}

void exposure_contrast_decrease(exposure_state_t *state)
{
    if (!state) { return; }

    if (state->contrast_grade > CONTRAST_GRADE_00) {
        state->contrast_grade--;
        exposure_recalculate_tone_graph_marks(state);
        exposure_recalculate_base_time(state);
        exposure_recalculate(state);
    }
}

uint16_t exposure_get_channel_value(const exposure_state_t *state, int index)
{
    if (!state || index > 2) { return 0; }
    return state->channel_values[index];
}

void exposure_set_channel_default_value(exposure_state_t *state, int index, uint16_t value)
{
    if (!state || index > 2) { return; }

    const uint16_t max_val = state->channel_wide_mode ? UINT16_MAX : UINT8_MAX;

    if (value > max_val) {
        state->channel_default_values[index] = max_val;
        state->channel_values[index] = max_val;
    } else {
        state->channel_default_values[index] = value;
        state->channel_values[index] = value;
    }
}

void exposure_channel_increase(exposure_state_t *state, int index, uint8_t amount)
{
    if (!state || amount == 0) { return; }

    const uint32_t max_val = state->channel_wide_mode ? UINT16_MAX : UINT8_MAX;

    if (index < 3) {
        state->channel_values[index] = (uint16_t)((state->channel_values[index] + amount) % (max_val + 1));
    } else {
        if (state->channel_values[0] == state->channel_values[1] && state->channel_values[0] == state->channel_values[2]) {
            state->channel_values[0] = value_adjust_with_rollover_u16(state->channel_values[0], amount, 0, max_val);
            state->channel_values[1] = state->channel_values[0];
            state->channel_values[2] = state->channel_values[0];
        } else {
            size_t biggest = 0;
            if (state->channel_values[1] > state->channel_values[0]) {
                biggest = 1;
            }
            if (state->channel_values[2] > state->channel_values[1]) {
                biggest = 2;
            }
            amount = MIN(amount, max_val - state->channel_values[biggest]);

            state->channel_values[0] += amount;
            state->channel_values[1] += amount;
            state->channel_values[2] += amount;
        }
    }
}

void exposure_channel_decrease(exposure_state_t *state, int index, uint8_t amount)
{
    if (!state || amount == 0) { return; }

    const uint16_t max_val = state->channel_wide_mode ? UINT16_MAX : UINT8_MAX;

    if (index < 3) {
        state->channel_values[index] = (uint16_t)((state->channel_values[index] + ((max_val + 1) - amount)) % (max_val + 1));
    } else {
        if (state->channel_values[0] == state->channel_values[1] && state->channel_values[0] == state->channel_values[2]) {
            state->channel_values[0] = value_adjust_with_rollover_u16(state->channel_values[0], -1 * (int16_t)amount, 0, max_val);
            state->channel_values[1] = state->channel_values[0];
            state->channel_values[2] = state->channel_values[0];
        } else {
            size_t smallest = 0;
            if (state->channel_values[1] < state->channel_values[0]) {
                smallest = 1;
            }
            if (state->channel_values[2] < state->channel_values[1]) {
                smallest = 2;
            }
            amount = MIN(amount, state->channel_values[smallest]);

            state->channel_values[0] -= amount;
            state->channel_values[1] -= amount;
            state->channel_values[2] -= amount;
        }
    }
}

bool exposure_get_channel_wide_mode(const exposure_state_t *state)
{
    if (!state) { return false; }
    return state->channel_wide_mode;
}

void exposure_set_channel_wide_mode(exposure_state_t *state, bool wide_mode)
{
    if (!state) { return; }

    if (!state->channel_wide_mode && wide_mode) {
        /* 8-bit -> 16-bit */
        for (size_t i = 0; i < 2; i++) {
            if (state->channel_values[i] == 0xFF) {
                state->channel_values[i] = 0xFFFF;
            } else {
                state->channel_values[i] = (state->channel_values[i] & 0xFF) << 8;
            }
        }
    } else if (state->channel_wide_mode && !wide_mode) {
        /* 16-bit -> 8-bit */
        for (size_t i = 0; i < 2; i++) {
            state->channel_values[i] = (state->channel_values[i] & 0xFF00) >> 8;
        }
    }

}

exposure_pev_preset_t exposure_calibration_pev_get_preset(const exposure_state_t *state)
{
    if (!state) { return EXPOSURE_PEV_PRESET_BASE; }
    return state->calibration_pev_preset;
}

void exposure_calibration_pev_set_preset(exposure_state_t *state, exposure_pev_preset_t preset)
{
    if (!state) { return; }

    if (state->mode == EXPOSURE_MODE_CALIBRATION
        && state->lux_reading_count > 0
        && isnormal(state->lux_readings[0]) && state->lux_readings[0] > 0
        && preset != state->calibration_pev_preset
        && preset >= EXPOSURE_PEV_PRESET_BASE && preset <= EXPOSURE_PEV_PRESET_STRIP) {
        state->calibration_pev_preset = preset;
        float updated_base_time = exposure_base_time_for_calibration_pev(state->lux_readings[0], exposure_pev_for_preset(state->calibration_pev_preset));
        if (isnormal(updated_base_time) && updated_base_time > 0) {
            state->base_time = updated_base_time;
        } else {
            state->base_time = settings_get_default_exposure_time() / 1000.0F;
        }
        state->adjustment_value = 0;
        exposure_recalculate(state);
    }
}

void exposure_calibration_pev_increase(exposure_state_t *state)
{
    if (!state) { return; }

    if (state->mode == EXPOSURE_MODE_CALIBRATION
        && state->lux_reading_count > 0
        && isnormal(state->lux_readings[0]) && state->lux_readings[0] > 0) {
        if (state->calibration_pev < 999) {
            float updated_base_time = exposure_base_time_for_calibration_pev(state->lux_readings[0], state->calibration_pev + 1);
            if (isnormal(updated_base_time) && updated_base_time <= 999) {
                state->base_time = updated_base_time;
                state->adjustment_value = 0;
                exposure_recalculate(state);
            }
        }
    }
}

void exposure_calibration_pev_decrease(exposure_state_t *state)
{
    if (!state) { return; }

    if (state->mode == EXPOSURE_MODE_CALIBRATION
        && state->lux_reading_count > 0
        && isnormal(state->lux_readings[0]) && state->lux_readings[0] > 0) {
        if (state->calibration_pev > 0) {
            float updated_base_time = exposure_base_time_for_calibration_pev(state->lux_readings[0], state->calibration_pev - 1);
            if (isnormal(updated_base_time) && updated_base_time > 0) {
                state->base_time = updated_base_time;
                state->adjustment_value = 0;
                exposure_recalculate(state);
            }
        }
    }
}

int exposure_adj_increment_get(const exposure_state_t *state)
{
    if (!state) { return 0; }
    return state->adjustment_increment;
}

void exposure_adj_increment_increase(exposure_state_t *state)
{
    if (!state) { return; }

    switch (state->adjustment_increment) {
    case EXPOSURE_ADJ_SIXTH:
        state->adjustment_increment = EXPOSURE_ADJ_TWELFTH;
        break;
    case EXPOSURE_ADJ_QUARTER:
        state->adjustment_increment = EXPOSURE_ADJ_SIXTH;
        break;
    case EXPOSURE_ADJ_THIRD:
        state->adjustment_increment = EXPOSURE_ADJ_QUARTER;
        break;
    case EXPOSURE_ADJ_HALF:
        state->adjustment_increment = EXPOSURE_ADJ_THIRD;
        break;
    case EXPOSURE_ADJ_WHOLE:
        state->adjustment_increment = EXPOSURE_ADJ_HALF;
        break;
    default:
        break;
    }
}

void exposure_adj_increment_decrease(exposure_state_t *state)
{
    if (!state) { return; }

    switch (state->adjustment_increment) {
    case EXPOSURE_ADJ_TWELFTH:
        state->adjustment_increment = EXPOSURE_ADJ_SIXTH;
        break;
    case EXPOSURE_ADJ_SIXTH:
        state->adjustment_increment = EXPOSURE_ADJ_QUARTER;
        break;
    case EXPOSURE_ADJ_QUARTER:
        state->adjustment_increment = EXPOSURE_ADJ_THIRD;
        break;
    case EXPOSURE_ADJ_THIRD:
        state->adjustment_increment = EXPOSURE_ADJ_HALF;
        break;
    case EXPOSURE_ADJ_HALF:
        state->adjustment_increment = EXPOSURE_ADJ_WHOLE;
        break;
    default:
        break;
    }
}

uint8_t exposure_adj_increment_get_denominator(const exposure_state_t *state)
{
    if (!state) { return 0; }

    switch (state->adjustment_increment) {
    case EXPOSURE_ADJ_TWELFTH:
        return 12;
    case EXPOSURE_ADJ_SIXTH:
        return 6;
    case EXPOSURE_ADJ_QUARTER:
        return 4;
    case EXPOSURE_ADJ_THIRD:
        return 3;
    case EXPOSURE_ADJ_HALF:
        return 2;
    case EXPOSURE_ADJ_WHOLE:
        return 1;
    default:
        return 0;
    }
}

int exposure_burn_dodge_count(const exposure_state_t *state)
{
    if (!state) { return 0; }
    return state->burn_dodge_count;
}

const exposure_burn_dodge_t *exposure_burn_dodge_get(const exposure_state_t *state, int index)
{
    if (!state) { return NULL; }

    if (index < 0 || index >= state->burn_dodge_count) {
        return NULL;
    }

    return &state->burn_dodge_entry[index];
}

void exposure_burn_dodge_set(exposure_state_t *state, const exposure_burn_dodge_t *entry, int index)
{
    if (!state || !entry) { return; }

    if (index < 0 || index > MIN(state->burn_dodge_count, EXPOSURE_BURN_DODGE_MAX - 1)) {
        return;
    }

    memcpy(&state->burn_dodge_entry[index], entry, sizeof(exposure_burn_dodge_t));
    if (state->burn_dodge_count == index) {
        state->burn_dodge_count++;
    }
}

void exposure_burn_dodge_delete(exposure_state_t *state, int index)
{
    if (!state) { return; }

    if (index < 0 || index >= state->burn_dodge_count) {
        return;
    }

    for (int i = index; i < EXPOSURE_BURN_DODGE_MAX; i++) {
        if (i < state->burn_dodge_count) {
            memcpy(&state->burn_dodge_entry[i], &state->burn_dodge_entry[i + 1], sizeof(exposure_burn_dodge_t));
        } else {
            memset(&state->burn_dodge_entry[i], 0, sizeof(exposure_burn_dodge_t));
        }
    }
    state->burn_dodge_count--;
}

void exposure_burn_dodge_delete_all(exposure_state_t *state)
{
    if (!state) { return; }

    for (int i = 0; i < EXPOSURE_BURN_DODGE_MAX; i++) {
        memset(&state->burn_dodge_entry[i], 0, sizeof(exposure_burn_dodge_t));
    }
    state->burn_dodge_count = 0;
}

float exposure_get_test_strip_time_incremental(const exposure_state_t *state,
    int patch_min, unsigned int patches_covered)
{
    if (!state) { return NAN; }

    if (patches_covered == 0) {
        float base_exposure = exposure_get_test_strip_time_complete(state, patch_min);
        return base_exposure;
    } else {
        float prev_exposure = exposure_get_test_strip_time_complete(state, patch_min + (patches_covered - 1));
        float curr_exposure = exposure_get_test_strip_time_complete(state, patch_min + patches_covered);
        return curr_exposure - prev_exposure;
    }
}

float exposure_get_test_strip_time_complete(const exposure_state_t *state, int patch)
{
    if (!state) { return NAN; }

    int patch_adjustment = state->adjustment_increment * patch;
    float patch_stops = patch_adjustment / 12.0f;
    float patch_time = state->adjusted_time * powf(2.0f, patch_stops);
    return patch_time;
}

uint32_t exposure_get_test_strip_patch_pev(const exposure_state_t *state, int patch)
{
    if (!state) { return 0; }

    if (state->mode == EXPOSURE_MODE_CALIBRATION
        && state->lux_reading_count > 0
        && isnormal(state->lux_readings[0]) && state->lux_readings[0] > 0) {
        float patch_time = exposure_get_test_strip_time_complete(state, patch);
        float calculated_pev = log10f(patch_time * state->lux_readings[0]) * 100.0;
        return (calculated_pev > 0.5) ? lroundf(calculated_pev) : 0;
    } else {
        return 0;
    }
}

void exposure_recalculate(exposure_state_t *state)
{
    float stops = state->adjustment_value / 12.0f;
    state->adjusted_time = state->base_time * powf(2.0f, stops);

    if (state->mode == EXPOSURE_MODE_PRINTING_BW || state->mode == EXPOSURE_MODE_PRINTING_COLOR) {
        exposure_populate_tone_graph(state);
    } else if (state->mode == EXPOSURE_MODE_CALIBRATION) {
        if (state->lux_reading_count > 0 && isnormal(state->lux_readings[0]) && state->lux_readings[0] > 0) {
            float calculated_pev = log10f(state->adjusted_time * state->lux_readings[0]) * 100.0;
            state->calibration_pev = (calculated_pev > 0.5) ? lroundf(calculated_pev) : 0;
        } else {
            state->calibration_pev = 0;
        }
    }
}

void exposure_recalculate_tone_graph_marks(exposure_state_t *state)
{
    exposure_recalculate_tone_graph_marks_impl(state, state->contrast_grade, state->tone_graph_marks);
}

void exposure_recalculate_tone_graph_marks_impl(const exposure_state_t *state, contrast_grade_t contrast_grade, float *tone_graph_marks)
{
    /* Make sure there is a valid profile */
    if (!paper_profile_is_valid(&state->paper_profile)) {
        log_w("Cannot recalculate tone graph for invalid profile");
        for (size_t i = 0; i < TONE_GRAPH_MARKS_SIZE; i++) {
            tone_graph_marks[i] = NAN;
        }
        return;
    }

    /* Collect the relevant log exposure values from the paper profile */
    uint32_t ht_lev100 = state->paper_profile.grade[contrast_grade].ht_lev100;
    uint32_t hm_lev100 = state->paper_profile.grade[contrast_grade].hm_lev100;
    uint32_t hs_lev100 = state->paper_profile.grade[contrast_grade].hs_lev100;
    float d_net = state->paper_profile.max_net_density;

    if (ht_lev100 > 0 && hm_lev100 > 0 && hs_lev100 > 0
        && ht_lev100 < hs_lev100
        && ht_lev100 <  hm_lev100 && hm_lev100 < hs_lev100
        && isnormal(d_net) && d_net > 0) {
        /*
         * We have all the relevant fields, with valid relationships, to
         * interpolate the full tone curve.
         */
        log_d("Tone graph using full T+M+S+Dnet interpolation");

        float d_ht = 0.04F;
        float d_hm = 0.60F;
        float d_hs = 0.90F * d_net;
        float d_range = d_hs - d_ht;

        float d_inc = d_range / (TONE_GRAPH_MARKS_SIZE - 1);
        float d_mark = d_ht;

        for (size_t i = 0; i < TONE_GRAPH_MARKS_SIZE; i++) {
            tone_graph_marks[i] = interpolate(d_ht, ht_lev100, d_hm, hm_lev100, d_hs, hs_lev100, d_mark);
            d_mark += d_inc;
        }

    } else if (ht_lev100 > 0 && hs_lev100 > 0 && ht_lev100 < hs_lev100) {
        /*
         * We just have the minimum number of relevant fields, so do a linear
         * graph of the ISO Range.
         */
        log_d("Tone graph using simple T+M ISO(R) line");

        uint32_t iso_r = hs_lev100 - ht_lev100;

        float mark_increment = (float)iso_r / (TONE_GRAPH_MARKS_SIZE - 1);
        float mark_value = (float)ht_lev100;

        for (size_t i = 0; i < TONE_GRAPH_MARKS_SIZE; i++) {
            tone_graph_marks[i] = mark_value;
            mark_value += mark_increment;
        }
    } else {
        log_w("Insufficient profile data to calculate tone graph");
        for (size_t i = 0; i < TONE_GRAPH_MARKS_SIZE; i++) {
            tone_graph_marks[i] = NAN;
        }
    }
}

void exposure_recalculate_base_time(exposure_state_t *state)
{
    /* Make sure there is a usable Ht value in the active profile */
    if (state->paper_profile.grade[state->contrast_grade].ht_lev100 == 0) {
        return;
    }

    /* Make sure we have at least one meter reading */
    if (state->lux_reading_count <= 0) {
        return;
    }

    /* Find the lowest meter reading */
    float lux_value = NAN;
    for (size_t i = 0; i < state->lux_reading_count; i++) {
        if (isnan(lux_value) || state->lux_readings[i] < lux_value) {
            lux_value = state->lux_readings[i];
        }
    }

    /* Find the target exposure time */
    float ht_lev100 = state->paper_profile.grade[state->contrast_grade].ht_lev100;
    float target_time = powf(10, ht_lev100 / 100.0F) / lux_value;

    /* Adjust exposure time if it is too low */
    float min_time = MAX(state->min_exposure_time, EXPOSURE_TIME_LOWER_BOUND);
    if (target_time < min_time) {
        target_time = min_time;
    }

    /* Set the new base time, if changed */
    if (fabs(state->base_time - target_time) >= 0.01F) {
        log_d("Updating base time from meter reading: %.2f -> %.2f", state->base_time, target_time);
        state->base_time = target_time;
    }
}

void exposure_populate_tone_graph(exposure_state_t *state)
{
    state->tone_graph = exposure_calculate_tone_graph(state, state->adjusted_time);
}


uint32_t exposure_calculate_tone_graph(const exposure_state_t *state, float adjusted_time)
{
    return exposure_calculate_tone_graph_impl(state, state->tone_graph_marks, adjusted_time);
}

uint32_t exposure_calculate_tone_graph_impl(const exposure_state_t *state, const float *tone_graph_marks, float adjusted_time)
{
    uint32_t tone_graph = 0;

    for (size_t i = 0; i < state->lux_reading_count; i++) {
        tone_graph |= exposure_calculate_tone_graph_element_impl(state->lux_readings[i], tone_graph_marks, adjusted_time);
    }
    return tone_graph;
}

uint32_t exposure_calculate_tone_graph_element_impl(float lux_reading, const float *tone_graph_marks, float adjusted_time)
{
    uint32_t result = 0;

    /* Abort if the tone graph marks are not set */
    if (isnan(tone_graph_marks[0])) {
        return result;
    }

    /* Calculate the log-exposure value for the next reading */
    float lev_value = log10f(lux_reading * adjusted_time) * 100.0F;

    if (lev_value < tone_graph_marks[0]) {
        /* Check whether to set the lower-bound mark */
        result |= 0x00000001UL;
    } else if (lev_value >= tone_graph_marks[TONE_GRAPH_MARKS_SIZE - 1]) {
        /* Check whether to set the upper-bound mark */
        result |= 0x00010000UL;
    } else {
        /* Check which marks of the main graph to set */
        for (size_t j = 0; j < TONE_GRAPH_MARKS_SIZE + 1; j++) {
            if (lev_value >= tone_graph_marks[j] && lev_value < tone_graph_marks[j + 1]) {
                result |= (1UL << (j + 1));
                break;
            }
        }
    }

    return result;
}

uint32_t exposure_pev_for_preset(exposure_pev_preset_t preset)
{
    switch (preset) {
    case EXPOSURE_PEV_PRESET_BASE:
        return CALIBRATION_BASE_PEV;
    case EXPOSURE_PEV_PRESET_STRIP:
        return CALIBRATION_STRIP_PEV;
    default:
        return 0;
    }
}
