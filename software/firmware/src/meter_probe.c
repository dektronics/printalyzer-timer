#include "meter_probe.h"

#include <FreeRTOS.h>
#include <task.h>

#include <math.h>

#define LOG_TAG "meter_probe"
#include <elog.h>

#include "tcs3472.h"
#include "settings.h"

extern I2C_HandleTypeDef hi2c2;

static bool meter_probe_initialized = false;

meter_probe_result_t meter_probe_initialize()
{
    meter_probe_result_t result = METER_READING_OK;
    HAL_StatusTypeDef ret = HAL_OK;
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
        meter_probe_initialized = false;
    } else {
        meter_probe_initialized = true;
    }

    return result;
}

meter_probe_result_t meter_probe_measure(float *lux, uint32_t *cct)
{
    if (!meter_probe_initialized) {
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

void meter_probe_shutdown()
{
    tcs3472_disable(&hi2c2);
    meter_probe_initialized = false;
}
