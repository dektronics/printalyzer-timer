#include "settings.h"

#include <string.h>
#include <esp_log.h>

#include "buzzer.h"
#include "m24m01.h"

static const char *TAG = "settings";

#define LATEST_CONFIG_VERSION           1
#define DEFAULT_EXPOSURE_TIME           15000
#define DEFAULT_CONTRAST_GRADE          CONTRAST_GRADE_2
#define DEFAULT_STEP_SIZE               EXPOSURE_ADJ_QUARTER
#define DEFAULT_SAFELIGHT_MODE          SAFELIGHT_MODE_AUTO
#define DEFAULT_ENLARGER_FOCUS_TIMEOUT  300000
#define DEFAULT_DISPLAY_BRIGHTNESS      0x0F
#define DEFAULT_LED_BRIGHTNESS          127
#define DEFAULT_BUZZER_VOLUME           BUZZER_VOLUME_MEDIUM
#define DEFAULT_TESTSTRIP_MODE          TESTSTRIP_MODE_INCREMENTAL
#define DEFAULT_TESTSTRIP_PATCHES       TESTSTRIP_PATCHES_7
#define DEFAULT_ENLARGER_PROFILE        0
#define DEFAULT_PAPER_PROFILE           0

#define LATEST_ENLARGER_PROFILE_VERSION 1
#define LATEST_PAPER_PROFILE_VERSION    1

/* Handle to I2C peripheral used by the EEPROM */
static I2C_HandleTypeDef *eeprom_i2c = NULL;

/* Persistent user settings backed by EEPROM values */
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
static uint8_t setting_enlarger_profile = DEFAULT_ENLARGER_PROFILE;
static uint8_t setting_paper_profile = DEFAULT_PAPER_PROFILE;

#define PAGE_BASE                        0x00000
#define BASE_MAGIC                       0 /* "PRINTALYZER\0" */

#define PAGE_CONFIG                      0x00100
#define CONFIG_VERSION                   0
#define CONFIG_EXPOSURE_TIME             4
#define CONFIG_CONTRAST_GRADE            8
#define CONFIG_STEP_SIZE                 12
#define CONFIG_SAFELIGHT_MODE            16
#define CONFIG_ENLARGER_FOCUS_TIMEOUT    20
#define CONFIG_DISPLAY_BRIGHTNESS        24
#define CONFIG_LED_BRIGHTNESS            28
#define CONFIG_BUZZER_VOLUME             32
#define CONFIG_TESTSTRIP_MODE            36
#define CONFIG_TESTSTRIP_PATCHES         40
#define CONFIG_ENLARGER_PROFILE          44
#define CONFIG_PAPER_PROFILE             48
/**
 * Each enlarger profile is allocated a full 256-byte page,
 * starting at this address, up to a maximum of 16
 * profile entries.
 */
#define PAGE_ENLARGER_PROFILE_BASE       0x01000
#define ENLARGER_PROFILE_VERSION         0
#define ENLARGER_PROFILE_NAME            4  /* char[32] */
#define ENLARGER_PROFILE_TURN_ON_DELAY   36
#define ENLARGER_PROFILE_RISE_TIME       40
#define ENLARGER_PROFILE_RISE_TIME_EQUIV 44
#define ENLARGER_PROFILE_TURN_OFF_DELAY  48
#define ENLARGER_PROFILE_FALL_TIME       52
#define ENLARGER_PROFILE_FALL_TIME_EQUIV 56
#define ENLARGER_PROFILE_COLOR_TEMP      60
#define ENLARGER_PROFILE_LIGHT_SOURCE    64
#define ENLARGER_PROFILE_IR_CONTENT      68

/**
 * Each paper profile is allocated a full 256-byte page,
 * starting at this address, up to a maximum of 16
 * profile entries.
 */
#define PAGE_PAPER_PROFILE_BASE          0x02000

/**
 * Size of a memory page.
 */
#define PAGE_SIZE                        0x00100

/**
 * End of the EEPROM memory space, kept here for reference.
 */
#define PAGE_LIMIT                       0x20000

HAL_StatusTypeDef settings_init_default_config();
void settings_init_parse_config_page(const uint8_t *data);
static void settings_enlarger_profile_parse_page(enlarger_profile_t *profile, const uint8_t *data);
static void settings_enlarger_profile_populate_page(const enlarger_profile_t *profile, uint8_t *data);
static bool write_u32(uint32_t address, uint32_t val);
static void copy_from_u32(uint8_t *buf, uint32_t val);
static uint32_t copy_to_u32(const uint8_t *buf);

HAL_StatusTypeDef settings_init(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef ret = HAL_OK;

    if (!hi2c) {
        return HAL_ERROR;
    }

    eeprom_i2c = hi2c;

    do {
        ESP_LOGI(TAG, "settings_init");

        // Read the base page
        uint8_t data[PAGE_SIZE];
        ret = m24m01_read_buffer(eeprom_i2c, PAGE_BASE, data, sizeof(data));
        if (ret != HAL_OK) { break; }

        if (memcmp(data, "PRINTALYZER\0", 12) != 0) {
            ESP_LOGI(TAG, "Initializing base page");
            memset(data, 0, sizeof(data));
            memcpy(data, "PRINTALYZER\0", 12);
            ret = m24m01_write_page(eeprom_i2c, PAGE_BASE, data, sizeof(data));
            if (ret != HAL_OK) { break; }

            ret = settings_init_default_config();
            if (ret != HAL_OK) { break; }
        } else {
            ESP_LOGI(TAG, "Loading config page");
            ret = m24m01_read_buffer(eeprom_i2c, PAGE_CONFIG, data, sizeof(data));
            if (ret != HAL_OK) { break; }

            uint32_t config_version = copy_to_u32(data + CONFIG_VERSION);
            if (config_version == 0 || config_version > LATEST_CONFIG_VERSION) {
                ESP_LOGW(TAG, "Invalid config version %ld", config_version);
                ret = settings_init_default_config();
                if (ret != HAL_OK) { break; }
                break;
            } else {
                settings_init_parse_config_page(data);
            }
        }

    } while (0);

    return ret;
}

HAL_StatusTypeDef settings_init_default_config()
{
    uint8_t data[256];
    ESP_LOGI(TAG, "Initializing config page");
    memset(data, 0, sizeof(data));
    copy_from_u32(data + CONFIG_VERSION,                LATEST_CONFIG_VERSION);
    copy_from_u32(data + CONFIG_EXPOSURE_TIME,          DEFAULT_EXPOSURE_TIME);
    copy_from_u32(data + CONFIG_CONTRAST_GRADE,         DEFAULT_CONTRAST_GRADE);
    copy_from_u32(data + CONFIG_STEP_SIZE,              DEFAULT_STEP_SIZE);
    copy_from_u32(data + CONFIG_SAFELIGHT_MODE,         DEFAULT_SAFELIGHT_MODE);
    copy_from_u32(data + CONFIG_ENLARGER_FOCUS_TIMEOUT, DEFAULT_ENLARGER_FOCUS_TIMEOUT);
    copy_from_u32(data + CONFIG_DISPLAY_BRIGHTNESS,     DEFAULT_DISPLAY_BRIGHTNESS);
    copy_from_u32(data + CONFIG_LED_BRIGHTNESS,         DEFAULT_LED_BRIGHTNESS);
    copy_from_u32(data + CONFIG_BUZZER_VOLUME,          DEFAULT_BUZZER_VOLUME);
    copy_from_u32(data + CONFIG_TESTSTRIP_MODE,         DEFAULT_TESTSTRIP_MODE);
    copy_from_u32(data + CONFIG_TESTSTRIP_PATCHES,      DEFAULT_TESTSTRIP_PATCHES);
    return m24m01_write_page(eeprom_i2c, PAGE_CONFIG, data, sizeof(data));
}

void settings_init_parse_config_page(const uint8_t *data)
{
    uint32_t val;
    val = copy_to_u32(data + CONFIG_EXPOSURE_TIME);
    if (val > 1000 && val <= 999000) {
        setting_default_exposure_time = val;
    } else {
        setting_default_exposure_time = DEFAULT_EXPOSURE_TIME;
    }

    val = copy_to_u32(data + CONFIG_CONTRAST_GRADE);
    if (val >= CONTRAST_GRADE_00 && val <= CONTRAST_GRADE_5) {
        setting_default_contrast_grade = val;
    } else {
        setting_default_contrast_grade = DEFAULT_CONTRAST_GRADE;
    }

    val = copy_to_u32(data + CONFIG_STEP_SIZE);
    if (val >= EXPOSURE_ADJ_TWELFTH && val <=  EXPOSURE_ADJ_WHOLE) {
        setting_default_step_size = val;
    } else {
        setting_default_step_size = DEFAULT_STEP_SIZE;
    }

    val = copy_to_u32(data + CONFIG_SAFELIGHT_MODE);
    if (val >= SAFELIGHT_MODE_OFF && val <= SAFELIGHT_MODE_AUTO) {
        setting_safelight_mode = val;
    } else {
        setting_safelight_mode = DEFAULT_SAFELIGHT_MODE;
    }

    val = copy_to_u32(data + CONFIG_ENLARGER_FOCUS_TIMEOUT);
    if (val <= (10 * 60000)) {
        setting_enlarger_focus_timeout = val;
    } else {
        setting_enlarger_focus_timeout = DEFAULT_ENLARGER_FOCUS_TIMEOUT;
    }

    val = copy_to_u32(data + CONFIG_DISPLAY_BRIGHTNESS);
    if (val <= 0x0F) {
        setting_display_brightness = val;
    } else {
        setting_display_brightness = DEFAULT_DISPLAY_BRIGHTNESS;
    }

    val = copy_to_u32(data + CONFIG_LED_BRIGHTNESS);
    if (val <= 0xFF) {
        setting_led_brightness = val;
    } else {
        setting_led_brightness = DEFAULT_LED_BRIGHTNESS;
    }

    val = copy_to_u32(data + CONFIG_BUZZER_VOLUME);
    if (val >= BUZZER_VOLUME_OFF && val <= BUZZER_VOLUME_HIGH) {
        setting_buzzer_volume = val;
    } else {
        setting_buzzer_volume = DEFAULT_BUZZER_VOLUME;
    }

    val = copy_to_u32(data + CONFIG_TESTSTRIP_MODE);
    if (val >= TESTSTRIP_MODE_INCREMENTAL && val <= TESTSTRIP_MODE_SEPARATE) {
        setting_teststrip_mode = val;
    } else {
        setting_teststrip_mode = DEFAULT_TESTSTRIP_MODE;
    }

    val = copy_to_u32(data + CONFIG_TESTSTRIP_PATCHES);
    if (val >= TESTSTRIP_PATCHES_7 && val <= TESTSTRIP_PATCHES_5) {
        setting_teststrip_patches = val;
    } else {
        setting_teststrip_patches = DEFAULT_TESTSTRIP_PATCHES;
    }

    val = copy_to_u32(data + CONFIG_ENLARGER_PROFILE);
    if (val < 16) {
        setting_enlarger_profile = val;
    } else {
        setting_enlarger_profile = DEFAULT_ENLARGER_PROFILE;
    }

    val = copy_to_u32(data + CONFIG_PAPER_PROFILE);
    if (val < 16) {
        setting_paper_profile = val;
    } else {
        setting_paper_profile = DEFAULT_PAPER_PROFILE;
    }
}

HAL_StatusTypeDef settings_clear(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef ret = HAL_OK;

    if (!hi2c) {
        return HAL_ERROR;
    }

    uint8_t data[PAGE_SIZE];
    memset(data, 0xFF, sizeof(data));

    do {
        ESP_LOGI(TAG, "settings_clear");

        ret = m24m01_write_page(hi2c, PAGE_BASE, data, sizeof(data));
        if (ret != HAL_OK) { break; }

        ret = m24m01_write_page(hi2c, PAGE_CONFIG, data, sizeof(data));
        if (ret != HAL_OK) { break; }

    } while (0);

    return ret;
}

uint32_t settings_get_default_exposure_time()
{
    return setting_default_exposure_time;
}

void settings_set_default_exposure_time(uint32_t exposure_time)
{
    if (setting_default_exposure_time != exposure_time
        && exposure_time > 1000 && exposure_time <= 999000) {
        if (write_u32(PAGE_CONFIG + CONFIG_EXPOSURE_TIME, exposure_time)) {
            setting_default_exposure_time = exposure_time;
        }
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
        if (write_u32(PAGE_CONFIG + CONFIG_CONTRAST_GRADE, contrast_grade)) {
            setting_default_contrast_grade = contrast_grade;
        }
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
        if (write_u32(PAGE_CONFIG + CONFIG_STEP_SIZE, step_size)) {
            setting_default_step_size = step_size;
        }
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
        if (write_u32(PAGE_CONFIG + CONFIG_SAFELIGHT_MODE, mode)) {
            setting_safelight_mode = mode;
        }
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
        if (write_u32(PAGE_CONFIG + CONFIG_ENLARGER_FOCUS_TIMEOUT, timeout)) {
            setting_enlarger_focus_timeout = timeout;
        }
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
        if (write_u32(PAGE_CONFIG + CONFIG_DISPLAY_BRIGHTNESS, brightness)) {
            setting_display_brightness = brightness;
        }
    }
}

uint8_t settings_get_led_brightness()
{
    return setting_led_brightness;
}

void settings_set_led_brightness(uint8_t brightness)
{
    if (setting_led_brightness != brightness) {
        if (write_u32(PAGE_CONFIG + CONFIG_LED_BRIGHTNESS, brightness)) {
            setting_led_brightness = brightness;
        }
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
        if (write_u32(PAGE_CONFIG + CONFIG_BUZZER_VOLUME, volume)) {
            setting_buzzer_volume = volume;
        }
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
        if (write_u32(PAGE_CONFIG + CONFIG_TESTSTRIP_MODE, mode)) {
            setting_teststrip_mode = mode;
        }
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
        if (write_u32(PAGE_CONFIG + CONFIG_TESTSTRIP_PATCHES, patches)) {
            setting_teststrip_patches = patches;
        }
    }
}

uint8_t settings_get_default_enlarger_profile_index()
{
    return setting_enlarger_profile;
}

void settings_set_default_enlarger_profile_index(uint8_t index)
{
    if (setting_enlarger_profile != index && index < 16) {
        if (write_u32(PAGE_CONFIG + CONFIG_ENLARGER_PROFILE, index)) {
            setting_enlarger_profile = index;
        }
    }
}

uint8_t settings_get_default_paper_profile_index()
{
    return setting_paper_profile;
}

void settings_set_default_paper_profile_index(uint8_t index)
{
    if (setting_paper_profile != index && index < 16) {
        if (write_u32(PAGE_CONFIG + CONFIG_PAPER_PROFILE, index)) {
            setting_paper_profile = index;
        }
    }
}

bool settings_get_enlarger_profile(enlarger_profile_t *profile, uint8_t index)
{
    if (!profile || index >= 16) { return false; }

    ESP_LOGI(TAG, "Load enlarger profile: %d", index);

    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t data[PAGE_SIZE];
    memset(data, 0, sizeof(data));

    do {
        ret = m24m01_read_buffer(eeprom_i2c,
            PAGE_ENLARGER_PROFILE_BASE + (PAGE_SIZE * index),
            data, sizeof(data));
        if (ret != HAL_OK) { break; }

        uint32_t profile_version = copy_to_u32(data + ENLARGER_PROFILE_VERSION);
        if (profile_version == UINT32_MAX) {
            ESP_LOGD(TAG, "Profile index is empty");
            ret = HAL_ERROR;
            break;
        }
        if (profile_version == 0 || profile_version > LATEST_CONFIG_VERSION) {
            ESP_LOGW(TAG, "Invalid profile version %ld", profile_version);
            ret = HAL_ERROR;
            break;
        }

        settings_enlarger_profile_parse_page(profile, data);

    } while (0);

    return (ret == HAL_OK);
}

void settings_enlarger_profile_parse_page(enlarger_profile_t *profile, const uint8_t *data)
{
    memset(profile, 0, sizeof(enlarger_profile_t));

    strncpy(profile->name, (const char *)(data + ENLARGER_PROFILE_NAME), 32);
    profile->name[31] = '\0';

    profile->turn_on_delay = copy_to_u32(data + ENLARGER_PROFILE_TURN_ON_DELAY);
    profile->rise_time = copy_to_u32(data + ENLARGER_PROFILE_RISE_TIME);
    profile->rise_time_equiv = copy_to_u32(data + ENLARGER_PROFILE_RISE_TIME_EQUIV);
    profile->turn_off_delay = copy_to_u32(data + ENLARGER_PROFILE_TURN_OFF_DELAY);
    profile->fall_time = copy_to_u32(data + ENLARGER_PROFILE_FALL_TIME);
    profile->fall_time_equiv = copy_to_u32(data + ENLARGER_PROFILE_FALL_TIME_EQUIV);
    profile->color_temperature = copy_to_u32(data + ENLARGER_PROFILE_COLOR_TEMP);
}

bool settings_set_enlarger_profile(const enlarger_profile_t *profile, uint8_t index)
{
    if (!profile || index >= 16) { return false; }

    ESP_LOGI(TAG, "Save enlarger profile: %d", index);

    uint8_t data[PAGE_SIZE];
    memset(data, 0, sizeof(data));

    settings_enlarger_profile_populate_page(profile, data);

    HAL_StatusTypeDef ret = m24m01_write_page(eeprom_i2c,
        PAGE_ENLARGER_PROFILE_BASE + (PAGE_SIZE * index),
        data, sizeof(data));
    return (ret == HAL_OK);
}

void settings_enlarger_profile_populate_page(const enlarger_profile_t *profile, uint8_t *data)
{
    copy_from_u32(data + ENLARGER_PROFILE_VERSION, LATEST_ENLARGER_PROFILE_VERSION);

    strncpy((char *)(data + ENLARGER_PROFILE_NAME), profile->name, 32);

    copy_from_u32(data + ENLARGER_PROFILE_TURN_ON_DELAY,   profile->turn_on_delay);
    copy_from_u32(data + ENLARGER_PROFILE_RISE_TIME,       profile->rise_time);
    copy_from_u32(data + ENLARGER_PROFILE_RISE_TIME_EQUIV, profile->rise_time_equiv);
    copy_from_u32(data + ENLARGER_PROFILE_TURN_OFF_DELAY,  profile->turn_off_delay);
    copy_from_u32(data + ENLARGER_PROFILE_FALL_TIME,       profile->fall_time);
    copy_from_u32(data + ENLARGER_PROFILE_FALL_TIME_EQUIV, profile->fall_time_equiv);
    copy_from_u32(data + ENLARGER_PROFILE_COLOR_TEMP, profile->color_temperature);
}

void settings_clear_enlarger_profile(uint8_t index)
{
    if (index >= 16) { return; }

    uint8_t data[PAGE_SIZE];
    memset(data, 0xFF, sizeof(data));

    ESP_LOGI(TAG, "Clear enlarger profile: %d", index);

    m24m01_write_page(eeprom_i2c,
        PAGE_ENLARGER_PROFILE_BASE + (PAGE_SIZE * index),
        data, sizeof(data));
}

bool write_u32(uint32_t address, uint32_t val)
{
    uint8_t data[4];
    copy_from_u32(data, val);
    return m24m01_write_buffer(eeprom_i2c, address, data, sizeof(data)) == HAL_OK;
}

void copy_from_u32(uint8_t *buf, uint32_t val)
{
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >> 8) & 0xFF;
    buf[3] = val & 0xFF;
}

uint32_t copy_to_u32(const uint8_t *buf)
{
    return (uint32_t)buf[0] << 24
        | (uint32_t)buf[1] << 16
        | (uint32_t)buf[2] << 8
        | (uint32_t)buf[3];
}
