#include "meter_probe_settings.h"

#include <string.h>
#include <math.h>

#define LOG_TAG "meter_probe_settings"
#include <elog.h>

#include "settings_util.h"
#include "m24c08.h"
#include "tsl2585.h"

extern CRC_HandleTypeDef hcrc;

#define LATEST_CAL_TSL2585_VERSION 1

/*
 * The first 256B are reserved for FT260 configuration and shall never be
 * modified by this code. If the format is documented, then it can be read
 * for some of the device ID information. However, that can also be read
 * out of the USB device properties.
 */
#define PAGE_RESERVED          0x000UL
#define PAGE_RESERVED_SIZE     (256UL)

/* Meter Probe Memory Header (16B) */
#define PAGE_HEADER            0x100UL
#define PAGE_HEADER_SIZE       (16UL)

#define HEADER_MAGIC             0 /* 3B = {'D', 'P', 'D'} */
#define HEADER_VERSION           3 /* 1B (uint8_t) */
#define HEADER_DEV_TYPE          4 /* 1B (uint8_t) */
#define HEADER_DEV_REV_MAJOR     5 /* 1B (uint8_t) */
#define HEADER_DEV_REV_MINOR     6 /* 1B (uint8_t) */

#define PAGE_CAL               0x110UL
#define PAGE_CAL_SIZE          (112UL)

/* TSL2585 Data Format Header (16B) */
#define CAL_TSL2585_VERSION      0 /* 4B (uint32_t) */
#define CAL_TSL2585_RESERVED0    4 /* 12B (for page alignment) */

/* TSL2585 Gain Calibration (48B) */
#define CAL_TSL2585_GAIN_0_5X   16 /* 4B (float) */
#define CAL_TSL2585_GAIN_1X     20 /* 4B (float) */
#define CAL_TSL2585_GAIN_2X     24 /* 4B (float) */
#define CAL_TSL2585_GAIN_4X     28 /* 4B (float) */
#define CAL_TSL2585_GAIN_8X     32 /* 4B (float) */
#define CAL_TSL2585_GAIN_16X    36 /* 4B (float) */
#define CAL_TSL2585_GAIN_32X    40 /* 4B (float) */
#define CAL_TSL2585_GAIN_64X    44 /* 4B (float) */
#define CAL_TSL2585_GAIN_128X   48 /* 4B (float) */
#define CAL_TSL2585_GAIN_256X   52 /* 4B (float) */
#define CAL_TSL2585_RESERVED1   56 /* 4B (for page alignment) */
#define CAL_TSL2585_GAIN_CRC    60 /* 4B (uint32_t) */

/* TSL2585 Slope Calibration (16B) */
#define CAL_TSL2585_SLOPE_RESERVED 64 /* 16B (unused) */

/* TSL2585 Target Calibration (16B) */
#define CAL_TSL2585_TARGET_LUX_SLOPE     80 /* 4B (float) */
#define CAL_TSL2585_TARGET_LUX_INTERCEPT 84 /* 4B (float) */
#define CAL_TSL2585_RESERVED2            88 /* 4B (for page alignment) */
#define CAL_TSL2585_TARGET_CRC           92 /* 4B (uint32_t) */

HAL_StatusTypeDef meter_probe_settings_init(peripheral_settings_handle_t *handle, i2c_handle_t *hi2c)
{
    HAL_StatusTypeDef ret;
    uint8_t header_data[PAGE_HEADER_SIZE];

    if (!handle || handle->initialized || !hi2c) { return HAL_ERROR; }

    do {
        /* Read the identification area of the meter probe memory */
        ret = m24c08_read_buffer(hi2c, PAGE_HEADER, header_data, sizeof(header_data));
        if (ret != HAL_OK) { break; }

        /* Verify the memory header prefix */
        if (header_data[0] != 'D'
            && header_data[1] != 'P'
            && header_data[2] != 'D'
            && header_data[3] != 0x01) {
            log_w("Unexpected memory header: %02X%02X%02X%02X",
                header_data[0], header_data[1], header_data[2], header_data[3]);
            ret = HAL_ERROR;
            break;
        }

        handle->hi2c = hi2c;

        /* Read the probe type information out of the rest of the ID page */
        handle->type = header_data[HEADER_DEV_TYPE];
        handle->id.rev_major = header_data[HEADER_DEV_REV_MAJOR];
        handle->id.rev_minor = header_data[HEADER_DEV_REV_MINOR];

        if (handle->type == METER_PROBE_TYPE_UNKNOWN) {
            log_w("Probe type is not set, defaulting to baseline");
            handle->type = METER_PROBE_TYPE_BASELINE;
        }

        handle->hi2c = hi2c;

        handle->initialized = true;
    } while (0);

    return ret;
}

HAL_StatusTypeDef meter_probe_settings_clear(i2c_handle_t *hi2c)
{
    HAL_StatusTypeDef ret = HAL_OK;

    if (!hi2c) { return HAL_ERROR; }

    uint8_t data[M24C08_PAGE_SIZE];
    memset(data, 0xFF, sizeof(data));

    log_i("meter_probe_settings_clear");

    /*
     * This loop clears everything except the reserved region at the start of
     * the EEPROM. This area must remain untouched because it is used to
     * configure the FT260 USB controller on power-up.
     */
    for (size_t addr = (PAGE_RESERVED + PAGE_RESERVED_SIZE); addr < M24C08_MEM_SIZE; addr += M24C08_PAGE_SIZE) {
        ret = m24c08_write_page(hi2c, addr, data, sizeof(data));
        if (ret != HAL_OK) { break; }
    }

    return ret;
}

HAL_StatusTypeDef meter_probe_settings_get(const peripheral_settings_handle_t *handle,
    meter_probe_settings_t *settings)
{
    HAL_StatusTypeDef ret = HAL_OK;
    if (!handle || !settings) { return HAL_ERROR; }
    if (!handle->initialized) { return HAL_ERROR; }
    if (handle->type != METER_PROBE_TYPE_BASELINE) {
        return HAL_ERROR;
    }

    uint8_t data[PAGE_CAL_SIZE];
    uint32_t version;
    uint32_t crc;
    uint32_t calculated_crc;

    /* Read the whole data buffer */
    ret = m24c08_read_buffer(handle->hi2c, PAGE_CAL, data, PAGE_CAL_SIZE);
    if (ret != HAL_OK) { return ret; }

    /* Get the version */
    version = copy_to_u32(data + CAL_TSL2585_VERSION);

    if (version != LATEST_CAL_TSL2585_VERSION) {
        log_w("Unexpected TSL2585 cal version: %d != %d", version, LATEST_CAL_TSL2585_VERSION);
        return HAL_ERROR;
    }

    memset(settings, 0, sizeof(meter_probe_settings_t));
    settings->type = handle->type;

    /* Validate the gain data CRC */
    crc = copy_to_u32(data + CAL_TSL2585_GAIN_CRC);
    calculated_crc = HAL_CRC_Calculate(&hcrc,
        (uint32_t *)(data + CAL_TSL2585_GAIN_0_5X),
        (CAL_TSL2585_GAIN_CRC - CAL_TSL2585_GAIN_0_5X) / 4UL);
    if (crc == calculated_crc) {
        /* Parse the gain calibration data */
        peripheral_cal_gain_t *cal_gain = &settings->cal_gain;
        cal_gain->values[TSL2585_GAIN_0_5X] = copy_to_f32(data + CAL_TSL2585_GAIN_0_5X);
        cal_gain->values[TSL2585_GAIN_1X] = copy_to_f32(data + CAL_TSL2585_GAIN_1X);
        cal_gain->values[TSL2585_GAIN_2X] = copy_to_f32(data + CAL_TSL2585_GAIN_2X);
        cal_gain->values[TSL2585_GAIN_4X] = copy_to_f32(data + CAL_TSL2585_GAIN_4X);
        cal_gain->values[TSL2585_GAIN_8X] = copy_to_f32(data + CAL_TSL2585_GAIN_8X);
        cal_gain->values[TSL2585_GAIN_16X] = copy_to_f32(data + CAL_TSL2585_GAIN_16X);
        cal_gain->values[TSL2585_GAIN_32X] = copy_to_f32(data + CAL_TSL2585_GAIN_32X);
        cal_gain->values[TSL2585_GAIN_64X] = copy_to_f32(data + CAL_TSL2585_GAIN_64X);
        cal_gain->values[TSL2585_GAIN_128X] = copy_to_f32(data + CAL_TSL2585_GAIN_128X);
        cal_gain->values[TSL2585_GAIN_256X] = copy_to_f32(data + CAL_TSL2585_GAIN_256X);
    } else {
        log_w("Invalid TSL2585 cal gain CRC: %08X != %08X", crc, calculated_crc);
        for (size_t i = 0; i <= TSL2585_GAIN_256X; i++) {
            settings->cal_gain.values[i] = NAN;
        }
    }

    /* Validate the target data CRC */
    crc = copy_to_u32(data + CAL_TSL2585_TARGET_CRC);
    calculated_crc = HAL_CRC_Calculate(&hcrc,
        (uint32_t *)(data + CAL_TSL2585_TARGET_LUX_SLOPE),
        (CAL_TSL2585_TARGET_CRC - CAL_TSL2585_TARGET_LUX_SLOPE) / 4UL);
    if (crc == calculated_crc) {
        /* Parse the target calibration data */
        peripheral_cal_linear_target_t *cal_target = &settings->cal_target;
        cal_target->lux_slope = copy_to_f32(data + CAL_TSL2585_TARGET_LUX_SLOPE);
        cal_target->lux_intercept = copy_to_f32(data + CAL_TSL2585_TARGET_LUX_INTERCEPT);
    } else {
        log_w("Invalid TSL2585 cal target CRC: %08X != %08X", crc, calculated_crc);
        peripheral_cal_linear_target_t *cal_target = &settings->cal_target;
        cal_target->lux_slope = NAN;
        cal_target->lux_intercept = NAN;
    }

    return ret;
}

HAL_StatusTypeDef meter_probe_settings_set(const peripheral_settings_handle_t *handle,
    const meter_probe_settings_t *settings)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint32_t crc;

    if (!handle || !settings) { return HAL_ERROR; }
    if (!handle->initialized || handle->type != settings->type) { return HAL_ERROR; }

    /* Prepare an empty buffer */
    uint8_t data[PAGE_CAL_SIZE] = {0};

    /* Set the version */
    copy_from_u32(data + CAL_TSL2585_VERSION, LATEST_CAL_TSL2585_VERSION);

    /* Fill in the gain calibration data */
    const peripheral_cal_gain_t *cal_gain = &settings->cal_gain;
    copy_from_f32(data + CAL_TSL2585_GAIN_0_5X, cal_gain->values[TSL2585_GAIN_0_5X]);
    copy_from_f32(data + CAL_TSL2585_GAIN_1X, cal_gain->values[TSL2585_GAIN_1X]);
    copy_from_f32(data + CAL_TSL2585_GAIN_2X, cal_gain->values[TSL2585_GAIN_2X]);
    copy_from_f32(data + CAL_TSL2585_GAIN_4X, cal_gain->values[TSL2585_GAIN_4X]);
    copy_from_f32(data + CAL_TSL2585_GAIN_8X, cal_gain->values[TSL2585_GAIN_8X]);
    copy_from_f32(data + CAL_TSL2585_GAIN_16X, cal_gain->values[TSL2585_GAIN_16X]);
    copy_from_f32(data + CAL_TSL2585_GAIN_32X, cal_gain->values[TSL2585_GAIN_32X]);
    copy_from_f32(data + CAL_TSL2585_GAIN_64X, cal_gain->values[TSL2585_GAIN_64X]);
    copy_from_f32(data + CAL_TSL2585_GAIN_128X, cal_gain->values[TSL2585_GAIN_128X]);
    copy_from_f32(data + CAL_TSL2585_GAIN_256X, cal_gain->values[TSL2585_GAIN_256X]);

    crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)(data + CAL_TSL2585_GAIN_0_5X),
        (CAL_TSL2585_GAIN_CRC - CAL_TSL2585_GAIN_0_5X) / 4UL);
    copy_from_u32(data + CAL_TSL2585_GAIN_CRC, crc);

    /* Fill in the target calibration data */
    const peripheral_cal_linear_target_t *cal_target = &settings->cal_target;
    copy_from_f32(data + CAL_TSL2585_TARGET_LUX_SLOPE, cal_target->lux_slope);
    copy_from_f32(data + CAL_TSL2585_TARGET_LUX_INTERCEPT, cal_target->lux_intercept);

    crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)(data + CAL_TSL2585_TARGET_LUX_SLOPE),
        (CAL_TSL2585_TARGET_CRC - CAL_TSL2585_TARGET_LUX_SLOPE) / 4UL);
    copy_from_u32(data + CAL_TSL2585_TARGET_CRC, crc);

    /* Write the whole data buffer */
    ret = m24c08_write_buffer(handle->hi2c, PAGE_CAL, data, PAGE_CAL_SIZE);
    if (ret != HAL_OK) { return ret; }

    return ret;
}

HAL_StatusTypeDef meter_probe_settings_set_target(const peripheral_settings_handle_t *handle,
    const peripheral_cal_linear_target_t *cal_target)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t data[(CAL_TSL2585_TARGET_CRC - CAL_TSL2585_TARGET_LUX_SLOPE) + 4];
    const size_t offset = CAL_TSL2585_TARGET_LUX_SLOPE;
    uint32_t version;
    uint32_t crc;

    if (!handle || !cal_target) { return HAL_ERROR; }
    if (!handle->initialized || handle->type != METER_PROBE_TYPE_BASELINE) { return HAL_ERROR; }

    /* Read just the version */
    ret = m24c08_read_buffer(handle->hi2c, CAL_TSL2585_VERSION, data, 4);
    if (ret != HAL_OK) { return ret; }

    /* Parse and validate the version */
    version = copy_to_u32(data);

    if (version != LATEST_CAL_TSL2585_VERSION) {
        log_w("Unexpected TSL2585 cal version: %d != %d", version, LATEST_CAL_TSL2585_VERSION);
        return HAL_ERROR;
    }

    /* Fill in the target calibration data */
    copy_from_f32(data + (CAL_TSL2585_TARGET_LUX_SLOPE - offset), cal_target->lux_slope);
    copy_from_f32(data + (CAL_TSL2585_TARGET_LUX_INTERCEPT - offset), cal_target->lux_intercept);

    /* Calculate the target calibration data CRC */
    crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)data,
        (CAL_TSL2585_TARGET_CRC - CAL_TSL2585_TARGET_LUX_SLOPE) / 4UL);
    copy_from_u32(data + (CAL_TSL2585_TARGET_CRC - offset), crc);

    /* Write the data buffer */
    ret = m24c08_write_buffer(handle->hi2c, offset, data, sizeof(data));
    if (ret != HAL_OK) { return ret; }

    return ret;
}

const char *meter_probe_type_str(meter_probe_type_t type)
{
    switch (type) {
    case METER_PROBE_TYPE_UNKNOWN:
        return "Unknown";
    case METER_PROBE_TYPE_BASELINE:
        return "Baseline";
    default:
        return "?";
    }
}
