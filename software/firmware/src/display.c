#include "display.h"

#include <FreeRTOS.h>
#include <semphr.h>
#include <cmsis_os.h>
#include <stdio.h>
#include <stdlib.h>
#include <esp_log.h>
#include <ff.h>

#include "u8g2_stm32_hal.h"
#include "u8g2.h"
#include "display_assets.h"
#include "display_internal.h"
#include "keypad.h"

#define MENU_TIMEOUT_MS 30000

static const char *TAG = "display";

#define MAX(a,b) (((a)>(b))?(a):(b))

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

static void display_set_freq(uint8_t value);
static void display_draw_segment(u8g2_uint_t x, u8g2_uint_t y, display_seg_t segment);
static void display_draw_msegment(u8g2_uint_t x, u8g2_uint_t y, display_seg_t segment);
static void display_draw_tsegment(u8g2_uint_t x, u8g2_uint_t y, display_seg_t segment);
static void display_draw_vtsegment(u8g2_uint_t x, u8g2_uint_t y, display_seg_t segment);
static void display_draw_digit(u8g2_uint_t x, u8g2_uint_t y, uint8_t digit);
static void display_draw_sign(u8g2_uint_t x, u8g2_uint_t y, bool positive);
static void display_draw_mdigit(u8g2_uint_t x, u8g2_uint_t y, uint8_t digit);
static void display_draw_tdigit(u8g2_uint_t x, u8g2_uint_t y, uint8_t digit);
static void display_draw_vtdigit(u8g2_uint_t x, u8g2_uint_t y, uint8_t digit);
static void display_draw_tdigit_fraction(u8g2_uint_t x, u8g2_uint_t y, uint8_t num, uint8_t den);
static void display_draw_tdigit_fraction_part(u8g2_uint_t x, u8g2_uint_t y, uint8_t value);
static void display_draw_tdigit_fraction_divider(u8g2_uint_t x, u8g2_uint_t y, uint8_t max_value);
static void display_draw_tone_graph(uint32_t tone_graph);
static void display_draw_burn_dodge_count(uint8_t count);
static void display_draw_contrast_grade(u8g2_uint_t x, u8g2_uint_t y, display_grade_t grade);
static void display_draw_contrast_grade_medium(u8g2_uint_t x, u8g2_uint_t y, display_grade_t grade);
static void display_draw_counter_time(uint16_t seconds, uint16_t milliseconds, uint8_t fraction_digits);
static void display_draw_counter_time_small(u8g2_uint_t x2, u8g2_uint_t y1, const display_exposure_timer_t *time_elements);
static void display_prepare_menu_font();

static uint16_t display_GetMenuEvent(u8x8_t *u8x8, display_menu_params_t params);

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

static FIL *screenshot_fp = NULL;
static void display_save_screenshot_callback(const char *s)
{
    if (screenshot_fp) {
        FRESULT res = f_puts(s, screenshot_fp);
        if (res < 0) {
            ESP_LOGE(TAG, "Error writing screenshot data to file: %d", res);
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
            ESP_LOGE(TAG, "Error opening screenshot file: %d", res);
            break;
        }
        file_open = true;

        screenshot_fp = &fp;
        u8g2_WriteBufferXBM(&u8g2, display_save_screenshot_callback);
        screenshot_fp = NULL;

        ESP_LOGD(TAG, "Screenshot written to file: %s", filename);
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
 * Draw a segment of a digit on a 18x37 pixel grid
 */
void display_draw_msegment(u8g2_uint_t x, u8g2_uint_t y, display_seg_t segment)
{
    switch(segment) {
    case seg_a:
        u8g2_DrawLine(&u8g2, x + 1, y + 0, x + 16, y + 0);
        u8g2_DrawLine(&u8g2, x + 2, y + 1, x + 15, y + 1);
        u8g2_DrawLine(&u8g2, x + 3, y + 2, x + 14, y + 2);
        break;
    case seg_b:
        u8g2_DrawLine(&u8g2, x + 15, y + 3, x + 15, y + 16);
        u8g2_DrawLine(&u8g2, x + 16, y + 2, x + 16, y + 17);
        u8g2_DrawLine(&u8g2, x + 17, y + 1, x + 17, y + 16);
        break;
    case seg_c:
        u8g2_DrawLine(&u8g2, x + 15, y + 20, x + 15, y + 33);
        u8g2_DrawLine(&u8g2, x + 16, y + 19, x + 16, y + 34);
        u8g2_DrawLine(&u8g2, x + 17, y + 20, x + 17, y + 35);
        break;
    case seg_d:
        u8g2_DrawLine(&u8g2, x + 3, y + 34, x + 14, y + 34);
        u8g2_DrawLine(&u8g2, x + 2, y + 35, x + 15, y + 35);
        u8g2_DrawLine(&u8g2, x + 1, y + 36, x + 16, y + 36);
        break;
    case seg_e:
        u8g2_DrawLine(&u8g2, x + 0, y + 20, x + 0, y + 35);
        u8g2_DrawLine(&u8g2, x + 1, y + 19, x + 1, y + 34);
        u8g2_DrawLine(&u8g2, x + 2, y + 20, x + 2, y + 33);
        break;
    case seg_f:
        u8g2_DrawLine(&u8g2, x + 0, y + 1, x + 0, y + 16);
        u8g2_DrawLine(&u8g2, x + 1, y + 2, x + 1, y + 17);
        u8g2_DrawLine(&u8g2, x + 2, y + 3, x + 2, y + 16);
        break;
    case seg_g:
        u8g2_DrawLine(&u8g2, x + 3, y + 17, x + 14, y + 17);
        u8g2_DrawLine(&u8g2, x + 2, y + 18, x + 15, y + 18);
        u8g2_DrawLine(&u8g2, x + 3, y + 19, x + 14, y + 19);
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
 * Draw a segment of a digit on a 9x17 pixel grid
 */
void display_draw_vtsegment(u8g2_uint_t x, u8g2_uint_t y, display_seg_t segment)
{
    switch(segment) {
    case seg_a:
        u8g2_DrawLine(&u8g2, x + 1, y + 0, x + 7, y + 0);
        u8g2_DrawLine(&u8g2, x + 2, y + 1, x + 6, y + 1);
        break;
    case seg_b:
        u8g2_DrawLine(&u8g2, x + 7, y + 2, x + 7, y + 6);
        u8g2_DrawLine(&u8g2, x + 8, y + 1, x + 8, y + 7);
        break;
    case seg_c:
        u8g2_DrawLine(&u8g2, x + 7, y + 10, x + 7, y + 14);
        u8g2_DrawLine(&u8g2, x + 8, y + 9, x + 8, y + 15);
        break;
    case seg_d:
        u8g2_DrawLine(&u8g2, x + 2, y + 15, x + 6, y + 15);
        u8g2_DrawLine(&u8g2, x + 1, y + 16, x + 7, y + 16);
        break;
    case seg_e:
        u8g2_DrawLine(&u8g2, x + 0, y + 9, x + 0, y + 15);
        u8g2_DrawLine(&u8g2, x + 1, y + 10, x + 1, y + 14);
        break;
    case seg_f:
        u8g2_DrawLine(&u8g2, x + 0, y + 1, x + 0, y + 7);
        u8g2_DrawLine(&u8g2, x + 1, y + 2, x + 1, y + 6);
        break;
    case seg_g:
        u8g2_DrawLine(&u8g2, x + 2, y + 7, x + 6, y + 7);
        u8g2_DrawLine(&u8g2, x + 1, y + 8, x + 7, y + 8);
        u8g2_DrawLine(&u8g2, x + 2, y + 9, x + 6, y + 9);
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
 * Draw a 18x37 pixel digit
 */
void display_draw_mdigit(u8g2_uint_t x, u8g2_uint_t y, uint8_t digit)
{
    switch(digit) {
    case 0:
        display_draw_msegment(x, y, seg_a);
        display_draw_msegment(x, y, seg_b);
        display_draw_msegment(x, y, seg_c);
        display_draw_msegment(x, y, seg_d);
        display_draw_msegment(x, y, seg_e);
        display_draw_msegment(x, y, seg_f);
        break;
    case 1:
        display_draw_msegment(x, y, seg_b);
        display_draw_msegment(x, y, seg_c);
        break;
    case 2:
        display_draw_msegment(x, y, seg_a);
        display_draw_msegment(x, y, seg_b);
        display_draw_msegment(x, y, seg_d);
        display_draw_msegment(x, y, seg_e);
        display_draw_msegment(x, y, seg_g);
        break;
    case 3:
        display_draw_msegment(x, y, seg_a);
        display_draw_msegment(x, y, seg_b);
        display_draw_msegment(x, y, seg_c);
        display_draw_msegment(x, y, seg_d);
        display_draw_msegment(x, y, seg_g);
        break;
    case 4:
        display_draw_msegment(x, y, seg_b);
        display_draw_msegment(x, y, seg_c);
        display_draw_msegment(x, y, seg_f);
        display_draw_msegment(x, y, seg_g);
        break;
    case 5:
        display_draw_msegment(x, y, seg_a);
        display_draw_msegment(x, y, seg_c);
        display_draw_msegment(x, y, seg_d);
        display_draw_msegment(x, y, seg_f);
        display_draw_msegment(x, y, seg_g);
        break;
    case 6:
        display_draw_msegment(x, y, seg_a);
        display_draw_msegment(x, y, seg_c);
        display_draw_msegment(x, y, seg_d);
        display_draw_msegment(x, y, seg_e);
        display_draw_msegment(x, y, seg_f);
        display_draw_msegment(x, y, seg_g);
        break;
    case 7:
        display_draw_msegment(x, y, seg_a);
        display_draw_msegment(x, y, seg_b);
        display_draw_msegment(x, y, seg_c);
        break;
    case 8:
        display_draw_msegment(x, y, seg_a);
        display_draw_msegment(x, y, seg_b);
        display_draw_msegment(x, y, seg_c);
        display_draw_msegment(x, y, seg_d);
        display_draw_msegment(x, y, seg_e);
        display_draw_msegment(x, y, seg_f);
        display_draw_msegment(x, y, seg_g);
        break;
    case 9:
        display_draw_msegment(x, y, seg_a);
        display_draw_msegment(x, y, seg_b);
        display_draw_msegment(x, y, seg_c);
        display_draw_msegment(x, y, seg_d);
        display_draw_msegment(x, y, seg_f);
        display_draw_msegment(x, y, seg_g);
        break;
    default:
        break;
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

/**
 * Draw a 9x17 pixel digit
 */
void display_draw_vtdigit(u8g2_uint_t x, u8g2_uint_t y, uint8_t digit)
{
    switch(digit) {
    case 0:
        display_draw_vtsegment(x, y, seg_a);
        display_draw_vtsegment(x, y, seg_b);
        display_draw_vtsegment(x, y, seg_c);
        display_draw_vtsegment(x, y, seg_d);
        display_draw_vtsegment(x, y, seg_e);
        display_draw_vtsegment(x, y, seg_f);
        break;
    case 1:
        display_draw_vtsegment(x, y, seg_b);
        display_draw_vtsegment(x, y, seg_c);
        break;
    case 2:
        display_draw_vtsegment(x, y, seg_a);
        display_draw_vtsegment(x, y, seg_b);
        display_draw_vtsegment(x, y, seg_d);
        display_draw_vtsegment(x, y, seg_e);
        display_draw_vtsegment(x, y, seg_g);
        break;
    case 3:
        display_draw_vtsegment(x, y, seg_a);
        display_draw_vtsegment(x, y, seg_b);
        display_draw_vtsegment(x, y, seg_c);
        display_draw_vtsegment(x, y, seg_d);
        display_draw_vtsegment(x, y, seg_g);
        break;
    case 4:
        display_draw_vtsegment(x, y, seg_b);
        display_draw_vtsegment(x, y, seg_c);
        display_draw_vtsegment(x, y, seg_f);
        display_draw_vtsegment(x, y, seg_g);
        break;
    case 5:
        display_draw_vtsegment(x, y, seg_a);
        display_draw_vtsegment(x, y, seg_c);
        display_draw_vtsegment(x, y, seg_d);
        display_draw_vtsegment(x, y, seg_f);
        display_draw_vtsegment(x, y, seg_g);
        break;
    case 6:
        display_draw_vtsegment(x, y, seg_a);
        display_draw_vtsegment(x, y, seg_c);
        display_draw_vtsegment(x, y, seg_d);
        display_draw_vtsegment(x, y, seg_e);
        display_draw_vtsegment(x, y, seg_f);
        display_draw_vtsegment(x, y, seg_g);
        break;
    case 7:
        display_draw_vtsegment(x, y, seg_a);
        display_draw_vtsegment(x, y, seg_b);
        display_draw_vtsegment(x, y, seg_c);
        break;
    case 8:
        display_draw_vtsegment(x, y, seg_a);
        display_draw_vtsegment(x, y, seg_b);
        display_draw_vtsegment(x, y, seg_c);
        display_draw_vtsegment(x, y, seg_d);
        display_draw_vtsegment(x, y, seg_e);
        display_draw_vtsegment(x, y, seg_f);
        display_draw_vtsegment(x, y, seg_g);
        break;
    case 9:
        display_draw_vtsegment(x, y, seg_a);
        display_draw_vtsegment(x, y, seg_b);
        display_draw_vtsegment(x, y, seg_c);
        display_draw_vtsegment(x, y, seg_d);
        display_draw_vtsegment(x, y, seg_f);
        display_draw_vtsegment(x, y, seg_g);
        break;
    default:
        break;
    }
}

void display_draw_tdigit_fraction(u8g2_uint_t x, u8g2_uint_t y, uint8_t num, uint8_t den)
{
    uint8_t max_value = MAX(num, den);

    // Draw the numerator
    display_draw_tdigit_fraction_part(x, y, num);

    // Draw the dividing line
    display_draw_tdigit_fraction_divider(x, y + 27, max_value);

    // Draw the denominator
    display_draw_tdigit_fraction_part(x, y + 31, den);
}

void display_draw_tdigit_fraction_part(u8g2_uint_t x, u8g2_uint_t y, uint8_t value)
{
    if (value >= 20) {
        // NN/DD (1/20 - 1/99)
        display_draw_tdigit(x + 5, y, value % 100 / 10);
        display_draw_tdigit(x + 22, y, value % 10);
    } else if (value >= 10) {
        // NN/DD (1/10 - 1/19)
        display_draw_tdigit(x + 0, y, value % 100 / 10);
        display_draw_tdigit(x + 17, y, value % 10);
    } else if (value == 0 || value > 1) {
        // N/D
        display_draw_tdigit(x + 13, y, value);
    } else {
        // N/D (1/1)
        display_draw_tdigit(x + 7, y, value);
    }
}

void display_draw_tdigit_fraction_divider(u8g2_uint_t x, u8g2_uint_t y, uint8_t max_value)
{
    if (max_value >= 20) {
        // N/DD (1/20 - 1/99)
        u8g2_DrawLine(&u8g2, x + 2,  y + 0, x + 37, y + 0);
        u8g2_DrawLine(&u8g2, x + 1,  y + 1, x + 38, y + 1);
        u8g2_DrawLine(&u8g2, x + 2,  y + 2, x + 37, y + 2);
    } else if (max_value >= 10) {
        // N/DD (1/10 - 1/19)
        u8g2_DrawLine(&u8g2, x + 8,  y + 0, x + 32, y + 0);
        u8g2_DrawLine(&u8g2, x + 7,  y + 1, x + 33, y + 1);
        u8g2_DrawLine(&u8g2, x + 8,  y + 2, x + 32, y + 2);
    } else if (max_value == 0 || max_value > 1) {
        // N/D
        u8g2_DrawLine(&u8g2, x + 10, y + 0, x + 28, y + 0);
        u8g2_DrawLine(&u8g2, x + 9,  y + 1, x + 29, y + 1);
        u8g2_DrawLine(&u8g2, x + 10, y + 2, x + 28, y + 2);
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

void display_draw_contrast_grade(u8g2_uint_t x, u8g2_uint_t y, display_grade_t grade)
{
    bool show_half = false;

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

void display_draw_contrast_grade_medium(u8g2_uint_t x, u8g2_uint_t y, display_grade_t grade)
{
    bool show_half = false;

    switch (grade) {
    case DISPLAY_GRADE_00:
        display_draw_mdigit(x, y, 0);
        display_draw_mdigit(x + 22, y, 0);
        break;
    case DISPLAY_GRADE_0:
        display_draw_mdigit(x, y, 0);
        break;
    case DISPLAY_GRADE_0_HALF:
        x -= 21;
        show_half = true;
        break;
    case DISPLAY_GRADE_1:
        display_draw_mdigit(x, y, 1);
        break;
    case DISPLAY_GRADE_1_HALF:
        display_draw_mdigit(x, y, 1);
        show_half = true;
        break;
    case DISPLAY_GRADE_2:
        display_draw_mdigit(x, y, 2);
        break;
    case DISPLAY_GRADE_2_HALF:
        display_draw_mdigit(x, y, 2);
        show_half = true;
        break;
    case DISPLAY_GRADE_3:
        display_draw_mdigit(x, y, 3);
        break;
    case DISPLAY_GRADE_3_HALF:
        display_draw_mdigit(x, y, 3);
        show_half = true;
        break;
    case DISPLAY_GRADE_4:
        display_draw_mdigit(x, y, 4);
        break;
    case DISPLAY_GRADE_4_HALF:
        display_draw_mdigit(x, y, 4);
        show_half = true;
        break;
    case DISPLAY_GRADE_5:
        display_draw_mdigit(x, y, 5);
        break;
    case DISPLAY_GRADE_NONE:
    default:
        return;
    }

    if (show_half) {
        display_draw_vtdigit(x + 22, y - 1, 1);
        u8g2_DrawLine(&u8g2, x + 24, y + 16, x + 34, y + 16);
        u8g2_DrawLine(&u8g2, x + 23, y + 17, x + 35, y + 17);
        u8g2_DrawLine(&u8g2, x + 24, y + 18, x + 34, y + 18);
        display_draw_vtdigit(x + 25, y + 20, 2);
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
    display_draw_burn_dodge_count(elements->burn_dodge_count);
    display_draw_contrast_grade(9 + 24, 8, elements->contrast_grade);
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

    if (increment_den == 1) {
        display_draw_digit(x, y, 1);
    } else {
        display_draw_tdigit_fraction(x, y, 1, increment_den);
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
        u8g2_DrawUTF8(&u8g2,
            0, 11 + u8g2_GetAscent(&u8g2),
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

    if (elements->contrast_grade < DISPLAY_GRADE_MAX) {
        // Draw contrast grade
        display_draw_contrast_grade_medium(50, 26, elements->contrast_grade);
    } else {
        // Draw timer clock icon
        display_asset_get(&asset, ASSET_TIMER_ICON_32);
        u8g2_DrawXBM(&u8g2, 56, 28, asset.width, asset.height, asset.bits);
    }

    // Draw exposure time
    display_draw_counter_time(elements->time_elements.time_seconds,
        elements->time_elements.time_milliseconds,
        elements->time_elements.fraction_digits);

    u8g2_SendBuffer(&u8g2);

    osMutexRelease(display_mutex);
}

void display_draw_adjustment_exposure_timer(const display_exposure_timer_t *time_elements)
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
    y = 11 + u8g2_GetAscent(&u8g2);
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

    // Draw adjustment icon
    display_asset_get(&asset, ASSET_ADJUST_ICON_24);
    u8g2_DrawXBM(&u8g2, x, y, asset.width, asset.height, asset.bits);

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
            display_draw_tdigit_fraction(x, y, adj_num, elements->adj_den);
        }
        x -= 36;

        // Draw whole number part
        if (adj_whole != 0 || (adj_whole == 0 && adj_num == 0)) {
            display_draw_digit(x, y, adj_whole);
            x -= 36;
        }

        // Draw +/- sign part
        display_draw_sign(x, y, num_positive);
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
    osMutexAcquire(display_mutex, portMAX_DELAY);

    display_prepare_menu_font();
    keypad_clear_events();

    uint8_t option = display_UserInterfaceInputValueCB(&u8g2,
        title, prefix, value, low, high, digits, postfix,
        callback, user_data);

    osMutexRelease(display_mutex);

    return menu_event_timeout ? UINT8_MAX : option;
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

uint16_t display_GetMenuEvent(u8x8_t *u8x8, display_menu_params_t params)
{
    uint16_t result = 0;

    // If we were called via a function that is holding the display mutex,
    // then release that mutex while blocked on the keypad queue.
    osStatus_t mutex_released = osMutexRelease(display_mutex);

    int timeout = ((params & DISPLAY_MENU_TIMEOUT_DISABLED) != 0) ? -1 : MENU_TIMEOUT_MS;

    keypad_event_t event;
    HAL_StatusTypeDef ret = keypad_wait_for_event(&event, timeout);

    if (mutex_released == osOK) {
        osMutexAcquire(display_mutex, portMAX_DELAY);
    }

    if (ret == HAL_OK) {
        if (event.pressed) {
            // Button actions that stay within the menu are handled on
            // the press event
            switch (event.key) {
            case KEYPAD_DEC_CONTRAST:
                result = U8X8_MSG_GPIO_MENU_PREV;
                break;
            case KEYPAD_INC_CONTRAST:
                result = U8X8_MSG_GPIO_MENU_NEXT;
                break;
            case KEYPAD_INC_EXPOSURE:
                result = U8X8_MSG_GPIO_MENU_UP;
                break;
            case KEYPAD_DEC_EXPOSURE:
                result = U8X8_MSG_GPIO_MENU_DOWN;
                break;
            default:
                break;
            }
        } else {
            // Button actions that leave the menu, such as accept and cancel
            // are handled on the release event. This is to prevent side
            // effects that can occur from other components receiving
            // release events for these keys.
            if (((params & DISPLAY_MENU_ACCEPT_MENU) != 0 && event.key == KEYPAD_MENU)
                || ((params & DISPLAY_MENU_ACCEPT_ADD_ADJUSTMENT) != 0 && event.key == KEYPAD_ADD_ADJUSTMENT)
                || ((params & DISPLAY_MENU_ACCEPT_TEST_STRIP) != 0 && event.key == KEYPAD_TEST_STRIP)
                || ((params & DISPLAY_MENU_ACCEPT_ENCODER) != 0 && event.key == KEYPAD_ENCODER)) {
                result = ((uint16_t)event.key << 8) | U8X8_MSG_GPIO_MENU_SELECT;
            } else if (event.key == KEYPAD_CANCEL) {
                result = U8X8_MSG_GPIO_MENU_HOME;
            }
        }
    } else if (ret == HAL_TIMEOUT) {
        result = UINT16_MAX;
    }
    return result;
}

void display_prepare_menu_font()
{
    u8g2_SetFont(&u8g2, u8g2_font_pressstart2p_8f);
    u8g2_SetFontMode(&u8g2, 0);
    u8g2_SetDrawColor(&u8g2, 1);
}
