#include "display.h"

#include <esp_log.h>

#include "u8g2_stm32_hal.h"
#include "u8g2.h"
#include "display_assets.h"

static const char *TAG = "display";

static u8g2_t u8g2;
static uint8_t display_contrast = 0x9F;
static uint8_t display_brightness = 0x0F;

static void display_set_freq(uint8_t value);

HAL_StatusTypeDef display_init(const u8g2_display_handle_t *display_handle)
{
    ESP_LOGD(TAG, "display_init");

    // Initialize the STM32 display HAL, which includes library display
    // parameter and callback initialization.
    u8g2_stm32_hal_init(&u8g2, display_handle);

    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);

    // Slightly increase the display refresh frequency
    display_set_freq(0xC1);

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
    u8g2_ClearBuffer(&u8g2);
}

void display_enable(bool enabled)
{
    u8g2_SetPowerSave(&u8g2, enabled ? 0 : 1);
}

void display_set_contrast(uint8_t value)
{
    u8g2_SetContrast(&u8g2, value);
    display_contrast = value;
}

uint8_t display_get_contrast()
{
    return display_contrast;
}

void display_set_brightness(uint8_t value)
{
    uint8_t arg = value & 0x0F;

    u8x8_t *u8x8 = &(u8g2.u8x8);
    u8x8_cad_StartTransfer(u8x8);
    u8x8_cad_SendCmd(u8x8, 0x0C7);
    u8x8_cad_SendArg(u8x8, arg);
    u8x8_cad_EndTransfer(u8x8);

    display_brightness = arg;
}

uint8_t display_get_brightness()
{
    return display_brightness;
}

void display_draw_test_pattern(bool mode)
{
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
}

void display_draw_logo()
{
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetBitmapMode(&u8g2, 1);
    asset_info_t asset;
    display_asset_get(&asset, ASSET_PRINTALYZER);
    u8g2_DrawXBM(&u8g2, 0, 0, asset.width, asset.height, asset.bits);
    u8g2_SendBuffer(&u8g2);
}
