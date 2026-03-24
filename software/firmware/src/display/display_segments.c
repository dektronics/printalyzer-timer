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

typedef struct {
    display_seg_t segment;
    u8g2_uint_t line_count;
    const u8g2_uint_t *line_data;
} segment_data_t;

typedef struct {
    u8g2_uint_t width;
    u8g2_uint_t height;
    segment_data_t segment_data[7];
} segment_digit_data_t;

#pragma region DIGIT_FULL_SEG
static const u8g2_uint_t DIGIT_FULL_SEG_A_DATA[] = {
    1, 0, 28, 0,
    2, 1, 27, 1,
    3, 2, 26, 2,
    4, 3, 25, 3,
    5, 4, 24, 4
};
static const u8g2_uint_t DIGIT_FULL_SEG_B_DATA[] = {
    25, 5, 25, 25,
    26, 4, 26, 26,
    27, 3, 27, 27,
    28, 2, 28, 26,
    29, 1, 29, 25
};
static const u8g2_uint_t DIGIT_FULL_SEG_C_DATA[] = {
    25, 31, 25, 50,
    26, 30, 26, 51,
    27, 29, 27, 52,
    28, 30, 28, 53,
    29, 31, 29, 54
};
static const u8g2_uint_t DIGIT_FULL_SEG_D_DATA[] = {
    5, 51, 24, 51,
    4, 52, 25, 52,
    3, 53, 26, 53,
    2, 54, 27, 54,
    1, 55, 28, 55
};
static const u8g2_uint_t DIGIT_FULL_SEG_E_DATA[] = {
    0, 31, 0, 54,
    1, 30, 1, 53,
    2, 29, 2, 52,
    3, 30, 3, 51,
    4, 31, 4, 50
};
static const u8g2_uint_t DIGIT_FULL_SEG_F_DATA[] = {
    0, 1, 0, 25,
    1, 2, 1, 26,
    2, 3, 2, 27,
    3, 4, 3, 26,
    4, 5, 4, 25
};
static const u8g2_uint_t DIGIT_FULL_SEG_G_DATA[] = {
    5, 26, 24, 26,
    4, 27, 25, 27,
    3, 28, 26, 28,
    4, 29, 25, 29,
    5, 30, 24, 30
};

static const segment_digit_data_t DIGIT_FULL_DATA = {
    30, 56, {
        {seg_a, 5, DIGIT_FULL_SEG_A_DATA},
        {seg_b, 5, DIGIT_FULL_SEG_B_DATA},
        {seg_c, 5, DIGIT_FULL_SEG_C_DATA},
        {seg_d, 5, DIGIT_FULL_SEG_D_DATA},
        {seg_e, 5, DIGIT_FULL_SEG_E_DATA},
        {seg_f, 5, DIGIT_FULL_SEG_F_DATA},
        {seg_g, 5, DIGIT_FULL_SEG_G_DATA}
    }
};
#pragma endregion

#pragma region DIGIT_NARROW_SEG
static const u8g2_uint_t DIGIT_NARROW_SEG_A_DATA[] = {
    1, 0, 20, 0,
    2, 1, 19, 1,
    3, 2, 18, 2,
    4, 3, 17, 3,
    5, 4, 16, 4
};
static const u8g2_uint_t DIGIT_NARROW_SEG_B_DATA[] = {
    17, 5, 17, 25,
    18, 4, 18, 26,
    19, 3, 19, 27,
    20, 2, 20, 26,
    21, 1, 21, 25
};
static const u8g2_uint_t DIGIT_NARROW_SEG_C_DATA[] = {
    17, 31, 17, 50,
    18, 30, 18, 51,
    19, 29, 19, 52,
    20, 30, 20, 53,
    21, 31, 21, 54
};
static const u8g2_uint_t DIGIT_NARROW_SEG_D_DATA[] = {
    5, 51, 16, 51,
    4, 52, 17, 52,
    3, 53, 18, 53,
    2, 54, 19, 54,
    1, 55, 20, 55
};
static const u8g2_uint_t DIGIT_NARROW_SEG_E_DATA[] = {
    0, 31, 0, 54,
    1, 30, 1, 53,
    2, 29, 2, 52,
    3, 30, 3, 51,
    4, 31, 4, 50
};
static const u8g2_uint_t DIGIT_NARROW_SEG_F_DATA[] = {
    0, 1, 0, 25,
    1, 2, 1, 26,
    2, 3, 2, 27,
    3, 4, 3, 26,
    4, 5, 4, 25
};
static const u8g2_uint_t DIGIT_NARROW_SEG_G_DATA[] = {
    5, 26, 16, 26,
    4, 27, 17, 27,
    3, 28, 18, 28,
    4, 29, 17, 29,
    5, 30, 16, 30
};

static const segment_digit_data_t DIGIT_NARROW_DATA = {
    22, 56, {
        {seg_a, 5, DIGIT_NARROW_SEG_A_DATA},
        {seg_b, 5, DIGIT_NARROW_SEG_B_DATA},
        {seg_c, 5, DIGIT_NARROW_SEG_C_DATA},
        {seg_d, 5, DIGIT_NARROW_SEG_D_DATA},
        {seg_e, 5, DIGIT_NARROW_SEG_E_DATA},
        {seg_f, 5, DIGIT_NARROW_SEG_F_DATA},
        {seg_g, 5, DIGIT_NARROW_SEG_G_DATA}
    }
};
#pragma endregion

#pragma region DIGIT_MEDIUM_SEG
static const u8g2_uint_t DIGIT_MEDIUM_SEG_A_DATA[] = {
    1, 0, 16, 0,
    2, 1, 15, 1,
    3, 2, 14, 2
};
static const u8g2_uint_t DIGIT_MEDIUM_SEG_B_DATA[] = {
    15, 3, 15, 16,
    16, 2, 16, 17,
    17, 1, 17, 16
};
static const u8g2_uint_t DIGIT_MEDIUM_SEG_C_DATA[] = {
    15, 20, 15, 33,
    16, 19, 16, 34,
    17, 20, 17, 35
};
static const u8g2_uint_t DIGIT_MEDIUM_SEG_D_DATA[] = {
    3, 34, 14, 34,
    2, 35, 15, 35,
    1, 36, 16, 36
};
static const u8g2_uint_t DIGIT_MEDIUM_SEG_E_DATA[] = {
    0, 20, 0, 35,
    1, 19, 1, 34,
    2, 20, 2, 33
};
static const u8g2_uint_t DIGIT_MEDIUM_SEG_F_DATA[] = {
    0, 1, 0, 16,
    1, 2, 1, 17,
    2, 3, 2, 16
};
static const u8g2_uint_t DIGIT_MEDIUM_SEG_G_DATA[] = {
    3, 17, 14, 17,
    2, 18, 15, 18,
    3, 19, 14, 19
};

static const segment_digit_data_t DIGIT_MEDIUM_DATA = {
    18, 37, {
        {seg_a, 3, DIGIT_MEDIUM_SEG_A_DATA},
        {seg_b, 3, DIGIT_MEDIUM_SEG_B_DATA},
        {seg_c, 3, DIGIT_MEDIUM_SEG_C_DATA},
        {seg_d, 3, DIGIT_MEDIUM_SEG_D_DATA},
        {seg_e, 3, DIGIT_MEDIUM_SEG_E_DATA},
        {seg_f, 3, DIGIT_MEDIUM_SEG_F_DATA},
        {seg_g, 3, DIGIT_MEDIUM_SEG_G_DATA}
    }
};
#pragma endregion

#pragma region DIGIT_TINY_SEG
static const u8g2_uint_t DIGIT_TINY_SEG_A_DATA[] = {
    1, 0, 12, 0,
    2, 1, 11, 1,
    3, 2, 10, 2
};
static const u8g2_uint_t DIGIT_TINY_SEG_B_DATA[] = {
    11, 3, 11, 10,
    12, 2, 12, 11,
    13, 1, 13, 10
};
static const u8g2_uint_t DIGIT_TINY_SEG_C_DATA[] = {
    11, 14, 11, 21,
    12, 13, 12, 22,
    13, 14, 13, 23
};
static const u8g2_uint_t DIGIT_TINY_SEG_D_DATA[] = {
    3, 22, 10, 22,
    2, 23, 11, 23,
    1, 24, 12, 24
};
static const u8g2_uint_t DIGIT_TINY_SEG_E_DATA[] = {
    0, 14, 0, 23,
    1, 13, 1, 22,
    2, 14, 2, 21
};
static const u8g2_uint_t DIGIT_TINY_SEG_F_DATA[] = {
    0, 1, 0, 10,
    1, 2, 1, 11,
    2, 3, 2, 10
};
static const u8g2_uint_t DIGIT_TINY_SEG_G_DATA[] = {
    3, 11, 10, 11,
    2, 12, 11, 12,
    3, 13, 10, 13
};

static const segment_digit_data_t DIGIT_TINY_DATA = {
    14, 25, {
        {seg_a, 3, DIGIT_TINY_SEG_A_DATA},
        {seg_b, 3, DIGIT_TINY_SEG_B_DATA},
        {seg_c, 3, DIGIT_TINY_SEG_C_DATA},
        {seg_d, 3, DIGIT_TINY_SEG_D_DATA},
        {seg_e, 3, DIGIT_TINY_SEG_E_DATA},
        {seg_f, 3, DIGIT_TINY_SEG_F_DATA},
        {seg_g, 3, DIGIT_TINY_SEG_G_DATA}
    }
};
#pragma endregion

#pragma region DIGIT_VERY_TINY_SEG
static const u8g2_uint_t DIGIT_VERY_TINY_SEG_A_DATA[] = {
    1, 0, 7, 0,
    2, 1, 6, 1
};
static const u8g2_uint_t DIGIT_VERY_TINY_SEG_B_DATA[] = {
    7, 2, 7, 6,
    8, 1, 8, 7
};
static const u8g2_uint_t DIGIT_VERY_TINY_SEG_C_DATA[] = {
    7, 10, 7, 14,
    8, 9, 8, 15
};
static const u8g2_uint_t DIGIT_VERY_TINY_SEG_D_DATA[] = {
    2, 15, 6, 15,
    1, 16, 7, 16
};
static const u8g2_uint_t DIGIT_VERY_TINY_SEG_E_DATA[] = {
    0, 9, 0, 15,
    1, 10, 1, 14
};
static const u8g2_uint_t DIGIT_VERY_TINY_SEG_F_DATA[] = {
    0, 1, 0, 7,
    1, 2, 1, 6
};
static const u8g2_uint_t DIGIT_VERY_TINY_SEG_G_DATA[] = {
    2, 7, 6, 7,
    1, 8, 7, 8,
    2, 9, 6, 9
};

static const segment_digit_data_t DIGIT_VERY_TINY_DATA = {
    9, 17, {
        {seg_a, 2, DIGIT_VERY_TINY_SEG_A_DATA},
        {seg_b, 2, DIGIT_VERY_TINY_SEG_B_DATA},
        {seg_c, 2, DIGIT_VERY_TINY_SEG_C_DATA},
        {seg_d, 2, DIGIT_VERY_TINY_SEG_D_DATA},
        {seg_e, 2, DIGIT_VERY_TINY_SEG_E_DATA},
        {seg_f, 2, DIGIT_VERY_TINY_SEG_F_DATA},
        {seg_g, 3, DIGIT_VERY_TINY_SEG_G_DATA}
    }
};
#pragma endregion

static const segment_digit_data_t DIGIT_LIST[] = {
    DIGIT_FULL_DATA,
    DIGIT_NARROW_DATA,
    DIGIT_MEDIUM_DATA,
    DIGIT_TINY_DATA,
    DIGIT_VERY_TINY_DATA
};

static void display_draw_tdigit_fraction_part(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t value);
static void display_draw_tdigit_fraction_divider(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t max_value);

static void display_draw_digit_impl(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y,
    const segment_digit_data_t *seg_digit_data, uint8_t digit);
static void display_draw_segments_impl(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y,
    const segment_digit_data_t *seg_digit_data, display_seg_t segments);

void display_digit_draw(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, display_digit_t digit_size, uint8_t digit)
{
    if (digit_size >= DIGIT_MAX) { return; }
    display_draw_digit_impl(u8g2, x, y, &DIGIT_LIST[digit_size], digit);
}

u8g2_uint_t display_digit_width(display_digit_t digit_size)
{
    if (digit_size >= DIGIT_MAX) { return 0; }
    return DIGIT_LIST[digit_size].width;
}

u8g2_uint_t display_digit_height(display_digit_t digit_size)
{
    if (digit_size >= DIGIT_MAX) { return 0; }
    return DIGIT_LIST[digit_size].height;
}


void display_draw_digit(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit)
{
    display_digit_draw(u8g2, x, y, DIGIT_FULL, digit);
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
    display_draw_segments_impl(u8g2, x, y, &DIGIT_FULL_DATA, seg_a | seg_b | seg_c| seg_d);

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

void display_draw_ndigit(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit)
{
    display_digit_draw(u8g2, x, y, DIGIT_NARROW, digit);
}

void display_draw_mdigit(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit)
{
    display_digit_draw(u8g2, x, y, DIGIT_MEDIUM, digit);
}

void display_draw_tdigit(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit)
{
    display_digit_draw(u8g2, x, y, DIGIT_TINY, digit);
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
    display_digit_draw(u8g2, x, y, DIGIT_VERY_TINY, digit);
}

void display_draw_digit_impl(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y,
    const segment_digit_data_t *seg_digit_data, uint8_t digit)
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
    case UINT8_MAX:
        segments = seg_g;
        break;
    default:
        break;
    }

    display_draw_segments_impl(u8g2, x, y, seg_digit_data, segments);
}

void display_draw_segments_impl(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y,
    const segment_digit_data_t *seg_digit_data, display_seg_t segments)
{
    for (u8g2_uint_t i = 0; i < 7; i++) {
        if ((seg_digit_data->segment_data[i].segment & segments) != 0) {
            for (u8g2_uint_t j = 0; j < seg_digit_data->segment_data[i].line_count * 4; j += 4) {
                u8g2_DrawLine(u8g2,
                    x + seg_digit_data->segment_data[i].line_data[j], y + seg_digit_data->segment_data[i].line_data[j + 1],
                    x + seg_digit_data->segment_data[i].line_data[j + 2], y + seg_digit_data->segment_data[i].line_data[j + 3]);
            }

        }
    }
}
