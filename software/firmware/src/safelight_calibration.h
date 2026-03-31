#ifndef SAFELIGHT_CALIBRATION_H
#define SAFELIGHT_CALIBRATION_H

#include "main_menu.h"

typedef struct __safelight_config_t safelight_config_t;

menu_result_t menu_safelight_calibration(safelight_config_t *config);

void menu_safelight_test_enable(const safelight_config_t *config, bool enable);
void menu_safelight_test_toggle(const safelight_config_t *config, bool on);

#endif /* SAFELIGHT_CALIBRATION_H */
