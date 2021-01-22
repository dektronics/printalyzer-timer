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

    return true;
}

bool paper_profile_grade_is_empty(const paper_profile_grade_t *profile_grade)
{
    if (!profile_grade) {
        return true;
    }

    if (profile_grade->ht_lev100 == 0
        && profile_grade->hm_lev100 == 0
        && profile_grade->hs_lev100 == 0) {
        return true;
    }

    return false;
}

bool paper_profile_grade_is_valid(const paper_profile_grade_t *profile_grade)
{
    if (!profile_grade) {
        return false;
    }

    /* All values must be within a sensible maximum */
    if (profile_grade->ht_lev100 > 999
        || profile_grade->hm_lev100 > 999
        || profile_grade->hs_lev100 > 999) {
        return false;
    }

    /* Ht and Hs values must be non-zero */
    if (profile_grade->ht_lev100 == 0 || profile_grade->hs_lev100 == 0) {
        return false;
    }

    /* Ht must be less than Hs */
    if (profile_grade->ht_lev100 >= profile_grade->hs_lev100) {
        return false;
    }

    /* If Hm is defined, it must be between Ht and Hs */
    if (profile_grade->hm_lev100 > 0 &&
        (profile_grade->hm_lev100 <= profile_grade->ht_lev100
        || profile_grade->hm_lev100 >= profile_grade->hs_lev100)) {
        return false;
    }

    //TODO Consider enforcing a minimum separation between Ht and Hs

    return true;
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
    memset(mid_grade, 0, sizeof(paper_profile_grade_t));

    /* Do not proceed if the adjacent grades are not valid */
    if (!paper_profile_grade_is_valid(grade_a) || !paper_profile_grade_is_valid(grade_b)) {
        memset(mid_grade, 0, sizeof(paper_profile_grade_t));
        return;
    }

    /* Set Ht to the midpoint between grades A and B */
    mid_grade->ht_lev100 = lroundf(((float)grade_a->ht_lev100 + (float)grade_b->ht_lev100) / 2.0F);

    /* Set Hs to the midpoint between grades A and B */
    mid_grade->hs_lev100 = lroundf(((float)grade_a->hs_lev100 + (float)grade_b->hs_lev100) / 2.0F);

    /* Set Hm to the midpoint between grades A and B if Hm is available for both grades */
    if (grade_a->hm_lev100 > 0 && grade_b->hm_lev100 > 0) {
        mid_grade->hm_lev100 = lroundf(((float)grade_a->hm_lev100 + (float)grade_b->hm_lev100) / 2.0F);
    }

    //TODO Calculate Hm if it is only available for A or B, but not both

    /* Final check to make sure some rounding error didn't produce an invalid profile */
    if (!paper_profile_grade_is_valid(mid_grade)) {
        memset(mid_grade, 0, sizeof(paper_profile_grade_t));
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
    memset(mid_grade, 0, sizeof(paper_profile_grade_t));

    /* Do not proceed if the adjacent grades are not valid */
    if (!paper_profile_grade_is_valid(grade_a) || !paper_profile_grade_is_valid(grade_b)) {
        memset(mid_grade, 0, sizeof(paper_profile_grade_t));
        return;
    }

    /* Set Ht to the same value as grade A */
    mid_grade->ht_lev100 = lroundf(((float)grade_a->ht_lev100 + (float)grade_b->ht_lev100) / 2.0F);

    /* Set Hs to the midpoint contrast between grades A and B, offset by Ht of grade A. */
    float contrast_a = grade_a->hs_lev100 - grade_a->ht_lev100;
    float contrast_b = grade_b->hs_lev100 - grade_b->ht_lev100;
    mid_grade->hs_lev100 = mid_grade->ht_lev100 + lroundf((contrast_a + contrast_b) / 2.0F);

    /* Set Hm to the same value as grade A, if available */
    if (grade_a->hm_lev100 > 0) {
        mid_grade->hm_lev100 = grade_a->hm_lev100;
    }

    /* Final check to make sure some rounding error didn't produce an invalid profile */
    if (!paper_profile_grade_is_valid(mid_grade)) {
        memset(mid_grade, 0, sizeof(paper_profile_grade_t));
    }
}
