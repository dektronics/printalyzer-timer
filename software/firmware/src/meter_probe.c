#include "meter_probe.h"

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <cmsis_os.h>

#include <string.h>
#include <math.h>

#define LOG_TAG "meter_probe"
#include <elog.h>

#include "board_config.h"
#include "meter_probe_settings.h"
#include "tsl2585.h"
#include "keypad.h"
#include "util.h"

extern TIM_HandleTypeDef htim3;

/**
 * Meter probe control event types.
 */
typedef enum {
    METER_PROBE_CONTROL_STOP = 0,
    METER_PROBE_CONTROL_START,
    METER_PROBE_CONTROL_SENSOR_ENABLE,
    METER_PROBE_CONTROL_SENSOR_ENABLE_SINGLE_SHOT,
    METER_PROBE_CONTROL_SENSOR_DISABLE,
    METER_PROBE_CONTROL_SENSOR_SET_GAIN,
    METER_PROBE_CONTROL_SENSOR_SET_INTEGRATION,
    METER_PROBE_CONTROL_SENSOR_ENABLE_AGC,
    METER_PROBE_CONTROL_SENSOR_DISABLE_AGC,
    METER_PROBE_CONTROL_SENSOR_TRIGGER_NEXT_READING,
    METER_PROBE_CONTROL_INTERRUPT
} meter_probe_control_event_type_t;

typedef struct {
    tsl2585_gain_t gain;
    tsl2585_modulator_t mod;
} sensor_control_gain_params_t;

typedef struct {
    uint16_t sample_time;
    uint16_t sample_count;
} sensor_control_integration_params_t;

typedef struct {
    uint16_t sample_count;
} sensor_control_agc_params_t;

typedef struct {
    uint32_t ticks;
} sensor_control_interrupt_params_t;

/**
 * Meter probe control event data.
 */
typedef struct {
    meter_probe_control_event_type_t event_type;
    osStatus_t *result;
    union {
        sensor_control_gain_params_t gain;
        sensor_control_integration_params_t integration;
        sensor_control_agc_params_t agc;
        sensor_control_interrupt_params_t interrupt;
    };
} meter_probe_control_event_t;

/**
 * Configuration state for the TSL2585 light sensor
 */
typedef struct {
    bool running;
    bool single_shot;
    uint8_t uv_calibration;
    tsl2585_gain_t gain[3];
    uint16_t sample_time;
    uint16_t sample_count;
    bool agc_enabled;
    uint16_t agc_sample_count;
    bool gain_pending;
    bool integration_pending;
    bool agc_pending;
    bool sai_active;
    bool agc_disabled_reset_gain;
    bool discard_next_reading;
} tsl2585_state_t;

/**
 * Modulator data read out of the FIFO
 */
typedef struct {
    uint8_t als_status;
    uint8_t als_status2;
    uint8_t als_status3;
    uint32_t als_data0;
    bool overflow;
} tsl2585_fifo_data_t;

/* Global I2C handle for the meter probe */
extern I2C_HandleTypeDef hi2c2;

static bool meter_probe_initialized = false;
static bool meter_probe_started = false;

static bool meter_probe_has_sensor_settings = false;
static meter_probe_settings_handle_t probe_settings_handle = {0};
static meter_probe_settings_tsl2585_t sensor_settings = {0};
static uint8_t sensor_device_id[3];
static tsl2585_state_t sensor_state = {0};
static bool sensor_osc_calibration_enabled = false;
static uint32_t last_aint_ticks = 0;

/* Queue for meter probe control events */
static osMessageQueueId_t meter_probe_control_queue = NULL;
static const osMessageQueueAttr_t meter_probe_control_queue_attrs = {
    .name = "meter_probe_control_queue"
};

/* Queue to hold the latest sensor reading */
static osMessageQueueId_t sensor_reading_queue = NULL;
static const osMessageQueueAttr_t sensor_reading_queue_attrs = {
    .name = "sensor_reading_queue"
};

/* Semaphore to synchronize sensor control calls */
static osSemaphoreId_t meter_probe_control_semaphore = NULL;
static const osSemaphoreAttr_t meter_probe_control_semaphore_attrs = {
    .name = "meter_probe_control_semaphore"
};

/* Only enable Photopic photodiodes on modulator 0 */
static const tsl2585_modulator_t sensor_phd_mod_vis[] = {
    0, TSL2585_MOD0, 0, 0, 0, TSL2585_MOD0
};

/* Meter probe control implementation functions */
static osStatus_t meter_probe_control_start();
static osStatus_t meter_probe_control_stop();
static osStatus_t meter_probe_control_sensor_enable(bool single_shot);
static osStatus_t meter_probe_control_sensor_disable();
static osStatus_t meter_probe_control_sensor_set_gain(const sensor_control_gain_params_t *params);
static osStatus_t meter_probe_control_sensor_integration(const sensor_control_integration_params_t *params);
static osStatus_t meter_probe_control_sensor_enable_agc(const sensor_control_agc_params_t *params);
static osStatus_t meter_probe_control_sensor_disable_agc();
static osStatus_t meter_probe_control_sensor_trigger_next_reading();
static osStatus_t meter_probe_control_interrupt(const sensor_control_interrupt_params_t *params);

static HAL_StatusTypeDef sensor_osc_calibration();
static void sensor_int_timer_output_init();
static void sensor_int_timer_output_start();
static void sensor_int_timer_output_stop();
static void sensor_int_gpio_exti_input();
static void sensor_int_gpio_exti_enable();
static void sensor_int_gpio_exti_disable();

static HAL_StatusTypeDef sensor_control_read_fifo(tsl2585_fifo_data_t *fifo_data);

void task_meter_probe_run(void *argument)
{
    osSemaphoreId_t task_start_semaphore = argument;
    meter_probe_control_event_t control_event;

    log_d("meter_probe_task start");

    /* Create the queue for meter probe control events */
    meter_probe_control_queue = osMessageQueueNew(20, sizeof(meter_probe_control_event_t), &meter_probe_control_queue_attrs);
    if (!meter_probe_control_queue) {
        log_e("Unable to create control queue");
        return;
    }

    /* Create the one-element queue to hold the latest sensor reading */
    sensor_reading_queue = osMessageQueueNew(1, sizeof(meter_probe_sensor_reading_t), &sensor_reading_queue_attrs);
    if (!sensor_reading_queue) {
        log_e("Unable to create reading queue");
        return;
    }

    /* Create the semaphore used to synchronize sensor control */
    meter_probe_control_semaphore = osSemaphoreNew(1, 0, &meter_probe_control_semaphore_attrs);
    if (!meter_probe_control_semaphore) {
        log_e("meter_probe_control_semaphore create error");
        return;
    }

    meter_probe_initialized = true;

    /* Release the startup semaphore */
    if (osSemaphoreRelease(task_start_semaphore) != osOK) {
        log_e("Unable to release task_start_semaphore");
        return;
    }

    /* Start the main control event loop */
    for (;;) {
        if(osMessageQueueGet(meter_probe_control_queue, &control_event, NULL, portMAX_DELAY) == osOK) {
            osStatus_t ret = osOK;
            switch (control_event.event_type) {
            case METER_PROBE_CONTROL_STOP:
                ret = meter_probe_control_stop();
                break;
            case METER_PROBE_CONTROL_START:
                ret = meter_probe_control_start();
                break;
            case METER_PROBE_CONTROL_SENSOR_ENABLE:
                ret = meter_probe_control_sensor_enable(false);
                break;
            case METER_PROBE_CONTROL_SENSOR_ENABLE_SINGLE_SHOT:
                ret = meter_probe_control_sensor_enable(true);
                break;
            case METER_PROBE_CONTROL_SENSOR_DISABLE:
                ret = meter_probe_control_sensor_disable();
                break;
            case METER_PROBE_CONTROL_SENSOR_SET_GAIN:
                ret = meter_probe_control_sensor_set_gain(&control_event.gain);
                break;
            case METER_PROBE_CONTROL_SENSOR_SET_INTEGRATION:
                ret = meter_probe_control_sensor_integration(&control_event.integration);
                break;
            case METER_PROBE_CONTROL_SENSOR_ENABLE_AGC:
                ret = meter_probe_control_sensor_enable_agc(&control_event.agc);
                break;
            case METER_PROBE_CONTROL_SENSOR_DISABLE_AGC:
                ret = meter_probe_control_sensor_disable_agc();
                break;
            case METER_PROBE_CONTROL_SENSOR_TRIGGER_NEXT_READING:
                ret = meter_probe_control_sensor_trigger_next_reading();
                break;
            case METER_PROBE_CONTROL_INTERRUPT:
                ret = meter_probe_control_interrupt(&control_event.interrupt);
                break;
            default:
                break;
            }

            /* Handle all external commands by propagating their completion */
            if (control_event.event_type != METER_PROBE_CONTROL_INTERRUPT) {
                if (control_event.result) {
                    *(control_event.result) = ret;
                }
                if (osSemaphoreRelease(meter_probe_control_semaphore) != osOK) {
                    log_e("Unable to release meter_probe_control_semaphore");
                }
            }
        }
    }

}

bool meter_probe_is_started()
{
    return meter_probe_started;
}

osStatus_t meter_probe_start()
{
    if (!meter_probe_initialized || meter_probe_started) { return osErrorResource; }

    osStatus_t result = osOK;
    meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_START,
        .result = &result
    };
    osMessageQueuePut(meter_probe_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(meter_probe_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_start()
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("meter_probe_control_start");

    /* Apply power to the meter probe port */
    HAL_GPIO_WritePin(SENSOR_VBUS_GPIO_Port, SENSOR_VBUS_Pin, GPIO_PIN_RESET);

    /* Brief delay to ensure power has stabilized */
    osDelay(1);

    do {
        /* Read the meter probe's settings memory */
        ret = meter_probe_settings_init(&probe_settings_handle, &hi2c2);

        /* If the first I2C operation since power-up fails, try reinitializing the I2C bus */
        if (ret == HAL_BUSY) {
            ret = HAL_I2C_DeInit(&hi2c2);
            if (ret != HAL_OK) {
                log_w("Unable to de-init I2C");
                break;
            }

            ret = HAL_I2C_Init(&hi2c2);
            if (ret != HAL_OK) {
                log_w("Unable to re-init I2C");
                break;
            }

            /* Now retry the operation */
            ret = meter_probe_settings_init(&probe_settings_handle, &hi2c2);
        }

        if (ret != HAL_OK) {
            break;
        }

        log_i("Meter probe: type=%d, rev=%d, serial=%ld",
            probe_settings_handle.id.probe_type,
            probe_settings_handle.id.probe_revision,
            probe_settings_handle.id.probe_serial);

        if (probe_settings_handle.id.probe_type != METER_PROBE_TYPE_TSL2585) {
            log_w("Unknown meter probe type");
            ret = HAL_ERROR;
            break;
        }

        /* Read the settings for the current sensor type */
        ret = meter_probe_settings_get_tsl2585(&probe_settings_handle, &sensor_settings);
        if (ret == HAL_OK) {
            meter_probe_has_sensor_settings = true;
        } else {
            log_w("Unable to load sensor calibration");
            meter_probe_has_sensor_settings = false;
        }

        /*
         * Do a basic initialization of the sensor, which verifies that
         * the sensor is functional and responding to commands.
         * This routine should complete with the sensor in a disabled
         * state.
         */
        ret = tsl2585_init(&hi2c2, sensor_device_id);
        if (ret != HAL_OK) { break; }

        ret = tsl2585_get_uv_calibration(&hi2c2, &sensor_state.uv_calibration);
        if (ret != HAL_OK) { break; }

        log_d("UV calibration value: %d", sensor_state.uv_calibration);

        /* Enable the meter probe button */
        keypad_enable_meter_probe();

        meter_probe_started = true;
    } while (0);

    if (!meter_probe_started) {
        log_w("Meter probe initialization failed");
        meter_probe_control_stop();
    }

    return hal_to_os_status(ret);
}

osStatus_t meter_probe_stop()
{
    if (!meter_probe_initialized || !meter_probe_started) { return osErrorResource; }

    osStatus_t result = osOK;
    meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_STOP,
        .result = &result
    };
    osMessageQueuePut(meter_probe_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(meter_probe_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_stop()
{
    log_d("meter_probe_control_stop");

    /* Disable the meter probe button */
    keypad_disable_meter_probe();

    /* Remove power from the meter probe port */
    HAL_GPIO_WritePin(SENSOR_VBUS_GPIO_Port, SENSOR_VBUS_Pin, GPIO_PIN_SET);

    /* Reset state variables */
    meter_probe_started = false;

    /* Clear the settings */
    memset(&probe_settings_handle, 0, sizeof(meter_probe_settings_handle_t));
    memset(&sensor_settings, 0, sizeof(meter_probe_settings_tsl2585_t));
    memset(&sensor_state, 0, sizeof(tsl2585_state_t));
    meter_probe_has_sensor_settings = false;

    return osOK;
}

osStatus_t meter_probe_get_device_info(meter_probe_device_info_t *info)
{
    if (!info) { return osErrorParameter; }
    if (!meter_probe_initialized || !meter_probe_started) { return osErrorResource; }

    memset(info, 0, sizeof(meter_probe_device_info_t));

    memcpy(&info->probe_id, &probe_settings_handle.id, sizeof(meter_probe_id_t));
    memcpy(info->sensor_id, sensor_device_id, 3);

    return osOK;
}

bool meter_probe_has_settings()
{
    if (!meter_probe_initialized || !meter_probe_started) { return false; }
    return meter_probe_has_sensor_settings;
}

osStatus_t meter_probe_get_settings(meter_probe_settings_t *settings)
{
    if (!settings) { return osErrorParameter; }
    if (!meter_probe_initialized || !meter_probe_started) { return osErrorResource; }

    memset(settings, 0, sizeof(meter_probe_settings_t));

    settings->type = probe_settings_handle.id.probe_type;
    if (settings->type == METER_PROBE_TYPE_TSL2585 && meter_probe_has_sensor_settings) {
        memcpy(&settings->settings_tsl2585, &sensor_settings, sizeof(meter_probe_settings_tsl2585_t));
    }

    return osOK;
}

osStatus_t meter_probe_set_settings(const meter_probe_settings_t *settings)
{
    if (!settings) { return osErrorParameter; }
    if (!meter_probe_initialized || !meter_probe_started || sensor_state.running) { return osErrorResource; }

    if (settings->type != probe_settings_handle.id.probe_type) {
        log_w("Invalid settings device type");
        return osErrorParameter;
    }

    if (settings->type == METER_PROBE_TYPE_TSL2585) {
        HAL_StatusTypeDef ret = meter_probe_settings_set_tsl2585(&probe_settings_handle, &settings->settings_tsl2585);
        return hal_to_os_status(ret);
    } else {
        log_w("Unsupported settings type");
        return osError;
    }
}

osStatus_t meter_probe_sensor_enable_osc_calibration()
{
    if (!meter_probe_initialized || !meter_probe_started || sensor_state.running) { return osErrorResource; }

    if (probe_settings_handle.id.probe_type == METER_PROBE_TYPE_TSL2585) {
        sensor_osc_calibration_enabled = true;
        return osOK;
    } else {
        return osErrorParameter;
    }
}

osStatus_t meter_probe_sensor_disable_osc_calibration()
{
    if (!meter_probe_initialized || !meter_probe_started || sensor_state.running) { return osErrorResource; }

    if (probe_settings_handle.id.probe_type == METER_PROBE_TYPE_TSL2585) {
        sensor_osc_calibration_enabled = false;
        return osOK;
    } else {
        return osErrorParameter;
    }
}

osStatus_t meter_probe_sensor_enable()
{
    if (!meter_probe_initialized || !meter_probe_started || sensor_state.running) { return osErrorResource; }

    osStatus_t result = osOK;
    meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_SENSOR_ENABLE,
        .result = &result
    };
    osMessageQueuePut(meter_probe_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(meter_probe_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_sensor_enable_single_shot()
{
    if (!meter_probe_initialized || !meter_probe_started || sensor_state.running) { return osErrorResource; }

    osStatus_t result = osOK;
    meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_SENSOR_ENABLE_SINGLE_SHOT,
        .result = &result
    };
    osMessageQueuePut(meter_probe_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(meter_probe_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_sensor_enable(bool single_shot)
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("meter_probe_control_sensor_enable: %s", single_shot ? "single_shot" : "continuous");

    /* Disable handling of sensor interrupts */
    sensor_int_gpio_exti_disable();

    do {
        /* Query the initial state of the sensor */
        if (!sensor_state.gain_pending) {
            ret = tsl2585_get_mod_gain(&hi2c2, TSL2585_MOD0, TSL2585_STEP0, &sensor_state.gain[0]);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_mod_gain(&hi2c2, TSL2585_MOD1, TSL2585_STEP0, &sensor_state.gain[1]);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_mod_gain(&hi2c2, TSL2585_MOD2, TSL2585_STEP0, &sensor_state.gain[2]);
            if (ret != HAL_OK) { break; }
        }

        if (!sensor_state.integration_pending) {
            ret = tsl2585_get_sample_time(&hi2c2, &sensor_state.sample_time);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_als_num_samples(&hi2c2, &sensor_state.sample_count);
            if (ret != HAL_OK) { break; }
        }

        if (!sensor_state.agc_pending) {
            ret = tsl2585_get_agc_num_samples(&hi2c2, &sensor_state.agc_sample_count);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_agc_calibration(&hi2c2, &sensor_state.agc_enabled);
            if (ret != HAL_OK) { break; }
        }

        /* Put the sensor into a known initial state */

        /* Enable writing of ALS status to the FIFO */
        ret = tsl2585_set_fifo_als_status_write_enable(&hi2c2, true);
        if (ret != HAL_OK) { break; }

        /* Enable writing of results to the FIFO */
        ret = tsl2585_set_fifo_data_write_enable(&hi2c2, TSL2585_MOD0, true);
        if (ret != HAL_OK) { break; }
        ret = tsl2585_set_fifo_data_write_enable(&hi2c2, TSL2585_MOD1, false);
        if (ret != HAL_OK) { break; }
        ret = tsl2585_set_fifo_data_write_enable(&hi2c2, TSL2585_MOD2, false);
        if (ret != HAL_OK) { break; }

        /* Set FIFO data format to 32-bits */
        ret = tsl2585_set_fifo_als_data_format(&hi2c2, TSL2585_ALS_FIFO_32BIT);
        if (ret != HAL_OK) { break; }

        /* Set MSB position for full 26-bit result */
        ret = tsl2585_set_als_msb_position(&hi2c2, 6);
        if (ret != HAL_OK) { break; }

        /* Make sure residuals are enabled */
        ret = tsl2585_set_mod_residual_enable(&hi2c2, TSL2585_MOD0, TSL2585_STEPS_ALL);
        if (ret != HAL_OK) { break; }
        ret = tsl2585_set_mod_residual_enable(&hi2c2, TSL2585_MOD1, TSL2585_STEPS_ALL);
        if (ret != HAL_OK) { break; }
        ret = tsl2585_set_mod_residual_enable(&hi2c2, TSL2585_MOD2, TSL2585_STEPS_ALL);
        if (ret != HAL_OK) { break; }

        /* Select alternate gain table, which caps gain at 256x but gives us more residual bits */
        ret = tsl2585_set_mod_gain_table_select(&hi2c2, true);
        if (ret != HAL_OK) { break; }

        /* Set maximum gain to 256x per app note on residual measurement */
        ret = tsl2585_set_max_mod_gain(&hi2c2, TSL2585_GAIN_256X);
        if (ret != HAL_OK) { break; }

        /* Enable modulator */
        ret = tsl2585_enable_modulators(&hi2c2, TSL2585_MOD0);
        if (ret != HAL_OK) { break; }

        /* Enable internal calibration on every sequencer round */
        ret = tsl2585_set_calibration_nth_iteration(&hi2c2, 1);
        if (ret != HAL_OK) { break; }

        /* Configure photodiodes for Photopic measurement only */
        ret = tsl2585_set_mod_photodiode_smux(&hi2c2, TSL2585_STEP0, sensor_phd_mod_vis);
        if (ret != HAL_OK) { break; }

        if (sensor_state.gain_pending) {
            ret = tsl2585_set_mod_gain(&hi2c2, TSL2585_MOD0, TSL2585_STEP0, sensor_state.gain[0]);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_mod_gain(&hi2c2, TSL2585_MOD1, TSL2585_STEP0, sensor_state.gain[1]);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_mod_gain(&hi2c2, TSL2585_MOD2, TSL2585_STEP0, sensor_state.gain[2]);
            if (ret != HAL_OK) { break; }

            sensor_state.gain_pending = false;
        }

        if (sensor_state.integration_pending) {
            ret = tsl2585_set_sample_time(&hi2c2, sensor_state.sample_time);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_als_num_samples(&hi2c2, sensor_state.sample_count);
            if (ret != HAL_OK) { break; }

            sensor_state.integration_pending = false;
        }

        if (sensor_state.agc_pending) {
            ret = tsl2585_set_agc_num_samples(&hi2c2, sensor_state.agc_sample_count);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_agc_calibration(&hi2c2, sensor_state.agc_enabled);
            if (ret != HAL_OK) { break; }

            sensor_state.agc_pending = false;
        }

        const float als_atime = tsl2585_integration_time_ms(sensor_state.sample_time, sensor_state.sample_count);
        const float agc_atime = tsl2585_integration_time_ms(sensor_state.sample_time, sensor_state.agc_sample_count);
        log_d("TSL2585 Initial State: Gain=%s, ALS_ATIME=%.2fms, AGC_ATIME=%.2fms",
            tsl2585_gain_str(sensor_state.gain[0]), als_atime, agc_atime);

        /* Clear out any old sensor readings */
        osMessageQueueReset(sensor_reading_queue);

        ret = tsl2585_set_single_shot_mode(&hi2c2, single_shot);
        if (ret != HAL_OK) { break; }

        ret = tsl2585_set_sleep_after_interrupt(&hi2c2, single_shot);
        if (ret != HAL_OK) { break; }

        /* Configure to interrupt on every ALS cycle */
        ret = tsl2585_set_als_interrupt_persistence(&hi2c2, 0);
        if (ret != HAL_OK) {
            break;
        }

        /* Enable sensor interrupts */
        ret = tsl2585_set_interrupt_enable(&hi2c2, TSL2585_INTENAB_AIEN);
        if (ret != HAL_OK) {
            break;
        }

        if (sensor_osc_calibration_enabled) {
            ret = sensor_osc_calibration();
            if (ret != HAL_OK) {
                break;
            }
        }

        /* Enable handling of sensor interrupts */
        sensor_int_gpio_exti_enable();

        /* Enable the sensor (ALS Enable and Power ON) */
        ret = tsl2585_enable(&hi2c2);
        if (ret != HAL_OK) {
            break;
        }

        sensor_state.sai_active = false;
        sensor_state.agc_disabled_reset_gain = false;
        sensor_state.discard_next_reading = false;
        sensor_state.single_shot = single_shot;
        sensor_state.running = true;
    } while (0);

    return hal_to_os_status(ret);
}

HAL_StatusTypeDef sensor_osc_calibration()
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t status3 = 0;
    uint16_t vsync_period = 0;

    log_d("TSL2585 oscillator calibration");

    do {
        /* Set VSYNC period target */
        ret = tsl2585_set_vsync_period_target(&hi2c2, 720, true); /* 1000Hz */
        if (ret != HAL_OK) { break; }

        /* Configure sensor interrupt pin as an input */
        ret = tsl2585_set_vsync_gpio_int(&hi2c2, TSL2585_GPIO_INT_INT_IN_EN | TSL2585_GPIO_INT_VSYNC_GPIO_OUT);
        if (ret != HAL_OK) { break; }

        /* Set calibration mode and configure sensor to use the interrupt pin for VSYNC */
        ret = tsl2585_set_vsync_cfg(&hi2c2, TSL2585_VSYNC_CFG_OSC_CALIB_AFTER_PON | TSL2585_VSYNC_CFG_VSYNC_SELECT);
        if (ret != HAL_OK) { break; }

        /* Reconfigure SENSOR_INT as a PWM output */
        sensor_int_timer_output_init();

        /* Start PWM output on SENSOR_INT for VSYNC */
        sensor_int_timer_output_start();

        /* Short delay for output to stabilize */
        osDelay(5);

        /* Power on the sensor (PON only) */
        tsl2585_set_enable(&hi2c2, TSL2585_ENABLE_PON);
        if (ret != HAL_OK) { break; }

        /* Poll STATUS3 until calibration is complete */
        uint32_t poll_start = osKernelGetTickCount();
        do {
            ret = tsl2585_get_status3(&hi2c2, &status3);
            if (ret != HAL_OK) { break; }

            if ((status3 & TSL2585_STATUS3_OSC_CALIB_FINISHED) != 0) { break; }
            osThreadYield();
        } while (ret == HAL_OK && (osKernelGetTickCount() - poll_start) < pdMS_TO_TICKS(1000));

        if ((status3 & TSL2585_STATUS3_OSC_CALIB_FINISHED) == 0) {
            log_w("VSYNC calibration timeout");
            ret = HAL_TIMEOUT;
            break;
        }

        ret = tsl2585_get_vsync_period(&hi2c2, &vsync_period);
        if (ret != HAL_OK) { break; }

        log_d("VSYNC_PERIOD=%d (%f Hz)", vsync_period, (1.0F/(vsync_period * 1.388889F))*1000000.0F);
    } while (0);

    /* Stop PWM output on SENSOR_INT for VSYNC */
    sensor_int_timer_output_stop();

    /* Reinitialize the INT pin as an input */
    sensor_int_gpio_exti_input();

    /* Try to reconfigure the sensor pins regardless of results */
    do {
        /* Reconfigure the INT pin of the sensor back to its default */
        ret = tsl2585_set_vsync_gpio_int(&hi2c2, TSL2585_GPIO_INT_VSYNC_GPIO_OUT);
        if (ret != HAL_OK) { break; }

        /* Reconfigure the VSYNC of the sensor back to its default */
        ret = tsl2585_set_vsync_cfg(&hi2c2, 0x00);
        if (ret != HAL_OK) { break; }
    } while (0);

    /* Disable the sensor in case of failure */
    if (ret != HAL_OK) {
        log_w("VSYNC calibration failed");
        tsl2585_disable(&hi2c2);
    }

    return ret;
}

void sensor_int_timer_output_init()
{
    /* Reconfigure SENSOR_INT as a PWM output driven by TIM3/CH4 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = SENSOR_INT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(SENSOR_INT_GPIO_Port, &GPIO_InitStruct);
}

void sensor_int_timer_output_start()
{
    /* Start PWM output on SENSOR_INT */
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
}

void sensor_int_timer_output_stop()
{
    /* Stop PWM output on SENSOR_INT */
    HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_4);
}

void sensor_int_gpio_exti_input()
{
    /* Reconfigure SENSOR_INT as its default mode as an EXTI input */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = SENSOR_INT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void sensor_int_gpio_exti_enable()
{
    /* Enable interrupts from the SENSOR_INT pin */
    HAL_NVIC_EnableIRQ(EXTI1_IRQn);
}

void sensor_int_gpio_exti_disable()
{
    /* Disable interrupts from the SENSOR_INT pin */
    HAL_NVIC_DisableIRQ(EXTI1_IRQn);
}

osStatus_t meter_probe_sensor_disable()
{
    if (!meter_probe_initialized || !meter_probe_started || !sensor_state.running) { return osErrorResource; }

    osStatus_t result = osOK;
    meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_SENSOR_DISABLE,
        .result = &result
    };
    osMessageQueuePut(meter_probe_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(meter_probe_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_sensor_disable()
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("meter_probe_control_sensor_disable");

    do {
        ret = tsl2585_disable(&hi2c2);
        if (ret != HAL_OK) { break; }
        sensor_state.running = false;
    } while (0);

    return hal_to_os_status(ret);
}

osStatus_t meter_probe_sensor_set_gain(tsl2585_gain_t gain)
{
    if (!meter_probe_initialized || !meter_probe_started) { return osErrorResource; }

    osStatus_t result = osOK;
    meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_SENSOR_SET_GAIN,
        .result = &result,
        .gain = {
            .gain = gain,
            .mod = TSL2585_MOD0
        }
    };
    osMessageQueuePut(meter_probe_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(meter_probe_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_sensor_set_gain(const sensor_control_gain_params_t *params)
{
    HAL_StatusTypeDef ret = HAL_OK;
    log_d("meter_probe_control_sensor_set_gain: %d, 0x%02X", params->gain, params->mod);

    uint8_t mod_index;
    switch (params->mod) {
    case TSL2585_MOD0:
        mod_index = 0;
        break;
    case TSL2585_MOD1:
        mod_index = 1;
        break;
    case TSL2585_MOD2:
        mod_index = 2;
        break;
    default:
        return osErrorParameter;
    }

    if (sensor_state.running) {
        ret = tsl2585_set_mod_gain(&hi2c2, params->mod, TSL2585_STEP0, params->gain);
        if (ret == HAL_OK) {
            sensor_state.gain[mod_index] = params->gain;
        }

        sensor_state.discard_next_reading = true;
        osMessageQueueReset(sensor_reading_queue);
    } else {
        sensor_state.gain[mod_index] = params->gain;
        sensor_state.gain_pending = true;
    }

    return hal_to_os_status(ret);
}

osStatus_t meter_probe_sensor_set_integration(uint16_t sample_time, uint16_t sample_count)
{
    if (!meter_probe_initialized || !meter_probe_started) { return osErrorResource; }

    osStatus_t result = osOK;
    meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_SENSOR_SET_INTEGRATION,
        .result = &result,
        .integration = {
            .sample_time = sample_time,
            .sample_count = sample_count
        }
    };
    osMessageQueuePut(meter_probe_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(meter_probe_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_sensor_integration(const sensor_control_integration_params_t *params)
{
    HAL_StatusTypeDef ret = HAL_OK;
    log_d("meter_probe_control_sensor_integration: %d, %d", params->sample_time, params->sample_count);

    if (sensor_state.running) {
        ret = tsl2585_set_sample_time(&hi2c2, params->sample_time);
        if (ret == HAL_OK) {
            sensor_state.sample_time = params->sample_time;
        }

        ret = tsl2585_set_als_num_samples(&hi2c2, params->sample_count);
        if (ret == HAL_OK) {
            sensor_state.sample_count = params->sample_count;
        }

        sensor_state.discard_next_reading = true;
        osMessageQueueReset(sensor_reading_queue);
    } else {
        sensor_state.sample_time = params->sample_time;
        sensor_state.sample_count = params->sample_count;
        sensor_state.integration_pending = true;
    }

    return hal_to_os_status(ret);
}

osStatus_t meter_probe_sensor_enable_agc(uint16_t sample_count)
{
    if (!meter_probe_initialized || !meter_probe_started) { return osErrorResource; }

    osStatus_t result = osOK;
    meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_SENSOR_ENABLE_AGC,
        .result = &result,
        .agc = {
            .sample_count = sample_count
        }
    };
    osMessageQueuePut(meter_probe_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(meter_probe_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_sensor_enable_agc(const sensor_control_agc_params_t *params)
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("meter_probe_control_sensor_enable_agc");

    if (sensor_state.running) {
        ret = tsl2585_set_agc_num_samples(&hi2c2, params->sample_count);
        if (ret == HAL_OK) {
            sensor_state.agc_sample_count = params->sample_count;
        }

        ret = tsl2585_set_agc_calibration(&hi2c2, true);
        if (ret == HAL_OK) {
            sensor_state.agc_enabled = true;
        }
    } else {
        sensor_state.agc_enabled = true;
        sensor_state.agc_sample_count = params->sample_count;
        sensor_state.agc_pending = true;
    }

    return hal_to_os_status(ret);
}

osStatus_t meter_probe_sensor_disable_agc()
{
    if (!meter_probe_initialized || !meter_probe_started) { return osErrorResource; }

    osStatus_t result = osOK;
    meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_SENSOR_DISABLE_AGC,
        .result = &result
    };
    osMessageQueuePut(meter_probe_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(meter_probe_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_sensor_disable_agc()
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("meter_probe_control_sensor_disable_agc");

    if (sensor_state.running) {
        do {
            ret = tsl2585_set_agc_calibration(&hi2c2, false);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_agc_num_samples(&hi2c2, 0);
            if (ret != HAL_OK) { break; }
        } while (0);
        if (ret == HAL_OK) {
            sensor_state.agc_enabled = false;
            sensor_state.agc_disabled_reset_gain = true;
            sensor_state.discard_next_reading = true;
        }
    } else {
        sensor_state.agc_enabled = false;
        sensor_state.agc_pending = true;
    }

    return hal_to_os_status(ret);
}

osStatus_t meter_probe_sensor_trigger_next_reading()
{
    if (!meter_probe_initialized || !meter_probe_started || !sensor_state.running) { return osErrorResource; }

    if (!sensor_state.single_shot) {
        log_e("Not in single-shot mode");
        return osErrorResource;
    }

    osStatus_t result = osOK;
    meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_SENSOR_TRIGGER_NEXT_READING,
        .result = &result
    };
    osMessageQueuePut(meter_probe_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(meter_probe_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_sensor_trigger_next_reading()
{
    HAL_StatusTypeDef ret = HAL_OK;
    log_d("meter_probe_control_sensor_trigger_next_reading");

    if (!sensor_state.sai_active) {
        uint8_t status4 = 0;
        tsl2585_get_status4(&hi2c2, &status4);
        if ((status4 & TSL2585_STATUS4_SAI_ACTIVE) != 0) {
            sensor_state.sai_active = true;
        } else {
            log_e("Integration cycle in progress");
            return osErrorResource;
        }
    }

    ret = tsl2585_clear_sleep_after_interrupt(&hi2c2);
    if (ret == HAL_OK) {
        sensor_state.sai_active = false;
    }

    return hal_to_os_status(ret);
}

osStatus_t meter_probe_sensor_clear_last_reading()
{
    return osMessageQueueReset(sensor_reading_queue);
}

osStatus_t meter_probe_sensor_get_next_reading(meter_probe_sensor_reading_t *reading, uint32_t timeout)
{
    if (!meter_probe_initialized || !meter_probe_started || !sensor_state.running) { return osErrorResource; }

    if (!reading) {
        return osErrorParameter;
    }

    return osMessageQueueGet(sensor_reading_queue, reading, NULL, timeout);
}

meter_probe_result_t meter_probe_measure(float *lux)
{
    meter_probe_result_t result = METER_READING_OK;
    const int max_count = 10;
    osStatus_t ret = osOK;
    meter_probe_sensor_reading_t reading;
    int count = 0;
    float reading_lux = NAN;
    bool has_result = false;

    if (!lux) {
        return METER_READING_FAIL;
    }

    //TODO Implement some looping for value averaging and waiting for AGC to settle
    do {
        ret = meter_probe_sensor_get_next_reading(&reading, 500);
        if (ret == osErrorTimeout) { return METER_READING_TIMEOUT; }
        else if (ret != osOK) { return METER_READING_FAIL; }

        if (reading.result_status == METER_SENSOR_RESULT_VALID) {
            has_result = true;
            break;
        }

        count++;

    } while (count < max_count);

    if (has_result) {
        if (reading.result_status == METER_SENSOR_RESULT_VALID) {
            reading_lux = meter_probe_lux_result(&reading);
            if (!isnormal(reading_lux)) {
                log_w("Could not calculate lux from sensor reading");
                result = METER_READING_FAIL;
            } else if (reading_lux < 0.0001F) {
                log_w("Lux calculation result is too low: %f", reading_lux);
                result = METER_READING_LOW;
            } else {
                result = METER_READING_OK;
            }
        } else if (reading.result_status == METER_SENSOR_RESULT_SATURATED_ANALOG
            || reading.result_status == METER_SENSOR_RESULT_SATURATED_DIGITAL) {
            result = METER_READING_HIGH;
        } else {
            result = METER_READING_FAIL;
        }
    } else {
        result = METER_READING_FAIL;
    }

    *lux = reading_lux;
    return result;
}

float meter_probe_basic_result(const meter_probe_sensor_reading_t *sensor_reading)
{
    if (!sensor_reading) { return NAN; }

    const float atime_ms = tsl2585_integration_time_ms(sensor_reading->sample_time, sensor_reading->sample_count);

    float als_gain;
    if (sensor_reading->gain <= TSL2585_GAIN_256X) {
        als_gain = sensor_settings.cal_gain.values[sensor_reading->gain];
    } else {
        als_gain = tsl2585_gain_value(sensor_reading->gain);
    }

    if (!is_valid_number(atime_ms) || !is_valid_number(als_gain)) { return NAN; }

    /* Divide to get numbers in a similar range as previous sensors */
    float als_reading = (float)sensor_reading->als_data / 16.0F;

    /* Calculate the basic reading */
    float basic_reading = als_reading / (atime_ms * als_gain);

    /* Apply the slope correction */
    const meter_probe_settings_tsl2585_cal_slope_t *cal_slope = &sensor_settings.cal_slope;
    float l_reading = log10f(basic_reading);
    float l_expected = cal_slope->b0 + (cal_slope->b1 * l_reading) + (cal_slope->b2 * powf(l_reading, 2.0F));
    float corr_reading = powf(10.0F, l_expected);

    return corr_reading;
}

float meter_probe_lux_result(const meter_probe_sensor_reading_t *sensor_reading)
{
    if (!sensor_reading) { return NAN; }
    if (sensor_reading->gain >= TSL2585_GAIN_MAX) { return NAN; }
    if (!meter_probe_has_sensor_settings) { return NAN; }

    const float slope = NAN; //sensor_settings.gain_cal[sensor_reading->gain].slope;
    const float offset = NAN; //sensor_settings.gain_cal[sensor_reading->gain].offset;
    if (!is_valid_number(slope) || !is_valid_number(offset)) { return NAN; }

    const float basic_value = meter_probe_basic_result(sensor_reading);
    if (!is_valid_number(basic_value)) { return NAN; }

    return (basic_value * slope) + offset;
}

void meter_probe_int_handler()
{
    if (!meter_probe_initialized || !meter_probe_started || !sensor_state.running) { return; }

    meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_INTERRUPT,
        .interrupt = {
            .ticks = osKernelGetTickCount()
        }
    };

    osMessageQueuePut(meter_probe_control_queue, &control_event, 0, 0);
}

osStatus_t meter_probe_control_interrupt(const sensor_control_interrupt_params_t *params)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t status = 0;
    meter_probe_sensor_reading_t reading = {0};
    bool has_reading = false;

    TaskHandle_t current_task_handle = xTaskGetCurrentTaskHandle();
    UBaseType_t current_task_priority = uxTaskPriorityGet(current_task_handle);
    vTaskPrioritySet(current_task_handle, osPriorityRealtime);

#if 0
    log_d("meter_probe_control_interrupt");
#endif

    if (!meter_probe_initialized || !meter_probe_started || !sensor_state.running) {
        log_d("Unexpected meter probe interrupt");
    }

    do {
        /* Get the interrupt status */
        ret = tsl2585_get_status(&hi2c2, &status);
        if (ret != HAL_OK) { break; }

#if 0
        /* Log interrupt flags */
        log_d("MINT=%d, AINT=%d, FINT=%d, SINT=%d",
            (status & TSL2585_STATUS_MINT) != 0,
            (status & TSL2585_STATUS_AINT) != 0,
            (status & TSL2585_STATUS_FINT) != 0,
            (status & TSL2585_STATUS_SINT) != 0);
#endif

        if ((status & TSL2585_STATUS_AINT) != 0) {
            uint32_t elapsed_ticks = params->ticks - last_aint_ticks;
            last_aint_ticks = params->ticks;

            tsl2585_fifo_data_t fifo_data;

            ret = sensor_control_read_fifo(&fifo_data);
            if (ret != HAL_OK) { break; }

            if (fifo_data.overflow) {
                reading.als_data = UINT32_MAX;
                reading.gain = sensor_state.gain[0];
                reading.result_status = METER_SENSOR_RESULT_INVALID;
            } else if ((fifo_data.als_status & TSL2585_ALS_DATA0_ANALOG_SATURATION_STATUS) != 0) {
#if 0
                log_d("TSL2585: [analog saturation]");
#endif
                reading.als_data = UINT32_MAX;
                reading.gain = sensor_state.gain[0];
                reading.result_status = METER_SENSOR_RESULT_SATURATED_ANALOG;
            } else {
                tsl2585_gain_t als_gain = (fifo_data.als_status2 & 0x0F);

                /* If AGC is enabled, then update the configured gain value */
                if (sensor_state.agc_enabled) {
                    sensor_state.gain[0] = als_gain;
                }

                reading.als_data = fifo_data.als_data0;
                reading.gain = als_gain;
                reading.result_status = METER_SENSOR_RESULT_VALID;
            }

            if (!sensor_state.discard_next_reading) {
                /* Fill out other reading fields */
                reading.sample_time = sensor_state.sample_time;
                reading.sample_count = sensor_state.sample_count;
                reading.ticks = params->ticks;
                reading.elapsed_ticks = elapsed_ticks;

                has_reading = true;
            } else {
                sensor_state.discard_next_reading = false;
            }

            /*
             * If AGC was just disabled, then reset the gain to its last
             * known value and ignore the reading. This is necessary because
             * disabling AGC on its own seems to reset the gain to a low
             * default, and attempting to set it immediately after setting
             * the registers to disable AGC does not seem to take.
             */
            if (sensor_state.agc_disabled_reset_gain) {
                ret = tsl2585_set_mod_gain(&hi2c2, TSL2585_MOD0, TSL2585_STEP0, sensor_state.gain[0]);
                if (ret != HAL_OK) { break; }
                sensor_state.agc_disabled_reset_gain = false;
                sensor_state.discard_next_reading = true;
            }
        }

        /* Clear the interrupt status */
        ret = tsl2585_set_status(&hi2c2, status);
        if (ret != HAL_OK) { break; }

        /* Check single-shot specific state */
        if (sensor_state.single_shot) {
            uint8_t status4 = 0;
            ret = tsl2585_get_status4(&hi2c2, &status4);
            if (ret != HAL_OK) { break; }
            if ((status4 & TSL2585_STATUS4_SAI_ACTIVE) != 0) {
#if 0
                log_d("Sleep after interrupt");
#endif
                sensor_state.sai_active = true;
            }
        }
    } while (0);

    vTaskPrioritySet(current_task_handle, current_task_priority);

    if (has_reading) {
#if 0
        log_d("TSL2585: MOD0=%lu, Gain=[%s], Time=%.2fms",
            reading.als_data, tsl2585_gain_str(reading.gain),
            tsl2585_integration_time_ms(sensor_state.sample_time, sensor_state.sample_count));
#endif

        QueueHandle_t queue = (QueueHandle_t)sensor_reading_queue;
        xQueueOverwrite(queue, &reading);
    }

    return hal_to_os_status(ret);
}

HAL_StatusTypeDef sensor_control_read_fifo(tsl2585_fifo_data_t *fifo_data)
{
    HAL_StatusTypeDef ret;
    tsl2585_fifo_status_t fifo_status;
    uint8_t data[7];
    uint8_t counter = 0;
    const uint8_t data_size = 7;

    do {
        ret = tsl2585_get_fifo_status(&hi2c2, &fifo_status);
        if (ret != HAL_OK) { break; }

#if 0
        log_d("FIFO_STATUS: OVERFLOW=%d, UNDERFLOW=%d, LEVEL=%d", fifo_status.overflow, fifo_status.underflow, fifo_status.level);
#endif

        if (fifo_status.overflow) {
            log_w("FIFO overflow, clearing");
            ret = tsl2585_clear_fifo(&hi2c2);
            if (ret != HAL_OK) { break; }

            if (fifo_data) {
                memset(fifo_data, 0, sizeof(tsl2585_fifo_data_t));
                fifo_data->overflow = true;
            }

            break;
        } else if ((fifo_status.level % data_size) != 0) {
            log_w("Unexpected size of data in FIFO: %d != %d", fifo_status.level, data_size);
            ret = HAL_ERROR;
            break;
        }

        while (fifo_status.level >= data_size) {
            ret = tsl2585_read_fifo(&hi2c2, data, data_size);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_fifo_status(&hi2c2, &fifo_status);
            if (ret != HAL_OK) { break; }

            counter++;
        }
        if (ret != HAL_OK) { break; }

        if (counter > 1) {
            log_w("Missed %d sensor read cycles", counter - 1);
        }

        if (fifo_data) {
            memset(fifo_data, 0, sizeof(tsl2585_fifo_data_t));
            fifo_data->als_data0 =
                (uint32_t)data[3] << 24
                | (uint32_t)data[2] << 16
                | (uint32_t)data[1] << 8
                | (uint32_t)data[0];

            fifo_data->als_status = data[4];
            fifo_data->als_status2 = data[5];
            fifo_data->als_status3 = data[6];
        }
    } while (0);

    return ret;
}
