#include "tsl2585.h"

#include "stm32f4xx_hal.h"

#define LOG_TAG "tsl2585"
#include <elog.h>

#include <math.h>

/* I2C device address */
static const uint8_t TSL2585_ADDRESS = 0x39 << 1; // Use 8-bit address

/*
 * Photodiode numbering:
 * 0 - IR        {1, x, 2, x}
 * 1 - Photopic  {0, 0, 0, 0}
 * 2 - IR        {1, x, x, 1}
 * 3 - UV-A      {0, 1, x, x}
 * 4 - UV-A      {1, 2, x, 2}
 * 5 - Photopic  {0, x, 1, x}
 *
 * Startup configuration:
 * Sequencer step 0:
 * - Photodiode 0(IR)  -> Modulator 1
 * - Photodiode 1(Pho) -> Modulator 0
 * - Photodiode 2(IR)  -> Modulator 1
 * - Photodiode 3(UVA) -> Modulator 0
 * - Photodiode 4(UVA) -> Modulator 1
 * - Photodiode 5(Pho) -> Modulator 0
 *
 * Sequencer step 1:
 * - Photodiode 0(IR)  -> NC
 * - Photodiode 1(Pho) -> Modulator 0
 * - Photodiode 2(IR)  -> NC
 * - Photodiode 3(UVA) -> Modulator 1
 * - Photodiode 4(UVA) -> Modulator 2
 * - Photodiode 5(Pho) -> NC
 *
 * Sequencer step 2:
 * - Photodiode 0(IR)  -> Modulator 2
 * - Photodiode 1(Pho) -> Modulator 0
 * - Photodiode 2(IR)  -> NC
 * - Photodiode 3(UVA) -> NC
 * - Photodiode 4(UVA) -> NC
 * - Photodiode 5(Pho) -> Modulator 1
 *
 * Sequencer step 3:
 * - Photodiode 0(IR)  -> NC
 * - Photodiode 1(Pho) -> Modulator 0
 * - Photodiode 2(IR)  -> Modulator 1
 * - Photodiode 3(UVA) -> NC
 * - Photodiode 4(UVA) -> Modulator 2
 * - Photodiode 5(Pho) -> NC
 *
 * Default gain: 128x
 * Sample time: 179 (250μs)
 * ALSIntegrationTimeStep = (SAMPLE_TIME+1) * 1.388889μs
 */

/* Registers */
#define TSL2585_UV_CALIB         0x08 /*!< UV calibration factor */
#define TSL2585_MOD_CHANNEL_CTRL 0x40 /*!< Modulator channel control */
#define TSL2585_ENABLE           0x80 /*!< Enables device states */
#define TSL2585_MEAS_MODE0       0x81 /*!< Measurement mode settings 0 */
#define TSL2585_MEAS_MODE1       0x82 /*!< Measurement mode settings 1 */
#define TSL2585_SAMPLE_TIME0     0x83 /*!< Flicker sample time settings 0 [7:0] */
#define TSL2585_SAMPLE_TIME1     0x84 /*!< Flicker sample time settings 1 [10:8] */
#define TSL2585_ALS_NR_SAMPLES0  0x85 /*!< ALS measurement time settings 0 [7:0] */
#define TSL2585_ALS_NR_SAMPLES1  0x86 /*!< ALS measurement time settings 1 [10:8] */
#define TSL2585_FD_NR_SAMPLES0   0x87 /*!< Flicker number of samples 0 [7:0] */
#define TSL2585_FD_NR_SAMPLES1   0x88 /*!< Flicker number of samples 1 [10:8] */
#define TSL2585_WTIME            0x89 /*!< Wait time */
#define TSL2585_AILT0            0x8A /*!< ALS Interrupt Low Threshold [7:0] */
#define TSL2585_AILT1            0x8B /*!< ALS Interrupt Low Threshold [15:8] */
#define TSL2585_AILT2            0x8C /*!< ALS Interrupt Low Threshold [23:16] */
#define TSL2585_AIHT0            0x8D /*!< ALS Interrupt High Threshold [7:0] */
#define TSL2585_AIHT1            0x8E /*!< ALS Interrupt High Threshold [15:8] */
#define TSL2585_AIHT2            0x8F /*!< ALS interrupt High Threshold [23:16] */
#define TSL2585_AUX_ID           0x90 /*!< Auxiliary identification */
#define TSL2585_REV_ID           0x91 /*!< Revision identification */
#define TSL2585_ID               0x92 /*!< Device identification */
#define TSL2585_STATUS           0x93 /*!< Device status information 1 */
#define TSL2585_ALS_STATUS       0x94 /*!< ALS Status information 1 */
#define TSL2585_ALS_DATA0_L      0x95 /*!< ALS data channel 0 low byte [7:0] */
#define TSL2585_ALS_DATA0_H      0x96 /*!< ALS data channel 0 high byte [15:8] */
#define TSL2585_ALS_DATA1_L      0x97 /*!< ALS data channel 1 low byte [7:0] */
#define TSL2585_ALS_DATA1_H      0x98 /*!< ALS data channel 1 high byte [15:8] */
#define TSL2585_ALS_DATA2_L      0x99 /*!< ALS data channel 2 low byte [7:0] */
#define TSL2585_ALS_DATA2_H      0x9A /*!< ALS data channel 2 high byte [15:8] */
#define TSL2585_ALS_STATUS2      0x9B /*!< ALS Status information 2 */
#define TSL2585_ALS_STATUS3      0x9C /*!< ALS Status information 3 */
#define TSL2585_STATUS2          0x9D /*!< Device Status information 2 */
#define TSL2585_STATUS3          0x9E /*!< Device Status information 3 */
#define TSL2585_STATUS4          0x9F /*!< Device Status information 4 */
#define TSL2585_STATUS5          0xA0 /*!< Device Status information 5 */
#define TSL2585_CFG0             0xA1 /*!< Configuration 0 */
#define TSL2585_CFG1             0xA2 /*!< Configuration 1 */
#define TSL2585_CFG2             0xA3 /*!< Configuration 2 */
#define TSL2585_CFG3             0xA4 /*!< Configuration 3 */
#define TSL2585_CFG4             0xA5 /*!< Configuration 4 */
#define TSL2585_CFG5             0xA6 /*!< Configuration 5 */
#define TSL2585_CFG6             0xA7 /*!< Configuration 6 */
#define TSL2585_CFG7             0xA8 /*!< Configuration 7 */
#define TSL2585_CFG8             0xA9 /*!< Configuration 8 */
#define TSL2585_CFG9             0xAA /*!< Configuration 9 */
#define TSL2585_AGC_NR_SAMPLES_L 0xAC /*!< Number of samples for measurement with AGC low [7:0] */
#define TSL2585_AGC_NR_SAMPLES_H 0xAD /*!< Number of samples for measurement with AGC high [10:8] */
#define TSL2585_TRIGGER_MODE     0xAE /*!< Wait Time Mode */
#define TSL2585_CONTROL          0xB1 /*!< Device control settings */
#define TSL2585_INTENAB          0xBA /*!< Enable interrupts */
#define TSL2585_SIEN             0xBB /*!< Enable saturation interrupts */
#define TSL2585_MOD_COMP_CFG1    0xCE /*!< Adjust AutoZero range */
#define TSL2585_MEAS_SEQR_FD_0                  0xCF /*!< Flicker measurement with sequencer on modulator0 */
#define TSL2585_MEAS_SEQR_ALS_FD_1              0xD0 /*!< ALS measurement with sequencer on all modulators */
#define TSL2585_MEAS_SEQR_APERS_AND_VSYNC_WAIT  0xD1 /*!< Defines the measurement sequencer pattern */
#define TSL2585_MEAS_SEQR_RESIDUAL_0            0xD2 /*!< Residual measurement configuration with sequencer on modulator0 and modulator1 */
#define TSL2585_MEAS_SEQR_RESIDUAL_1_AND_WAIT   0xD3 /*!< Residual measurement configuration with sequencer on modulator2 and wait time configuration for all sequencers */
#define TSL2585_MEAS_SEQR_STEP0_MOD_GAINX_L     0xD4 /*!< Gain of modulator0 and modulator1 for sequencer step 0 */
#define TSL2585_MEAS_SEQR_STEP0_MOD_GAINX_H     0xD5 /*!< Gain of modulator2 for sequencer step 0 */
#define TSL2585_MEAS_SEQR_STEP1_MOD_GAINX_L     0xD6 /*!< Gain of modulator0 and modulator1 for sequencer step 1 */
#define TSL2585_MEAS_SEQR_STEP1_MOD_GAINX_H     0xD7 /*!< Gain of modulator2 for sequencer step 1 */
#define TSL2585_MEAS_SEQR_STEP2_MOD_GAINX_L     0xD8 /*!< Gain of modulator0 and modulator1 for sequencer step 2 */
#define TSL2585_MEAS_SEQR_STEP2_MOD_GAINX_H     0xD9 /*!< Gain of modulator2 for sequencer step 2 */
#define TSL2585_MEAS_SEQR_STEP3_MOD_GAINX_L     0xDA /*!< Gain of modulator0 and modulator1 for sequencer step 3 */
#define TSL2585_MEAS_SEQR_STEP3_MOD_GAINX_H     0xDB /*!< Gain of modulator2 for sequencer step 3 */
#define TSL2585_MEAS_SEQR_STEP0_MOD_PHDX_SMUX_L 0xDC /*!< Photodiode 0-3 to modulator mapping through multiplexer for sequencer step 0 */
#define TSL2585_MEAS_SEQR_STEP0_MOD_PHDX_SMUX_H 0xDD /*!< Photodiode 4-5 to modulator mapping through multiplexer for sequencer step 0 */
#define TSL2585_MEAS_SEQR_STEP1_MOD_PHDX_SMUX_L 0xDE /*!< Photodiode 0-3 to modulator mapping through multiplexer for sequencer step 1 */
#define TSL2585_MEAS_SEQR_STEP1_MOD_PHDX_SMUX_H 0xDF /*!< Photodiode 4-5 to modulator mapping through multiplexer for sequencer step 1 */
#define TSL2585_MEAS_SEQR_STEP2_MOD_PHDX_SMUX_L 0xE0 /*!< Photodiode 0-3 to modulator mapping through multiplexer for sequencer step 2 */
#define TSL2585_MEAS_SEQR_STEP2_MOD_PHDX_SMUX_H 0xE1 /*!< Photodiode 4-5 to modulator mapping through multiplexer for sequencer step 2 */
#define TSL2585_MEAS_SEQR_STEP3_MOD_PHDX_SMUX_L 0xE2 /*!< Photodiode 0-3 to modulator mapping through multiplexer for sequencer step 3 */
#define TSL2585_MEAS_SEQR_STEP3_MOD_PHDX_SMUX_H 0xE3 /*!< Photodiode 4-5 to modulator mapping through multiplexer for sequencer step 3 */
#define TSL2585_MOD_CALIB_CFG0   0xE4 /*!< Modulator calibration config0 */
#define TSL2585_MOD_CALIB_CFG2   0xE6 /*!< Modulator calibration config2 */
#define TSL2585_VSYNC_PERIOD_L        0xF2 /*!< Measured VSYNC period */
#define TSL2585_VSYNC_PERIOD_H        0xF3 /*!< Read and clear measured VSYNC period */
#define TSL2585_VSYNC_PERIOD_TARGET_L 0xF4 /*!< Targeted VSYNC period */
#define TSL2585_VSYNC_PERIOD_TARGET_H 0xF5 /*!< Alternative target VSYNC period */
#define TSL2585_VSYNC_CONTROL         0xF6 /*!< Control of VSYNC period */
#define TSL2585_VSYNC_CFG             0xF7 /*!< Configuration of VSYNC input */
#define TSL2585_VSYNC_GPIO_INT        0xF8 /*!< Configuration of GPIO pin */
#define TSL2585_MOD_FIFO_DATA_CFG0 0xF9 /*!< Configuration of FIFO access for modulator 0 */
#define TSL2585_MOD_FIFO_DATA_CFG1 0xFA /*!< Configuration of FIFO access for modulator 1 */
#define TSL2585_MOD_FIFO_DATA_CFG2 0xFB /*!< Configuration of FIFO access for modulator 2 */
#define TSL2585_FIFO_THR         0xFC /*!< Configuration of FIFO threshold interrupt */
#define TSL2585_FIFO_STATUS0     0xFD /*!< FIFO status information 0 */
#define TSL2585_FIFO_STATUS1     0xFE /*!< FIFO status information 1 */
#define TSL2585_FIFO_DATA        0xFF /*!< FIFO readout */

/* CFG0 register values */
#define TSL2585_CFG0_SAI                 0x40
#define TSL2585_CFG0_LOWPOWER_IDLE       0x20

/* CONTROL register values */
#define TSL2585_CONTROL_SOFT_RESET       0x08
#define TSL2585_CONTROL_FIFO_CLR         0x02
#define TSL2585_CONTROL_CLEAR_SAI_ACTIVE 0x01

/* MOD_CALIB_CFG2 register values */
#define TSL2585_MOD_CALIB_NTH_ITERATION_RC_ENABLE 0x80
#define TSL2585_MOD_CALIB_NTH_ITERATION_AZ_ENABLE 0x40
#define TSL2585_MOD_CALIB_NTH_ITERATION_AGC_ENABLE 0x20
#define TSL2585_MOD_CALIB_RESIDUAL_ENABLE_AUTO_CALIB_ON_GAIN_CHANGE 0x10

/* MEAS_MODE0 register values */
#define TSL2585_MEAS_MODE0_STOP_AFTER_NTH_ITERATION 0x80
#define TSL2585_MEAS_MODE0_ENABLE_AGC_ASAT_DOUBLE_STEP_DOWN 0x40
#define TSL2585_MEAS_MODE0_MEASUREMENT_SEQUENCER_SINGLE_SHOT_MODE 0x20
#define TSL2585_MEAS_MODE0_MOD_FIFO_ALS_STATUS_WRITE_ENABLE 0x10

HAL_StatusTypeDef tsl2585_init(I2C_HandleTypeDef *hi2c, uint8_t *sensor_id)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    log_i("Initializing TSL2585");

    ret = HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_ID, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    log_i("Device ID: %02X", data);
    if (sensor_id) { sensor_id[0] = data; }

    if (data != 0x5C) {
        log_e("Invalid Device ID");
        return HAL_ERROR;
    }

    ret = HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_REV_ID, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    log_i("Revision ID: %02X", data);
    if (sensor_id) { sensor_id[1] = data; }

    ret = HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_AUX_ID, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    log_i("Aux ID: %02X", data & 0x0F);
    if (sensor_id) { sensor_id[2] = data & 0x0F; }

    ret = HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_STATUS, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    log_i("Status: %02X", data);

    /* Power on the sensor */
    ret = tsl2585_enable(hi2c);
    if (ret != HAL_OK) {
        return ret;
    }

    /* Power off the sensor */
    ret = tsl2585_disable(hi2c);
    if (ret != HAL_OK) {
        return ret;
    }

    log_i("TSL2585 Initialized");

    return HAL_OK;
}

HAL_StatusTypeDef tsl2585_set_enable(I2C_HandleTypeDef *hi2c, uint8_t value)
{
    uint8_t data = value & 0x43; /* Mask bits 6,1:0 */
    HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(hi2c, TSL2585_ADDRESS,
        TSL2585_ENABLE, I2C_MEMADD_SIZE_8BIT,
        &data, 1, HAL_MAX_DELAY);
    return ret;
}

HAL_StatusTypeDef tsl2585_enable(I2C_HandleTypeDef *hi2c)
{
    return tsl2585_set_enable(hi2c, TSL2585_ENABLE_PON | TSL2585_ENABLE_AEN);
}

HAL_StatusTypeDef tsl2585_disable(I2C_HandleTypeDef *hi2c)
{
    return tsl2585_set_enable(hi2c, 0x00);
}

HAL_StatusTypeDef tsl2585_set_interrupt_enable(I2C_HandleTypeDef *hi2c, uint8_t value)
{
    uint8_t data = value & 0x8D; /* Mask bits 7,3,2,0 */
    HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(hi2c, TSL2585_ADDRESS,
        TSL2585_INTENAB, I2C_MEMADD_SIZE_8BIT,
        &data, 1, HAL_MAX_DELAY);
    return ret;
}

HAL_StatusTypeDef tsl2585_set_sleep_after_interrupt(I2C_HandleTypeDef *hi2c, bool enabled)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    ret =  HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_CFG0, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    data = (data & ~TSL2585_CFG0_SAI) | (enabled ? TSL2585_CFG0_SAI : 0);

    ret = HAL_I2C_Mem_Write(hi2c, TSL2585_ADDRESS, TSL2585_CFG0, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);

    return ret;
}

HAL_StatusTypeDef tsl2585_clear_sleep_after_interrupt(I2C_HandleTypeDef *hi2c)
{
    uint8_t data;

    data = TSL2585_CONTROL_CLEAR_SAI_ACTIVE;

    return HAL_I2C_Mem_Write(hi2c, TSL2585_ADDRESS, TSL2585_CONTROL, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
}

HAL_StatusTypeDef tsl2585_enable_modulators(I2C_HandleTypeDef *hi2c, tsl2585_modulator_t mods)
{
    /* Mask bits [2:0] and invert since asserting disables the modulators */
    uint8_t data = ~((uint8_t)mods) & 0x07;

    HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(hi2c, TSL2585_ADDRESS,
        TSL2585_MOD_CHANNEL_CTRL, I2C_MEMADD_SIZE_8BIT,
        &data, 1, HAL_MAX_DELAY);
    return ret;
}

HAL_StatusTypeDef tsl2585_get_status(I2C_HandleTypeDef *hi2c, uint8_t *status)
{
    return HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_STATUS, I2C_MEMADD_SIZE_8BIT, status, 1, HAL_MAX_DELAY);
}

HAL_StatusTypeDef tsl2585_set_status(I2C_HandleTypeDef *hi2c, uint8_t status)
{
    return HAL_I2C_Mem_Write(hi2c, TSL2585_ADDRESS, TSL2585_STATUS, I2C_MEMADD_SIZE_8BIT, &status, 1, HAL_MAX_DELAY);
}

HAL_StatusTypeDef tsl2585_get_status2(I2C_HandleTypeDef *hi2c, uint8_t *status)
{
    return HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_STATUS2, I2C_MEMADD_SIZE_8BIT, status, 1, HAL_MAX_DELAY);
}

HAL_StatusTypeDef tsl2585_get_status3(I2C_HandleTypeDef *hi2c, uint8_t *status)
{
    return HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_STATUS3, I2C_MEMADD_SIZE_8BIT, status, 1, HAL_MAX_DELAY);
}

HAL_StatusTypeDef tsl2585_get_status4(I2C_HandleTypeDef *hi2c, uint8_t *status)
{
    return HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_STATUS4, I2C_MEMADD_SIZE_8BIT, status, 1, HAL_MAX_DELAY);
}

HAL_StatusTypeDef tsl2585_get_status5(I2C_HandleTypeDef *hi2c, uint8_t *status)
{
    return HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_STATUS5, I2C_MEMADD_SIZE_8BIT, status, 1, HAL_MAX_DELAY);
}

HAL_StatusTypeDef tsl2585_get_max_mod_gain(I2C_HandleTypeDef *hi2c, tsl2585_gain_t *gain)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    ret =  HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_CFG8, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    if (gain) {
        *gain = (tsl2585_gain_t)((data & 0xF0) >> 4);
    }

    return HAL_OK;
}

HAL_StatusTypeDef tsl2585_set_max_mod_gain(I2C_HandleTypeDef *hi2c, tsl2585_gain_t gain)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    ret =  HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_CFG8, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    data = (data & 0x0F) | (((uint8_t)gain & 0x0F) << 4);

    ret = HAL_I2C_Mem_Write(hi2c, TSL2585_ADDRESS, TSL2585_CFG8, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);

    return ret;
}

bool tsl2686_get_gain_register(tsl2585_modulator_t mod, tsl2585_step_t step, uint8_t *reg, bool *upper)
{
    *reg = 0;
    if (mod == TSL2585_MOD0) {
        if (step == TSL2585_STEP0) {
            *reg = TSL2585_MEAS_SEQR_STEP0_MOD_GAINX_L;
            *upper = false;
        } else if (step == TSL2585_STEP1) {
            *reg = TSL2585_MEAS_SEQR_STEP1_MOD_GAINX_L;
            *upper = false;
        } else if (step == TSL2585_STEP2) {
            *reg = TSL2585_MEAS_SEQR_STEP2_MOD_GAINX_L;
            *upper = false;
        }
    } else if (mod == TSL2585_MOD1) {
        if (step == TSL2585_STEP0) {
            *reg = TSL2585_MEAS_SEQR_STEP0_MOD_GAINX_L;
            *upper = true;
        } else if (step == TSL2585_STEP1) {
            *reg = TSL2585_MEAS_SEQR_STEP1_MOD_GAINX_L;
            *upper = true;
        } else if (step == TSL2585_STEP2) {
            *reg = TSL2585_MEAS_SEQR_STEP2_MOD_GAINX_L;
            *upper = true;
        }
    } else if (mod == TSL2585_MOD2) {
        if (step == TSL2585_STEP0) {
            *reg = TSL2585_MEAS_SEQR_STEP0_MOD_GAINX_H;
            *upper = false;
        } else if (step == TSL2585_STEP1) {
            *reg = TSL2585_MEAS_SEQR_STEP1_MOD_GAINX_H;
            *upper = false;
        } else if (step == TSL2585_STEP2) {
            *reg = TSL2585_MEAS_SEQR_STEP2_MOD_GAINX_H;
            *upper = false;
        }
    }
    return *reg != 0;
}

HAL_StatusTypeDef tsl2585_get_mod_gain(I2C_HandleTypeDef *hi2c, tsl2585_modulator_t mod, tsl2585_step_t step, tsl2585_gain_t *gain)
{
    HAL_StatusTypeDef ret;
    uint8_t data;
    uint8_t reg;
    bool upper;

    if (!tsl2686_get_gain_register(mod, step, &reg, &upper)) {
        return HAL_ERROR;
    }

    ret =  HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, reg, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    if (gain) {
        if (upper) {
            *gain = (data & 0xF0) >> 4;
        } else {
            *gain = (data & 0x0F);
        }
    }

    return HAL_OK;
}

HAL_StatusTypeDef tsl2585_set_mod_gain(I2C_HandleTypeDef *hi2c, tsl2585_modulator_t mod, tsl2585_step_t step, tsl2585_gain_t gain)
{
    HAL_StatusTypeDef ret;
    uint8_t data;
    uint8_t reg;
    bool upper;

    if (!tsl2686_get_gain_register(mod, step, &reg, &upper)) {
        return HAL_ERROR;
    }

    ret = HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, reg, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    if (upper) {
        data = (data & 0x0F) | ((uint8_t)gain << 4);
    } else {
        data = (data & 0xF0) | (uint8_t)gain;
    }

    ret = HAL_I2C_Mem_Write(hi2c, TSL2585_ADDRESS, reg, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);

    return ret;
}

HAL_StatusTypeDef tsl2585_get_calibration_nth_iteration(I2C_HandleTypeDef *hi2c, uint8_t *iteration)
{
    HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS,
        TSL2585_MOD_CALIB_CFG0, I2C_MEMADD_SIZE_8BIT,
        iteration, 1, HAL_MAX_DELAY);
    return ret;
}

HAL_StatusTypeDef tsl2585_set_calibration_nth_iteration(I2C_HandleTypeDef *hi2c, uint8_t iteration)
{
    HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(hi2c, TSL2585_ADDRESS,
        TSL2585_MOD_CALIB_CFG0, I2C_MEMADD_SIZE_8BIT,
        &iteration, 1, HAL_MAX_DELAY);
    return ret;
}

HAL_StatusTypeDef tsl2585_get_agc_calibration(I2C_HandleTypeDef *hi2c, bool *enabled)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    ret =  HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_MOD_CALIB_CFG2, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    if (enabled) {
        *enabled = (data & TSL2585_MOD_CALIB_NTH_ITERATION_AGC_ENABLE) != 0;
    }

    return HAL_OK;
}

HAL_StatusTypeDef tsl2585_set_agc_calibration(I2C_HandleTypeDef *hi2c, bool enabled)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    ret =  HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_MOD_CALIB_CFG2, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    data = (data & ~TSL2585_MOD_CALIB_NTH_ITERATION_AGC_ENABLE) | (enabled ? TSL2585_MOD_CALIB_NTH_ITERATION_AGC_ENABLE : 0);

    ret = HAL_I2C_Mem_Write(hi2c, TSL2585_ADDRESS, TSL2585_MOD_CALIB_CFG2, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);

    return ret;
}

HAL_StatusTypeDef tsl2585_get_single_shot_mode(I2C_HandleTypeDef *hi2c, bool *enabled)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    ret =  HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_MEAS_MODE0, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    if (enabled) {
        *enabled = (data & TSL2585_MEAS_MODE0_MEASUREMENT_SEQUENCER_SINGLE_SHOT_MODE) != 0;
    }

    return HAL_OK;
}

HAL_StatusTypeDef tsl2585_set_single_shot_mode(I2C_HandleTypeDef *hi2c, bool enabled)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    ret =  HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_MEAS_MODE0, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    data = (data & ~TSL2585_MEAS_MODE0_MEASUREMENT_SEQUENCER_SINGLE_SHOT_MODE)
        | (enabled ? TSL2585_MEAS_MODE0_MEASUREMENT_SEQUENCER_SINGLE_SHOT_MODE : 0);

    ret = HAL_I2C_Mem_Write(hi2c, TSL2585_ADDRESS, TSL2585_MEAS_MODE0, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);

    return ret;
}

HAL_StatusTypeDef tsl2585_get_vsync_period(I2C_HandleTypeDef *hi2c, uint16_t *period)
{
    HAL_StatusTypeDef ret;
    uint8_t buf[2];

    ret = HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS,
        TSL2585_VSYNC_PERIOD_L, I2C_MEMADD_SIZE_8BIT,
        buf, sizeof(buf), HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    if (period) {
        *period = (uint16_t)buf[0] | (uint16_t)((buf[1] << 8));
    }

    return HAL_OK;
}

HAL_StatusTypeDef tsl2585_set_vsync_period(I2C_HandleTypeDef *hi2c, uint16_t period)
{
    HAL_StatusTypeDef ret;
    uint8_t buf[2];

    buf[0] = (uint8_t)(period & 0x00FF);
    buf[1] = (uint8_t)((period & 0xFF00) >> 8);

    ret = HAL_I2C_Mem_Write(hi2c, TSL2585_ADDRESS, TSL2585_VSYNC_PERIOD_L,
        I2C_MEMADD_SIZE_8BIT, buf, sizeof(buf), HAL_MAX_DELAY);

    return ret;
}

HAL_StatusTypeDef tsl2585_set_vsync_period_target(I2C_HandleTypeDef *hi2c, uint16_t period_target, bool use_fast_timing)
{
    HAL_StatusTypeDef ret;
    uint8_t buf[2];

    if (period_target > 0x7FFF) {
        return HAL_ERROR;
    }

    buf[0] = (uint8_t)(period_target & 0x00FF);
    buf[1] = (uint8_t)((period_target & 0x7F00) >> 8) | (use_fast_timing ? 0x80 : 0x00);

    ret = HAL_I2C_Mem_Write(hi2c, TSL2585_ADDRESS, TSL2585_VSYNC_PERIOD_TARGET_L,
        I2C_MEMADD_SIZE_8BIT, buf, sizeof(buf), HAL_MAX_DELAY);

    return ret;

}

HAL_StatusTypeDef tsl2585_set_vsync_cfg(I2C_HandleTypeDef *hi2c, uint8_t value)
{
    uint8_t data = value & 0xC7;

    HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(hi2c, TSL2585_ADDRESS,
        TSL2585_VSYNC_CFG, I2C_MEMADD_SIZE_8BIT,
        &data, 1, HAL_MAX_DELAY);
    return ret;
}

HAL_StatusTypeDef tsl2585_set_vsync_gpio_int(I2C_HandleTypeDef *hi2c, uint8_t value)
{
    uint8_t data = value & 0x7F;

    HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(hi2c, TSL2585_ADDRESS,
        TSL2585_VSYNC_GPIO_INT, I2C_MEMADD_SIZE_8BIT,
        &data, 1, HAL_MAX_DELAY);
    return ret;
}

HAL_StatusTypeDef tsl2585_get_agc_num_samples(I2C_HandleTypeDef *hi2c, uint16_t *value)
{
    HAL_StatusTypeDef ret;
    uint8_t buf[2];

    ret = HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS,
        TSL2585_AGC_NR_SAMPLES_L, I2C_MEMADD_SIZE_8BIT,
        buf, sizeof(buf), HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    if (value) {
        *value = (uint16_t)buf[0] | (uint16_t)(((buf[1] & 0x07) << 8));
    }

    return HAL_OK;
}

HAL_StatusTypeDef tsl2585_set_agc_num_samples(I2C_HandleTypeDef *hi2c, uint16_t value)
{
    HAL_StatusTypeDef ret;
    uint8_t buf[2];

    if (value > 0x7FF) {
        return HAL_ERROR;
    }

    buf[0] = (uint8_t)(value & 0x0FF);
    buf[1] = (uint8_t)((value & 0x700) >> 8);

    ret = HAL_I2C_Mem_Write(hi2c, TSL2585_ADDRESS, TSL2585_AGC_NR_SAMPLES_L, I2C_MEMADD_SIZE_8BIT, buf, sizeof(buf), HAL_MAX_DELAY);

    return ret;
}

HAL_StatusTypeDef tsl2585_get_sample_time(I2C_HandleTypeDef *hi2c, uint16_t *value)
{
    HAL_StatusTypeDef ret;
    uint8_t buf[2];

    ret = HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS,
        TSL2585_SAMPLE_TIME0, I2C_MEMADD_SIZE_8BIT,
        buf, sizeof(buf), HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    if (value) {
        *value = (uint16_t)buf[0] | (uint16_t)(((buf[1] & 0x07) << 8));
    }

    return HAL_OK;
}

HAL_StatusTypeDef tsl2585_set_sample_time(I2C_HandleTypeDef *hi2c, uint16_t value)
{
    HAL_StatusTypeDef ret;
    uint8_t buf[2];

    if (value > 0x7FF) {
        return HAL_ERROR;
    }

    buf[0] = (uint8_t)(value & 0x0FF);
    buf[1] = (uint8_t)((value & 0x700) >> 8);

    ret = HAL_I2C_Mem_Write(hi2c, TSL2585_ADDRESS, TSL2585_SAMPLE_TIME0, I2C_MEMADD_SIZE_8BIT, buf, sizeof(buf), HAL_MAX_DELAY);

    return ret;
}

HAL_StatusTypeDef tsl2585_get_als_num_samples(I2C_HandleTypeDef *hi2c, uint16_t *value)
{
    HAL_StatusTypeDef ret;
    uint8_t buf[2];

    ret = HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS,
        TSL2585_ALS_NR_SAMPLES0, I2C_MEMADD_SIZE_8BIT,
        buf, sizeof(buf), HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    if (value) {
        *value = (uint16_t)buf[0] | (uint16_t)(((buf[1] & 0x07) << 8));
    }

    return HAL_OK;
}

HAL_StatusTypeDef tsl2585_set_als_num_samples(I2C_HandleTypeDef *hi2c, uint16_t value)
{
    HAL_StatusTypeDef ret;
    uint8_t buf[2];

    if (value > 0x7FF) {
        return HAL_ERROR;
    }

    buf[0] = (uint8_t)(value & 0x0FF);
    buf[1] = (uint8_t)((value & 0x700) >> 8);

    ret = HAL_I2C_Mem_Write(hi2c, TSL2585_ADDRESS, TSL2585_ALS_NR_SAMPLES0, I2C_MEMADD_SIZE_8BIT, buf, sizeof(buf), HAL_MAX_DELAY);

    return ret;
}

HAL_StatusTypeDef tsl2585_get_als_interrupt_persistence(I2C_HandleTypeDef *hi2c, uint8_t *value)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    ret =  HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_CFG5, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    if (value) {
        *value = data & 0x0F;
    }

    return HAL_OK;
}

HAL_StatusTypeDef tsl2585_set_als_interrupt_persistence(I2C_HandleTypeDef *hi2c, uint8_t value)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    ret =  HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_CFG5, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    data = (data & 0xF0) | (value & 0x0F);

    ret = HAL_I2C_Mem_Write(hi2c, TSL2585_ADDRESS, TSL2585_CFG5, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);

    return ret;
}

HAL_StatusTypeDef tsl2585_get_als_status(I2C_HandleTypeDef *hi2c, uint8_t *status)
{
    return HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_ALS_STATUS, I2C_MEMADD_SIZE_8BIT, status, 1, HAL_MAX_DELAY);
}

HAL_StatusTypeDef tsl2585_get_als_scale(I2C_HandleTypeDef *hi2c, uint8_t *scale)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    ret =  HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_MEAS_MODE0, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    if (scale) {
        *scale = data & 0x0F;
    }

    return HAL_OK;
}

HAL_StatusTypeDef tsl2585_get_als_msb_position(I2C_HandleTypeDef *hi2c, uint8_t *position)
{
    HAL_StatusTypeDef ret;
    uint8_t data;

    ret =  HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS, TSL2585_MEAS_MODE1, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    if (position) {
        *position = data & 0x1F;
    }

    return HAL_OK;
}

HAL_StatusTypeDef tsl2585_get_als_data0(I2C_HandleTypeDef *hi2c, uint16_t *data)
{
    HAL_StatusTypeDef ret;
    uint8_t buf[2];

    ret = HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS,
        TSL2585_ALS_DATA0_L, I2C_MEMADD_SIZE_8BIT,
        buf, sizeof(buf), HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    if (data) {
        *data = buf[0] | buf[1] << 8;
    }

    return HAL_OK;
}

HAL_StatusTypeDef tsl2585_get_als_data1(I2C_HandleTypeDef *hi2c, uint16_t *data)
{
    HAL_StatusTypeDef ret;
    uint8_t buf[2];

    ret = HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS,
        TSL2585_ALS_DATA1_L, I2C_MEMADD_SIZE_8BIT,
        buf, sizeof(buf), HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    if (data) {
        *data = buf[0] | buf[1] << 8;
    }

    return HAL_OK;
}

HAL_StatusTypeDef tsl2585_get_als_data2(I2C_HandleTypeDef *hi2c, uint16_t *data)
{
    HAL_StatusTypeDef ret;
    uint8_t buf[2];

    ret = HAL_I2C_Mem_Read(hi2c, TSL2585_ADDRESS,
        TSL2585_ALS_DATA2_L, I2C_MEMADD_SIZE_8BIT,
        buf, sizeof(buf), HAL_MAX_DELAY);
    if (ret != HAL_OK) {
        return ret;
    }

    if (data) {
        *data = buf[0] | buf[1] << 8;
    }

    return HAL_OK;
}

static const char *TSL2585_GAIN_STR[] = {
    "0.5x", "1x", "2x", "4x", "8x", "16x", "32x", "64x",
    "128x", "256x", "512x", "1024x", "2048x", "4096x"
};

const char* tsl2585_gain_str(tsl2585_gain_t gain)
{
    if (gain >= TSL2585_GAIN_0_5X && gain <= TSL2585_GAIN_4096X) {
        return TSL2585_GAIN_STR[gain];
    } else {
        return "?";
    }
}

static const float TSL2585_GAIN_VAL[] = {
    0.5F, 1.0F, 2.0F, 4.0F, 8.0F, 16.0F, 32.0F, 64.0F,
    128.0F, 256.0F, 512.0F, 1024.0F, 2048.0F, 4096.0F
};

float tsl2585_gain_value(tsl2585_gain_t gain)
{
    if (gain >= TSL2585_GAIN_0_5X && gain <= TSL2585_GAIN_4096X) {
        return TSL2585_GAIN_VAL[gain];
    } else {
        return NAN;
    }
}

float tsl2585_integration_time_ms(uint16_t sample_time, uint16_t num_samples)
{
    return ((num_samples + 1) * (sample_time + 1) * 1.388889F) / 1000.0F;
}
