#include "display.h"

#include <FreeRTOS.h>
#include <semphr.h>
#include <cmsis_os.h>
#include <stdlib.h>
#include <esp_log.h>

#include "u8g2_stm32_hal.h"
#include "u8g2.h"
#include "display_assets.h"
#include "keypad.h"

#define MENU_TIMEOUT_MS 30000

static const char *TAG = "display";

/* Library function declarations */
void u8g2_DrawSelectionList(u8g2_t *u8g2, u8sl_t *u8sl, u8g2_uint_t y, const char *s);

typedef enum {
    seg_a,
    seg_b,
    seg_c,
    seg_d,
    seg_e,
    seg_f,
    seg_g
} display_seg_t;

static u8g2_t u8g2;

static osMutexId_t display_mutex;
static const osMutexAttr_t display_mutex_attributes = {
  .name = "display_mutex"
};

static uint8_t display_contrast = 0x9F;
static uint8_t display_brightness = 0x0F;
static bool menu_event_timeout = false;

static void display_set_freq(uint8_t value);
static void display_draw_segment(u8g2_uint_t x, u8g2_uint_t y, display_seg_t segment);
static void display_draw_tsegment(u8g2_uint_t x, u8g2_uint_t y, display_seg_t segment);
static void display_draw_digit(u8g2_uint_t x, u8g2_uint_t y, uint8_t digit);
static void display_draw_sign(u8g2_uint_t x, u8g2_uint_t y, bool positive);
static void display_draw_tdigit(u8g2_uint_t x, u8g2_uint_t y, uint8_t digit);
static void display_draw_tone_graph(uint32_t tone_graph);
static void display_draw_contrast_grade(display_grade_t grade);
static void display_draw_counter_time(uint16_t seconds, uint16_t milliseconds, uint8_t fraction_digits);
static void display_draw_counter_time_small(u8g2_uint_t x2, u8g2_uint_t y1, const display_exposure_timer_t *time_elements);
static void display_prepare_menu_font();

HAL_StatusTypeDef display_init(const u8g2_display_handle_t *display_handle)
{
    ESP_LOGD(TAG, "display_init");

    // Initialize the STM32 display HAL, which includes library display
    // parameter and callback initialization.
    u8g2_stm32_hal_init(&u8g2, display_handle);

    u8g2_InitDisplay(&u8g2);

    // Slightly increase the display refresh frequency
    display_set_freq(0xC1);

    display_mutex = osMutexNew(&display_mutex_attributes);
    if (!display_mutex) {
        ESP_LOGE(TAG, "xSemaphoreCreateMutex error");
    }

    return HAL_OK;
}

void display_set_freq(uint8_t value)
{
    u8x8_t *u8x8 = &(u8g2.u8x8);
    u8x8_cad_StartTransfer(u8x8);
    u8x8_cad_SendCmd(u8x8, 0x0B3);
    u8x8_cad_SendArg(u8x8, value);
    u8x8_cad_EndTransfer(u8x8);
}

void display_clear()
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8g2_ClearBuffer(&u8g2);

    osMutexRelease(display_mutex);
}

void display_enable(bool enabled)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8g2_SetPowerSave(&u8g2, enabled ? 0 : 1);

    osMutexRelease(display_mutex);
}

void display_set_contrast(uint8_t value)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8g2_SetContrast(&u8g2, value);
    display_contrast = value;

    osMutexRelease(display_mutex);
}

uint8_t display_get_contrast()
{
    return display_contrast;
}

void display_set_brightness(uint8_t value)
{
    uint8_t arg = value & 0x0F;

    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8x8_t *u8x8 = &(u8g2.u8x8);
    u8x8_cad_StartTransfer(u8x8);
    u8x8_cad_SendCmd(u8x8, 0x0C7);
    u8x8_cad_SendArg(u8x8, arg);
    u8x8_cad_EndTransfer(u8x8);

    display_brightness = arg;

    osMutexRelease(display_mutex);
}

uint8_t display_get_brightness()
{
    return display_brightness;
}

void display_draw_test_pattern(bool mode)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8g2_ClearBuffer(&u8g2);
    u8g2_SetDrawColor(&u8g2, 1);

    bool draw = mode;
    for (int y = 0; y < 64; y += 16) {
        for (int x = 0; x < 256; x += 16) {
            draw = !draw;
            if (draw) {
                u8g2_DrawBox(&u8g2, x, y, 16, 16);
            }
        }
        draw = !draw;
    }

    u8g2_SendBuffer(&u8g2);

    osMutexRelease(display_mutex);
}

void display_draw_logo()
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8g2_ClearBuffer(&u8g2);
    u8g2_SetBitmapMode(&u8g2, 1);
    asset_info_t asset;
    display_asset_get(&asset, ASSET_PRINTALYZER);
    u8g2_DrawXBM(&u8g2, 0, 0, asset.width, asset.height, asset.bits);
    u8g2_SendBuffer(&u8g2);

    osMutexRelease(display_mutex);
}

/**
 * Draw a segment of a digit on a 30x56 pixel grid
 */
void display_draw_segment(u8g2_uint_t x, u8g2_uint_t y, display_seg_t segment)
{
    switch(segment) {
    case seg_a:
        u8g2_DrawLine(&u8g2, x + 1, y + 0, x + 28, y + 0);
        u8g2_DrawLine(&u8g2, x + 2, y + 1, x + 27, y + 1);
        u8g2_DrawLine(&u8g2, x + 3, y + 2, x + 26, y + 2);
        u8g2_DrawLine(&u8g2, x + 4, y + 3, x + 25, y + 3);
        u8g2_DrawLine(&u8g2, x + 5, y + 4, x + 24, y + 4);
        break;
    case seg_b:
        u8g2_DrawLine(&u8g2, x + 25, y + 5, x + 25, y + 25);
        u8g2_DrawLine(&u8g2, x + 26, y + 4, x + 26, y + 26);
        u8g2_DrawLine(&u8g2, x + 27, y + 3, x + 27, y + 27);
        u8g2_DrawLine(&u8g2, x + 28, y + 2, x + 28, y + 26);
        u8g2_DrawLine(&u8g2, x + 29, y + 1, x + 29, y + 25);
        break;
    case seg_c:
        u8g2_DrawLine(&u8g2, x + 25, y + 31, x + 25, y + 50);
        u8g2_DrawLine(&u8g2, x + 26, y + 30, x + 26, y + 51);
        u8g2_DrawLine(&u8g2, x + 27, y + 29, x + 27, y + 52);
        u8g2_DrawLine(&u8g2, x + 28, y + 30, x + 28, y + 53);
        u8g2_DrawLine(&u8g2, x + 29, y + 31, x + 29, y + 54);
        break;
    case seg_d:
        u8g2_DrawLine(&u8g2, x + 5, y + 51, x + 24, y + 51);
        u8g2_DrawLine(&u8g2, x + 4, y + 52, x + 25, y + 52);
        u8g2_DrawLine(&u8g2, x + 3, y + 53, x + 26, y + 53);
        u8g2_DrawLine(&u8g2, x + 2, y + 54, x + 27, y + 54);
        u8g2_DrawLine(&u8g2, x + 1, y + 55, x + 28, y + 55);
        break;
    case seg_e:
        u8g2_DrawLine(&u8g2, x + 0, y + 31, x + 0, y + 54);
        u8g2_DrawLine(&u8g2, x + 1, y + 30, x + 1, y + 53);
        u8g2_DrawLine(&u8g2, x + 2, y + 29, x + 2, y + 52);
        u8g2_DrawLine(&u8g2, x + 3, y + 30, x + 3, y + 51);
        u8g2_DrawLine(&u8g2, x + 4, y + 31, x + 4, y + 50);
        break;
    case seg_f:
        u8g2_DrawLine(&u8g2, x + 0, y + 1, x + 0, y + 25);
        u8g2_DrawLine(&u8g2, x + 1, y + 2, x + 1, y + 26);
        u8g2_DrawLine(&u8g2, x + 2, y + 3, x + 2, y + 27);
        u8g2_DrawLine(&u8g2, x + 3, y + 4, x + 3, y + 26);
        u8g2_DrawLine(&u8g2, x + 4, y + 5, x + 4, y + 25);
        break;
    case seg_g:
        u8g2_DrawLine(&u8g2, x + 5, y + 26, x + 24, y + 26);
        u8g2_DrawLine(&u8g2, x + 4, y + 27, x + 25, y + 27);
        u8g2_DrawLine(&u8g2, x + 3, y + 28, x + 26, y + 28);
        u8g2_DrawLine(&u8g2, x + 4, y + 29, x + 25, y + 29);
        u8g2_DrawLine(&u8g2, x + 5, y + 30, x + 24, y + 30);
        break;
    default:
        break;
    }
}

/**
 * Draw a segment of a digit on a 14x25 pixel grid
 */
void display_draw_tsegment(u8g2_uint_t x, u8g2_uint_t y, display_seg_t segment)
{
    switch(segment) {
    case seg_a:
        u8g2_DrawLine(&u8g2, x + 1, y + 0, x + 12, y + 0);
        u8g2_DrawLine(&u8g2, x + 2, y + 1, x + 11, y + 1);
        u8g2_DrawLine(&u8g2, x + 3, y + 2, x + 10, y + 2);
        break;
    case seg_b:
        u8g2_DrawLine(&u8g2, x + 11, y + 3, x + 11, y + 10);
        u8g2_DrawLine(&u8g2, x + 12, y + 2, x + 12, y + 11);
        u8g2_DrawLine(&u8g2, x + 13, y + 1, x + 13, y + 10);
        break;
    case seg_c:
        u8g2_DrawLine(&u8g2, x + 11, y + 14, x + 11, y + 21);
        u8g2_DrawLine(&u8g2, x + 12, y + 13, x + 12, y + 22);
        u8g2_DrawLine(&u8g2, x + 13, y + 14, x + 13, y + 23);
        break;
    case seg_d:
        u8g2_DrawLine(&u8g2, x + 3, y + 22, x + 10, y + 22);
        u8g2_DrawLine(&u8g2, x + 2, y + 23, x + 11, y + 23);
        u8g2_DrawLine(&u8g2, x + 1, y + 24, x + 12, y + 24);
        break;
    case seg_e:
        u8g2_DrawLine(&u8g2, x + 0, y + 14, x + 0, y + 23);
        u8g2_DrawLine(&u8g2, x + 1, y + 13, x + 1, y + 22);
        u8g2_DrawLine(&u8g2, x + 2, y + 14, x + 2, y + 21);
        break;
    case seg_f:
        u8g2_DrawLine(&u8g2, x + 0, y + 1, x + 0, y + 10);
        u8g2_DrawLine(&u8g2, x + 1, y + 2, x + 1, y + 11);
        u8g2_DrawLine(&u8g2, x + 2, y + 3, x + 2, y + 10);
        break;
    case seg_g:
        u8g2_DrawLine(&u8g2, x + 3, y + 11, x + 10, y + 11);
        u8g2_DrawLine(&u8g2, x + 2, y + 12, x + 11, y + 12);
        u8g2_DrawLine(&u8g2, x + 3, y + 13, x + 10, y + 13);
        break;
    default:
        break;
    }
}

/**
 * Draw a 30x56 pixel digit
 */
void display_draw_digit(u8g2_uint_t x, u8g2_uint_t y, uint8_t digit)
{
    switch(digit) {
    case 0:
        display_draw_segment(x, y, seg_a);
        display_draw_segment(x, y, seg_b);
        display_draw_segment(x, y, seg_c);
        display_draw_segment(x, y, seg_d);
        display_draw_segment(x, y, seg_e);
        display_draw_segment(x, y, seg_f);
        break;
    case 1:
        display_draw_segment(x, y, seg_b);
        display_draw_segment(x, y, seg_c);
        break;
    case 2:
        display_draw_segment(x, y, seg_a);
        display_draw_segment(x, y, seg_b);
        display_draw_segment(x, y, seg_d);
        display_draw_segment(x, y, seg_e);
        display_draw_segment(x, y, seg_g);
        break;
    case 3:
        display_draw_segment(x, y, seg_a);
        display_draw_segment(x, y, seg_b);
        display_draw_segment(x, y, seg_c);
        display_draw_segment(x, y, seg_d);
        display_draw_segment(x, y, seg_g);
        break;
    case 4:
        display_draw_segment(x, y, seg_b);
        display_draw_segment(x, y, seg_c);
        display_draw_segment(x, y, seg_f);
        display_draw_segment(x, y, seg_g);
        break;
    case 5:
        display_draw_segment(x, y, seg_a);
        display_draw_segment(x, y, seg_c);
        display_draw_segment(x, y, seg_d);
        display_draw_segment(x, y, seg_f);
        display_draw_segment(x, y, seg_g);
        break;
    case 6:
        display_draw_segment(x, y, seg_a);
        display_draw_segment(x, y, seg_c);
        display_draw_segment(x, y, seg_d);
        display_draw_segment(x, y, seg_e);
        display_draw_segment(x, y, seg_f);
        display_draw_segment(x, y, seg_g);
        break;
    case 7:
        display_draw_segment(x, y, seg_a);
        display_draw_segment(x, y, seg_b);
        display_draw_segment(x, y, seg_c);
        break;
    case 8:
        display_draw_segment(x, y, seg_a);
        display_draw_segment(x, y, seg_b);
        display_draw_segment(x, y, seg_c);
        display_draw_segment(x, y, seg_d);
        display_draw_segment(x, y, seg_e);
        display_draw_segment(x, y, seg_f);
        display_draw_segment(x, y, seg_g);
        break;
    case 9:
        display_draw_segment(x, y, seg_a);
        display_draw_segment(x, y, seg_b);
        display_draw_segment(x, y, seg_c);
        display_draw_segment(x, y, seg_d);
        display_draw_segment(x, y, seg_f);
        display_draw_segment(x, y, seg_g);
        break;
    default:
        break;
    }
}

/**
 * Draw a +/- sign on a 30x56 pixel digit grid
 */
void display_draw_sign(u8g2_uint_t x, u8g2_uint_t y, bool positive)
{
    u8g2_DrawLine(&u8g2, x + 4, y + 26, x + 24, y + 26);
    u8g2_DrawLine(&u8g2, x + 3, y + 27, x + 25, y + 27);
    u8g2_DrawLine(&u8g2, x + 2, y + 28, x + 26, y + 28);
    u8g2_DrawLine(&u8g2, x + 3, y + 29, x + 25, y + 29);
    u8g2_DrawLine(&u8g2, x + 4, y + 30, x + 24, y + 30);

    if (positive) {
        u8g2_DrawLine(&u8g2, x + 12, y + 18, x + 12, y + 38);
        u8g2_DrawLine(&u8g2, x + 13, y + 17, x + 13, y + 39);
        u8g2_DrawLine(&u8g2, x + 14, y + 16, x + 14, y + 40);
        u8g2_DrawLine(&u8g2, x + 15, y + 17, x + 15, y + 39);
        u8g2_DrawLine(&u8g2, x + 16, y + 18, x + 16, y + 38);
    }
}

/**
 * Draw a 14x25 pixel digit
 */
void display_draw_tdigit(u8g2_uint_t x, u8g2_uint_t y, uint8_t digit)
{
    switch(digit) {
    case 0:
        display_draw_tsegment(x, y, seg_a);
        display_draw_tsegment(x, y, seg_b);
        display_draw_tsegment(x, y, seg_c);
        display_draw_tsegment(x, y, seg_d);
        display_draw_tsegment(x, y, seg_e);
        display_draw_tsegment(x, y, seg_f);
        break;
    case 1:
        display_draw_tsegment(x, y, seg_b);
        display_draw_tsegment(x, y, seg_c);
        break;
    case 2:
        display_draw_tsegment(x, y, seg_a);
        display_draw_tsegment(x, y, seg_b);
        display_draw_tsegment(x, y, seg_d);
        display_draw_tsegment(x, y, seg_e);
        display_draw_tsegment(x, y, seg_g);
        break;
    case 3:
        display_draw_tsegment(x, y, seg_a);
        display_draw_tsegment(x, y, seg_b);
        display_draw_tsegment(x, y, seg_c);
        display_draw_tsegment(x, y, seg_d);
        display_draw_tsegment(x, y, seg_g);
        break;
    case 4:
        display_draw_tsegment(x, y, seg_b);
        display_draw_tsegment(x, y, seg_c);
        display_draw_tsegment(x, y, seg_f);
        display_draw_tsegment(x, y, seg_g);
        break;
    case 5:
        display_draw_tsegment(x, y, seg_a);
        display_draw_tsegment(x, y, seg_c);
        display_draw_tsegment(x, y, seg_d);
        display_draw_tsegment(x, y, seg_f);
        display_draw_tsegment(x, y, seg_g);
        break;
    case 6:
        display_draw_tsegment(x, y, seg_a);
        display_draw_tsegment(x, y, seg_c);
        display_draw_tsegment(x, y, seg_d);
        display_draw_tsegment(x, y, seg_e);
        display_draw_tsegment(x, y, seg_f);
        display_draw_tsegment(x, y, seg_g);
        break;
    case 7:
        display_draw_tsegment(x, y, seg_a);
        display_draw_tsegment(x, y, seg_b);
        display_draw_tsegment(x, y, seg_c);
        break;
    case 8:
        display_draw_tsegment(x, y, seg_a);
        display_draw_tsegment(x, y, seg_b);
        display_draw_tsegment(x, y, seg_c);
        display_draw_tsegment(x, y, seg_d);
        display_draw_tsegment(x, y, seg_e);
        display_draw_tsegment(x, y, seg_f);
        display_draw_tsegment(x, y, seg_g);
        break;
    case 9:
        display_draw_tsegment(x, y, seg_a);
        display_draw_tsegment(x, y, seg_b);
        display_draw_tsegment(x, y, seg_c);
        display_draw_tsegment(x, y, seg_d);
        display_draw_tsegment(x, y, seg_f);
        display_draw_tsegment(x, y, seg_g);
        break;
    default:
        break;
    }
}

void display_draw_tone_graph(uint32_t tone_graph)
{
    /*
     * The tone graph is passed as a 17-bit value with the following layout:
     *
     *  1 | 1  1  1  1  1  1       |
     *  6 | 5  4  3  2  1  0  9  8 | 7  6  5  4  3  2  1  0
     * [<]|[ ][ ][ ][ ][ ][ ][ ][ ]|[ ][ ][ ][ ][ ][ ][ ][>]
     *  - | 7  6  5  4  3  2  1  0 | 1  2  3  4  5  6  7  +
     */

    if (tone_graph & DISPLAY_TONE_UNDER) {
        u8g2_DrawPixel(&u8g2, 1, 2);
        u8g2_DrawLine(&u8g2, 2, 1, 2, 3);
        u8g2_DrawBox(&u8g2, 3, 0, 4, 5);
    }

    uint32_t mask = 0x08000;
    uint16_t x_offset = 9;
    do {
        if (tone_graph & mask) {
            u8g2_DrawBox(&u8g2, x_offset, 0, 14, 5);
        }
        x_offset += 16;
        mask = mask >> 1;
    } while (mask != 0x00001);

    if (tone_graph & DISPLAY_TONE_OVER) {
        u8g2_DrawBox(&u8g2, 249, 0, 4, 5);
        u8g2_DrawLine(&u8g2, 253, 1, 253, 3);
        u8g2_DrawPixel(&u8g2, 254, 2);
    }
}

void display_draw_contrast_grade(display_grade_t grade)
{
    bool show_half = false;
    u8g2_uint_t x = 9;
    u8g2_uint_t y = 8;

    switch (grade) {
    case DISPLAY_GRADE_00:
        display_draw_digit(x, y, 0);
        display_draw_digit(x + 36, y, 0);
        break;
    case DISPLAY_GRADE_0:
        display_draw_digit(x, y, 0);
        break;
    case DISPLAY_GRADE_0_HALF:
        x -= 40;
        show_half = true;
        break;
    case DISPLAY_GRADE_1:
        display_draw_digit(x, y, 1);
        break;
    case DISPLAY_GRADE_1_HALF:
        display_draw_digit(x, y, 1);
        show_half = true;
        break;
    case DISPLAY_GRADE_2:
        display_draw_digit(x, y, 2);
        break;
    case DISPLAY_GRADE_2_HALF:
        display_draw_digit(x, y, 2);
        show_half = true;
        break;
    case DISPLAY_GRADE_3:
        display_draw_digit(x, y, 3);
        break;
    case DISPLAY_GRADE_3_HALF:
        display_draw_digit(x, y, 3);
        show_half = true;
        break;
    case DISPLAY_GRADE_4:
        display_draw_digit(x, y, 4);
        break;
    case DISPLAY_GRADE_4_HALF:
        display_draw_digit(x, y, 4);
        show_half = true;
        break;
    case DISPLAY_GRADE_5:
        display_draw_digit(x, y, 5);
        break;
    case DISPLAY_GRADE_NONE:
    default:
        return;
    }

    if (show_half) {
        display_draw_tdigit(x + 43, y, 1);
        u8g2_DrawLine(&u8g2, x + 46, y + 26, x + 64, y + 26);
        u8g2_DrawLine(&u8g2, x + 45, y + 27, x + 65, y + 27);
        u8g2_DrawLine(&u8g2, x + 46, y + 28, x + 64, y + 28);
        display_draw_tdigit(x + 49, y + 31, 2);
    }
}

void display_draw_counter_time(uint16_t seconds, uint16_t milliseconds, uint8_t fraction_digits)
{
    if (milliseconds >= 1000) {
        seconds += milliseconds / 1000;
        milliseconds = milliseconds % 1000;
    }

    if (fraction_digits > 2) {
        fraction_digits = 2;
    }

    u8g2_uint_t x = 217;
    u8g2_uint_t y = 8;

    if (fraction_digits == 0) {
        // Time format: SSS
        if (seconds > 999) {
            seconds = 999;
        }

        display_draw_digit(x, y, seconds % 10);
        x -= 36;

        if (seconds >= 10) {
            display_draw_digit(x, y, seconds % 100 / 10);
            x -= 36;
        }

        if (seconds >= 100) {
            display_draw_digit(x, y, (seconds % 1000) / 100);
        }
    } else if (fraction_digits == 1) {
        // Time format: SS.m
        if (seconds > 99) {
            seconds = 99;
        }

        display_draw_digit(x, y, (milliseconds % 1000) / 100);
        x -= 12;

        u8g2_DrawBox(&u8g2, x, y + 50, 6, 6);
        x -= 36;

        display_draw_digit(x, y, seconds % 10);
        x -= 36;

        if (seconds >= 10) {
            display_draw_digit(x, y, seconds % 100 / 10);
        }
    } else if (fraction_digits == 2) {
        // Time format: S.mm
        if (seconds > 9) {
            seconds = 9;
        }

        display_draw_digit(x, y, milliseconds % 100 / 10);
        x -= 36;

        display_draw_digit(x, y, (milliseconds % 1000) / 100);
        x -= 12;

        u8g2_DrawBox(&u8g2, x, y + 50, 6, 6);
        x -= 36;

        display_draw_digit(x, y, seconds % 10);
    }
}

void display_draw_counter_time_small(u8g2_uint_t x2, u8g2_uint_t y1, const display_exposure_timer_t *time_elements)
{
    uint16_t seconds = time_elements->time_seconds;
    uint16_t milliseconds = time_elements->time_milliseconds;
    uint8_t fraction_digits = time_elements->fraction_digits;

    if (milliseconds >= 1000) {
        seconds += milliseconds / 1000;
        milliseconds = milliseconds % 1000;
    }

    if (fraction_digits > 2) {
        fraction_digits = 2;
    }

    u8g2_uint_t x = x2 - 14;
    u8g2_uint_t y = y1;

    if (fraction_digits == 0) {
        // Time format: SSS
        if (seconds > 999) {
            seconds = 999;
        }

        display_draw_tdigit(x, y, seconds % 10);
        x -= 17;

        if (seconds >= 10) {
            display_draw_tdigit(x, y, seconds % 100 / 10);
            x -= 17;
        }

        if (seconds >= 100) {
            display_draw_tdigit(x, y, (seconds % 1000) / 100);
        }
    } else if (fraction_digits == 1) {
        // Time format: SS.m
        if (seconds > 99) {
            seconds = 99;
        }

        display_draw_tdigit(x, y, (milliseconds % 1000) / 100);
        x -= 6;

        u8g2_DrawBox(&u8g2, x, y + 22, 3, 3);
        x -= 17;

        display_draw_tdigit(x, y, seconds % 10);
        x -= 17;

        if (seconds >= 10) {
            display_draw_tdigit(x, y, seconds % 100 / 10);
        }
    } else if (fraction_digits == 2) {
        // Time format: S.mm
        if (seconds > 9) {
            seconds = 9;
        }

        display_draw_tdigit(x, y, milliseconds % 100 / 10);
        x -= 17;

        display_draw_tdigit(x, y, (milliseconds % 1000) / 100);
        x -= 6;

        u8g2_DrawBox(&u8g2, x, y + 22, 3, 3);
        x -= 17;

        display_draw_tdigit(x, y, seconds % 10);
    }
}

void display_draw_main_elements(const display_main_elements_t *elements)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetBitmapMode(&u8g2, 1);

    display_draw_tone_graph(elements->tone_graph);
    display_draw_contrast_grade(elements->contrast_grade);
    display_draw_counter_time(elements->time_seconds,
        elements->time_milliseconds,
        elements->fraction_digits);

    u8g2_SendBuffer(&u8g2);

    osMutexRelease(display_mutex);
}

void display_draw_stop_increment(uint8_t increment_den)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetFont(&u8g2, u8g2_font_logisoso34_tf);
    u8g2_SetFontMode(&u8g2, 0);

    u8g2_uint_t x = 50;
    u8g2_uint_t y = 8;

    // Clamp the denominator at 99, which is a value beyond
    // what we ever expect to actually display.
    if (increment_den > 99) {
        increment_den = 99;
    }

    if (increment_den >= 20) {
        // 1/DD stops (1/20 - 1/99)
        display_draw_tdigit(x + 7, y, 1);
        u8g2_DrawLine(&u8g2, x + 2, y + 26, x + 37, y + 26);
        u8g2_DrawLine(&u8g2, x + 1, y + 27, x + 38, y + 27);
        u8g2_DrawLine(&u8g2, x + 2, y + 28, x + 37, y + 28);
        display_draw_tdigit(x + 5, y + 31, increment_den % 100 / 10);
        display_draw_tdigit(x + 22, y + 31, increment_den % 10);
    } else if (increment_den >= 10) {
        // 1/DD stops (1/10 - 1/19)
        display_draw_tdigit(x + 7, y, 1);
        u8g2_DrawLine(&u8g2, x + 8, y + 26, x + 32, y + 26);
        u8g2_DrawLine(&u8g2, x + 7, y + 27, x + 33, y + 27);
        u8g2_DrawLine(&u8g2, x + 8, y + 28, x + 32, y + 28);
        display_draw_tdigit(x + 0, y + 31, increment_den % 100 / 10);
        display_draw_tdigit(x + 17, y + 31, increment_den % 10);
    } else if (increment_den > 1) {
        // 1/D stops
        display_draw_tdigit(x + 7, y, 1);
        u8g2_DrawLine(&u8g2, x + 10, y + 26, x + 28, y + 26);
        u8g2_DrawLine(&u8g2, x + 9, y + 27, x + 29, y + 27);
        u8g2_DrawLine(&u8g2, x + 10, y + 28, x + 28, y + 28);
        display_draw_tdigit(x + 13, y + 31, increment_den);
    } else {
        // 1 stop
        display_draw_digit(x, y, 1);
    }

    u8g2_SetFontDirection(&u8g2, 0);
    u8g2_SetFontPosBaseline(&u8g2);
    u8g2_DrawUTF8(&u8g2, x + 56, y + 46, "Stop");

    u8g2_SendBuffer(&u8g2);

    osMutexRelease(display_mutex);
}

void display_draw_exposure_adj(int value)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetBitmapMode(&u8g2, 1);

    asset_info_t asset;
    display_asset_get(&asset, ASSET_EXPOSURE_ADJ_ICON_48);
    u8g2_DrawXBM(&u8g2, 10, 12, asset.width, asset.height, asset.bits);

    u8g2_uint_t x = 217;
    u8g2_uint_t y = 8;
    int disp_val = abs(value);

    display_draw_digit(x, y, disp_val % 10);
    x -= 36;

    if (disp_val >= 10) {
        display_draw_digit(x, y, disp_val % 100 / 10);
        x -= 36;
    }

    if (disp_val >= 100) {
        display_draw_digit(x, y, (disp_val % 1000) / 100);
        x -= 36;
    }

    if (value != 0) {
        display_draw_sign(x, y, value > 0);
    }

    u8g2_SendBuffer(&u8g2);

    osMutexRelease(display_mutex);
}

void display_draw_timer_adj(const display_exposure_timer_t *elements)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetBitmapMode(&u8g2, 1);

    asset_info_t asset;
    display_asset_get(&asset, ASSET_TIMER_ADJ_ICON_48);
    u8g2_DrawXBM(&u8g2, 10, 12, asset.width, asset.height, asset.bits);

    display_draw_counter_time(elements->time_seconds,
        elements->time_milliseconds,
        elements->fraction_digits);

    u8g2_SendBuffer(&u8g2);

    osMutexRelease(display_mutex);
}

void display_draw_exposure_timer(const display_exposure_timer_t *elements, const display_exposure_timer_t *prev_elements)
{
    bool clean_display = true;
    bool time_changed = true;
    bool send_buffer = false;

    if (prev_elements) {
        clean_display = false;
        if (elements->fraction_digits == prev_elements->fraction_digits) {
            if (elements->time_seconds == prev_elements->time_seconds
                && elements->time_milliseconds == prev_elements->time_milliseconds) {
                time_changed = false;
            }
        }
    }

    osMutexAcquire(display_mutex, portMAX_DELAY);

    if (clean_display) {
        u8g2_SetDrawColor(&u8g2, 0);
        u8g2_ClearBuffer(&u8g2);
        u8g2_SetDrawColor(&u8g2, 1);
        u8g2_SetBitmapMode(&u8g2, 1);

        asset_info_t asset;
        display_asset_get(&asset, ASSET_TIMER_ICON_48);
        u8g2_DrawXBM(&u8g2, 10, 12, asset.width, asset.height, asset.bits);
        send_buffer = true;
    }

    if (time_changed) {
        if (!clean_display) {
            u8g2_SetDrawColor(&u8g2, 0);
            u8g2_DrawBox(&u8g2, 96, 8,
                u8g2_GetDisplayWidth(&u8g2) - 96,
                u8g2_GetDisplayHeight(&u8g2) - 8);
            u8g2_SetDrawColor(&u8g2, 1);
        }
        display_draw_counter_time(elements->time_seconds,
            elements->time_milliseconds,
            elements->fraction_digits);
        send_buffer = true;
    }

    if (send_buffer) {
        if (clean_display) {
            u8g2_SendBuffer(&u8g2);
        } else {
            u8g2_UpdateDisplayArea(&u8g2, 12, 1, 20, 7);
        }
    }

    osMutexRelease(display_mutex);
}

void display_draw_test_strip_elements(const display_test_strip_elements_t *elements)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_pressstart2p_8f);
    u8g2_SetFontMode(&u8g2, 0);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetBitmapMode(&u8g2, 1);
    u8g2_SetFontDirection(&u8g2, 0);
    u8g2_SetFontPosBaseline(&u8g2);

    // Draw test strip grid
    u8g2_uint_t x;
    u8g2_uint_t y = 13;
    if (elements->patches == DISPLAY_PATCHES_5) {
        x = 30;
        u8g2_DrawFrame(&u8g2, 0, 11, 152, 53);
        u8g2_DrawFrame(&u8g2, 1, 12, 150, 51);
        for (int i = 0; i < 4; i++) {
            u8g2_DrawLine(&u8g2, x + 0, y, x + 0, y + 48);
            u8g2_DrawLine(&u8g2, x + 1, y, x + 1, y + 48);
            x += 30;
        }
    } else {
        x = 22;
        u8g2_DrawFrame(&u8g2, 0, 11, 156, 53);
        u8g2_DrawFrame(&u8g2, 1, 12, 154, 51);
        for (int i = 0; i < 6; i++) {
            u8g2_DrawLine(&u8g2, x + 0, y, x + 0, y + 48);
            u8g2_DrawLine(&u8g2, x + 1, y, x + 1, y + 48);
            x += 22;
        }
    }

    // Draw each test strip patch
    x = 2;
    y = 13;
    if (elements->patches == DISPLAY_PATCHES_5) {
        for (int i = 0; i < 5; i++) {
            if (elements->covered_patches & (1 << (4 - i))) {
                u8g2_DrawBox(&u8g2, x + 1, y + 1, 26, 47);
                u8g2_SetDrawColor(&u8g2, 0);
            }

            switch (i) {
            case 0:
                u8g2_DrawStr(&u8g2, x + 5, y + 29, "-2");
                break;
            case 1:
                u8g2_DrawStr(&u8g2, x + 5, y + 29, "-1");
                break;
            case 2:
                u8g2_DrawStr(&u8g2, x + 11, y + 29, "0");
                break;
            case 3:
                u8g2_DrawStr(&u8g2, x + 5, y + 29, "+1");
                break;
            case 4:
                u8g2_DrawStr(&u8g2, x + 5, y + 29, "+2");
                break;
            default:
                break;
            }

            u8g2_SetDrawColor(&u8g2, 1);
            x += 30;
        }
    } else {
        for (int i = 0; i < 7; i++) {
            if (elements->covered_patches & (1 << (6 - i))) {
                u8g2_DrawBox(&u8g2, x + 1, y + 1, 18, 47);
                u8g2_SetDrawColor(&u8g2, 0);
            }

            switch (i) {
            case 0:
                u8g2_DrawStr(&u8g2, x + 1, y + 29, "-3");
                break;
            case 1:
                u8g2_DrawStr(&u8g2, x + 1, y + 29, "-2");
                break;
            case 2:
                u8g2_DrawStr(&u8g2, x + 1, y + 29, "-1");
                break;
            case 3:
                u8g2_DrawStr(&u8g2, x + 7, y + 29, "0");
                break;
            case 4:
                u8g2_DrawStr(&u8g2, x + 1, y + 29, "+1");
                break;
            case 5:
                u8g2_DrawStr(&u8g2, x + 1, y + 29, "+2");
                break;
            case 6:
                u8g2_DrawStr(&u8g2, x + 1, y + 29, "+3");
                break;
            default:
                break;
            }

            u8g2_SetDrawColor(&u8g2, 1);
            x += 22;
        }
    }

    // Draw title text
    x = 166;
    y = 11 + u8g2_GetAscent(&u8g2);;
    u8g2_uint_t line_height = u8g2_GetAscent(&u8g2) - u8g2_GetDescent(&u8g2) + 1;
    if (elements->title1) {
        u8g2_DrawUTF8Line(&u8g2, x, y, u8g2_GetDisplayWidth(&u8g2) - x, elements->title1, 0, 0);
        y += line_height + 1;
    }
    if (elements->title2) {
        u8g2_DrawUTF8Line(&u8g2, x, y, u8g2_GetDisplayWidth(&u8g2) - x, elements->title2, 0, 0);
    }

    // Draw timer icon
    x = 165;
    y = 35;
    asset_info_t asset;
    display_asset_get(&asset, ASSET_TIMER_ICON_24);
    u8g2_DrawXBM(&u8g2, x, y, asset.width, asset.height, asset.bits);

    // Draw timer
    x = 251;
    y = 34;
    display_draw_counter_time_small(x, y, &(elements->time_elements));

    u8g2_SendBuffer(&u8g2);

    osMutexRelease(display_mutex);
}

void display_draw_test_strip_timer(const display_exposure_timer_t *elements)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawBox(&u8g2, 192, 8,
        u8g2_GetDisplayWidth(&u8g2) - 192,
        u8g2_GetDisplayHeight(&u8g2) - 8);
    u8g2_SetDrawColor(&u8g2, 1);

    u8g2_uint_t x = 251;
    u8g2_uint_t y = 34;
    display_draw_counter_time_small(x, y, elements);

    u8g2_UpdateDisplayArea(&u8g2, 24, 4, 8, 4);

    osMutexRelease(display_mutex);
}

uint8_t display_selection_list(const char *title, uint8_t start_pos, const char *list)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    display_prepare_menu_font();
    keypad_clear_events();
    uint8_t option = u8g2_UserInterfaceSelectionList(&u8g2, title, start_pos, list);

    osMutexRelease(display_mutex);

    return menu_event_timeout ? UINT8_MAX : option;
}

void display_static_list(const char *title, const char *list)
{
    // Based off u8g2_UserInterfaceSelectionList() with changes to use
    // full frame buffer mode and to remove actual menu functionality.

    osMutexAcquire(display_mutex, portMAX_DELAY);

    display_prepare_menu_font();

    u8g2_ClearBuffer(&u8g2);

    u8sl_t u8sl;
    u8g2_uint_t yy;

    u8g2_uint_t line_height = u8g2_GetAscent(&u8g2) - u8g2_GetDescent(&u8g2) + 1;

    uint8_t title_lines = u8x8_GetStringLineCnt(title);
    uint8_t display_lines;

    if (title_lines > 0) {
        display_lines = (u8g2_GetDisplayHeight(&u8g2) - 3) / line_height;
        u8sl.visible = display_lines;
        u8sl.visible -= title_lines;
    }
    else {
        display_lines = u8g2_GetDisplayHeight(&u8g2) / line_height;
        u8sl.visible = display_lines;
    }

    u8sl.total = u8x8_GetStringLineCnt(list);
    u8sl.first_pos = 0;
    u8sl.current_pos = -1;

    u8g2_SetFontPosBaseline(&u8g2);

    yy = u8g2_GetAscent(&u8g2);
    if (title_lines > 0) {
        yy += u8g2_DrawUTF8Lines(&u8g2, 0, yy, u8g2_GetDisplayWidth(&u8g2), line_height, title);
        u8g2_DrawHLine(&u8g2, 0, yy - line_height - u8g2_GetDescent(&u8g2) + 1, u8g2_GetDisplayWidth(&u8g2));
        yy += 3;
    }
    u8g2_DrawSelectionList(&u8g2, &u8sl, yy, list);

    u8g2_SendBuffer(&u8g2);

    osMutexRelease(display_mutex);
}

uint8_t display_message(const char *title1, const char *title2, const char *title3, const char *buttons)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    display_prepare_menu_font();
    keypad_clear_events();
    uint8_t option = u8g2_UserInterfaceMessage(&u8g2, title1, title2, title3, buttons);

    osMutexRelease(display_mutex);

    return menu_event_timeout ? UINT8_MAX : option;
}

uint8_t display_input_value(const char *title, const char *prefix, uint8_t *value,
        uint8_t low, uint8_t high, uint8_t digits, const char *postfix)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    display_prepare_menu_font();
    keypad_clear_events();
    uint8_t option = u8g2_UserInterfaceInputValue(&u8g2, title, prefix, value, low, high, digits, postfix);

    osMutexRelease(display_mutex);

    return menu_event_timeout ? UINT8_MAX : option;
}

uint8_t display_input_value_cb(const char *title, const char *prefix, uint8_t *value,
    uint8_t low, uint8_t high, uint8_t digits, const char *postfix,
    display_input_value_callback_t callback, void *user_data)
{
    // Based off u8g2_UserInterfaceInputValue() with changes to
    // invoke a callback on value change.

    osMutexAcquire(display_mutex, portMAX_DELAY);

    display_prepare_menu_font();
    keypad_clear_events();

    uint8_t line_height;
    uint8_t height;
    u8g2_uint_t pixel_height;
    u8g2_uint_t  y, yy;
    u8g2_uint_t  pixel_width;
    u8g2_uint_t  x, xx;

    uint8_t local_value = *value;
    //uint8_t r; /* not used ??? */
    uint8_t event;

    /* only horizontal strings are supported, so force this here */
    u8g2_SetFontDirection(&u8g2, 0);

    /* force baseline position */
    u8g2_SetFontPosBaseline(&u8g2);

    /* calculate line height */
    line_height = u8g2_GetAscent(&u8g2);
    line_height -= u8g2_GetDescent(&u8g2);


    /* calculate overall height of the input value box */
    height = 1;   /* value input line */
    height += u8x8_GetStringLineCnt(title);

    /* calculate the height in pixel */
    pixel_height = height;
    pixel_height *= line_height;


    /* calculate offset from top */
    y = 0;
    if (pixel_height < u8g2_GetDisplayHeight(&u8g2)) {
        y = u8g2_GetDisplayHeight(&u8g2);
        y -= pixel_height;
        y /= 2;
    }

    /* calculate offset from left for the label */
    x = 0;
    pixel_width = u8g2_GetUTF8Width(&u8g2, prefix);
    pixel_width += u8g2_GetUTF8Width(&u8g2, "0") * digits;
    pixel_width += u8g2_GetUTF8Width(&u8g2, postfix);
    if (pixel_width < u8g2_GetDisplayWidth(&u8g2)) {
        x = u8g2_GetDisplayWidth(&u8g2);
        x -= pixel_width;
        x /= 2;
    }

    /* event loop */
    for(;;) {
        u8g2_FirstPage(&u8g2);
        do {
            /* render */
            yy = y;
            yy += u8g2_DrawUTF8Lines(&u8g2, 0, yy, u8g2_GetDisplayWidth(&u8g2), line_height, title);
            xx = x;
            xx += u8g2_DrawUTF8(&u8g2, xx, yy, prefix);
            xx += u8g2_DrawUTF8(&u8g2, xx, yy, u8x8_u8toa(local_value, digits));
            u8g2_DrawUTF8(&u8g2, xx, yy, postfix);
        } while(u8g2_NextPage(&u8g2));

        for(;;)
        {
            event = u8x8_GetMenuEvent(u8g2_GetU8x8(&u8g2));
            if (event == U8X8_MSG_GPIO_MENU_SELECT) {
                *value = local_value;
                osMutexRelease(display_mutex);
                return 1;
            }
            else if (event == U8X8_MSG_GPIO_MENU_HOME) {
                return menu_event_timeout ? UINT8_MAX : 0;
            }
            else if (event == U8X8_MSG_GPIO_MENU_NEXT || event == U8X8_MSG_GPIO_MENU_UP) {
                if (local_value >= high) {
                    local_value = low;
                } else {
                    local_value++;
                }
                if (callback) {
                    callback(local_value, user_data);
                }
                break;
            }
            else if (event == U8X8_MSG_GPIO_MENU_PREV || event == U8X8_MSG_GPIO_MENU_DOWN) {
                if (local_value <= low) {
                    local_value = high;
                } else {
                    local_value--;
                }
                if (callback) {
                    callback(local_value, user_data);
                }
                break;
            }
        }
    }

    /* never reached */
    osMutexRelease(display_mutex);
    //return r;
}

uint8_t u8x8_GetMenuEvent(u8x8_t *u8x8)
{
    // This function should override a u8g2 framework function with the
    // same name, due to its declaration with the "weak" pragma.
    menu_event_timeout = false;

    // If we were called via a function that is holding the display mutex,
    // then release that mutex while blocked on the keypad queue.
    osStatus_t mutex_released = osMutexRelease(display_mutex);

    keypad_event_t event;
    HAL_StatusTypeDef ret = keypad_wait_for_event(&event, MENU_TIMEOUT_MS);

    if (mutex_released == osOK) {
        osMutexAcquire(display_mutex, portMAX_DELAY);
    }

    if (ret == HAL_OK) {
        if (event.pressed) {
            switch (event.key) {
            case KEYPAD_DEC_CONTRAST:
                return U8X8_MSG_GPIO_MENU_PREV;
            case KEYPAD_INC_CONTRAST:
                return U8X8_MSG_GPIO_MENU_NEXT;
            case KEYPAD_INC_EXPOSURE:
                return U8X8_MSG_GPIO_MENU_UP;
            case KEYPAD_DEC_EXPOSURE:
                return U8X8_MSG_GPIO_MENU_DOWN;
            case KEYPAD_MENU:
                return U8X8_MSG_GPIO_MENU_SELECT;
            case KEYPAD_CANCEL:
                return U8X8_MSG_GPIO_MENU_HOME;
            default:
                break;
            }
        }
    } else if (ret == HAL_TIMEOUT) {
        menu_event_timeout = true;
        return U8X8_MSG_GPIO_MENU_HOME;
    }
    return 0;
}

void display_prepare_menu_font()
{
    u8g2_SetFont(&u8g2, u8g2_font_pressstart2p_8f);
    u8g2_SetFontMode(&u8g2, 0);
    u8g2_SetDrawColor(&u8g2, 1);
}
