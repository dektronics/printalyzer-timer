#ifndef PERIPHERAL_SETTINGS_H
#define PERIPHERAL_SETTINGS_H

#include "tsl2585.h"

typedef struct i2c_handle_t i2c_handle_t;

typedef struct {
    uint8_t rev_major;
    uint8_t rev_minor;
    char serial[32];
} peripheral_id_t;

typedef struct {
    i2c_handle_t *hi2c;
    bool initialized;
    peripheral_id_t id;
    uint8_t type;
} peripheral_settings_handle_t;

typedef struct {
    float values[TSL2585_GAIN_256X + 1];
} peripheral_cal_gain_t;

typedef struct {
    float lux_slope;
    float lux_intercept;
} peripheral_cal_linear_target_t;

typedef struct {
    float lo_density;
    float lo_reading;
    float hi_density;
    float hi_reading;
} peripheral_cal_density_target_t;

#endif /* PERIPHERAL_SETTINGS_H */
