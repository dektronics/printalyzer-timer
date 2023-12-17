cmake_minimum_required(VERSION 3.20)

add_library(u8g2
    src/u8x8_8x8.c
    src/u8x8_byte.c
    src/u8x8_cad.c
    src/u8x8_capture.c
    src/u8x8_debounce.c
    src/u8x8_display.c
    src/u8x8_d_ssd1322.c
    src/u8x8_fonts.c
    src/u8x8_gpio.c
    src/u8x8_input_value.c
    src/u8x8_message.c
    src/u8x8_selection_list.c
    src/u8x8_setup.c
    src/u8x8_string.c
    src/u8x8_u16toa.c
    src/u8x8_u8toa.c
    src/u8g2_bitmap.c
    src/u8g2_box.c
    src/u8g2_buffer.c
    src/u8g2_button.c
    src/u8g2_circle.c
    src/u8g2_cleardisplay.c
    src/u8g2_d_memory.c
    src/u8g2_d_setup.c
    src/u8g2_font.c
    src/u8g2_fonts.c
    src/u8g2_hvline.c
    src/u8g2_input_value.c
    src/u8g2_intersection.c
    src/u8g2_kerning.c
    src/u8g2_line.c
    src/u8g2_ll_hvline.c
    src/u8g2_message.c
    src/u8g2_polygon.c
    src/u8g2_selection_list.c
    src/u8g2_setup.c
    src/u8log.c
    src/u8log_u8g2.c
    src/u8log_u8x8.c
)

target_include_directories(u8g2 PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
