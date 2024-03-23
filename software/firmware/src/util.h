/*
 * General purpose utility functions
 */

#ifndef UTIL_H
#define UTIL_H

#include <FreeRTOS.h>
#include <cmsis_os.h>
#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

typedef struct __display_exposure_timer_t display_exposure_timer_t;
typedef struct __display_main_printing_elements_t display_main_printing_elements_t;
typedef struct __display_main_densitometer_elements_t display_main_densitometer_elements_t;
typedef struct __display_main_calibration_elements_t display_main_calibration_elements_t;
typedef struct __exposure_state_t exposure_state_t;
typedef struct __enlarger_config_t enlarger_config_t;

#define SAFELIGHT_OFF_DELAY pdMS_TO_TICKS(500)

#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#endif

/** Standard length for all profile name strings */
#define PROFILE_NAME_LEN (32U)

/**
 * Convert the current exposure state into printing display elements.
 */
void convert_exposure_to_display_printing(display_main_printing_elements_t *elements,
    const exposure_state_t *exposure, const enlarger_config_t *enlarger);

/**
 * Convert the current exposure state into densitometer mode display elements.
 */
void convert_exposure_to_display_densitometer(display_main_densitometer_elements_t *elements, const exposure_state_t *exposure);

/**
 * Convert the current exposure state into calibration mode display elements.
 */
void convert_exposure_to_display_calibration(display_main_calibration_elements_t *elements, const exposure_state_t *exposure);

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

/**
 * Check if the provided floating point number is valid.
 */
bool is_valid_number(float num);

/**
 * Convert two adjacent 8-bit elements in an array into a 16-bit unsigned integer
 * @param src Pointer to the source array
 * @param val Output value
 */
uint16_t conv_array_u16(const uint8_t *src);

/**
 * Convert a 16-bit unsigned integer into two adjacent 8-bit elements in an array
 * @param dst Pointer to the destination array
 * @param src Input value
 */
void conv_u16_array(uint8_t *dst, uint16_t src);

uint8_t value_adjust_with_rollover_u8(uint8_t value, int8_t increment, uint8_t lower_bound, uint8_t upper_bound);
uint16_t value_adjust_with_rollover_u16(uint16_t value, int16_t increment, uint16_t lower_bound, uint16_t upper_bound);

osStatus_t hal_to_os_status(HAL_StatusTypeDef hal_status);
HAL_StatusTypeDef os_to_hal_status(osStatus_t os_status);

HAL_StatusTypeDef usb_to_hal_status(int usb_status);
osStatus_t usb_to_os_status(int usb_status);

/**
 * Scrub a user-provided file name to ensure it meets requirements.
 *
 * @param filename The filename to scrub
 * @param file extension (e.g. ".dat")
 * @return True if the filename was modified
 */
bool scrub_export_filename(char *filename, const char *ext);

#endif /* UTIL_H */
