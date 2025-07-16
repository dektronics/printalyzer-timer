#ifndef METER_PROBE_SETTINGS_H
#define METER_PROBE_SETTINGS_H

#include <stm32f4xx_hal.h>

#include "peripheral_settings.h"
#include "tsl2585.h"

typedef struct i2c_handle_t i2c_handle_t;

typedef enum {
    METER_PROBE_TYPE_UNKNOWN = 0,
    METER_PROBE_TYPE_BASELINE = 1
} meter_probe_type_t;

typedef struct {
    meter_probe_type_t type;
    peripheral_cal_gain_t cal_gain;
    peripheral_cal_linear_target_t cal_target;
} meter_probe_settings_t;

/**
 * Initialize the settings interface for the connected meter probe.
 *
 * @param hi2c Handle for the I2C peripheral used by the meter probe
 */
HAL_StatusTypeDef meter_probe_settings_init(peripheral_settings_handle_t *handle, i2c_handle_t *hi2c);

/**
 * Clear the meter probe settings to factory blank.
 *
 * This function does not require the settings interface to be initialized,
 * and should only be called out of band from normal operation.
 *
 * @param hi2c Handle for the I2C peripheral used by the meter probe
 */
HAL_StatusTypeDef meter_probe_settings_clear(i2c_handle_t *hi2c);

HAL_StatusTypeDef meter_probe_settings_get(const peripheral_settings_handle_t *handle,
    meter_probe_settings_t *settings);

HAL_StatusTypeDef meter_probe_settings_set(const peripheral_settings_handle_t *handle,
    const meter_probe_settings_t *settings);

HAL_StatusTypeDef meter_probe_settings_set_target(const peripheral_settings_handle_t *handle,
    const peripheral_cal_linear_target_t *cal_target);

const char *meter_probe_type_str(meter_probe_type_t type);

#endif /* METER_PROBE_SETTINGS_H */
