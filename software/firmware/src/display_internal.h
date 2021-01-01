#ifndef DISPLAY_INTERNAL_H
#define DISPLAY_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>

#include "display.h"

extern bool menu_event_timeout;

/*
 * These functions are derived from various u8g2 library functions,
 * with changes made to better suit our integration needs.
 * They retain some of the naming conventions to more clearly
 * distinguish them.
 */

typedef uint16_t (*display_GetMenuEvent_t)(u8x8_t *u8x8, display_menu_params_t params);

void display_UserInterfaceStaticList(u8g2_t *u8g2, const char *title, const char *list);

uint8_t display_UserInterfaceInputValueCB(u8g2_t *u8g2, const char *title, const char *prefix, uint8_t *value,
    uint8_t low, uint8_t high, uint8_t digits, const char *postfix,
    display_input_value_callback_t callback, void *user_data);

uint16_t display_UserInterfaceSelectionListCB(u8g2_t *u8g2, const char *title, uint8_t start_pos, const char *sl,
    display_GetMenuEvent_t event_callback, display_menu_params_t params);

#endif /* DISPLAY_INTERNAL_H */
