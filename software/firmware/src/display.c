#include "display.h"

#include <FreeRTOS.h>
#include <cmsis_os.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ff.h>

#define LOG_TAG "display"
#include <elog.h>

#include "u8g2_stm32_hal.h"
#include "u8g2.h"
#include "display_assets.h"
#include "display_segments.h"
#include "display_internal.h"
#include "keypad.h"

static u8g2_t u8g2;

osMutexId_t display_mutex;
static const osMutexAttr_t display_mutex_attributes = {
  .name = "display_mutex",
};

static uint8_t display_contrast = 0x9F;
static uint8_t display_brightness = 0x0F;

static void display_set_freq(uint8_t value);

static void display_draw_tone_graph(uint32_t tone_graph, uint32_t overlay_marks);
static void display_draw_split_tone_graph(uint32_t base_tone_graph, uint32_t adj_tone_graph, uint32_t overlay_marks);
static void display_draw_paper_profile_num(uint8_t num);
static void display_draw_burn_dodge_count(uint8_t count);
static void display_draw_bw_elements(const display_main_printing_bw_t *bw_elements);
static void display_draw_color_elements(const display_main_printing_color_t *color_elements);
static void display_draw_time_icon(display_main_printing_time_icon_t time_icon, bool highlight);
static void display_draw_calibration_value(u8g2_uint_t x, u8g2_uint_t y, const char *title1, const char *title2, uint16_t value);
static void display_draw_contrast_grade(u8g2_uint_t x, u8g2_uint_t y, contrast_grade_t grade);
static void display_draw_contrast_grade_medium(u8g2_uint_t x, u8g2_uint_t y, contrast_grade_t grade);
static void display_draw_density_prefix();
static void display_draw_density_placeholder();
static void display_draw_counter_time(uint16_t seconds, uint16_t milliseconds, uint8_t fraction_digits);
static void display_draw_counter_time_small(u8g2_uint_t x2, u8g2_uint_t y1, const display_exposure_timer_t *time_elements);
static void display_prepare_menu_font();

HAL_StatusTypeDef display_init(const u8g2_display_handle_t *display_handle)
{
    log_d("display_init");

    // Initialize the STM32 display HAL, which includes library display
    // parameter and callback initialization.
    u8g2_stm32_hal_init(&u8g2, display_handle);

    u8g2_InitDisplay(&u8g2);

    // Slightly increase the display refresh frequency
    display_set_freq(0xC1);

    display_mutex = osMutexNew(&display_mutex_attributes);
    if (!display_mutex) {
        log_e("xSemaphoreCreateMutex error");
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

static FIL *screenshot_fp = NULL;
static void display_save_screenshot_callback(const char *s)
{
    if (screenshot_fp) {
        FRESULT res = f_puts(s, screenshot_fp);
        if (res < 0) {
            log_e("Error writing screenshot data to file: %d", res);
            screenshot_fp = NULL;
        }
    }
}

void display_save_screenshot()
{
    osMutexAcquire(display_mutex, portMAX_DELAY);
    static uint16_t image_index = 1;

    FRESULT res;
    FIL fp;
    bool file_open = false;
    char filename[32];

    do {
        memset(&fp, 0, sizeof(FIL));
        sprintf(filename, "img-%04d.xbm", image_index % 10000);

        res = f_open(&fp, filename, FA_WRITE | FA_CREATE_ALWAYS);
        if (res != FR_OK) {
            log_e("Error opening screenshot file: %d", res);
            break;
        }
        file_open = true;

        screenshot_fp = &fp;
        u8g2_WriteBufferXBM(&u8g2, display_save_screenshot_callback);
        screenshot_fp = NULL;

        log_d("Screenshot written to file: %s", filename);
        image_index++;
    } while (0);

    if (file_open) {
        f_close(&fp);
    }

    osMutexRelease(display_mutex);
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

void display_redraw_tone_graph(uint32_t tone_graph, uint32_t overlay_marks)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    /* Clear the tone graph area */
    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawBox(&u8g2, 0, 0, u8g2_GetDisplayWidth(&u8g2), 5);
    u8g2_SetDrawColor(&u8g2, 1);

    /* Redraw the tone graph */
    display_draw_tone_graph(tone_graph, overlay_marks);

    /* Update just the modified display area */
    u8g2_UpdateDisplayArea(&u8g2, 0, 0, u8g2_GetDisplayWidth(&u8g2) / 8, 1);

    osMutexRelease(display_mutex);
}

void display_draw_tone_graph(uint32_t tone_graph, uint32_t overlay_marks)
{
    /*
     * The tone graph is passed as a 17-bit value with the following layout:
     *
     *  1 | 1  1  1  1  1  1       |
     *  6 | 5  4  3  2  1  0  9  8 | 7  6  5  4  3  2  1  0
     * [<]|[ ][ ][ ][ ][ ][ ][ ][ ]|[ ][ ][ ][ ][ ][ ][ ][>]
     *  + |                        |                      -
     *
     *  It is rendered in the opposite direction, with the under tones
     *  on the left side of the display.
     */

    if (tone_graph & 0x00000001UL) {
        if (overlay_marks & 0x00000001UL) {
            u8g2_DrawLine(&u8g2, 2, 0, 6, 0);
            u8g2_DrawLine(&u8g2, 1, 1, 2, 1);
            u8g2_DrawLine(&u8g2, 0, 2, 1, 2);
            u8g2_DrawLine(&u8g2, 1, 3, 2, 3);
            u8g2_DrawLine(&u8g2, 2, 4, 6, 4);
            u8g2_DrawLine(&u8g2, 6, 1, 6, 3);
        } else {
            u8g2_DrawPixel(&u8g2, 0, 2);
            u8g2_DrawLine(&u8g2, 1, 1, 1, 3);
            u8g2_DrawBox(&u8g2, 2, 0, 5, 5);
        }
    } else if (overlay_marks & 0x00000001UL) {
        u8g2_DrawPixel(&u8g2, 2, 2);
        u8g2_DrawBox(&u8g2, 3, 1, 3, 3);
    }

    uint32_t mask = 0x00000002UL;
    uint16_t x_offset = 9;
    do {
        if (tone_graph & mask) {
            if (overlay_marks & mask) {
                u8g2_DrawFrame(&u8g2, x_offset, 0, 14, 5);
                u8g2_DrawLine(&u8g2, x_offset + 1, 1, x_offset + 1, 3);
                u8g2_DrawLine(&u8g2, x_offset + 2, 1, x_offset + 2, 3);
                u8g2_DrawLine(&u8g2, x_offset + 12, 1, x_offset + 12, 3);
                u8g2_DrawLine(&u8g2, x_offset + 11, 1, x_offset + 11, 3);
                u8g2_DrawPixel(&u8g2, x_offset + 3, 1);
                u8g2_DrawPixel(&u8g2, x_offset + 3, 3);
                u8g2_DrawPixel(&u8g2, x_offset + 10, 1);
                u8g2_DrawPixel(&u8g2, x_offset + 10, 3);
            } else {
                u8g2_DrawBox(&u8g2, x_offset, 0, 14, 5);
            }
        } else if (overlay_marks & mask) {
            u8g2_DrawLine(&u8g2, x_offset + 4, 1, x_offset + 9, 1);
            u8g2_DrawLine(&u8g2, x_offset + 3, 2, x_offset + 10, 2);
            u8g2_DrawLine(&u8g2, x_offset + 4, 3, x_offset + 9, 3);
        }
        x_offset += 16;
        mask = mask << 1UL;
    } while (mask != 0x00010000UL);

    if (tone_graph & 0x00010000UL) {
        if (overlay_marks & 0x00010000UL) {
            u8g2_DrawLine(&u8g2, 249, 0, 253, 0);

            u8g2_DrawLine(&u8g2, 253, 1, 254, 1);
            u8g2_DrawLine(&u8g2, 254, 2, 255, 2);
            u8g2_DrawLine(&u8g2, 253, 3, 254, 3);

            u8g2_DrawLine(&u8g2, 249, 4, 253, 4);
            u8g2_DrawLine(&u8g2, 249, 1, 249, 3);

        } else {
            u8g2_DrawBox(&u8g2, 249, 0, 5, 5);
            u8g2_DrawLine(&u8g2, 254, 1, 254, 3);
            u8g2_DrawPixel(&u8g2, 255, 2);
        }
    } else if (overlay_marks & 0x00010000UL) {
        u8g2_DrawPixel(&u8g2, 253, 2);
        u8g2_DrawBox(&u8g2, 250, 1, 3, 3);
    }
}

void display_draw_split_tone_graph(uint32_t base_tone_graph, uint32_t adj_tone_graph, uint32_t overlay_marks)
{
    /*
     * The tone graph is passed as a 17-bit value as described in the
     * comments under `display_draw_tone_graph`.
     */

    if (base_tone_graph & 0x00000001UL) {
        u8g2_DrawLine(&u8g2, 2, 0, 6, 0);
        u8g2_DrawLine(&u8g2, 1, 1, 6, 1);
        u8g2_DrawLine(&u8g2, 0, 2, 6, 2);
    }

    if (adj_tone_graph & 0x00000001UL) {
        if (overlay_marks & 0x00000001UL) {
            u8g2_DrawLine(&u8g2, 0, 4, 6, 4);
            u8g2_DrawPixel(&u8g2, 1, 5);
            u8g2_DrawPixel(&u8g2, 2, 5);
            u8g2_DrawPixel(&u8g2, 6, 5);
            u8g2_DrawLine(&u8g2, 2, 6, 6, 6);
        } else {
            u8g2_DrawLine(&u8g2, 0, 4, 6, 4);
            u8g2_DrawLine(&u8g2, 1, 5, 6, 5);
            u8g2_DrawLine(&u8g2, 2, 6, 6, 6);
        }
    } else if (overlay_marks & 0x00000001UL) {
        u8g2_DrawLine(&u8g2, 3, 5, 5, 5);
    }

    uint32_t mask = 0x00000002UL;
    uint16_t x_offset = 9;
    do {
        if (base_tone_graph & mask) {
            u8g2_DrawBox(&u8g2, x_offset, 0, 14, 3);
        }

        if (adj_tone_graph & mask) {
            if (overlay_marks & mask) {
                u8g2_DrawFrame(&u8g2, x_offset, 4, 14, 3);
                u8g2_DrawPixel(&u8g2, x_offset + 1, 5);
                u8g2_DrawPixel(&u8g2, x_offset + 12, 5);
            } else {
                u8g2_DrawBox(&u8g2, x_offset, 4, 14, 3);
            }
        } else if (overlay_marks & mask) {
             u8g2_DrawLine(&u8g2, x_offset + 2, 5, x_offset + 11, 5);
        }
        x_offset += 16;
        mask = mask << 1UL;
    } while (mask != 0x00010000UL);

    if (base_tone_graph & 0x00010000UL) {
        u8g2_DrawLine(&u8g2, 249, 0, 253, 0);
        u8g2_DrawLine(&u8g2, 249, 1, 254, 1);
        u8g2_DrawLine(&u8g2, 249, 2, 255, 2);
    }

    if (adj_tone_graph & 0x00010000UL) {
        if (overlay_marks & 0x00010000UL) {
            u8g2_DrawLine(&u8g2, 249, 4, 255, 4);
            u8g2_DrawPixel(&u8g2, 254, 5);
            u8g2_DrawPixel(&u8g2, 253, 5);
            u8g2_DrawPixel(&u8g2, 249, 5);
            u8g2_DrawLine(&u8g2, 249, 6, 253, 6);

        } else {
            u8g2_DrawLine(&u8g2, 249, 4, 255, 4);
            u8g2_DrawLine(&u8g2, 249, 5, 254, 5);
            u8g2_DrawLine(&u8g2, 249, 6, 253, 6);
        }
    } else if (overlay_marks & 0x00010000UL) {
        u8g2_DrawLine(&u8g2, 250, 5, 252, 5);
    }
}

void display_draw_paper_profile_num(uint8_t num)
{
    if (num < 1 || num > 16) { return; }

    asset_info_t asset;
    switch (num) {
    case 1:
        display_asset_get(&asset, ASSET_PAPER_INDEX_1_18);
        break;
    case 2:
        display_asset_get(&asset, ASSET_PAPER_INDEX_2_18);
        break;
    case 3:
        display_asset_get(&asset, ASSET_PAPER_INDEX_3_18);
        break;
    case 4:
        display_asset_get(&asset, ASSET_PAPER_INDEX_4_18);
        break;
    case 5:
        display_asset_get(&asset, ASSET_PAPER_INDEX_5_18);
        break;
    case 6:
        display_asset_get(&asset, ASSET_PAPER_INDEX_6_18);
        break;
    case 7:
        display_asset_get(&asset, ASSET_PAPER_INDEX_7_18);
        break;
    case 8:
        display_asset_get(&asset, ASSET_PAPER_INDEX_8_18);
        break;
    case 9:
        display_asset_get(&asset, ASSET_PAPER_INDEX_9_18);
        break;
    case 10:
        display_asset_get(&asset, ASSET_PAPER_INDEX_10_18);
        break;
    case 11:
        display_asset_get(&asset, ASSET_PAPER_INDEX_11_18);
        break;
    case 12:
        display_asset_get(&asset, ASSET_PAPER_INDEX_12_18);
        break;
    case 13:
        display_asset_get(&asset, ASSET_PAPER_INDEX_13_18);
        break;
    case 14:
        display_asset_get(&asset, ASSET_PAPER_INDEX_14_18);
        break;
    case 15:
        display_asset_get(&asset, ASSET_PAPER_INDEX_15_18);
        break;
    case 16:
        display_asset_get(&asset, ASSET_PAPER_INDEX_16_18);
        break;
    default:
        return;
    }

    u8g2_DrawXBM(&u8g2, 0, u8g2_GetDisplayHeight(&u8g2) - asset.height, asset.width, asset.height, asset.bits);
}

void display_draw_burn_dodge_count(uint8_t count)
{
    if (count < 1) { return; }

    u8g2_uint_t x = 0;
    u8g2_uint_t y = 8;

    asset_info_t asset;
    switch (count) {
    case 0:
        break;
    case 1:
        display_asset_get(&asset, ASSET_ADJUST_COUNT_1_18);
        break;
    case 2:
        display_asset_get(&asset, ASSET_ADJUST_COUNT_2_18);
        break;
    case 3:
        display_asset_get(&asset, ASSET_ADJUST_COUNT_3_18);
        break;
    case 4:
        display_asset_get(&asset, ASSET_ADJUST_COUNT_4_18);
        break;
    case 5:
        display_asset_get(&asset, ASSET_ADJUST_COUNT_5_18);
        break;
    case 6:
        display_asset_get(&asset, ASSET_ADJUST_COUNT_6_18);
        break;
    case 7:
        display_asset_get(&asset, ASSET_ADJUST_COUNT_7_18);
        break;
    case 8:
        display_asset_get(&asset, ASSET_ADJUST_COUNT_8_18);
        break;
    case 9:
        display_asset_get(&asset, ASSET_ADJUST_COUNT_9_18);
        break;
    default:
        display_asset_get(&asset, ASSET_ADJUST_COUNT_9P_18);
        break;
    }

    u8g2_DrawXBM(&u8g2, x, y, asset.width, asset.height, asset.bits);
}

static void display_draw_bw_elements(const display_main_printing_bw_t *bw_elements)
{
    if (bw_elements->contrast_note) {
        u8g2_SetFont(&u8g2, u8g2_font_logisoso16_tr);
        u8g2_SetFontMode(&u8g2, 0);
        u8g2_SetFontDirection(&u8g2, 0);
        u8g2_SetFontPosBaseline(&u8g2);
        display_draw_contrast_grade_medium(9 + 24, 8, bw_elements->contrast_grade);
        u8g2_DrawUTF8(&u8g2, 9 + 24, 64, bw_elements->contrast_note);
    } else {
        display_draw_contrast_grade(9 + 24, 8, bw_elements->contrast_grade);
    }
}

void display_draw_color_elements(const display_main_printing_color_t *color_elements)
{
    u8g2_uint_t x = 9 + 24;
    u8g2_uint_t y = 8;

    u8g2_SetFont(&u8g2, u8g2_font_pressstart2p_8f);
    u8g2_SetFontMode(&u8g2, 0);

    for (size_t i = 0; i < 3; i++) {
        const uint16_t ch_value = color_elements->ch_values[i];
        const bool highlight = (color_elements->ch_highlight == i + 1 || color_elements->ch_highlight > 3);

        if (highlight) {
            u8g2_SetDrawColor(&u8g2, 1);
            u8g2_DrawBox(&u8g2, x, y + 3, 13, 13);

            u8g2_SetDrawColor(&u8g2, 0);
            u8g2_DrawPixel(&u8g2, x, y + 3);
            u8g2_DrawPixel(&u8g2, x + 12, y + 3);
            u8g2_DrawPixel(&u8g2, x, y + 15);
            u8g2_DrawPixel(&u8g2, x + 12, y + 15);
        } else {
            u8g2_SetDrawColor(&u8g2, 1);
        }

        u8g2_DrawGlyph(&u8g2, x + 3, y + 14, color_elements->ch_labels[i]);

        if (highlight) {
            u8g2_SetDrawColor(&u8g2, 1);
        }

        if (color_elements->ch_wide) {
            if (ch_value >= 10000) {
                display_draw_vtdigit(&u8g2, x + 16, y + 1, ch_value / 10000);
            }
            if (ch_value >= 1000) {
                display_draw_vtdigit(&u8g2, x + 26, y + 1, (ch_value % 10000) / 1000);
            }
            if (ch_value >= 100) {
                display_draw_vtdigit(&u8g2, x + 36, y + 1, (ch_value % 1000) / 100);
            }
            if (ch_value >= 10) {
                display_draw_vtdigit(&u8g2, x + 46, y + 1, (ch_value % 100) / 10);
            }
            display_draw_vtdigit(&u8g2, x + 56, y + 1, ch_value % 10);
        } else {
            if (ch_value >= 100) {
                display_draw_vtdigit(&u8g2, x + 20, y + 1, (ch_value % 1000) / 100);
            }
            if (ch_value >= 10) {
                display_draw_vtdigit(&u8g2, x + 30, y + 1, (ch_value % 100) / 10);
            }
            display_draw_vtdigit(&u8g2, x + 40, y + 1, ch_value % 10);
        }

        y += 19;
    }
}

void display_draw_time_icon(display_main_printing_time_icon_t time_icon, bool highlight)
{
    u8g2_uint_t x = 106;
    u8g2_uint_t y = 10;

    if (highlight) {
        u8g2_SetDrawColor(&u8g2, 1);
        u8g2_DrawBox(&u8g2, x, y, 24, 24);

        u8g2_SetDrawColor(&u8g2, 0);
        u8g2_DrawPixel(&u8g2, x, y);
        u8g2_DrawPixel(&u8g2, x, y + 1);
        u8g2_DrawPixel(&u8g2, x + 1, y);

        u8g2_DrawPixel(&u8g2, x + 23, y);
        u8g2_DrawPixel(&u8g2, x + 22, y);
        u8g2_DrawPixel(&u8g2, x + 23, y + 1);

        u8g2_DrawPixel(&u8g2, x, y + 23);
        u8g2_DrawPixel(&u8g2, x, y + 22);
        u8g2_DrawPixel(&u8g2, x + 1, y + 23);

        u8g2_DrawPixel(&u8g2, x + 23, y + 23);
        u8g2_DrawPixel(&u8g2, x + 23, y + 22);
        u8g2_DrawPixel(&u8g2, x + 22, y + 23);
    }

    if (time_icon == DISPLAY_MAIN_PRINTING_TIME_ICON_INVALID) {
        asset_info_t asset;
        display_asset_get(&asset, ASSET_TIMER_OFF_ICON_24);
        u8g2_DrawXBM(&u8g2, x, y, asset.width, asset.height, asset.bits);
    } else if (time_icon == DISPLAY_MAIN_PRINTING_TIME_ICON_NORMAL) {
        asset_info_t asset;
        display_asset_get(&asset, ASSET_TIMER_ICON_24);
        u8g2_DrawXBM(&u8g2, x, y, asset.width, asset.height, asset.bits);
    }

    if (highlight) {
        u8g2_SetDrawColor(&u8g2, 1);
    }
}

void display_draw_calibration_value(u8g2_uint_t x, u8g2_uint_t y, const char *title1, const char *title2, uint16_t value)
{
    u8g2_SetFont(&u8g2, u8g2_font_pressstart2p_8f);
    u8g2_SetFontMode(&u8g2, 0);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetFontDirection(&u8g2, 0);
    u8g2_SetFontPosBaseline(&u8g2);

    y += u8g2_GetAscent(&u8g2);
    u8g2_uint_t line_height = u8g2_GetAscent(&u8g2) - u8g2_GetDescent(&u8g2) /*+ 1*/;
    if (title1) {
        u8g2_DrawUTF8(&u8g2, x, y, title1);
        y += line_height + 1;
    }
    if (title2) {
        u8g2_DrawUTF8(&u8g2, x, y, title2);
    }

    y = u8g2_GetDisplayHeight(&u8g2) - 37;

    if (value > 999) {
        value = 999;
    }

    if (value >= 100) {
        display_draw_mdigit(&u8g2, x, y, (value % 1000) / 100);
    }
    x += 22;

    if (value >= 10) {
        display_draw_mdigit(&u8g2, x, y, value % 100 / 10);
    }
    x += 22;

    display_draw_mdigit(&u8g2, x, y, value % 10);
}

void display_draw_contrast_grade(u8g2_uint_t x, u8g2_uint_t y, contrast_grade_t grade)
{
    bool show_half = false;

    switch (grade) {
    case CONTRAST_GRADE_00:
        display_draw_digit(&u8g2, x, y, 0);
        display_draw_digit(&u8g2, x + 36, y, 0);
        break;
    case CONTRAST_GRADE_0:
        display_draw_digit(&u8g2, x, y, 0);
        break;
    case CONTRAST_GRADE_0_HALF:
        x -= 40;
        show_half = true;
        break;
    case CONTRAST_GRADE_1:
        display_draw_digit(&u8g2, x, y, 1);
        break;
    case CONTRAST_GRADE_1_HALF:
        display_draw_digit(&u8g2, x, y, 1);
        show_half = true;
        break;
    case CONTRAST_GRADE_2:
        display_draw_digit(&u8g2, x, y, 2);
        break;
    case CONTRAST_GRADE_2_HALF:
        display_draw_digit(&u8g2, x, y, 2);
        show_half = true;
        break;
    case CONTRAST_GRADE_3:
        display_draw_digit(&u8g2, x, y, 3);
        break;
    case CONTRAST_GRADE_3_HALF:
        display_draw_digit(&u8g2, x, y, 3);
        show_half = true;
        break;
    case CONTRAST_GRADE_4:
        display_draw_digit(&u8g2, x, y, 4);
        break;
    case CONTRAST_GRADE_4_HALF:
        display_draw_digit(&u8g2, x, y, 4);
        show_half = true;
        break;
    case CONTRAST_GRADE_5:
        display_draw_digit(&u8g2, x, y, 5);
        break;
    case CONTRAST_GRADE_MAX:
    default:
        return;
    }

    if (show_half) {
        display_draw_tdigit(&u8g2, x + 43, y, 1);
        u8g2_DrawLine(&u8g2, x + 46, y + 26, x + 64, y + 26);
        u8g2_DrawLine(&u8g2, x + 45, y + 27, x + 65, y + 27);
        u8g2_DrawLine(&u8g2, x + 46, y + 28, x + 64, y + 28);
        display_draw_tdigit(&u8g2, x + 49, y + 31, 2);
    }
}

void display_draw_contrast_grade_medium(u8g2_uint_t x, u8g2_uint_t y, contrast_grade_t grade)
{
    bool show_half = false;

    switch (grade) {
    case CONTRAST_GRADE_00:
        display_draw_mdigit(&u8g2, x, y, 0);
        display_draw_mdigit(&u8g2, x + 22, y, 0);
        break;
    case CONTRAST_GRADE_0:
        display_draw_mdigit(&u8g2, x, y, 0);
        break;
    case CONTRAST_GRADE_0_HALF:
        x -= 21;
        show_half = true;
        break;
    case CONTRAST_GRADE_1:
        display_draw_mdigit(&u8g2, x, y, 1);
        break;
    case CONTRAST_GRADE_1_HALF:
        display_draw_mdigit(&u8g2, x, y, 1);
        show_half = true;
        break;
    case CONTRAST_GRADE_2:
        display_draw_mdigit(&u8g2, x, y, 2);
        break;
    case CONTRAST_GRADE_2_HALF:
        display_draw_mdigit(&u8g2, x, y, 2);
        show_half = true;
        break;
    case CONTRAST_GRADE_3:
        display_draw_mdigit(&u8g2, x, y, 3);
        break;
    case CONTRAST_GRADE_3_HALF:
        display_draw_mdigit(&u8g2, x, y, 3);
        show_half = true;
        break;
    case CONTRAST_GRADE_4:
        display_draw_mdigit(&u8g2, x, y, 4);
        break;
    case CONTRAST_GRADE_4_HALF:
        display_draw_mdigit(&u8g2, x, y, 4);
        show_half = true;
        break;
    case CONTRAST_GRADE_5:
        display_draw_mdigit(&u8g2, x, y, 5);
        break;
    case CONTRAST_GRADE_MAX:
    default:
        return;
    }

    if (show_half) {
        display_draw_vtdigit(&u8g2, x + 22, y - 1, 1);
        u8g2_DrawLine(&u8g2, x + 24, y + 16, x + 34, y + 16);
        u8g2_DrawLine(&u8g2, x + 23, y + 17, x + 35, y + 17);
        u8g2_DrawLine(&u8g2, x + 24, y + 18, x + 34, y + 18);
        display_draw_vtdigit(&u8g2, x + 25, y + 20, 2);
    }
}

void display_draw_density_prefix()
{
    u8g2_uint_t x = 101;
    u8g2_uint_t y = 8;

    // "="
    u8g2_DrawBox(&u8g2, x, y + 18, 20, 5);
    u8g2_DrawBox(&u8g2, x, y + 34, 20, 5);

    // "D"
    x -= 42;
    display_draw_digit_letter_d(&u8g2, x, y);
}

void display_draw_density_placeholder()
{
    u8g2_uint_t x = 217;
    u8g2_uint_t y = 8;

    display_draw_digit_sign(&u8g2, x, y, false);
    x -= 36;

    display_draw_digit_sign(&u8g2, x, y, false);
    x -= 12;

    u8g2_DrawBox(&u8g2, x, y + 50, 6, 6);
    x -= 36;

    display_draw_digit_sign(&u8g2, x, y, false);
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

        display_draw_digit(&u8g2, x, y, seconds % 10);
        x -= 36;

        if (seconds >= 10) {
            display_draw_digit(&u8g2, x, y, seconds % 100 / 10);
            x -= 36;
        }

        if (seconds >= 100) {
            display_draw_digit(&u8g2, x, y, (seconds % 1000) / 100);
        }
    } else if (fraction_digits == 1) {
        // Time format: SS.m
        if (seconds > 99) {
            seconds = 99;
        }

        display_draw_digit(&u8g2, x, y, (milliseconds % 1000) / 100);
        x -= 12;

        u8g2_DrawBox(&u8g2, x, y + 50, 6, 6);
        x -= 36;

        display_draw_digit(&u8g2, x, y, seconds % 10);
        x -= 36;

        if (seconds >= 10) {
            display_draw_digit(&u8g2, x, y, seconds % 100 / 10);
        }
    } else if (fraction_digits == 2) {
        // Time format: S.mm
        if (seconds > 9) {
            seconds = 9;
        }

        display_draw_digit(&u8g2, x, y, milliseconds % 100 / 10);
        x -= 36;

        display_draw_digit(&u8g2, x, y, (milliseconds % 1000) / 100);
        x -= 12;

        u8g2_DrawBox(&u8g2, x, y + 50, 6, 6);
        x -= 36;

        display_draw_digit(&u8g2, x, y, seconds % 10);
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

        display_draw_tdigit(&u8g2, x, y, seconds % 10);
        x -= 17;

        if (seconds >= 10) {
            display_draw_tdigit(&u8g2, x, y, seconds % 100 / 10);
            x -= 17;
        }

        if (seconds >= 100) {
            display_draw_tdigit(&u8g2, x, y, (seconds % 1000) / 100);
        }
    } else if (fraction_digits == 1) {
        // Time format: SS.m
        if (seconds > 99) {
            seconds = 99;
        }

        display_draw_tdigit(&u8g2, x, y, (milliseconds % 1000) / 100);
        x -= 6;

        u8g2_DrawBox(&u8g2, x, y + 22, 3, 3);
        x -= 17;

        display_draw_tdigit(&u8g2, x, y, seconds % 10);
        x -= 17;

        if (seconds >= 10) {
            display_draw_tdigit(&u8g2, x, y, seconds % 100 / 10);
        }
    } else if (fraction_digits == 2) {
        // Time format: S.mm
        if (seconds > 9) {
            seconds = 9;
        }

        display_draw_tdigit(&u8g2, x, y, milliseconds % 100 / 10);
        x -= 17;

        display_draw_tdigit(&u8g2, x, y, (milliseconds % 1000) / 100);
        x -= 6;

        u8g2_DrawBox(&u8g2, x, y + 22, 3, 3);
        x -= 17;

        display_draw_tdigit(&u8g2, x, y, seconds % 10);
    }
}

void display_draw_main_elements_printing(const display_main_printing_elements_t *elements)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetBitmapMode(&u8g2, 1);

    display_draw_tone_graph(elements->tone_graph, elements->tone_graph_overlay);
    display_draw_paper_profile_num(elements->paper_profile_num);
    display_draw_burn_dodge_count(elements->burn_dodge_count);

    if (elements->printing_type == DISPLAY_MAIN_PRINTING_BW) {
        display_draw_bw_elements(&elements->bw);
    } else if (elements->printing_type == DISPLAY_MAIN_PRINTING_COLOR) {
        display_draw_color_elements(&elements->color);
    }

    display_draw_counter_time(elements->time_elements.time_seconds,
        elements->time_elements.time_milliseconds,
        elements->time_elements.fraction_digits);

    display_draw_time_icon(elements->time_icon, elements->time_icon_highlight);

    u8g2_SendBuffer(&u8g2);

    osMutexRelease(display_mutex);
}

void display_draw_main_elements_densitometer(const display_main_densitometer_elements_t *elements)
{
    asset_info_t asset;
    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetBitmapMode(&u8g2, 1);

    display_draw_density_prefix();

    if (elements->density_whole == UINT16_MAX
        && elements->density_fractional == UINT16_MAX
        && elements->fraction_digits == UINT8_MAX) {
        display_draw_density_placeholder();
    } else {
        display_draw_counter_time(elements->density_whole,
            elements->density_fractional,
            elements->fraction_digits);
    }

    /* Draw probe element */
    if (elements->show_ind_probe > 0) {
        u8g2_SetDrawColor(&u8g2, 1);
        if (elements->show_ind_probe > 1) {
            u8g2_DrawRBox(&u8g2, 8, 8, 32, 26, 3);
            u8g2_SetDrawColor(&u8g2, 0);
        }
        display_asset_get(&asset, ASSET_DENS_IND_PROBE);
        u8g2_DrawXBM(&u8g2, 10, 10, asset.width, asset.height, asset.bits);
    }

    /* Draw densitometer element */
    if (elements->show_ind_dens > 0) {
        u8g2_SetDrawColor(&u8g2, 1);
        if (elements->show_ind_dens > 1) {
            u8g2_DrawRBox(&u8g2, 8, 38, 32, 26, 3);
            u8g2_SetDrawColor(&u8g2, 0);
        }
        display_asset_get(&asset, (elements->ind_dens == 0) ? ASSET_DENS_IND_REFL : ASSET_DENS_IND_TRAN);
        u8g2_DrawXBM(&u8g2, 13, 40, asset.width, asset.height, asset.bits);
    }

    u8g2_SendBuffer(&u8g2);

    osMutexRelease(display_mutex);
}

void display_draw_main_elements_calibration(const display_main_calibration_elements_t *elements)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetBitmapMode(&u8g2, 1);

    display_draw_calibration_value(9 + 24, 8, elements->cal_title1, elements->cal_title2, elements->cal_value);

    display_draw_counter_time(elements->time_elements.time_seconds,
        elements->time_elements.time_milliseconds,
        elements->time_elements.fraction_digits);

    if (elements->time_too_short) {
        asset_info_t asset;
        display_asset_get(&asset, ASSET_TIMER_OFF_ICON_24);
        u8g2_DrawXBM(&u8g2, 106, 10, asset.width, asset.height, asset.bits);
    }

    u8g2_SendBuffer(&u8g2);

    osMutexRelease(display_mutex);
}

void display_draw_stop_increment(uint8_t increment_den)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetFont(&u8g2, u8g2_font_logisoso32_tf);
    u8g2_SetFontMode(&u8g2, 0);

    u8g2_uint_t x = 50;
    u8g2_uint_t y = 8;

    // Clamp the denominator at 99, which is a value beyond
    // what we ever expect to actually display.
    if (increment_den > 99) {
        increment_den = 99;
    }

    if (increment_den == 1) {
        display_draw_digit(&u8g2, x, y, 1);
    } else {
        display_draw_tdigit_fraction(&u8g2, x, y, 1, increment_den);
    }

    u8g2_SetFontDirection(&u8g2, 0);
    u8g2_SetFontPosBaseline(&u8g2);
    u8g2_DrawUTF8(&u8g2, x + 56, y + 46, "Stop");

    u8g2_SendBuffer(&u8g2);

    osMutexRelease(display_mutex);
}

void display_draw_mode_text(const char *text)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetFont(&u8g2, u8g2_font_logisoso32_tf);
    u8g2_SetFontMode(&u8g2, 0);
    u8g2_SetFontDirection(&u8g2, 0);
    u8g2_SetFontPosBaseline(&u8g2);

    if (text) {
        u8g2_DrawUTF8Line(&u8g2, 0, 50,
            u8g2_GetDisplayWidth(&u8g2), text, 0, 0);
    }

    u8g2_SendBuffer(&u8g2);

    osMutexRelease(display_mutex);
}

void display_draw_exposure_adj(int value, uint32_t tone_graph)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetBitmapMode(&u8g2, 1);

    display_draw_tone_graph(tone_graph, 0);

    asset_info_t asset;
    display_asset_get(&asset, ASSET_EXPOSURE_ADJ_ICON_48);
    u8g2_DrawXBM(&u8g2, 10, 12, asset.width, asset.height, asset.bits);

    u8g2_uint_t x = 217;
    u8g2_uint_t y = 8;
    int disp_val = abs(value);

    display_draw_digit(&u8g2, x, y, disp_val % 10);
    x -= 36;

    if (disp_val >= 10) {
        display_draw_digit(&u8g2, x, y, disp_val % 100 / 10);
        x -= 36;
    }

    if (disp_val >= 100) {
        display_draw_digit(&u8g2, x, y, (disp_val % 1000) / 100);
        x -= 36;
    }

    if (value != 0) {
        display_draw_digit_sign(&u8g2, x, y, value > 0);
    }

    u8g2_SendBuffer(&u8g2);

    osMutexRelease(display_mutex);
}

void display_draw_timer_adj(const display_exposure_timer_t *elements, uint32_t tone_graph)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetBitmapMode(&u8g2, 1);

    display_draw_tone_graph(tone_graph, 0);

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

void display_draw_adjustment_exposure_elements(const display_adjustment_exposure_elements_t *elements)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_pressstart2p_8f);
    u8g2_SetFontMode(&u8g2, 0);
    u8g2_SetFontDirection(&u8g2, 0);
    u8g2_SetFontPosBaseline(&u8g2);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetBitmapMode(&u8g2, 1);

    // Draw adjustment title text
    if (elements->title) {
        u8g2_uint_t y_offset = elements->contrast_note ? 4 : 11;
        u8g2_DrawUTF8(&u8g2,
            0, y_offset + u8g2_GetAscent(&u8g2),
            elements->title);
    }

    // Draw adjustment index icon
    asset_info_t asset = {0};
    switch (elements->burn_dodge_index) {
    case 0:
        break;
    case 1:
        display_asset_get(&asset, ASSET_ADJUST_COUNT_1_32);
        break;
    case 2:
        display_asset_get(&asset, ASSET_ADJUST_COUNT_2_32);
        break;
    case 3:
        display_asset_get(&asset, ASSET_ADJUST_COUNT_3_32);
        break;
    case 4:
        display_asset_get(&asset, ASSET_ADJUST_COUNT_4_32);
        break;
    case 5:
        display_asset_get(&asset, ASSET_ADJUST_COUNT_5_32);
        break;
    case 6:
        display_asset_get(&asset, ASSET_ADJUST_COUNT_6_32);
        break;
    case 7:
        display_asset_get(&asset, ASSET_ADJUST_COUNT_7_32);
        break;
    case 8:
        display_asset_get(&asset, ASSET_ADJUST_COUNT_8_32);
        break;
    case 9:
        display_asset_get(&asset, ASSET_ADJUST_COUNT_9_32);
        break;
    default:
        display_asset_get(&asset, ASSET_ADJUST_COUNT_9P_32);
        break;
    }
    if (asset.bits) {
        u8g2_DrawXBM(&u8g2, 0, 28, asset.width, asset.height, asset.bits);
    }

    if (elements->contrast_grade < CONTRAST_GRADE_MAX) {
        // Draw contrast grade
        if (elements->contrast_note) {
            display_draw_contrast_grade_medium(50, 15, elements->contrast_grade);
            u8g2_DrawUTF8(&u8g2, 50, 65, elements->contrast_note);
        } else {
            display_draw_contrast_grade_medium(50, 26, elements->contrast_grade);
        }
    } else {
        // Draw timer clock icon
        display_asset_get(&asset, ASSET_TIMER_ICON_32);
        u8g2_DrawXBM(&u8g2, 56, 28, asset.width, asset.height, asset.bits);
    }

    // Draw exposure time
    display_draw_counter_time(elements->time_elements.time_seconds,
        elements->time_elements.time_milliseconds,
        elements->time_elements.fraction_digits);

    if (elements->time_too_short) {
        display_asset_get(&asset, ASSET_TIMER_OFF_ICON_24);
        u8g2_DrawXBM(&u8g2, 106, 10, asset.width, asset.height, asset.bits);
    }

    u8g2_SendBuffer(&u8g2);

    osMutexRelease(display_mutex);
}

void display_redraw_adjustment_exposure_timer(const display_exposure_timer_t *time_elements)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawBox(&u8g2, 96, 8,
        u8g2_GetDisplayWidth(&u8g2) - 96,
        u8g2_GetDisplayHeight(&u8g2) - 8);
    u8g2_SetDrawColor(&u8g2, 1);

    display_draw_counter_time(time_elements->time_seconds,
        time_elements->time_milliseconds,
        time_elements->fraction_digits);

    u8g2_UpdateDisplayArea(&u8g2, 12, 1, 20, 7);

    osMutexRelease(display_mutex);
}

void display_draw_test_strip_elements(const display_test_strip_elements_t *elements)
{
    asset_info_t asset;

    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_pressstart2p_8f);
    u8g2_SetFontMode(&u8g2, 0);
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetBitmapMode(&u8g2, 1);
    u8g2_SetFontDirection(&u8g2, 0);
    u8g2_SetFontPosBaseline(&u8g2);

    /* Draw test strip grid */
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

    /* Draw each test strip patch */
    if (elements->patches == DISPLAY_PATCHES_5) {
        x = 2;
        for (int i = 0; i < 5; i++) {
            y = 13;
            if (elements->covered_patches & (1 << (4 - i))) {
                u8g2_DrawBox(&u8g2, x + 1, y + 1, 26, 47);
                u8g2_SetDrawColor(&u8g2, 0);
            }

            if (elements->patch_cal_values[i] > 0) {
                if (elements->invalid_patches & (1 << (6 - i))) {
                    display_asset_get(&asset, ASSET_TIMER_OFF_ICON_18);
                    u8g2_DrawXBM(&u8g2, x + 5, y + 5, asset.width, asset.height, asset.bits);
                } else {
                    u8g2_SetFontDirection(&u8g2, 3);
                    u8g2_DrawUTF8(&u8g2, x + 19, y + 28, display_u16toa(elements->patch_cal_values[i], 3));
                    u8g2_SetFontDirection(&u8g2, 0);
                }
                y += 19;
            } else {
                if (elements->invalid_patches & (1 << (6 - i))) {
                    display_asset_get(&asset, ASSET_TIMER_OFF_ICON_18);
                    u8g2_DrawXBM(&u8g2, x + 5, y + 30, asset.width, asset.height, asset.bits);
                }
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
        x = 2;
        for (int i = 0; i < 7; i++) {
            y = 13;
            if (elements->covered_patches & (1 << (6 - i))) {
                u8g2_DrawBox(&u8g2, x + 1, y + 1, 18, 47);
                u8g2_SetDrawColor(&u8g2, 0);
            }

            if (elements->patch_cal_values[i] > 0) {
                if (elements->invalid_patches & (1 << (6 - i))) {
                    display_asset_get(&asset, ASSET_TIMER_OFF_ICON_18);
                    u8g2_DrawXBM(&u8g2, x + 1, y + 5, asset.width, asset.height, asset.bits);
                } else {
                    u8g2_SetFontDirection(&u8g2, 3);
                    u8g2_DrawUTF8(&u8g2, x + 15, y + 28, display_u16toa(elements->patch_cal_values[i], 3));
                    u8g2_SetFontDirection(&u8g2, 0);
                }
                y += 19;
            } else {
                if (elements->invalid_patches & (1 << (6 - i))) {
                    display_asset_get(&asset, ASSET_TIMER_OFF_ICON_18);
                    u8g2_DrawXBM(&u8g2, x + 1, y + 30, asset.width, asset.height, asset.bits);
                }
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

    /* Draw title text */
    x = 166;
    y = 11 + u8g2_GetAscent(&u8g2);
    u8g2_uint_t line_height = u8g2_GetAscent(&u8g2) - u8g2_GetDescent(&u8g2) + 1;
    if (elements->title1) {
        u8g2_DrawUTF8Line(&u8g2, x, y, u8g2_GetDisplayWidth(&u8g2) - x, elements->title1, 0, 0);
        y += line_height + 1;
    }
    if (elements->title2) {
        u8g2_DrawUTF8Line(&u8g2, x, y, u8g2_GetDisplayWidth(&u8g2) - x, elements->title2, 0, 0);
    }

    /* Draw timer icon */
    x = 165;
    y = 35;
    display_asset_get(&asset, ASSET_TIMER_ICON_24);
    u8g2_DrawXBM(&u8g2, x, y, asset.width, asset.height, asset.bits);

    /* Draw timer */
    x = 251;
    y = 34;
    display_draw_counter_time_small(x, y, &(elements->time_elements));

    u8g2_SendBuffer(&u8g2);

    osMutexRelease(display_mutex);
}

void display_redraw_test_strip_timer(const display_exposure_timer_t *elements)
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

void display_draw_edit_adjustment_elements(const display_edit_adjustment_elements_t *elements)
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

    asset_info_t asset;
    u8g2_uint_t line_height = u8g2_GetAscent(&u8g2) - u8g2_GetDescent(&u8g2) + 1;
    u8g2_uint_t x = 0;
    u8g2_uint_t y = 11;

    // Draw the tone graph
    display_draw_split_tone_graph(elements->base_tone_graph, elements->adj_tone_graph, 0);

    // Draw adjustment icon
    if (elements->time_too_short) {
        display_asset_get(&asset, ASSET_TIMER_OFF_ICON_24);
        u8g2_DrawXBM(&u8g2, x, y - 1, asset.width, asset.height, asset.bits);

    } else {
        display_asset_get(&asset, ASSET_ADJUST_ICON_24);
        u8g2_DrawXBM(&u8g2, x, y, asset.width, asset.height, asset.bits);
    }

    y += 28;

    // Draw base exposure icon
    display_asset_get(&asset, ASSET_PHOTO_ICON_24);
    u8g2_DrawXBM(&u8g2, x, y, asset.width, asset.height, asset.bits);

    x += 30;

    // Draw adjustment text
    y = 15 + u8g2_GetAscent(&u8g2);
    if (elements->adj_title1) {
        u8g2_DrawUTF8(&u8g2, x, y, elements->adj_title1);
        y += line_height + 1;
    }
    if (elements->adj_title2) {
        u8g2_DrawUTF8(&u8g2, x, y, elements->adj_title2);
    }

    // Draw base exposure text
    y = 15 + 28 + u8g2_GetAscent(&u8g2);
    if (elements->base_title1) {
        u8g2_DrawUTF8(&u8g2, x, y, elements->base_title1);
        y += line_height + 1;
    }
    if (elements->base_title2) {
        u8g2_DrawUTF8(&u8g2, x, y, elements->base_title2);
    }

    if (elements->tip_up || elements->tip_down) {
        // Draw the up/down tips
        x = 170;
        if (elements->tip_up) {
            y = 14;
            display_asset_get(&asset, ASSET_ARROW_UP_18);
            u8g2_DrawXBM(&u8g2, x, y, asset.width, asset.height, asset.bits);
            u8g2_DrawFrame(&u8g2, x - 3, y - 3, 24, 24);
            u8g2_DrawUTF8(&u8g2, x + 28, y + 13, elements->tip_up);
        }
        if (elements->tip_down) {
            y = 42;
            display_asset_get(&asset, ASSET_ARROW_DOWN_18);
            u8g2_DrawXBM(&u8g2, x, y, asset.width, asset.height, asset.bits);
            u8g2_DrawFrame(&u8g2, x - 3, y - 3, 24, 24);
            u8g2_DrawUTF8(&u8g2, x + 28, y + 13, elements->tip_down);
        }
    } else {
        // Draw the adjustment value
        bool num_positive = elements->adj_num >= 0;
        uint8_t adj_num = abs(elements->adj_num);
        uint8_t adj_whole = adj_num / elements->adj_den;
        adj_num -= adj_whole * elements->adj_den;

        x = 217;
        y = 8;

        // Draw fraction part
        if (adj_num > 0) {
            if (adj_whole == 0) {
                x -= 36;
            }
            display_draw_tdigit_fraction(&u8g2, x, y, adj_num, elements->adj_den);
        }
        x -= 36;

        // Draw whole number part
        if (adj_whole != 0 || (adj_whole == 0 && adj_num == 0)) {
            display_draw_digit(&u8g2, x, y, adj_whole);
            x -= 36;
        }

        // Draw +/- sign part
        display_draw_digit_sign(&u8g2, x, y, num_positive);
    }

    u8g2_SendBuffer(&u8g2);

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

uint16_t display_selection_list_params(const char *title, uint8_t start_pos, const char *list,
    display_menu_params_t params)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    display_prepare_menu_font();
    keypad_clear_events();

    uint16_t option = display_UserInterfaceSelectionListCB(&u8g2, title, start_pos, list,
        display_GetMenuEvent, params);

    osMutexRelease(display_mutex);

    return option;
}

uint8_t display_message_graph(const char *title, const char *list, const char *buttons, const uint8_t *graph_points, size_t graph_size)
{
    int8_t result = -1;

    osMutexAcquire(display_mutex, portMAX_DELAY);

    u8g2_uint_t list_width = u8g2_GetDisplayWidth(&u8g2) / 2;

    /* Clamp the max size of the graph */
    if (graph_size > 126) {
        graph_size = 126;
    }

    display_prepare_menu_font();

    u8g2_ClearBuffer(&u8g2);

    /* Draw static list */
    display_UserInterfaceStaticListDraw(&u8g2, title, list, list_width);

    /* Draw graph decorations */
    u8g2_DrawLine(&u8g2, 129, 11, 129, 62);
    u8g2_DrawLine(&u8g2, 129, 62, 255, 62);
    for (u8g2_uint_t x = 134; x < 256; x += 5) {
        u8g2_DrawPixel(&u8g2, x, 63);
    }
    for (u8g2_uint_t y = 57; y >= 11; y -= 5) {
        u8g2_DrawPixel(&u8g2, 128, y);
    }

    if (graph_points) {
        /* Draw graph points */
        for (u8g2_uint_t i = 0; i < graph_size - 1; i++) {
            uint8_t pt1 = graph_points[i];
            if (pt1 > 50) {
                pt1 = 50;
            }
            uint8_t pt2 = graph_points[i + 1];
            if (pt2 > 50) {
                pt2 = 50;
            }
            u8g2_DrawLine(&u8g2, 130 + i, 61 - pt1, 130 + i + 1, 61 - pt2);
        }
    }

    /* Draw buttons */
    u8g2_uint_t yy = u8g2_GetDisplayHeight(&u8g2) - 2;
    uint8_t cursor = 0;

    for(;;) {
        uint8_t button_cnt = display_DrawButtonLine(&u8g2, yy, list_width, cursor, buttons);
        u8g2_SendBuffer(&u8g2);

        for(;;) {
            uint16_t event;
            uint16_t event_result = display_GetMenuEvent(u8g2_GetU8x8(&u8g2), DISPLAY_MENU_ACCEPT_MENU);

            if (event_result == UINT16_MAX) {
                result = UINT8_MAX;
                break;
            } else {
                event = (uint8_t)(event_result & 0x00FF);
            }

            if (event == U8X8_MSG_GPIO_MENU_SELECT) {
                result = cursor+1;
                break;
            } else if (event == U8X8_MSG_GPIO_MENU_HOME) {
                result = 0;
                break;
            } else if (event == U8X8_MSG_GPIO_MENU_NEXT || event == U8X8_MSG_GPIO_MENU_DOWN) {
                cursor++;
                if (cursor >= button_cnt) {
                    cursor = 0;
                }
                break;
            } else if (event == U8X8_MSG_GPIO_MENU_PREV || event == U8X8_MSG_GPIO_MENU_UP) {
                if (cursor == 0) {
                    cursor = button_cnt;
                }
                cursor--;
                break;
            }
        }
        if (result >= 0) {
            break;
        }
    }

    osMutexRelease(display_mutex);

    return (uint8_t)result;
}

void display_static_list(const char *title, const char *list)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    display_prepare_menu_font();

    display_UserInterfaceStaticList(&u8g2, title, list);

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

uint8_t display_message_params(const char *title1, const char *title2, const char *title3, const char *buttons,
    display_menu_params_t params)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    display_prepare_menu_font();
    keypad_clear_events();
    uint8_t option = display_UserInterfaceMessageCB(&u8g2, title1, title2, title3, buttons,
        display_GetMenuEvent, params);

    osMutexRelease(display_mutex);

    return menu_event_timeout ? UINT8_MAX : option;
}

uint8_t display_input_value(const char *title, const char *msg, const char *prefix, uint8_t *value,
        uint8_t low, uint8_t high, uint8_t digits, const char *postfix)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    display_prepare_menu_font();
    keypad_clear_events();
    uint8_t option = display_UserInterfaceInputValue(&u8g2, title, msg, prefix, value, low, high, digits, postfix);

    osMutexRelease(display_mutex);

    return menu_event_timeout ? UINT8_MAX : option;
}

uint8_t display_input_value_u16(const char *title, const char *msg, const char *prefix, uint16_t *value,
        uint16_t low, uint16_t high, uint8_t digits, const char *postfix)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    display_prepare_menu_font();
    keypad_clear_events();
    uint8_t option = display_UserInterfaceInputValueU16(&u8g2, title, msg, prefix, value, low, high, digits, postfix);

    osMutexRelease(display_mutex);

    return menu_event_timeout ? UINT8_MAX : option;
}

uint8_t display_input_value_f1_2(const char *title, const char *prefix, uint16_t *value,
    uint16_t low, uint16_t high, char sep, const char *postfix)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    display_prepare_menu_font();
    keypad_clear_events();
    uint8_t option = display_UserInterfaceInputValueF1_2(&u8g2, title, prefix, value, low, high, sep, postfix);

    osMutexRelease(display_mutex);

    return menu_event_timeout ? UINT8_MAX : option;
}

uint8_t display_input_value_f16(const char *title, const char *msg, const char *prefix, uint16_t *value,
        uint16_t low, uint16_t high, uint8_t wdigits, uint8_t fdigits, const char *postfix)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    display_prepare_menu_font();
    keypad_clear_events();

    uint8_t option = display_UserInterfaceInputValueF16(&u8g2, title, msg, prefix, value, low, high, wdigits, fdigits, postfix,
        display_GetMenuEvent, DISPLAY_MENU_ACCEPT_MENU, NULL, NULL);

    osMutexRelease(display_mutex);

    return menu_event_timeout ? UINT8_MAX : option;
}

uint8_t display_input_value_f16_data_cb(const char *title, const char *msg, const char *prefix, uint16_t *value,
        uint16_t low, uint16_t high, uint8_t wdigits, uint8_t fdigits, const char *postfix,
        display_data_source_callback_t data_callback, void *user_data)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    display_menu_params_t params = DISPLAY_MENU_ACCEPT_MENU;
    if (data_callback) {
        params |= DISPLAY_MENU_INPUT_POLL;
    }

    display_prepare_menu_font();
    keypad_clear_events();

    uint8_t option = display_UserInterfaceInputValueF16(&u8g2, title, msg, prefix, value, low, high, wdigits, fdigits, postfix,
        display_GetMenuEvent, params, data_callback, user_data);

    osMutexRelease(display_mutex);

    return menu_event_timeout ? UINT8_MAX : option;
}

uint8_t display_input_value_cb(const char *title, const char *msg, const char *prefix, uint8_t *value,
    uint8_t low, uint8_t high, uint8_t digits, const char *postfix,
    display_input_value_callback_t callback, void *user_data)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    display_prepare_menu_font();
    keypad_clear_events();

    uint8_t option = display_UserInterfaceInputValueCB(&u8g2,
        title, msg, prefix, value, low, high, digits, postfix,
        callback, user_data);

    osMutexRelease(display_mutex);

    return menu_event_timeout ? UINT8_MAX : option;
}

static const char* const INPUT_CHARS[] = {
    "ABCDEFGHIJKLM0123456789-=",
    "NOPQRSTUVWXYZ!@#$%^&*()_+",
    "abcdefghijklm[]{}\\|;:'\",.",
    "nopqrstuvwxyz/<>?`~\xFA\xFB\xFC \xFD\xFE"
};

static void display_draw_input_char(u8g2_uint_t x, u8g2_uint_t y, uint16_t ch, uint8_t char_width, u8g2_uint_t line_height)
{
    if (ch == ' ') {
        u8g2_DrawLine(&u8g2, x, y - 1, (x + char_width) - 1, y - 1);
        u8g2_DrawLine(&u8g2, x, y - 2, x, y - 4);
        u8g2_DrawLine(&u8g2, (x + char_width) - 1, y - 2, (x + char_width) - 1, y - 4);
    } else if (ch == 0xFD) {
        u8g2_DrawLine(&u8g2, x + 1, y - 5, (x + char_width) - 1, y - 5);
        u8g2_DrawLine(&u8g2, x + 1, y - 4, (x + char_width) - 1, y - 4);
        u8g2_DrawPixel(&u8g2, x + 2, y - 6);
        u8g2_DrawPixel(&u8g2, x + 3, y - 6);
        u8g2_DrawPixel(&u8g2, x + 3, y - 7);
        u8g2_DrawPixel(&u8g2, x + 2, y - 3);
        u8g2_DrawPixel(&u8g2, x + 3, y - 3);
        u8g2_DrawPixel(&u8g2, x + 3, y - 2);
    } else if (ch == 0xFE) {
        u8g2_DrawLine(&u8g2, x + 1, y - 1, x + 1, y - 4);
        u8g2_DrawBox(&u8g2, x + 3, y - 4, 4, 4);
        u8g2_DrawBox(&u8g2, x + 4, y - 6, 2, 2);
        u8g2_DrawPixel(&u8g2, x + 5, y - 7);
        u8g2_DrawPixel(&u8g2, x + 7, y - 4);
        u8g2_DrawPixel(&u8g2, x + 7, y - 3);
    } else if (ch >= 0xFA) {
        u8g2_DrawGlyph(&u8g2, x, y, ' ');
    } else {
        u8g2_DrawGlyph(&u8g2, x, y, ch);
    }
}

static void display_draw_input_grid(u8g2_uint_t y, char selected)
{
    uint8_t char_width = u8g2_GetMaxCharWidth(&u8g2);
    u8g2_uint_t line_height = u8g2_GetAscent(&u8g2) - u8g2_GetDescent(&u8g2) + 1;

    uint16_t ch;
    for (uint8_t row = 0; row < 4; row++) {
        u8g2_uint_t x = 3;
        for (uint8_t i = 0; i < 25; i++) {
            ch = INPUT_CHARS[row][i];
            if (ch == '\0') { break; }

            if (ch == selected) {
                u8g2_SetDrawColor(&u8g2, 1);
                u8g2_DrawBox(&u8g2, x - 1, y - line_height, char_width + 2, line_height + 1);
                u8g2_SetDrawColor(&u8g2, 0);
                display_draw_input_char(x, y, ch, char_width, line_height);
                u8g2_SetDrawColor(&u8g2, 1);
            } else {
                u8g2_SetDrawColor(&u8g2, 0);
                u8g2_DrawBox(&u8g2, x - 1, y - line_height, char_width + 2, line_height + 1);
                u8g2_SetDrawColor(&u8g2, 1);
                display_draw_input_char(x, y, ch, char_width, line_height);
            }

            x += char_width + 2;
        }
        y += line_height + 1;
    }
}

uint8_t display_input_text(const char *title, char *text, size_t text_len)
{
    osMutexAcquire(display_mutex, portMAX_DELAY);

    display_prepare_menu_font();
    u8g2_ClearBuffer(&u8g2);

    char str[31];
    u8g2_uint_t yy;
    u8g2_uint_t grid_y;
    u8g2_uint_t line_height = u8g2_GetAscent(&u8g2) - u8g2_GetDescent(&u8g2) + 1;
    uint8_t char_width = u8g2_GetMaxCharWidth(&u8g2);

    memset(str, 0, sizeof(str));

    u8g2_SetFontPosBaseline(&u8g2);

    yy = u8g2_GetAscent(&u8g2);
    yy += u8g2_DrawUTF8Lines(&u8g2, 0, yy, u8g2_GetDisplayWidth(&u8g2), line_height, title);
    u8g2_DrawHLine(&u8g2, 0, yy - line_height - u8g2_GetDescent(&u8g2) + 1, u8g2_GetDisplayWidth(&u8g2));
    yy += 3;

    grid_y = u8g2_GetDisplayHeight(&u8g2) - (line_height + 1) * 3 - 1;

    u8g2_DrawGlyph(&u8g2, 0, yy, ']');

    uint8_t cursor = 0;
    uint8_t row = 0;
    uint8_t col = 0;
    bool accepted = false;

    if (text && strlen(text) > 0) {
        strncpy(str, text, sizeof(str));
        cursor = strlen(str);
    }

    for(;;) {
        char ch = INPUT_CHARS[row][col];

        uint8_t cursor_x = (cursor + 1) * char_width;

        u8g2_SetDrawColor(&u8g2, 0);
        u8g2_DrawBox(&u8g2, char_width, yy - line_height, u8g2_GetDisplayWidth(&u8g2) - char_width, line_height);

        u8g2_SetDrawColor(&u8g2, 1);
        u8g2_DrawUTF8(&u8g2, char_width, yy, str);
        u8g2_DrawLine(&u8g2, cursor_x, yy - 1, (cursor_x + char_width) - 1, yy - 1);
        u8g2_DrawLine(&u8g2, cursor_x, yy - 2, (cursor_x + char_width) - 1, yy - 2);

        display_draw_input_grid(grid_y, ch);
        u8g2_SendBuffer(&u8g2);

        uint16_t event_result = display_GetMenuEvent(u8g2_GetU8x8(&u8g2),
            DISPLAY_MENU_ACCEPT_MENU | DISPLAY_MENU_ACCEPT_ADD_ADJUSTMENT | DISPLAY_MENU_INPUT_ASCII);
        uint8_t event_action = (uint8_t)(event_result & 0x00FF);
        uint8_t event_keycode = (uint8_t)((event_result & 0xFF00) >> 8);

        if (event_action == U8X8_MSG_GPIO_MENU_SELECT && event_keycode == KEYPAD_ADD_ADJUSTMENT) {
            if (ch == 0xFD) {
                if (cursor > 0) {
                    cursor--;
                    str[cursor] = '\0';
                }
            } else if (ch == 0xFE) {
                accepted = true;
                break;
            } else if (cursor < sizeof(str) - 1) {
                str[cursor++] = ch;
            }
        }
        else if (event_action == U8X8_MSG_GPIO_MENU_SELECT && event_keycode == KEYPAD_MENU) {
            accepted = true;
            break;
        }
        else if (event_action == U8X8_MSG_GPIO_MENU_INPUT_ASCII) {
            if (event_keycode >= 32 && event_keycode < 127) {
                if (cursor < sizeof(str) - 1) {
                    str[cursor++] = (char)event_keycode;
                }
            } else if (event_keycode == '\b') {
                if (cursor > 0) {
                    cursor--;
                    str[cursor] = '\0';
                }
            }
        }
        else if (event_action == U8X8_MSG_GPIO_MENU_HOME) {
            break;
        }
        else if (event_action == U8X8_MSG_GPIO_MENU_NEXT) {
            col = (col >= 24) ? 0 : col + 1;
            if (row == 3 && col >= 19 && col < 22) {
                col = 22;
            }
        }
        else if (event_action == U8X8_MSG_GPIO_MENU_PREV) {
            col = (col == 0) ? 24 : col - 1;
            if (row == 3 && col >= 19 && col < 22) {
                col = 18;
            }
        }
        else if (event_action == U8X8_MSG_GPIO_MENU_DOWN) {
            row = (row >= 3) ? 0 : row + 1;
            if (row == 3 && col >= 19 && col < 22) {
                row = 0;
            }
        }
        else if (event_action == U8X8_MSG_GPIO_MENU_UP) {
            row = (row == 0) ? 3 : row - 1;
            if (row == 3 && col >= 19 && col < 22) {
                row = 2;
            }
        }
    }

    uint8_t result;

    if (accepted) {
        result = strlen(str);
        if (text && result > 0) {
            strncpy(text, str, text_len);
        }
    } else {
        result = 0;
    }

    osMutexRelease(display_mutex);

    return result;
}

uint8_t u8x8_GetMenuEvent(u8x8_t *u8x8)
{
    // This function should override a u8g2 framework function with the
    // same name, due to its declaration with the "weak" pragma.
    menu_event_timeout = false;

    uint16_t result = display_GetMenuEvent(u8x8, DISPLAY_MENU_ACCEPT_MENU);

    if (result == UINT16_MAX) {
        menu_event_timeout = true;
        return U8X8_MSG_GPIO_MENU_HOME;
    } else {
        return (uint8_t)(result & 0x00FF);
    }
}

void display_prepare_menu_font()
{
    u8g2_SetFont(&u8g2, u8g2_font_pressstart2p_8f);
    u8g2_SetFontMode(&u8g2, 0);
    u8g2_SetDrawColor(&u8g2, 1);
}
