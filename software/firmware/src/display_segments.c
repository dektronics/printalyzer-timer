#include "display_segments.h"

#include "display.h"
#include "u8g2.h"

void display_draw_digit(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit)
{
    switch(digit) {
    case 0:
        display_draw_segment(u8g2, x, y, seg_a);
        display_draw_segment(u8g2, x, y, seg_b);
        display_draw_segment(u8g2, x, y, seg_c);
        display_draw_segment(u8g2, x, y, seg_d);
        display_draw_segment(u8g2, x, y, seg_e);
        display_draw_segment(u8g2, x, y, seg_f);
        break;
    case 1:
        display_draw_segment(u8g2, x, y, seg_b);
        display_draw_segment(u8g2, x, y, seg_c);
        break;
    case 2:
        display_draw_segment(u8g2, x, y, seg_a);
        display_draw_segment(u8g2, x, y, seg_b);
        display_draw_segment(u8g2, x, y, seg_d);
        display_draw_segment(u8g2, x, y, seg_e);
        display_draw_segment(u8g2, x, y, seg_g);
        break;
    case 3:
        display_draw_segment(u8g2, x, y, seg_a);
        display_draw_segment(u8g2, x, y, seg_b);
        display_draw_segment(u8g2, x, y, seg_c);
        display_draw_segment(u8g2, x, y, seg_d);
        display_draw_segment(u8g2, x, y, seg_g);
        break;
    case 4:
        display_draw_segment(u8g2, x, y, seg_b);
        display_draw_segment(u8g2, x, y, seg_c);
        display_draw_segment(u8g2, x, y, seg_f);
        display_draw_segment(u8g2, x, y, seg_g);
        break;
    case 5:
        display_draw_segment(u8g2, x, y, seg_a);
        display_draw_segment(u8g2, x, y, seg_c);
        display_draw_segment(u8g2, x, y, seg_d);
        display_draw_segment(u8g2, x, y, seg_f);
        display_draw_segment(u8g2, x, y, seg_g);
        break;
    case 6:
        display_draw_segment(u8g2, x, y, seg_a);
        display_draw_segment(u8g2, x, y, seg_c);
        display_draw_segment(u8g2, x, y, seg_d);
        display_draw_segment(u8g2, x, y, seg_e);
        display_draw_segment(u8g2, x, y, seg_f);
        display_draw_segment(u8g2, x, y, seg_g);
        break;
    case 7:
        display_draw_segment(u8g2, x, y, seg_a);
        display_draw_segment(u8g2, x, y, seg_b);
        display_draw_segment(u8g2, x, y, seg_c);
        break;
    case 8:
        display_draw_segment(u8g2, x, y, seg_a);
        display_draw_segment(u8g2, x, y, seg_b);
        display_draw_segment(u8g2, x, y, seg_c);
        display_draw_segment(u8g2, x, y, seg_d);
        display_draw_segment(u8g2, x, y, seg_e);
        display_draw_segment(u8g2, x, y, seg_f);
        display_draw_segment(u8g2, x, y, seg_g);
        break;
    case 9:
        display_draw_segment(u8g2, x, y, seg_a);
        display_draw_segment(u8g2, x, y, seg_b);
        display_draw_segment(u8g2, x, y, seg_c);
        display_draw_segment(u8g2, x, y, seg_d);
        display_draw_segment(u8g2, x, y, seg_f);
        display_draw_segment(u8g2, x, y, seg_g);
        break;
    default:
        break;
    }
}

void display_draw_mdigit(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit)
{
    switch(digit) {
    case 0:
        display_draw_msegment(u8g2, x, y, seg_a);
        display_draw_msegment(u8g2, x, y, seg_b);
        display_draw_msegment(u8g2, x, y, seg_c);
        display_draw_msegment(u8g2, x, y, seg_d);
        display_draw_msegment(u8g2, x, y, seg_e);
        display_draw_msegment(u8g2, x, y, seg_f);
        break;
    case 1:
        display_draw_msegment(u8g2, x, y, seg_b);
        display_draw_msegment(u8g2, x, y, seg_c);
        break;
    case 2:
        display_draw_msegment(u8g2, x, y, seg_a);
        display_draw_msegment(u8g2, x, y, seg_b);
        display_draw_msegment(u8g2, x, y, seg_d);
        display_draw_msegment(u8g2, x, y, seg_e);
        display_draw_msegment(u8g2, x, y, seg_g);
        break;
    case 3:
        display_draw_msegment(u8g2, x, y, seg_a);
        display_draw_msegment(u8g2, x, y, seg_b);
        display_draw_msegment(u8g2, x, y, seg_c);
        display_draw_msegment(u8g2, x, y, seg_d);
        display_draw_msegment(u8g2, x, y, seg_g);
        break;
    case 4:
        display_draw_msegment(u8g2, x, y, seg_b);
        display_draw_msegment(u8g2, x, y, seg_c);
        display_draw_msegment(u8g2, x, y, seg_f);
        display_draw_msegment(u8g2, x, y, seg_g);
        break;
    case 5:
        display_draw_msegment(u8g2, x, y, seg_a);
        display_draw_msegment(u8g2, x, y, seg_c);
        display_draw_msegment(u8g2, x, y, seg_d);
        display_draw_msegment(u8g2, x, y, seg_f);
        display_draw_msegment(u8g2, x, y, seg_g);
        break;
    case 6:
        display_draw_msegment(u8g2, x, y, seg_a);
        display_draw_msegment(u8g2, x, y, seg_c);
        display_draw_msegment(u8g2, x, y, seg_d);
        display_draw_msegment(u8g2, x, y, seg_e);
        display_draw_msegment(u8g2, x, y, seg_f);
        display_draw_msegment(u8g2, x, y, seg_g);
        break;
    case 7:
        display_draw_msegment(u8g2, x, y, seg_a);
        display_draw_msegment(u8g2, x, y, seg_b);
        display_draw_msegment(u8g2, x, y, seg_c);
        break;
    case 8:
        display_draw_msegment(u8g2, x, y, seg_a);
        display_draw_msegment(u8g2, x, y, seg_b);
        display_draw_msegment(u8g2, x, y, seg_c);
        display_draw_msegment(u8g2, x, y, seg_d);
        display_draw_msegment(u8g2, x, y, seg_e);
        display_draw_msegment(u8g2, x, y, seg_f);
        display_draw_msegment(u8g2, x, y, seg_g);
        break;
    case 9:
        display_draw_msegment(u8g2, x, y, seg_a);
        display_draw_msegment(u8g2, x, y, seg_b);
        display_draw_msegment(u8g2, x, y, seg_c);
        display_draw_msegment(u8g2, x, y, seg_d);
        display_draw_msegment(u8g2, x, y, seg_f);
        display_draw_msegment(u8g2, x, y, seg_g);
        break;
    default:
        break;
    }
}

void display_draw_tdigit(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit)
{
    switch(digit) {
    case 0:
        display_draw_tsegment(u8g2, x, y, seg_a);
        display_draw_tsegment(u8g2, x, y, seg_b);
        display_draw_tsegment(u8g2, x, y, seg_c);
        display_draw_tsegment(u8g2, x, y, seg_d);
        display_draw_tsegment(u8g2, x, y, seg_e);
        display_draw_tsegment(u8g2, x, y, seg_f);
        break;
    case 1:
        display_draw_tsegment(u8g2, x, y, seg_b);
        display_draw_tsegment(u8g2, x, y, seg_c);
        break;
    case 2:
        display_draw_tsegment(u8g2, x, y, seg_a);
        display_draw_tsegment(u8g2, x, y, seg_b);
        display_draw_tsegment(u8g2, x, y, seg_d);
        display_draw_tsegment(u8g2, x, y, seg_e);
        display_draw_tsegment(u8g2, x, y, seg_g);
        break;
    case 3:
        display_draw_tsegment(u8g2, x, y, seg_a);
        display_draw_tsegment(u8g2, x, y, seg_b);
        display_draw_tsegment(u8g2, x, y, seg_c);
        display_draw_tsegment(u8g2, x, y, seg_d);
        display_draw_tsegment(u8g2, x, y, seg_g);
        break;
    case 4:
        display_draw_tsegment(u8g2, x, y, seg_b);
        display_draw_tsegment(u8g2, x, y, seg_c);
        display_draw_tsegment(u8g2, x, y, seg_f);
        display_draw_tsegment(u8g2, x, y, seg_g);
        break;
    case 5:
        display_draw_tsegment(u8g2, x, y, seg_a);
        display_draw_tsegment(u8g2, x, y, seg_c);
        display_draw_tsegment(u8g2, x, y, seg_d);
        display_draw_tsegment(u8g2, x, y, seg_f);
        display_draw_tsegment(u8g2, x, y, seg_g);
        break;
    case 6:
        display_draw_tsegment(u8g2, x, y, seg_a);
        display_draw_tsegment(u8g2, x, y, seg_c);
        display_draw_tsegment(u8g2, x, y, seg_d);
        display_draw_tsegment(u8g2, x, y, seg_e);
        display_draw_tsegment(u8g2, x, y, seg_f);
        display_draw_tsegment(u8g2, x, y, seg_g);
        break;
    case 7:
        display_draw_tsegment(u8g2, x, y, seg_a);
        display_draw_tsegment(u8g2, x, y, seg_b);
        display_draw_tsegment(u8g2, x, y, seg_c);
        break;
    case 8:
        display_draw_tsegment(u8g2, x, y, seg_a);
        display_draw_tsegment(u8g2, x, y, seg_b);
        display_draw_tsegment(u8g2, x, y, seg_c);
        display_draw_tsegment(u8g2, x, y, seg_d);
        display_draw_tsegment(u8g2, x, y, seg_e);
        display_draw_tsegment(u8g2, x, y, seg_f);
        display_draw_tsegment(u8g2, x, y, seg_g);
        break;
    case 9:
        display_draw_tsegment(u8g2, x, y, seg_a);
        display_draw_tsegment(u8g2, x, y, seg_b);
        display_draw_tsegment(u8g2, x, y, seg_c);
        display_draw_tsegment(u8g2, x, y, seg_d);
        display_draw_tsegment(u8g2, x, y, seg_f);
        display_draw_tsegment(u8g2, x, y, seg_g);
        break;
    default:
        break;
    }
}

void display_draw_vtdigit(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, uint8_t digit)
{
    switch(digit) {
    case 0:
        display_draw_vtsegment(u8g2, x, y, seg_a);
        display_draw_vtsegment(u8g2, x, y, seg_b);
        display_draw_vtsegment(u8g2, x, y, seg_c);
        display_draw_vtsegment(u8g2, x, y, seg_d);
        display_draw_vtsegment(u8g2, x, y, seg_e);
        display_draw_vtsegment(u8g2, x, y, seg_f);
        break;
    case 1:
        display_draw_vtsegment(u8g2, x, y, seg_b);
        display_draw_vtsegment(u8g2, x, y, seg_c);
        break;
    case 2:
        display_draw_vtsegment(u8g2, x, y, seg_a);
        display_draw_vtsegment(u8g2, x, y, seg_b);
        display_draw_vtsegment(u8g2, x, y, seg_d);
        display_draw_vtsegment(u8g2, x, y, seg_e);
        display_draw_vtsegment(u8g2, x, y, seg_g);
        break;
    case 3:
        display_draw_vtsegment(u8g2, x, y, seg_a);
        display_draw_vtsegment(u8g2, x, y, seg_b);
        display_draw_vtsegment(u8g2, x, y, seg_c);
        display_draw_vtsegment(u8g2, x, y, seg_d);
        display_draw_vtsegment(u8g2, x, y, seg_g);
        break;
    case 4:
        display_draw_vtsegment(u8g2, x, y, seg_b);
        display_draw_vtsegment(u8g2, x, y, seg_c);
        display_draw_vtsegment(u8g2, x, y, seg_f);
        display_draw_vtsegment(u8g2, x, y, seg_g);
        break;
    case 5:
        display_draw_vtsegment(u8g2, x, y, seg_a);
        display_draw_vtsegment(u8g2, x, y, seg_c);
        display_draw_vtsegment(u8g2, x, y, seg_d);
        display_draw_vtsegment(u8g2, x, y, seg_f);
        display_draw_vtsegment(u8g2, x, y, seg_g);
        break;
    case 6:
        display_draw_vtsegment(u8g2, x, y, seg_a);
        display_draw_vtsegment(u8g2, x, y, seg_c);
        display_draw_vtsegment(u8g2, x, y, seg_d);
        display_draw_vtsegment(u8g2, x, y, seg_e);
        display_draw_vtsegment(u8g2, x, y, seg_f);
        display_draw_vtsegment(u8g2, x, y, seg_g);
        break;
    case 7:
        display_draw_vtsegment(u8g2, x, y, seg_a);
        display_draw_vtsegment(u8g2, x, y, seg_b);
        display_draw_vtsegment(u8g2, x, y, seg_c);
        break;
    case 8:
        display_draw_vtsegment(u8g2, x, y, seg_a);
        display_draw_vtsegment(u8g2, x, y, seg_b);
        display_draw_vtsegment(u8g2, x, y, seg_c);
        display_draw_vtsegment(u8g2, x, y, seg_d);
        display_draw_vtsegment(u8g2, x, y, seg_e);
        display_draw_vtsegment(u8g2, x, y, seg_f);
        display_draw_vtsegment(u8g2, x, y, seg_g);
        break;
    case 9:
        display_draw_vtsegment(u8g2, x, y, seg_a);
        display_draw_vtsegment(u8g2, x, y, seg_b);
        display_draw_vtsegment(u8g2, x, y, seg_c);
        display_draw_vtsegment(u8g2, x, y, seg_d);
        display_draw_vtsegment(u8g2, x, y, seg_f);
        display_draw_vtsegment(u8g2, x, y, seg_g);
        break;
    default:
        break;
    }
}

void display_draw_segment(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, display_seg_t segment)
{
    switch(segment) {
    case seg_a:
        u8g2_DrawLine(u8g2, x + 1, y + 0, x + 28, y + 0);
        u8g2_DrawLine(u8g2, x + 2, y + 1, x + 27, y + 1);
        u8g2_DrawLine(u8g2, x + 3, y + 2, x + 26, y + 2);
        u8g2_DrawLine(u8g2, x + 4, y + 3, x + 25, y + 3);
        u8g2_DrawLine(u8g2, x + 5, y + 4, x + 24, y + 4);
        break;
    case seg_b:
        u8g2_DrawLine(u8g2, x + 25, y + 5, x + 25, y + 25);
        u8g2_DrawLine(u8g2, x + 26, y + 4, x + 26, y + 26);
        u8g2_DrawLine(u8g2, x + 27, y + 3, x + 27, y + 27);
        u8g2_DrawLine(u8g2, x + 28, y + 2, x + 28, y + 26);
        u8g2_DrawLine(u8g2, x + 29, y + 1, x + 29, y + 25);
        break;
    case seg_c:
        u8g2_DrawLine(u8g2, x + 25, y + 31, x + 25, y + 50);
        u8g2_DrawLine(u8g2, x + 26, y + 30, x + 26, y + 51);
        u8g2_DrawLine(u8g2, x + 27, y + 29, x + 27, y + 52);
        u8g2_DrawLine(u8g2, x + 28, y + 30, x + 28, y + 53);
        u8g2_DrawLine(u8g2, x + 29, y + 31, x + 29, y + 54);
        break;
    case seg_d:
        u8g2_DrawLine(u8g2, x + 5, y + 51, x + 24, y + 51);
        u8g2_DrawLine(u8g2, x + 4, y + 52, x + 25, y + 52);
        u8g2_DrawLine(u8g2, x + 3, y + 53, x + 26, y + 53);
        u8g2_DrawLine(u8g2, x + 2, y + 54, x + 27, y + 54);
        u8g2_DrawLine(u8g2, x + 1, y + 55, x + 28, y + 55);
        break;
    case seg_e:
        u8g2_DrawLine(u8g2, x + 0, y + 31, x + 0, y + 54);
        u8g2_DrawLine(u8g2, x + 1, y + 30, x + 1, y + 53);
        u8g2_DrawLine(u8g2, x + 2, y + 29, x + 2, y + 52);
        u8g2_DrawLine(u8g2, x + 3, y + 30, x + 3, y + 51);
        u8g2_DrawLine(u8g2, x + 4, y + 31, x + 4, y + 50);
        break;
    case seg_f:
        u8g2_DrawLine(u8g2, x + 0, y + 1, x + 0, y + 25);
        u8g2_DrawLine(u8g2, x + 1, y + 2, x + 1, y + 26);
        u8g2_DrawLine(u8g2, x + 2, y + 3, x + 2, y + 27);
        u8g2_DrawLine(u8g2, x + 3, y + 4, x + 3, y + 26);
        u8g2_DrawLine(u8g2, x + 4, y + 5, x + 4, y + 25);
        break;
    case seg_g:
        u8g2_DrawLine(u8g2, x + 5, y + 26, x + 24, y + 26);
        u8g2_DrawLine(u8g2, x + 4, y + 27, x + 25, y + 27);
        u8g2_DrawLine(u8g2, x + 3, y + 28, x + 26, y + 28);
        u8g2_DrawLine(u8g2, x + 4, y + 29, x + 25, y + 29);
        u8g2_DrawLine(u8g2, x + 5, y + 30, x + 24, y + 30);
        break;
    default:
        break;
    }
}

void display_draw_msegment(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, display_seg_t segment)
{
    switch(segment) {
    case seg_a:
        u8g2_DrawLine(u8g2, x + 1, y + 0, x + 16, y + 0);
        u8g2_DrawLine(u8g2, x + 2, y + 1, x + 15, y + 1);
        u8g2_DrawLine(u8g2, x + 3, y + 2, x + 14, y + 2);
        break;
    case seg_b:
        u8g2_DrawLine(u8g2, x + 15, y + 3, x + 15, y + 16);
        u8g2_DrawLine(u8g2, x + 16, y + 2, x + 16, y + 17);
        u8g2_DrawLine(u8g2, x + 17, y + 1, x + 17, y + 16);
        break;
    case seg_c:
        u8g2_DrawLine(u8g2, x + 15, y + 20, x + 15, y + 33);
        u8g2_DrawLine(u8g2, x + 16, y + 19, x + 16, y + 34);
        u8g2_DrawLine(u8g2, x + 17, y + 20, x + 17, y + 35);
        break;
    case seg_d:
        u8g2_DrawLine(u8g2, x + 3, y + 34, x + 14, y + 34);
        u8g2_DrawLine(u8g2, x + 2, y + 35, x + 15, y + 35);
        u8g2_DrawLine(u8g2, x + 1, y + 36, x + 16, y + 36);
        break;
    case seg_e:
        u8g2_DrawLine(u8g2, x + 0, y + 20, x + 0, y + 35);
        u8g2_DrawLine(u8g2, x + 1, y + 19, x + 1, y + 34);
        u8g2_DrawLine(u8g2, x + 2, y + 20, x + 2, y + 33);
        break;
    case seg_f:
        u8g2_DrawLine(u8g2, x + 0, y + 1, x + 0, y + 16);
        u8g2_DrawLine(u8g2, x + 1, y + 2, x + 1, y + 17);
        u8g2_DrawLine(u8g2, x + 2, y + 3, x + 2, y + 16);
        break;
    case seg_g:
        u8g2_DrawLine(u8g2, x + 3, y + 17, x + 14, y + 17);
        u8g2_DrawLine(u8g2, x + 2, y + 18, x + 15, y + 18);
        u8g2_DrawLine(u8g2, x + 3, y + 19, x + 14, y + 19);
        break;
    default:
        break;
    }
}

void display_draw_tsegment(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, display_seg_t segment)
{
    switch(segment) {
    case seg_a:
        u8g2_DrawLine(u8g2, x + 1, y + 0, x + 12, y + 0);
        u8g2_DrawLine(u8g2, x + 2, y + 1, x + 11, y + 1);
        u8g2_DrawLine(u8g2, x + 3, y + 2, x + 10, y + 2);
        break;
    case seg_b:
        u8g2_DrawLine(u8g2, x + 11, y + 3, x + 11, y + 10);
        u8g2_DrawLine(u8g2, x + 12, y + 2, x + 12, y + 11);
        u8g2_DrawLine(u8g2, x + 13, y + 1, x + 13, y + 10);
        break;
    case seg_c:
        u8g2_DrawLine(u8g2, x + 11, y + 14, x + 11, y + 21);
        u8g2_DrawLine(u8g2, x + 12, y + 13, x + 12, y + 22);
        u8g2_DrawLine(u8g2, x + 13, y + 14, x + 13, y + 23);
        break;
    case seg_d:
        u8g2_DrawLine(u8g2, x + 3, y + 22, x + 10, y + 22);
        u8g2_DrawLine(u8g2, x + 2, y + 23, x + 11, y + 23);
        u8g2_DrawLine(u8g2, x + 1, y + 24, x + 12, y + 24);
        break;
    case seg_e:
        u8g2_DrawLine(u8g2, x + 0, y + 14, x + 0, y + 23);
        u8g2_DrawLine(u8g2, x + 1, y + 13, x + 1, y + 22);
        u8g2_DrawLine(u8g2, x + 2, y + 14, x + 2, y + 21);
        break;
    case seg_f:
        u8g2_DrawLine(u8g2, x + 0, y + 1, x + 0, y + 10);
        u8g2_DrawLine(u8g2, x + 1, y + 2, x + 1, y + 11);
        u8g2_DrawLine(u8g2, x + 2, y + 3, x + 2, y + 10);
        break;
    case seg_g:
        u8g2_DrawLine(u8g2, x + 3, y + 11, x + 10, y + 11);
        u8g2_DrawLine(u8g2, x + 2, y + 12, x + 11, y + 12);
        u8g2_DrawLine(u8g2, x + 3, y + 13, x + 10, y + 13);
        break;
    default:
        break;
    }
}

void display_draw_vtsegment(u8g2_t *u8g2, u8g2_uint_t x, u8g2_uint_t y, display_seg_t segment)
{
    switch(segment) {
    case seg_a:
        u8g2_DrawLine(u8g2, x + 1, y + 0, x + 7, y + 0);
        u8g2_DrawLine(u8g2, x + 2, y + 1, x + 6, y + 1);
        break;
    case seg_b:
        u8g2_DrawLine(u8g2, x + 7, y + 2, x + 7, y + 6);
        u8g2_DrawLine(u8g2, x + 8, y + 1, x + 8, y + 7);
        break;
    case seg_c:
        u8g2_DrawLine(u8g2, x + 7, y + 10, x + 7, y + 14);
        u8g2_DrawLine(u8g2, x + 8, y + 9, x + 8, y + 15);
        break;
    case seg_d:
        u8g2_DrawLine(u8g2, x + 2, y + 15, x + 6, y + 15);
        u8g2_DrawLine(u8g2, x + 1, y + 16, x + 7, y + 16);
        break;
    case seg_e:
        u8g2_DrawLine(u8g2, x + 0, y + 9, x + 0, y + 15);
        u8g2_DrawLine(u8g2, x + 1, y + 10, x + 1, y + 14);
        break;
    case seg_f:
        u8g2_DrawLine(u8g2, x + 0, y + 1, x + 0, y + 7);
        u8g2_DrawLine(u8g2, x + 1, y + 2, x + 1, y + 6);
        break;
    case seg_g:
        u8g2_DrawLine(u8g2, x + 2, y + 7, x + 6, y + 7);
        u8g2_DrawLine(u8g2, x + 1, y + 8, x + 7, y + 8);
        u8g2_DrawLine(u8g2, x + 2, y + 9, x + 6, y + 9);
        break;
    default:
        break;
    }
}
