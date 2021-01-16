#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include "state_controller.h"

typedef enum {
    MENU_OK = 0,
    MENU_CANCEL = 1,
    MENU_TIMEOUT = 99
} menu_result_t;

menu_result_t main_menu_start(state_controller_t *controller);

#endif /* MAIN_MENU_H */
