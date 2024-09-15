#include "display.h"

#include <stdint.h>
#include <stdbool.h>

#include "u8g2_stm32_hal.h"
#include "u8g2.h"
#include "display_assets.h"

static u8g2_t u8g2;

static void display_set_freq(uint8_t value);

HAL_StatusTypeDef display_init(const u8g2_display_handle_t *display_handle)
{
    /*
     * Initialize the STM32 display HAL, which includes library display
     * parameter and callback initialization.
     */
    u8g2_stm32_hal_init(&u8g2, display_handle);

    u8g2_InitDisplay(&u8g2);

    /* Slightly increase the display refresh frequency */
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
    u8g2_SendBuffer(&u8g2);
}

void display_enable(bool enabled)
{
    u8g2_SetPowerSave(&u8g2, enabled ? 0 : 1);
}

void display_draw_test_pattern()
{
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetDrawColor(&u8g2, 1);

    bool draw = false;
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

void display_graphic_insert_thumbdrive()
{
    asset_info_t asset;

    u8g2_ClearBuffer(&u8g2);

    display_asset_get(&asset, ASSET_THUMBDRIVE);
    u8g2_DrawXBM(&u8g2, 20, 6, asset.width, asset.height, asset.bits);

    display_asset_get(&asset, ASSET_ARROW);
    u8g2_DrawXBM(&u8g2, 167, 24, asset.width, asset.height, asset.bits);

    display_asset_get(&asset, ASSET_USB_PORT);
    u8g2_DrawXBM(&u8g2, 204, 1, asset.width, asset.height, asset.bits);

    u8g2_SendBuffer(&u8g2);
}

void display_graphic_checking_thumbdrive()
{
    asset_info_t asset;

    u8g2_ClearBuffer(&u8g2);

    display_asset_get(&asset, ASSET_THUMBDRIVE);
    u8g2_DrawXBM(&u8g2, 20, 6, asset.width, asset.height, asset.bits);

    display_asset_get(&asset, ASSET_ARROW);
    u8g2_DrawXBM(&u8g2, 167, 24, asset.width, asset.height, asset.bits);

    display_asset_get(&asset, ASSET_HOURGLASS);
    u8g2_DrawXBM(&u8g2, 204, 1, asset.width, asset.height, asset.bits);

    u8g2_SendBuffer(&u8g2);
}

void display_graphic_file_missing()
{
    asset_info_t asset;

    u8g2_ClearBuffer(&u8g2);

    display_asset_get(&asset, ASSET_THUMBDRIVE);
    u8g2_DrawXBM(&u8g2, 20, 6, asset.width, asset.height, asset.bits);

    display_asset_get(&asset, ASSET_BAD_FILE);
    u8g2_DrawXBM(&u8g2, 180, 10, asset.width, asset.height, asset.bits);

    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawBox(&u8g2, 180, 46, 11, 11);
    u8g2_DrawBox(&u8g2, 192, 19, 19, 7);
    u8g2_DrawBox(&u8g2, 192, 29, 25, 16);
    u8g2_SetDrawColor(&u8g2, 1);

    u8g2_DrawBox(&u8g2, 183, 43, 5, 11);
    u8g2_DrawBox(&u8g2, 188, 49, 6, 5);

    display_asset_get(&asset, ASSET_QUESTION);
    u8g2_DrawXBM(&u8g2, 197, 16, asset.width, asset.height, asset.bits);

    u8g2_SendBuffer(&u8g2);
}

void display_graphic_file_read_error()
{
    asset_info_t asset;

    u8g2_ClearBuffer(&u8g2);

    display_asset_get(&asset, ASSET_THUMBDRIVE);
    u8g2_DrawXBM(&u8g2, 20, 6, asset.width, asset.height, asset.bits);

    display_asset_get(&asset, ASSET_BAD_FILE);
    u8g2_DrawXBM(&u8g2, 180, 10, asset.width, asset.height, asset.bits);

    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawBox(&u8g2, 180, 46, 11, 11);
    u8g2_DrawBox(&u8g2, 192, 19, 19, 7);
    u8g2_DrawBox(&u8g2, 192, 29, 25, 16);
    u8g2_SetDrawColor(&u8g2, 1);

    u8g2_DrawBox(&u8g2, 183, 43, 5, 11);
    u8g2_DrawBox(&u8g2, 188, 49, 6, 5);

    u8g2_DrawVLine(&u8g2, 203, 22, 14);
    u8g2_DrawVLine(&u8g2, 204, 21, 16);
    u8g2_DrawVLine(&u8g2, 205, 21, 16);
    u8g2_DrawVLine(&u8g2, 206, 22, 14);

    u8g2_DrawVLine(&u8g2, 203, 40, 2);
    u8g2_DrawVLine(&u8g2, 204, 39, 4);
    u8g2_DrawVLine(&u8g2, 205, 39, 4);
    u8g2_DrawVLine(&u8g2, 206, 40, 2);

    u8g2_SendBuffer(&u8g2);
}

void display_graphic_file_bad()
{
    asset_info_t asset;

    u8g2_ClearBuffer(&u8g2);

    display_asset_get(&asset, ASSET_THUMBDRIVE);
    u8g2_DrawXBM(&u8g2, 20, 6, asset.width, asset.height, asset.bits);

    display_asset_get(&asset, ASSET_BAD_FILE);
    u8g2_DrawXBM(&u8g2, 180, 10, asset.width, asset.height, asset.bits);

    u8g2_SendBuffer(&u8g2);
}

void display_graphic_update_prompt()
{
    asset_info_t asset;

    u8g2_ClearBuffer(&u8g2);

    display_asset_get(&asset, ASSET_THUMBDRIVE);
    u8g2_DrawXBM(&u8g2, 7, 6, asset.width, asset.height, asset.bits);

    display_asset_get(&asset, ASSET_ARROW);
    u8g2_SetClipWindow(&u8g2, 149, 24, 161, 40);
    u8g2_DrawXBM(&u8g2, 149, 24, asset.width, asset.height, asset.bits);
    u8g2_SetClipWindow(&u8g2, 180, 24, 192, 40);
    u8g2_DrawXBM(&u8g2, 169, 24, asset.width, asset.height, asset.bits);
    u8g2_SetMaxClipWindow(&u8g2);

    display_asset_get(&asset, ASSET_QUESTION);
    u8g2_DrawXBM(&u8g2, 162, 16, asset.width, asset.height, asset.bits);

    display_asset_get(&asset, ASSET_MEMORY);
    u8g2_DrawXBM(&u8g2, 201, 8, asset.width, asset.height, asset.bits);

    u8g2_SendBuffer(&u8g2);
}

void display_graphic_unlock_prompt()
{
    asset_info_t asset;

    u8g2_ClearBuffer(&u8g2);

    display_asset_get(&asset, ASSET_MEMORY);
    u8g2_DrawXBM(&u8g2, 50, 8, asset.width, asset.height, asset.bits);
    u8g2_DrawXBM(&u8g2, 158, 8, asset.width, asset.height, asset.bits);

    display_asset_get(&asset, ASSET_ARROW);
    u8g2_SetClipWindow(&u8g2, 106, 24, 118, 40);
    u8g2_DrawXBM(&u8g2, 106, 24, asset.width, asset.height, asset.bits);
    u8g2_SetClipWindow(&u8g2, 137, 24, 149, 40);
    u8g2_DrawXBM(&u8g2, 126, 24, asset.width, asset.height, asset.bits);
    u8g2_SetMaxClipWindow(&u8g2);

    display_asset_get(&asset, ASSET_QUESTION);
    u8g2_DrawXBM(&u8g2, 119, 16, asset.width, asset.height, asset.bits);

    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawBox(&u8g2, 61, 19, 26, 26);
    u8g2_SetDrawColor(&u8g2, 1);

    display_asset_get(&asset, ASSET_PADLOCK);
    u8g2_DrawXBM(&u8g2, 66, 22, asset.width, asset.height, asset.bits);

    u8g2_SendBuffer(&u8g2);
}

void display_graphic_update_progress()
{
    asset_info_t asset;

    u8g2_ClearBuffer(&u8g2);

    display_asset_get(&asset, ASSET_THUMBDRIVE);
    u8g2_DrawXBM(&u8g2, 20, 0, asset.width, asset.height, asset.bits);

    display_asset_get(&asset, ASSET_ARROW);
    u8g2_DrawXBM(&u8g2, 159, 18, asset.width, asset.height, asset.bits);

    display_asset_get(&asset, ASSET_MEMORY);
    u8g2_DrawXBM(&u8g2, 188, 2, asset.width, asset.height, asset.bits);

    u8g2_DrawLine(&u8g2, 22, 54, 233, 54);
    u8g2_DrawLine(&u8g2, 22, 55, 233, 55);

    u8g2_DrawLine(&u8g2, 21, 55, 21, 62);
    u8g2_DrawLine(&u8g2, 234, 55, 234, 62);

    u8g2_DrawLine(&u8g2, 20, 57, 20, 60);
    u8g2_DrawLine(&u8g2, 235, 57, 235, 60);

    u8g2_DrawPixel(&u8g2, 22, 56);
    u8g2_DrawPixel(&u8g2, 22, 61);
    u8g2_DrawPixel(&u8g2, 233, 56);
    u8g2_DrawPixel(&u8g2, 233, 61);

    u8g2_DrawLine(&u8g2, 22, 62, 233, 62);
    u8g2_DrawLine(&u8g2, 22, 63, 233, 63);

    u8g2_SendBuffer(&u8g2);
}

void display_graphic_update_progress_increment(uint8_t value)
{
    /*
     * This function assumes the results of display_graphic_update_progress()
     * are already on the display, and it only draws additional progress.
     * Setting a value less than the last displayed progress will not clear
     * out and redraw the progress bar.
     *
     * Rendered progress values are from 1 to 210.
     */

    if (value > 0) {
        u8g2_DrawLine(&u8g2, 22, 57, 22, 60);
    }

    if (value >= 1 && value <= 210) {
        u8g2_DrawBox(&u8g2, 23, 56, value, 6);
    }

    if (value >= 210) {
        u8g2_DrawLine(&u8g2, 233, 57, 233, 60);
    }

    u8g2_SendBuffer(&u8g2);
}

void display_graphic_update_progress_success()
{
    /*
     * This function assumes the results of display_graphic_update_progress()
     * are already on the display.
     */

    u8g2_SetDrawColor(&u8g2, 0);

    u8g2_DrawBox(&u8g2, 199, 13, 26, 26);

    u8g2_SetDrawColor(&u8g2, 1);

    u8g2_DrawLine(&u8g2, 202, 28, 207, 33);
    u8g2_DrawLine(&u8g2, 201, 28, 207, 34);
    u8g2_DrawLine(&u8g2, 201, 29, 207, 35);

    u8g2_DrawLine(&u8g2, 208, 32, 221, 19);
    u8g2_DrawLine(&u8g2, 208, 33, 222, 19);
    u8g2_DrawLine(&u8g2, 208, 34, 222, 20);

    u8g2_SendBuffer(&u8g2);
}

void display_graphic_update_progress_failure()
{
    /*
     * This function assumes the results of display_graphic_update_progress()
     * are already on the display.
     */

    u8g2_SetDrawColor(&u8g2, 0);

    u8g2_DrawBox(&u8g2, 199, 13, 26, 26);

    u8g2_SetDrawColor(&u8g2, 1);

    u8g2_DrawLine(&u8g2, 210, 16, 210, 29);
    u8g2_DrawLine(&u8g2, 211, 15, 211, 30);
    u8g2_DrawLine(&u8g2, 212, 15, 212, 30);
    u8g2_DrawLine(&u8g2, 213, 16, 213, 29);

    u8g2_DrawLine(&u8g2, 210, 34, 210, 35);
    u8g2_DrawLine(&u8g2, 211, 33, 211, 36);
    u8g2_DrawLine(&u8g2, 212, 33, 212, 36);
    u8g2_DrawLine(&u8g2, 213, 34, 213, 35);

    u8g2_SendBuffer(&u8g2);
}

void display_graphic_restart()
{
    asset_info_t asset;

    u8g2_ClearBuffer(&u8g2);

    display_asset_get(&asset, ASSET_RESTART);
    u8g2_DrawXBM(&u8g2, 107, 11, asset.width, asset.height, asset.bits);

    u8g2_SendBuffer(&u8g2);
}

void display_graphic_failure()
{
    asset_info_t asset;

    u8g2_ClearBuffer(&u8g2);

    display_asset_get(&asset, ASSET_MEMORY);
    u8g2_DrawXBM(&u8g2, 104, 8, asset.width, asset.height, asset.bits);

    u8g2_SetDrawColor(&u8g2, 0);

    u8g2_DrawBox(&u8g2, 115, 19, 26, 26);

    u8g2_SetDrawColor(&u8g2, 1);

    u8g2_DrawVLine(&u8g2, 126, 22, 14);
    u8g2_DrawVLine(&u8g2, 127, 21, 16);
    u8g2_DrawVLine(&u8g2, 128, 21, 16);
    u8g2_DrawVLine(&u8g2, 129, 22, 14);

    u8g2_DrawVLine(&u8g2, 126, 40, 2);
    u8g2_DrawVLine(&u8g2, 127, 39, 4);
    u8g2_DrawVLine(&u8g2, 128, 39, 4);
    u8g2_DrawVLine(&u8g2, 129, 40, 2);

    u8g2_SendBuffer(&u8g2);
}
