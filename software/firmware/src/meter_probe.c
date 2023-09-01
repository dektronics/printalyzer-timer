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
#include "util.h"

/**
 * Meter probe control event types.
 */
typedef enum {
    METER_PROBE_CONTROL_STOP = 0,
    METER_PROBE_CONTROL_START,
    METER_PROBE_CONTROL_SENSOR_ENABLE,
    METER_PROBE_CONTROL_SENSOR_ENABLE_SINGLE_SHOT,
    METER_PROBE_CONTROL_SENSOR_DISABLE,
    METER_PROBE_CONTROL_SENSOR_SET_CONFIG,
    METER_PROBE_CONTROL_SENSOR_ENABLE_AGC,
    METER_PROBE_CONTROL_SENSOR_DISABLE_AGC,
    METER_PROBE_CONTROL_SENSOR_TRIGGER_NEXT_READING,
    METER_PROBE_CONTROL_INTERRUPT
} meter_probe_control_event_type_t;

typedef struct {
    tsl2585_gain_t gain;
    uint16_t sample_time;
    uint16_t sample_count;
} sensor_control_config_params_t;

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
        sensor_control_config_params_t config;
        sensor_control_agc_params_t agc;
        sensor_control_interrupt_params_t interrupt;
    };
} meter_probe_control_event_t;

/**
 * Configuration state for the TSL2585 light sensor
 */
typedef struct {
    tsl2585_gain_t gain;
    uint16_t sample_time;
    uint16_t sample_count;
    uint8_t calibration_iteration;
    bool agc_enabled;
    uint16_t agc_sample_count;
    bool config_pending;
    bool agc_pending;
    bool sai_active;
    bool agc_disabled_reset_gain;
    bool discard_next_reading;
} tsl2585_config_t;

/* Global I2C handle for the meter probe */
extern I2C_HandleTypeDef hi2c2;

static bool meter_probe_initialized = false;
static bool meter_probe_started = false;
static bool meter_probe_sensor_enabled = false;
static bool meter_probe_sensor_single_shot = false;

static bool meter_probe_has_sensor_settings = false;
static meter_probe_settings_handle_t meter_probe_settings = {0};
static meter_probe_settings_tsl2585_t sensor_settings = {0};
static uint8_t sensor_device_id[3];
static tsl2585_config_t sensor_config = {0};

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

/* Meter probe control implementation functions */
static osStatus_t meter_probe_control_start();
static osStatus_t meter_probe_control_stop();
static osStatus_t meter_probe_control_sensor_enable(bool single_shot);
static osStatus_t meter_probe_control_sensor_disable();
static osStatus_t meter_probe_control_sensor_set_config(const sensor_control_config_params_t *params);
static osStatus_t meter_probe_control_sensor_enable_agc(const sensor_control_agc_params_t *params);
static osStatus_t meter_probe_control_sensor_disable_agc();
static osStatus_t meter_probe_control_sensor_trigger_next_reading();
static osStatus_t meter_probe_control_interrupt(const sensor_control_interrupt_params_t *params);

static HAL_StatusTypeDef meter_probe_sensor_read_als(meter_probe_sensor_reading_t *reading);

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
            case METER_PROBE_CONTROL_SENSOR_SET_CONFIG:
                ret = meter_probe_control_sensor_set_config(&control_event.config);
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
        ret = meter_probe_settings_init(&meter_probe_settings, &hi2c2);

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
            ret = meter_probe_settings_init(&meter_probe_settings, &hi2c2);
        }

        if (ret != HAL_OK) {
            break;
        }

        if (meter_probe_settings.type != METER_PROBE_TYPE_TSL2585) {
            log_w("Unknown meter probe type");
            ret = HAL_ERROR;
            break;
        }

        /* Read the settings for the current sensor type */
        ret = meter_probe_settings_get_tsl2585(&meter_probe_settings, &sensor_settings);
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
        if (ret != HAL_OK) {
            break;
        }

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

    /* Remove power from the meter probe port */
    HAL_GPIO_WritePin(SENSOR_VBUS_GPIO_Port, SENSOR_VBUS_Pin, GPIO_PIN_SET);

    /* Reset state variables */
    meter_probe_started = false;
    meter_probe_sensor_enabled = false;

    /* Clear the settings */
    memset(&meter_probe_settings, 0, sizeof(meter_probe_settings_handle_t));
    memset(&sensor_settings, 0, sizeof(meter_probe_settings_tsl2585_t));
    meter_probe_has_sensor_settings = false;

    return osOK;
}

osStatus_t meter_probe_get_device_info(meter_probe_device_info_t *info)
{
    if (!info) { return osErrorParameter; }
    if (!meter_probe_initialized || !meter_probe_started) { return osErrorResource; }

    memset(info, 0, sizeof(meter_probe_device_info_t));

    info->type = meter_probe_settings.type;
    info->revision = meter_probe_settings.probe_revision;
    info->serial = meter_probe_settings.probe_serial;
    memcpy(info->sensor_id, sensor_device_id, 3);
    memcpy(info->memory_id, meter_probe_settings.memory_id, 3);

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

    settings->type = meter_probe_settings.type;
    if (settings->type == METER_PROBE_TYPE_TSL2585 && meter_probe_has_sensor_settings) {
        memcpy(&settings->settings_tsl2585, &sensor_settings, sizeof(meter_probe_settings_tsl2585_t));
    }

    return osOK;
}

osStatus_t meter_probe_set_settings(const meter_probe_settings_t *settings)
{
    if (!settings) { return osErrorParameter; }
    if (!meter_probe_initialized || !meter_probe_started || meter_probe_sensor_enabled) { return osErrorResource; }

    if (settings->type != meter_probe_settings.type) {
        log_w("Invalid settings device type");
        return osErrorParameter;
    }

    if (settings->type == METER_PROBE_TYPE_TSL2585) {
        HAL_StatusTypeDef ret = meter_probe_settings_set_tsl2585(&meter_probe_settings, &settings->settings_tsl2585);
        return hal_to_os_status(ret);
    } else {
        log_w("Unsupported settings type");
        return osError;
    }
}

osStatus_t meter_probe_sensor_enable()
{
    if (!meter_probe_initialized || !meter_probe_started || meter_probe_sensor_enabled) { return osErrorResource; }

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
    if (!meter_probe_initialized || !meter_probe_started || meter_probe_sensor_enabled) { return osErrorResource; }

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

    do {
        /* Query the initial state of the sensor */
        if (!sensor_config.config_pending) {
            ret = tsl2585_get_mod_gain(&hi2c2, TSL2585_MOD0, TSL2585_STEP0, &sensor_config.gain);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_sample_time(&hi2c2, &sensor_config.sample_time);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_als_num_samples(&hi2c2, &sensor_config.sample_count);
            if (ret != HAL_OK) { break; }
        }

        if (!sensor_config.agc_pending) {
            ret = tsl2585_get_agc_num_samples(&hi2c2, &sensor_config.agc_sample_count);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_calibration_nth_iteration(&hi2c2, &sensor_config.calibration_iteration);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_agc_calibration(&hi2c2, &sensor_config.agc_enabled);
            if (ret != HAL_OK) { break; }
        }

        /* Put the sensor into a known initial state */
        ret = tsl2585_enable_modulators(&hi2c2, TSL2585_MOD0);
        if (ret != HAL_OK) { break; }

        ret = tsl2585_set_max_mod_gain(&hi2c2, TSL2585_GAIN_4096X);
        if (ret != HAL_OK) { break; }

        if (sensor_config.config_pending) {
            ret = tsl2585_set_mod_gain(&hi2c2, TSL2585_MOD0, TSL2585_STEP0, sensor_config.gain);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_sample_time(&hi2c2, sensor_config.sample_time);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_als_num_samples(&hi2c2, sensor_config.sample_count);
            if (ret != HAL_OK) { break; }

            sensor_config.config_pending = false;
        }

        if (sensor_config.agc_pending) {
            ret = tsl2585_set_agc_num_samples(&hi2c2, sensor_config.agc_sample_count);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_calibration_nth_iteration(&hi2c2, sensor_config.calibration_iteration);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_agc_calibration(&hi2c2, sensor_config.agc_enabled);
            if (ret != HAL_OK) { break; }

            sensor_config.agc_pending = false;
        }

        const float als_atime = tsl2585_integration_time_ms(sensor_config.sample_time, sensor_config.sample_count);
        const float agc_atime = tsl2585_integration_time_ms(sensor_config.sample_time, sensor_config.agc_sample_count);
        log_d("TSL2585 Initial State: Gain=%s, ALS_ATIME=%.2fms, AGC_ATIME=%.2fms",
            tsl2585_gain_str(sensor_config.gain), als_atime, agc_atime);

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

        /* Enable the sensor (ALS Enable and Power ON) */
        ret = tsl2585_enable(&hi2c2);
        if (ret != HAL_OK) {
            break;
        }

        sensor_config.sai_active = false;
        sensor_config.agc_disabled_reset_gain = false;
        sensor_config.discard_next_reading = false;
        meter_probe_sensor_single_shot = single_shot;
        meter_probe_sensor_enabled = true;
    } while (0);

    return hal_to_os_status(ret);
}

osStatus_t meter_probe_sensor_disable()
{
    if (!meter_probe_initialized || !meter_probe_started || !meter_probe_sensor_enabled) { return osErrorResource; }

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
        meter_probe_sensor_enabled = false;
    } while (0);

    return hal_to_os_status(ret);
}

osStatus_t meter_probe_sensor_set_config(tsl2585_gain_t gain, uint16_t sample_time, uint16_t sample_count)
{
    if (!meter_probe_initialized || !meter_probe_started) { return osErrorResource; }

    osStatus_t result = osOK;
    meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_SENSOR_SET_CONFIG,
        .result = &result,
        .config = {
            .gain = gain,
            .sample_time = sample_time,
            .sample_count = sample_count
        }
    };
    osMessageQueuePut(meter_probe_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(meter_probe_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_sensor_set_config(const sensor_control_config_params_t *params)
{
    HAL_StatusTypeDef ret = HAL_OK;
    log_d("meter_probe_control_sensor_set_config: %d, %d, %d", params->gain, params->sample_time, params->sample_count);

    if (meter_probe_sensor_enabled) {
        ret = tsl2585_set_mod_gain(&hi2c2, TSL2585_MOD0, TSL2585_STEP0, params->gain);
        if (ret == HAL_OK) {
            sensor_config.gain = params->gain;
        }

        ret = tsl2585_set_sample_time(&hi2c2, params->sample_time);
        if (ret == HAL_OK) {
            sensor_config.sample_time = params->sample_time;
        }

        ret = tsl2585_set_als_num_samples(&hi2c2, params->sample_count);
        if (ret == HAL_OK) {
            sensor_config.sample_count = params->sample_count;
        }

        osMessageQueueReset(sensor_reading_queue);
    } else {
        sensor_config.gain = params->gain;
        sensor_config.sample_time = params->sample_time;
        sensor_config.sample_count = params->sample_count;
        sensor_config.config_pending = true;
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

    if (meter_probe_sensor_enabled) {
        ret = tsl2585_set_agc_num_samples(&hi2c2, params->sample_count);
        if (ret == HAL_OK) {
            sensor_config.agc_sample_count = params->sample_count;
        }

        ret = tsl2585_set_calibration_nth_iteration(&hi2c2, 1);
        if (ret == HAL_OK) {
            sensor_config.calibration_iteration = 1;
        }

        ret = tsl2585_set_agc_calibration(&hi2c2, true);
        if (ret == HAL_OK) {
            sensor_config.agc_enabled = true;
        }
    } else {
        sensor_config.agc_enabled = true;
        sensor_config.agc_sample_count = params->sample_count;
        sensor_config.calibration_iteration = 1;
        sensor_config.agc_pending = true;
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

    if (meter_probe_sensor_enabled) {
        do {
            ret = tsl2585_set_agc_calibration(&hi2c2, false);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_calibration_nth_iteration(&hi2c2, 0);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_agc_num_samples(&hi2c2, 0);
            if (ret != HAL_OK) { break; }
        } while (0);
        if (ret == HAL_OK) {
            sensor_config.agc_enabled = false;
            sensor_config.agc_disabled_reset_gain = true;
            sensor_config.discard_next_reading = true;
        }
    } else {
        sensor_config.agc_enabled = false;
        sensor_config.calibration_iteration = 0;
        sensor_config.agc_pending = true;
    }

    return hal_to_os_status(ret);
}

osStatus_t meter_probe_sensor_trigger_next_reading()
{
    if (!meter_probe_initialized || !meter_probe_started || !meter_probe_sensor_enabled) { return osErrorResource; }

    if (!meter_probe_sensor_single_shot) {
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

    if (!sensor_config.sai_active) {
        log_e("Integration cycle in progress");
        return osErrorResource;
    }

    ret = tsl2585_clear_sleep_after_interrupt(&hi2c2);
    if (ret == HAL_OK) {
        sensor_config.sai_active = false;
    }

    return hal_to_os_status(ret);
}

osStatus_t meter_probe_sensor_clear_last_reading()
{
    return osMessageQueueReset(sensor_reading_queue);
}

osStatus_t meter_probe_sensor_get_next_reading(meter_probe_sensor_reading_t *reading, uint32_t timeout)
{
    if (!meter_probe_initialized || !meter_probe_started || !meter_probe_sensor_enabled) { return osErrorResource; }

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
            } else if (reading_lux < 0.01F) {
                log_w("Lux calculation result is too low");
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

uint32_t meter_probe_scaled_result(const meter_probe_sensor_reading_t *sensor_reading)
{
    if (!sensor_reading) { return 0; }
    return (uint32_t)sensor_reading->raw_result * (uint32_t)sensor_reading->scale;
}

float meter_probe_basic_result(const meter_probe_sensor_reading_t *sensor_reading)
{
    if (!sensor_reading) { return NAN; }

    const uint32_t scaled_value = meter_probe_scaled_result(sensor_reading);
    const float atime = tsl2585_integration_time_ms(sensor_reading->sample_time, sensor_reading->sample_count);
    const float gain_val = tsl2585_gain_value(sensor_reading->gain);

    if (!is_valid_number(atime) || !is_valid_number(gain_val)) { return NAN; }

    return (float)scaled_value / (atime * gain_val);
}

float meter_probe_lux_result(const meter_probe_sensor_reading_t *sensor_reading)
{
    if (!sensor_reading) { return NAN; }
    if (sensor_reading->gain >= TSL2585_GAIN_MAX) { return NAN; }
    if (!meter_probe_has_sensor_settings) { return NAN; }

    const float slope = sensor_settings.gain_cal[sensor_reading->gain].slope;
    const float offset = sensor_settings.gain_cal[sensor_reading->gain].offset;
    if (!is_valid_number(slope) || !is_valid_number(offset)) { return NAN; }

    const float basic_value = meter_probe_basic_result(sensor_reading);
    if (!is_valid_number(basic_value)) { return NAN; }

    return (basic_value * slope) + offset;
}

void meter_probe_int_handler()
{
    if (!meter_probe_initialized || !meter_probe_started || !meter_probe_sensor_enabled) { return; }

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

    if (!meter_probe_initialized || !meter_probe_started || !meter_probe_sensor_enabled) {
        log_d("Unexpected meter probe interrupt");
    }

    do {
        /* Get the interrupt status */
        ret = tsl2585_get_status(&hi2c2, &status);
        if (ret != HAL_OK) { break; }

#if 0
        log_d("MINT=%d, AINT=%d, FINT=%d, SINT=%d",
            (status & TSL2585_STATUS_MINT) != 0,
            (status & TSL2585_STATUS_AINT) != 0,
            (status & TSL2585_STATUS_FINT) != 0,
            (status & TSL2585_STATUS_SINT) != 0);
#endif

        if ((status & TSL2585_STATUS_AINT) != 0) {
            uint8_t status2 = 0;
            ret = tsl2585_get_status2(&hi2c2, &status2);
            if (ret != HAL_OK) { break; }

#if 0
            log_d("STATUS2=0x%02X", status2);
#endif

            if ((status2 & TSL2585_STATUS2_ALS_DATA_VALID) != 0) {
                if ((status2 & TSL2585_STATUS2_MOD_ANALOG_SATURATION0) != 0) {
                    log_d("TSL2585: [analog saturation]");
                    reading.raw_result = UINT16_MAX;
                    reading.result_status = METER_SENSOR_RESULT_SATURATED_ANALOG;

                } else if ((status2 & TSL2585_STATUS2_ALS_DIGITAL_SATURATION) != 0) {
                    log_d("TSL2585: [digital saturation]");
                    reading.raw_result = UINT16_MAX;
                    reading.result_status = METER_SENSOR_RESULT_SATURATED_DIGITAL;
                } else {
                    /* Get the sensor reading */
                    ret = meter_probe_sensor_read_als(&reading);
                    if (ret != HAL_OK) { break; }
                }

                /* If AGC is enabled, then update the configured gain value */
                if (sensor_config.agc_enabled) {
                    ret = tsl2585_get_mod_gain(&hi2c2, TSL2585_MOD0, TSL2585_STEP0, &sensor_config.gain);
                    if (ret != HAL_OK) { break; }
                }

                if (!sensor_config.discard_next_reading) {
                    /* Fill out other reading fields */
                    reading.gain = sensor_config.gain;
                    reading.sample_time = sensor_config.sample_time;
                    reading.sample_count = sensor_config.sample_count;
                    reading.ticks = params->ticks;

                    has_reading = true;
                } else {
                    sensor_config.discard_next_reading = false;
                }

                /*
                 * If AGC was just disabled, then reset the gain to its last
                 * known value and ignore the reading. This is necessary because
                 * disabling AGC on its own seems to reset the gain to a low
                 * default, and attempting to set it immediately after setting
                 * the registers to disable AGC does not seem to take.
                 */
                if (sensor_config.agc_disabled_reset_gain) {
                    ret = tsl2585_set_mod_gain(&hi2c2, TSL2585_MOD0, TSL2585_STEP0, sensor_config.gain);
                    if (ret != HAL_OK) { break; }
                    sensor_config.agc_disabled_reset_gain = false;
                    sensor_config.discard_next_reading = true;
                }
            }
        }

        /* Clear the interrupt status */
        ret = tsl2585_set_status(&hi2c2, status);
        if (ret != HAL_OK) { break; }

        /* Check single-shot specific state */
        if (meter_probe_sensor_single_shot) {
            uint8_t status4 = 0;
            ret = tsl2585_get_status4(&hi2c2, &status4);
            if (ret != HAL_OK) { break; }
            if ((status4 & TSL2585_STATUS4_SAI_ACTIVE) != 0) {
#if 0
                log_d("Sleep after interrupt");
#endif
                sensor_config.sai_active = true;
            }
        }
    } while (0);

    vTaskPrioritySet(current_task_handle, current_task_priority);

    if (has_reading) {
        QueueHandle_t queue = (QueueHandle_t)sensor_reading_queue;
        xQueueOverwrite(queue, &reading);
    }

    return hal_to_os_status(ret);
}

HAL_StatusTypeDef meter_probe_sensor_read_als(meter_probe_sensor_reading_t *reading)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t als_status = 0;
    uint16_t als_data0 = 0;
    uint8_t asat = 0;
    uint8_t scale = 0;
    uint16_t scale_value = 1;

    do {
        ret = tsl2585_get_als_status(&hi2c2, &als_status);
        if (ret != HAL_OK) { break; }

        asat = (als_status & TSL2585_ALS_DATA0_ANALOG_SATURATION_STATUS) != 0;

        ret = tsl2585_get_als_data0(&hi2c2, &als_data0);
        if (ret != HAL_OK) { break; }

        ret = tsl2585_get_als_scale(&hi2c2, &scale);
        if (ret != HAL_OK) { break; }

        if ((als_status & TSL2585_ALS_DATA0_SCALED_STATUS) == 0) {
            /* Need to scale value: 2^(ALS_SCALED) */
            uint16_t base = 2;
            for (;;) {
                if (scale & 1) {
                    scale_value *= base;
                }
                scale >>= 1;
                if (!scale) { break; }
                base *= base;
            }
        }

        if (asat) {
#if 0
            log_d("TSL2585: [saturated]");
#endif
            reading->result_status = METER_SENSOR_RESULT_SATURATED_ANALOG;
        } else {
#if 0
            log_d("TSL2585: %d", raw_result);
#endif
            reading->result_status = METER_SENSOR_RESULT_VALID;
        }

        reading->raw_result = als_data0;
        reading->scale = scale_value;
    } while (0);

    return ret;
}
