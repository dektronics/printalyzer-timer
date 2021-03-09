#ifndef MAIN_MENU_H
#define MAIN_MENU_H

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

#endif /* MAIN_MENU_H */
