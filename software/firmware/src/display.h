#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "u8g2_stm32_hal.h"

typedef enum {
    DISPLAY_GRADE_00,
    DISPLAY_GRADE_0,
    DISPLAY_GRADE_0_HALF,
    DISPLAY_GRADE_1,
    DISPLAY_GRADE_1_HALF,
    DISPLAY_GRADE_2,
    DISPLAY_GRADE_2_HALF,
    DISPLAY_GRADE_3,
    DISPLAY_GRADE_3_HALF,
    DISPLAY_GRADE_4,
    DISPLAY_GRADE_4_HALF,
    DISPLAY_GRADE_5,
    DISPLAY_GRADE_NONE
} display_grade_t;

#define DISPLAY_TONE_UNDER 0x10000
#define DISPLAY_TONE_OVER  0x00001
#define DISPLAY_TONE_ELEMENT(x) ((x < 0) ? (0x100 << (-1 * x)) : (0x100 >> x))

typedef struct {
    uint32_t tone_graph;
    display_grade_t contrast_grade;
    uint16_t time_seconds;
    uint16_t time_milliseconds;
    uint8_t fraction_digits;
} display_main_elements_t;

typedef struct {
    uint16_t time_seconds;
    uint16_t time_milliseconds;
    uint8_t fraction_digits;
} display_exposure_timer_t;

HAL_StatusTypeDef display_init(const u8g2_display_handle_t *display_handle);

void display_clear();
void display_enable(bool enabled);
void display_set_contrast(uint8_t value);
uint8_t display_get_contrast();
void display_set_brightness(uint8_t value);
uint8_t display_get_brightness();

void display_draw_test_pattern(bool mode);
void display_draw_logo();

void display_draw_main_elements(const display_main_elements_t *elements);
void display_draw_stop_increment(uint8_t increment_den);
void display_draw_exposure_timer(const display_exposure_timer_t *elements, const display_exposure_timer_t *prev_elements);

uint8_t display_selection_list(const char *title, uint8_t start_pos, const char *list);
void display_static_list(const char *title, const char *list);
uint8_t display_message(const char *title1, const char *title2, const char *title3, const char *buttons);

#endif /* DISPLAY_H */
