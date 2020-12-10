#ifndef MAIN_MENU_H
#define MAIN_MENU_H

typedef enum {
    MENU_OK = 0,
    MENU_CANCEL = 1,
    MENU_TIMEOUT = 99
} menu_result_t;

menu_result_t main_menu_start();

#endif /* MAIN_MENU_H */
