/*
 * TSL2585
 * Miniature Ambient Light Sensor
 * with UV and Light Flicker Detection
 */

#ifndef TSL2585_H
#define TSL2585_H

#include "stm32f4xx_hal.h"

typedef enum {
    TSL2585_MOD0 = 0x01,
    TSL2585_MOD1 = 0x02,
    TSL2585_MOD2 = 0x04
} tsl2585_modulator_t;

/* ENABLE register values */
#define TSL2585_ENABLE_FDEN 0x40 /*!< Flicker detection enable */
#define TSL2585_ENABLE_AEN  0x02 /*!< ALS Enable */
#define TSL2585_ENABLE_PON  0x01 /*!< Power on */

/* STATUS register values */
#define TSL2585_MEAS_SEQR_STEP 0xC0 /*< Mask for bits that contains the sequencer step where ALS data was measured */
#define TSL2585_ALS_DATA0_ANALOG_SATURATION_STATUS 0x20 /*< Indicates analog saturation of ALS data0 in data registers ALS_ADATA0 */
#define TSL2585_ALS_DATA1_ANALOG_SATURATION_STATUS 0x10 /*< Indicates analog saturation of ALS data1 in data registers ALS_ADATA1 */
#define TSL2585_ALS_DATA2_ANALOG_SATURATION_STATUS 0x08 /*< Indicates analog saturation of ALS data2 in data registers ALS_ADATA2 */
#define TSL2585_ALS_DATA0_SCALED_STATUS 0x04 /*< Indicates if ALS data0 needs to be multiplied */
#define TSL2585_ALS_DATA1_SCALED_STATUS 0x02 /*< Indicates if ALS data1 needs to be multiplied */
#define TSL2585_ALS_DATA2_SCALED_STATUS 0x01 /*< Indicates if ALS data2 needs to be multiplied */

HAL_StatusTypeDef tsl2585_init(I2C_HandleTypeDef *hi2c);

HAL_StatusTypeDef tsl2585_set_enable(I2C_HandleTypeDef *hi2c, uint8_t value);
HAL_StatusTypeDef tsl2585_enable(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef tsl2585_disable(I2C_HandleTypeDef *hi2c);

HAL_StatusTypeDef tsl2585_enable_modulators(I2C_HandleTypeDef *hi2c, tsl2585_modulator_t mods);

HAL_StatusTypeDef tsl2585_get_als_status(I2C_HandleTypeDef *hi2c, uint8_t *status);
HAL_StatusTypeDef tsl2585_get_als_scale(I2C_HandleTypeDef *hi2c, uint8_t *scale);
HAL_StatusTypeDef tsl2585_get_als_msb_position(I2C_HandleTypeDef *hi2c, uint8_t *position);

HAL_StatusTypeDef tsl2585_get_als_data0(I2C_HandleTypeDef *hi2c, uint16_t *data);
HAL_StatusTypeDef tsl2585_get_als_data1(I2C_HandleTypeDef *hi2c, uint16_t *data);
HAL_StatusTypeDef tsl2585_get_als_data2(I2C_HandleTypeDef *hi2c, uint16_t *data);



#endif /* TSL2585_H */
