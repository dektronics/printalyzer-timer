#ifndef STEP_WEDGE_H
#define STEP_WEDGE_H

#include <stdint.h>
#include <stddef.h>

/*
 * Profile describing the characteristics of a step wedge used for
 * paper calibration tests.
 */
typedef struct {
    /**
     * Name of the step wedge.
     */
    char name[32];

    /**
     * Density of the base of the step wedge.
     *
     * Used as the default value for the first step when there is no
     * calibration data.
     */
    float base_density;

    /**
     * Density increment of each successive patch of the step wedge.
     *
     * Used in combination with the base density to determine step
     * densities when there is no calibration data.
     */
    float density_increment;

    /**
     * Number of patches on the step wedge.
     */
    uint32_t step_count;

    /**
     * Calibration data for each patch on the step wedge.
     *
     * This array should contain 'step_count' elements, where each element
     * is either a valid density value or NAN. When this value is NAN,
     * the patch density can be determined from the base and increment values.
     */
    float step_density[];

} step_wedge_t;

/**
 * Index for the stock step wedge profile to be loaded when
 * there is no existing configuration.
 */
#define DEFAULT_STOCK_WEDGE_INDEX 2

/**
 * Minimum number of steps that are supported for a step wedge.
 */
#define MIN_STEP_WEDGE_STEP_COUNT 5

/**
 * Maximum number of steps that are supported for a step wedge.
 */
#define MAX_STEP_WEDGE_STEP_COUNT 51

/**
 * Get the name of a particular stock step wedge profile.
 */
const char *step_wedge_stock_name(size_t index);

/**
 * Get the number of patches on a particular stock step wedge profile.
 */
int step_wedge_stock_step_count(size_t index);

/**
 * Get the number of available stock step wedge profiles.
 */
int step_wedge_stock_count();

/**
 * Create a new step wedge structure with default properties.
 *
 * The structure is allocated to contain the number of steps provided
 * as an argument.
 */
step_wedge_t *step_wedge_create(uint32_t step_count);

/**
 * Create a new step wedge structure from a stock profile.
 */
step_wedge_t *step_wedge_create_from_stock(size_t index);

/**
 * Create a copy of an existing step wedge profile with a different step count.
 */
step_wedge_t *step_wedge_copy(const step_wedge_t *source_wedge, uint32_t step_count);

/**
 * Free the provided step wedge structure.
 */
void step_wedge_free(step_wedge_t *wedge);

/**
 * Get the density at a particular patch of the step wedge.
 *
 * Will either return a calculated or calibrated value depending on what
 * exists within the profile.
 */
float step_wedge_get_density(const step_wedge_t *wedge, uint32_t step);

#endif /* STEP_WEDGE_H */
