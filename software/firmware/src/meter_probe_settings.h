#ifndef METER_PROBE_SETTINGS_H
#define METER_PROBE_SETTINGS_H

#include <stm32f4xx_hal.h>
#include <stdbool.h>

#include "tsl2585.h"

typedef enum {
    METER_PROBE_TYPE_UNKNOWN = 0,
    METER_PROBE_TYPE_TSL2585
} meter_probe_type_t;

typedef struct __meter_probe_settings_handle_t {
    I2C_HandleTypeDef *hi2c;
    bool initialized;
    meter_probe_type_t type;
    int probe_revision;
    int probe_serial;
    uint8_t memory_id[3];
} meter_probe_settings_handle_t;

typedef struct {
    float slope;
    float offset;
} meter_probe_settings_tsl2585_gain_cal_t;

typedef struct __meter_probe_settings_tsl2585_t {
    meter_probe_settings_tsl2585_gain_cal_t gain_cal[TSL2585_GAIN_MAX];
} meter_probe_settings_tsl2585_t;

/**
 * Initialize the settings interface for the connected meter probe.
 *
 * @param hi2c Handle for the I2C peripheral used by the meter probe
 */
HAL_StatusTypeDef meter_probe_settings_init(meter_probe_settings_handle_t *handle, I2C_HandleTypeDef *hi2c);

/**
 * Clear the meter probe settings to factory blank.
 *
 * This function does not require the settings interface to be initialized,
 * and should only be called out of band from normal operation.
 *
 * @param hi2c Handle for the I2C peripheral used by the meter probe
 */
HAL_StatusTypeDef meter_probe_settings_clear(I2C_HandleTypeDef *hi2c);

HAL_StatusTypeDef meter_probe_settings_get_tsl2585(const meter_probe_settings_handle_t *handle,
    meter_probe_settings_tsl2585_t *settings_tsl2585);

HAL_StatusTypeDef meter_probe_settings_set_tsl2585(const meter_probe_settings_handle_t *handle,
    const meter_probe_settings_tsl2585_t *settings_tsl2585);

#endif /* METER_PROBE_SETTINGS_H */
