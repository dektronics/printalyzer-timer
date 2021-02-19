#include "display.h"

#include <stdint.h>
#include <stdbool.h>

#include "u8g2_stm32_hal.h"
#include "u8g2.h"

static u8g2_t u8g2;

static void display_set_freq(uint8_t value);
static void display_prepare_menu_font();

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

void display_prepare_menu_font()
{
    u8g2_SetFont(&u8g2, u8g2_font_pressstart2p_8f);
    u8g2_SetFontMode(&u8g2, 0);
    u8g2_SetDrawColor(&u8g2, 1);
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

void display_static_message(const char *message)
{
    display_prepare_menu_font();

    uint8_t height;
    uint8_t line_height;
    u8g2_uint_t pixel_height;
    u8g2_uint_t y;

    /* Only horizontal strings are supported, so force this here */
    u8g2_SetFontDirection(&u8g2, 0);

    /* Force baseline position */
    u8g2_SetFontPosBaseline(&u8g2);

    /* Calculate line height */
    line_height = u8g2_GetAscent(&u8g2);
    line_height -= u8g2_GetDescent(&u8g2);

    /* Calculate overall height of the message in lines */
    height = u8x8_GetStringLineCnt(message);

    /* Calculate the height in pixels */
    pixel_height = height * line_height;

    /* Calculate offset from the top */
    y = 0;
    if (pixel_height < u8g2_GetDisplayHeight(&u8g2)) {
        y = u8g2_GetDisplayHeight(&u8g2);
        y -= pixel_height;
        y /= 2;
    }
    y += u8g2_GetAscent(&u8g2);

    u8g2_ClearBuffer(&u8g2);

    u8g2_DrawUTF8Lines(&u8g2, 0, y, u8g2_GetDisplayWidth(&u8g2), line_height, message);

    u8g2_SendBuffer(&u8g2);
}
