#ifndef DISPLAY_SEGMENTS_H
#define DISPLAY_SEGMENTS_H

#include <stdbool.h>
#include "u8g2.h"

/**
 * These functions support drawing 7-segment style numeric digits
 * on the display, in a variety of sizes.
 */

typedef enum : uint8_t {
    DIGIT_FULL = 0, /*!< 30x56 pixel digit */
    DIGIT_NARROW,   /*!< 22x56 pixel digit */
    DIGIT_MEDIUM,   /*!< 18x37 pixel digit */
    DIGIT_TINY,     /*!< 14x25 pixel digit */
    DIGIT_VERY_TINY,/*!< 9x17 pixel digit */
    DIGIT_MAX
} display_digit_t;

/**
 * Draw a digit of the specified size
 *
 * @param x X position for the digit
 * @param y Y position for the digit
 * @param digit_size size of the digit
 * @param digit digit to draw
 */
void display_digit_draw(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, display_digit_t digit_size, uint8_t digit);

/**
 * Get the width of the specified digit size
 */
u8g2_uint_t display_digit_width(display_digit_t digit_size);

/**
 * Get the height of the specified digit size
 */
u8g2_uint_t display_digit_height(display_digit_t digit_size);

/**
 * Draw a 30x56 pixel digit
 */
void display_draw_digit(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit);

/**
 * Draw a +/- sign on a 30x56 pixel digit grid
 */
void display_draw_digit_sign(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, bool positive);

/**
 * Draw the letter "D"  on a 30x56 pixel digit grid
 */
void display_draw_digit_letter_d(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y);

/**
 * Draw a 22x56 pixel digit
 */
void display_draw_ndigit(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit);

/**
 * Draw a 18x37 pixel digit
 */
void display_draw_mdigit(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit);

/**
 * Draw a 14x25 pixel digit
 */
void display_draw_tdigit(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit);

/**
 * Draw a fraction with 14x25 pixel digits and a divider.
 *
 * This fraction fits adjacent to a 30x56 pixel digit.
 */
void display_draw_tdigit_fraction(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t num, uint8_t den);

/**
 * Draw a 9x17 pixel digit
 */
void display_draw_vtdigit(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit);

#endif /* DISPLAY_SEGMENTS_H */
