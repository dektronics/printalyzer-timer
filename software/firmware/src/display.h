#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "u8g2_stm32_hal.h"
#include "contrast.h"

#define MENU_TIMEOUT_MS 30000

typedef enum {
    DISPLAY_MENU_ACCEPT_MENU = 0x0001,
    DISPLAY_MENU_ACCEPT_ADD_ADJUSTMENT = 0x0002,
    DISPLAY_MENU_ACCEPT_TEST_STRIP = 0x0004,
    DISPLAY_MENU_ACCEPT_ENCODER = 0x0008,
    DISPLAY_MENU_ACCEPT_PROBE = 0x0010,
    DISPLAY_MENU_ACCEPT_STICK = 0x0020,
    DISPLAY_MENU_TIMEOUT_DISABLED = 0x0100,
    DISPLAY_MENU_INPUT_ASCII = 0x0200,
    DISPLAY_MENU_INPUT_POLL = 0x0400
} display_menu_params_t;

typedef enum {
    DISPLAY_PATCHES_5,
    DISPLAY_PATCHES_7
} display_patches_t;

#define DISPLAY_MENU_ROW_LENGTH 32
#define DISPLAY_HALF_ROW_LENGTH 16

typedef struct __display_exposure_timer_t {
    uint16_t time_seconds;
    uint16_t time_milliseconds;
    uint8_t fraction_digits;
} display_exposure_timer_t;

typedef enum __display_main_printing_type_t {
    DISPLAY_MAIN_PRINTING_BW = 0,
    DISPLAY_MAIN_PRINTING_COLOR
} display_main_printing_type_t;

typedef struct __display_main_printing_bw_t {
    contrast_grade_t contrast_grade;
    const char *contrast_note;
} display_main_printing_bw_t;

typedef struct __display_main_printing_color_t {
    char ch_labels[3];
    uint16_t ch_values[3];
    uint8_t ch_highlight; /*<! 0 = no highlight, [1..3] = label highlight, 4 = all label highlight */
    bool ch_wide; /*< true for 0-65535, false for 0-255 */
} display_main_printing_color_t;

typedef enum __display_main_printing_time_icon_t {
    DISPLAY_MAIN_PRINTING_TIME_ICON_NONE = 0,
    DISPLAY_MAIN_PRINTING_TIME_ICON_INVALID,
    DISPLAY_MAIN_PRINTING_TIME_ICON_NORMAL
} display_main_printing_time_icon_t;

typedef struct __display_main_printing_elements_t {
    uint32_t tone_graph;
    uint32_t tone_graph_overlay;
    uint8_t paper_profile_num;
    uint8_t burn_dodge_count;
    display_exposure_timer_t time_elements;
    display_main_printing_time_icon_t time_icon;
    bool time_icon_highlight;
    display_main_printing_type_t printing_type;
    union {
        display_main_printing_bw_t bw;
        display_main_printing_color_t color;
    };
} display_main_printing_elements_t;

typedef struct __display_main_densitometer_elements_t {
    uint16_t density_whole;
    uint16_t density_fractional;
    uint8_t fraction_digits;
    uint8_t show_ind_probe; /* 0 = hidden, 1 = shown, 2 = highlighted */
    uint8_t show_ind_dens; /* 0 = hidden, 1 = shown, 2 = highlighted */
    uint8_t ind_dens; /* 0 = reflection, 1 = transmission */
} display_main_densitometer_elements_t;

typedef struct __display_main_calibration_elements_t {
    char *cal_title1;
    char *cal_title2;
    uint16_t cal_value;
    display_exposure_timer_t time_elements;
    bool time_too_short;
} display_main_calibration_elements_t;

typedef struct {
    display_patches_t patches;
    uint8_t covered_patches;
    uint8_t invalid_patches;
    uint16_t patch_cal_values[7];
    const char *title1;
    const char *title2;
    display_exposure_timer_t time_elements;
} display_test_strip_elements_t;

typedef struct {
    uint32_t base_tone_graph;
    uint32_t adj_tone_graph;
    const char *adj_title1;
    const char *adj_title2;
    const char *base_title1;
    const char *base_title2;
    const char *tip_up;
    const char *tip_down;
    int16_t adj_num;
    uint16_t adj_den;
    bool time_too_short;
} display_edit_adjustment_elements_t;

typedef struct {
    const char *title;
    uint8_t burn_dodge_index;
    contrast_grade_t contrast_grade;
    const char *contrast_note;
    display_exposure_timer_t time_elements;
    bool time_too_short;
} display_adjustment_exposure_elements_t;

typedef void (*display_input_value_callback_t)(uint8_t value, void *user_data);
typedef uint16_t (*display_data_source_callback_t)(uint8_t event_action, void *user_data);

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

void display_draw_main_elements_printing(const display_main_printing_elements_t *elements);
void display_draw_main_elements_densitometer(const display_main_densitometer_elements_t *elements);
void display_draw_main_elements_calibration(const display_main_calibration_elements_t *elements);

void display_draw_stop_increment(uint8_t increment_den);
void display_draw_mode_text(const char *text);
void display_draw_exposure_adj(int value, uint32_t tone_graph);
void display_draw_timer_adj(const display_exposure_timer_t *elements, uint32_t tone_graph);
void display_draw_exposure_timer(const display_exposure_timer_t *elements, const display_exposure_timer_t *prev_elements);

/**
 * Redraw the tone graph portion of the display.
 *
 * Note: This will not clear any existing display contents.
 */
void display_redraw_tone_graph(uint32_t tone_graph, uint32_t overlay_marks);

/**
 * Draw the complete set of burn/dodge display elements.
 */
void display_draw_adjustment_exposure_elements(const display_adjustment_exposure_elements_t *elements);

/**
 * Redraw the timer component of the burn/dodge display elements.
 */
void display_redraw_adjustment_exposure_timer(const display_exposure_timer_t *time_elements);

/**
 * Draw the complete set of test strip display elements.
 */
void display_draw_test_strip_elements(const display_test_strip_elements_t *elements);

/**
 * Redraw the timer component of the test strip elements.
 * This function assumes the test strip elements are already
 * visible on the display, and thus skips a lot of normal setup.
 */
void display_redraw_test_strip_timer(const display_exposure_timer_t *elements);

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
uint8_t display_message_graph(const char *title, const char *list, const char *buttons, const uint8_t *graph_points, size_t graph_size);

void display_static_list(const char *title, const char *list);
uint8_t display_message(const char *title1, const char *title2, const char *title3, const char *buttons);
uint8_t display_message_params(const char *title1, const char *title2, const char *title3, const char *buttons,
    display_menu_params_t params);
uint8_t display_input_value(const char *title, const char *msg, const char *prefix, uint8_t *value,
        uint8_t low, uint8_t high, uint8_t digits, const char *postfix);
uint8_t display_input_value_u16(const char *title, const char *msg, const char *prefix, uint16_t *value,
        uint16_t low, uint16_t high, uint8_t digits, const char *postfix);
uint8_t display_input_value_f1_2(const char *title, const char *prefix, uint16_t *value,
    uint16_t low, uint16_t high, char sep, const char *postfix);
uint8_t display_input_value_f16(const char *title, const char *msg, const char *prefix, uint16_t *value,
        uint16_t low, uint16_t high, uint8_t wdigits, uint8_t fdigits, const char *postfix);
uint8_t display_input_value_f16_data_cb(const char *title, const char *msg, const char *prefix, uint16_t *value,
        uint16_t low, uint16_t high, uint8_t wdigits, uint8_t fdigits, const char *postfix,
        display_data_source_callback_t data_callback, void *user_data);
uint8_t display_input_value_cb(const char *title, const char *msg, const char *prefix, uint8_t *value,
        uint8_t low, uint8_t high, uint8_t digits, const char *postfix,
        display_input_value_callback_t callback, void *user_data);

uint8_t display_input_text(const char *title, char *text, size_t text_len);

#endif /* DISPLAY_H */
