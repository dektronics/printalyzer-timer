#include "settings.h"

#include <string.h>
#include <esp_log.h>

#include "buzzer.h"
#include "m24m01.h"

static const char *TAG = "settings";

#define CONFIG_VERSION                 1
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

/* Handle to I2C peripheral used by the EEPROM */
static I2C_HandleTypeDef *eeprom_i2c = NULL;

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

#if 0
/* Profile for bench test lamp. */
static enlarger_profile_t settings_default_enlarger_profile = {
    .turn_on_delay = 60,
    .rise_time = 300,
    .rise_time_equiv = 180,
    .turn_off_delay = 10,
    .fall_time = 500,
    .fall_time_equiv = 50
};
#endif
#if 1
/* Profile for darkroom enlarger. */
static enlarger_profile_t settings_default_enlarger_profile = {
    .turn_on_delay = 124,
    .rise_time = 546,
    .rise_time_equiv = 361,
    .turn_off_delay = 31,
    .fall_time = 316,
    .fall_time_equiv = 57
};
#endif

#define PAGE_BASE                   0x00000

#define PAGE_CONFIG                 0x00100
#define ADDR_CONFIG_VERSION         (PAGE_CONFIG + 0)
#define ADDR_EXPOSURE_TIME          (PAGE_CONFIG + 4)
#define ADDR_CONTRAST_GRADE         (PAGE_CONFIG + 8)
#define ADDR_STEP_SIZE              (PAGE_CONFIG + 12)
#define ADDR_SAFELIGHT_MODE         (PAGE_CONFIG + 16)
#define ADDR_ENLARGER_FOCUS_TIMEOUT (PAGE_CONFIG + 20)
#define ADDR_DISPLAY_BRIGHTNESS     (PAGE_CONFIG + 24)
#define ADDR_LED_BRIGHTNESS         (PAGE_CONFIG + 28)
#define ADDR_BUZZER_VOLUME          (PAGE_CONFIG + 32)
#define ADDR_TESTSTRIP_MODE         (PAGE_CONFIG + 36)
#define ADDR_TESTSTRIP_PATCHES      (PAGE_CONFIG + 40)

HAL_StatusTypeDef settings_init_default_config();
void settings_init_parse_config_page(uint8_t *data);
static bool write_u32(uint32_t address, uint32_t val);
static void copy_from_u32(uint8_t *buf, uint32_t val);
static uint32_t copy_to_u32(uint8_t *buf);

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
        uint8_t data[256];
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

            uint32_t config_version = copy_to_u32(data + (ADDR_CONFIG_VERSION - PAGE_CONFIG));
            if (config_version == 0 || config_version > CONFIG_VERSION) {
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
    copy_from_u32(data + (ADDR_CONFIG_VERSION - PAGE_CONFIG),         CONFIG_VERSION);
    copy_from_u32(data + (ADDR_EXPOSURE_TIME - PAGE_CONFIG),          DEFAULT_EXPOSURE_TIME);
    copy_from_u32(data + (ADDR_CONTRAST_GRADE - PAGE_CONFIG),         DEFAULT_CONTRAST_GRADE);
    copy_from_u32(data + (ADDR_STEP_SIZE - PAGE_CONFIG),              DEFAULT_STEP_SIZE);
    copy_from_u32(data + (ADDR_SAFELIGHT_MODE - PAGE_CONFIG),         DEFAULT_SAFELIGHT_MODE);
    copy_from_u32(data + (ADDR_ENLARGER_FOCUS_TIMEOUT - PAGE_CONFIG), DEFAULT_ENLARGER_FOCUS_TIMEOUT);
    copy_from_u32(data + (ADDR_DISPLAY_BRIGHTNESS - PAGE_CONFIG),     DEFAULT_DISPLAY_BRIGHTNESS);
    copy_from_u32(data + (ADDR_LED_BRIGHTNESS - PAGE_CONFIG),         DEFAULT_LED_BRIGHTNESS);
    copy_from_u32(data + (ADDR_BUZZER_VOLUME - PAGE_CONFIG),          DEFAULT_BUZZER_VOLUME);
    copy_from_u32(data + (ADDR_TESTSTRIP_MODE - PAGE_CONFIG),         DEFAULT_TESTSTRIP_MODE);
    copy_from_u32(data + (ADDR_TESTSTRIP_PATCHES - PAGE_CONFIG),      DEFAULT_TESTSTRIP_PATCHES);
    return m24m01_write_page(eeprom_i2c, PAGE_CONFIG, data, sizeof(data));
}

void settings_init_parse_config_page(uint8_t *data)
{
    uint32_t val;
    val = copy_to_u32(data + (ADDR_EXPOSURE_TIME - PAGE_CONFIG));
    if (val > 1000 && val <= 999000) {
        setting_default_exposure_time = val;
    } else {
        setting_default_exposure_time = DEFAULT_EXPOSURE_TIME;
    }

    val = copy_to_u32(data + (ADDR_CONTRAST_GRADE - PAGE_CONFIG));
    if (val >= CONTRAST_GRADE_00 && val <= CONTRAST_GRADE_5) {
        setting_default_contrast_grade = val;
    } else {
        setting_default_contrast_grade = DEFAULT_CONTRAST_GRADE;
    }

    val = copy_to_u32(data + (ADDR_STEP_SIZE - PAGE_CONFIG));
    if (val >= EXPOSURE_ADJ_TWELFTH && val <=  EXPOSURE_ADJ_WHOLE) {
        setting_default_step_size = val;
    } else {
        setting_default_step_size = DEFAULT_STEP_SIZE;
    }

    val = copy_to_u32(data + (ADDR_SAFELIGHT_MODE - PAGE_CONFIG));
    if (val >= SAFELIGHT_MODE_OFF && val <= SAFELIGHT_MODE_AUTO) {
        setting_safelight_mode = val;
    } else {
        setting_safelight_mode = DEFAULT_SAFELIGHT_MODE;
    }

    val = copy_to_u32(data + (ADDR_ENLARGER_FOCUS_TIMEOUT - PAGE_CONFIG));
    if (val <= (10 * 60000)) {
        setting_enlarger_focus_timeout = val;
    } else {
        setting_enlarger_focus_timeout = DEFAULT_ENLARGER_FOCUS_TIMEOUT;
    }

    val = copy_to_u32(data + (ADDR_DISPLAY_BRIGHTNESS - PAGE_CONFIG));
    if (val <= 0x0F) {
        setting_display_brightness = val;
    } else {
        setting_display_brightness = DEFAULT_DISPLAY_BRIGHTNESS;
    }

    val = copy_to_u32(data + (ADDR_LED_BRIGHTNESS - PAGE_CONFIG));
    if (val <= 0xFF) {
        setting_led_brightness = val;
    } else {
        setting_led_brightness = DEFAULT_LED_BRIGHTNESS;
    }

    val = copy_to_u32(data + (ADDR_BUZZER_VOLUME - PAGE_CONFIG));
    if (val >= BUZZER_VOLUME_OFF && val <= BUZZER_VOLUME_HIGH) {
        setting_buzzer_volume = val;
    } else {
        setting_buzzer_volume = DEFAULT_BUZZER_VOLUME;
    }

    val = copy_to_u32(data + (ADDR_TESTSTRIP_MODE - PAGE_CONFIG));
    if (val >= TESTSTRIP_MODE_INCREMENTAL && val <= TESTSTRIP_MODE_SEPARATE) {
        setting_teststrip_mode = val;
    } else {
        setting_teststrip_mode = DEFAULT_TESTSTRIP_MODE;
    }

    val = copy_to_u32(data + (ADDR_TESTSTRIP_PATCHES - PAGE_CONFIG));
    if (val >= TESTSTRIP_PATCHES_7 && val <= TESTSTRIP_PATCHES_5) {
        setting_teststrip_patches = val;
    } else {
        setting_teststrip_patches = DEFAULT_TESTSTRIP_PATCHES;
    }
}

HAL_StatusTypeDef settings_clear(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef ret = HAL_OK;

    if (!hi2c) {
        return HAL_ERROR;
    }

    uint8_t data[256];
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
        if (write_u32(ADDR_EXPOSURE_TIME, exposure_time)) {
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
        if (write_u32(ADDR_CONTRAST_GRADE, contrast_grade)) {
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
        if (write_u32(ADDR_STEP_SIZE, step_size)) {
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
        if (write_u32(ADDR_SAFELIGHT_MODE, mode)) {
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
        if (write_u32(ADDR_ENLARGER_FOCUS_TIMEOUT, timeout)) {
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
        if (write_u32(ADDR_DISPLAY_BRIGHTNESS, brightness)) {
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
        if (write_u32(ADDR_LED_BRIGHTNESS, brightness)) {
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
        if (write_u32(ADDR_BUZZER_VOLUME, volume)) {
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
        if (write_u32(ADDR_TESTSTRIP_MODE, mode)) {
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
        if (write_u32(ADDR_TESTSTRIP_PATCHES, patches)) {
            setting_teststrip_patches = patches;
        }
    }
}

const enlarger_profile_t *settings_get_default_enlarger_profile()
{
    return &settings_default_enlarger_profile;
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

uint32_t copy_to_u32(uint8_t *buf)
{
    return (uint32_t)buf[0] << 24
        | (uint32_t)buf[1] << 16
        | (uint32_t)buf[2] << 8
        | (uint32_t)buf[3];
}
