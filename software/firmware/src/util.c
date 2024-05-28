#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include <usb_errno.h>

#include "display.h"
#include "enlarger_config.h"
#include "exposure_state.h"
#include "paper_profile.h"

static void convert_exposure_float_to_display_timer(display_exposure_timer_t *elements, float exposure_time);

void convert_exposure_to_display_printing(display_main_printing_elements_t *elements,
    const exposure_state_t *exposure, const enlarger_config_t *enlarger)
{
    const exposure_mode_t mode = exposure_get_mode(exposure);

    if (mode == EXPOSURE_MODE_PRINTING_BW) {
        elements->tone_graph = exposure_get_tone_graph(exposure);
        elements->tone_graph_overlay = 0;

        int paper_index = exposure_get_active_paper_profile_index(exposure);
        if (paper_index >= 0) {
            elements->paper_profile_num = paper_index + 1;
        } else {
            elements->paper_profile_num = 0;
        }
    } else {
        elements->tone_graph = 0;
        elements->tone_graph_overlay = 0;
        elements->paper_profile_num = 0;
    }

    elements->burn_dodge_count = exposure_burn_dodge_count(exposure);

    if (mode == EXPOSURE_MODE_PRINTING_BW) {
        /* Set B&W printing elements */
        elements->printing_type = DISPLAY_MAIN_PRINTING_BW;
        elements->bw.contrast_grade = exposure_get_contrast_grade(exposure);
        elements->bw.contrast_note = contrast_filter_grade_str(
            (enlarger->control.dmx_control ? CONTRAST_FILTER_REGULAR : enlarger->contrast_filter),
            exposure_get_contrast_grade(exposure));
        elements->time_icon_highlight = false;
    } else if (mode == EXPOSURE_MODE_PRINTING_COLOR) {
        /* Set color printing elements */
        elements->printing_type = DISPLAY_MAIN_PRINTING_COLOR;
        elements->color.ch_labels[0] = 'R';
        elements->color.ch_labels[1] = 'G';
        elements->color.ch_labels[2] = 'B';
        elements->color.ch_values[0] = exposure_get_channel_value(exposure, 0);
        elements->color.ch_values[1] = exposure_get_channel_value(exposure, 1);
        elements->color.ch_values[2] = exposure_get_channel_value(exposure, 2);
        elements->color.ch_wide = exposure_get_channel_wide_mode(exposure);
    }

    float exposure_time = exposure_get_exposure_time(exposure);
    convert_exposure_float_to_display_timer(&(elements->time_elements), exposure_time);

    /* Check for minimum exposure time */
    float min_exposure_time = exposure_get_min_exposure_time(exposure);
    if ((min_exposure_time > 0) && (exposure_time < min_exposure_time)) {
        elements->time_icon = DISPLAY_MAIN_PRINTING_TIME_ICON_INVALID;
    } else if (mode == EXPOSURE_MODE_PRINTING_COLOR) {
        elements->time_icon = DISPLAY_MAIN_PRINTING_TIME_ICON_NORMAL;
    } else {
        elements->time_icon = DISPLAY_MAIN_PRINTING_TIME_ICON_NONE;
    }
}

void convert_exposure_to_display_densitometer(display_main_densitometer_elements_t *elements, const exposure_state_t *exposure)
{
    float density = exposure_get_relative_density(exposure);
    if (isnanf(density)) {
        elements->density_whole = UINT16_MAX;
        elements->density_fractional = UINT16_MAX;
        elements->fraction_digits = UINT8_MAX;
    } else {
        float whole;
        float fractional;
        fractional = modff(density, &whole);
        elements->density_whole = whole;
        elements->density_fractional = round_to_10(lroundf(fractional * 1000.0f));
        elements->fraction_digits = 2;
    }
}

void convert_exposure_to_display_calibration(display_main_calibration_elements_t *elements, const exposure_state_t *exposure)
{
    elements->cal_title1 = "Print";
    elements->cal_title2 = "Exposure";
    elements->cal_value = exposure_get_calibration_pev(exposure);

    float exposure_time = exposure_get_exposure_time(exposure);
    convert_exposure_float_to_display_timer(&(elements->time_elements), exposure_time);

    float min_exposure_time = exposure_get_min_exposure_time(exposure);
    elements->time_too_short = (min_exposure_time > 0) && (exposure_time < min_exposure_time);
}

void convert_exposure_float_to_display_timer(display_exposure_timer_t *elements, float exposure_time)
{
    float seconds;
    float fractional;
    fractional = modff(exposure_time, &seconds);
    elements->time_seconds = seconds;
    elements->time_milliseconds = round_to_10(lroundf(fractional * 1000.0f));

    if (exposure_time < 10) {
        elements->fraction_digits = 2;

    } else if (exposure_time < 100) {
        elements->fraction_digits = 1;
    } else {
        elements->fraction_digits = 0;
    }
}

void convert_exposure_to_display_timer(display_exposure_timer_t *elements, uint32_t exposure_ms)
{
    update_display_timer(elements, exposure_ms);

    if (exposure_ms < 10000) {
        elements->fraction_digits = 2;

    } else if (exposure_ms < 100000) {
        elements->fraction_digits = 1;
    } else {
        elements->fraction_digits = 0;
    }
}

void update_display_timer(display_exposure_timer_t *elements, uint32_t exposure_ms)
{
    elements->time_seconds = exposure_ms / 1000;
    elements->time_milliseconds = round_to_10(exposure_ms % 1000);
}

uint32_t round_to_10(uint32_t n)
{
    uint32_t a = (n / 10) * 10;
    uint32_t b = a + 10;
    return (n - a > b - n) ? b : a;
}

uint32_t rounded_exposure_time_ms(float seconds)
{
    uint32_t milliseconds = lroundf(seconds * 1000.0f);
    if (milliseconds > 1000000) {
        milliseconds = 1000000;
    }
    milliseconds = round_to_10(milliseconds);
    return milliseconds;
}

size_t pad_str_to_length(char *str, char c, size_t length)
{
    if (!str) {
        return 0;
    }

    size_t i = strlen(str);
    if (i >= length) {
        return i;
    }

    while (i <= length) {
        str[i++] = c;
    }
    str[i] = '\0';

    return i - 1;
}

size_t append_signed_fraction(char *str, int8_t numerator, uint8_t denominator)
{
    size_t count = 0;
    bool num_positive = numerator >= 0;
    char val_sign = num_positive ? '+' : '-';
    uint8_t val_num = abs(numerator);
    uint8_t val_whole = val_num / denominator;
    val_num -= val_whole * denominator;

    if (!str) {
        return count;
    }

    if (val_whole != 0 || (val_whole == 0 && val_num == 0)) {
        if (val_num > 0) {
            // Whole with fraction
            count = sprintf(str, "%c%d-%d/%d",
                val_sign, val_whole, val_num, denominator);
        } else {
            // Whole without fraction
            count = sprintf(str, "%c%d",
                val_sign, val_whole);
        }
    } else {
        // Just the fraction
        count = sprintf(str, "%c%d/%d",
            val_sign, val_num, denominator);
    }
    return count;
}

size_t append_exposure_time(char *str, float time)
{
    size_t count = 0;
    uint32_t exposure_ms = rounded_exposure_time_ms(time);
    uint16_t display_seconds = exposure_ms / 1000;
    uint16_t display_milliseconds = round_to_10(exposure_ms % 1000);

    if (exposure_ms < 10000) {
        count = sprintf(str, "%01d.%02ds", display_seconds, display_milliseconds / 10);
    } else if (exposure_ms < 100000) {
        count = sprintf(str, "%d.%01ds", display_seconds, display_milliseconds / 100);
    } else {
        count = sprintf(str, "%ds", display_seconds);
    }
    return count;
}

float interpolate(float x1, float y1, float x2, float y2, float x3, float y3, float x)
{
    return (y1 * ((x - x2) / (x1 - x2)) * ((x - x3) / (x1 - x3))) +
        (y2 * ((x - x1) / (x2 - x1)) * ((x - x3) / (x2 - x3))) +
        (y3 * ((x - x1) / (x3 - x1)) * ((x - x2) / (x3 - x2)));
}

bool is_valid_number(float num)
{
    return isnormal(num) || fpclassify(num) == FP_ZERO;
}

uint16_t conv_array_u16(const uint8_t *src)
{
    return (uint16_t)(*src) << 8 | *(src + 1);
}

void conv_u16_array(uint8_t *dst, uint16_t src)
{
    *dst = (uint8_t)((src & 0xFF00) >> 8);
    *(dst + 1) = (uint8_t)(src & 0x00FF);
}

uint8_t value_adjust_with_rollover_u8(uint8_t value, int8_t increment, uint8_t lower_bound, uint8_t upper_bound)
{
    uint32_t temp_value = value - lower_bound;
    uint32_t temp_upper = upper_bound - lower_bound;

    if (lower_bound > upper_bound) {
        return value;
    } else if (value < lower_bound) {
        return lower_bound;
    } else if (value > upper_bound) {
        return upper_bound;
    } else if (increment > 0) {
        temp_value = (temp_value + increment) % (temp_upper + 1);
        return (uint8_t)(temp_value + lower_bound);
    } else if (increment < 0) {
        temp_value = (temp_value + ((temp_upper + 1) + increment)) % (temp_upper + 1);
        return (uint8_t)(temp_value + lower_bound);
    } else {
        return value;
    }
}

uint16_t value_adjust_with_rollover_u16(uint16_t value, int16_t increment, uint16_t lower_bound, uint16_t upper_bound)
{
    uint32_t temp_value = value - lower_bound;
    uint32_t temp_upper = upper_bound - lower_bound;

    if (lower_bound > upper_bound) {
        return value;
    } else if (value < lower_bound) {
        return lower_bound;
    } else if (value > upper_bound) {
        return upper_bound;
    } else if (increment > 0) {
        temp_value = (temp_value + increment) % (temp_upper + 1);
        return (uint16_t)(temp_value + lower_bound);
    } else if (increment < 0) {
        temp_value = (temp_value + ((temp_upper + 1) + increment)) % (temp_upper + 1);
        return (uint16_t)(temp_value + lower_bound);
    } else {
        return value;
    }
}

osStatus_t hal_to_os_status(HAL_StatusTypeDef hal_status)
{
    switch (hal_status) {
    case HAL_OK:
        return osOK;
    case HAL_ERROR:
        return osError;
    case HAL_BUSY:
        return osErrorResource;
    case HAL_TIMEOUT:
        return osErrorTimeout;
    default:
        return osError;
    }
}

HAL_StatusTypeDef os_to_hal_status(osStatus_t os_status)
{
    switch (os_status) {
    case osOK:
        return HAL_OK;
    case osError:
        return HAL_ERROR;
    case osErrorTimeout:
        return HAL_TIMEOUT;
    case osErrorResource:
        return HAL_BUSY;
    case osErrorParameter:
    case osErrorNoMemory:
    case osErrorISR:
    default:
        return HAL_ERROR;
    }
}

HAL_StatusTypeDef usb_to_hal_status(int usb_status)
{
    /*
     * HAL does not provide many error code options, so it is better
     * to directly use the USB error codes if more specific behavior
     * or useful logging matters.
     */
    if (usb_status >= 0) {
        return HAL_OK;
    } else if (usb_status == -USB_ERR_BUSY) {
        return HAL_BUSY;
    } else if (usb_status == -USB_ERR_TIMEOUT) {
        return HAL_TIMEOUT;
    } else {
        return HAL_ERROR;
    }
}

osStatus_t usb_to_os_status(int usb_status)
{
    /*
     * There is a lot of guesswork in this translation, so its better
     * to directly use the USB error codes if more specific behavior
     * or useful logging matters.
     */
    if (usb_status >= 0) {
        return osOK;
    } else {
        switch (usb_status) {
        case -USB_ERR_TIMEOUT:
            return osErrorTimeout;
        case -USB_ERR_BUSY:
        case -USB_ERR_NODEV:
        case -USB_ERR_NOTCONN:
            return osErrorResource;
        case -USB_ERR_INVAL:
        case -USB_ERR_NOTSUPP:
        case -USB_ERR_RANGE:
            return osErrorParameter;
        case -USB_ERR_NOMEM:
            return osErrorNoMemory;
        default:
            return osError;
        }
    }
}

static const char* const VALID_NONALPHA_CHARS = "!#$%&'()-@^_`{}~+,;=[]";

bool scrub_export_filename(char *filename, const char *ext)
{
    char buf[32];
    size_t len;
    char ch;
    char *p;
    bool changed = false;

    /* Copy the filename to a working buffer, keeping only legal characters */
    len = MIN(strlen(filename), 30);
    memset(buf, 0, sizeof(buf));
    p = buf;
    for (size_t i = 0; i < len; i++) {
        ch = filename[i];
        if (isalnum(ch) || ch == ' ' || ch == '.' || ch > 0x80
            || strchr(VALID_NONALPHA_CHARS, ch)) {
            *p = ch;
            p++;
        }
    }

    /* Trim any trailing dots or spaces */
    while (p > buf && (*(p - 1) == '.' || *(p - 1) == ' ')) { p--; *p = '\0'; }

    /* Strip the file extension */
    p = strrchr(buf, '.');
    if (p && p > buf) {
        *p = '\0';
    }

    /* Truncate the name */
    buf[31] = '\0';
    buf[30] = '\0';
    buf[29] = '\0';
    buf[28] = '\0';
    buf[27] = '\0';
    buf[26] = '\0';

    /* Make sure the file ends in the correct extension */
    strcat(buf, ext);

    /* If the file actually changed, copy back and take note */
    if (strcmp(filename, buf) != 0) {
        strcpy(filename, buf);
        changed = true;
    }

    return changed;
}
