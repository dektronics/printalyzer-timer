#ifndef DENSISTICK_SETTINGS_H
#define DENSISTICK_SETTINGS_H

#include <stm32f4xx_hal.h>
#include <stdbool.h>

#include "tsl2585.h"
#include "meter_probe_settings.h"

typedef struct i2c_handle_t i2c_handle_t;

typedef struct __densistick_settings_tsl2585_cal_target_t {
    float lo_density;
    float lo_reading;
    float hi_density;
    float hi_reading;
} densistick_settings_tsl2585_cal_target_t;

typedef struct __densistick_settings_tsl2585_t {
    meter_probe_settings_tsl2585_cal_gain_t cal_gain;
    meter_probe_settings_tsl2585_cal_slope_t cal_slope;
    densistick_settings_tsl2585_cal_target_t cal_target;
} densistick_settings_tsl2585_t;

/**
 * Initialize the settings interface for the connected DensiStick.
 *
 * @param hi2c Handle for the I2C peripheral used by the DensiStick
 */
HAL_StatusTypeDef densistick_settings_init(meter_probe_settings_handle_t *handle, i2c_handle_t *hi2c);

/**
 * Clear the DensiStick settings to factory blank.
 *
 * This function does not require the settings interface to be initialized,
 * and should only be called out of band from normal operation.
 *
 * @param hi2c Handle for the I2C peripheral used by the DensiStick
 */
HAL_StatusTypeDef densistick_settings_clear(i2c_handle_t *hi2c);

HAL_StatusTypeDef densistick_settings_get_tsl2585(const meter_probe_settings_handle_t *handle,
    densistick_settings_tsl2585_t *settings_tsl2585);

HAL_StatusTypeDef densistick_settings_set_tsl2585(const meter_probe_settings_handle_t *handle,
    const densistick_settings_tsl2585_t *settings_tsl2585);

HAL_StatusTypeDef densistick_settings_set_tsl2585_target(const meter_probe_settings_handle_t *handle,
    const densistick_settings_tsl2585_cal_target_t *cal_target);

#endif /* DENSISTICK_SETTINGS_H */
