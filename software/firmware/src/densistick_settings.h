#ifndef DENSISTICK_SETTINGS_H
#define DENSISTICK_SETTINGS_H

#include <stm32f4xx_hal.h>

#include "peripheral_settings.h"
#include "tsl2585.h"

typedef struct i2c_handle_t i2c_handle_t;

typedef enum {
    DENSISTICK_TYPE_UNKNOWN = 0,
    DENSISTICK_TYPE_BASELINE = 1
} densistick_type_t;

typedef struct {
    densistick_type_t type;
    peripheral_cal_gain_t cal_gain;
    peripheral_cal_density_target_t cal_target;
} densistick_settings_t;

/**
 * Initialize the settings interface for the connected DensiStick.
 *
 * @param hi2c Handle for the I2C peripheral used by the DensiStick
 */
HAL_StatusTypeDef densistick_settings_init(peripheral_settings_handle_t *handle, i2c_handle_t *hi2c);

/**
 * Clear the DensiStick settings to factory blank.
 *
 * This function does not require the settings interface to be initialized,
 * and should only be called out of band from normal operation.
 *
 * @param hi2c Handle for the I2C peripheral used by the DensiStick
 */
HAL_StatusTypeDef densistick_settings_clear(i2c_handle_t *hi2c);

HAL_StatusTypeDef densistick_settings_get(const peripheral_settings_handle_t *handle,
    densistick_settings_t *settings);

HAL_StatusTypeDef densistick_settings_set(const peripheral_settings_handle_t *handle,
    const densistick_settings_t *settings);

HAL_StatusTypeDef densistick_settings_set_target(const peripheral_settings_handle_t *handle,
    const peripheral_cal_density_target_t *cal_target);

const char *densistick_type_str(densistick_type_t type);

#endif /* DENSISTICK_SETTINGS_H */
