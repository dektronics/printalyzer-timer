/*
 * TSL2585
 * Miniature Ambient Light Sensor
 * with UV and Light Flicker Detection
 */

#ifndef TSL2585_H
#define TSL2585_H

#include "stm32f4xx_hal.h"

HAL_StatusTypeDef tsl2585_init(I2C_HandleTypeDef *hi2c);

#endif /* TSL2585_H */
