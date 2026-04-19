#include "paper_profile.h"

#include <stddef.h>
#include <string.h>
#include <math.h>

static void paper_profile_calculate_midpoint_grade(
    paper_profile_grade_t *mid_grade,
    const paper_profile_grade_t *grade_a,
    const paper_profile_grade_t *grade_b);
static void paper_profile_calculate_midpoint_grade_exposure_a(
    paper_profile_grade_t *mid_grade,
    const paper_profile_grade_t *grade_a,
    const paper_profile_grade_t *grade_b);

bool paper_profile_is_valid(const paper_profile_t *profile)
{
    if (!profile) {
        return false;
    }

    for (size_t i = 0; i < CONTRAST_GRADE_MAX; i++) {
        if (!paper_profile_grade_is_empty(&profile->grade[i]) && !paper_profile_grade_is_valid(&profile->grade[i])) {
            return false;
        }
    }

    /* Dmax must be a value greater than zero */
    if (!(isnan(profile->paper_dmax) || (isnormal(profile->paper_dmax) && profile->paper_dmax > 0.0F))) {
        return false;
    }

    /* Dmin must be NAN or less than Dmax */
    if (!isnan(profile->paper_dmin) && profile->paper_dmin >= profile->paper_dmax) {
        return false;
    }

    return true;
}

bool paper_profile_grade_is_empty(const paper_profile_grade_t *profile_grade)
{
    if (!profile_grade) {
        return true;
    }

    if (pev_is_empty(profile_grade->ht_lev100)
        && pev_is_empty(profile_grade->hm_lev100)
        && pev_is_empty(profile_grade->hs_lev100)) {
        return true;
    }

    return false;
}

bool paper_profile_grade_is_valid(const paper_profile_grade_t *profile_grade)
{
    if (!profile_grade) {
        return false;
    }

    /* Ht and Hs values must be within a sensible range */
    if (!pev_in_range(profile_grade->ht_lev100) || !pev_in_range(profile_grade->hs_lev100)) {
        return false;
    }

    /* Hm must be empty or within a sensible range */
    if (!pev_is_empty(profile_grade->hm_lev100) && !pev_in_range(profile_grade->hm_lev100)) {
        return false;
    }

    /* Ht must be less than Hs */
    if (profile_grade->ht_lev100 >= profile_grade->hs_lev100) {
        return false;
    }

    /* If Hm is defined, it must be between Ht and Hs */
    if (!pev_is_empty(profile_grade->hm_lev100) &&
        (profile_grade->hm_lev100 <= profile_grade->ht_lev100
        || profile_grade->hm_lev100 >= profile_grade->hs_lev100)) {
        return false;
    }

    //TODO Consider enforcing a minimum separation between Ht and Hs

    return true;
}

bool paper_profile_compare(const paper_profile_t *profile1, const paper_profile_t *profile2)
{
    /* Both are the same pointer */
    if (profile1 == profile2) {
        return true;
    }

    /* Both are null */
    if (!profile1 && !profile2) {
        return true;
    }

    /* One is null and the other is not */
    if ((profile1 && !profile2) || (!profile1 && profile2)) {
        return false;
    }

    /* Compare the name strings */
    if (strncmp(profile1->name, profile2->name, sizeof(profile1->name)) != 0) {
        return false;
    }

    /* Compare the floating point fields with a tight tolerance */
    if (!is_equal_numbers(profile1->paper_dmin, profile2->paper_dmin, 0.0001F)) {
        return false;
    }
    if (!is_equal_numbers(profile1->paper_dmax, profile2->paper_dmax, 0.0001F)) {
        return false;
    }

    /* Compare each contrast grade entry */
    for (size_t i = 0; i < CONTRAST_GRADE_MAX; i++) {
        if (profile1->grade[i].ht_lev100 != profile2->grade[i].ht_lev100
            || profile1->grade[i].hm_lev100 != profile2->grade[i].hm_lev100
            || profile1->grade[i].hs_lev100 != profile2->grade[i].hs_lev100) {
            return false;
        }
    }

    return true;
}

float paper_profile_max_net_density(const paper_profile_t *profile)
{
    float result;

    if (!profile || !is_valid_number(profile->paper_dmax)) { return NAN; }

    result = profile->paper_dmax;

    if (is_valid_number(profile->paper_dmin)) {
        result += profile->paper_dmin;
    }

    return result;
}

void paper_profile_recalculate(paper_profile_t *profile)
{
    if (!profile) {
        return;
    }

    /*
     * It is unclear as to the most correct way to derive these intermediate
     * grades. The approach used for now will mostly be based on a midpoint
     * between the two adjacent grades. There will be an exception for
     * exposure, however, for grade 3-1/2. This is due to the following remark
     * in the Ilford contrast control datasheet:
     * "The exposure time for filters 00-3 1/2 is the same;
     * that for filters 4-5 is double."
     */

    paper_profile_calculate_midpoint_grade(
        &profile->grade[CONTRAST_GRADE_0_HALF],
        &profile->grade[CONTRAST_GRADE_0],
        &profile->grade[CONTRAST_GRADE_1]);

    paper_profile_calculate_midpoint_grade(
        &profile->grade[CONTRAST_GRADE_1_HALF],
        &profile->grade[CONTRAST_GRADE_1],
        &profile->grade[CONTRAST_GRADE_2]);

    paper_profile_calculate_midpoint_grade(
        &profile->grade[CONTRAST_GRADE_2_HALF],
        &profile->grade[CONTRAST_GRADE_2],
        &profile->grade[CONTRAST_GRADE_3]);

    paper_profile_calculate_midpoint_grade_exposure_a(
        &profile->grade[CONTRAST_GRADE_3_HALF],
        &profile->grade[CONTRAST_GRADE_3],
        &profile->grade[CONTRAST_GRADE_4]);

    paper_profile_calculate_midpoint_grade(
        &profile->grade[CONTRAST_GRADE_4_HALF],
        &profile->grade[CONTRAST_GRADE_4],
        &profile->grade[CONTRAST_GRADE_5]);
}

/**
 * Calculate a mid-point grade profile by averaging the exposure and contrast
 * values for grades A and B.
 */
void paper_profile_calculate_midpoint_grade(
    paper_profile_grade_t *mid_grade,
    const paper_profile_grade_t *grade_a,
    const paper_profile_grade_t *grade_b)
{
    /* Clear the mid-grade profile so we start fresh. */
    paper_profile_grade_clear(mid_grade);

    /* Do not proceed if the adjacent grades are not valid */
    if (!paper_profile_grade_is_valid(grade_a) || !paper_profile_grade_is_valid(grade_b)) {
        return;
    }

    /* Set Ht to the midpoint between grades A and B */
    mid_grade->ht_lev100 = lroundf(((float)grade_a->ht_lev100 + (float)grade_b->ht_lev100) / 2.0F);

    /* Set Hs to the midpoint between grades A and B */
    mid_grade->hs_lev100 = lroundf(((float)grade_a->hs_lev100 + (float)grade_b->hs_lev100) / 2.0F);

    /* Set Hm to the midpoint between grades A and B if Hm is available for both grades */
    if (!pev_is_empty(grade_a->hm_lev100) && !pev_is_empty(grade_b->hm_lev100)) {
        mid_grade->hm_lev100 = lroundf(((float)grade_a->hm_lev100 + (float)grade_b->hm_lev100) / 2.0F);
    }

    //TODO Calculate Hm if it is only available for A or B, but not both

    /* Final check to make sure some rounding error didn't produce an invalid profile */
    if (!paper_profile_grade_is_valid(mid_grade)) {
        paper_profile_grade_clear(mid_grade);
    }
}

/**
 * Special case of calculating a mid-point grade profile that uses the
 * exposure values from just grade A.
 */
static void paper_profile_calculate_midpoint_grade_exposure_a(
    paper_profile_grade_t *mid_grade,
    const paper_profile_grade_t *grade_a,
    const paper_profile_grade_t *grade_b)
{
    /* Clear the mid-grade profile so we start fresh. */
    paper_profile_grade_clear(mid_grade);

    /* Do not proceed if the adjacent grades are not valid */
    if (!paper_profile_grade_is_valid(grade_a) || !paper_profile_grade_is_valid(grade_b)) {
        return;
    }

    /* Set Ht to the same value as grade A */
    mid_grade->ht_lev100 = lroundf(((float)grade_a->ht_lev100 + (float)grade_b->ht_lev100) / 2.0F);

    /* Set Hs to the midpoint contrast between grades A and B, offset by Ht of grade A. */
    float contrast_a = (float)(grade_a->hs_lev100 - grade_a->ht_lev100);
    float contrast_b = (float)(grade_b->hs_lev100 - grade_b->ht_lev100);
    mid_grade->hs_lev100 = mid_grade->ht_lev100 + lroundf((contrast_a + contrast_b) / 2.0F);

    /* Set Hm to the same value as grade A, if available */
    if (!pev_is_empty(grade_a->hm_lev100)) {
        mid_grade->hm_lev100 = grade_a->hm_lev100;
    }

    /* Final check to make sure some rounding error didn't produce an invalid profile */
    if (!paper_profile_grade_is_valid(mid_grade)) {
        paper_profile_grade_clear(mid_grade);
    }
}

void paper_profile_clear(paper_profile_t *profile)
{
    memset(profile, 0, sizeof(paper_profile_t));

    for (size_t i = 0; i < CONTRAST_GRADE_MAX; i++) {
        paper_profile_grade_clear(&profile->grade[i]);
    }

    profile->paper_dmin = NAN;
    profile->paper_dmax = NAN;
}

void paper_profile_grade_clear(paper_profile_grade_t *profile_grade)
{
    memset(profile_grade, 0, sizeof(paper_profile_grade_t));
    profile_grade->ht_lev100 = INT32_MAX;
    profile_grade->hm_lev100 = INT32_MAX;
    profile_grade->hs_lev100 = INT32_MAX;
}

void paper_profile_set_defaults(paper_profile_t *profile)
{
    paper_profile_clear(profile);
    strcpy(profile->name, "Default");

    profile->grade[CONTRAST_GRADE_00].ht_lev100 = 14;
    profile->grade[CONTRAST_GRADE_00].hs_lev100 = 193;

    profile->grade[CONTRAST_GRADE_0].ht_lev100 = 19;
    profile->grade[CONTRAST_GRADE_0].hs_lev100 = 163;

    profile->grade[CONTRAST_GRADE_1].ht_lev100 = 24;
    profile->grade[CONTRAST_GRADE_1].hs_lev100 = 156;

    profile->grade[CONTRAST_GRADE_2].ht_lev100 = 27;
    profile->grade[CONTRAST_GRADE_2].hs_lev100 = 136;

    profile->grade[CONTRAST_GRADE_3].ht_lev100 = 34;
    profile->grade[CONTRAST_GRADE_3].hs_lev100 = 123;

    profile->grade[CONTRAST_GRADE_4].ht_lev100 = 74;
    profile->grade[CONTRAST_GRADE_4].hs_lev100 = 139;

    profile->grade[CONTRAST_GRADE_5].ht_lev100 = 95;
    profile->grade[CONTRAST_GRADE_5].hs_lev100 = 142;

    paper_profile_recalculate(profile);
}

bool pev_is_empty(int32_t pev)
{
    return pev == INT32_MAX || pev == INT32_MIN;
}

bool pev_in_range(int32_t pev)
{
    return pev >= PAPER_PEV_MIN && pev <= PAPER_PEV_MAX;
}
