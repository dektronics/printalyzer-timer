/*
 * General purpose utility functions
 */

#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

#include "display.h"
#include "exposure_state.h"

/**
 * Convert the current exposure state into display elements.
 */
void convert_exposure_to_display(display_main_elements_t *elements, const exposure_state_t *exposure);

/**
 * Populate display timer elements with a fresh time value.
 *
 * This sets both the time and the number of fraction digits.
 */
void convert_exposure_to_display_timer(display_exposure_timer_t *elements, uint32_t exposure_ms);

/**
 * Update display timer elements with a new time value.
 *
 * This sets just the time, and not any other members.
 */
void update_display_timer(display_exposure_timer_t *elements, uint32_t exposure_ms);

/**
 * Round an integer to the nearest 10th.
 */
uint32_t round_to_10(uint32_t n);

/**
 * Convert an exposure time from seconds to milliseconds with rounding.
 *
 * This function takes an exposure time in floating-point seconds, as it is
 * normally maintained for calculation purposes, and produces an equivalent
 * time in milliseconds. This milliseconds value is then rounded to the
 * nearest 10ms, which is the smallest increment used for display and timing.
 */
uint32_t rounded_exposure_time_ms(float seconds);

#endif /* UTIL_H */
