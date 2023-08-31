/*
 * TSL2585
 * Miniature Ambient Light Sensor
 * with UV and Light Flicker Detection
 */

#ifndef TSL2585_H
#define TSL2585_H

#include "stm32f4xx_hal.h"
#include <stdbool.h>

#define TSL2585_SAMPLE_TIME_BASE 1.388889F /*!< Sample time base in microseconds */

typedef enum {
    TSL2585_MOD0 = 0x01,
    TSL2585_MOD1 = 0x02,
    TSL2585_MOD2 = 0x04
} tsl2585_modulator_t;

typedef enum {
    TSL2585_STEP0 = 0,
    TSL2585_STEP1,
    TSL2585_STEP2
} tsl2585_step_t;

typedef enum {
    TSL2585_GAIN_0_5X  = 0,
    TSL2585_GAIN_1X    = 1,
    TSL2585_GAIN_2X    = 2,
    TSL2585_GAIN_4X    = 3,
    TSL2585_GAIN_8X    = 4,
    TSL2585_GAIN_16X   = 5,
    TSL2585_GAIN_32X   = 6,
    TSL2585_GAIN_64X   = 7,
    TSL2585_GAIN_128X  = 8,
    TSL2585_GAIN_256X  = 9,
    TSL2585_GAIN_512X  = 10,
    TSL2585_GAIN_1024X = 11,
    TSL2585_GAIN_2048X = 12,
    TSL2585_GAIN_4096X = 13,
    TSL2585_GAIN_MAX   = 14
} tsl2585_gain_t;

/* ENABLE register values */
#define TSL2585_ENABLE_FDEN 0x40 /*!< Flicker detection enable */
#define TSL2585_ENABLE_AEN  0x02 /*!< ALS Enable */
#define TSL2585_ENABLE_PON  0x01 /*!< Power on */

/* INTENAB register values */
#define TSL2585_INTENAB_MIEN 0x80 /*!< Modulator Interrupt Enable*/
#define TSL2585_INTENAB_AIEN 0x08 /*!< ALS Interrupt Enable */
#define TSL2585_INTENAB_FIEN 0x04 /*!< FIFO Interrupt Enable */
#define TSL2585_INTENAB_SIEN 0x01 /*!< System Interrupt Enable */

/* STATUS register values */
#define TSL2585_STATUS_MINT 0x80 /*!< Modulator Interrupt (STATUS2 for details) */
#define TSL2585_STATUS_AINT 0x08 /*!< ALS Interrupt (STATUS3 for details) */
#define TSL2585_STATUS_FINT 0x04 /*!< FIFO Interrupt */
#define TSL2585_STATUS_SINT 0x01 /*!< System Interrupt (STATUS5 for details) */

/* STATUS2 register values */
#define TSL2585_STATUS2_ALS_DATA_VALID         0x40 /*!< ALS Data Valid */
#define TSL2585_STATUS2_ALS_DIGITAL_SATURATION 0x10 /*!< ALS Digital Saturation */
#define TSL2585_STATUS2_FD_DIGITAL_SATURATION  0x08 /*!< Flicker Detect Digital Saturation */
#define TSL2585_STATUS2_MOD_ANALOG_SATURATION2 0x04 /*!< ALS Analog Saturation of modulator 2 */
#define TSL2585_STATUS2_MOD_ANALOG_SATURATION1 0x02 /*!< ALS Analog Saturation of modulator 1 */
#define TSL2585_STATUS2_MOD_ANALOG_SATURATION0 0x01 /*!< ALS Analog Saturation of modulator 0 */

/* STATUS4 register values */
#define TSL2585_STATUS4_MOD_SAMPLE_TRIGGER_ERROR 0x08 /*!< Measured data is corrupted */
#define TSL2585_STATUS4_MOD_TRIGGER_ERROR        0x04 /*!< WTIME is too short for the programmed configuration */
#define TSL2585_STATUS4_SAI_ACTIVE               0x02 /*!< Sleep After Interrupt Active */
#define TSL2585_STATUS4_INIT_BUSY                0x01 /*!< Initialization Busy */

/* ALS_STATUS register values */
#define TSL2585_MEAS_SEQR_STEP 0xC0 /*< Mask for bits that contains the sequencer step where ALS data was measured */
#define TSL2585_ALS_DATA0_ANALOG_SATURATION_STATUS 0x20 /*< Indicates analog saturation of ALS data0 in data registers ALS_ADATA0 */
#define TSL2585_ALS_DATA1_ANALOG_SATURATION_STATUS 0x10 /*< Indicates analog saturation of ALS data1 in data registers ALS_ADATA1 */
#define TSL2585_ALS_DATA2_ANALOG_SATURATION_STATUS 0x08 /*< Indicates analog saturation of ALS data2 in data registers ALS_ADATA2 */
#define TSL2585_ALS_DATA0_SCALED_STATUS 0x04 /*< Indicates if ALS data0 needs to be multiplied */
#define TSL2585_ALS_DATA1_SCALED_STATUS 0x02 /*< Indicates if ALS data1 needs to be multiplied */
#define TSL2585_ALS_DATA2_SCALED_STATUS 0x01 /*< Indicates if ALS data2 needs to be multiplied */

HAL_StatusTypeDef tsl2585_init(I2C_HandleTypeDef *hi2c, uint8_t *sensor_id);

HAL_StatusTypeDef tsl2585_set_enable(I2C_HandleTypeDef *hi2c, uint8_t value);
HAL_StatusTypeDef tsl2585_enable(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef tsl2585_disable(I2C_HandleTypeDef *hi2c);

HAL_StatusTypeDef tsl2585_set_interrupt_enable(I2C_HandleTypeDef *hi2c, uint8_t value);

HAL_StatusTypeDef tsl2585_set_sleep_after_interrupt(I2C_HandleTypeDef *hi2c, bool enabled);

HAL_StatusTypeDef tsl2585_clear_sleep_after_interrupt(I2C_HandleTypeDef *hi2c);

HAL_StatusTypeDef tsl2585_enable_modulators(I2C_HandleTypeDef *hi2c, tsl2585_modulator_t mods);

HAL_StatusTypeDef tsl2585_get_status(I2C_HandleTypeDef *hi2c, uint8_t *status);
HAL_StatusTypeDef tsl2585_set_status(I2C_HandleTypeDef *hi2c, uint8_t status);

HAL_StatusTypeDef tsl2585_get_status2(I2C_HandleTypeDef *hi2c, uint8_t *status);
HAL_StatusTypeDef tsl2585_get_status3(I2C_HandleTypeDef *hi2c, uint8_t *status);
HAL_StatusTypeDef tsl2585_get_status4(I2C_HandleTypeDef *hi2c, uint8_t *status);
HAL_StatusTypeDef tsl2585_get_status5(I2C_HandleTypeDef *hi2c, uint8_t *status);

HAL_StatusTypeDef tsl2585_get_max_mod_gain(I2C_HandleTypeDef *hi2c, tsl2585_gain_t *gain);
HAL_StatusTypeDef tsl2585_set_max_mod_gain(I2C_HandleTypeDef *hi2c, tsl2585_gain_t gain);

HAL_StatusTypeDef tsl2585_get_mod_gain(I2C_HandleTypeDef *hi2c, tsl2585_modulator_t mod, tsl2585_step_t step, tsl2585_gain_t *gain);
HAL_StatusTypeDef tsl2585_set_mod_gain(I2C_HandleTypeDef *hi2c, tsl2585_modulator_t mod, tsl2585_step_t step, tsl2585_gain_t gain);

HAL_StatusTypeDef tsl2585_get_calibration_nth_iteration(I2C_HandleTypeDef *hi2c, uint8_t *iteration);
HAL_StatusTypeDef tsl2585_set_calibration_nth_iteration(I2C_HandleTypeDef *hi2c, uint8_t iteration);

HAL_StatusTypeDef tsl2585_get_agc_calibration(I2C_HandleTypeDef *hi2c, bool *enabled);
HAL_StatusTypeDef tsl2585_set_agc_calibration(I2C_HandleTypeDef *hi2c, bool enabled);

HAL_StatusTypeDef tsl2585_get_single_shot_mode(I2C_HandleTypeDef *hi2c, bool *enabled);
HAL_StatusTypeDef tsl2585_set_single_shot_mode(I2C_HandleTypeDef *hi2c, bool enabled);

/**
 * Get the number of samples in an AGC measurement cycle.
 *
 * The total measurement time is based on a combination of the sample time
 * and the number of samples:
 * i.e. AGC ATIME = (AGC_NR_SAMPLES + 1) * (SAMPLE_TIME + 1) * 1.388889μs
 *
 * @param value An 11-bit value that determines the number of samples (0-2047)
 */
HAL_StatusTypeDef tsl2585_get_agc_num_samples(I2C_HandleTypeDef *hi2c, uint16_t *value);

/**
 * Set the number of samples in an AGC measurement cycle.
 *
 * @param value An 11-bit value that determines the number of samples (0-2047)
 */
HAL_StatusTypeDef tsl2585_set_agc_num_samples(I2C_HandleTypeDef *hi2c, uint16_t value);

/**
 * Get the sample time.
 *
 * The sample time is determined in units of 1.388889μs:
 * i.e. ALS Integration Time Step = (value + 1) * 1.388889μs
 *
 * @param value An 11-bit value that determines the sample time (0-2047)
 */
HAL_StatusTypeDef tsl2585_get_sample_time(I2C_HandleTypeDef *hi2c, uint16_t *value);

/**
 * Set the sample time.
 *
 * @param value An 11-bit value that determines the sample time (0-2047)
 */
HAL_StatusTypeDef tsl2585_set_sample_time(I2C_HandleTypeDef *hi2c, uint16_t value);

/**
 * Get the number of samples in an ALS integration cycle.
 *
 * The total measurement time is based on a combination of the sample time
 * and the number of samples:
 * i.e. ALS ATIME = (ALS_NR_SAMPLES + 1) * (SAMPLE_TIME + 1) * 1.388889μs
 *
 * @param value An 11-bit value that determines the number of samples (0-2047)
 */
HAL_StatusTypeDef tsl2585_get_als_num_samples(I2C_HandleTypeDef *hi2c, uint16_t *value);

/**
 * Set the number of samples in an ALS integration cycle.
 *
 * @param value An 11-bit value that determines the number of samples (0-2047)
 */
HAL_StatusTypeDef tsl2585_set_als_num_samples(I2C_HandleTypeDef *hi2c, uint16_t value);

/**
 * Get the ALS interrupt persistence value.
 *
 * This defines the number of consecutive occurrences that the ALS measurement
 * data must remain outside of the specified threshold range before an
 * interrupt is generated. The meaning of specific values is described in the
 * datasheet, however a value of 0 will interrupt on every ALS cycle.
 *
 * @param value A 4-bit value from 0x0 to 0xF
 */
HAL_StatusTypeDef tsl2585_get_als_interrupt_persistence(I2C_HandleTypeDef *hi2c, uint8_t *value);

/**
 * Set the ALS interrupt persistence value.
 *
 * @param value A 4-bit value from 0x0 to 0xF
 */
HAL_StatusTypeDef tsl2585_set_als_interrupt_persistence(I2C_HandleTypeDef *hi2c, uint8_t value);

HAL_StatusTypeDef tsl2585_get_als_status(I2C_HandleTypeDef *hi2c, uint8_t *status);
HAL_StatusTypeDef tsl2585_get_als_scale(I2C_HandleTypeDef *hi2c, uint8_t *scale);
HAL_StatusTypeDef tsl2585_get_als_msb_position(I2C_HandleTypeDef *hi2c, uint8_t *position);

HAL_StatusTypeDef tsl2585_get_als_data0(I2C_HandleTypeDef *hi2c, uint16_t *data);
HAL_StatusTypeDef tsl2585_get_als_data1(I2C_HandleTypeDef *hi2c, uint16_t *data);
HAL_StatusTypeDef tsl2585_get_als_data2(I2C_HandleTypeDef *hi2c, uint16_t *data);

const char* tsl2585_gain_str(tsl2585_gain_t gain);
float tsl2585_gain_value(tsl2585_gain_t gain);
float tsl2585_integration_time_ms(uint16_t sample_time, uint16_t num_samples);

#endif /* TSL2585_H */
