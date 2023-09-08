#include "meter_probe_settings.h"

#include <string.h>

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
#define CAL_TSL2585_VERSION      0 /* 4B (uint32_t) */
#define CAL_TSL2585_CRC          4 /* 4B (uint32_t) */
#define CAL_TSL2585_RESERVED0    8 /* 8B (for page alignment) */
#define CAL_TSL2585_GAIN_0_5X   16 /* 8B (float + float) */
#define CAL_TSL2585_GAIN_1X     24 /* 8B (float + float) */
#define CAL_TSL2585_GAIN_2X     32 /* 8B (float + float) */
#define CAL_TSL2585_GAIN_4X     40 /* 8B (float + float) */
#define CAL_TSL2585_GAIN_8X     48 /* 8B (float + float) */
#define CAL_TSL2585_GAIN_16X    56 /* 8B (float + float) */
#define CAL_TSL2585_GAIN_32X    64 /* 8B (float + float) */
#define CAL_TSL2585_GAIN_64X    72 /* 8B (float + float) */
#define CAL_TSL2585_GAIN_128X   80 /* 8B (float + float) */
#define CAL_TSL2585_GAIN_256X   88 /* 8B (float + float) */
#define CAL_TSL2585_GAIN_512X   96 /* 8B (float + float) */
#define CAL_TSL2585_GAIN_1024X 104 /* 8B (float + float) */
#define CAL_TSL2585_GAIN_2048X 112 /* 8B (float + float) */
#define CAL_TSL2585_GAIN_4096X 120 /* 8B (float + float) */
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
    if (!handle->initialized || handle->id.probe_type != METER_PROBE_TYPE_TSL2585) { return HAL_ERROR; }

    uint8_t data[PAGE_CAL_SIZE];
    uint32_t version;
    uint32_t crc;
    uint32_t calculated_crc;

    /* Read the whole data buffer */
    ret = m24c02_read_buffer(handle->hi2c, PAGE_CAL, data, PAGE_CAL_SIZE);
    if (ret != HAL_OK) { return ret; }

    /* Get the version */
    version = copy_to_u32(data + CAL_TSL2585_VERSION);

    /* Separate the CRC value and zero it in the buffer */
    crc = copy_to_u32(data + CAL_TSL2585_CRC);
    memset(data + CAL_TSL2585_CRC, 0, sizeof(uint32_t));

    /* Calculate the CRC for the page */
    calculated_crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)data, PAGE_CAL_SIZE / 4UL);
    if (crc != calculated_crc) {
        log_w("Invalid TSL2585 cal CRC: %08X != %08X", crc, calculated_crc);
        return HAL_ERROR;
    }

    if (version != LATEST_CAL_TSL2585_VERSION) {
        log_w("Unexpected TSL2585 cal version: %d != %d", version, LATEST_CAL_TSL2585_VERSION);
        return HAL_ERROR;
    }

    //TODO Report version/CRC errors differently from normal I2C errors

    /* Fill in the calibration data */
    size_t offset = CAL_TSL2585_GAIN_0_5X;
    for (size_t i = 0; i < TSL2585_GAIN_MAX; i++) {
        if (offset > CAL_TSL2585_GAIN_4096X) {
            /* This should not be possible */
            log_w("Cal data overrun");
            return HAL_ERROR;
        }

        settings_tsl2585->gain_cal[i].slope = copy_to_f32(data + offset);
        offset += 4;
        settings_tsl2585->gain_cal[i].offset = copy_to_f32(data + offset);
        offset += 4;
    }

    return ret;
}

HAL_StatusTypeDef meter_probe_settings_set_tsl2585(const meter_probe_settings_handle_t *handle,
    const meter_probe_settings_tsl2585_t *settings_tsl2585)
{
    HAL_StatusTypeDef ret = HAL_OK;
    if (!handle || !settings_tsl2585) { return HAL_ERROR; }
    if (!handle->initialized || handle->id.probe_type != METER_PROBE_TYPE_TSL2585) { return HAL_ERROR; }

    /* Prepare an empty buffer */
    uint8_t data[PAGE_CAL_SIZE];
    memset(data, 0, sizeof(data));

    /* Set the version */
    copy_from_u32(data + CAL_TSL2585_VERSION, LATEST_CAL_TSL2585_VERSION);

    /* Fill in the calibration data */
    size_t offset = CAL_TSL2585_GAIN_0_5X;
    for (size_t i = 0; i < TSL2585_GAIN_MAX; i++) {
        if (offset > CAL_TSL2585_GAIN_4096X) {
            /* This should not be possible */
            log_w("Cal data overrun");
            return HAL_ERROR;
        }
        copy_from_f32(data + offset, settings_tsl2585->gain_cal[i].slope);
        offset += 4;
        copy_from_f32(data + offset, settings_tsl2585->gain_cal[i].offset);
        offset += 4;
    }

    /* Calculate the CRC, with its field zeroed, and populate the value */
    uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)data, PAGE_CAL_SIZE / 4UL);
    copy_from_u32(data + CAL_TSL2585_CRC, crc);

    /* Write the non-header portions of the data buffer first */
    ret = m24c02_write_buffer(handle->hi2c, PAGE_CAL + CAL_TSL2585_GAIN_0_5X,
        data + M24C02_PAGE_SIZE, PAGE_CAL_SIZE - M24C02_PAGE_SIZE);
    if (ret != HAL_OK) { return ret; }

    /* Write the header last, so a failed write will ensure an invalid CRC */
    ret = m24c02_write_page(handle->hi2c, PAGE_CAL, data, M24C02_PAGE_SIZE);

    return ret;
}
