#ifndef DISPLAY_H
#define DISPLAY_H

#include <stm32f4xx_hal.h>
#include <stdbool.h>

#include "u8g2_stm32_hal.h"

HAL_StatusTypeDef display_init(const u8g2_display_handle_t *display_handle);

void display_clear();
void display_enable(bool enabled);
void display_draw_test_pattern();

void display_graphic_insert_thumbdrive();
void display_graphic_checking_thumbdrive();
void display_graphic_update_prompt();
void display_graphic_unlock_prompt();
void display_graphic_update_progress();
void display_graphic_update_progress_increment(uint8_t value);
void display_graphic_update_progress_success();
void display_graphic_update_progress_failure();
void display_graphic_restart();
void display_graphic_failure();

#endif /* DISPLAY_H */
