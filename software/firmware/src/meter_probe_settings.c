#include "meter_probe_settings.h"

#include <string.h>
#include <math.h>

#define LOG_TAG "meter_probe_settings"
#include <elog.h>

#include "settings_util.h"
#include "m24c02.h"
#include "tsl2585.h"

extern CRC_HandleTypeDef hcrc;

#define LATEST_CAL_TSL2585_VERSION 1

#define ID_MEMORY_ID             0 /* 3B */
#define ID_PROBE_TYPE            3 /* 1B (uint8_t) */
#define ID_PROBE_REVISION        4 /* 1B (uint8_t) */
#define ID_PROBE_RESERVED0       5 /* 7B (for page alignment) */
#define ID_PROBE_SERIAL         12 /* 4B (uint32_t) */

#define PAGE_CAL               0x000UL
#define PAGE_CAL_SIZE          (128UL)

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
#define CAL_TSL2585_SLOPE_B0    64 /* 4B (float) */
#define CAL_TSL2585_SLOPE_B1    68 /* 4B (float) */
#define CAL_TSL2585_SLOPE_B2    72 /* 4B (float) */
#define CAL_TSL2558_SLOPE_CRC   76 /* 4B (uint32_t) */

/* TSL2585 Target Calibration (16B) */
#define CAL_TSL2585_TARGET_LUX_SLOPE     80 /* 4B (float) */
#define CAL_TSL2585_TARGET_LUX_INTERCEPT 84 /* 4B (float) */
#define CAL_TSL2585_RESERVED2            88 /* 4B (for page alignment) */
#define CAL_TSL2585_TARGET_CRC           92 /* 4B (uint32_t) */

#define CAL_TSL2585_END        128

HAL_StatusTypeDef meter_probe_settings_init(meter_probe_settings_handle_t *handle, I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef ret;
    uint8_t id_data[M24C02_PAGE_SIZE];

    if (!handle || handle->initialized || !hi2c) { return HAL_ERROR; }

    do {
        /* Read the identification area of the meter probe memory */
        ret = m24c02_read_id_page(hi2c, id_data);
        if (ret != HAL_OK) { break; }

        log_d("Memory ID: 0x%02X%02X%02X", id_data[0], id_data[1], id_data[2]);

        /* Verify the memory device identification code */
        if (id_data[0] != 0x20 && id_data[1] != 0xE0 && id_data[2] != 0x08) {
            log_w("Unexpected memory type");
            ret = HAL_ERROR;
            break;
        }

        handle->hi2c = hi2c;

        /* Read the probe type information out of the rest of the ID page */
        memcpy(handle->id.memory_id, id_data, 3);
        handle->id.probe_type = id_data[ID_PROBE_TYPE];
        handle->id.probe_revision = id_data[ID_PROBE_REVISION];
        handle->id.probe_serial = copy_to_u32(id_data + ID_PROBE_SERIAL);

        handle->initialized = true;
    } while (0);

    return ret;
}

HAL_StatusTypeDef meter_probe_settings_clear(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef ret = HAL_OK;

    if (!hi2c) { return HAL_ERROR; }

    uint8_t data[M24C02_PAGE_SIZE];
    memset(data, 0xFF, sizeof(data));

    log_i("meter_probe_settings_clear");

    for (size_t addr = 0; addr < M24C02_PAGE_LIMIT; addr += M24C02_PAGE_SIZE) {
        ret = m24c02_write_page(hi2c, addr, data, sizeof(data));
        if (ret != HAL_OK) { break; }
    }

    return ret;
}

HAL_StatusTypeDef meter_probe_settings_get_tsl2585(const meter_probe_settings_handle_t *handle,
    meter_probe_settings_tsl2585_t *settings_tsl2585)
{
    HAL_StatusTypeDef ret = HAL_OK;
    if (!handle || !settings_tsl2585) { return HAL_ERROR; }
    if (!handle->initialized) { return HAL_ERROR; }
    if (handle->id.probe_type != METER_PROBE_TYPE_TSL2585
        && handle->id.probe_type != METER_PROBE_TYPE_TSL2521) {
        return HAL_ERROR;
    }

    uint8_t data[PAGE_CAL_SIZE];
    uint32_t version;
    uint32_t crc;
    uint32_t calculated_crc;

    /* Read the whole data buffer */
    ret = m24c02_read_buffer(handle->hi2c, PAGE_CAL, data, PAGE_CAL_SIZE);
    if (ret != HAL_OK) { return ret; }

    /* Get the version */
    version = copy_to_u32(data + CAL_TSL2585_VERSION);

    if (version != LATEST_CAL_TSL2585_VERSION) {
        log_w("Unexpected TSL2585 cal version: %d != %d", version, LATEST_CAL_TSL2585_VERSION);
        return HAL_ERROR;
    }

    memset(settings_tsl2585, 0, sizeof(meter_probe_settings_tsl2585_t));

    /* Validate the gain data CRC */
    crc = copy_to_u32(data + CAL_TSL2585_GAIN_CRC);
    calculated_crc = HAL_CRC_Calculate(&hcrc,
        (uint32_t *)(data + CAL_TSL2585_GAIN_0_5X),
        (CAL_TSL2585_GAIN_CRC - CAL_TSL2585_GAIN_0_5X) / 4UL);
    if (crc == calculated_crc) {
        /* Parse the gain calibration data */
        meter_probe_settings_tsl2585_cal_gain_t *cal_gain = &settings_tsl2585->cal_gain;
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
            settings_tsl2585->cal_gain.values[i] = NAN;
        }
    }

    /* Validate the slope data CRC */
    crc = copy_to_u32(data + CAL_TSL2558_SLOPE_CRC);
    calculated_crc = HAL_CRC_Calculate(&hcrc,
        (uint32_t *)(data + CAL_TSL2585_SLOPE_B0),
        (CAL_TSL2558_SLOPE_CRC - CAL_TSL2585_SLOPE_B0) / 4UL);
    if (crc == calculated_crc) {
        /* Parse the gain calibration data */
        meter_probe_settings_tsl2585_cal_slope_t *cal_slope = &settings_tsl2585->cal_slope;
        cal_slope->b0 = copy_to_f32(data + CAL_TSL2585_SLOPE_B0);
        cal_slope->b1 = copy_to_f32(data + CAL_TSL2585_SLOPE_B1);
        cal_slope->b2 = copy_to_f32(data + CAL_TSL2585_SLOPE_B2);
    } else {
        log_w("Invalid TSL2585 cal slope CRC: %08X != %08X", crc, calculated_crc);
        meter_probe_settings_tsl2585_cal_slope_t *cal_slope = &settings_tsl2585->cal_slope;
        cal_slope->b0 = NAN;
        cal_slope->b1 = NAN;
        cal_slope->b2 = NAN;
    }

    /* Validate the target data CRC */
    crc = copy_to_u32(data + CAL_TSL2585_TARGET_CRC);
    calculated_crc = HAL_CRC_Calculate(&hcrc,
        (uint32_t *)(data + CAL_TSL2585_TARGET_LUX_SLOPE),
        (CAL_TSL2585_TARGET_CRC - CAL_TSL2585_TARGET_LUX_SLOPE) / 4UL);
    if (crc == calculated_crc) {
        /* Parse the target calibration data */
        meter_probe_settings_tsl2585_cal_target_t *cal_target = &settings_tsl2585->cal_target;
        cal_target->lux_slope = copy_to_f32(data + CAL_TSL2585_TARGET_LUX_SLOPE);
        cal_target->lux_intercept = copy_to_f32(data + CAL_TSL2585_TARGET_LUX_INTERCEPT);
    } else {
        log_w("Invalid TSL2585 cal target CRC: %08X != %08X", crc, calculated_crc);
        meter_probe_settings_tsl2585_cal_target_t *cal_target = &settings_tsl2585->cal_target;
        cal_target->lux_slope = NAN;
        cal_target->lux_intercept = NAN;
    }

    return ret;
}

HAL_StatusTypeDef meter_probe_settings_set_tsl2585(const meter_probe_settings_handle_t *handle,
    const meter_probe_settings_tsl2585_t *settings_tsl2585)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint32_t crc;

    if (!handle || !settings_tsl2585) { return HAL_ERROR; }
    if (!handle->initialized || handle->id.probe_type != METER_PROBE_TYPE_TSL2585) { return HAL_ERROR; }

    /* Prepare an empty buffer */
    uint8_t data[PAGE_CAL_SIZE];
    memset(data, 0, sizeof(data));

    /* Set the version */
    copy_from_u32(data + CAL_TSL2585_VERSION, LATEST_CAL_TSL2585_VERSION);

    /* Fill in the gain calibration data */
    const meter_probe_settings_tsl2585_cal_gain_t *cal_gain = &settings_tsl2585->cal_gain;
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

    /* Fill in the slope calibration data */
    const meter_probe_settings_tsl2585_cal_slope_t *cal_slope = &settings_tsl2585->cal_slope;
    copy_from_f32(data + CAL_TSL2585_SLOPE_B0, cal_slope->b0);
    copy_from_f32(data + CAL_TSL2585_SLOPE_B1, cal_slope->b1);
    copy_from_f32(data + CAL_TSL2585_SLOPE_B2, cal_slope->b2);

    crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)(data + CAL_TSL2585_SLOPE_B0),
        (CAL_TSL2558_SLOPE_CRC - CAL_TSL2585_SLOPE_B0) / 4UL);
    copy_from_u32(data + CAL_TSL2558_SLOPE_CRC, crc);

    /* Fill in the target calibration data */
    const meter_probe_settings_tsl2585_cal_target_t *cal_target = &settings_tsl2585->cal_target;
    copy_from_f32(data + CAL_TSL2585_TARGET_LUX_SLOPE, cal_target->lux_slope);
    copy_from_f32(data + CAL_TSL2585_TARGET_LUX_INTERCEPT, cal_target->lux_intercept);

    crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)(data + CAL_TSL2585_TARGET_LUX_SLOPE),
        (CAL_TSL2585_TARGET_CRC - CAL_TSL2585_TARGET_LUX_SLOPE) / 4UL);
    copy_from_u32(data + CAL_TSL2585_TARGET_CRC, crc);

    /* Write the whole data buffer */
    ret = m24c02_write_buffer(handle->hi2c, PAGE_CAL, data, PAGE_CAL_SIZE);
    if (ret != HAL_OK) { return ret; }

    return ret;
}

HAL_StatusTypeDef meter_probe_settings_set_tsl2585_target(const meter_probe_settings_handle_t *handle,
    const meter_probe_settings_tsl2585_cal_target_t *cal_target)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t data[(CAL_TSL2585_TARGET_CRC - CAL_TSL2585_TARGET_LUX_SLOPE) + 4];
    const size_t offset = CAL_TSL2585_TARGET_LUX_SLOPE;
    uint32_t version;
    uint32_t crc;

    if (!handle || !cal_target) { return HAL_ERROR; }
    if (!handle->initialized || handle->id.probe_type != METER_PROBE_TYPE_TSL2585) { return HAL_ERROR; }

    /* Read just the version */
    ret = m24c02_read_buffer(handle->hi2c, CAL_TSL2585_VERSION, data, 4);
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
    ret = m24c02_write_buffer(handle->hi2c, offset, data, sizeof(data));
    if (ret != HAL_OK) { return ret; }

    return ret;
}

const char *meter_probe_type_str(meter_probe_type_t type)
{
    switch (type) {
    case METER_PROBE_TYPE_UNKNOWN:
        return "UNKNOWN";
    case METER_PROBE_TYPE_TSL2585:
        return "TSL2585";
    case METER_PROBE_TYPE_TSL2521:
        return "TSL2521";
    default:
        return "?";
    }
}
