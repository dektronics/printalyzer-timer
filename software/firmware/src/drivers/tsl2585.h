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
    TSL2585_STEP0 = 0x01,
    TSL2585_STEP1 = 0x02,
    TSL2585_STEP2 = 0x04,
    TSL2585_STEP3 = 0x08,
    TSL2585_STEPS_NONE = 0x00,
    TSL2585_STEPS_ALL = 0x0F
} tsl2585_step_t;

typedef enum {
    TSL2585_PHD_0 = 0, /*!< TSL2585: IR,       TSL2521: IR */
    TSL2585_PHD_1,     /*!< TSL2585: Photopic, TSL2521: Clear */
    TSL2585_PHD_2,     /*!< TSL2585: IR,       TSL2521: Clear */
    TSL2585_PHD_3,     /*!< TSL2585: UV-A,     TSL2521: Clear */
    TSL2585_PHD_4,     /*!< TSL2585: UV-A,     TSL2521: Clear */
    TSL2585_PHD_5,     /*!< TSL2585: Photopic, TSL2521: IR    */
    TSL2585_PHD_MAX
} tsl2585_photodiode_t;

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

typedef struct {
    bool overflow;
    bool underflow;
    uint16_t level;
} tsl2585_fifo_status_t;

typedef enum {
    TSL2585_ALS_FIFO_16BIT = 0x00,
    TSL2585_ALS_FIFO_24BIT = 0x01,
    TSL2585_ALS_FIFO_32BIT = 0x03
} tsl2585_als_fifo_data_format_t;

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

/* STATUS3 register values */
#define TSL2585_STATUS3_AINT_HYST_STATE_VALID 0x80 /*!< Indicates that the ALS interrupt hysteresis state is valid */
#define TSL2585_STATUS3_AINT_HYST_STATE_RD    0x40 /*!< Indicates the state in the hysteresis defined with AINT_AILT and AINT_AIHT */
#define TSL2585_STATUS3_AINT_AIHT             0x20 /*!< ALS Interrupt High */
#define TSL2585_STATUS3_AINT_AILT             0x10 /*!< ALS Interrupt Low */
#define TSL2585_STATUS3_VSYNC_LOST            0x08 /*!< Indicates that synchronization is out of sync with clock provided at the VSYNC pin */
#define TSL2585_STATUS3_OSC_CALIB_SATURATION  0x02 /*!< Indicates that oscillator calibration is out of range */
#define TSL2585_STATUS3_OSC_CALIB_FINISHED    0x01 /*!< Indicates that oscillator calibration is finished */

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

/* VSYNC_CONTROL register values */
#define TSL2585_VSYNC_CONTROL_HOLD_VSYNC_PERIOD 0x02 /*!< If set to "1" VSYNC_PERIOD cannot be updated until read */
#define TSL2585_VSYNC_CONTROL_SW_VSYNC_TRIGGER  0x01 /*!< If set to "1", this bit can be used to trigger a SW sync */

/* VSYNC_CFG register values */
#define TSL2585_VSYNC_CFG_OSC_CALIB_DISABLED  0x00 /*!< Oscillator calibration disabled */
#define TSL2585_VSYNC_CFG_OSC_CALIB_AFTER_PON 0x40 /*!< Oscillator calibration after PON */
#define TSL2585_VSYNC_CFG_OSC_CALIB_ALWAYS    0x80 /*!< Oscillator calibration always on */
#define TSL2585_VSYNC_CFG_VSYNC_MODE          0x04 /*!< Select trigger signal (0=VSYNC/GPIO/INT, 1=SW_VSYNC_TRIGGER) */
#define TSL2585_VSYNC_CFG_VSYNC_SELECT        0x02 /*!< Select trigger pin (0=VSYNC/GPIO, 1=INT) */
#define TSL2585_VSYNC_CFG_VSYNC_INVERT        0x01 /*!< If set to "1" the VSYNC input signal is inverted */

/* VSYNC_GPIO_INT register values */
#define TSL2585_GPIO_INT_INT_INVERT        0x40 /*!< Set to invert the INT pin output */
#define TSL2585_GPIO_INT_INT_IN_EN         0x20 /*!< Set to configure the INT pin as input */
#define TSL2585_GPIO_INT_INT_IN            0x10 /*!< External HIGH or LOW value applied to INT pin */
#define TSL2585_GPIO_INT_VSYNC_GPIO_INVERT 0x08 /*!< Set to invert the VSYNC/GPIO output */
#define TSL2585_GPIO_INT_VSYNC_GPIO_IN_EN  0x04 /*!< Set to configure the VSYNC/GPIO pin as input */
#define TSL2585_GPIO_INT_VSYNC_GPIO_OUT    0x02 /*!< Set the VSYNC/GPIO pin HI or LOW */
#define TSL2585_GPIO_INT_VSYNC_GPIO_IN     0x01 /*!< External HIGH or LOW value applied to the VSYNC/GPIO pin */

HAL_StatusTypeDef tsl2585_init(I2C_HandleTypeDef *hi2c, uint8_t *sensor_id);

HAL_StatusTypeDef tsl2585_get_enable(I2C_HandleTypeDef *hi2c, uint8_t *value);
HAL_StatusTypeDef tsl2585_set_enable(I2C_HandleTypeDef *hi2c, uint8_t value);
HAL_StatusTypeDef tsl2585_enable(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef tsl2585_disable(I2C_HandleTypeDef *hi2c);

HAL_StatusTypeDef tsl2585_get_interrupt_enable(I2C_HandleTypeDef *hi2c, uint8_t *value);
HAL_StatusTypeDef tsl2585_set_interrupt_enable(I2C_HandleTypeDef *hi2c, uint8_t value);

HAL_StatusTypeDef tsl2585_set_sleep_after_interrupt(I2C_HandleTypeDef *hi2c, bool enabled);

HAL_StatusTypeDef tsl2585_soft_reset(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef tsl2585_clear_fifo(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef tsl2585_clear_sleep_after_interrupt(I2C_HandleTypeDef *hi2c);

HAL_StatusTypeDef tsl2585_enable_modulators(I2C_HandleTypeDef *hi2c, tsl2585_modulator_t mods);

HAL_StatusTypeDef tsl2585_get_status(I2C_HandleTypeDef *hi2c, uint8_t *status);
HAL_StatusTypeDef tsl2585_set_status(I2C_HandleTypeDef *hi2c, uint8_t status);

HAL_StatusTypeDef tsl2585_get_status2(I2C_HandleTypeDef *hi2c, uint8_t *status);
HAL_StatusTypeDef tsl2585_get_status3(I2C_HandleTypeDef *hi2c, uint8_t *status);
HAL_StatusTypeDef tsl2585_get_status4(I2C_HandleTypeDef *hi2c, uint8_t *status);
HAL_StatusTypeDef tsl2585_get_status5(I2C_HandleTypeDef *hi2c, uint8_t *status);

HAL_StatusTypeDef tsl2585_set_mod_gain_table_select(I2C_HandleTypeDef *hi2c, bool alternate);

HAL_StatusTypeDef tsl2585_get_max_mod_gain(I2C_HandleTypeDef *hi2c, tsl2585_gain_t *gain);
HAL_StatusTypeDef tsl2585_set_max_mod_gain(I2C_HandleTypeDef *hi2c, tsl2585_gain_t gain);

HAL_StatusTypeDef tsl2585_get_mod_gain(I2C_HandleTypeDef *hi2c, tsl2585_modulator_t mod, tsl2585_step_t step, tsl2585_gain_t *gain);
HAL_StatusTypeDef tsl2585_set_mod_gain(I2C_HandleTypeDef *hi2c, tsl2585_modulator_t mod, tsl2585_step_t step, tsl2585_gain_t gain);

/**
 * Gets whether residual measurement shall be performed on the selected modulator
 *
 * @param mod Modulator to configure residual measurement on
 * @param steps Mask of sequencer steps residual measurement is enabled for
 */
HAL_StatusTypeDef tsl2585_get_mod_residual_enable(I2C_HandleTypeDef *hi2c, tsl2585_modulator_t mod, tsl2585_step_t *steps);

/**
 * Sets whether residual measurement shall be performed on the selected modulator
 *
 * @param mod Modulator to configure residual measurement on
 * @param steps Mask of sequencer steps to enable residual measurement for
 */
HAL_StatusTypeDef tsl2585_set_mod_residual_enable(I2C_HandleTypeDef *hi2c, tsl2585_modulator_t mod, tsl2585_step_t steps);

HAL_StatusTypeDef tsl2585_set_mod_photodiode_smux(I2C_HandleTypeDef *hi2c,
    tsl2585_step_t step, const tsl2585_modulator_t phd_mod[static TSL2585_PHD_MAX]);

HAL_StatusTypeDef tsl2585_get_uv_calibration(I2C_HandleTypeDef *hi2c, uint8_t *value);

HAL_StatusTypeDef tsl2585_set_mod_idac_range(I2C_HandleTypeDef *hi2c, uint8_t value);

HAL_StatusTypeDef tsl2585_get_calibration_nth_iteration(I2C_HandleTypeDef *hi2c, uint8_t *iteration);
HAL_StatusTypeDef tsl2585_set_calibration_nth_iteration(I2C_HandleTypeDef *hi2c, uint8_t iteration);

HAL_StatusTypeDef tsl2585_get_agc_calibration(I2C_HandleTypeDef *hi2c, bool *enabled);
HAL_StatusTypeDef tsl2585_set_agc_calibration(I2C_HandleTypeDef *hi2c, bool enabled);

HAL_StatusTypeDef tsl2585_get_single_shot_mode(I2C_HandleTypeDef *hi2c, bool *enabled);
HAL_StatusTypeDef tsl2585_set_single_shot_mode(I2C_HandleTypeDef *hi2c, bool enabled);

HAL_StatusTypeDef tsl2585_get_vsync_period(I2C_HandleTypeDef *hi2c, uint16_t *period);
HAL_StatusTypeDef tsl2585_set_vsync_period(I2C_HandleTypeDef *hi2c, uint16_t period);

HAL_StatusTypeDef tsl2585_get_vsync_period_target(I2C_HandleTypeDef *hi2c, uint16_t *period_target, bool *use_fast_timing);
HAL_StatusTypeDef tsl2585_set_vsync_period_target(I2C_HandleTypeDef *hi2c, uint16_t period_target, bool use_fast_timing);

HAL_StatusTypeDef tsl2585_get_vsync_control(I2C_HandleTypeDef *hi2c, uint8_t *value);
HAL_StatusTypeDef tsl2585_set_vsync_control(I2C_HandleTypeDef *hi2c, uint8_t value);

HAL_StatusTypeDef tsl2585_get_vsync_cfg(I2C_HandleTypeDef *hi2c, uint8_t *value);
HAL_StatusTypeDef tsl2585_set_vsync_cfg(I2C_HandleTypeDef *hi2c, uint8_t value);

HAL_StatusTypeDef tsl2585_set_vsync_gpio_int(I2C_HandleTypeDef *hi2c, uint8_t value);

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
HAL_StatusTypeDef tsl2585_get_als_status2(I2C_HandleTypeDef *hi2c, uint8_t *status);
HAL_StatusTypeDef tsl2585_get_als_status3(I2C_HandleTypeDef *hi2c, uint8_t *status);

HAL_StatusTypeDef tsl2585_get_als_scale(I2C_HandleTypeDef *hi2c, uint8_t *scale);
HAL_StatusTypeDef tsl2585_set_als_scale(I2C_HandleTypeDef *hi2c, uint8_t scale);

HAL_StatusTypeDef tsl2585_get_fifo_als_status_write_enable(I2C_HandleTypeDef *hi2c, bool *enable);
HAL_StatusTypeDef tsl2585_set_fifo_als_status_write_enable(I2C_HandleTypeDef *hi2c, bool enable);

HAL_StatusTypeDef tsl2585_get_fifo_als_data_format(I2C_HandleTypeDef *hi2c, tsl2585_als_fifo_data_format_t *format);
HAL_StatusTypeDef tsl2585_set_fifo_als_data_format(I2C_HandleTypeDef *hi2c, tsl2585_als_fifo_data_format_t format);

HAL_StatusTypeDef tsl2585_get_als_msb_position(I2C_HandleTypeDef *hi2c, uint8_t *position);
HAL_StatusTypeDef tsl2585_set_als_msb_position(I2C_HandleTypeDef *hi2c, uint8_t position);

HAL_StatusTypeDef tsl2585_get_als_data0(I2C_HandleTypeDef *hi2c, uint16_t *data);
HAL_StatusTypeDef tsl2585_get_als_data1(I2C_HandleTypeDef *hi2c, uint16_t *data);
HAL_StatusTypeDef tsl2585_get_als_data2(I2C_HandleTypeDef *hi2c, uint16_t *data);

HAL_StatusTypeDef tsl2585_set_fifo_data_write_enable(I2C_HandleTypeDef *hi2c, tsl2585_modulator_t mod, bool enable);

HAL_StatusTypeDef tsl2585_get_fifo_status(I2C_HandleTypeDef *hi2c, tsl2585_fifo_status_t *status);

HAL_StatusTypeDef tsl2585_read_fifo(I2C_HandleTypeDef *hi2c, uint8_t *data, uint16_t len);

const char* tsl2585_gain_str(tsl2585_gain_t gain);
float tsl2585_gain_value(tsl2585_gain_t gain);
float tsl2585_integration_time_ms(uint16_t sample_time, uint16_t num_samples);

#endif /* TSL2585_H */
