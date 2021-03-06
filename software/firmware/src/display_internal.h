#ifndef DISPLAY_INTERNAL_H
#define DISPLAY_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>

#include "display.h"

extern bool menu_event_timeout;

/*
 * Custom menu result codes in the value range of 0..63,
 * as the built-in ones are offset by 64.
 */
#define U8X8_MSG_GPIO_MENU_INPUT_ASCII 1

/*
 * These functions are derived from various u8g2 library functions,
 * with changes made to better suit our integration needs.
 * They retain some of the naming conventions to more clearly
 * distinguish them.
 */

typedef uint16_t (*display_GetMenuEvent_t)(u8x8_t *u8x8, display_menu_params_t params);

void display_UserInterfaceStaticList(u8g2_t *u8g2, const char *title, const char *list);

void display_UserInterfaceStaticListDraw(u8g2_t *u8g2, const char *title, const char *list, u8g2_uint_t list_width);

uint8_t display_UserInterfaceInputValue(u8g2_t *u8g2, const char *title, const char *msg, const char *prefix, uint8_t *value,
    uint8_t low, uint8_t high, uint8_t digits, const char *postfix);

uint8_t display_UserInterfaceInputValueU16(u8g2_t *u8g2, const char *title, const char *msg, const char *prefix, uint16_t *value,
    uint16_t low, uint16_t high, uint8_t digits, const char *postfix);

uint8_t display_UserInterfaceInputValueF16(u8g2_t *u8g2, const char *title, const char *msg, const char *prefix, uint16_t *value,
    uint16_t low, uint16_t high, uint8_t wdigits, uint8_t fdigits, const char *postfix,
    display_GetMenuEvent_t event_callback, display_menu_params_t params,
    display_data_source_callback_t data_callback, void *user_data);

uint8_t display_UserInterfaceInputValueCB(u8g2_t *u8g2, const char *title, const char *msg, const char *prefix, uint8_t *value,
    uint8_t low, uint8_t high, uint8_t digits, const char *postfix,
    display_input_value_callback_t callback, void *user_data);

uint16_t display_UserInterfaceSelectionListCB(u8g2_t *u8g2, const char *title, uint8_t start_pos, const char *sl,
    display_GetMenuEvent_t event_callback, display_menu_params_t params);

const char *display_u16toa(uint16_t v, uint8_t d);

uint8_t display_DrawButtonLine(u8g2_t *u8g2, u8g2_uint_t y, u8g2_uint_t w, uint8_t cursor, const char *s);

#endif /* DISPLAY_INTERNAL_H */
