#ifndef PAPER_PROFILE_H
#define PAPER_PROFILE_H

#include <stdint.h>
#include <stdbool.h>

#include "exposure_state.h"
#include "contrast.h"
#include "util.h"

/**
 * Profile values for a specific contrast grade of a printing paper.
 */
typedef struct {
    /**
     * Log exposure value (x100) at Dmin + 0.04
     */
    uint32_t ht_lev100;

    /**
     * Log exposure value (x100) at Dmin + 0.60
     *
     * This is known as the "speed point," and can be directly converted
     * to the paper's specified ISO(P) value.
     *
     * It is optional, and should be omitted if not known with some accuracy
     * relative to the 'ht_lev100' value.
     */
    uint32_t hm_lev100;

    /**
     * Log exposure value (x100) at 0.90 * Dn
     *
     * This is the exposure point that gives 90% of Dmax, and is more
     * explicitly defined as:
     * Dmin + ((Dmax - Dmin) * 0.90)
     *
     * The paper's specified ISO(R) value is the difference between this
     * value and the value of 'ht_lev100'.
     *
     */
    uint32_t hs_lev100;

} paper_profile_grade_t;

/**
 * Profile used to define the characteristic curve of a printing paper.
 *
 * The values used in this profile shall be known throughout the system
 * as "Paper Exposure Values" (PEV) and are a form of logarithmic exposure
 * value that is defined as follows:
 *   PEV = log10(H) * 100
 * where H is the exposure, measured in lux-seconds, required to produce
 * a specific density.
 *
 * This format was chosen because it is the same unit a paper's contrast
 * range is specified in terms of, and because it provides an integer
 * value that is both convenient to enter and useful for calculations.
 *
 * When only two values are provided for a profile (Ht and Hs), the
 * characteristic curve will be simplified as a linear average gradient.
 * For some papers and contrast grades, this is sufficient.
 *
 * When all three values are provided for a profile, the characteristic
 * curve will be interpolated across these points. Therefore, it is important
 * to only include the third point (Hm) if it is determined via the same
 * process used to determine the other values.
 *
 * Terminology used for handling paper profiles will be largely based on
 * ISO 6846:1992(E) which is the standard for measuring the photographic
 * characteristics of papers used for black and white printing.
 */
typedef struct {
    /**
     * Name of the paper profile.
     */
    char name[PROFILE_NAME_LEN];

    /**
     * Profile data for each contrast grade.
     *
     * The whole-numbered grades are saved with the profile, while
     * the half-grades are calculated from them as needed.
     */
    paper_profile_grade_t grade[CONTRAST_GRADE_MAX];

    /**
     * Maximum net density (Dn) of the paper.
     *
     * This is the paper's maximum density value (Dmax), adjusted to be
     * relative to the paper base density (Dmin). It is necessary to
     * correctly place a contrast grade's 'hs_lev100' value on a density
     * scale.
     */
    float max_net_density;

    /**
     * Type of contrast filter used when exposing the paper.
     *
     * This value is used purely for display purposes, to improve the
     * usefulness of the displayed contrast grade to those using dichroic
     * enlarger heads.
     */
    contrast_filter_t contrast_filter;

} paper_profile_t;

/**
 * Check if the complete paper profile is valid.
 *
 * A valid paper profile contains either empty or valid contrast grade
 * profiles.
 */
bool paper_profile_is_valid(const paper_profile_t *profile);

/**
 * Check if a particular contrast grade profile is empty.
 */
bool paper_profile_grade_is_empty(const paper_profile_grade_t *profile_grade);

/**
 * Check if a particular contrast grade profile is valid.
 */
bool paper_profile_grade_is_valid(const paper_profile_grade_t *profile_grade);

/**
 * Check whether the two enlarger profiles are equivalent.
 */
bool paper_profile_compare(const paper_profile_t *profile1, const paper_profile_t *profile2);

/**
 * Update the values of any calculated paper profile members.
 *
 * This refreshes the values of portions of the profile that are derived
 * from the values of other portions, such as intermediate grade profiles.
 */
void paper_profile_recalculate(paper_profile_t *profile);

/**
 * Set the paper profile to a series of default values.
 *
 * These values should only be used as a fallback when no saved paper
 * profile is available, but should be avoided for any actual printing
 * use. They're roughly based on one interpretation of what an
 * Ilford MGIV RC paper profile should look like.
 */
void paper_profile_set_defaults(paper_profile_t *profile);

#endif /* PAPER_PROFILE_H */
