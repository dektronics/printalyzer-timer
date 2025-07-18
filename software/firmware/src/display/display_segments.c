#include "display_segments.h"

#include "display.h"
#include "u8g2.h"
#include "util.h"

typedef enum {
    seg_a = 0x01,
    seg_b = 0x02,
    seg_c = 0x04,
    seg_d = 0x08,
    seg_e = 0x10,
    seg_f = 0x20,
    seg_g = 0x40
} display_seg_t;

static void display_draw_tdigit_fraction_part(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t value);
static void display_draw_tdigit_fraction_divider(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t max_value);

typedef void (*display_draw_segment_func)(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, display_seg_t segments);

static void display_draw_digit_impl(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit,
    display_draw_segment_func draw_func);
static void display_draw_segment(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, display_seg_t segments);
static void display_draw_msegment(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, display_seg_t segments);
static void display_draw_tsegment(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, display_seg_t segments);
static void display_draw_vtsegment(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, display_seg_t segments);

void display_draw_digit(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit)
{
    display_draw_digit_impl(u8g2, x, y, digit, display_draw_segment);
}

void display_draw_digit_sign(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, bool positive)
{
    u8g2_DrawLine(u8g2, x + 4, y + 26, x + 24, y + 26);
    u8g2_DrawLine(u8g2, x + 3, y + 27, x + 25, y + 27);
    u8g2_DrawLine(u8g2, x + 2, y + 28, x + 26, y + 28);
    u8g2_DrawLine(u8g2, x + 3, y + 29, x + 25, y + 29);
    u8g2_DrawLine(u8g2, x + 4, y + 30, x + 24, y + 30);

    if (positive) {
        u8g2_DrawLine(u8g2, x + 12, y + 18, x + 12, y + 38);
        u8g2_DrawLine(u8g2, x + 13, y + 17, x + 13, y + 39);
        u8g2_DrawLine(u8g2, x + 14, y + 16, x + 14, y + 40);
        u8g2_DrawLine(u8g2, x + 15, y + 17, x + 15, y + 39);
        u8g2_DrawLine(u8g2, x + 16, y + 18, x + 16, y + 38);
    }
}

void display_draw_digit_letter_d(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y)
{
    display_draw_segment(u8g2, x, y, seg_a | seg_b | seg_c| seg_d);

    u8g2_DrawLine(u8g2, x + 12, y + 6, x + 12, y + 25);
    u8g2_DrawLine(u8g2, x + 13, y + 6, x + 13, y + 26);
    u8g2_DrawLine(u8g2, x + 14, y + 6, x + 14, y + 27);
    u8g2_DrawLine(u8g2, x + 15, y + 6, x + 15, y + 26);
    u8g2_DrawLine(u8g2, x + 16, y + 6, x + 16, y + 25);

    u8g2_DrawLine(u8g2, x + 12, y + 31, x + 12, y + 49);
    u8g2_DrawLine(u8g2, x + 13, y + 30, x + 13, y + 49);
    u8g2_DrawLine(u8g2, x + 14, y + 29, x + 14, y + 49);
    u8g2_DrawLine(u8g2, x + 15, y + 30, x + 15, y + 49);
    u8g2_DrawLine(u8g2, x + 16, y + 31, x + 16, y + 49);
}

void display_draw_mdigit(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit)
{
    display_draw_digit_impl(u8g2, x, y, digit, display_draw_msegment);
}

void display_draw_tdigit(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit)
{
    display_draw_digit_impl(u8g2, x, y, digit, display_draw_tsegment);
}

void display_draw_tdigit_fraction(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t num, uint8_t den)
{
    uint8_t max_value = MAX(num, den);

    // Draw the numerator
    display_draw_tdigit_fraction_part(u8g2, x, y, num);

    // Draw the dividing line
    display_draw_tdigit_fraction_divider(u8g2, x, y + 27, max_value);

    // Draw the denominator
    display_draw_tdigit_fraction_part(u8g2, x, y + 31, den);
}

void display_draw_tdigit_fraction_part(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t value)
{
    if (value >= 20) {
        // NN/DD (1/20 - 1/99)
        display_draw_tdigit(u8g2, x + 5, y, value % 100 / 10);
        display_draw_tdigit(u8g2, x + 22, y, value % 10);
    } else if (value >= 10) {
        // NN/DD (1/10 - 1/19)
        display_draw_tdigit(u8g2, x + 0, y, value % 100 / 10);
        display_draw_tdigit(u8g2, x + 17, y, value % 10);
    } else if (value == 0 || value > 1) {
        // N/D
        display_draw_tdigit(u8g2, x + 13, y, value);
    } else {
        // N/D (1/1)
        display_draw_tdigit(u8g2, x + 7, y, value);
    }
}

void display_draw_tdigit_fraction_divider(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t max_value)
{
    if (max_value >= 20) {
        // N/DD (1/20 - 1/99)
        u8g2_DrawLine(u8g2, x + 2,  y + 0, x + 37, y + 0);
        u8g2_DrawLine(u8g2, x + 1,  y + 1, x + 38, y + 1);
        u8g2_DrawLine(u8g2, x + 2,  y + 2, x + 37, y + 2);
    } else if (max_value >= 10) {
        // N/DD (1/10 - 1/19)
        u8g2_DrawLine(u8g2, x + 8,  y + 0, x + 32, y + 0);
        u8g2_DrawLine(u8g2, x + 7,  y + 1, x + 33, y + 1);
        u8g2_DrawLine(u8g2, x + 8,  y + 2, x + 32, y + 2);
    } else if (max_value == 0 || max_value > 1) {
        // N/D
        u8g2_DrawLine(u8g2, x + 10, y + 0, x + 28, y + 0);
        u8g2_DrawLine(u8g2, x + 9,  y + 1, x + 29, y + 1);
        u8g2_DrawLine(u8g2, x + 10, y + 2, x + 28, y + 2);
    }
}

void display_draw_vtdigit(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit)
{
    display_draw_digit_impl(u8g2, x, y, digit, display_draw_vtsegment);
}

void display_draw_digit_impl(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit,
    display_draw_segment_func draw_func)
{
    display_seg_t segments = 0;
    switch(digit) {
    case 0:
        segments = seg_a | seg_b | seg_c | seg_d | seg_e | seg_f;
        break;
    case 1:
        segments = seg_b | seg_c;
        break;
    case 2:
        segments = seg_a | seg_b | seg_d | seg_e | seg_g;
        break;
    case 3:
        segments = seg_a | seg_b | seg_c | seg_d | seg_g;
        break;
    case 4:
        segments = seg_b | seg_c | seg_f | seg_g;
        break;
    case 5:
        segments = seg_a | seg_c | seg_d | seg_f | seg_g;
        break;
    case 6:
        segments = seg_a | seg_c | seg_d | seg_e | seg_f | seg_g;
        break;
    case 7:
        segments = seg_a | seg_b | seg_c;
        break;
    case 8:
        segments = seg_a | seg_b | seg_c | seg_d | seg_e | seg_f | seg_g;
        break;
    case 9:
        segments = seg_a | seg_b | seg_c | seg_d | seg_f | seg_g;
        break;
    default:
        break;
    }
    draw_func(u8g2, x, y, segments);
}

/**
 * Draw a segments of a digit on a 30x56 pixel grid
 */
void display_draw_segment(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, display_seg_t segments)
{
    if ((segments & seg_a) != 0) {
        u8g2_DrawLine(u8g2, x + 1, y + 0, x + 28, y + 0);
        u8g2_DrawLine(u8g2, x + 2, y + 1, x + 27, y + 1);
        u8g2_DrawLine(u8g2, x + 3, y + 2, x + 26, y + 2);
        u8g2_DrawLine(u8g2, x + 4, y + 3, x + 25, y + 3);
        u8g2_DrawLine(u8g2, x + 5, y + 4, x + 24, y + 4);
    }
    if ((segments & seg_b) != 0) {
        u8g2_DrawLine(u8g2, x + 25, y + 5, x + 25, y + 25);
        u8g2_DrawLine(u8g2, x + 26, y + 4, x + 26, y + 26);
        u8g2_DrawLine(u8g2, x + 27, y + 3, x + 27, y + 27);
        u8g2_DrawLine(u8g2, x + 28, y + 2, x + 28, y + 26);
        u8g2_DrawLine(u8g2, x + 29, y + 1, x + 29, y + 25);
    }
    if ((segments & seg_c) != 0) {
        u8g2_DrawLine(u8g2, x + 25, y + 31, x + 25, y + 50);
        u8g2_DrawLine(u8g2, x + 26, y + 30, x + 26, y + 51);
        u8g2_DrawLine(u8g2, x + 27, y + 29, x + 27, y + 52);
        u8g2_DrawLine(u8g2, x + 28, y + 30, x + 28, y + 53);
        u8g2_DrawLine(u8g2, x + 29, y + 31, x + 29, y + 54);
    }
    if ((segments & seg_d) != 0) {
        u8g2_DrawLine(u8g2, x + 5, y + 51, x + 24, y + 51);
        u8g2_DrawLine(u8g2, x + 4, y + 52, x + 25, y + 52);
        u8g2_DrawLine(u8g2, x + 3, y + 53, x + 26, y + 53);
        u8g2_DrawLine(u8g2, x + 2, y + 54, x + 27, y + 54);
        u8g2_DrawLine(u8g2, x + 1, y + 55, x + 28, y + 55);
    }
    if ((segments & seg_e) != 0) {
        u8g2_DrawLine(u8g2, x + 0, y + 31, x + 0, y + 54);
        u8g2_DrawLine(u8g2, x + 1, y + 30, x + 1, y + 53);
        u8g2_DrawLine(u8g2, x + 2, y + 29, x + 2, y + 52);
        u8g2_DrawLine(u8g2, x + 3, y + 30, x + 3, y + 51);
        u8g2_DrawLine(u8g2, x + 4, y + 31, x + 4, y + 50);
    }
    if ((segments & seg_f) != 0) {
        u8g2_DrawLine(u8g2, x + 0, y + 1, x + 0, y + 25);
        u8g2_DrawLine(u8g2, x + 1, y + 2, x + 1, y + 26);
        u8g2_DrawLine(u8g2, x + 2, y + 3, x + 2, y + 27);
        u8g2_DrawLine(u8g2, x + 3, y + 4, x + 3, y + 26);
        u8g2_DrawLine(u8g2, x + 4, y + 5, x + 4, y + 25);
    }
    if ((segments & seg_g) != 0) {
        u8g2_DrawLine(u8g2, x + 5, y + 26, x + 24, y + 26);
        u8g2_DrawLine(u8g2, x + 4, y + 27, x + 25, y + 27);
        u8g2_DrawLine(u8g2, x + 3, y + 28, x + 26, y + 28);
        u8g2_DrawLine(u8g2, x + 4, y + 29, x + 25, y + 29);
        u8g2_DrawLine(u8g2, x + 5, y + 30, x + 24, y + 30);
    }
}

/**
 * Draw a segments of a digit on a 18x37 pixel grid
 */
void display_draw_msegment(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, display_seg_t segments)
{
    if ((segments & seg_a) != 0) {
        u8g2_DrawLine(u8g2, x + 1, y + 0, x + 16, y + 0);
        u8g2_DrawLine(u8g2, x + 2, y + 1, x + 15, y + 1);
        u8g2_DrawLine(u8g2, x + 3, y + 2, x + 14, y + 2);
    }
    if ((segments & seg_b) != 0) {
        u8g2_DrawLine(u8g2, x + 15, y + 3, x + 15, y + 16);
        u8g2_DrawLine(u8g2, x + 16, y + 2, x + 16, y + 17);
        u8g2_DrawLine(u8g2, x + 17, y + 1, x + 17, y + 16);
    }
    if ((segments & seg_c) != 0) {
        u8g2_DrawLine(u8g2, x + 15, y + 20, x + 15, y + 33);
        u8g2_DrawLine(u8g2, x + 16, y + 19, x + 16, y + 34);
        u8g2_DrawLine(u8g2, x + 17, y + 20, x + 17, y + 35);
    }
    if ((segments & seg_d) != 0) {
        u8g2_DrawLine(u8g2, x + 3, y + 34, x + 14, y + 34);
        u8g2_DrawLine(u8g2, x + 2, y + 35, x + 15, y + 35);
        u8g2_DrawLine(u8g2, x + 1, y + 36, x + 16, y + 36);
    }
    if ((segments & seg_e) != 0) {
        u8g2_DrawLine(u8g2, x + 0, y + 20, x + 0, y + 35);
        u8g2_DrawLine(u8g2, x + 1, y + 19, x + 1, y + 34);
        u8g2_DrawLine(u8g2, x + 2, y + 20, x + 2, y + 33);
    }
    if ((segments & seg_f) != 0) {
        u8g2_DrawLine(u8g2, x + 0, y + 1, x + 0, y + 16);
        u8g2_DrawLine(u8g2, x + 1, y + 2, x + 1, y + 17);
        u8g2_DrawLine(u8g2, x + 2, y + 3, x + 2, y + 16);
    }
    if ((segments & seg_g) != 0) {
        u8g2_DrawLine(u8g2, x + 3, y + 17, x + 14, y + 17);
        u8g2_DrawLine(u8g2, x + 2, y + 18, x + 15, y + 18);
        u8g2_DrawLine(u8g2, x + 3, y + 19, x + 14, y + 19);
    }
}

/**
 * Draw a segments of a digit on a 14x25 pixel grid
 */
void display_draw_tsegment(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, display_seg_t segments)
{
    if ((segments & seg_a) != 0) {
        u8g2_DrawLine(u8g2, x + 1, y + 0, x + 12, y + 0);
        u8g2_DrawLine(u8g2, x + 2, y + 1, x + 11, y + 1);
        u8g2_DrawLine(u8g2, x + 3, y + 2, x + 10, y + 2);
    }
    if ((segments & seg_b) != 0) {
        u8g2_DrawLine(u8g2, x + 11, y + 3, x + 11, y + 10);
        u8g2_DrawLine(u8g2, x + 12, y + 2, x + 12, y + 11);
        u8g2_DrawLine(u8g2, x + 13, y + 1, x + 13, y + 10);
    }
    if ((segments & seg_c) != 0) {
        u8g2_DrawLine(u8g2, x + 11, y + 14, x + 11, y + 21);
        u8g2_DrawLine(u8g2, x + 12, y + 13, x + 12, y + 22);
        u8g2_DrawLine(u8g2, x + 13, y + 14, x + 13, y + 23);
    }
    if ((segments & seg_d) != 0) {
        u8g2_DrawLine(u8g2, x + 3, y + 22, x + 10, y + 22);
        u8g2_DrawLine(u8g2, x + 2, y + 23, x + 11, y + 23);
        u8g2_DrawLine(u8g2, x + 1, y + 24, x + 12, y + 24);
    }
    if ((segments & seg_e) != 0) {
        u8g2_DrawLine(u8g2, x + 0, y + 14, x + 0, y + 23);
        u8g2_DrawLine(u8g2, x + 1, y + 13, x + 1, y + 22);
        u8g2_DrawLine(u8g2, x + 2, y + 14, x + 2, y + 21);
    }
    if ((segments & seg_f) != 0) {
        u8g2_DrawLine(u8g2, x + 0, y + 1, x + 0, y + 10);
        u8g2_DrawLine(u8g2, x + 1, y + 2, x + 1, y + 11);
        u8g2_DrawLine(u8g2, x + 2, y + 3, x + 2, y + 10);
    }
    if ((segments & seg_g) != 0) {
        u8g2_DrawLine(u8g2, x + 3, y + 11, x + 10, y + 11);
        u8g2_DrawLine(u8g2, x + 2, y + 12, x + 11, y + 12);
        u8g2_DrawLine(u8g2, x + 3, y + 13, x + 10, y + 13);
    }
}

/**
 * Draw a segments of a digit on a 9x17 pixel grid
 */
void display_draw_vtsegment(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, display_seg_t segments)
{
    if ((segments & seg_a) != 0) {
        u8g2_DrawLine(u8g2, x + 1, y + 0, x + 7, y + 0);
        u8g2_DrawLine(u8g2, x + 2, y + 1, x + 6, y + 1);
    }
    if ((segments & seg_b) != 0) {
        u8g2_DrawLine(u8g2, x + 7, y + 2, x + 7, y + 6);
        u8g2_DrawLine(u8g2, x + 8, y + 1, x + 8, y + 7);
    }
    if ((segments & seg_c) != 0) {
        u8g2_DrawLine(u8g2, x + 7, y + 10, x + 7, y + 14);
        u8g2_DrawLine(u8g2, x + 8, y + 9, x + 8, y + 15);
    }
    if ((segments & seg_d) != 0) {
        u8g2_DrawLine(u8g2, x + 2, y + 15, x + 6, y + 15);
        u8g2_DrawLine(u8g2, x + 1, y + 16, x + 7, y + 16);
    }
    if ((segments & seg_e) != 0) {
        u8g2_DrawLine(u8g2, x + 0, y + 9, x + 0, y + 15);
        u8g2_DrawLine(u8g2, x + 1, y + 10, x + 1, y + 14);
    }
    if ((segments & seg_f) != 0) {
        u8g2_DrawLine(u8g2, x + 0, y + 1, x + 0, y + 7);
        u8g2_DrawLine(u8g2, x + 1, y + 2, x + 1, y + 6);
    }
    if ((segments & seg_g) != 0) {
        u8g2_DrawLine(u8g2, x + 2, y + 7, x + 6, y + 7);
        u8g2_DrawLine(u8g2, x + 1, y + 8, x + 7, y + 8);
        u8g2_DrawLine(u8g2, x + 2, y + 9, x + 6, y + 9);
    }
}
