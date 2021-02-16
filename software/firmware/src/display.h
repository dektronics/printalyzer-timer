#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "u8g2_stm32_hal.h"

typedef enum {
    DISPLAY_MENU_ACCEPT_MENU = 0x01,
    DISPLAY_MENU_ACCEPT_ADD_ADJUSTMENT = 0x02,
    DISPLAY_MENU_ACCEPT_TEST_STRIP = 0x04,
    DISPLAY_MENU_ACCEPT_ENCODER = 0x08,
    DISPLAY_MENU_TIMEOUT_DISABLED = 0x10,
    DISPLAY_MENU_INPUT_ASCII = 0x20,
    DISPLAY_MENU_INPUT_POLL = 0x40
} display_menu_params_t;

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
    DISPLAY_GRADE_NONE,
    DISPLAY_GRADE_MAX
} display_grade_t;

typedef enum {
    DISPLAY_PATCHES_5,
    DISPLAY_PATCHES_7
} display_patches_t;

#define DISPLAY_MENU_ROW_LENGTH 32
#define DISPLAY_HALF_ROW_LENGTH 16
#define DISPLAY_TONE_UNDER 0x10000
#define DISPLAY_TONE_OVER  0x00001
#define DISPLAY_TONE_ELEMENT(x) ((x < 0) ? (0x100 << (-1 * x)) : (0x100 >> x))

typedef struct {
    uint32_t tone_graph;
    uint8_t paper_profile_num;
    uint8_t burn_dodge_count;
    display_grade_t contrast_grade;
    char *cal_title1;
    char *cal_title2;
    uint16_t cal_value;
    uint16_t time_seconds;
    uint16_t time_milliseconds;
    uint8_t fraction_digits;
} display_main_elements_t;

typedef struct {
    uint16_t time_seconds;
    uint16_t time_milliseconds;
    uint8_t fraction_digits;
} display_exposure_timer_t;

typedef struct {
    display_patches_t patches;
    uint8_t covered_patches;
    uint16_t patch_cal_values[7];
    const char *title1;
    const char *title2;
    display_exposure_timer_t time_elements;
} display_test_strip_elements_t;

typedef struct {
    const char *adj_title1;
    const char *adj_title2;
    const char *base_title1;
    const char *base_title2;
    const char *tip_up;
    const char *tip_down;
    int16_t adj_num;
    uint16_t adj_den;
} display_edit_adjustment_elements_t;

typedef struct {
    const char *title;
    uint8_t burn_dodge_index;
    display_grade_t contrast_grade;
    display_exposure_timer_t time_elements;
} display_adjustment_exposure_elements_t;

typedef void (*display_input_value_callback_t)(uint8_t value, void *user_data);
typedef uint16_t (*display_data_source_callback_t)(void *user_data);

HAL_StatusTypeDef display_init(const u8g2_display_handle_t *display_handle);

void display_clear();
void display_enable(bool enabled);
void display_set_contrast(uint8_t value);
uint8_t display_get_contrast();
void display_set_brightness(uint8_t value);
uint8_t display_get_brightness();

void display_save_screenshot();

void display_draw_test_pattern(bool mode);
void display_draw_logo();

void display_draw_main_elements(const display_main_elements_t *elements);
void display_draw_stop_increment(uint8_t increment_den);
void display_draw_mode_text(const char *text);
void display_draw_exposure_adj(int value);
void display_draw_timer_adj(const display_exposure_timer_t *elements);
void display_draw_exposure_timer(const display_exposure_timer_t *elements, const display_exposure_timer_t *prev_elements);

/**
 * Draw the complete set of burn/dodge display elements.
 */
void display_draw_adjustment_exposure_elements(const display_adjustment_exposure_elements_t *elements);

/**
 * Redraw the timer component of the burn/dodge display elements.
 */
void display_draw_adjustment_exposure_timer(const display_exposure_timer_t *time_elements);

/**
 * Draw the complete set of test strip display elements.
 */
void display_draw_test_strip_elements(const display_test_strip_elements_t *elements);

/**
 * Redraw the timer component of the test strip elements.
 * This function assumes the test strip elements are already
 * visible on the display, and thus skips a lot of normal setup.
 */
void display_draw_test_strip_timer(const display_exposure_timer_t *elements);

void display_draw_edit_adjustment_elements(const display_edit_adjustment_elements_t *elements);

uint8_t display_selection_list(const char *title, uint8_t start_pos, const char *list);

/**
 * Display a list of scrollable and selectable options.
 * The user can select one of the options.
 *
 * @param title The title shown at the top of the list.
 * @param start_pos The element, which is highlighted first (starts with 1).
 * @param list List of options, one per line (Lines have to be separated with \n).
 * @param params Bit field with various parameters for the menu behavior.
 * @return The lower 8 bits contain 1 to n for the item that has been selected.
 *         The upper 8 bits contain the keycode of the button that was used to
 *         accept the item.
 *         If the the menu timed out, then UINT16_MAX is returned.
 *         If the user pressed the home/cancel button, then 0 is returned.
 */
uint16_t display_selection_list_params(const char *title, uint8_t start_pos, const char *list,
    display_menu_params_t params);

/**
 * Display a list of items, selectable buttons, and a 2D graph of data.
 *
 * The title and list formats is the same as a regular selection or static
 * list, however the item width cuts off at around 15 characters.
 *
 * The button format is similar to the message display, albeit with
 * the actual button space far more constrained.
 *
 * The graph is a simple 2D line, with X values ranging from 0 to 126
 * and Y values ranging from 0 to 50. Any values in excess of these
 * ranges may be clipped or not drawn.
 */
uint8_t display_message_graph(const char *title, const char *list, const char *buttons, uint8_t *graph_points, size_t graph_size);

void display_static_list(const char *title, const char *list);
uint8_t display_message(const char *title1, const char *title2, const char *title3, const char *buttons);
uint8_t display_input_value(const char *title, const char *prefix, uint8_t *value,
        uint8_t low, uint8_t high, uint8_t digits, const char *postfix);
uint8_t display_input_value_u16(const char *title, const char *prefix, uint16_t *value,
        uint16_t low, uint16_t high, uint8_t digits, const char *postfix);
uint8_t display_input_value_f16(const char *title, const char *prefix, uint16_t *value,
        uint16_t low, uint16_t high, uint8_t wdigits, uint8_t fdigits, const char *postfix);
uint8_t display_input_value_f16_data_cb(const char *title, const char *prefix, uint16_t *value,
        uint16_t low, uint16_t high, uint8_t wdigits, uint8_t fdigits, const char *postfix,
        display_data_source_callback_t data_callback, void *user_data);
uint8_t display_input_value_cb(const char *title, const char *prefix, uint8_t *value,
        uint8_t low, uint8_t high, uint8_t digits, const char *postfix,
        display_input_value_callback_t callback, void *user_data);

uint8_t display_input_text(const char *title, char *text, size_t text_len);

#endif /* DISPLAY_H */
