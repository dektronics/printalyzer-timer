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
#include "usb_host.h"
#include "usb_ft260.h"
#include "i2c_interface.h"
#include "util.h"

/*
 * Size of an ALS reading in the TSL2585 FIFO, when using our
 * preferred settings: 3B status, 4B ALS data
 */
#define FIFO_ALS_ENTRY_SIZE 7

typedef enum {
    METER_PROBE_STATE_NONE = 0,
    METER_PROBE_STATE_INIT,
    METER_PROBE_STATE_ATTACHED,
    METER_PROBE_STATE_STARTED,
    METER_PROBE_STATE_RUNNING
} meter_probe_state_t;

/**
 * Meter probe control event types.
 */
typedef enum {
    METER_PROBE_CONTROL_STOP = 0,
    METER_PROBE_CONTROL_START,
    METER_PROBE_CONTROL_ATTACH,
    METER_PROBE_CONTROL_DETACH,
    METER_PROBE_CONTROL_SENSOR_ENABLE,
    METER_PROBE_CONTROL_SENSOR_DISABLE,
    METER_PROBE_CONTROL_SENSOR_SET_GAIN,
    METER_PROBE_CONTROL_SENSOR_SET_INTEGRATION,
    METER_PROBE_CONTROL_SENSOR_SET_MOD_CALIBRATION,
    METER_PROBE_CONTROL_SENSOR_ENABLE_AGC,
    METER_PROBE_CONTROL_SENSOR_DISABLE_AGC,
    METER_PROBE_CONTROL_SENSOR_TRIGGER_NEXT_READING,
    METER_PROBE_CONTROL_INTERRUPT
} meter_probe_control_event_type_t;

typedef enum {
    METER_PROBE_START_NONE = 0,
    METER_PROBE_START_NORMAL,
    METER_PROBE_START_FAST_MODE,
    METER_PROBE_START_SINGLE_SHOT
} sensor_control_start_mode_t;

typedef struct {
    tsl2585_gain_t gain;
    tsl2585_modulator_t mod;
} sensor_control_gain_params_t;

typedef struct {
    uint16_t sample_time;
    uint16_t sample_count;
} sensor_control_integration_params_t;

typedef struct {
    uint8_t nth_iteration_value;
} sensor_control_mod_cal_params_t;

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
        sensor_control_start_mode_t start_mode;
        sensor_control_gain_params_t gain;
        sensor_control_integration_params_t integration;
        sensor_control_mod_cal_params_t mod_calibration;
        sensor_control_agc_params_t agc;
        sensor_control_interrupt_params_t interrupt;
    };
} meter_probe_control_event_t;

/**
 * Configuration state for the TSL2585 light sensor
 */
typedef struct {
    sensor_control_start_mode_t start_mode;
    bool running;
    bool single_shot;
    bool fast_mode;
    uint8_t uv_calibration;
    tsl2585_gain_t gain[3];
    uint16_t sample_time;
    uint16_t sample_count;
    uint8_t nth_iteration_value;
    bool agc_enabled;
    uint16_t agc_sample_count;
    bool gain_pending;
    bool integration_pending;
    bool mod_calibration_pending;
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
} tsl2585_fifo_data_t;

/* Global handles for the meter probe */
static ft260_device_t *device_handle = NULL;
static i2c_handle_t *hi2c = NULL;

static meter_probe_state_t meter_probe_state = METER_PROBE_STATE_NONE;

static bool meter_probe_has_sensor_settings = false;
static meter_probe_settings_handle_t probe_settings_handle = {0};
static meter_probe_settings_tsl2585_t sensor_settings = {0};
static uint8_t sensor_device_id[3];
static tsl2585_state_t sensor_state = {0};
static bool sensor_osc_calibration_enabled = false;
static uint32_t last_aint_ticks = 0;
static volatile uint8_t sensor_osc_sync_count = 0;

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

/* TSL2585: Only enable Photopic photodiodes on modulator 0 */
static const tsl2585_modulator_t sensor_tsl2585_phd_mod_vis[] = {
    0, TSL2585_MOD0, 0, 0, 0, TSL2585_MOD0
};

/* TSL2521: Only enable Clear photodiodes on modulator 0 */
static const tsl2585_modulator_t sensor_tsl2521_phd_mod_vis[] = {
    0, TSL2585_MOD0, TSL2585_MOD0, TSL2585_MOD0, TSL2585_MOD0, 0
};

/* Meter probe interface functions only called by internally handled events */
static osStatus_t meter_probe_attach();
static osStatus_t meter_probe_detach();

/* Meter probe control implementation functions */
static osStatus_t meter_probe_control_start();
static osStatus_t meter_probe_control_stop();
static osStatus_t meter_probe_control_attach();
static osStatus_t meter_probe_control_detach();
static osStatus_t meter_probe_sensor_enable_impl(sensor_control_start_mode_t start_mode);
static osStatus_t meter_probe_control_sensor_enable(sensor_control_start_mode_t start_mode);
static osStatus_t meter_probe_control_sensor_disable();
static osStatus_t meter_probe_control_sensor_set_gain(const sensor_control_gain_params_t *params);
static osStatus_t meter_probe_control_sensor_integration(const sensor_control_integration_params_t *params);
static osStatus_t meter_probe_control_sensor_set_mod_calibration(const sensor_control_mod_cal_params_t *params);
static osStatus_t meter_probe_control_sensor_enable_agc(const sensor_control_agc_params_t *params);
static osStatus_t meter_probe_control_sensor_disable_agc();
static osStatus_t meter_probe_control_sensor_trigger_next_reading();
static osStatus_t meter_probe_control_interrupt(const sensor_control_interrupt_params_t *params);

static void usb_meter_probe_event_callback(ft260_device_t *device, ft260_device_event_t event_type, uint32_t ticks);
static void meter_probe_int_handler(uint32_t ticks);

static HAL_StatusTypeDef sensor_osc_calibration();

static HAL_StatusTypeDef sensor_control_read_fifo(tsl2585_fifo_data_t *fifo_data, bool *overflow);
static HAL_StatusTypeDef sensor_control_read_fifo_fast_mode(tsl2585_fifo_data_t *fifo_data, bool *overflow, uint32_t ticks);

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

    /* Get the persistent device handles used to talk to a connected meter probe */
    device_handle = usbh_ft260_get_device(FT260_METER_PROBE);
    if (!device_handle) {
        log_e("Unable to get meter probe interface");
        return;
    }
    hi2c = usbh_ft260_get_device_i2c(device_handle);

    /* Set the meter probe event callback */
    usbh_ft260_set_device_callback(device_handle, usb_meter_probe_event_callback);

    meter_probe_state = METER_PROBE_STATE_INIT;

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
            case METER_PROBE_CONTROL_ATTACH:
                ret = meter_probe_control_attach();
                break;
            case METER_PROBE_CONTROL_DETACH:
                ret = meter_probe_control_detach();
                break;
            case METER_PROBE_CONTROL_SENSOR_ENABLE:
                ret = meter_probe_control_sensor_enable(control_event.start_mode);
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
            case METER_PROBE_CONTROL_SENSOR_SET_MOD_CALIBRATION:
                ret = meter_probe_control_sensor_set_mod_calibration(&control_event.mod_calibration);
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
            if (control_event.event_type != METER_PROBE_CONTROL_INTERRUPT
                && control_event.event_type != METER_PROBE_CONTROL_ATTACH
                && control_event.event_type != METER_PROBE_CONTROL_DETACH) {
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

void usb_meter_probe_event_callback(ft260_device_t *device, ft260_device_event_t event_type, uint32_t ticks)
{
    if (device_handle != device) { return; }

    switch (event_type) {
    case FT260_EVENT_ATTACH:
        log_d("Meter probe attached");
        meter_probe_attach();
        break;
    case FT260_EVENT_DETACH:
        log_d("Meter probe detached");
        meter_probe_detach();
        break;
    case FT260_EVENT_INTERRUPT:
        meter_probe_int_handler(ticks);
        break;
    case FT260_EVENT_BUTTON_DOWN:
        keypad_inject_raw_event(KEYPAD_METER_PROBE, true, ticks);
        break;
    case FT260_EVENT_BUTTON_UP:
        keypad_inject_raw_event(KEYPAD_METER_PROBE, false, ticks);
        break;
    default:
        break;
    }
}

bool meter_probe_is_attached()
{
    return meter_probe_state >= METER_PROBE_STATE_ATTACHED;
}

bool meter_probe_is_started()
{
    return meter_probe_state == METER_PROBE_STATE_STARTED || meter_probe_state == METER_PROBE_STATE_RUNNING;
}

osStatus_t meter_probe_start()
{
    if (meter_probe_state != METER_PROBE_STATE_ATTACHED) { return osErrorResource; }

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

    do {
        if (!usb_meter_probe_is_attached()) {
            log_w("Meter probe not attached");
            ret = HAL_ERROR;
            break;
        }

        /* Populate values that come from the USB descriptor */
        usbh_ft260_get_device_serial_number(device_handle, probe_settings_handle.id.probe_serial);

        /* Set the FT260 I2C clock speed to the default of 400kHz */
        if (usbh_ft260_set_i2c_clock_speed(device_handle, 400) != osOK) {
            log_w("Unable to set I2C clock speed");
            ret = HAL_ERROR;
            break;
        }

        /* Read the meter probe's settings memory */
        ret = meter_probe_settings_init(&probe_settings_handle, hi2c);
        if (ret != HAL_OK) {
            break;
        }

        log_i("Meter probe: type=%s, rev=[%d,%d], serial=%s",
            meter_probe_type_str(probe_settings_handle.id.probe_type),
            probe_settings_handle.id.probe_rev_major,
            probe_settings_handle.id.probe_rev_minor,
            probe_settings_handle.id.probe_serial);

        /*
         * The TSL2585 and TSL2521 are almost identical, except for the
         * modulator count and photodiode configuration. Therefore, both
         * use the same driver with only some minor differences in usage.
         */
        if (probe_settings_handle.id.probe_type != METER_PROBE_TYPE_TSL2585
            && probe_settings_handle.id.probe_type != METER_PROBE_TYPE_TSL2521) {
            log_w("Unknown meter probe type: %d", probe_settings_handle.id.probe_type);
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
        ret = tsl2585_init(hi2c, sensor_device_id);
        if (ret != HAL_OK) { break; }

        sensor_state.uv_calibration = 0;
        if (probe_settings_handle.id.probe_type == METER_PROBE_TYPE_TSL2585) {
            ret = tsl2585_get_uv_calibration(hi2c, &sensor_state.uv_calibration);
            if (ret != HAL_OK) { break; }
            log_d("UV calibration value: %d", sensor_state.uv_calibration);
        }

        meter_probe_state = METER_PROBE_STATE_STARTED;
    } while (0);

    if (meter_probe_state != METER_PROBE_STATE_STARTED) {
        log_w("Meter probe initialization failed");
        meter_probe_control_stop();
    }

    return hal_to_os_status(ret);
}

osStatus_t meter_probe_stop()
{
    if (meter_probe_state < METER_PROBE_STATE_ATTACHED) { return osErrorResource; }

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

    /* Reset state variables */
    if (meter_probe_state > METER_PROBE_STATE_ATTACHED) {
        meter_probe_state = METER_PROBE_STATE_ATTACHED;
    } else {
        meter_probe_state = METER_PROBE_STATE_INIT;
    }

    /* Clear the settings */
    memset(&probe_settings_handle, 0, sizeof(meter_probe_settings_handle_t));
    memset(&sensor_settings, 0, sizeof(meter_probe_settings_tsl2585_t));
    memset(&sensor_state, 0, sizeof(tsl2585_state_t));
    meter_probe_has_sensor_settings = false;

    return osOK;
}

osStatus_t meter_probe_attach()
{
    osStatus_t result = osOK;
    meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_ATTACH,
        .result = &result
    };
    osMessageQueuePut(meter_probe_control_queue, &control_event, 0, portMAX_DELAY);
    /* This command does not block on being handled by the task */
    return result;
}

osStatus_t meter_probe_control_attach()
{
    osStatus_t result = osOK;
    log_d("meter_probe_control_attach");

    if (meter_probe_state == METER_PROBE_STATE_INIT) {
        meter_probe_state = METER_PROBE_STATE_ATTACHED;
        result = osOK;
    } else if (meter_probe_state == METER_PROBE_STATE_STARTED || meter_probe_state == METER_PROBE_STATE_RUNNING) {
        /* Not an expected case, so handle it like a glitch that should stop the meter probe */
        log_w("Meter probe attach while running");

        /* Try to disable the sensor, but ignore any errors */
        if (meter_probe_state == METER_PROBE_STATE_RUNNING) {
            meter_probe_control_sensor_disable();
        }

        /* Set the meter probe to the stopped state */
        result = meter_probe_control_stop();

        meter_probe_state = METER_PROBE_STATE_ATTACHED;
    } else {
        /* Should not get here */
        result = osErrorResource;
    }

    return result;
}

osStatus_t meter_probe_detach()
{
    osStatus_t result = osOK;
    meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_DETACH,
        .result = &result
    };
    osMessageQueuePut(meter_probe_control_queue, &control_event, 0, portMAX_DELAY);
    /* This command does not block on being handled by the task */
    return result;
}

osStatus_t meter_probe_control_detach()
{
    osStatus_t result = osOK;
    log_d("meter_probe_control_detach");

    if (meter_probe_state == METER_PROBE_STATE_STARTED || meter_probe_state == METER_PROBE_STATE_RUNNING) {
        /* The meter probe went away, so clear out the existing state and return to init */
        result = meter_probe_control_stop();
        meter_probe_state = METER_PROBE_STATE_INIT;
    } else if (meter_probe_state == METER_PROBE_STATE_ATTACHED) {
        meter_probe_state = METER_PROBE_STATE_INIT;
    }

    /* A detach in any other state is not relevant */

    return result;
}

osStatus_t meter_probe_get_device_info(meter_probe_device_info_t *info)
{
    if (!info) { return osErrorParameter; }
    if (meter_probe_state < METER_PROBE_STATE_STARTED) { return osErrorResource; }

    memset(info, 0, sizeof(meter_probe_device_info_t));

    memcpy(&info->probe_id, &probe_settings_handle.id, sizeof(meter_probe_id_t));
    memcpy(info->sensor_id, sensor_device_id, 3);

    return osOK;
}

bool meter_probe_has_settings()
{
    if (meter_probe_state < METER_PROBE_STATE_STARTED) { return osErrorResource; }
    return meter_probe_has_sensor_settings;
}

osStatus_t meter_probe_get_settings(meter_probe_settings_t *settings)
{
    if (!settings) { return osErrorParameter; }
    if (meter_probe_state < METER_PROBE_STATE_STARTED) { return osErrorResource; }

    memset(settings, 0, sizeof(meter_probe_settings_t));

    settings->type = probe_settings_handle.id.probe_type;
    if ((settings->type == METER_PROBE_TYPE_TSL2585 || settings->type == METER_PROBE_TYPE_TSL2521)
        && meter_probe_has_sensor_settings) {
        memcpy(&settings->settings_tsl2585, &sensor_settings, sizeof(meter_probe_settings_tsl2585_t));
    }

    return osOK;
}

osStatus_t meter_probe_set_settings(const meter_probe_settings_t *settings)
{
    if (!settings) { return osErrorParameter; }
    if (meter_probe_state < METER_PROBE_STATE_STARTED || sensor_state.running) { return osErrorResource; }

    if (settings->type != probe_settings_handle.id.probe_type) {
        log_w("Invalid settings device type");
        return osErrorParameter;
    }

    if (settings->type == METER_PROBE_TYPE_TSL2585 || settings->type == METER_PROBE_TYPE_TSL2521) {
        HAL_StatusTypeDef ret = meter_probe_settings_set_tsl2585(&probe_settings_handle, &settings->settings_tsl2585);
        return hal_to_os_status(ret);
    } else {
        log_w("Unsupported settings type");
        return osError;
    }
}

osStatus_t meter_probe_sensor_enable_osc_calibration()
{
    //FIXME Might no longer support OSC calibration
    return osOK;
#if 0
    if (!meter_probe_initialized || !meter_probe_started || sensor_state.running) { return osErrorResource; }

    if (probe_settings_handle.id.probe_type == METER_PROBE_TYPE_TSL2585
        || probe_settings_handle.id.probe_type == METER_PROBE_TYPE_TSL2521) {
        sensor_osc_calibration_enabled = true;
        return osOK;
    } else {
        return osErrorParameter;
    }
#endif
}

osStatus_t meter_probe_sensor_disable_osc_calibration()
{
    //FIXME Might no longer support OSC calibration
    return osOK;
#if 0
    if (!meter_probe_initialized || !meter_probe_started || sensor_state.running) { return osErrorResource; }

    if (probe_settings_handle.id.probe_type == METER_PROBE_TYPE_TSL2585
        || probe_settings_handle.id.probe_type == METER_PROBE_TYPE_TSL2521) {
        sensor_osc_calibration_enabled = false;
        return osOK;
    } else {
        return osErrorParameter;
    }
#endif
}

osStatus_t meter_probe_sensor_enable()
{
    return meter_probe_sensor_enable_impl(METER_PROBE_START_NORMAL);
}

osStatus_t meter_probe_sensor_enable_fast_mode()
{
    return meter_probe_sensor_enable_impl(METER_PROBE_START_FAST_MODE);
}

osStatus_t meter_probe_sensor_enable_single_shot()
{
    return meter_probe_sensor_enable_impl(METER_PROBE_START_SINGLE_SHOT);
}

osStatus_t meter_probe_sensor_enable_impl(sensor_control_start_mode_t start_mode)
{
    if (meter_probe_state < METER_PROBE_STATE_STARTED || sensor_state.running) { return osErrorResource; }

    osStatus_t result = osOK;
    meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_SENSOR_ENABLE,
        .result = &result,
        .start_mode = start_mode
    };
    osMessageQueuePut(meter_probe_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(meter_probe_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_sensor_enable(sensor_control_start_mode_t start_mode)
{
    HAL_StatusTypeDef ret = HAL_OK;

    bool fast_mode;
    bool single_shot;
    switch (start_mode) {
    case METER_PROBE_START_FAST_MODE:
        fast_mode = true;
        single_shot = false;
        sensor_state.start_mode = METER_PROBE_START_FAST_MODE;
        log_d("meter_probe_control_sensor_enable: fast_mode");
        break;
    case METER_PROBE_START_SINGLE_SHOT:
        fast_mode = false;
        single_shot = true;
        sensor_state.start_mode = METER_PROBE_START_SINGLE_SHOT;
        log_d("meter_probe_control_sensor_enable: single_shot");
        break;
    case METER_PROBE_START_NORMAL:
    default:
        fast_mode = false;
        single_shot = false;
        sensor_state.start_mode = METER_PROBE_START_NORMAL;
        log_d("meter_probe_control_sensor_enable: continuous");
        break;
    }

    do {
        /* Query the initial state of the sensor */
        if (!sensor_state.gain_pending) {
            ret = tsl2585_get_mod_gain(hi2c, TSL2585_MOD0, TSL2585_STEP0, &sensor_state.gain[0]);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_mod_gain(hi2c, TSL2585_MOD1, TSL2585_STEP0, &sensor_state.gain[1]);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_mod_gain(hi2c, TSL2585_MOD2, TSL2585_STEP0, &sensor_state.gain[2]);
            if (ret != HAL_OK) { break; }
        }

        if (!sensor_state.integration_pending) {
            ret = tsl2585_get_sample_time(hi2c, &sensor_state.sample_time);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_als_num_samples(hi2c, &sensor_state.sample_count);
            if (ret != HAL_OK) { break; }
        }

        if (!sensor_state.mod_calibration_pending) {
            ret = tsl2585_get_calibration_nth_iteration(hi2c, &sensor_state.nth_iteration_value);
            if (ret != HAL_OK) { break; }
        }

        if (!sensor_state.agc_pending) {
            ret = tsl2585_get_agc_num_samples(hi2c, &sensor_state.agc_sample_count);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_agc_calibration(hi2c, &sensor_state.agc_enabled);
            if (ret != HAL_OK) { break; }
        }

        /* Put the sensor into a known initial state */

        /* Enable writing of ALS status to the FIFO */
        ret = tsl2585_set_fifo_als_status_write_enable(hi2c, true);
        if (ret != HAL_OK) { break; }

        /* Enable writing of results to the FIFO */
        ret = tsl2585_set_fifo_data_write_enable(hi2c, TSL2585_MOD0, true);
        if (ret != HAL_OK) { break; }
        ret = tsl2585_set_fifo_data_write_enable(hi2c, TSL2585_MOD1, false);
        if (ret != HAL_OK) { break; }
        ret = tsl2585_set_fifo_data_write_enable(hi2c, TSL2585_MOD2, false);
        if (ret != HAL_OK) { break; }

        /* Set FIFO data format to 32-bits */
        ret = tsl2585_set_fifo_als_data_format(hi2c, TSL2585_ALS_FIFO_32BIT);
        if (ret != HAL_OK) { break; }

        /* Set MSB position for full 26-bit result */
        ret = tsl2585_set_als_msb_position(hi2c, 6);
        if (ret != HAL_OK) { break; }

        /* Make sure residuals are enabled */
        ret = tsl2585_set_mod_residual_enable(hi2c, TSL2585_MOD0, TSL2585_STEPS_ALL);
        if (ret != HAL_OK) { break; }
        ret = tsl2585_set_mod_residual_enable(hi2c, TSL2585_MOD1, TSL2585_STEPS_ALL);
        if (ret != HAL_OK) { break; }
        ret = tsl2585_set_mod_residual_enable(hi2c, TSL2585_MOD2, TSL2585_STEPS_ALL);
        if (ret != HAL_OK) { break; }

        /* Select alternate gain table, which caps gain at 256x but gives us more residual bits */
        ret = tsl2585_set_mod_gain_table_select(hi2c, true);
        if (ret != HAL_OK) { break; }

        /* Set maximum gain to 256x per app note on residual measurement */
        ret = tsl2585_set_max_mod_gain(hi2c, TSL2585_GAIN_256X);
        if (ret != HAL_OK) { break; }

        /* Enable modulator */
        ret = tsl2585_enable_modulators(hi2c, TSL2585_MOD0);
        if (ret != HAL_OK) { break; }

        /* Set modulator calibration rate */
        if (sensor_state.mod_calibration_pending) {
            ret = tsl2585_set_calibration_nth_iteration(hi2c, sensor_state.nth_iteration_value);
            if (ret != HAL_OK) { break; }

            sensor_state.mod_calibration_pending = false;
        }

        if (probe_settings_handle.id.probe_type == METER_PROBE_TYPE_TSL2585) {
            /* Configure photodiodes for Photopic measurement only */
            ret = tsl2585_set_mod_photodiode_smux(hi2c, TSL2585_STEP0, sensor_tsl2585_phd_mod_vis);
            if (ret != HAL_OK) { break; }
        } else if (probe_settings_handle.id.probe_type == METER_PROBE_TYPE_TSL2521) {
            /* Configure photodiodes for Clear measurement only */
            ret = tsl2585_set_mod_photodiode_smux(hi2c, TSL2585_STEP0, sensor_tsl2521_phd_mod_vis);
            if (ret != HAL_OK) { break; }
        }

        if (sensor_state.gain_pending) {
            ret = tsl2585_set_mod_gain(hi2c, TSL2585_MOD0, TSL2585_STEP0, sensor_state.gain[0]);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_mod_gain(hi2c, TSL2585_MOD1, TSL2585_STEP0, sensor_state.gain[1]);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_mod_gain(hi2c, TSL2585_MOD2, TSL2585_STEP0, sensor_state.gain[2]);
            if (ret != HAL_OK) { break; }

            sensor_state.gain_pending = false;
        }

        if (sensor_state.integration_pending) {
            ret = tsl2585_set_sample_time(hi2c, sensor_state.sample_time);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_als_num_samples(hi2c, sensor_state.sample_count);
            if (ret != HAL_OK) { break; }

            sensor_state.integration_pending = false;
        }

        if (sensor_state.agc_pending) {
            ret = tsl2585_set_agc_num_samples(hi2c, sensor_state.agc_sample_count);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_agc_calibration(hi2c, sensor_state.agc_enabled);
            if (ret != HAL_OK) { break; }

            sensor_state.agc_pending = false;
        }

        const float als_atime = tsl2585_integration_time_ms(sensor_state.sample_time, sensor_state.sample_count);
        const float agc_atime = tsl2585_integration_time_ms(sensor_state.sample_time, sensor_state.agc_sample_count);
        log_d("TSL2585 Initial State: Gain=%s, ALS_ATIME=%.2fms, AGC_ATIME=%.2fms",
            tsl2585_gain_str(sensor_state.gain[0]), als_atime, agc_atime);

        /* Clear out any old sensor readings */
        osMessageQueueReset(sensor_reading_queue);

        ret = tsl2585_set_single_shot_mode(hi2c, single_shot);
        if (ret != HAL_OK) { break; }

        ret = tsl2585_set_sleep_after_interrupt(hi2c, single_shot);
        if (ret != HAL_OK) { break; }


        if (fast_mode) {
            /* Set ALS interrupt persistence to track an unused modulator to avoid unwanted interrupts */
            ret = tsl2585_set_als_interrupt_persistence(hi2c, TSL2585_MOD1, 1);
            if (ret != HAL_OK) { break; }

            /* Set FIFO threshold to a group of readings */
            ret = tsl2585_set_fifo_threshold(hi2c, (FIFO_ALS_ENTRY_SIZE * MAX_ALS_COUNT) - 1);
            if (ret != HAL_OK) { break; }

            /* Enable sensor FIFO interrupt */
            ret = tsl2585_set_interrupt_enable(hi2c, TSL2585_INTENAB_FIEN);
            if (ret != HAL_OK) { break; }
        } else {
            /* Configure to interrupt on every ALS cycle */
            ret = tsl2585_set_als_interrupt_persistence(hi2c, TSL2585_MOD0, 0);
            if (ret != HAL_OK) { break; }

            /* Set FIFO threshold to default value */
            ret = tsl2585_set_fifo_threshold(hi2c, 255);
            if (ret != HAL_OK) { break; }

            /* Enable sensor ALS interrupt */
            ret = tsl2585_set_interrupt_enable(hi2c, TSL2585_INTENAB_AIEN);
            if (ret != HAL_OK) { break; }
        }

        if (sensor_osc_calibration_enabled) {
            ret = sensor_osc_calibration();
            if (ret != HAL_OK) {
                break;
            }
        }

        if (fast_mode) {
            /* Set the FT260 I2C clock speed to 1MHz for faster FIFO reads */
            if (usbh_ft260_set_i2c_clock_speed(device_handle, 1000) != osOK) {
                log_w("Unable to set I2C clock speed");
                ret = HAL_ERROR;
                break;
            }
        }

        /* Enable the sensor (ALS Enable and Power ON) */
        ret = tsl2585_enable(hi2c);
        if (ret != HAL_OK) {
            break;
        }

        sensor_state.sai_active = false;
        sensor_state.agc_disabled_reset_gain = false;
        sensor_state.discard_next_reading = false;
        sensor_state.single_shot = single_shot;
        sensor_state.fast_mode = fast_mode;
        sensor_state.running = true;
        meter_probe_state = METER_PROBE_STATE_RUNNING;
    } while (0);

    return hal_to_os_status(ret);
}

HAL_StatusTypeDef sensor_osc_calibration()
{
    //FIXME This routine will need to be rewritten to use an HW trigger synchronized to USB SOF or just removed
    HAL_StatusTypeDef ret = HAL_OK;
#if 0
    uint8_t status3 = 0;
    uint16_t vsync_period = 0;
    uint32_t sync_start;

    log_d("TSL2585 oscillator calibration (SW_VSYNC_TRIGGER)");

    do {
        /* Set VSYNC period target */
        ret = tsl2585_set_vsync_period_target(hi2c, 720, true); /* 1000Hz */
        if (ret != HAL_OK) { break; }

        /* Set calibration mode */
        ret = tsl2585_set_vsync_cfg(hi2c, TSL2585_VSYNC_CFG_OSC_CALIB_AFTER_PON | TSL2585_VSYNC_CFG_VSYNC_MODE);
        if (ret != HAL_OK) { break; }

        /* Power on the sensor (PON only) */
        ret = tsl2585_set_enable(hi2c, TSL2585_ENABLE_PON);
        if (ret != HAL_OK) { break; }

        /* Enable interrupts from the 1kHz timer being used for this process */
        sync_start = osKernelGetTickCount();
        sensor_osc_sync_count = 0;
        __HAL_TIM_ENABLE_IT(&htim3, TIM_IT_UPDATE);

        /* Wait until we've sent at least two I2C sync commands from the ISR */
        while (sensor_osc_sync_count < 3) {
            /* Check for timeout conditions and fail accordingly */
            if (osKernelGetTickCount() - sync_start > 10) {
                __HAL_TIM_DISABLE_IT(&htim3, TIM_IT_UPDATE);
                log_w("VSYNC timeout error");
                break;
            }
        }

        /* Check if there was an I2C error inside the ISR */
        if (sensor_osc_sync_count > 10) {
            log_w("I2C error during VSYNC trigger interrupt");
            ret = HAL_ERROR;
            break;
        }

        ret = tsl2585_get_status3(hi2c, &status3);
        if (ret != HAL_OK) { break; }

        if ((status3 & TSL2585_STATUS3_OSC_CALIB_FINISHED) != 0) {
            ret = tsl2585_get_vsync_period(hi2c, &vsync_period);
            if (ret != HAL_OK) { break; }
            log_d("VSYNC_PERIOD=%d (%f Hz)", vsync_period, (1.0F/(vsync_period * 1.388889F))*1000000.0F);
        } else {
            log_w("VSYNC_PERIOD did not match the target");
            ret = HAL_ERROR;
            break;
        }

        /* Reconfigure the VSYNC of the sensor back to its default */
        ret = tsl2585_set_vsync_cfg(hi2c, 0x00);
        if (ret != HAL_OK) { break; }

    } while (0);

    /* Disable the sensor in case of failure */
    if (ret != HAL_OK) {
        log_w("VSYNC calibration failed");
        tsl2585_disable(hi2c);
    }
#endif
    return ret;
}

osStatus_t meter_probe_sensor_disable()
{
    if (meter_probe_state < METER_PROBE_STATE_STARTED || !sensor_state.running) { return osErrorResource; }

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
        ret = tsl2585_disable(hi2c);
        if (ret != HAL_OK) { break; }

        if (sensor_state.fast_mode) {
            /* Return the FT260 I2C clock speed its default */
            if (usbh_ft260_set_i2c_clock_speed(device_handle, 400) != osOK) {
                log_w("Unable to set I2C clock speed");
            }
        }

        sensor_state.running = false;
        sensor_state.start_mode = METER_PROBE_START_NONE;
        meter_probe_state = METER_PROBE_STATE_STARTED;
    } while (0);

    return hal_to_os_status(ret);
}

osStatus_t meter_probe_sensor_set_gain(tsl2585_gain_t gain)
{
    if (meter_probe_state < METER_PROBE_STATE_STARTED) { return osErrorResource; }

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
        ret = tsl2585_set_mod_gain(hi2c, params->mod, TSL2585_STEP0, params->gain);
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
    if (meter_probe_state < METER_PROBE_STATE_STARTED) { return osErrorResource; }

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
        ret = tsl2585_set_sample_time(hi2c, params->sample_time);
        if (ret == HAL_OK) {
            sensor_state.sample_time = params->sample_time;
        }

        ret = tsl2585_set_als_num_samples(hi2c, params->sample_count);
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

osStatus_t meter_probe_sensor_set_mod_calibration(uint8_t value)
{
    if (meter_probe_state < METER_PROBE_STATE_STARTED) { return osErrorResource; }

    osStatus_t result = osOK;
    meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_SENSOR_SET_MOD_CALIBRATION,
        .result = &result,
        .mod_calibration = {
            .nth_iteration_value = value
        }
    };
    osMessageQueuePut(meter_probe_control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(meter_probe_control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_sensor_set_mod_calibration(const sensor_control_mod_cal_params_t *params)
{
    HAL_StatusTypeDef ret = HAL_OK;
    log_d("meter_probe_control_sensor_set_mod_calibration: %d", params->nth_iteration_value);

    if (sensor_state.running) {
        ret = tsl2585_set_calibration_nth_iteration(hi2c, params->nth_iteration_value);
        if (ret == HAL_OK) {
            sensor_state.nth_iteration_value = params->nth_iteration_value;
        }

        sensor_state.discard_next_reading = true;
        osMessageQueueReset(sensor_reading_queue);
    } else {
        sensor_state.nth_iteration_value = params->nth_iteration_value;
        sensor_state.mod_calibration_pending = true;
    }

    return hal_to_os_status(ret);
}

osStatus_t meter_probe_sensor_enable_agc(uint16_t sample_count)
{
    if (meter_probe_state < METER_PROBE_STATE_STARTED) { return osErrorResource; }

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
        ret = tsl2585_set_agc_num_samples(hi2c, params->sample_count);
        if (ret == HAL_OK) {
            sensor_state.agc_sample_count = params->sample_count;
        }

        ret = tsl2585_set_agc_calibration(hi2c, true);
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
    if (meter_probe_state < METER_PROBE_STATE_STARTED) { return osErrorResource; }

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
            ret = tsl2585_set_agc_calibration(hi2c, false);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_agc_num_samples(hi2c, 0);
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
    if (meter_probe_state < METER_PROBE_STATE_STARTED || !sensor_state.running) { return osErrorResource; }

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
        tsl2585_get_status4(hi2c, &status4);
        if ((status4 & TSL2585_STATUS4_SAI_ACTIVE) != 0) {
            sensor_state.sai_active = true;
        } else {
            log_e("Integration cycle in progress");
            return osErrorResource;
        }
    }

    ret = tsl2585_clear_sleep_after_interrupt(hi2c);
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
    if (meter_probe_state < METER_PROBE_STATE_STARTED || !sensor_state.running) { return osErrorResource; }

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

        if (reading.reading[0].status == METER_SENSOR_RESULT_VALID) {
            has_result = true;
            break;
        }

        count++;

    } while (count < max_count);

    if (has_result) {
        if (reading.reading[0].status == METER_SENSOR_RESULT_VALID) {
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
        } else if (reading.reading[0].status == METER_SENSOR_RESULT_SATURATED_ANALOG
            || reading.reading[0].status == METER_SENSOR_RESULT_SATURATED_DIGITAL) {
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
    if (sensor_reading->reading[0].gain <= TSL2585_GAIN_256X) {
        als_gain = sensor_settings.cal_gain.values[sensor_reading->reading[0].gain];
    } else {
        als_gain = tsl2585_gain_value(sensor_reading->reading[0].gain);
    }

    if (!is_valid_number(atime_ms) || !is_valid_number(als_gain)) { return NAN; }

    /* Divide to get numbers in a similar range as previous sensors */
    float als_reading = (float)sensor_reading->reading[0].data / 16.0F;

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
    if (sensor_reading->reading[0].gain >= TSL2585_GAIN_MAX) { return NAN; }
    if (!meter_probe_has_sensor_settings) { return NAN; }

    const float lux_slope = sensor_settings.cal_target.lux_slope;
    const float lux_intercept = sensor_settings.cal_target.lux_intercept;
    if (!is_valid_number(lux_slope) || !is_valid_number(lux_intercept)) { return NAN; }

    const float basic_value = meter_probe_basic_result(sensor_reading);
    if (!is_valid_number(basic_value)) { return NAN; }

    float lux = (basic_value * lux_slope) + lux_intercept;

    /* Prevent negative results */
    if (lux < 0.0F) {
        lux = 0.0F;
    }

    return lux;
}

void meter_probe_int_handler(uint32_t ticks)
{
    if (meter_probe_state < METER_PROBE_STATE_STARTED || !sensor_state.running) { return; }

    meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_INTERRUPT,
        .interrupt = {
            .ticks = ticks
        }
    };

    osMessageQueuePut(meter_probe_control_queue, &control_event, 0, 0);
}

osStatus_t meter_probe_control_interrupt(const sensor_control_interrupt_params_t *params)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t status = 0;
    meter_probe_sensor_reading_t sensor_reading = {0};
    bool has_reading = false;

#if 0
    log_d("meter_probe_control_interrupt");
#endif

    if (meter_probe_state < METER_PROBE_STATE_STARTED || !sensor_state.running) {
        log_d("Unexpected meter probe interrupt");
    }

    do {
        /* Get the interrupt status */
        ret = tsl2585_get_status(hi2c, &status);
        if (ret != HAL_OK) { break; }

#if 0
        /* Log interrupt flags */
        log_d("MINT=%d, AINT=%d, FINT=%d, SINT=%d",
            (status & TSL2585_STATUS_MINT) != 0,
            (status & TSL2585_STATUS_AINT) != 0,
            (status & TSL2585_STATUS_FINT) != 0,
            (status & TSL2585_STATUS_SINT) != 0);
#endif

        if ((!sensor_state.fast_mode && (status & TSL2585_STATUS_AINT) != 0) || (sensor_state.fast_mode && (status & TSL2585_STATUS_FINT) != 0)) {
            uint32_t elapsed_ticks = params->ticks - last_aint_ticks;
            last_aint_ticks = params->ticks;

            tsl2585_fifo_data_t fifo_data[MAX_ALS_COUNT];
            bool overflow = false;

            if (sensor_state.fast_mode) {
                ret = sensor_control_read_fifo_fast_mode(fifo_data, &overflow, params->ticks);
            } else {
                ret = sensor_control_read_fifo(&fifo_data[0], &overflow);
            }
            if (ret != HAL_OK) { break; }

            /* FIFO reads implicitly clear the FINT bit */
            if (sensor_state.fast_mode) {
                status &= ~TSL2585_STATUS_FINT;
            }

            if (sensor_state.discard_next_reading) {
                sensor_state.discard_next_reading = false;
            } else {
                if (overflow) {
                    for (uint8_t i = 0; i < (uint8_t) (sensor_state.fast_mode ? MAX_ALS_COUNT : 1); i++) {
                        sensor_reading.reading[i].data = UINT32_MAX;
                        sensor_reading.reading[i].gain = sensor_state.gain[i];
                        sensor_reading.reading[i].status = METER_SENSOR_RESULT_INVALID;
                    }
                } else {
                    for (uint8_t i = 0; i < (uint8_t) (sensor_state.fast_mode ? MAX_ALS_COUNT : 1); i++) {
                        if ((fifo_data[i].als_status & TSL2585_ALS_DATA0_ANALOG_SATURATION_STATUS) != 0) {
#if 0
                            log_d("TSL2585: [analog saturation]");
#endif
                            sensor_reading.reading[i].data = UINT32_MAX;
                            sensor_reading.reading[i].gain = sensor_state.gain[i];
                            sensor_reading.reading[i].status = METER_SENSOR_RESULT_SATURATED_ANALOG;
                        } else {
                            tsl2585_gain_t als_gain = (fifo_data[i].als_status2 & 0x0F);

                            /* If AGC is enabled, then update the configured gain value */
                            if (sensor_state.agc_enabled) {
                                sensor_state.gain[0] = als_gain;
                            }

                            sensor_reading.reading[i].data = fifo_data[i].als_data0;
                            sensor_reading.reading[i].gain = als_gain;
                            sensor_reading.reading[i].status = METER_SENSOR_RESULT_VALID;
                        }
                    }
                }

                /* Fill out other reading fields */
                sensor_reading.sample_time = sensor_state.sample_time;
                sensor_reading.sample_count = sensor_state.sample_count;
                sensor_reading.ticks = params->ticks;
                sensor_reading.elapsed_ticks = elapsed_ticks;

                has_reading = true;
            }

            /*
             * If AGC was just disabled, then reset the gain to its last
             * known value and ignore the reading. This is necessary because
             * disabling AGC on its own seems to reset the gain to a low
             * default, and attempting to set it immediately after setting
             * the registers to disable AGC does not seem to take.
             */
            if (sensor_state.agc_disabled_reset_gain) {
                ret = tsl2585_set_mod_gain(hi2c, TSL2585_MOD0, TSL2585_STEP0, sensor_state.gain[0]);
                if (ret != HAL_OK) { break; }
                sensor_state.agc_disabled_reset_gain = false;
                sensor_state.discard_next_reading = true;
            }
        }

        if (status != 0) {
            /* Clear the interrupt status */
            ret = tsl2585_set_status(hi2c, status);
            if (ret != HAL_OK) { break; }
        }

        /* Check single-shot specific state */
        if (sensor_state.single_shot) {
            uint8_t status4 = 0;
            ret = tsl2585_get_status4(hi2c, &status4);
            if (ret != HAL_OK) { break; }
            if ((status4 & TSL2585_STATUS4_SAI_ACTIVE) != 0) {
#if 0
                log_d("Sleep after interrupt");
#endif
                sensor_state.sai_active = true;
            }
        }
    } while (0);

    if (has_reading) {
#if 0
        log_d("TSL2585: MOD0=%lu, Gain=[%s], Time=%.2fms",
            reading.als_data, tsl2585_gain_str(reading.gain),
            tsl2585_integration_time_ms(sensor_state.sample_time, sensor_state.sample_count));
#endif

        QueueHandle_t queue = (QueueHandle_t)sensor_reading_queue;
        xQueueOverwrite(queue, &sensor_reading);
    }

    return hal_to_os_status(ret);
}

HAL_StatusTypeDef sensor_control_read_fifo(tsl2585_fifo_data_t *fifo_data, bool *overflow)
{
    HAL_StatusTypeDef ret;
    tsl2585_fifo_status_t fifo_status;
    uint8_t data[FIFO_ALS_ENTRY_SIZE];
    uint8_t counter = 0;
    const uint8_t data_size = FIFO_ALS_ENTRY_SIZE;

    do {
        ret = tsl2585_get_fifo_status(hi2c, &fifo_status);
        if (ret != HAL_OK) { break; }

#if 0
        log_d("FIFO_STATUS: OVERFLOW=%d, UNDERFLOW=%d, LEVEL=%d", fifo_status.overflow, fifo_status.underflow, fifo_status.level);
#endif

        if (fifo_status.overflow) {
            log_w("FIFO overflow, clearing");
            ret = tsl2585_clear_fifo(hi2c);
            if (ret != HAL_OK) { break; }

            if (fifo_data) {
                memset(fifo_data, 0, sizeof(tsl2585_fifo_data_t));
            }
            if (overflow) {
                *overflow = true;
            }

            break;
        } else if ((fifo_status.level % data_size) != 0) {
            log_w("Unexpected size of data in FIFO: %d != %d", fifo_status.level, data_size);
            ret = HAL_ERROR;
            break;
        }

        while (fifo_status.level >= data_size) {
            ret = tsl2585_read_fifo(hi2c, data, data_size);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_fifo_status(hi2c, &fifo_status);
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
        if (overflow) {
            *overflow = false;
        }
    } while (0);

    return ret;
}

HAL_StatusTypeDef sensor_control_read_fifo_fast_mode(tsl2585_fifo_data_t *fifo_data, bool *overflow, uint32_t ticks)
{
    HAL_StatusTypeDef ret = HAL_OK;
    tsl2585_fifo_status_t fifo_status;
    uint8_t data[FIFO_ALS_ENTRY_SIZE * MAX_ALS_COUNT];

#if 0
    /* Need to track any receive overhead */
    const uint32_t time_elapsed = osKernelGetTickCount() - ticks;
#endif

    do {
        /* Read FIFO status and data in a single operation */
        ret = tsl2585_read_fifo_combo(hi2c, &fifo_status, data, sizeof(data));
        if (ret != HAL_OK) { break; }

#if 0
        const uint32_t time_read = osKernelGetTickCount() - ticks;
        log_d("FIFO_STATUS: OVERFLOW=%d, UNDERFLOW=%d, LEVEL=%d", fifo_status.overflow, fifo_status.underflow, fifo_status.level);
        log_d("tt_process=%dms, tt_read=%dms", time_elapsed, time_read - time_elapsed);
#endif

        if (fifo_status.overflow) {
            log_w("FIFO overflow, clearing");
            ret = tsl2585_clear_fifo(hi2c);
            if (ret != HAL_OK) { break; }

            if (fifo_data) {
                memset(fifo_data, 0, sizeof(tsl2585_fifo_data_t));
            }
            if (overflow) {
                *overflow = true;
            }

            break;
        } else {
            uint16_t fifo_readings = (fifo_status.level / 7);

            if (fifo_readings >= MAX_ALS_COUNT * 2) {
                log_w("Missed %d sensor read cycles", (fifo_readings / MAX_ALS_COUNT) - 1);
                ret = tsl2585_clear_fifo(hi2c);
                if (ret != HAL_OK) { break; }
                if (fifo_data) {
                    memset(fifo_data, 0, sizeof(tsl2585_fifo_data_t));
                }
                if (overflow) {
                    *overflow = true;
                }
            }
        }

        /* Parse out the received FIFO data set */
        if (fifo_data) {
            uint8_t offset = 0;
            memset(fifo_data, 0, sizeof(tsl2585_fifo_data_t) * MAX_ALS_COUNT);

            for (uint8_t i = 0; i < MAX_ALS_COUNT; i++) {
                fifo_data[i].als_data0 =
                    (uint32_t) data[offset + 3] << 24
                    | (uint32_t) data[offset + 2] << 16
                    | (uint32_t) data[offset + 1] << 8
                    | (uint32_t) data[offset + 0];

                fifo_data[i].als_status = data[offset + 4];
                fifo_data[i].als_status2 = data[offset + 5];
                fifo_data[i].als_status3 = data[offset + 6];
                offset += FIFO_ALS_ENTRY_SIZE;
            }
        }
        if (overflow) {
            *overflow = false;
        }
    } while (0);
    return ret;
}
