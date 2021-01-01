/**
 * Represents and manipulates the state of currently selected
 * exposure settings.
 */
#ifndef EXPOSURE_STATE_H
#define EXPOSURE_STATE_H

#include <stdint.h>

#define EXPOSURE_BURN_DODGE_MAX 9

typedef enum {
    CONTRAST_GRADE_00 = 0,
    CONTRAST_GRADE_0,
    CONTRAST_GRADE_0_HALF,
    CONTRAST_GRADE_1,
    CONTRAST_GRADE_1_HALF,
    CONTRAST_GRADE_2,
    CONTRAST_GRADE_2_HALF,
    CONTRAST_GRADE_3,
    CONTRAST_GRADE_3_HALF,
    CONTRAST_GRADE_4,
    CONTRAST_GRADE_4_HALF,
    CONTRAST_GRADE_5,
    CONTRAST_GRADE_MAX
} exposure_contrast_grade_t;

typedef enum {
    EXPOSURE_ADJ_TWELFTH = 1,
    EXPOSURE_ADJ_SIXTH = 2,
    EXPOSURE_ADJ_QUARTER = 3,
    EXPOSURE_ADJ_THIRD = 4,
    EXPOSURE_ADJ_HALF = 6,
    EXPOSURE_ADJ_WHOLE = 12
} exposure_adjustment_increment_t;

typedef struct __exposure_adjustment_t {
    exposure_contrast_grade_t contrast_grade;
    int8_t numerator;
    uint8_t denominator;
} exposure_burn_dodge_t;

typedef struct __exposure_state_t {
    exposure_contrast_grade_t contrast_grade;
    float base_time;
    float adjusted_time;
    int adjustment_value;
    int adjustment_increment;
    exposure_burn_dodge_t burn_dodge_entry[EXPOSURE_BURN_DODGE_MAX];
    int burn_dodge_count;
} exposure_state_t;

void exposure_state_defaults(exposure_state_t *state);

void exposure_set_base_time(exposure_state_t *state, float value);

void exposure_adj_increase(exposure_state_t *state);
void exposure_adj_decrease(exposure_state_t *state);
void exposure_adj_set(exposure_state_t *state, int value);
int exposure_adj_min(exposure_state_t *state);
int exposure_adj_max(exposure_state_t *state);

void exposure_contrast_increase(exposure_state_t *state);
void exposure_contrast_decrease(exposure_state_t *state);

void exposure_adj_increment_increase(exposure_state_t *state);
void exposure_adj_increment_decrease(exposure_state_t *state);
uint8_t exposure_adj_increment_get_denominator(const exposure_state_t *state);

void exposure_burn_dodge_delete(exposure_state_t *state, int index);
void exposure_burn_dodge_delete_all(exposure_state_t *state);

float exposure_get_test_strip_time_incremental(const exposure_state_t *state,
    int patch_min, unsigned int patches_covered);
float exposure_get_test_strip_time_complete(const exposure_state_t *state, int patch);

const char *contrast_grade_str(exposure_contrast_grade_t contrast_grade);

#endif /* EXPOSURE_STATE_H */
