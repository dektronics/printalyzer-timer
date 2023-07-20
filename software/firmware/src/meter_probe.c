#include "meter_probe.h"

#include <FreeRTOS.h>
#include <task.h>
#include <cmsis_os.h>

#include <math.h>

#define LOG_TAG "meter_probe"
#include <elog.h>

#include "board_config.h"
#include "tcs3472.h"
#include "m24c02.h"
#include "tsl2585.h"
#include "settings.h"
#include "util.h"

/**
 * Meter probe control event types.
 */
typedef enum {
    METER_PROBE_CONTROL_STOP = 0,
    METER_PROBE_CONTROL_START,
    METER_PROBE_CONTROL_SENSOR_ENABLE,
    METER_PROBE_CONTROL_SENSOR_DISABLE,
    METER_PROBE_CONTROL_SENSOR_SET_CONFIG,
    METER_PROBE_CONTROL_INTERRUPT
} meter_probe_control_event_type_t;

typedef struct {
    //tsl2591_gain_t gain;
    //tsl2591_time_t time;
} sensor_control_config_params_t;

typedef struct {
    uint32_t ticks;
    //uint32_t light_ticks;
    //uint32_t reading_count;
} sensor_control_interrupt_params_t;

/**
 * Meter probe control event data.
 */
typedef struct {
    meter_probe_control_event_type_t event_type;
    osStatus_t *result;
    union {
        sensor_control_config_params_t config;
        sensor_control_interrupt_params_t interrupt;
    };
} meter_probe_control_event_t;

/* Global I2C handle for the meter probe */
extern I2C_HandleTypeDef hi2c2;

static bool meter_probe_initialized = false;
static bool meter_probe_started = false;
static bool meter_probe_sensor_enabled = false;

/* Queue for meter probe control events */
static osMessageQueueId_t meter_probe_control_queue = NULL;
static const osMessageQueueAttr_t meter_probe_control_queue_attrs = {
    .name = "meter_probe_control_queue"
};

#if 0
/* Queue to hold the latest sensor reading */
static osMessageQueueId_t sensor_reading_queue = NULL;
static const osMessageQueueAttr_t sensor_reading_queue_attrs = {
    .name = "sensor_reading_queue"
};
#endif

/* Semaphore to synchronize sensor control calls */
static osSemaphoreId_t meter_probe_control_semaphore = NULL;
static const osSemaphoreAttr_t meter_probe_control_semaphore_attrs = {
    .name = "meter_probe_control_semaphore"
};

/* Meter probe control implementation functions */
static osStatus_t meter_probe_control_start();
static osStatus_t meter_probe_control_stop();
static osStatus_t meter_probe_control_sensor_enable();
static osStatus_t meter_probe_control_sensor_disable();
static osStatus_t meter_probe_control_sensor_set_config(const sensor_control_config_params_t *params);
static osStatus_t meter_probe_control_interrupt(const sensor_control_interrupt_params_t *params);

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

#if 0
    /* Create the one-element queue to hold the latest sensor reading */
    sensor_reading_queue = osMessageQueueNew(1, sizeof(sensor_reading_t), &sensor_reading_queue_attrs);
    if (!sensor_reading_queue) {
        log_e("Unable to create reading queue");
        return;
    }
#endif

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
                ret = meter_probe_control_sensor_enable();
                break;
            case METER_PROBE_CONTROL_SENSOR_DISABLE:
                ret = meter_probe_control_sensor_disable();
                break;
            case METER_PROBE_CONTROL_SENSOR_SET_CONFIG:
                ret = meter_probe_control_sensor_set_config(&control_event.config);
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
    uint8_t id_data[16];

    log_d("meter_probe_control_start");

    /* Apply power to the meter probe port */
    HAL_GPIO_WritePin(SENSOR_VBUS_GPIO_Port, SENSOR_VBUS_Pin, GPIO_PIN_RESET);

    /* Brief delay to ensure power has stabilized */
    osDelay(1);

    do {
        /* Read the identification area of the meter probe memory */
        ret = m24c02_read_id_page(&hi2c2, id_data);
        if (ret != HAL_OK) {
            break;
        }

        log_d("Memory ID: 0x%02X%02X%02X", id_data[0], id_data[1], id_data[2]);

        /* Verify the memory device identification code */
        if (id_data[0] != 0x20 && id_data[1] != 0xE0 && id_data[2] != 0x08) {
            log_w("Unexpected memory type");
            ret = HAL_ERROR;
            break;
        }

        /*
         * TODO: Read probe type info out of the rest of the ID page
         * Can't do this until the ID data format is standardized,
         * and a way to program it is implemented.
         */

        /*
         * Do a basic initialization of the sensor, which verifies that
         * the sensor is functional and responding to commands.
         * This routine should complete with the sensor in a disabled
         * state.
         */
        ret = tsl2585_init(&hi2c2);
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

    return osOK;
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

osStatus_t meter_probe_control_sensor_enable()
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("meter_probe_control_sensor_enable");
    //TODO

    do {
        /* Put the sensor into a known initial state */
        ret = tsl2585_enable(&hi2c2);
        if (ret != HAL_OK) {
            break;
        }

        //TODO Set the sensor's initial configuration

#if 0
        /* Clear out any old sensor readings */
        osMessageQueueReset(sensor_reading_queue);
#endif

        //TODO Enable sensor interrupts

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

osStatus_t meter_probe_control_sensor_set_config(const sensor_control_config_params_t *params)
{
    log_d("meter_probe_control_sensor_set_config");
    //TODO
    return osOK;
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
    log_d("meter_probe_control_interrupt");

    if (!meter_probe_initialized || !meter_probe_started || !meter_probe_sensor_enabled) {
        log_d("Unexpected meter probe interrupt");
    }
    //TODO

    return osOK;
}

//XXX -----------------------------------------------------------------------

meter_probe_result_t meter_probe_sensor_enable_old()
{
    meter_probe_result_t result = METER_READING_OK;
    HAL_StatusTypeDef ret = HAL_OK;

    if (!meter_probe_initialized) {
        return METER_READING_FAIL;
    }

    if (meter_probe_sensor_enabled) {
        return METER_READING_FAIL;
    }

    do {
        log_i("Initializing sensor");
        ret = tcs3472_init(&hi2c2);
        if (ret != HAL_OK) {
            log_e("Error initializing TCS3472: %d", ret);
            result = METER_READING_FAIL;
            break;
        }

        ret = tcs3472_enable(&hi2c2);
        if (ret != HAL_OK) {
            log_e("Error enabling TCS3472: %d", ret);
            result = METER_READING_FAIL;
            break;
        }

        ret = tcs3472_set_gain(&hi2c2, TCS3472_AGAIN_60X);
        if (ret != HAL_OK) {
            log_e("Error setting TCS3472 gain: %d", ret);
            result = METER_READING_FAIL;
            break;
        }

        ret = tcs3472_set_time(&hi2c2, TCS3472_ATIME_614MS);
        if (ret != HAL_OK) {
            log_e("Error setting TCS3472 time: %d", ret);
            result = METER_READING_FAIL;
            break;
        }

        bool valid = false;
        TickType_t start_ticks = xTaskGetTickCount();
        do {
            ret = tcs3472_get_status_valid(&hi2c2, &valid);
            if (ret != HAL_OK) {
                log_e("Error getting TCS3472 status: %d", ret);
                result = METER_READING_FAIL;
                break;
            }
            if ((xTaskGetTickCount() - start_ticks) / pdMS_TO_TICKS(2000)) {
                log_w("Timeout waiting for sensor valid status");
                result = METER_READING_TIMEOUT;
                break;
            }
        } while (!valid);

        log_i("Sensor initialized");
    } while (0);

    if (result != METER_READING_OK) {
        tcs3472_disable(&hi2c2);
        meter_probe_sensor_enabled = false;
    } else {
        meter_probe_sensor_enabled = true;
    }

    return result;
}

meter_probe_result_t meter_probe_measure_old(float *lux, uint32_t *cct)
{
    if (!meter_probe_sensor_enabled) {
        return METER_READING_FAIL;
    }

    meter_probe_result_t result = METER_READING_OK;
    HAL_StatusTypeDef ret = HAL_OK;
    do {
        tcs3472_channel_data_t ch_data;
        ret = tcs3472_get_full_channel_data(&hi2c2, &ch_data);
        if (ret != HAL_OK) {
            log_e("Error getting TCS3472 channel data: %d", ret);
            return METER_READING_FAIL;
        }

        if (tcs3472_is_sensor_saturated(&ch_data)) {
            log_w("Sensor is in saturation");
            //TODO implement an auto-gain algorithm
            result = METER_READING_HIGH;
            break;
        }

        if (ch_data.clear < 10) {
            log_w("Sensor reading is too low");
            result = METER_READING_LOW;
            break;
        }

        //TODO take more than one reading to ensure the data is stable

        float reading_lux = tcs3472_calculate_lux(&ch_data, settings_get_tcs3472_ga_factor());
        if (!isnormal(reading_lux) || reading_lux < 0.0001F) {
            log_w("Could not calculate lux from sensor reading");
            result = METER_READING_FAIL;
            break;
        } else if (reading_lux < 0.01F) {
            log_w("Lux calculation result is too low");
            result = METER_READING_LOW;
            break;
        }

        uint16_t reading_cct = tcs3472_calculate_color_temp(&ch_data);
        if (reading_cct == 0) {
            log_w("Could not calculate CCT from sensor reading");
            result = METER_READING_FAIL;
            break;
        }

        log_i("Sensor reading: lux=%f, CCT=%dK", reading_lux, reading_cct);

        if (lux) {
            *lux = reading_lux;
        }
        if (cct) {
            *cct = reading_cct;
        }

    } while (0);

    return result;
}

void meter_probe_sensor_disable_old()
{
    tcs3472_disable(&hi2c2);
    meter_probe_sensor_enabled = false;
}
