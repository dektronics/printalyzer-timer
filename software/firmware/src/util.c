#include "util.h"

#include <math.h>

void convert_exposure_to_display(display_main_elements_t *elements, const exposure_state_t *exposure)
{
    //TODO Fix this once the exposure state contains tone graph data
    elements->tone_graph = 0;

    switch (exposure->contrast_grade) {
    case CONTRAST_GRADE_00:
        elements->contrast_grade = DISPLAY_GRADE_00;
        break;
    case CONTRAST_GRADE_0:
        elements->contrast_grade = DISPLAY_GRADE_0;
        break;
    case CONTRAST_GRADE_0_HALF:
        elements->contrast_grade = DISPLAY_GRADE_0_HALF;
        break;
    case CONTRAST_GRADE_1:
        elements->contrast_grade = DISPLAY_GRADE_1;
        break;
    case CONTRAST_GRADE_1_HALF:
        elements->contrast_grade = DISPLAY_GRADE_1_HALF;
        break;
    case CONTRAST_GRADE_2:
        elements->contrast_grade = DISPLAY_GRADE_2;
        break;
    case CONTRAST_GRADE_2_HALF:
        elements->contrast_grade = DISPLAY_GRADE_2_HALF;
        break;
    case CONTRAST_GRADE_3:
        elements->contrast_grade = DISPLAY_GRADE_3;
        break;
    case CONTRAST_GRADE_3_HALF:
        elements->contrast_grade = DISPLAY_GRADE_3_HALF;
        break;
    case CONTRAST_GRADE_4:
        elements->contrast_grade = DISPLAY_GRADE_4;
        break;
    case CONTRAST_GRADE_4_HALF:
        elements->contrast_grade = DISPLAY_GRADE_4_HALF;
        break;
    case CONTRAST_GRADE_5:
        elements->contrast_grade = DISPLAY_GRADE_5;
        break;
    default:
        elements->contrast_grade = DISPLAY_GRADE_NONE;
        break;
    }

    float seconds;
    float fractional;
    fractional = modff(exposure->adjusted_time, &seconds);
    elements->time_seconds = seconds;
    elements->time_milliseconds = round_to_10(roundf(fractional * 1000.0f));

    if (exposure->adjusted_time < 10) {
        elements->fraction_digits = 2;

    } else if (exposure->adjusted_time < 100) {
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
    uint32_t milliseconds = (uint32_t)(roundf(seconds * 1000.0f));
    if (milliseconds > 1000000) {
        milliseconds = 1000000;
    }
    milliseconds = round_to_10(milliseconds);
    return milliseconds;
}
