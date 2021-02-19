#ifndef DISPLAY_H
#define DISPLAY_H

#include <stm32f4xx_hal.h>
#include <stdbool.h>

#include "u8g2_stm32_hal.h"

HAL_StatusTypeDef display_init(const u8g2_display_handle_t *display_handle);

void display_clear();
void display_enable(bool enabled);
void display_draw_test_pattern();

void display_static_message(const char *message);

#endif /* DISPLAY_H */
