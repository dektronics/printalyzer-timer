#include "settings.h"

#include "buzzer.h"

/*
 * Right now this is simply a placeholder for settings
 * that will eventually be user-configurable and
 * persisted.
 */
//TODO Add user configuration UI for these settings
//TODO Add persistence to these settings

uint32_t settings_get_default_exposure_time()
{
    return 15000;
}

exposure_contrast_grade_t settings_get_default_contrast_grade()
{
    return CONTRAST_GRADE_2;
}

exposure_adjustment_increment_t settings_get_default_step_size()
{
    return EXPOSURE_ADJ_QUARTER;
}

safelight_mode_t settings_get_safelight_mode()
{
    return SAFELIGHT_MODE_AUTO;
}

uint32_t settings_get_enlarger_focus_timeout()
{
    /* Default value is 5 minutes */
    return 300000;
}

uint8_t settings_get_display_brightness()
{
    return 0x0F;
}

uint8_t settings_get_led_brightness()
{
    return 127;
}

buzzer_volume_t settings_get_buzzer_volume()
{
    return BUZZER_VOLUME_MEDIUM;
}

teststrip_mode_t settings_get_teststrip_mode()
{
    return TESTSTRIP_MODE_INCREMENTAL;
}

teststrip_patches_t settings_get_teststrip_patches()
{
    return TESTSTRIP_PATCHES_7;
}
