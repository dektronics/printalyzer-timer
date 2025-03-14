#include "settings.h"

#include <string.h>
#include <math.h>
#include <cmsis_os.h>

#define LOG_TAG "settings"
#include <elog.h>

#include "settings_util.h"
#include "buzzer.h"
#include "m24m01.h"
#include "contrast.h"
#include "util.h"

/*
 * Make sure any length constants, which the memory alignment of
 * settings may depend upon, haven't been changed.
 */
#ifndef __CDT_PARSER__
_Static_assert(PROFILE_NAME_LEN == 32, "PROFILE_NAME_LEN length has been changed");
_Static_assert(CONTRAST_WHOLE_GRADE_COUNT == 7, "CONTRAST_WHOLE_GRADE_COUNT length has been changed");
#endif

#define LATEST_CONFIG_VERSION           1
#define DEFAULT_EXPOSURE_TIME           15000
#define DEFAULT_CONTRAST_GRADE          CONTRAST_GRADE_2
#define DEFAULT_STEP_SIZE               EXPOSURE_ADJ_QUARTER
#define DEFAULT_ENLARGER_FOCUS_TIMEOUT  300000
#define DEFAULT_DISPLAY_BRIGHTNESS      0x0F
#define DEFAULT_LED_BRIGHTNESS          127
#define DEFAULT_BUZZER_VOLUME           BUZZER_VOLUME_MEDIUM
#define DEFAULT_TESTSTRIP_MODE          TESTSTRIP_MODE_INCREMENTAL
#define DEFAULT_TESTSTRIP_PATCHES       TESTSTRIP_PATCHES_7
#define DEFAULT_ENLARGER_CONFIG         0
#define DEFAULT_PAPER_PROFILE           0

#define LATEST_CONFIG2_VERSION          1
#define DEFAULT_SAFELIGHT_CONFIG        { SAFELIGHT_MODE_AUTO, SAFELIGHT_CONTROL_RELAY, 0, false, 255 }

#define LATEST_ENLARGER_CONFIG_VERSION  1
#define LATEST_PAPER_PROFILE_VERSION    1
#define LATEST_STEP_WEDGE_VERSION       1

/* Handle to I2C peripheral used by the EEPROM */
static I2C_HandleTypeDef *eeprom_i2c = NULL;
static osMutexId_t eeprom_i2c_mutex = NULL;

/* Persistent user settings backed by EEPROM values */
static uint32_t setting_default_exposure_time = DEFAULT_EXPOSURE_TIME;
static contrast_grade_t setting_default_contrast_grade = DEFAULT_CONTRAST_GRADE;
static exposure_adjustment_increment_t setting_default_step_size = DEFAULT_STEP_SIZE;
static uint32_t setting_enlarger_focus_timeout = DEFAULT_ENLARGER_FOCUS_TIMEOUT;
static uint8_t setting_display_brightness = DEFAULT_DISPLAY_BRIGHTNESS;
static uint8_t setting_led_brightness = DEFAULT_LED_BRIGHTNESS;
static buzzer_volume_t setting_buzzer_volume = DEFAULT_BUZZER_VOLUME;
static teststrip_mode_t setting_teststrip_mode = DEFAULT_TESTSTRIP_MODE;
static teststrip_patches_t setting_teststrip_patches = DEFAULT_TESTSTRIP_PATCHES;
static uint8_t setting_enlarger_config = DEFAULT_ENLARGER_CONFIG;
static uint8_t setting_paper_profile = DEFAULT_PAPER_PROFILE;
static safelight_config_t setting_safelight_config = DEFAULT_SAFELIGHT_CONFIG;

/**
 * Header Page (256B)
 * Mostly unused at the moment, will be populated if any top-level system
 * data needs to be stored. Unlike other pages, it begins with a magic
 * string.
 */
#define PAGE_HEADER                      0x00000UL
#define PAGE_HEADER_SIZE                 (256U)
#define HEADER_MAGIC                     0 /* "PRINTALYZER\0" */
#define HEADER_START                     16
#define HEADER_VERSION                   1UL

/**
 * Basic configuration page (256B)
 * Contains simple configuration values that are stored
 * as standalone elements.
 */
#define PAGE_CONFIG                      0x00100UL
#define PAGE_CONFIG_SIZE                 (256U)
#define CONFIG_VERSION                   0
#define CONFIG_EXPOSURE_TIME             4
#define CONFIG_CONTRAST_GRADE            8
#define CONFIG_STEP_SIZE                 12
/* RESERVED                              16*/
#define CONFIG_ENLARGER_FOCUS_TIMEOUT    20
#define CONFIG_DISPLAY_BRIGHTNESS        24
#define CONFIG_LED_BRIGHTNESS            28
#define CONFIG_BUZZER_VOLUME             32
#define CONFIG_TESTSTRIP_MODE            36
#define CONFIG_TESTSTRIP_PATCHES         40
#define CONFIG_ENLARGER_CONFIG           44
#define CONFIG_PAPER_PROFILE             48
/* RESERVED                              52*/

/**
 * Detailed configuration page (256B)
 * Contains configuration values that are stored as structures of
 * related elements.
 */
#define PAGE_CONFIG2                     0x00200UL
#define PAGE_CONFIG2_SIZE                (256U)
#define CONFIG2_VERSION                  0
#define CONFIG2_SAFELIGHT_CONTROL        4
#define CONFIG2_SAFELIGHT_CONTROL_SIZE   (8U)

/**
 * Enlarger configurations (4096B)
 * Each enlarger configuration is allocated a full 256-byte page,
 * starting at this address, up to a maximum of 16
 * configuration entries.
 */
#define PAGE_ENLARGER_CONFIG_BASE              0x01000UL
#define ENLARGER_CONFIG_VERSION                0
#define ENLARGER_CONFIG_NAME                   4  /* char[32] */
#define ENLARGER_CONFIG_TIMING_TURN_ON_DELAY   36
#define ENLARGER_CONFIG_TIMING_RISE_TIME       40
#define ENLARGER_CONFIG_TIMING_RISE_TIME_EQUIV 44
#define ENLARGER_CONFIG_TIMING_TURN_OFF_DELAY  48
#define ENLARGER_CONFIG_TIMING_FALL_TIME       52
#define ENLARGER_CONFIG_TIMING_FALL_TIME_EQUIV 56
/* RESERVED (12B) */
#define ENLARGER_CONFIG_CONTRAST_FILTER        72 /* 1B (contrast_filter_t) */
/* RESERVED (56B) */
#define ENLARGER_CONFIG_CONTROL_DMX_ENABLED   128 /* 1B (bool) */
#define ENLARGER_CONFIG_CONTROL_CHANNEL_SET   129 /* 1B (enlarger_channel_set_t) */
#define ENLARGER_CONFIG_CONTROL_DMX_WIDE      130 /* 1B (bool) */
#define ENLARGER_CONFIG_CONTROL_CHANNEL_ID_R  131 /* 2B (uint16_t) */
#define ENLARGER_CONFIG_CONTROL_CHANNEL_ID_G  133 /* 2B (uint16_t) */
#define ENLARGER_CONFIG_CONTROL_CHANNEL_ID_B  135 /* 2B (uint16_t) */
#define ENLARGER_CONFIG_CONTROL_CHANNEL_ID_W  137 /* 2B (uint16_t) */
#define ENLARGER_CONFIG_CONTROL_CONTRAST_MODE 139 /* 1B (enlarger_contrast_mode_t) */
#define ENLARGER_CONFIG_CONTROL_FOCUS_VALUE   140 /* 2B (uint16_t) */
#define ENLARGER_CONFIG_CONTROL_SAFE_VALUE    142 /* 2B (uint16_t) */
#define ENLARGER_CONFIG_CONTROL_GRADE_VALUES  144 /* 56B (7 * (4 * uint16_t)) */

/**
 * Paper profiles (4096B)
 * Each paper profile is allocated a full 256-byte page,
 * starting at this address, up to a maximum of 16
 * profile entries.
 */
#define PAGE_PAPER_PROFILE_BASE          0x02000UL
#define PAPER_PROFILE_VERSION            0
#define PAPER_PROFILE_NAME               4  /* char[32] */
#define PAPER_PROFILE_GRADE00_HT         36
#define PAPER_PROFILE_GRADE00_HM         40
#define PAPER_PROFILE_GRADE00_HS         44
#define PAPER_PROFILE_GRADE0_HT          48
#define PAPER_PROFILE_GRADE0_HM          52
#define PAPER_PROFILE_GRADE0_HS          56
#define PAPER_PROFILE_GRADE1_HT          60
#define PAPER_PROFILE_GRADE1_HM          64
#define PAPER_PROFILE_GRADE1_HS          68
#define PAPER_PROFILE_GRADE2_HT          72
#define PAPER_PROFILE_GRADE2_HM          76
#define PAPER_PROFILE_GRADE2_HS          80
#define PAPER_PROFILE_GRADE3_HT          84
#define PAPER_PROFILE_GRADE3_HM          88
#define PAPER_PROFILE_GRADE3_HS          92
#define PAPER_PROFILE_GRADE4_HT          96
#define PAPER_PROFILE_GRADE4_HM          100
#define PAPER_PROFILE_GRADE4_HS          104
#define PAPER_PROFILE_GRADE5_HT          108
#define PAPER_PROFILE_GRADE5_HM          112
#define PAPER_PROFILE_GRADE5_HS          116
#define PAPER_PROFILE_DNET               120

/**
 * Step wedge profile (256B)
 * Only a single step wedge profile is supported, and
 * it is given a full 256-byte page.
 */
#define PAGE_STEP_WEDGE_BASE             0x03000UL
#define STEP_WEDGE_VERSION               0
#define STEP_WEDGE_NAME                  4  /* char[32] */
#define STEP_WEDGE_BASE_DENSITY          36
#define STEP_WEDGE_DENSITY_INCREMENT     40
#define STEP_WEDGE_STEP_COUNT            44
#define STEP_WEDGE_STEP_DENSITY_0        48 /* up to 51 steps supported */

/**
 * Bootloader page (512B)
 * Reserved page at the end of the settings memory used to pass instructions
 * to the bootloader when resetting the device.
 */
#define PAGE_BOOTLOADER                  0x1FE00UL
#define BOOTLOADER_COMMAND               0   /* 1B (uint8_t) */
#define BOOTLOADER_FW_DEVICE             1   /* 21B (char[21]) */
#define BOOTLOADER_FW_CHECKSUM           22  /* 4B (uint32_t) */
/* Rest of the first page is unused */
#define BOOTLOADER_FW_FILE               256 /* char[256] */

/**
 * Size of a memory page.
 */
#define PAGE_SIZE                        0x00100UL

/**
 * End of the EEPROM memory space, kept here for reference.
 */
#define PAGE_LIMIT                       0x20000UL

static HAL_StatusTypeDef settings_read_header(bool *valid);
static HAL_StatusTypeDef settings_write_header();

static bool settings_init_config(bool force_clear);
static HAL_StatusTypeDef settings_init_default_config();
static void settings_init_parse_config_page(const uint8_t *data);

static bool settings_init_config2(bool force_clear);
static bool settings_clear_config2();
static void settings_set_safelight_config_defaults(safelight_config_t *safelight_config);
static bool settings_validate_safelight_config(const safelight_config_t *safelight_config);
static bool settings_load_safelight_config();

static void settings_enlarger_config_parse_page(enlarger_config_t *config, const uint8_t *data);
static void settings_enlarger_config_populate_page(const enlarger_config_t *config, uint8_t *data);
static void settings_paper_profile_parse_page(paper_profile_t *profile, const uint8_t *data);
static void settings_paper_profile_populate_page(const paper_profile_t *profile, uint8_t *data);
static void settings_step_wedge_parse_page(step_wedge_t **wedge, const uint8_t *data);
static void settings_step_wedge_populate_page(const step_wedge_t *wedge, uint8_t *data);

static bool settings_cleanup_bootloader_firmware();

static bool read_u32(uint32_t address, uint32_t *val);
static bool write_u32(uint32_t address, uint32_t val);
static bool write_f32(uint32_t address, float val) __attribute__ ((unused));

HAL_StatusTypeDef settings_init(I2C_HandleTypeDef *hi2c, osMutexId_t i2c_mutex)
{
    HAL_StatusTypeDef ret = HAL_OK;
    bool valid = false;

    if (!hi2c || !i2c_mutex) {
        return HAL_ERROR;
    }

    eeprom_i2c = hi2c;
    eeprom_i2c_mutex = i2c_mutex;

    osMutexAcquire(eeprom_i2c_mutex, portMAX_DELAY);
    do {
        log_i("Settings init");

        /* Cleanup the bootloader instruction page */
        settings_cleanup_bootloader_firmware();

        /* Read and validate the header page */
        ret = settings_read_header(&valid);
        if (ret != HAL_OK) { break; }

        /* Initialize all settings data pages */
        if (!settings_init_config(!valid)) { break; }
        if (!settings_init_config2(!valid)) { break; }

        /* Initialize the header page if necessary */
        if (!valid) {
            ret = settings_write_header();
            if (ret != HAL_OK) { break; }
        }

        log_i("Settings loaded");
    } while (0);
    osMutexRelease(eeprom_i2c_mutex);

    return ret;
}


HAL_StatusTypeDef settings_read_header(bool *valid)
{
    HAL_StatusTypeDef ret = HAL_OK;
    bool is_valid = true;
    uint8_t data[PAGE_HEADER_SIZE];

    do {
        /* Read the header into a buffer */
        ret = m24m01_read_buffer(eeprom_i2c, PAGE_HEADER, data, sizeof(data));
        if (ret != HAL_OK) {
            log_e("Unable to read settings header: %d", ret);
            break;
        }

        /* Validate the magic bytes at the start of the header */
        if (memcmp(data, "PRINTALYZER\0", 12) != 0) {
            log_w("Invalid magic");
            is_valid = false;
            break;
        }

        /* Validate the header version */
        uint32_t version = copy_to_u32(&data[HEADER_START - PAGE_HEADER]);
        if (version != HEADER_VERSION) {
            log_w("Unexpected header version: %d", version);
            /*
             * When there are multiple versions, then this should be handled
             * gracefully and shouldn't cause EEPROM validation to fail.
             * Until then, it is probably okay to treat this as if it were
             * part of the magic string.
             */
            is_valid = false;
            break;
        }
    } while (0);

    if (ret == HAL_OK) {
        if (valid) { *valid = is_valid; }
    }
    return ret;
}

HAL_StatusTypeDef settings_write_header()
{
    log_i("Write settings header");
    HAL_StatusTypeDef ret;
    uint8_t data[PAGE_HEADER_SIZE];

    /* Fill the page with the magic bytes and version header */
    memset(data, 0, sizeof(data));
    memcpy(data, "PRINTALYZER\0", 12);
    copy_from_u32(&data[HEADER_START - PAGE_HEADER], HEADER_VERSION);

    /* Write the buffer */
    ret = m24m01_write_buffer(eeprom_i2c, PAGE_HEADER, data, sizeof(data));
    if (ret != HAL_OK) {
        log_e("Unable to write settings header: %d", ret);
    }
    return ret;
}

bool settings_init_config(bool force_clear)
{
    HAL_StatusTypeDef ret = HAL_OK;

    if (force_clear) {
        log_i("Clearing config page");
        ret = settings_init_default_config();
        if (ret != HAL_OK) { return false; }
    } else {
        uint8_t data[PAGE_CONFIG_SIZE];

        log_i("Loading config page");
        ret = m24m01_read_buffer(eeprom_i2c, PAGE_CONFIG, data, sizeof(data));
        if (ret != HAL_OK) { return false; }

        uint32_t config_version = copy_to_u32(data + CONFIG_VERSION);
        if (config_version == 0 || config_version > LATEST_CONFIG_VERSION) {
            log_w("Invalid config version %ld", config_version);
            ret = settings_init_default_config();
            if (ret != HAL_OK) { return false; }
        } else {
            settings_init_parse_config_page(data);
        }
    }
    return true;
}

HAL_StatusTypeDef settings_init_default_config()
{
    uint8_t data[256];
    log_i("Initializing config page");
    memset(data, 0, sizeof(data));
    copy_from_u32(data + CONFIG_VERSION,                LATEST_CONFIG_VERSION);
    copy_from_u32(data + CONFIG_EXPOSURE_TIME,          DEFAULT_EXPOSURE_TIME);
    copy_from_u32(data + CONFIG_CONTRAST_GRADE,         DEFAULT_CONTRAST_GRADE);
    copy_from_u32(data + CONFIG_STEP_SIZE,              DEFAULT_STEP_SIZE);
    copy_from_u32(data + CONFIG_ENLARGER_FOCUS_TIMEOUT, DEFAULT_ENLARGER_FOCUS_TIMEOUT);
    copy_from_u32(data + CONFIG_DISPLAY_BRIGHTNESS,     DEFAULT_DISPLAY_BRIGHTNESS);
    copy_from_u32(data + CONFIG_LED_BRIGHTNESS,         DEFAULT_LED_BRIGHTNESS);
    copy_from_u32(data + CONFIG_BUZZER_VOLUME,          DEFAULT_BUZZER_VOLUME);
    copy_from_u32(data + CONFIG_TESTSTRIP_MODE,         DEFAULT_TESTSTRIP_MODE);
    copy_from_u32(data + CONFIG_TESTSTRIP_PATCHES,      DEFAULT_TESTSTRIP_PATCHES);
    copy_from_u32(data + CONFIG_ENLARGER_CONFIG,        DEFAULT_ENLARGER_CONFIG);
    copy_from_u32(data + CONFIG_PAPER_PROFILE,          DEFAULT_PAPER_PROFILE);
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

    val = copy_to_u32(data + CONFIG_ENLARGER_CONFIG);
    if (val < MAX_ENLARGER_CONFIGS) {
        setting_enlarger_config = val;
    } else {
        setting_enlarger_config = DEFAULT_ENLARGER_CONFIG;
    }

    val = copy_to_u32(data + CONFIG_PAPER_PROFILE);
    if (val < MAX_PAPER_PROFILES) {
        setting_paper_profile = val;
    } else {
        setting_paper_profile = DEFAULT_PAPER_PROFILE;
    }
}

bool settings_init_config2(bool force_clear)
{
    /* Initialize all fields to their default values */
    settings_set_safelight_config_defaults(&setting_safelight_config);

    if (force_clear) {
        if (!settings_clear_config2()) {
            return false;
        }
    } else {
        log_i("Loading config2 page");
        /* Load settings if the version matches */
        uint32_t version;
        if (!read_u32(PAGE_CONFIG2, &version)) {
            return false;
        }

        if (version == LATEST_CONFIG2_VERSION) {
            /* Version is good, load data with per-field validation */
            settings_load_safelight_config();
        } else {
            /* Version is bad, initialize a blank page */
            log_w("Unexpected config2 version: %d != %d", version, LATEST_CONFIG2_VERSION);
            if (!settings_clear_config2()) {
                return false;
            }
        }
    }
    return true;
}

bool settings_clear_config2()
{
    log_i("Clearing config2 page");

    /* Zero the page version */
    if (!write_u32(PAGE_CONFIG2, 0UL)) {
        return false;
    }

    /* Write a default safelight config struct */
    safelight_config_t safelight_config;
    settings_set_safelight_config_defaults(&safelight_config);
    if (!settings_set_safelight_config(&safelight_config)) {
        return false;
    }

    /* Write the page version */
    if (!write_u32(PAGE_CONFIG2, LATEST_CONFIG2_VERSION)) {
        return false;
    }

    return true;
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
        log_i("settings_clear");

        ret = m24m01_write_page(hi2c, PAGE_HEADER, data, sizeof(data));
        if (ret != HAL_OK) { break; }

        ret = m24m01_write_page(hi2c, PAGE_CONFIG, data, sizeof(data));
        if (ret != HAL_OK) { break; }

        ret = m24m01_write_page(hi2c, PAGE_CONFIG2, data, sizeof(data));
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

contrast_grade_t settings_get_default_contrast_grade()
{
    return setting_default_contrast_grade;
}

void settings_set_default_contrast_grade(contrast_grade_t contrast_grade)
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

uint8_t settings_get_default_enlarger_config_index()
{
    return setting_enlarger_config;
}

void settings_set_default_enlarger_config_index(uint8_t index)
{
    if (setting_enlarger_config != index && index < MAX_ENLARGER_CONFIGS) {
        if (write_u32(PAGE_CONFIG + CONFIG_ENLARGER_CONFIG, index)) {
            setting_enlarger_config = index;
        }
    }
}

uint8_t settings_get_default_paper_profile_index()
{
    return setting_paper_profile;
}

void settings_set_default_paper_profile_index(uint8_t index)
{
    if (setting_paper_profile != index && index < MAX_PAPER_PROFILES) {
        if (write_u32(PAGE_CONFIG + CONFIG_PAPER_PROFILE, index)) {
            setting_paper_profile = index;
        }
    }
}

void settings_set_safelight_config_defaults(safelight_config_t *safelight_config)
{
    if (!safelight_config) { return; }
    memset(safelight_config, 0, sizeof(safelight_config_t));
    safelight_config->mode = SAFELIGHT_MODE_AUTO;
    safelight_config->control = SAFELIGHT_CONTROL_RELAY;
    safelight_config->dmx_address = 0;
    safelight_config->dmx_wide_mode = false;
    safelight_config->dmx_on_value = 255;
}

bool settings_validate_safelight_config(const safelight_config_t *safelight_config)
{
    if (!safelight_config) { return false; }

    /* Validate field properties */
    if (safelight_config->mode > SAFELIGHT_MODE_AUTO) {
        return false;
    }
    if (safelight_config->control > SAFELIGHT_CONTROL_BOTH) {
        return false;
    }
    if (safelight_config->dmx_address > 511) {
        return false;
    }
    if (!safelight_config->dmx_wide_mode && safelight_config->dmx_on_value > 0xFF) {
        return false;
    }

    return true;
}

bool settings_load_safelight_config()
{
    uint8_t buf[CONFIG2_SAFELIGHT_CONTROL_SIZE];

    if (m24m01_read_buffer(eeprom_i2c, PAGE_CONFIG2 + CONFIG2_SAFELIGHT_CONTROL, buf, sizeof(buf)) != HAL_OK) {
        return false;
    }

    setting_safelight_config.mode = (safelight_mode_t)buf[0];
    setting_safelight_config.control = (safelight_control_t)buf[1];
    setting_safelight_config.dmx_wide_mode = buf[3];
    setting_safelight_config.dmx_address = copy_to_u16(&buf[4]);
    setting_safelight_config.dmx_on_value = copy_to_u16(&buf[6]);

    return true;
}

bool settings_get_safelight_config(safelight_config_t *safelight_config)
{
    if (!safelight_config) { return false; }

    /* Copy over the settings values */
    memcpy(safelight_config, &setting_safelight_config, sizeof(safelight_config_t));

    /* Set default values if validation fails */
    if (!settings_validate_safelight_config(safelight_config)) {
        settings_set_safelight_config_defaults(safelight_config);
        return false;
    } else {
        return true;
    }
}

bool settings_set_safelight_config(const safelight_config_t *safelight_config)
{
    HAL_StatusTypeDef ret = HAL_OK;
    if (!safelight_config) { return false; }

    uint8_t buf[CONFIG2_SAFELIGHT_CONTROL_SIZE];
    buf[0] = (uint8_t)safelight_config->mode;
    buf[1] = (uint8_t)safelight_config->control;
    buf[3] = safelight_config->dmx_wide_mode;
    copy_from_u16(&buf[4], safelight_config->dmx_address);
    copy_from_u16(&buf[6], safelight_config->dmx_on_value);

    osMutexAcquire(eeprom_i2c_mutex, portMAX_DELAY);
    ret = m24m01_write_buffer(eeprom_i2c, PAGE_CONFIG2 + CONFIG2_SAFELIGHT_CONTROL, buf, sizeof(buf));
    osMutexRelease(eeprom_i2c_mutex);

    if (ret == HAL_OK) {
        memcpy(&setting_safelight_config, safelight_config, sizeof(safelight_config_t));
        return true;
    } else {
        return false;
    }
}

bool settings_get_enlarger_config_name(char *name, uint8_t index)
{
    if (!name || index >= MAX_ENLARGER_CONFIGS) { return false; }

    log_i("Load enlarger config name: %d", index);

    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t data[4 + PROFILE_NAME_LEN];
    memset(data, 0, sizeof(data));

    do {
        osMutexAcquire(eeprom_i2c_mutex, portMAX_DELAY);
        ret = m24m01_read_buffer(eeprom_i2c,
            PAGE_ENLARGER_CONFIG_BASE + (PAGE_SIZE * index),
            data, sizeof(data));
        osMutexRelease(eeprom_i2c_mutex);
        if (ret != HAL_OK) { break; }

        uint32_t config_version = copy_to_u32(data + ENLARGER_CONFIG_VERSION);
        if (config_version == UINT32_MAX) {
            log_d("Config index is empty");
            ret = HAL_ERROR;
            break;
        }
        if (config_version == 0 || config_version > LATEST_ENLARGER_CONFIG_VERSION) {
            log_w("Invalid config version %ld", config_version);
            ret = HAL_ERROR;
            break;
        }

        strncpy(name, (const char *)(data + ENLARGER_CONFIG_NAME), PROFILE_NAME_LEN);
        name[PROFILE_NAME_LEN - 1] = '\0';

    } while (0);

    return (ret == HAL_OK);
}

bool settings_get_enlarger_config_dmx_control(bool *dmx_control, uint8_t index)
{
    if (!dmx_control || index >= MAX_ENLARGER_CONFIGS) { return false; }

    log_i("Load enlarger config dmx_mode: %d", index);

    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t data[5] = {0};

    do {
        osMutexAcquire(eeprom_i2c_mutex, portMAX_DELAY);
        ret = m24m01_read_buffer(eeprom_i2c,
            PAGE_ENLARGER_CONFIG_BASE + (PAGE_SIZE * index),
            data, 4);
        if (ret == HAL_OK) {
            ret = m24m01_read_buffer(eeprom_i2c,
                PAGE_ENLARGER_CONFIG_BASE + (PAGE_SIZE * index)
                + ENLARGER_CONFIG_CONTROL_DMX_ENABLED,
                data + 4, 1);
        }
        osMutexRelease(eeprom_i2c_mutex);
        if (ret != HAL_OK) { break; }

        uint32_t config_version = copy_to_u32(data + ENLARGER_CONFIG_VERSION);
        if (config_version == UINT32_MAX) {
            log_d("Config index is empty");
            ret = HAL_ERROR;
            break;
        }
        if (config_version == 0 || config_version > LATEST_ENLARGER_CONFIG_VERSION) {
            log_w("Invalid config version %ld", config_version);
            ret = HAL_ERROR;
            break;
        }

        *dmx_control = (bool)data[4];
    } while (0);

    return (ret == HAL_OK);
}

bool settings_get_enlarger_config(enlarger_config_t *config, uint8_t index)
{
    if (!config || index >= MAX_ENLARGER_CONFIGS) { return false; }

    log_i("Load enlarger config: %d", index);

    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t data[PAGE_SIZE];
    memset(data, 0, sizeof(data));

    do {
        osMutexAcquire(eeprom_i2c_mutex, portMAX_DELAY);
        ret = m24m01_read_buffer(eeprom_i2c,
            PAGE_ENLARGER_CONFIG_BASE + (PAGE_SIZE * index),
            data, sizeof(data));
        osMutexRelease(eeprom_i2c_mutex);
        if (ret != HAL_OK) { break; }

        uint32_t config_version = copy_to_u32(data + ENLARGER_CONFIG_VERSION);
        if (config_version == UINT32_MAX) {
            log_d("Config index is empty");
            ret = HAL_ERROR;
            break;
        }
        if (config_version == 0 || config_version > LATEST_ENLARGER_CONFIG_VERSION) {
            log_w("Invalid profile version %ld", config_version);
            ret = HAL_ERROR;
            break;
        }

        settings_enlarger_config_parse_page(config, data);
        enlarger_config_recalculate(config);
    } while (0);

    return (ret == HAL_OK);
}

void settings_enlarger_config_parse_page(enlarger_config_t *config, const uint8_t *data)
{
    memset(config, 0, sizeof(enlarger_config_t));

    /* Top-level fields */
    strncpy(config->name, (const char *)(data + ENLARGER_CONFIG_NAME), PROFILE_NAME_LEN);
    config->name[PROFILE_NAME_LEN - 1] = '\0';
    config->contrast_filter = (contrast_filter_t)data[ENLARGER_CONFIG_CONTRAST_FILTER];

    /* Timing profile data */
    config->timing.turn_on_delay = copy_to_u32(data + ENLARGER_CONFIG_TIMING_TURN_ON_DELAY);
    config->timing.rise_time = copy_to_u32(data + ENLARGER_CONFIG_TIMING_RISE_TIME);
    config->timing.rise_time_equiv = copy_to_u32(data + ENLARGER_CONFIG_TIMING_RISE_TIME_EQUIV);
    config->timing.turn_off_delay = copy_to_u32(data + ENLARGER_CONFIG_TIMING_TURN_OFF_DELAY);
    config->timing.fall_time = copy_to_u32(data + ENLARGER_CONFIG_TIMING_FALL_TIME);
    config->timing.fall_time_equiv = copy_to_u32(data + ENLARGER_CONFIG_TIMING_FALL_TIME_EQUIV);

    /* Enlarger control configuration */
    config->control.dmx_control = (bool)data[ENLARGER_CONFIG_CONTROL_DMX_ENABLED];
    config->control.channel_set = (enlarger_channel_set_t)data[ENLARGER_CONFIG_CONTROL_CHANNEL_SET];
    config->control.dmx_wide_mode = (bool)data[ENLARGER_CONFIG_CONTROL_DMX_WIDE];
    config->control.dmx_channel_red = copy_to_u16(data + ENLARGER_CONFIG_CONTROL_CHANNEL_ID_R);
    config->control.dmx_channel_green = copy_to_u16(data + ENLARGER_CONFIG_CONTROL_CHANNEL_ID_G);
    config->control.dmx_channel_blue = copy_to_u16(data + ENLARGER_CONFIG_CONTROL_CHANNEL_ID_B);
    config->control.dmx_channel_white = copy_to_u16(data + ENLARGER_CONFIG_CONTROL_CHANNEL_ID_W);
    config->control.contrast_mode = (enlarger_contrast_mode_t)data[ENLARGER_CONFIG_CONTROL_CONTRAST_MODE];
    config->control.focus_value = copy_to_u16(data + ENLARGER_CONFIG_CONTROL_FOCUS_VALUE);
    config->control.safe_value = copy_to_u16(data + ENLARGER_CONFIG_CONTROL_SAFE_VALUE);

    size_t offset = ENLARGER_CONFIG_CONTROL_GRADE_VALUES;
    for (size_t i = 0; i < CONTRAST_WHOLE_GRADE_COUNT; i++) {
        config->control.grade_values[CONTRAST_WHOLE_GRADES[i]].channel_red = copy_to_u16(data + offset);
        offset += 2;
        config->control.grade_values[CONTRAST_WHOLE_GRADES[i]].channel_green = copy_to_u16(data + offset);
        offset += 2;
        config->control.grade_values[CONTRAST_WHOLE_GRADES[i]].channel_blue = copy_to_u16(data + offset);
        offset += 2;
        config->control.grade_values[CONTRAST_WHOLE_GRADES[i]].channel_white = copy_to_u16(data + offset);
        offset += 2;
    }
}

bool settings_set_enlarger_config(const enlarger_config_t *config, uint8_t index)
{
    if (!config || index >= MAX_ENLARGER_CONFIGS) { return false; }

    log_i("Save enlarger config: %d", index);

    uint8_t data[PAGE_SIZE];
    memset(data, 0, sizeof(data));

    settings_enlarger_config_populate_page(config, data);

    osMutexAcquire(eeprom_i2c_mutex, portMAX_DELAY);
    HAL_StatusTypeDef ret = m24m01_write_page(eeprom_i2c,
        PAGE_ENLARGER_CONFIG_BASE + (PAGE_SIZE * index),
        data, sizeof(data));
    osMutexRelease(eeprom_i2c_mutex);
    return (ret == HAL_OK);
}

void settings_enlarger_config_populate_page(const enlarger_config_t *config, uint8_t *data)
{
    copy_from_u32(data + ENLARGER_CONFIG_VERSION, LATEST_ENLARGER_CONFIG_VERSION);

    /* Top-level fields */
    strncpy((char *)(data + ENLARGER_CONFIG_NAME), config->name, PROFILE_NAME_LEN);
    data[ENLARGER_CONFIG_CONTRAST_FILTER] = (uint8_t)config->contrast_filter;

    /* Timing profile data */
    copy_from_u32(data + ENLARGER_CONFIG_TIMING_TURN_ON_DELAY,   config->timing.turn_on_delay);
    copy_from_u32(data + ENLARGER_CONFIG_TIMING_RISE_TIME,       config->timing.rise_time);
    copy_from_u32(data + ENLARGER_CONFIG_TIMING_RISE_TIME_EQUIV, config->timing.rise_time_equiv);
    copy_from_u32(data + ENLARGER_CONFIG_TIMING_TURN_OFF_DELAY,  config->timing.turn_off_delay);
    copy_from_u32(data + ENLARGER_CONFIG_TIMING_FALL_TIME,       config->timing.fall_time);
    copy_from_u32(data + ENLARGER_CONFIG_TIMING_FALL_TIME_EQUIV, config->timing.fall_time_equiv);

    /* Enlarger control configuration */
    data[ENLARGER_CONFIG_CONTROL_DMX_ENABLED] = (uint8_t)config->control.dmx_control;
    data[ENLARGER_CONFIG_CONTROL_CHANNEL_SET] = (uint8_t)config->control.channel_set;
    data[ENLARGER_CONFIG_CONTROL_DMX_WIDE] = (uint8_t)config->control.dmx_wide_mode;
    copy_from_u16(data + ENLARGER_CONFIG_CONTROL_CHANNEL_ID_R, config->control.dmx_channel_red);
    copy_from_u16(data + ENLARGER_CONFIG_CONTROL_CHANNEL_ID_G, config->control.dmx_channel_green);
    copy_from_u16(data + ENLARGER_CONFIG_CONTROL_CHANNEL_ID_B, config->control.dmx_channel_blue);
    copy_from_u16(data + ENLARGER_CONFIG_CONTROL_CHANNEL_ID_W, config->control.dmx_channel_white);
    data[ENLARGER_CONFIG_CONTROL_CONTRAST_MODE] = (uint8_t)config->control.contrast_mode;
    copy_from_u16(data + ENLARGER_CONFIG_CONTROL_FOCUS_VALUE, config->control.focus_value);
    copy_from_u16(data + ENLARGER_CONFIG_CONTROL_SAFE_VALUE, config->control.safe_value);

    size_t offset = ENLARGER_CONFIG_CONTROL_GRADE_VALUES;
    for (size_t i = 0; i < CONTRAST_WHOLE_GRADE_COUNT; i++) {
        copy_from_u16(data + offset, config->control.grade_values[CONTRAST_WHOLE_GRADES[i]].channel_red);
        offset += 2;
        copy_from_u16(data + offset, config->control.grade_values[CONTRAST_WHOLE_GRADES[i]].channel_green);
        offset += 2;
        copy_from_u16(data + offset, config->control.grade_values[CONTRAST_WHOLE_GRADES[i]].channel_blue);
        offset += 2;
        copy_from_u16(data + offset, config->control.grade_values[CONTRAST_WHOLE_GRADES[i]].channel_white);
        offset += 2;
    }
}

void settings_clear_enlarger_config(uint8_t index)
{
    if (index >= MAX_ENLARGER_CONFIGS) { return; }

    uint8_t data[PAGE_SIZE];

    /* Read the config page, and abort if it is already blank */
    if (m24m01_read_buffer(eeprom_i2c,
        PAGE_ENLARGER_CONFIG_BASE + (PAGE_SIZE * index),
        data, sizeof(data)) == HAL_OK) {
        bool is_empty = true;
        for (size_t i = 0; i < PAGE_SIZE; i++) {
            if (data[i] != 0xFF) {
                is_empty = false;
                break;
            }
        }
        if (is_empty) {
            return;
        }
    }

    memset(data, 0xFF, sizeof(data));

    log_i("Clear enlarger profile: %d", index);

    osMutexAcquire(eeprom_i2c_mutex, portMAX_DELAY);
    m24m01_write_page(eeprom_i2c,
        PAGE_ENLARGER_CONFIG_BASE + (PAGE_SIZE * index),
        data, sizeof(data));
    osMutexRelease(eeprom_i2c_mutex);
}

bool settings_get_paper_profile(paper_profile_t *profile, uint8_t index)
{
    if (!profile || index >= MAX_PAPER_PROFILES) { return false; }

    log_i("Load paper profile: %d", index);

    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t data[PAGE_SIZE];
    memset(data, 0, sizeof(data));

    do {
        osMutexAcquire(eeprom_i2c_mutex, portMAX_DELAY);
        ret = m24m01_read_buffer(eeprom_i2c,
            PAGE_PAPER_PROFILE_BASE + (PAGE_SIZE * index),
            data, sizeof(data));
        osMutexRelease(eeprom_i2c_mutex);
        if (ret != HAL_OK) { break; }

        uint32_t profile_version = copy_to_u32(data + PAPER_PROFILE_VERSION);
        if (profile_version == UINT32_MAX) {
            log_d("Profile index is empty");
            ret = HAL_ERROR;
            break;
        }
        if (profile_version == 0 || profile_version > LATEST_PAPER_PROFILE_VERSION) {
            log_w("Invalid profile version %ld", profile_version);
            ret = HAL_ERROR;
            break;
        }

        settings_paper_profile_parse_page(profile, data);
        paper_profile_recalculate(profile);

    } while (0);

    return (ret == HAL_OK);
}

static void settings_paper_profile_parse_page(paper_profile_t *profile, const uint8_t *data)
{
    memset(profile, 0, sizeof(paper_profile_t));

    strncpy(profile->name, (const char *)(data + PAPER_PROFILE_NAME), PROFILE_NAME_LEN);
    profile->name[PROFILE_NAME_LEN - 1] = '\0';

    profile->grade[CONTRAST_GRADE_00].ht_lev100 = copy_to_u32(data + PAPER_PROFILE_GRADE00_HT);
    profile->grade[CONTRAST_GRADE_00].hm_lev100 = copy_to_u32(data + PAPER_PROFILE_GRADE00_HM);
    profile->grade[CONTRAST_GRADE_00].hs_lev100 = copy_to_u32(data + PAPER_PROFILE_GRADE00_HS);

    profile->grade[CONTRAST_GRADE_0].ht_lev100 = copy_to_u32(data + PAPER_PROFILE_GRADE0_HT);
    profile->grade[CONTRAST_GRADE_0].hm_lev100 = copy_to_u32(data + PAPER_PROFILE_GRADE0_HM);
    profile->grade[CONTRAST_GRADE_0].hs_lev100 = copy_to_u32(data + PAPER_PROFILE_GRADE0_HS);

    profile->grade[CONTRAST_GRADE_1].ht_lev100 = copy_to_u32(data + PAPER_PROFILE_GRADE1_HT);
    profile->grade[CONTRAST_GRADE_1].hm_lev100 = copy_to_u32(data + PAPER_PROFILE_GRADE1_HM);
    profile->grade[CONTRAST_GRADE_1].hs_lev100 = copy_to_u32(data + PAPER_PROFILE_GRADE1_HS);

    profile->grade[CONTRAST_GRADE_2].ht_lev100 = copy_to_u32(data + PAPER_PROFILE_GRADE2_HT);
    profile->grade[CONTRAST_GRADE_2].hm_lev100 = copy_to_u32(data + PAPER_PROFILE_GRADE2_HM);
    profile->grade[CONTRAST_GRADE_2].hs_lev100 = copy_to_u32(data + PAPER_PROFILE_GRADE2_HS);

    profile->grade[CONTRAST_GRADE_3].ht_lev100 = copy_to_u32(data + PAPER_PROFILE_GRADE3_HT);
    profile->grade[CONTRAST_GRADE_3].hm_lev100 = copy_to_u32(data + PAPER_PROFILE_GRADE3_HM);
    profile->grade[CONTRAST_GRADE_3].hs_lev100 = copy_to_u32(data + PAPER_PROFILE_GRADE3_HS);

    profile->grade[CONTRAST_GRADE_4].ht_lev100 = copy_to_u32(data + PAPER_PROFILE_GRADE4_HT);
    profile->grade[CONTRAST_GRADE_4].hm_lev100 = copy_to_u32(data + PAPER_PROFILE_GRADE4_HM);
    profile->grade[CONTRAST_GRADE_4].hs_lev100 = copy_to_u32(data + PAPER_PROFILE_GRADE4_HS);

    profile->grade[CONTRAST_GRADE_5].ht_lev100 = copy_to_u32(data + PAPER_PROFILE_GRADE5_HT);
    profile->grade[CONTRAST_GRADE_5].hm_lev100 = copy_to_u32(data + PAPER_PROFILE_GRADE5_HM);
    profile->grade[CONTRAST_GRADE_5].hs_lev100 = copy_to_u32(data + PAPER_PROFILE_GRADE5_HS);

    profile->max_net_density = copy_to_f32(data + PAPER_PROFILE_DNET);
    if (profile->max_net_density != NAN && (!isnormal(profile->max_net_density) || profile->max_net_density < 0.0F)) {
        profile->max_net_density = NAN;
    }
}

bool settings_set_paper_profile(const paper_profile_t *profile, uint8_t index)
{
    if (!profile || index >= MAX_PAPER_PROFILES) { return false; }

    log_i("Save paper profile: %d", index);

    uint8_t data[PAGE_SIZE];
    memset(data, 0, sizeof(data));

    settings_paper_profile_populate_page(profile, data);

    osMutexAcquire(eeprom_i2c_mutex, portMAX_DELAY);
    HAL_StatusTypeDef ret = m24m01_write_page(eeprom_i2c,
        PAGE_PAPER_PROFILE_BASE + (PAGE_SIZE * index),
        data, sizeof(data));
    osMutexRelease(eeprom_i2c_mutex);
    return (ret == HAL_OK);
}

void settings_paper_profile_populate_page(const paper_profile_t *profile, uint8_t *data)
{
    copy_from_u32(data + PAPER_PROFILE_VERSION, LATEST_PAPER_PROFILE_VERSION);

    strncpy((char *)(data + PAPER_PROFILE_NAME), profile->name, PROFILE_NAME_LEN);

    copy_from_u32(data + PAPER_PROFILE_GRADE00_HT, profile->grade[CONTRAST_GRADE_00].ht_lev100);
    copy_from_u32(data + PAPER_PROFILE_GRADE00_HM, profile->grade[CONTRAST_GRADE_00].hm_lev100);
    copy_from_u32(data + PAPER_PROFILE_GRADE00_HS, profile->grade[CONTRAST_GRADE_00].hs_lev100);

    copy_from_u32(data + PAPER_PROFILE_GRADE0_HT, profile->grade[CONTRAST_GRADE_0].ht_lev100);
    copy_from_u32(data + PAPER_PROFILE_GRADE0_HM, profile->grade[CONTRAST_GRADE_0].hm_lev100);
    copy_from_u32(data + PAPER_PROFILE_GRADE0_HS, profile->grade[CONTRAST_GRADE_0].hs_lev100);

    copy_from_u32(data + PAPER_PROFILE_GRADE1_HT, profile->grade[CONTRAST_GRADE_1].ht_lev100);
    copy_from_u32(data + PAPER_PROFILE_GRADE1_HM, profile->grade[CONTRAST_GRADE_1].hm_lev100);
    copy_from_u32(data + PAPER_PROFILE_GRADE1_HS, profile->grade[CONTRAST_GRADE_1].hs_lev100);

    copy_from_u32(data + PAPER_PROFILE_GRADE2_HT, profile->grade[CONTRAST_GRADE_2].ht_lev100);
    copy_from_u32(data + PAPER_PROFILE_GRADE2_HM, profile->grade[CONTRAST_GRADE_2].hm_lev100);
    copy_from_u32(data + PAPER_PROFILE_GRADE2_HS, profile->grade[CONTRAST_GRADE_2].hs_lev100);

    copy_from_u32(data + PAPER_PROFILE_GRADE3_HT, profile->grade[CONTRAST_GRADE_3].ht_lev100);
    copy_from_u32(data + PAPER_PROFILE_GRADE3_HM, profile->grade[CONTRAST_GRADE_3].hm_lev100);
    copy_from_u32(data + PAPER_PROFILE_GRADE3_HS, profile->grade[CONTRAST_GRADE_3].hs_lev100);

    copy_from_u32(data + PAPER_PROFILE_GRADE4_HT, profile->grade[CONTRAST_GRADE_4].ht_lev100);
    copy_from_u32(data + PAPER_PROFILE_GRADE4_HM, profile->grade[CONTRAST_GRADE_4].hm_lev100);
    copy_from_u32(data + PAPER_PROFILE_GRADE4_HS, profile->grade[CONTRAST_GRADE_4].hs_lev100);

    copy_from_u32(data + PAPER_PROFILE_GRADE5_HT, profile->grade[CONTRAST_GRADE_5].ht_lev100);
    copy_from_u32(data + PAPER_PROFILE_GRADE5_HM, profile->grade[CONTRAST_GRADE_5].hm_lev100);
    copy_from_u32(data + PAPER_PROFILE_GRADE5_HS, profile->grade[CONTRAST_GRADE_5].hs_lev100);

    copy_from_f32(data + PAPER_PROFILE_DNET, profile->max_net_density);
}

void settings_clear_paper_profile(uint8_t index)
{
    if (index >= MAX_PAPER_PROFILES) { return; }

    uint8_t data[PAGE_SIZE];

    /* Read the profile page, and abort if it is already blank */
    if (m24m01_read_buffer(eeprom_i2c,
        PAGE_PAPER_PROFILE_BASE + (PAGE_SIZE * index),
        data, sizeof(data)) == HAL_OK) {
        bool is_empty = true;
        for (size_t i = 0; i < PAGE_SIZE; i++) {
            if (data[i] != 0xFF) {
                is_empty = false;
                break;
            }
        }
        if (is_empty) {
            return;
        }
    }

    memset(data, 0xFF, sizeof(data));

    log_i("Clear paper profile: %d", index);

    osMutexAcquire(eeprom_i2c_mutex, portMAX_DELAY);
    m24m01_write_page(eeprom_i2c,
        PAGE_PAPER_PROFILE_BASE + (PAGE_SIZE * index),
        data, sizeof(data));
    osMutexRelease(eeprom_i2c_mutex);
}

bool settings_get_step_wedge(step_wedge_t **wedge)
{
    if (!wedge) { return false; }

    log_i("Load step wedge");

    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t data[PAGE_SIZE];
    memset(data, 0, sizeof(data));

    do {
        osMutexAcquire(eeprom_i2c_mutex, portMAX_DELAY);
        ret = m24m01_read_buffer(eeprom_i2c, PAGE_STEP_WEDGE_BASE, data, sizeof(data));
        osMutexRelease(eeprom_i2c_mutex);
        if (ret != HAL_OK) { break; }

        uint32_t wedge_version = copy_to_u32(data + STEP_WEDGE_VERSION);
        if (wedge_version == UINT32_MAX) {
            log_d("Step wedge is empty");
            ret = HAL_ERROR;
            break;
        }
        if (wedge_version == 0 || wedge_version > LATEST_STEP_WEDGE_VERSION) {
            log_w("Invalid step wedge version %ld", wedge_version);
            ret = HAL_ERROR;
            break;
        }

        settings_step_wedge_parse_page(wedge, data);

    } while (0);

    return (ret == HAL_OK) && *wedge;
}

void settings_step_wedge_parse_page(step_wedge_t **wedge, const uint8_t *data)
{
    uint32_t step_count = copy_to_u32(data + STEP_WEDGE_STEP_COUNT);
    if (step_count < MIN_STEP_WEDGE_STEP_COUNT || step_count > MAX_STEP_WEDGE_STEP_COUNT) {
        log_w("Invalid step count: %lu", step_count);
        return;
    }

    *wedge = step_wedge_create(step_count);
    if (!(*wedge)) {
        log_w("Unable to create step wedge");
        return;
    }

    strncpy((*wedge)->name, (const char *)(data + PAPER_PROFILE_NAME), PROFILE_NAME_LEN);
    (*wedge)->name[31] = '\0';

    float val = copy_to_f32(data + STEP_WEDGE_BASE_DENSITY);
    if (isnormal(val) || fpclassify(val) == FP_ZERO) {
        (*wedge)->base_density = val;
    }

    val = copy_to_f32(data + STEP_WEDGE_DENSITY_INCREMENT);
    if (isnormal(val) || fpclassify(val) == FP_ZERO) {
        (*wedge)->density_increment = val;
    }

    for (size_t i = 0; i < step_count; i++) {
        val = copy_to_f32(data + STEP_WEDGE_STEP_DENSITY_0 + (4 * i));
        if (isnormal(val) || fpclassify(val) == FP_ZERO) {
            (*wedge)->step_density[i] = val;
        }
    }
}

bool settings_set_step_wedge(const step_wedge_t *wedge)
{
    if (!wedge) { return false; }

    log_i("Save step wedge");

    uint8_t data[PAGE_SIZE];
    memset(data, 0, sizeof(data));

    settings_step_wedge_populate_page(wedge, data);

    osMutexAcquire(eeprom_i2c_mutex, portMAX_DELAY);
    HAL_StatusTypeDef ret = m24m01_write_page(eeprom_i2c, PAGE_STEP_WEDGE_BASE, data, sizeof(data));
    osMutexRelease(eeprom_i2c_mutex);
    return (ret == HAL_OK);
}

void settings_step_wedge_populate_page(const step_wedge_t *wedge, uint8_t *data)
{
    copy_from_u32(data + STEP_WEDGE_VERSION, LATEST_STEP_WEDGE_VERSION);

    strncpy((char *)(data + STEP_WEDGE_NAME), wedge->name, PROFILE_NAME_LEN);

    copy_from_f32(data + STEP_WEDGE_BASE_DENSITY, wedge->base_density);
    copy_from_f32(data + STEP_WEDGE_DENSITY_INCREMENT, wedge->density_increment);
    copy_from_u32(data + STEP_WEDGE_STEP_COUNT, wedge->step_count);

    for (size_t i = 0; i < wedge->step_count; i++) {
        copy_from_f32(data + STEP_WEDGE_STEP_DENSITY_0 + (4 * i), wedge->step_density[i]);
    }
}

bool settings_set_bootloader_firmware(const char *dev_serial, uint32_t checksum, const char *file_path)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t data[PAGE_SIZE];

    memset(data, 0x00, PAGE_SIZE);
    data[BOOTLOADER_COMMAND] = 0xBB;

    strncpy((char *)(data + BOOTLOADER_FW_DEVICE), dev_serial, 21);
    data[BOOTLOADER_FW_DEVICE + 20] = '\0';

    copy_from_u32(data + BOOTLOADER_FW_CHECKSUM, checksum);

    ret = m24m01_write_page(eeprom_i2c, PAGE_BOOTLOADER, data, PAGE_SIZE);
    if (ret != HAL_OK) {
        log_e("Unable to write first bootloader page: %d", ret);
        return false;
    }

    memset(data, 0x00, PAGE_SIZE);
    strncpy((char *)data, file_path, PAGE_SIZE);
    data[PAGE_SIZE - 1] = '\0';

    ret = m24m01_write_page(eeprom_i2c, PAGE_BOOTLOADER + BOOTLOADER_FW_FILE, data, PAGE_SIZE);
    if (ret != HAL_OK) {
        log_e("Unable to write second bootloader page: %d", ret);
        return false;
    }

    return true;
}

bool settings_cleanup_bootloader_firmware()
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t data[PAGE_SIZE];
    bool erase_page = false;

    do {
        /* Read the first 16 bytes of the bootloader page into a buffer */
        ret = m24m01_read_buffer(eeprom_i2c, PAGE_BOOTLOADER, data, 16);
        if (ret != HAL_OK) {
            log_e("Unable to read bootloader prefix: %d", ret);
            break;
        }

        /* If anything in this section was set, then mark it all to be erased */
        for (uint8_t i = 0; i < 16; i++) {
            if (data[i] != 0xFF) {
                erase_page = true;
                break;
            }
        }

        if (erase_page) {
            log_i("Clearing bootloader settings pages");
            memset(data, 0xFF, PAGE_SIZE);
            ret = m24m01_write_page(eeprom_i2c, PAGE_BOOTLOADER, data, PAGE_SIZE);
            if (ret != HAL_OK) {
                log_e("Unable to clear first bootloader page: %d", ret);
                break;
            }

            ret = m24m01_write_page(eeprom_i2c, PAGE_BOOTLOADER + PAGE_SIZE, data, PAGE_SIZE);
            if (ret != HAL_OK) {
                log_e("Unable to clear second bootloader page: %d", ret);
                break;
            }
        }
    } while (0);

    return (ret == HAL_OK);
}

bool read_u32(uint32_t address, uint32_t *val)
{
    uint8_t data[4];
    osMutexAcquire(eeprom_i2c_mutex, portMAX_DELAY);
    bool result = m24m01_read_buffer(eeprom_i2c, address, data, sizeof(data)) == HAL_OK;
    osMutexRelease(eeprom_i2c_mutex);
    if (result && val) {
        *val = copy_to_u32(data);
    }
    return result;
}

bool write_u32(uint32_t address, uint32_t val)
{
    uint8_t data[4];
    copy_from_u32(data, val);
    osMutexAcquire(eeprom_i2c_mutex, portMAX_DELAY);
    bool result = m24m01_write_buffer(eeprom_i2c, address, data, sizeof(data)) == HAL_OK;
    osMutexRelease(eeprom_i2c_mutex);
    return result;
}

bool write_f32(uint32_t address, float val)
{
    uint8_t data[4];
    copy_from_f32(data, val);
    osMutexAcquire(eeprom_i2c_mutex, portMAX_DELAY);
    bool result = m24m01_write_buffer(eeprom_i2c, address, data, sizeof(data)) == HAL_OK;
    osMutexRelease(eeprom_i2c_mutex);
    return result;
}
