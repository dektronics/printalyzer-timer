#include "display_internal.h"

#include "display.h"
#include "u8g2.h"

#define MY_BORDER_SIZE 1

bool menu_event_timeout = false;

/* Library function declarations */
void u8g2_DrawSelectionList(u8g2_t *u8g2, u8sl_t *u8sl, u8g2_uint_t y, const char *s);

void display_UserInterfaceStaticList(u8g2_t *u8g2, const char *title, const char *list)
{
    // Based off u8g2_UserInterfaceSelectionList() with changes to use
    // full frame buffer mode and to remove actual menu functionality.

    u8g2_ClearBuffer(u8g2);

    u8sl_t u8sl;
    u8g2_uint_t yy;

    u8g2_uint_t line_height = u8g2_GetAscent(u8g2) - u8g2_GetDescent(u8g2) + MY_BORDER_SIZE;

    uint8_t title_lines = u8x8_GetStringLineCnt(title);
    uint8_t display_lines;

    if (title_lines > 0) {
        display_lines = (u8g2_GetDisplayHeight(u8g2) - 3) / line_height;
        u8sl.visible = display_lines;
        u8sl.visible -= title_lines;
    }
    else {
        display_lines = u8g2_GetDisplayHeight(u8g2) / line_height;
        u8sl.visible = display_lines;
    }

    u8sl.total = u8x8_GetStringLineCnt(list);
    u8sl.first_pos = 0;
    u8sl.current_pos = -1;

    u8g2_SetFontPosBaseline(u8g2);

    yy = u8g2_GetAscent(u8g2);
    if (title_lines > 0) {
        yy += u8g2_DrawUTF8Lines(u8g2, 0, yy, u8g2_GetDisplayWidth(u8g2), line_height, title);
        u8g2_DrawHLine(u8g2, 0, yy - line_height - u8g2_GetDescent(u8g2) + 1, u8g2_GetDisplayWidth(u8g2));
        yy += 3;
    }
    u8g2_DrawSelectionList(u8g2, &u8sl, yy, list);

    u8g2_SendBuffer(u8g2);
}

uint8_t display_UserInterfaceInputValueCB(u8g2_t *u8g2, const char *title, const char *prefix, uint8_t *value,
    uint8_t low, uint8_t high, uint8_t digits, const char *postfix,
    display_input_value_callback_t callback, void *user_data)
{
    // Based off u8g2_UserInterfaceInputValue() with changes to
    // invoke a callback on value change.

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
    u8g2_SetFontDirection(u8g2, 0);

    /* force baseline position */
    u8g2_SetFontPosBaseline(u8g2);

    /* calculate line height */
    line_height = u8g2_GetAscent(u8g2);
    line_height -= u8g2_GetDescent(u8g2);


    /* calculate overall height of the input value box */
    height = 1;   /* value input line */
    height += u8x8_GetStringLineCnt(title);

    /* calculate the height in pixel */
    pixel_height = height;
    pixel_height *= line_height;


    /* calculate offset from top */
    y = 0;
    if (pixel_height < u8g2_GetDisplayHeight(u8g2)) {
        y = u8g2_GetDisplayHeight(u8g2);
        y -= pixel_height;
        y /= 2;
    }

    /* calculate offset from left for the label */
    x = 0;
    pixel_width = u8g2_GetUTF8Width(u8g2, prefix);
    pixel_width += u8g2_GetUTF8Width(u8g2, "0") * digits;
    pixel_width += u8g2_GetUTF8Width(u8g2, postfix);
    if (pixel_width < u8g2_GetDisplayWidth(u8g2)) {
        x = u8g2_GetDisplayWidth(u8g2);
        x -= pixel_width;
        x /= 2;
    }

    /* event loop */
    for(;;) {
        u8g2_FirstPage(u8g2);
        do {
            /* render */
            yy = y;
            yy += u8g2_DrawUTF8Lines(u8g2, 0, yy, u8g2_GetDisplayWidth(u8g2), line_height, title);
            xx = x;
            xx += u8g2_DrawUTF8(u8g2, xx, yy, prefix);
            xx += u8g2_DrawUTF8(u8g2, xx, yy, u8x8_u8toa(local_value, digits));
            u8g2_DrawUTF8(u8g2, xx, yy, postfix);
        } while(u8g2_NextPage(u8g2));

        for(;;) {
            event = u8x8_GetMenuEvent(u8g2_GetU8x8(u8g2));
            if (event == U8X8_MSG_GPIO_MENU_SELECT) {
                *value = local_value;
                return 1;
            }
            else if (event == U8X8_MSG_GPIO_MENU_HOME) {
                return 0;
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
    //return r;
}

uint16_t display_UserInterfaceSelectionListCB(u8g2_t *u8g2, const char *title, uint8_t start_pos, const char *sl,
    display_GetMenuEvent_t event_callback, display_menu_params_t params)
{
    // Based off u8g2_UserInterfaceSelectionList() with changes to
    // support parameters for key event handling that allow for
    // multiple accept keys.

    u8sl_t u8sl;
    u8g2_uint_t yy;

    u8g2_uint_t line_height = u8g2_GetAscent(u8g2) - u8g2_GetDescent(u8g2) + MY_BORDER_SIZE;

    uint8_t title_lines = u8x8_GetStringLineCnt(title);
    uint8_t display_lines;


    if (start_pos > 0) {
        start_pos--;
    }

    if (title_lines > 0) {
        display_lines = (u8g2_GetDisplayHeight(u8g2) - 3) / line_height;
        u8sl.visible = display_lines;
        u8sl.visible -= title_lines;
    } else {
        display_lines = u8g2_GetDisplayHeight(u8g2) / line_height;
        u8sl.visible = display_lines;
    }

    u8sl.total = u8x8_GetStringLineCnt(sl);
    u8sl.first_pos = 0;
    u8sl.current_pos = start_pos;

    if (u8sl.current_pos >= u8sl.total) {
        u8sl.current_pos = u8sl.total - 1;
    }
    if (u8sl.first_pos+u8sl.visible <= u8sl.current_pos) {
        u8sl.first_pos = u8sl.current_pos-u8sl.visible + 1;
    }

    u8g2_SetFontPosBaseline(u8g2);

    for(;;) {
        u8g2_FirstPage(u8g2);
        do {
            yy = u8g2_GetAscent(u8g2);
            if (title_lines > 0) {
                yy += u8g2_DrawUTF8Lines(u8g2, 0, yy, u8g2_GetDisplayWidth(u8g2), line_height, title);

                u8g2_DrawHLine(u8g2, 0, yy-line_height- u8g2_GetDescent(u8g2) + 1, u8g2_GetDisplayWidth(u8g2));

                yy += 3;
            }
            u8g2_DrawSelectionList(u8g2, &u8sl, yy, sl);
        } while (u8g2_NextPage(u8g2));

        for(;;) {
            uint8_t event_action;
            uint8_t event_keycode;

            if (event_callback) {
                uint16_t result = event_callback(u8g2_GetU8x8(u8g2), params);
                if (result == UINT16_MAX) {
                    return result;
                }
                event_action = (uint8_t)(result & 0x00FF);
                event_keycode = (uint8_t)((result & 0xFF00) >> 8);
            }

            if (event_action == U8X8_MSG_GPIO_MENU_SELECT) {
                return ((uint16_t)event_keycode << 8) | (u8sl.current_pos + 1);
            }
            else if (event_action == U8X8_MSG_GPIO_MENU_HOME) {
                return 0;
            }
            else if (event_action == U8X8_MSG_GPIO_MENU_NEXT || event_action == U8X8_MSG_GPIO_MENU_DOWN) {
                u8sl_Next(&u8sl);
                break;
            }
            else if (event_action == U8X8_MSG_GPIO_MENU_PREV || event_action == U8X8_MSG_GPIO_MENU_UP) {
                u8sl_Prev(&u8sl);
                break;
            }
        }
    }
}
