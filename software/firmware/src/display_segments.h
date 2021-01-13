#ifndef DISPLAY_SEGMENTS_H
#define DISPLAY_SEGMENTS_H

#include "u8g2.h"

/**
 * These functions support drawing 7-segment style numeric digits
 * on the display, in a variety of sizes.
 */

/**
 * Draw a 30x56 pixel digit
 */
void display_draw_digit(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit);

/**
 * Draw a 18x37 pixel digit
 */
void display_draw_mdigit(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit);

/**
 * Draw a 14x25 pixel digit
 */
void display_draw_tdigit(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit);

/**
 * Draw a 9x17 pixel digit
 */
void display_draw_vtdigit(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit);

#endif /* DISPLAY_SEGMENTS_H */
