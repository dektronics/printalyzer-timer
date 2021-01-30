/*
 * General purpose utility functions
 */

#ifndef UTIL_H
#define UTIL_H

#include <FreeRTOS.h>
#include <stdint.h>

#include "display.h"
#include "exposure_state.h"

#define SAFELIGHT_OFF_DELAY pdMS_TO_TICKS(500)

/**
 * Convert the current exposure state into display elements.
 */
void convert_exposure_to_display(display_main_elements_t *elements, const exposure_state_t *exposure);

/**
 * Convert a contrast grade from the exposure enum to the display enum.
 */
display_grade_t convert_exposure_to_display_contrast(exposure_contrast_grade_t contrast_grade);

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

/**
 * Pad the provided string out to the specified length.
 *
 * If the string is longer than the specified length, then it will not be
 * modified and its existing length will be returned.
 *
 * @param str    The string to pad
 * @param c      The character to pad the string with
 * @param length The length to pad the string out to
 * @return Length of the string after padding, excluding the
 *         terminating null byte.
 */
size_t pad_str_to_length(char *str, char c, size_t length);

/**
 * Break a fraction into sign, whole, numerator, and denominator
 * portions and append it to the provided string.
 *
 * @param str The string to append onto
 * @param numerator The fraction numerator, which can be larger than the denominator
 * @param denominator The fraction denominator
 * @return Number of characters appended to the string
 */
size_t append_signed_fraction(char *str, int8_t numerator, uint8_t denominator);

/**
 * Append an exposure time, in seconds, to the provided string.
 *
 * This does all the necessary rounding and formatting so that
 * the time appears as desired for consistent display.
 *
 * @param str The string to append onto
 * @param time The time, in floating point seconds, to format and append
 * @return Number of characters appended to the string
 */
size_t append_exposure_time(char *str, float time);

/**
 * Perform polynomial interpolation on a graph defined by 3 points.
 *
 * Interpolates a graph based on the three (x,y) points provided, and
 * determines the y value that corresponds to the supplied x value.
 */
float interpolate(float x1, float y1, float x2, float y2, float x3, float y3, float x);

#endif /* UTIL_H */
