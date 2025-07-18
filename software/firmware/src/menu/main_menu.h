#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include <stddef.h>

#include "state_controller.h"

typedef enum {
    MENU_OK = 0,
    MENU_CANCEL = 1,
    MENU_SAVE = 3,
    MENU_DELETE = 4,
    MENU_TIMEOUT = 99
} menu_result_t;

menu_result_t main_menu_start(state_controller_t *controller);
menu_result_t menu_confirm_cancel(const char *title);

/**
 * Build a full-length padded menu text row.
 *
 * This function assumes that the provided buffer is at least
 * DISPLAY_MENU_ROW_LENGTH+2 in length, to accommodate a line
 * break character and a null terminator.
 */
size_t menu_build_padded_str_row(char *buf, const char *label, const char *value);

size_t menu_build_padded_format_row(char *buf, const char *label, const char *format, ...);

#endif /* MAIN_MENU_H */
