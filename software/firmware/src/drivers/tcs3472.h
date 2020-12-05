/*
 * TCS3472 Color Light-to-Digital Converter with IR Filter
 */

#ifndef TCS3472_H
#define TCS3472_H

#include <stdbool.h>

#include "stm32f4xx_hal.h"

typedef enum {
    TCS3472_ATIME_2_4MS = 0xFF,
    TCS3472_ATIME_24MS = 0xF6,
    TCS3472_ATIME_50MS = 0xEB,
    TCS3472_ATIME_101MS = 0xD5,
    TCS3472_ATIME_154MS = 0xC0,
    TCS3472_ATIME_614MS = 0x00
} tcs3472_atime_t;

typedef enum {
    TCS3472_WTIME_2_4MS = 0xFF,
    TCS3472_WTIME_204MS = 0xAB,
    TCS3472_WTIME_614MS = 0x00
} tcs3472_wtime_t;

typedef enum {
    TCS3472_AGAIN_1X = 0,
    TCS3472_AGAIN_4X = 1,
    TCS3472_AGAIN_16X = 2,
    TCS3472_AGAIN_60X = 3
} tcs3472_again_t;

typedef struct {
    uint16_t clear;
    uint16_t red;
    uint16_t green;
    uint16_t blue;
    tcs3472_atime_t integration;
    tcs3472_again_t gain;
} tcs3472_channel_data_t;

HAL_StatusTypeDef tcs3472_init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef tcs3472_enable(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef tcs3472_disable(I2C_HandleTypeDef *hi2c);

HAL_StatusTypeDef tcs3472_set_time(I2C_HandleTypeDef *hi2c, tcs3472_atime_t atime);
HAL_StatusTypeDef tcs3472_get_time(I2C_HandleTypeDef *hi2c, tcs3472_atime_t *atime);
HAL_StatusTypeDef tcs3472_set_gain(I2C_HandleTypeDef *hi2c, tcs3472_again_t gain);
HAL_StatusTypeDef tcs3472_get_gain(I2C_HandleTypeDef *hi2c, tcs3472_again_t *gain);

HAL_StatusTypeDef tcs3472_get_status_valid(I2C_HandleTypeDef *hi2c, bool *valid);
HAL_StatusTypeDef tcs3472_get_full_channel_data(I2C_HandleTypeDef *hi2c, tcs3472_channel_data_t *ch_data);

const char* tcs3472_atime_str(tcs3472_atime_t atime);
const char* tcs3472_gain_str(tcs3472_again_t gain);

uint16_t tcs3472_calculate_color_temp(const tcs3472_channel_data_t *ch_data);
float tcs3472_calculate_lux(const tcs3472_channel_data_t *ch_data);

#endif /* TCS3472_H */
