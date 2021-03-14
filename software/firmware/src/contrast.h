#ifndef CONTRAST_H
#define CONTRAST_H

/**
 * Enumerated contrast grades used for exposure settings
 * and paper profiles.
 */
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
} contrast_grade_t;

/**
 * Different types of contrast filters that can be used for
 * contrast grade filtration.
 */
typedef enum {
    CONTRAST_FILTER_REGULAR = 0,
    CONTRAST_FILTER_DURST_170M,
    CONTRAST_FILTER_DURST_130M,
    CONTRAST_FILTER_KODAK,
    CONTRAST_FILTER_LEITZ_FOCOMAT_V35,
    CONTRAST_FILTER_MEOPTA,
    CONTRAST_FILTER_MAX
} contrast_filter_t;

const char *contrast_grade_str(contrast_grade_t contrast_grade);

const char *contrast_filter_str(contrast_filter_t filter, contrast_grade_t contrast_grade);

#endif /* CONTRAST_H */
