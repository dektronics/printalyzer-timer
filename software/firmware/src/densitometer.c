#include "densitometer.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "usb_host.h"
#include "dens_remote.h"
#include "meter_probe.h"
#include "buzzer.h"

#define LOG_TAG "densitometer"
#include <elog.h>

static bool dens_enable = false;
static densitometer_mode_t dens_mode = DENSITOMETER_MODE_UNKNOWN;

static bool dens_remote_ready = false;
static bool densistick_ready = false;
static bool densistick_idle_light_enabled = false;

static bool has_densistick_reading = false;
static densitometer_reading_t last_densistick_reading = {0};

static void densitometer_stick_set_idle_light();

void densitometer_enable(densitometer_mode_t mode)
{
    meter_probe_handle_t *handle = densistick_handle();

    /* Enable serial-attached densitometer reading and clear the buffer */
    if (usb_serial_is_attached()) {
        if (!dens_remote_ready) {
            usb_serial_clear_receive_buffer();
            dens_remote_ready = true;
        }
    } else {
        dens_remote_ready = false;
    }

    /* Enable the DensiStick if it is attached and not started */
    if (mode == DENSITOMETER_MODE_REFLECTION || mode == DENSITOMETER_MODE_UNKNOWN) {
        if (meter_probe_is_attached(handle)) {
            if (!meter_probe_is_started(handle)) {
                if (meter_probe_start(handle) == osOK) {
                    densistick_ready = true;
                }
            }
        } else {
            densistick_ready = false;
        }
    } else {
        if (meter_probe_is_started(handle)) {
            densistick_set_light_enable(handle, false);
            meter_probe_sensor_disable(handle);
            meter_probe_stop(handle);
        }
        densistick_ready = false;
    }

    if (densistick_ready) {
        densitometer_stick_set_idle_light();
    }

    dens_enable = true;
    dens_mode = mode;
}

void densitometer_idle_light(bool enabled)
{
    densistick_idle_light_enabled = enabled;
    if (densistick_ready) {
        densitometer_stick_set_idle_light();
    }
}

void densitometer_disable()
{
    dens_enable = false;
    dens_mode = DENSITOMETER_MODE_UNKNOWN;
    dens_remote_ready = false;
    has_densistick_reading = false;

    meter_probe_handle_t *handle = densistick_handle();
    if (meter_probe_is_started(handle)) {
        densistick_set_light_enable(handle, false);
        meter_probe_sensor_disable(handle);
        meter_probe_stop(handle);
    }
    densistick_ready = false;
    densistick_idle_light_enabled = true;
}

void densitometer_take_reading()
{
    if (dens_enable && densistick_ready) {
        meter_probe_handle_t *handle = densistick_handle();
        meter_probe_result_t result = METER_READING_OK;
        float density = NAN;
        buzzer_sequence(BUZZER_SEQUENCE_STICK_START);
        result = densistick_measure(handle, &density, NULL);
        buzzer_sequence(BUZZER_SEQUENCE_STICK_SUCCESS);
        if (result == METER_READING_OK) {
            last_densistick_reading.mode = DENSITOMETER_MODE_REFLECTION;
            last_densistick_reading.visual = density;
            has_densistick_reading = true;
        } else {
            buzzer_sequence(BUZZER_SEQUENCE_PROBE_ERROR);
        }

        densitometer_stick_set_idle_light();
    }
}

void densitometer_stick_set_idle_light()
{
    meter_probe_handle_t *handle = densistick_handle();

    if (densistick_idle_light_enabled) {
        if (densistick_get_light_brightness(handle) != 127) {
            densistick_set_light_brightness(handle, 127);
        }
        if (!densistick_get_light_enable(handle)) {
            densistick_set_light_enable(handle, true);
        }
    } else {
        if (densistick_get_light_enable(handle)) {
            densistick_set_light_enable(handle, false);
        }
    }
}

void densitometer_clear_reading()
{
    if (dens_remote_ready) {
        usb_serial_clear_receive_buffer();
    }
    if (has_densistick_reading) {
        has_densistick_reading = false;
    }
}

bool densitometer_get_reading(densitometer_reading_t *reading)
{
    if (!dens_enable) { return false; }

    if (has_densistick_reading) {
        if (reading) {
            memcpy(reading, &last_densistick_reading, sizeof(densitometer_reading_t));
        }
        has_densistick_reading = false;
        return true;
    }

    if (dens_remote_ready) {
        densitometer_reading_t remote_reading;
        if (dens_remote_reading_poll(&remote_reading) == DENS_REMOTE_RESULT_OK) {
            if (reading && (dens_mode == DENSITOMETER_MODE_UNKNOWN
                || remote_reading.mode == DENSITOMETER_MODE_UNKNOWN
                || remote_reading.mode == dens_mode)) {
                memcpy(reading, &remote_reading, sizeof(densitometer_reading_t));
                buzzer_sequence(BUZZER_SEQUENCE_STICK_SUCCESS);
                return true;
            }
        }
    }

    return false;
}

void densitometer_log_reading(const densitometer_reading_t *reading)
{
    if (reading) {
        char mode_ch;
        switch (reading->mode) {
        case DENSITOMETER_MODE_TRANSMISSION:
            mode_ch = 'T';
            break;
        case DENSITOMETER_MODE_REFLECTION:
            mode_ch = 'R';
            break;
        case DENSITOMETER_MODE_UNKNOWN:
        default:
            mode_ch = 'U';
            break;
        }
        log_i("mode=%c, vis=%0.02f, red=%0.02f, green=%0.02f, blue=%0.02f",
            mode_ch, reading->visual, reading->red, reading->green, reading->blue);
    } else {
        log_i("Reading invalid");
    }
}
