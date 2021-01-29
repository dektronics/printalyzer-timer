#include "exposure_state.h"

#include <FreeRTOS.h>
#include <string.h>
#include <math.h>

#include <esp_log.h>

#include "settings.h"

#define MIN(a,b) (((a)<(b))?(a):(b))

static const char *TAG = "exposure_state";

/**
 * Maximum number of light readings used to calculate exposure
 * and to create the tone graph.
 */
#define MAX_LUX_READINGS 8

/**
 * Approximate PEV value to use when recommending a base
 * exposure time in calibration mode. This should help get
 * a starting point that is close enough for test strips
 * to easily narrow down the correct values for a paper.
 */
#define CALIBRATION_BASE_PEV 30

typedef struct __exposure_state_t {
    exposure_mode_t mode;
    exposure_contrast_grade_t contrast_grade;
    float base_time;
    float adjusted_time;
    int adjustment_value;
    int adjustment_increment;
    float lux_readings[MAX_LUX_READINGS];
    int lux_reading_count;
    uint32_t calibration_pev;
    int paper_profile_index;
    paper_profile_t paper_profile;
    exposure_burn_dodge_t burn_dodge_entry[EXPOSURE_BURN_DODGE_MAX];
    int burn_dodge_count;
} exposure_state_t;

static float exposure_base_time_for_calibration_pev(float lux, uint32_t pev);
static void exposure_recalculate(exposure_state_t *state);

exposure_state_t *exposure_state_create()
{
    exposure_state_t *state = pvPortMalloc(sizeof(exposure_state_t));
    if (!state) {
        ESP_LOGE(TAG, "Unable to allocate exposure state");
        return NULL;
    }
    exposure_state_defaults(state);

    // Initialize fields that aren't reset every time the user
    // clears state.
    state->paper_profile_index = settings_get_default_paper_profile_index();
    if (!settings_get_paper_profile(&state->paper_profile, state->paper_profile_index)) {
        state->paper_profile_index = -1;
    }

    return state;
}

void exposure_state_free(exposure_state_t *state)
{
    vPortFree(state);
}

void exposure_state_defaults(exposure_state_t *state)
{
    if (!state) { return; }
    state->mode = EXPOSURE_MODE_PRINTING;
    state->contrast_grade = settings_get_default_contrast_grade();
    state->base_time = settings_get_default_exposure_time() / 1000.0f;
    state->adjusted_time = state->base_time;
    state->adjustment_value = 0;
    state->adjustment_increment = settings_get_default_step_size();

    for (int i = 0; i < EXPOSURE_BURN_DODGE_MAX; i++) {
        memset(&state->burn_dodge_entry[i], 0, sizeof(exposure_burn_dodge_t));
    }
    state->burn_dodge_count = 0;

    for (int i = 0; i < MAX_LUX_READINGS; i++) {
        state->lux_readings[i] = NAN;
    }
    state->lux_reading_count = 0;
    state->calibration_pev = 0;
}

exposure_mode_t exposure_get_mode(const exposure_state_t *state)
{
    if (!state) { return EXPOSURE_MODE_PRINTING; }
    return state->mode;
}

void exposure_set_mode(exposure_state_t *state, exposure_mode_t mode)
{
    if (!state) { return; }
    if (mode < EXPOSURE_MODE_PRINTING || mode > EXPOSURE_MODE_CALIBRATION) { return; }

    if (state->mode != mode) {
        state->mode = mode;

        if (state->mode == EXPOSURE_MODE_PRINTING) {
            // Reset the base exposure and light readings if entering printing mode
            state->base_time = settings_get_default_exposure_time() / 1000.0f;
            state->adjusted_time = state->base_time;
            state->adjustment_value = 0;

            for (int i = 0; i < MAX_LUX_READINGS; i++) {
                state->lux_readings[i] = NAN;
            }
            state->lux_reading_count = 0;
        } else if (state->mode == EXPOSURE_MODE_CALIBRATION) {
            // Clear any burn/dodge adjustments if entering calibration mode
            if (state->burn_dodge_count > 0) {
                exposure_burn_dodge_delete_all(state);
            }
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

float exposure_get_exposure_time(const exposure_state_t *state)
{
    if (!state) { return 0; }
    return state->adjusted_time;
}

int exposure_get_active_paper_profile_index(const exposure_state_t *state)
{
    if (!state) { return -1; }
    return state->paper_profile_index;
}

void exposure_set_active_paper_profile_index(exposure_state_t *state, int index)
{
    if (!state) { return; }
    if (index < 16) {
        state->paper_profile_index = index;
    }
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

void exposure_add_meter_reading(exposure_state_t *state, float lux)
{
    if (!state) { return; }

    if (state->mode == EXPOSURE_MODE_PRINTING) {
        //TODO
    } else if (state->mode == EXPOSURE_MODE_CALIBRATION) {
        float updated_base_time = exposure_base_time_for_calibration_pev(lux, CALIBRATION_BASE_PEV);
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

exposure_contrast_grade_t exposure_get_contrast_grade(const exposure_state_t *state)
{
    if (!state) { return CONTRAST_GRADE_MAX; }
    return state->contrast_grade;
}

void exposure_contrast_increase(exposure_state_t *state)
{
    if (!state) { return; }

    if (state->contrast_grade < CONTRAST_GRADE_5) {
        state->contrast_grade++;
        exposure_recalculate(state);
    }
}

void exposure_contrast_decrease(exposure_state_t *state)
{
    if (!state) { return; }

    if (state->contrast_grade > CONTRAST_GRADE_00) {
        state->contrast_grade--;
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

    if (state->mode == EXPOSURE_MODE_PRINTING) {
        //TODO Update the tone graph
    } else if (state->mode == EXPOSURE_MODE_CALIBRATION) {
        if (state->lux_reading_count > 0 && isnormal(state->lux_readings[0]) && state->lux_readings[0] > 0) {
            float calculated_pev = log10f(state->adjusted_time * state->lux_readings[0]) * 100.0;
            state->calibration_pev = (calculated_pev > 0.5) ? lroundf(calculated_pev) : 0;
        } else {
            state->calibration_pev = 0;
        }
    }
}

const char *contrast_grade_str(exposure_contrast_grade_t contrast_grade)
{
    switch (contrast_grade) {
    case CONTRAST_GRADE_00:
        return "00";
    case CONTRAST_GRADE_0:
        return "0";
    case CONTRAST_GRADE_0_HALF:
        return "1/2";
    case CONTRAST_GRADE_1:
        return "1";
    case CONTRAST_GRADE_1_HALF:
        return "1-1/2";
    case CONTRAST_GRADE_2:
        return "2";
    case CONTRAST_GRADE_2_HALF:
        return "2-1/2";
    case CONTRAST_GRADE_3:
        return "3";
    case CONTRAST_GRADE_3_HALF:
        return "3-1/2";
    case CONTRAST_GRADE_4:
        return "4";
    case CONTRAST_GRADE_4_HALF:
        return "4-1/2";
    case CONTRAST_GRADE_5:
        return "5";
    default:
        return "";
    }
}
