#include "settings.h"

#include "buzzer.h"

/*
 * Right now this is simply a placeholder for settings
 * that will eventually be user-configurable and
 * persisted.
 */
//TODO Add persistence to these settings

#define DEFAULT_EXPOSURE_TIME          15000
#define DEFAULT_CONTRAST_GRADE         CONTRAST_GRADE_2
#define DEFAULT_STEP_SIZE              EXPOSURE_ADJ_QUARTER
#define DEFAULT_SAFELIGHT_MODE         SAFELIGHT_MODE_AUTO
#define DEFAULT_ENLARGER_FOCUS_TIMEOUT 300000
#define DEFAULT_DISPLAY_BRIGHTNESS     0x0F
#define DEFAULT_LED_BRIGHTNESS         127
#define DEFAULT_BUZZER_VOLUME          BUZZER_VOLUME_MEDIUM
#define DEFAULT_TESTSTRIP_MODE         TESTSTRIP_MODE_INCREMENTAL
#define DEFAULT_TESTSTRIP_PATCHES      TESTSTRIP_PATCHES_7

static uint32_t setting_default_exposure_time = DEFAULT_EXPOSURE_TIME;
static exposure_contrast_grade_t setting_default_contrast_grade = DEFAULT_CONTRAST_GRADE;
static exposure_adjustment_increment_t setting_default_step_size = DEFAULT_STEP_SIZE;
static safelight_mode_t setting_safelight_mode = DEFAULT_SAFELIGHT_MODE;
static uint32_t setting_enlarger_focus_timeout = DEFAULT_ENLARGER_FOCUS_TIMEOUT;
static uint8_t setting_display_brightness = DEFAULT_DISPLAY_BRIGHTNESS;
static uint8_t setting_led_brightness = DEFAULT_LED_BRIGHTNESS;
static buzzer_volume_t setting_buzzer_volume = DEFAULT_BUZZER_VOLUME;
static teststrip_mode_t setting_teststrip_mode = DEFAULT_TESTSTRIP_MODE;
static teststrip_patches_t setting_teststrip_patches = DEFAULT_TESTSTRIP_PATCHES;

uint32_t settings_get_default_exposure_time()
{
    return setting_default_exposure_time;
}

void settings_set_default_exposure_time(uint32_t exposure_time)
{
    if (setting_default_exposure_time != exposure_time
        && exposure_time > 1000 && exposure_time <= 999000) {
        setting_default_exposure_time = exposure_time;
    }
}

exposure_contrast_grade_t settings_get_default_contrast_grade()
{
    return setting_default_contrast_grade;
}

void settings_set_default_contrast_grade(exposure_contrast_grade_t contrast_grade)
{
    if (setting_default_contrast_grade != contrast_grade
        && contrast_grade >= CONTRAST_GRADE_00 && contrast_grade <= CONTRAST_GRADE_5) {
        setting_default_contrast_grade = contrast_grade;
    }
}

exposure_adjustment_increment_t settings_get_default_step_size()
{
    return setting_default_step_size;
}

void settings_set_default_step_size(exposure_adjustment_increment_t step_size)
{
    if (setting_default_step_size != step_size
        && step_size >= EXPOSURE_ADJ_TWELFTH && step_size <=  EXPOSURE_ADJ_WHOLE) {
        setting_default_step_size = step_size;
    }
}

safelight_mode_t settings_get_safelight_mode()
{
    return setting_safelight_mode;
}

void settings_set_safelight_mode(safelight_mode_t mode)
{
    if (setting_safelight_mode != mode
        && mode >= SAFELIGHT_MODE_OFF && mode <= SAFELIGHT_MODE_AUTO) {
        setting_safelight_mode = mode;
    }
}

uint32_t settings_get_enlarger_focus_timeout()
{
    return setting_enlarger_focus_timeout;
}

void settings_set_enlarger_focus_timeout(uint32_t timeout)
{
    if (setting_enlarger_focus_timeout != timeout
        && timeout <= (10 * 60000)) {
        setting_enlarger_focus_timeout = timeout;
    }
}

uint8_t settings_get_display_brightness()
{
    return setting_display_brightness;
}

void settings_set_display_brightness(uint8_t brightness)
{
    if (setting_display_brightness != brightness
        && brightness <= 0x0F) {
        setting_display_brightness = brightness;
    }
}

uint8_t settings_get_led_brightness()
{
    return setting_led_brightness;
}

void settings_set_led_brightness(uint8_t brightness)
{
    if (setting_led_brightness != brightness) {
        setting_led_brightness = brightness;
    }
}

buzzer_volume_t settings_get_buzzer_volume()
{
    return setting_buzzer_volume;
}

void settings_set_buzzer_volume(buzzer_volume_t volume)
{
    if (setting_buzzer_volume != volume
        && volume >= BUZZER_VOLUME_OFF && volume <= BUZZER_VOLUME_HIGH) {
        setting_buzzer_volume = volume;
    }
}

teststrip_mode_t settings_get_teststrip_mode()
{
    return setting_teststrip_mode;
}

void settings_set_teststrip_mode(teststrip_mode_t mode)
{
    if (setting_teststrip_mode != mode
        && mode >= TESTSTRIP_MODE_INCREMENTAL && mode <= TESTSTRIP_MODE_SEPARATE) {
        setting_teststrip_mode = mode;
    }
}

teststrip_patches_t settings_get_teststrip_patches()
{
    return setting_teststrip_patches;
}

void settings_set_teststrip_patches(teststrip_patches_t patches)
{
    if (setting_teststrip_patches != patches
        && patches >= TESTSTRIP_PATCHES_7 && patches <= TESTSTRIP_PATCHES_5) {
        setting_teststrip_patches = patches;
    }
}
