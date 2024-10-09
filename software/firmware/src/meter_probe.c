#include "meter_probe.h"

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <cmsis_os.h>

#include <string.h>
#include <math.h>

#define LOG_TAG "meter_probe"
#include <elog.h>
#include <i2c_interface.h>

#include "board_config.h"
#include "meter_probe_settings.h"
#include "tsl2585.h"
#include "keypad.h"
#include "usb_host.h"
#include "usb_ft260.h"
#include "util.h"

/* I2C address of the digital potentiometer used to control DensiStick light intensity */
static const uint8_t MCP4017_ADDRESS = 0x2F; /* Use 7-bit address */

/*
 * Size of an ALS reading in the TSL2585 FIFO, when using our
 * preferred settings: 3B status, 4B ALS data
 */
#define FIFO_ALS_ENTRY_SIZE 7

typedef enum {
    METER_PROBE_DEVICE_METER_PROBE = 0,
    METER_PROBE_DEVICE_DENSISTICK
} meter_probe_device_type_t;

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
    METER_PROBE_CONTROL_STICK_SET_LIGHT_ENABLE,
    METER_PROBE_CONTROL_STICK_SET_LIGHT_VALUE,
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
        int value;
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

typedef struct {
    /* Device type to control functionality */
    meter_probe_device_type_t device_type;
    /* Queue for meter probe control events */
    const osMessageQueueAttr_t control_queue_attrs;
    /* Queue to hold the latest sensor reading */
    const osMessageQueueAttr_t reading_queue_attrs;
    /* Semaphore to synchronize sensor control calls */
    const osSemaphoreAttr_t control_semaphore_attrs;
} meter_probe_init_attrs_t;

static const meter_probe_init_attrs_t probe_init_attrs = {
    .device_type = METER_PROBE_DEVICE_METER_PROBE,
    .control_queue_attrs = { .name = "probe_control_queue" },
    .reading_queue_attrs =  { .name = "probe_sensor_reading_queue" },
    .control_semaphore_attrs = { .name = "probe_control_semaphore_attr" }
};

static const meter_probe_init_attrs_t stick_init_attrs = {
    .device_type = METER_PROBE_DEVICE_DENSISTICK,
    .control_queue_attrs = { .name = "stick_control_queue" },
    .reading_queue_attrs =  { .name = "stick_sensor_reading_queue" },
    .control_semaphore_attrs = { .name = "stick_control_semaphore_attr" }
};

typedef struct __meter_probe_handle_t {
    /* Device type */
    meter_probe_device_type_t device_type;

    /* Device handles */
    ft260_device_t *device_handle;
    i2c_handle_t *hi2c;

    /* Device state variables */
    meter_probe_state_t probe_state;
    bool has_sensor_settings;
    meter_probe_settings_handle_t settings_handle;
    union {
        meter_probe_settings_tsl2585_t probe_settings;
        densistick_settings_tsl2585_t stick_settings;
    };
    uint8_t sensor_device_id[3];
    tsl2585_state_t sensor_state;
    uint32_t last_aint_ticks;
    bool stick_light_enabled;
    uint8_t stick_light_brightness;

    /* Queues and semaphores */
    osMessageQueueId_t control_queue;
    osMessageQueueId_t sensor_reading_queue;
    osSemaphoreId_t control_semaphore;
} meter_probe_handle_t;

/* Handle for the instance managing the Meter Probe peripheral */
static meter_probe_handle_t probe_handle = {0};

/* Handle for the instance managing the DensiStick peripheral */
static meter_probe_handle_t stick_handle = {0};

/* TSL2585: Only enable Photopic photodiodes on modulator 0 */
static const tsl2585_modulator_t sensor_tsl2585_phd_mod_vis[] = {
    0, TSL2585_MOD0, 0, 0, 0, TSL2585_MOD0
};

/* TSL2521: Only enable Clear photodiodes on modulator 0 */
static const tsl2585_modulator_t sensor_tsl2521_phd_mod_vis[] = {
    0, TSL2585_MOD0, TSL2585_MOD0, TSL2585_MOD0, TSL2585_MOD0, 0
};

static bool task_meter_probe_run_init(meter_probe_handle_t *handle, const meter_probe_init_attrs_t *attrs);
static void task_meter_probe_run_loop(meter_probe_handle_t *handle);

/* Meter probe interface functions only called by internally handled events */
static osStatus_t meter_probe_attach(meter_probe_handle_t *handle);
static osStatus_t meter_probe_detach(meter_probe_handle_t *handle);

/* Meter probe control implementation functions */
static osStatus_t meter_probe_control_start(meter_probe_handle_t *handle);
static osStatus_t meter_probe_control_stop(meter_probe_handle_t *handle);
static osStatus_t meter_probe_control_attach(meter_probe_handle_t *handle);
static osStatus_t meter_probe_control_detach(meter_probe_handle_t *handle);
static osStatus_t meter_probe_sensor_enable_impl(meter_probe_handle_t *handle, sensor_control_start_mode_t start_mode);
static osStatus_t meter_probe_control_sensor_enable(meter_probe_handle_t *handle, sensor_control_start_mode_t start_mode);
static osStatus_t meter_probe_control_sensor_disable(meter_probe_handle_t *handle);
static osStatus_t meter_probe_control_sensor_set_gain(meter_probe_handle_t *handle, const sensor_control_gain_params_t *params);
static osStatus_t meter_probe_control_sensor_integration(meter_probe_handle_t *handle, const sensor_control_integration_params_t *params);
static osStatus_t meter_probe_control_sensor_set_mod_calibration(meter_probe_handle_t *handle, const sensor_control_mod_cal_params_t *params);
static osStatus_t meter_probe_control_sensor_enable_agc(meter_probe_handle_t *handle, const sensor_control_agc_params_t *params);
static osStatus_t meter_probe_control_sensor_disable_agc(meter_probe_handle_t *handle);
static osStatus_t meter_probe_control_sensor_trigger_next_reading(meter_probe_handle_t *handle);
static osStatus_t meter_probe_control_set_light_enable(meter_probe_handle_t *handle, bool enable);
static osStatus_t meter_probe_control_set_light_value(meter_probe_handle_t *handle, uint8_t value);
static osStatus_t meter_probe_control_interrupt(meter_probe_handle_t *handle, const sensor_control_interrupt_params_t *params);

static void usb_meter_probe_event_callback(ft260_device_t *device, ft260_device_event_t event_type, uint32_t ticks, void *user_data);
static void meter_probe_int_handler(meter_probe_handle_t *handle, uint32_t ticks);

static HAL_StatusTypeDef sensor_control_read_fifo(meter_probe_handle_t *handle, tsl2585_fifo_data_t *fifo_data, bool *overflow);
static HAL_StatusTypeDef sensor_control_read_fifo_fast_mode(meter_probe_handle_t *handle, tsl2585_fifo_data_t *fifo_data, bool *overflow, uint32_t ticks);

meter_probe_handle_t *meter_probe_handle()
{
    return &probe_handle;
}

meter_probe_handle_t *densistick_handle()
{
    return &stick_handle;
}

void task_meter_probe_run(void *argument)
{
    osSemaphoreId_t task_start_semaphore = argument;
    bool task_init_success = false;
    meter_probe_handle_t *handle = NULL;

    const char *task_name = osThreadGetName(osThreadGetId());

    log_d("%s task start", task_name);

    do {
        const meter_probe_init_attrs_t *attrs = NULL;

        if (strcmp(task_name, "meter_probe") == 0) {
            handle = &probe_handle;
            attrs = &probe_init_attrs;
        } else if (strcmp(task_name, "densistick") == 0) {
            handle = &stick_handle;
            attrs = &stick_init_attrs;
        } else {
            break;
        }

        if (!task_meter_probe_run_init(handle, attrs)) {
            break;
        }
        task_init_success = true;
    } while (0);

    /* Release the startup semaphore */
    if (osSemaphoreRelease(task_start_semaphore) != osOK) {
        log_e("Unable to release task_start_semaphore");
        return;
    }

    if (task_init_success) {
        task_meter_probe_run_loop(handle);
    }
}

bool task_meter_probe_run_init(meter_probe_handle_t *handle, const meter_probe_init_attrs_t *attrs)
{
    /* Set the device type */
    handle->device_type = attrs->device_type;

    /* Create the queue for meter probe control events */
    handle->control_queue = osMessageQueueNew(20, sizeof(meter_probe_control_event_t), &attrs->control_queue_attrs);
    if (!handle->control_queue) {
        log_e("Unable to create control queue");
        return false;
    }

    /* Create the one-element queue to hold the latest sensor reading */
    handle->sensor_reading_queue = osMessageQueueNew(1, sizeof(meter_probe_sensor_reading_t), &attrs->reading_queue_attrs);
    if (!handle->sensor_reading_queue) {
        log_e("Unable to create reading queue");
        return false;
    }

    /* Create the semaphore used to synchronize sensor control */
    handle->control_semaphore = osSemaphoreNew(1, 0, &attrs->control_semaphore_attrs);
    if (!handle->control_semaphore) {
        log_e("control_semaphore create error");
        return false;
    }

    /* Get the persistent device handles used to talk to a connected meter probe */
    if (attrs->device_type == METER_PROBE_DEVICE_METER_PROBE) {
        handle->device_handle = usbh_ft260_get_device(FT260_METER_PROBE);
    } else if (attrs->device_type == METER_PROBE_DEVICE_DENSISTICK) {
        handle->device_handle = usbh_ft260_get_device(FT260_DENSISTICK);
    }
    if (!handle->device_handle) {
        log_e("Unable to get FT260 device interface");
        return false;
    }
    handle->hi2c = usbh_ft260_get_device_i2c(handle->device_handle);

    /* Set the meter probe event callback */
    usbh_ft260_set_device_callback(handle->device_handle, usb_meter_probe_event_callback, handle);

    handle->probe_state = METER_PROBE_STATE_INIT;
    return true;
}

void task_meter_probe_run_loop(meter_probe_handle_t *handle)
{
    meter_probe_control_event_t control_event;

    /* Start the main control event loop */
    for (;;) {
        if(osMessageQueueGet(handle->control_queue, &control_event, NULL, portMAX_DELAY) == osOK) {
            osStatus_t ret = osOK;
            switch (control_event.event_type) {
            case METER_PROBE_CONTROL_STOP:
                ret = meter_probe_control_stop(handle);
                break;
            case METER_PROBE_CONTROL_START:
                ret = meter_probe_control_start(handle);
                break;
            case METER_PROBE_CONTROL_ATTACH:
                ret = meter_probe_control_attach(handle);
                break;
            case METER_PROBE_CONTROL_DETACH:
                ret = meter_probe_control_detach(handle);
                break;
            case METER_PROBE_CONTROL_SENSOR_ENABLE:
                ret = meter_probe_control_sensor_enable(handle, control_event.start_mode);
                break;
            case METER_PROBE_CONTROL_SENSOR_DISABLE:
                ret = meter_probe_control_sensor_disable(handle);
                break;
            case METER_PROBE_CONTROL_SENSOR_SET_GAIN:
                ret = meter_probe_control_sensor_set_gain(handle, &control_event.gain);
                break;
            case METER_PROBE_CONTROL_SENSOR_SET_INTEGRATION:
                ret = meter_probe_control_sensor_integration(handle, &control_event.integration);
                break;
            case METER_PROBE_CONTROL_SENSOR_SET_MOD_CALIBRATION:
                ret = meter_probe_control_sensor_set_mod_calibration(handle, &control_event.mod_calibration);
                break;
            case METER_PROBE_CONTROL_SENSOR_ENABLE_AGC:
                ret = meter_probe_control_sensor_enable_agc(handle, &control_event.agc);
                break;
            case METER_PROBE_CONTROL_SENSOR_DISABLE_AGC:
                ret = meter_probe_control_sensor_disable_agc(handle);
                break;
            case METER_PROBE_CONTROL_SENSOR_TRIGGER_NEXT_READING:
                ret = meter_probe_control_sensor_trigger_next_reading(handle);
                break;
            case METER_PROBE_CONTROL_STICK_SET_LIGHT_ENABLE:
                ret = meter_probe_control_set_light_enable(handle, (control_event.value == 1) ? true : false);
                break;
            case METER_PROBE_CONTROL_STICK_SET_LIGHT_VALUE:
                ret = meter_probe_control_set_light_value(handle, control_event.value);
                break;
            case METER_PROBE_CONTROL_INTERRUPT:
                ret = meter_probe_control_interrupt(handle, &control_event.interrupt);
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
                if (osSemaphoreRelease(handle->control_semaphore) != osOK) {
                    log_e("Unable to release control_semaphore");
                }
            }
        }
    }

}

void usb_meter_probe_event_callback(ft260_device_t *device, ft260_device_event_t event_type, uint32_t ticks, void *user_data)
{
    meter_probe_handle_t *handle = user_data;
    if (!handle || handle->device_handle != device) { return; }

    if (handle->device_type == METER_PROBE_DEVICE_METER_PROBE) {
        switch (event_type) {
        case FT260_EVENT_ATTACH:
            log_d("Meter probe attached");
            meter_probe_attach(handle);
            break;
        case FT260_EVENT_DETACH:
            log_d("Meter probe detached");
            meter_probe_detach(handle);
            break;
        case FT260_EVENT_INTERRUPT:
            meter_probe_int_handler(handle, ticks);
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
    } else if (handle->device_type == METER_PROBE_DEVICE_DENSISTICK) {
        switch (event_type) {
        case FT260_EVENT_ATTACH:
            log_d("DensiStick attached");
            meter_probe_attach(handle);
            break;
        case FT260_EVENT_DETACH:
            log_d("DensiStick detached");
            meter_probe_detach(handle);
            break;
        case FT260_EVENT_INTERRUPT:
            meter_probe_int_handler(handle, ticks);
            break;
        case FT260_EVENT_BUTTON_DOWN:
            keypad_inject_raw_event(KEYPAD_DENSISTICK, true, ticks);
            break;
        case FT260_EVENT_BUTTON_UP:
            keypad_inject_raw_event(KEYPAD_DENSISTICK, false, ticks);
            break;
        default:
            break;
        }
    }
}

bool meter_probe_is_attached(const meter_probe_handle_t *handle)
{
    if (!handle) { return false; }
    return handle->probe_state >= METER_PROBE_STATE_ATTACHED;
}

bool meter_probe_is_started(const meter_probe_handle_t *handle)
{
    if (!handle) { return false; }
    return handle->probe_state == METER_PROBE_STATE_STARTED || handle->probe_state == METER_PROBE_STATE_RUNNING;
}

osStatus_t meter_probe_start(meter_probe_handle_t *handle)
{
    if (!handle) { return osErrorParameter; }
    if (handle->probe_state != METER_PROBE_STATE_ATTACHED) { return osErrorResource; }

    osStatus_t result = osOK;
    const meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_START,
        .result = &result
    };
    osMessageQueuePut(handle->control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(handle->control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_start(meter_probe_handle_t *handle)
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("meter_probe_control_start");

    do {
        if (handle->device_type == METER_PROBE_DEVICE_METER_PROBE) {
            if (!usb_meter_probe_is_attached()) {
                log_w("Meter probe not attached");
                ret = HAL_ERROR;
                break;
            }
        } else if (handle->device_type == METER_PROBE_DEVICE_DENSISTICK) {
            if (!usb_densistick_is_attached()) {
                log_w("DensiStick not attached");
                ret = HAL_ERROR;
                break;
            }
        } else {
            ret = HAL_ERROR;
            break;
        }

        /* Populate values that come from the USB descriptor */
        usbh_ft260_get_device_serial_number(handle->device_handle, handle->settings_handle.id.probe_serial);

        /* Set the FT260 I2C clock speed to the default of 400kHz */
        if (usbh_ft260_set_i2c_clock_speed(handle->device_handle, 400) != osOK) {
            log_w("Unable to set I2C clock speed");
            ret = HAL_ERROR;
            break;
        }

        if (handle->device_type == METER_PROBE_DEVICE_DENSISTICK) {
            /* Read the DensiStick's settings memory */
            ret = densistick_settings_init(&handle->settings_handle, handle->hi2c);
        } else {
            /* Read the meter probe's settings memory */
            ret = meter_probe_settings_init(&handle->settings_handle, handle->hi2c);
        }
        if (ret != HAL_OK) {
            break;
        }

        log_i("%s: type=%s, rev=[%d,%d], serial=%s",
            (handle->device_type == METER_PROBE_DEVICE_DENSISTICK) ? "DensiStick" : "Meter Probe",
            meter_probe_type_str(handle->settings_handle.id.probe_type),
            handle->settings_handle.id.probe_rev_major,
            handle->settings_handle.id.probe_rev_minor,
            handle->settings_handle.id.probe_serial);

        /*
         * The TSL2585 and TSL2521 are almost identical, except for the
         * modulator count and photodiode configuration. Therefore, both
         * use the same driver with only some minor differences in usage.
         */
        if (handle->settings_handle.id.probe_type != METER_PROBE_SENSOR_TSL2585
            && handle->settings_handle.id.probe_type != METER_PROBE_SENSOR_TSL2521) {
            log_w("Unknown meter probe type: %d", handle->settings_handle.id.probe_type);
            ret = HAL_ERROR;
            break;
        }

        /* Read the settings for the current sensor type */
        if (handle->device_type == METER_PROBE_DEVICE_DENSISTICK) {
            ret = densistick_settings_get_tsl2585(&handle->settings_handle, &handle->stick_settings);
        } else {
            ret = meter_probe_settings_get_tsl2585(&handle->settings_handle, &handle->probe_settings);
        }
        if (ret == HAL_OK) {
            handle->has_sensor_settings = true;
        } else {
            log_w("Unable to load sensor calibration");
            handle->has_sensor_settings = false;
        }

        /*
         * Do a basic initialization of the sensor, which verifies that
         * the sensor is functional and responding to commands.
         * This routine should complete with the sensor in a disabled
         * state.
         */
        ret = tsl2585_init(handle->hi2c, handle->sensor_device_id);
        if (ret != HAL_OK) { break; }

        handle->sensor_state.uv_calibration = 0;
        if (handle->settings_handle.id.probe_type == METER_PROBE_SENSOR_TSL2585) {
            ret = tsl2585_get_uv_calibration(handle->hi2c, &handle->sensor_state.uv_calibration);
            if (ret != HAL_OK) { break; }
            log_d("UV calibration value: %d", handle->sensor_state.uv_calibration);
        }

        /* Read the initial value of the DensiStick LED current potentiometer */
        if (handle->device_type == METER_PROBE_DEVICE_DENSISTICK) {
            ret = i2c_receive(handle->hi2c, MCP4017_ADDRESS, &handle->stick_light_brightness, 1, HAL_MAX_DELAY);
            if (ret != HAL_OK) { break; }
            log_d("DensiStick LED brightness value: %d", handle->stick_light_brightness);
        }

        handle->probe_state = METER_PROBE_STATE_STARTED;
    } while (0);

    if (handle->probe_state != METER_PROBE_STATE_STARTED) {
        log_w("Meter probe initialization failed");
        meter_probe_control_stop(handle);
    }

    return hal_to_os_status(ret);
}

osStatus_t meter_probe_stop(meter_probe_handle_t *handle)
{
    if (!handle) { return osErrorParameter; }
    if (handle->probe_state < METER_PROBE_STATE_ATTACHED) { return osErrorResource; }

    osStatus_t result = osOK;
    const meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_STOP,
        .result = &result
    };
    osMessageQueuePut(handle->control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(handle->control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_stop(meter_probe_handle_t *handle)
{
    log_d("meter_probe_control_stop");

    /* Reset state variables */
    if (handle->probe_state > METER_PROBE_STATE_ATTACHED) {
        handle->probe_state = METER_PROBE_STATE_ATTACHED;
    } else {
        handle->probe_state = METER_PROBE_STATE_INIT;
    }

    /* Clear the settings */
    memset(&handle->settings_handle, 0, sizeof(meter_probe_settings_handle_t));

    if (handle->device_type == METER_PROBE_DEVICE_DENSISTICK) {
        meter_probe_control_set_light_enable(handle, false);
        memset(&handle->stick_settings, 0, sizeof(densistick_settings_tsl2585_t));
        handle->stick_light_brightness = 0;
    } else {
        memset(&handle->probe_settings, 0, sizeof(meter_probe_settings_tsl2585_t));
    }

    memset(&handle->sensor_state, 0, sizeof(tsl2585_state_t));
    handle->has_sensor_settings = false;

    return osOK;
}

osStatus_t meter_probe_attach(meter_probe_handle_t *handle)
{
    osStatus_t result = osOK;
    if (!handle) { return osErrorParameter; }

    const meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_ATTACH,
        .result = &result
    };
    osMessageQueuePut(handle->control_queue, &control_event, 0, portMAX_DELAY);
    /* This command does not block on being handled by the task */
    return result;
}

osStatus_t meter_probe_control_attach(meter_probe_handle_t *handle)
{
    osStatus_t result = osOK;
    if (!handle) { return osErrorParameter; }

    log_d("meter_probe_control_attach");

    if (handle->probe_state == METER_PROBE_STATE_INIT) {
        handle->probe_state = METER_PROBE_STATE_ATTACHED;
        result = osOK;
    } else if (handle->probe_state == METER_PROBE_STATE_STARTED || handle->probe_state == METER_PROBE_STATE_RUNNING) {
        /* Not an expected case, so handle it like a glitch that should stop the meter probe */
        log_w("Meter probe attach while running");

        /* Try to disable the sensor, but ignore any errors */
        if (handle->probe_state == METER_PROBE_STATE_RUNNING) {
            meter_probe_control_sensor_disable(handle);
        }

        /* Set the meter probe to the stopped state */
        result = meter_probe_control_stop(handle);

        handle->probe_state = METER_PROBE_STATE_ATTACHED;
    } else {
        /* Should not get here */
        result = osErrorResource;
    }

    return result;
}

osStatus_t meter_probe_detach(meter_probe_handle_t *handle)
{
    osStatus_t result = osOK;
    if (!handle) { return osErrorParameter; }

    const meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_DETACH,
        .result = &result
    };
    osMessageQueuePut(handle->control_queue, &control_event, 0, portMAX_DELAY);
    /* This command does not block on being handled by the task */
    return result;
}

osStatus_t meter_probe_control_detach(meter_probe_handle_t *handle)
{
    osStatus_t result = osOK;

    log_d("meter_probe_control_detach");

    if (handle->probe_state == METER_PROBE_STATE_STARTED || handle->probe_state == METER_PROBE_STATE_RUNNING) {
        /* The meter probe went away, so clear out the existing state and return to init */
        result = meter_probe_control_stop(handle);
        handle->probe_state = METER_PROBE_STATE_INIT;
    } else if (handle->probe_state == METER_PROBE_STATE_ATTACHED) {
        handle->probe_state = METER_PROBE_STATE_INIT;
    }

    /* A detach in any other state is not relevant */

    return result;
}

osStatus_t meter_probe_get_device_info(const meter_probe_handle_t *handle, meter_probe_device_info_t *info)
{
    if (!handle || !info) { return osErrorParameter; }
    if (handle->probe_state < METER_PROBE_STATE_STARTED) { return osErrorResource; }

    memset(info, 0, sizeof(meter_probe_device_info_t));

    memcpy(&info->probe_id, &handle->settings_handle.id, sizeof(meter_probe_id_t));
    memcpy(info->sensor_id, handle->sensor_device_id, 3);

    return osOK;
}

bool meter_probe_has_settings(const meter_probe_handle_t *handle)
{
    if (!handle) { return osErrorParameter; }
    if (handle->probe_state < METER_PROBE_STATE_STARTED) { return osErrorResource; }
    return handle->has_sensor_settings;
}

osStatus_t meter_probe_get_settings(const meter_probe_handle_t *handle, meter_probe_settings_t *settings)
{
    if (!handle || !settings) { return osErrorParameter; }
    if (handle->device_type != METER_PROBE_DEVICE_METER_PROBE) { return osErrorParameter; }
    if (handle->probe_state < METER_PROBE_STATE_STARTED) { return osErrorResource; }

    memset(settings, 0, sizeof(meter_probe_settings_t));

    settings->type = handle->settings_handle.id.probe_type;
    if ((settings->type == METER_PROBE_SENSOR_TSL2585 || settings->type == METER_PROBE_SENSOR_TSL2521)
        && handle->has_sensor_settings) {
        memcpy(&settings->settings_tsl2585, &handle->probe_settings, sizeof(meter_probe_settings_tsl2585_t));
    }

    return osOK;
}

osStatus_t meter_probe_set_settings(meter_probe_handle_t *handle, const meter_probe_settings_t *settings)
{
    if (!handle || !settings) { return osErrorParameter; }
    if (handle->device_type != METER_PROBE_DEVICE_METER_PROBE) { return osErrorParameter; }
    if (handle->probe_state < METER_PROBE_STATE_STARTED || handle->sensor_state.running) { return osErrorResource; }

    if (settings->type != handle->settings_handle.id.probe_type) {
        log_w("Invalid settings device type");
        return osErrorParameter;
    }

    if (settings->type == METER_PROBE_SENSOR_TSL2585 || settings->type == METER_PROBE_SENSOR_TSL2521) {
        HAL_StatusTypeDef ret = meter_probe_settings_set_tsl2585(&handle->settings_handle, &settings->settings_tsl2585);
        return hal_to_os_status(ret);
    } else {
        log_w("Unsupported settings type");
        return osError;
    }
}

osStatus_t densistick_get_settings(const meter_probe_handle_t *handle, densistick_settings_t *settings)
{
    if (!handle || !settings) { return osErrorParameter; }
    if (handle->device_type != METER_PROBE_DEVICE_DENSISTICK) { return osErrorParameter; }
    if (handle->probe_state < METER_PROBE_STATE_STARTED) { return osErrorResource; }

    memset(settings, 0, sizeof(densistick_settings_t));

    settings->type = handle->settings_handle.id.probe_type;
    if ((settings->type == METER_PROBE_SENSOR_TSL2585 || settings->type == METER_PROBE_SENSOR_TSL2521)
        && handle->has_sensor_settings) {
        memcpy(&settings->settings_tsl2585, &handle->stick_settings, sizeof(densistick_settings_tsl2585_t));
    }

    return osOK;
}

osStatus_t densistick_set_settings(meter_probe_handle_t *handle, const densistick_settings_t *settings)
{
    if (!handle || !settings) { return osErrorParameter; }
    if (handle->device_type != METER_PROBE_DEVICE_DENSISTICK) { return osErrorParameter; }
    if (handle->probe_state < METER_PROBE_STATE_STARTED || handle->sensor_state.running) { return osErrorResource; }

    if (settings->type != handle->settings_handle.id.probe_type) {
        log_w("Invalid settings device type");
        return osErrorParameter;
    }

    if (settings->type == METER_PROBE_SENSOR_TSL2585 || settings->type == METER_PROBE_SENSOR_TSL2521) {
        HAL_StatusTypeDef ret = densistick_settings_set_tsl2585(&handle->settings_handle, &settings->settings_tsl2585);
        return hal_to_os_status(ret);
    } else {
        log_w("Unsupported settings type");
        return osError;
    }
}

osStatus_t meter_probe_sensor_enable(meter_probe_handle_t *handle)
{
    if (!handle) { return osErrorParameter; }
    return meter_probe_sensor_enable_impl(handle, METER_PROBE_START_NORMAL);
}

osStatus_t meter_probe_sensor_enable_fast_mode(meter_probe_handle_t *handle)
{
    if (!handle) { return osErrorParameter; }
    return meter_probe_sensor_enable_impl(handle, METER_PROBE_START_FAST_MODE);
}

osStatus_t meter_probe_sensor_enable_single_shot(meter_probe_handle_t *handle)
{
    if (!handle) { return osErrorParameter; }
    return meter_probe_sensor_enable_impl(handle, METER_PROBE_START_SINGLE_SHOT);
}

osStatus_t meter_probe_sensor_enable_impl(meter_probe_handle_t *handle, sensor_control_start_mode_t start_mode)
{
    if (handle->probe_state < METER_PROBE_STATE_STARTED || handle->sensor_state.running) { return osErrorResource; }

    osStatus_t result = osOK;
    const meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_SENSOR_ENABLE,
        .result = &result,
        .start_mode = start_mode
    };
    osMessageQueuePut(handle->control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(handle->control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_sensor_enable(meter_probe_handle_t *handle, sensor_control_start_mode_t start_mode)
{
    HAL_StatusTypeDef ret = HAL_OK;

    bool fast_mode;
    bool single_shot;
    switch (start_mode) {
    case METER_PROBE_START_FAST_MODE:
        fast_mode = true;
        single_shot = false;
        handle->sensor_state.start_mode = METER_PROBE_START_FAST_MODE;
        log_d("meter_probe_control_sensor_enable: fast_mode");
        break;
    case METER_PROBE_START_SINGLE_SHOT:
        fast_mode = false;
        single_shot = true;
        handle->sensor_state.start_mode = METER_PROBE_START_SINGLE_SHOT;
        log_d("meter_probe_control_sensor_enable: single_shot");
        break;
    case METER_PROBE_START_NORMAL:
    default:
        fast_mode = false;
        single_shot = false;
        handle->sensor_state.start_mode = METER_PROBE_START_NORMAL;
        log_d("meter_probe_control_sensor_enable: continuous");
        break;
    }

    do {
        /* Query the initial state of the sensor */
        if (!handle->sensor_state.gain_pending) {
            ret = tsl2585_get_mod_gain(handle->hi2c, TSL2585_MOD0, TSL2585_STEP0, &handle->sensor_state.gain[0]);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_mod_gain(handle->hi2c, TSL2585_MOD1, TSL2585_STEP0, &handle->sensor_state.gain[1]);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_mod_gain(handle->hi2c, TSL2585_MOD2, TSL2585_STEP0, &handle->sensor_state.gain[2]);
            if (ret != HAL_OK) { break; }
        }

        if (!handle->sensor_state.integration_pending) {
            ret = tsl2585_get_sample_time(handle->hi2c, &handle->sensor_state.sample_time);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_als_num_samples(handle->hi2c, &handle->sensor_state.sample_count);
            if (ret != HAL_OK) { break; }
        }

        if (!handle->sensor_state.mod_calibration_pending) {
            ret = tsl2585_get_calibration_nth_iteration(handle->hi2c, &handle->sensor_state.nth_iteration_value);
            if (ret != HAL_OK) { break; }
        }

        if (!handle->sensor_state.agc_pending) {
            ret = tsl2585_get_agc_num_samples(handle->hi2c, &handle->sensor_state.agc_sample_count);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_agc_calibration(handle->hi2c, &handle->sensor_state.agc_enabled);
            if (ret != HAL_OK) { break; }
        }

        /* Put the sensor into a known initial state */

        /* Enable writing of ALS status to the FIFO */
        ret = tsl2585_set_fifo_als_status_write_enable(handle->hi2c, true);
        if (ret != HAL_OK) { break; }

        /* Enable writing of results to the FIFO */
        ret = tsl2585_set_fifo_data_write_enable(handle->hi2c, TSL2585_MOD0, true);
        if (ret != HAL_OK) { break; }
        ret = tsl2585_set_fifo_data_write_enable(handle->hi2c, TSL2585_MOD1, false);
        if (ret != HAL_OK) { break; }
        ret = tsl2585_set_fifo_data_write_enable(handle->hi2c, TSL2585_MOD2, false);
        if (ret != HAL_OK) { break; }

        /* Set FIFO data format to 32-bits */
        ret = tsl2585_set_fifo_als_data_format(handle->hi2c, TSL2585_ALS_FIFO_32BIT);
        if (ret != HAL_OK) { break; }

        /* Set MSB position for full 26-bit result */
        ret = tsl2585_set_als_msb_position(handle->hi2c, 6);
        if (ret != HAL_OK) { break; }

        /* Make sure residuals are enabled */
        ret = tsl2585_set_mod_residual_enable(handle->hi2c, TSL2585_MOD0, TSL2585_STEPS_ALL);
        if (ret != HAL_OK) { break; }
        ret = tsl2585_set_mod_residual_enable(handle->hi2c, TSL2585_MOD1, TSL2585_STEPS_ALL);
        if (ret != HAL_OK) { break; }
        ret = tsl2585_set_mod_residual_enable(handle->hi2c, TSL2585_MOD2, TSL2585_STEPS_ALL);
        if (ret != HAL_OK) { break; }

        /* Select alternate gain table, which caps gain at 256x but gives us more residual bits */
        ret = tsl2585_set_mod_gain_table_select(handle->hi2c, true);
        if (ret != HAL_OK) { break; }

        /* Set maximum gain to 256x per app note on residual measurement */
        ret = tsl2585_set_max_mod_gain(handle->hi2c, TSL2585_GAIN_256X);
        if (ret != HAL_OK) { break; }

        /* Enable modulator */
        ret = tsl2585_enable_modulators(handle->hi2c, TSL2585_MOD0);
        if (ret != HAL_OK) { break; }

        /* Set modulator calibration rate */
        if (handle->sensor_state.mod_calibration_pending) {
            ret = tsl2585_set_calibration_nth_iteration(handle->hi2c, handle->sensor_state.nth_iteration_value);
            if (ret != HAL_OK) { break; }

            handle->sensor_state.mod_calibration_pending = false;
        }

        if (handle->settings_handle.id.probe_type == METER_PROBE_SENSOR_TSL2585) {
            /* Configure photodiodes for Photopic measurement only */
            ret = tsl2585_set_mod_photodiode_smux(handle->hi2c, TSL2585_STEP0, sensor_tsl2585_phd_mod_vis);
            if (ret != HAL_OK) { break; }
        } else if (handle->settings_handle.id.probe_type == METER_PROBE_SENSOR_TSL2521) {
            /* Configure photodiodes for Clear measurement only */
            ret = tsl2585_set_mod_photodiode_smux(handle->hi2c, TSL2585_STEP0, sensor_tsl2521_phd_mod_vis);
            if (ret != HAL_OK) { break; }
        }

        if (handle->sensor_state.gain_pending) {
            ret = tsl2585_set_mod_gain(handle->hi2c, TSL2585_MOD0, TSL2585_STEP0, handle->sensor_state.gain[0]);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_mod_gain(handle->hi2c, TSL2585_MOD1, TSL2585_STEP0, handle->sensor_state.gain[1]);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_mod_gain(handle->hi2c, TSL2585_MOD2, TSL2585_STEP0, handle->sensor_state.gain[2]);
            if (ret != HAL_OK) { break; }

            handle->sensor_state.gain_pending = false;
        }

        if (handle->sensor_state.integration_pending) {
            ret = tsl2585_set_sample_time(handle->hi2c, handle->sensor_state.sample_time);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_als_num_samples(handle->hi2c, handle->sensor_state.sample_count);
            if (ret != HAL_OK) { break; }

            handle->sensor_state.integration_pending = false;
        }

        if (handle->sensor_state.agc_pending) {
            ret = tsl2585_set_agc_num_samples(handle->hi2c, handle->sensor_state.agc_sample_count);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_agc_calibration(handle->hi2c, handle->sensor_state.agc_enabled);
            if (ret != HAL_OK) { break; }

            handle->sensor_state.agc_pending = false;
        }

        const float als_atime = tsl2585_integration_time_ms(handle->sensor_state.sample_time, handle->sensor_state.sample_count);
        const float agc_atime = tsl2585_integration_time_ms(handle->sensor_state.sample_time, handle->sensor_state.agc_sample_count);
        log_d("TSL2585 Initial State: Gain=%s, ALS_ATIME=%.2fms, AGC_ATIME=%.2fms",
            tsl2585_gain_str(handle->sensor_state.gain[0]), als_atime, agc_atime);

        /* Clear out any old sensor readings */
        osMessageQueueReset(handle->sensor_reading_queue);

        ret = tsl2585_set_single_shot_mode(handle->hi2c, single_shot);
        if (ret != HAL_OK) { break; }

        ret = tsl2585_set_sleep_after_interrupt(handle->hi2c, single_shot);
        if (ret != HAL_OK) { break; }


        if (fast_mode) {
            /* Set ALS interrupt persistence to track an unused modulator to avoid unwanted interrupts */
            ret = tsl2585_set_als_interrupt_persistence(handle->hi2c, TSL2585_MOD1, 1);
            if (ret != HAL_OK) { break; }

            /* Set FIFO threshold to a group of readings */
            ret = tsl2585_set_fifo_threshold(handle->hi2c, (FIFO_ALS_ENTRY_SIZE * MAX_ALS_COUNT) - 1);
            if (ret != HAL_OK) { break; }

            /* Enable sensor FIFO interrupt */
            ret = tsl2585_set_interrupt_enable(handle->hi2c, TSL2585_INTENAB_FIEN);
            if (ret != HAL_OK) { break; }
        } else {
            /* Configure to interrupt on every ALS cycle */
            ret = tsl2585_set_als_interrupt_persistence(handle->hi2c, TSL2585_MOD0, 0);
            if (ret != HAL_OK) { break; }

            /* Set FIFO threshold to default value */
            ret = tsl2585_set_fifo_threshold(handle->hi2c, 255);
            if (ret != HAL_OK) { break; }

            /* Enable sensor ALS interrupt */
            ret = tsl2585_set_interrupt_enable(handle->hi2c, TSL2585_INTENAB_AIEN);
            if (ret != HAL_OK) { break; }
        }

        if (fast_mode) {
            /* Set the FT260 I2C clock speed to 1MHz for faster FIFO reads */
            if (usbh_ft260_set_i2c_clock_speed(handle->device_handle, 1000) != osOK) {
                log_w("Unable to set I2C clock speed");
                ret = HAL_ERROR;
                break;
            }
        }

        /* Enable the sensor (ALS Enable and Power ON) */
        ret = tsl2585_enable(handle->hi2c);
        if (ret != HAL_OK) {
            break;
        }

        handle->sensor_state.sai_active = false;
        handle->sensor_state.agc_disabled_reset_gain = false;
        handle->sensor_state.discard_next_reading = false;
        handle->sensor_state.single_shot = single_shot;
        handle->sensor_state.fast_mode = fast_mode;
        handle->sensor_state.running = true;
        handle->probe_state = METER_PROBE_STATE_RUNNING;
    } while (0);

    return hal_to_os_status(ret);
}

osStatus_t meter_probe_sensor_disable(meter_probe_handle_t *handle)
{
    if (!handle) { return osErrorParameter; }
    if (handle->probe_state < METER_PROBE_STATE_STARTED || !handle->sensor_state.running) { return osErrorResource; }

    osStatus_t result = osOK;
    const meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_SENSOR_DISABLE,
        .result = &result
    };
    osMessageQueuePut(handle->control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(handle->control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_sensor_disable(meter_probe_handle_t *handle)
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("meter_probe_control_sensor_disable");

    do {
        ret = tsl2585_disable(handle->hi2c);
        if (ret != HAL_OK) { break; }

        if (handle->sensor_state.fast_mode) {
            /* Return the FT260 I2C clock speed its default */
            if (usbh_ft260_set_i2c_clock_speed(handle->device_handle, 400) != osOK) {
                log_w("Unable to set I2C clock speed");
            }
        }

        handle->sensor_state.running = false;
        handle->sensor_state.start_mode = METER_PROBE_START_NONE;
        handle->probe_state = METER_PROBE_STATE_STARTED;
    } while (0);

    return hal_to_os_status(ret);
}

osStatus_t meter_probe_sensor_set_gain(meter_probe_handle_t *handle, tsl2585_gain_t gain)
{
    if (!handle) { return osErrorParameter; }
    if (handle->probe_state < METER_PROBE_STATE_STARTED) { return osErrorResource; }

    osStatus_t result = osOK;
    meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_SENSOR_SET_GAIN,
        .result = &result,
        .gain = {
            .gain = gain,
            .mod = TSL2585_MOD0
        }
    };
    osMessageQueuePut(handle->control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(handle->control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_sensor_set_gain(meter_probe_handle_t *handle, const sensor_control_gain_params_t *params)
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

    if (handle->sensor_state.running) {
        ret = tsl2585_set_mod_gain(handle->hi2c, params->mod, TSL2585_STEP0, params->gain);
        if (ret == HAL_OK) {
            handle->sensor_state.gain[mod_index] = params->gain;
        }

        handle->sensor_state.discard_next_reading = true;
        osMessageQueueReset(handle->sensor_reading_queue);
    } else {
        handle->sensor_state.gain[mod_index] = params->gain;
        handle->sensor_state.gain_pending = true;
    }

    return hal_to_os_status(ret);
}

osStatus_t meter_probe_sensor_set_integration(meter_probe_handle_t *handle, uint16_t sample_time, uint16_t sample_count)
{
    if (!handle) { return osErrorParameter; }
    if (handle->probe_state < METER_PROBE_STATE_STARTED) { return osErrorResource; }

    osStatus_t result = osOK;
    const meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_SENSOR_SET_INTEGRATION,
        .result = &result,
        .integration = {
            .sample_time = sample_time,
            .sample_count = sample_count
        }
    };
    osMessageQueuePut(handle->control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(handle->control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_sensor_integration(meter_probe_handle_t *handle, const sensor_control_integration_params_t *params)
{
    HAL_StatusTypeDef ret = HAL_OK;
    log_d("meter_probe_control_sensor_integration: %d, %d", params->sample_time, params->sample_count);

    if (handle->sensor_state.running) {
        ret = tsl2585_set_sample_time(handle->hi2c, params->sample_time);
        if (ret == HAL_OK) {
            handle->sensor_state.sample_time = params->sample_time;
        }

        ret = tsl2585_set_als_num_samples(handle->hi2c, params->sample_count);
        if (ret == HAL_OK) {
            handle->sensor_state.sample_count = params->sample_count;
        }

        handle->sensor_state.discard_next_reading = true;
        osMessageQueueReset(handle->sensor_reading_queue);
    } else {
        handle->sensor_state.sample_time = params->sample_time;
        handle->sensor_state.sample_count = params->sample_count;
        handle->sensor_state.integration_pending = true;
    }

    return hal_to_os_status(ret);
}

osStatus_t meter_probe_sensor_set_mod_calibration(meter_probe_handle_t *handle, uint8_t value)
{
    if (!handle) { return osErrorParameter; }
    if (handle->probe_state < METER_PROBE_STATE_STARTED) { return osErrorResource; }

    osStatus_t result = osOK;
    const meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_SENSOR_SET_MOD_CALIBRATION,
        .result = &result,
        .mod_calibration = {
            .nth_iteration_value = value
        }
    };
    osMessageQueuePut(handle->control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(handle->control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_sensor_set_mod_calibration(meter_probe_handle_t *handle, const sensor_control_mod_cal_params_t *params)
{
    HAL_StatusTypeDef ret = HAL_OK;
    log_d("meter_probe_control_sensor_set_mod_calibration: %d", params->nth_iteration_value);

    if (handle->sensor_state.running) {
        ret = tsl2585_set_calibration_nth_iteration(handle->hi2c, params->nth_iteration_value);
        if (ret == HAL_OK) {
            handle->sensor_state.nth_iteration_value = params->nth_iteration_value;
        }

        handle->sensor_state.discard_next_reading = true;
        osMessageQueueReset(handle->sensor_reading_queue);
    } else {
        handle->sensor_state.nth_iteration_value = params->nth_iteration_value;
        handle->sensor_state.mod_calibration_pending = true;
    }

    return hal_to_os_status(ret);
}

osStatus_t meter_probe_sensor_enable_agc(meter_probe_handle_t *handle, uint16_t sample_count)
{
    if (!handle) { return osErrorParameter; }
    if (handle->probe_state < METER_PROBE_STATE_STARTED) { return osErrorResource; }

    osStatus_t result = osOK;
    const meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_SENSOR_ENABLE_AGC,
        .result = &result,
        .agc = {
            .sample_count = sample_count
        }
    };
    osMessageQueuePut(handle->control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(handle->control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_sensor_enable_agc(meter_probe_handle_t *handle, const sensor_control_agc_params_t *params)
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("meter_probe_control_sensor_enable_agc");

    if (handle->sensor_state.running) {
        ret = tsl2585_set_agc_num_samples(handle->hi2c, params->sample_count);
        if (ret == HAL_OK) {
            handle->sensor_state.agc_sample_count = params->sample_count;
        }

        ret = tsl2585_set_agc_calibration(handle->hi2c, true);
        if (ret == HAL_OK) {
            handle->sensor_state.agc_enabled = true;
        }
    } else {
        handle->sensor_state.agc_enabled = true;
        handle->sensor_state.agc_sample_count = params->sample_count;
        handle->sensor_state.agc_pending = true;
    }

    return hal_to_os_status(ret);
}

osStatus_t meter_probe_sensor_disable_agc(meter_probe_handle_t *handle)
{
    if (!handle) { return osErrorParameter; }
    if (handle->probe_state < METER_PROBE_STATE_STARTED) { return osErrorResource; }

    osStatus_t result = osOK;
    meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_SENSOR_DISABLE_AGC,
        .result = &result
    };
    osMessageQueuePut(handle->control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(handle->control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_sensor_disable_agc(meter_probe_handle_t *handle)
{
    HAL_StatusTypeDef ret = HAL_OK;

    log_d("meter_probe_control_sensor_disable_agc");

    if (handle->sensor_state.running) {
        do {
            ret = tsl2585_set_agc_calibration(handle->hi2c, false);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_set_agc_num_samples(handle->hi2c, 0);
            if (ret != HAL_OK) { break; }
        } while (0);
        if (ret == HAL_OK) {
            handle->sensor_state.agc_enabled = false;
            handle->sensor_state.agc_disabled_reset_gain = true;
            handle->sensor_state.discard_next_reading = true;
        }
    } else {
        handle->sensor_state.agc_enabled = false;
        handle->sensor_state.agc_pending = true;
    }

    return hal_to_os_status(ret);
}

osStatus_t densistick_set_light_enable(meter_probe_handle_t *handle, bool enable)
{
    if (!handle) { return osErrorParameter; }
    if (handle->device_type != METER_PROBE_DEVICE_DENSISTICK) { return osErrorParameter; }
    if (handle->probe_state < METER_PROBE_STATE_STARTED) { return osErrorResource; }

    osStatus_t result = osOK;
    const meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_STICK_SET_LIGHT_ENABLE,
        .result = &result,
        .value = enable ? 1 : 0
    };
    osMessageQueuePut(handle->control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(handle->control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_set_light_enable(meter_probe_handle_t *handle, bool enable)
{
    osStatus_t ret = HAL_OK;

    log_d("meter_probe_control_set_light_enable: %d", enable);

    ret = usbh_ft260_set_device_gpio(handle->device_handle, enable);
    if (ret == osOK) {
        handle->stick_light_enabled = enable;
    }

    return ret;
}

bool densistick_get_light_enable(meter_probe_handle_t *handle)
{
    return handle->stick_light_enabled;
}

osStatus_t densistick_set_light_brightness(meter_probe_handle_t *handle, uint8_t value)
{
    if (!handle) { return osErrorParameter; }
    if (handle->device_type != METER_PROBE_DEVICE_DENSISTICK) { return osErrorParameter; }
    if (handle->probe_state < METER_PROBE_STATE_STARTED) { return osErrorResource; }

    osStatus_t result = osOK;
    const meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_STICK_SET_LIGHT_VALUE,
        .result = &result,
        .value = (value > 127) ? 127 : value
    };
    osMessageQueuePut(handle->control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(handle->control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_set_light_value(meter_probe_handle_t *handle, uint8_t value)
{
    HAL_StatusTypeDef ret = HAL_OK;

    /* log_d("meter_probe_control_set_light_value: %d", value); */

    ret = i2c_transmit(handle->hi2c, MCP4017_ADDRESS, &value, 1, HAL_MAX_DELAY);
    if (ret == HAL_OK) {
        handle->stick_light_brightness = value;
    }

    return hal_to_os_status(ret);
}

uint8_t densistick_get_light_brightness(const meter_probe_handle_t *handle)
{
    if (!handle) { return 0; }
    if (handle->device_type != METER_PROBE_DEVICE_DENSISTICK) { return 0; }
    return handle->stick_light_brightness;
}

osStatus_t meter_probe_sensor_trigger_next_reading(meter_probe_handle_t *handle)
{
    if (!handle) { return osErrorParameter; }
    if (handle->probe_state < METER_PROBE_STATE_STARTED || !handle->sensor_state.running) { return osErrorResource; }

    if (!handle->sensor_state.single_shot) {
        log_e("Not in single-shot mode");
        return osErrorResource;
    }

    osStatus_t result = osOK;
    meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_SENSOR_TRIGGER_NEXT_READING,
        .result = &result
    };
    osMessageQueuePut(handle->control_queue, &control_event, 0, portMAX_DELAY);
    osSemaphoreAcquire(handle->control_semaphore, portMAX_DELAY);
    return result;
}

osStatus_t meter_probe_control_sensor_trigger_next_reading(meter_probe_handle_t *handle)
{
    HAL_StatusTypeDef ret = HAL_OK;
    log_d("meter_probe_control_sensor_trigger_next_reading");

    if (!handle->sensor_state.sai_active) {
        uint8_t status4 = 0;
        tsl2585_get_status4(handle->hi2c, &status4);
        if ((status4 & TSL2585_STATUS4_SAI_ACTIVE) != 0) {
            handle->sensor_state.sai_active = true;
        } else {
            log_e("Integration cycle in progress");
            return osErrorResource;
        }
    }

    ret = tsl2585_clear_sleep_after_interrupt(handle->hi2c);
    if (ret == HAL_OK) {
        handle->sensor_state.sai_active = false;
    }

    return hal_to_os_status(ret);
}

osStatus_t meter_probe_sensor_clear_last_reading(meter_probe_handle_t *handle)
{
    if (!handle) { return osErrorParameter; }
    return osMessageQueueReset(handle->sensor_reading_queue);
}

osStatus_t meter_probe_sensor_get_next_reading(meter_probe_handle_t *handle, meter_probe_sensor_reading_t *reading, uint32_t timeout)
{
    if (!handle) { return osErrorParameter; }
    if (handle->probe_state < METER_PROBE_STATE_STARTED || !handle->sensor_state.running) { return osErrorResource; }

    if (!reading) {
        return osErrorParameter;
    }

    return osMessageQueueGet(handle->sensor_reading_queue, reading, NULL, timeout);
}

meter_probe_result_t meter_probe_measure(meter_probe_handle_t *handle, float *lux)
{
    meter_probe_result_t result = METER_READING_OK;
    const int max_count = 10;
    osStatus_t ret = osOK;
    meter_probe_sensor_reading_t reading;
    int count = 0;
    float reading_lux = NAN;
    bool has_result = false;

    if (!handle || !lux) {
        return METER_READING_FAIL;
    }

    if (handle->device_type != METER_PROBE_DEVICE_METER_PROBE) { return METER_READING_FAIL; }

    //TODO Implement some looping for value averaging and waiting for AGC to settle
    do {
        ret = meter_probe_sensor_get_next_reading(handle, &reading, 500);
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
            reading_lux = meter_probe_lux_result(handle, &reading);
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

meter_probe_result_t meter_probe_try_measure(meter_probe_handle_t *handle, float *lux)
{
    meter_probe_result_t result = METER_READING_OK;
    osStatus_t ret = osOK;
    meter_probe_sensor_reading_t reading;
    float reading_lux = NAN;

    if (!handle || !lux) {
        return METER_READING_FAIL;
    }

    if (handle->device_type != METER_PROBE_DEVICE_METER_PROBE) { return METER_READING_FAIL; }

    do {
        ret = meter_probe_sensor_get_next_reading(handle, &reading, 0);
        if (ret != osOK) {
            result = METER_READING_FAIL;
            break;
        }

        if (reading.reading[0].status != METER_SENSOR_RESULT_VALID) {
            result = METER_READING_FAIL;
            break;
        }

        reading_lux = meter_probe_lux_result(handle, &reading);
        if (!isnormal(reading_lux)) {
            result = METER_READING_FAIL;
        } else if (reading_lux < 0.0001F) {
            result = METER_READING_LOW;
        } else {
            result = METER_READING_OK;
        }
    } while (0);

    if (result == METER_READING_OK) {
        *lux = reading_lux;
    } else {
        *lux = NAN;
    }
    return result;
}

meter_probe_result_t densistick_measure(meter_probe_handle_t *handle, float *density, float *raw_reading)
{
    meter_probe_result_t result = METER_READING_OK;
    osStatus_t ret = osOK;
    meter_probe_sensor_reading_t reading;
    int agc_step;
    int invalid_count;
    int reading_count;
    float reading_sum;

    if (!handle) {
        return METER_READING_FAIL;
    }

    if (handle->device_type != METER_PROBE_DEVICE_DENSISTICK) { return METER_READING_FAIL; }

    do {
        /* Make sure sensor starts in a disabled state */
        if (handle->probe_state == METER_PROBE_STATE_RUNNING) {
            ret = meter_probe_sensor_disable(handle);
            if (ret != osOK) { break; }
        }

        /* Make sure light is disabled */
        ret = densistick_set_light_enable(handle, false);
        if (ret != osOK) { break; }

        /* Configure light for full power */
        ret = densistick_set_light_brightness(handle, 0);
        if (ret != osOK) { break; }

        /* Configure initial sensor settings */
        ret = meter_probe_sensor_set_gain(handle, TSL2585_GAIN_256X);
        if (ret != osOK) { break; }

        ret = meter_probe_sensor_set_integration(handle, 719, 29);
        if (ret != osOK) { break; }

        ret = meter_probe_sensor_enable_agc(handle, 19);
        if (ret != osOK) { break; }

        /* Activate light at full power */
        ret = densistick_set_light_enable(handle, true);
        if (ret != osOK) { break; }

        /* Start the sensor */
        ret = meter_probe_sensor_enable(handle);
        if (ret != osOK) { break; }

        agc_step = 1;
        invalid_count = 0;
        reading_count = 0;
        reading_sum = 0;
        do {
            ret = meter_probe_sensor_get_next_reading(handle, &reading, 500);
            if (ret == osErrorTimeout) {
                result = METER_READING_TIMEOUT;
                break;
            } else if (ret != osOK) {
                result = METER_READING_FAIL;
                break;
            }

            /* Make sure the reading is valid */
            if (reading.reading[0].status != METER_SENSOR_RESULT_VALID) {
                invalid_count++;
                if (invalid_count > 5) {
                    result = METER_READING_TIMEOUT;
                    break;
                } else {
                    continue;
                }
            }

            /* Handle the process of moving from AGC to measurement */
            if (agc_step == 1) {
                /* Disable AGC */
                ret = meter_probe_sensor_disable_agc(handle);
                if (ret != osOK) { break; }
                agc_step++;
                continue;
            } else if (agc_step == 2) {
                /* Set measurement sample time */
                ret = meter_probe_sensor_set_integration(handle, 719, 199);
                if (ret != osOK) { break; }
                agc_step = 0;
                continue;
            }

            /* Collect the measurement */
            float basic_reading = meter_probe_basic_result(handle, &reading);
            reading_sum += basic_reading;
            reading_count++;
        } while (reading_count < 2);
        if (ret != osOK) { break; }

        float avg_reading = reading_sum / (float)reading_count;

        log_d("Raw reading: %f", avg_reading);

        const densistick_settings_tsl2585_cal_target_t *cal_target = &handle->stick_settings.cal_target;

        /* Convert all values into log units */
        float meas_ll = log10f(avg_reading);
        float cal_hi_ll = log10f(cal_target->hi_reading);
        float cal_lo_ll = log10f(cal_target->lo_reading);

        /* Calculate the slope of the line */
        float m = (cal_target->hi_density - cal_target->lo_density) / (cal_hi_ll - cal_lo_ll);

        /* Calculate the measured density */
        float meas_d = (m * (meas_ll - cal_lo_ll)) + cal_target->lo_density;

        log_d("Target density: %f", meas_d);

        if (density) {
            *density = meas_d;
        }

        if (raw_reading) {
            *raw_reading = avg_reading;
        }
    } while (0);

    /* End the cycle by disabling the light and sensor */
    densistick_set_light_enable(handle, false);
    meter_probe_sensor_disable(handle);

    /* Fix any missed errors */
    if (ret != osOK && result == METER_READING_OK) {
        result = METER_READING_FAIL;
    }

    return result;
}

float meter_probe_basic_result(const meter_probe_handle_t *handle, const meter_probe_sensor_reading_t *sensor_reading)
{
    if (!handle || !sensor_reading) { return NAN; }

    const float atime_ms = tsl2585_integration_time_ms(sensor_reading->sample_time, sensor_reading->sample_count);

    float als_gain;
    if (sensor_reading->reading[0].gain <= TSL2585_GAIN_256X) {
        if (handle->device_type == METER_PROBE_DEVICE_METER_PROBE) {
            als_gain = handle->probe_settings.cal_gain.values[sensor_reading->reading[0].gain];
        } else {
            als_gain = handle->stick_settings.cal_gain.values[sensor_reading->reading[0].gain];
        }
    } else {
        als_gain = tsl2585_gain_value(sensor_reading->reading[0].gain);
    }

    if (!is_valid_number(atime_ms) || !is_valid_number(als_gain)) { return NAN; }

    /* Divide to get numbers in a similar range as previous sensors */
    float als_reading = (float)sensor_reading->reading[0].data / 16.0F;

    /* Calculate the basic reading */
    float basic_reading = als_reading / (atime_ms * als_gain);

    /* Slope correction is not used for the DensiStick, just the Meter Probe */
    if (handle->device_type == METER_PROBE_DEVICE_METER_PROBE) {
        /* Apply the slope correction */
        const meter_probe_settings_tsl2585_cal_slope_t *cal_slope = &handle->probe_settings.cal_slope;
        float l_reading = log10f(basic_reading);
        float l_expected = cal_slope->b0 + (cal_slope->b1 * l_reading) + (cal_slope->b2 * powf(l_reading, 2.0F));
        float corr_reading = powf(10.0F, l_expected);
        return corr_reading;
    } else {
        return basic_reading;
    }
}

float meter_probe_lux_result(const meter_probe_handle_t *handle, const meter_probe_sensor_reading_t *sensor_reading)
{
    if (!handle || !sensor_reading) { return NAN; }
    if (sensor_reading->reading[0].gain >= TSL2585_GAIN_MAX) { return NAN; }
    if (!handle->has_sensor_settings) { return NAN; }
    if (handle->device_type != METER_PROBE_DEVICE_METER_PROBE) { return NAN; }

    const float lux_slope = handle->probe_settings.cal_target.lux_slope;
    const float lux_intercept = handle->probe_settings.cal_target.lux_intercept;
    if (!is_valid_number(lux_slope) || !is_valid_number(lux_intercept)) { return NAN; }

    const float basic_value = meter_probe_basic_result(handle, sensor_reading);
    if (!is_valid_number(basic_value)) { return NAN; }

    float lux = (basic_value * lux_slope) + lux_intercept;

    /* Prevent negative results */
    if (lux < 0.0F) {
        lux = 0.0F;
    }

    return lux;
}

void meter_probe_int_handler(meter_probe_handle_t *handle, uint32_t ticks)
{
    if (!handle || handle->probe_state < METER_PROBE_STATE_STARTED || !handle->sensor_state.running) { return; }

    const meter_probe_control_event_t control_event = {
        .event_type = METER_PROBE_CONTROL_INTERRUPT,
        .interrupt = {
            .ticks = ticks
        }
    };

    osMessageQueuePut(handle->control_queue, &control_event, 0, 0);
}

osStatus_t meter_probe_control_interrupt(meter_probe_handle_t *handle, const sensor_control_interrupt_params_t *params)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint8_t status = 0;
    meter_probe_sensor_reading_t sensor_reading = {0};
    bool has_reading = false;

#if 0
    log_d("meter_probe_control_interrupt");
#endif

    if (handle->probe_state < METER_PROBE_STATE_STARTED || !handle->sensor_state.running) {
        log_d("Unexpected meter probe interrupt");
    }

    do {
        /* Get the interrupt status */
        ret = tsl2585_get_status(handle->hi2c, &status);
        if (ret != HAL_OK) { break; }

#if 0
        /* Log interrupt flags */
        log_d("MINT=%d, AINT=%d, FINT=%d, SINT=%d",
            (status & TSL2585_STATUS_MINT) != 0,
            (status & TSL2585_STATUS_AINT) != 0,
            (status & TSL2585_STATUS_FINT) != 0,
            (status & TSL2585_STATUS_SINT) != 0);
#endif

        if ((!handle->sensor_state.fast_mode && (status & TSL2585_STATUS_AINT) != 0) || (handle->sensor_state.fast_mode && (status & TSL2585_STATUS_FINT) != 0)) {
            uint32_t elapsed_ticks = params->ticks - handle->last_aint_ticks;
            handle->last_aint_ticks = params->ticks;

            tsl2585_fifo_data_t fifo_data[MAX_ALS_COUNT];
            bool overflow = false;

            if (handle->sensor_state.fast_mode) {
                ret = sensor_control_read_fifo_fast_mode(handle, fifo_data, &overflow, params->ticks);
            } else {
                ret = sensor_control_read_fifo(handle, &fifo_data[0], &overflow);
            }
            if (ret != HAL_OK) { break; }

            /* FIFO reads implicitly clear the FINT bit */
            if (handle->sensor_state.fast_mode) {
                status &= ~TSL2585_STATUS_FINT;
            }

            if (handle->sensor_state.discard_next_reading) {
                handle->sensor_state.discard_next_reading = false;
            } else {
                if (overflow) {
                    for (uint8_t i = 0; i < (uint8_t) (handle->sensor_state.fast_mode ? MAX_ALS_COUNT : 1); i++) {
                        sensor_reading.reading[i].data = UINT32_MAX;
                        sensor_reading.reading[i].gain = handle->sensor_state.gain[i];
                        sensor_reading.reading[i].status = METER_SENSOR_RESULT_INVALID;
                    }
                } else {
                    for (uint8_t i = 0; i < (uint8_t) (handle->sensor_state.fast_mode ? MAX_ALS_COUNT : 1); i++) {
                        if ((fifo_data[i].als_status & TSL2585_ALS_DATA0_ANALOG_SATURATION_STATUS) != 0) {
#if 0
                            log_d("TSL2585: [analog saturation]");
#endif
                            sensor_reading.reading[i].data = UINT32_MAX;
                            sensor_reading.reading[i].gain = handle->sensor_state.gain[i];
                            sensor_reading.reading[i].status = METER_SENSOR_RESULT_SATURATED_ANALOG;
                        } else {
                            tsl2585_gain_t als_gain = (fifo_data[i].als_status2 & 0x0F);

                            /* If AGC is enabled, then update the configured gain value */
                            if (handle->sensor_state.agc_enabled) {
                                handle->sensor_state.gain[0] = als_gain;
                            }

                            sensor_reading.reading[i].data = fifo_data[i].als_data0;
                            sensor_reading.reading[i].gain = als_gain;
                            sensor_reading.reading[i].status = METER_SENSOR_RESULT_VALID;
                        }
                    }
                }

                /* Fill out other reading fields */
                sensor_reading.sample_time = handle->sensor_state.sample_time;
                sensor_reading.sample_count = handle->sensor_state.sample_count;
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
            if (handle->sensor_state.agc_disabled_reset_gain) {
                ret = tsl2585_set_mod_gain(handle->hi2c, TSL2585_MOD0, TSL2585_STEP0, handle->sensor_state.gain[0]);
                if (ret != HAL_OK) { break; }
                handle->sensor_state.agc_disabled_reset_gain = false;
                handle->sensor_state.discard_next_reading = true;
            }
        }

        if (status != 0) {
            /* Clear the interrupt status */
            ret = tsl2585_set_status(handle->hi2c, status);
            if (ret != HAL_OK) { break; }
        }

        /* Check single-shot specific state */
        if (handle->sensor_state.single_shot) {
            uint8_t status4 = 0;
            ret = tsl2585_get_status4(handle->hi2c, &status4);
            if (ret != HAL_OK) { break; }
            if ((status4 & TSL2585_STATUS4_SAI_ACTIVE) != 0) {
#if 0
                log_d("Sleep after interrupt");
#endif
                handle->sensor_state.sai_active = true;
            }
        }
    } while (0);

    if (has_reading) {
#if 0
        log_d("TSL2585: MOD0=%lu, Gain=[%s], Time=%.2fms",
            reading.als_data, tsl2585_gain_str(reading.gain),
            tsl2585_integration_time_ms(sensor_state.sample_time, sensor_state.sample_count));
#endif

        QueueHandle_t queue = (QueueHandle_t)handle->sensor_reading_queue;
        xQueueOverwrite(queue, &sensor_reading);
    }

    return hal_to_os_status(ret);
}

HAL_StatusTypeDef sensor_control_read_fifo(meter_probe_handle_t *handle, tsl2585_fifo_data_t *fifo_data, bool *overflow)
{
    HAL_StatusTypeDef ret;
    tsl2585_fifo_status_t fifo_status;
    uint8_t data[FIFO_ALS_ENTRY_SIZE];
    uint8_t counter = 0;
    const uint8_t data_size = FIFO_ALS_ENTRY_SIZE;

    do {
        ret = tsl2585_get_fifo_status(handle->hi2c, &fifo_status);
        if (ret != HAL_OK) { break; }

#if 0
        log_d("FIFO_STATUS: OVERFLOW=%d, UNDERFLOW=%d, LEVEL=%d", fifo_status.overflow, fifo_status.underflow, fifo_status.level);
#endif

        if (fifo_status.overflow) {
            log_w("FIFO overflow, clearing");
            ret = tsl2585_clear_fifo(handle->hi2c);
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
            ret = tsl2585_read_fifo(handle->hi2c, data, data_size);
            if (ret != HAL_OK) { break; }

            ret = tsl2585_get_fifo_status(handle->hi2c, &fifo_status);
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

HAL_StatusTypeDef sensor_control_read_fifo_fast_mode(meter_probe_handle_t *handle, tsl2585_fifo_data_t *fifo_data, bool *overflow, uint32_t ticks)
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
        ret = tsl2585_read_fifo_combo(handle->hi2c, &fifo_status, data, sizeof(data));
        if (ret != HAL_OK) { break; }

#if 0
        const uint32_t time_read = osKernelGetTickCount() - ticks;
        log_d("FIFO_STATUS: OVERFLOW=%d, UNDERFLOW=%d, LEVEL=%d", fifo_status.overflow, fifo_status.underflow, fifo_status.level);
        log_d("tt_process=%dms, tt_read=%dms", time_elapsed, time_read - time_elapsed);
#endif

        if (fifo_status.overflow) {
            log_w("FIFO overflow, clearing");
            ret = tsl2585_clear_fifo(handle->hi2c);
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
                ret = tsl2585_clear_fifo(handle->hi2c);
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
