#ifndef MENU_SETTINGS_H
#define MENU_SETTINGS_H

#include "main_menu.h"

menu_result_t menu_settings();

menu_result_t menu_settings_test_strip_step_size(const char *title, exposure_adjustment_increment_t *setting);

#endif /* MENU_SETTINGS_H */
