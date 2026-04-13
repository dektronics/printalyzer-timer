#include "buzzer.h"

#include <string.h>
#include <cmsis_os.h>
#include <semphr.h>

#define LOG_TAG "buzzer"
#include <elog.h>

#include "pam8904e.h"
#include "settings.h"

typedef enum : uint8_t {
    BUZZER_START = 0,
    BUZZER_STOP,
    BUZZER_FREQ,
    BUZZER_DELAY,
    BUZZER_END
} buzzer_sequence_action_t;

typedef struct {
    buzzer_sequence_action_t action;
    uint16_t param;
} buzzer_sequence_element_t;

static const buzzer_sequence_element_t SEQ_PROBE_START[] = {
    { BUZZER_FREQ,  2000 },
    { BUZZER_START, 0 },
    { BUZZER_DELAY, 25 },
    { BUZZER_FREQ,  1500 },
    { BUZZER_DELAY, 25 },
    { BUZZER_FREQ,  1250 },
    { BUZZER_DELAY, 25 },
    { BUZZER_FREQ,  1000 },
    { BUZZER_DELAY, 25 },
    { BUZZER_STOP,  0 },
    { BUZZER_END,   0 },
};

static const buzzer_sequence_element_t SEQ_STICK_START[] = {
    { BUZZER_FREQ,  1500 },
    { BUZZER_START, 0 },
    { BUZZER_DELAY, 25 },
    { BUZZER_FREQ,  1000 },
    { BUZZER_DELAY, 25 },
    { BUZZER_FREQ,  750 },
    { BUZZER_DELAY, 50 },
    { BUZZER_FREQ,  500 },
    { BUZZER_DELAY, 50 },
    { BUZZER_STOP,  0 },
    { BUZZER_END,   0 },
};

static const buzzer_sequence_element_t SEQ_PROBE_SUCCESS[] = {
    { BUZZER_FREQ,  1000 },
    { BUZZER_START, 0 },
    { BUZZER_DELAY, 50 },
    { BUZZER_STOP,  0 },
    { BUZZER_DELAY, 100 },
    { BUZZER_FREQ,  1500 },
    { BUZZER_START, 0 },
    { BUZZER_DELAY, 50 },
    { BUZZER_STOP,  0 },
    { BUZZER_END,   0 },
};

static const buzzer_sequence_element_t SEQ_STICK_SUCCESS[] = {
    { BUZZER_FREQ,  750 },
    { BUZZER_START, 0 },
    { BUZZER_DELAY, 50 },
    { BUZZER_STOP,  0 },
    { BUZZER_DELAY, 100 },
    { BUZZER_FREQ,  1000 },
    { BUZZER_START, 0 },
    { BUZZER_DELAY, 50 },
    { BUZZER_STOP,  0 },
    { BUZZER_END,   0 },
};

static const buzzer_sequence_element_t SEQ_PROBE_WARNING[] = {
    { BUZZER_FREQ,  2000 },
    { BUZZER_START, 0 },
    { BUZZER_DELAY, 50 },
    { BUZZER_STOP,  0 },
    { BUZZER_DELAY, 100 },
    { BUZZER_START, 0 },
    { BUZZER_DELAY, 50 },
    { BUZZER_STOP,  0 },
    { BUZZER_END,   0 },
};

static const buzzer_sequence_element_t SEQ_PROBE_ERROR[] = {
    { BUZZER_FREQ,  500 },
    { BUZZER_START, 0 },
    { BUZZER_DELAY, 100 },
    { BUZZER_STOP,  0 },
    { BUZZER_DELAY, 100 },
    { BUZZER_START, 0 },
    { BUZZER_DELAY, 100 },
    { BUZZER_STOP,  0 },
    { BUZZER_END,   0 },
};

static const buzzer_sequence_element_t SEQ_ABORT_EXPOSURE[] = {
    { BUZZER_FREQ,  1000 },
    { BUZZER_START, 0 },
    { BUZZER_DELAY, 100 },
    { BUZZER_STOP,  0 },
    { BUZZER_DELAY, 100 },
    { BUZZER_START, 0 },
    { BUZZER_DELAY, 100 },
    { BUZZER_STOP,  0 },
    { BUZZER_END,   0 },
};

static const buzzer_sequence_element_t SEQ_EXPOSURE_END_SHORT[] = {
    { BUZZER_FREQ,  1000 },
    { BUZZER_START, 0 },
    { BUZZER_DELAY, 50 },
    { BUZZER_FREQ,  2000 },
    { BUZZER_DELAY, 50 },
    { BUZZER_FREQ,  1500 },
    { BUZZER_DELAY, 50 },
    { BUZZER_STOP,  0 },
    { BUZZER_END,   0 },
};

static const buzzer_sequence_element_t SEQ_EXPOSURE_END_REGULAR[] = {
    { BUZZER_FREQ,  1000 },
    { BUZZER_START, 0 },
    { BUZZER_DELAY, 120 },
    { BUZZER_FREQ,  2000 },
    { BUZZER_DELAY, 120 },
    { BUZZER_FREQ,  1500 },
    { BUZZER_DELAY, 120 },
    { BUZZER_STOP,  0 },
    { BUZZER_END,   0 },
};

static const buzzer_sequence_element_t * const BUZZER_SEQ_LIST[] = {
    [BUZZER_SEQUENCE_PROBE_START]          = SEQ_PROBE_START,
    [BUZZER_SEQUENCE_STICK_START]          = SEQ_STICK_START,
    [BUZZER_SEQUENCE_PROBE_SUCCESS]        = SEQ_PROBE_SUCCESS,
    [BUZZER_SEQUENCE_STICK_SUCCESS]        = SEQ_STICK_SUCCESS,
    [BUZZER_SEQUENCE_PROBE_WARNING]        = SEQ_PROBE_WARNING,
    [BUZZER_SEQUENCE_PROBE_ERROR]          = SEQ_PROBE_ERROR,
    [BUZZER_SEQUENCE_ABORT_EXPOSURE]       = SEQ_ABORT_EXPOSURE,
    [BUZZER_SEQUENCE_EXPOSURE_END_SHORT]   = SEQ_EXPOSURE_END_SHORT,
    [BUZZER_SEQUENCE_EXPOSURE_END_REGULAR] = SEQ_EXPOSURE_END_REGULAR
};

typedef struct {
    uint16_t freq;
    uint16_t duration;
} buzzer_beep_t;

typedef enum : uint8_t {
    BUZZER_CONTROL_ENABLE = 0,
    BUZZER_CONTROL_PLAY_BEEP,
    BUZZER_CONTROL_PLAY_SEQUENCE
} buzzer_control_event_type_t;

typedef struct {
    buzzer_control_event_type_t event_type;
    SemaphoreHandle_t event_semaphore;
    union {
        bool enable;
        const buzzer_beep_t beep;
        const buzzer_sequence_element_t *sequence;
    };
} buzzer_control_event_t;

static pam8904e_handle_t buzzer_handle = {0};
static buzzer_volume_t buzzer_volume = 0;
static uint16_t buzzer_frequency = 0;

/* Flag to track task initialization state */
static bool buzzer_task_started = false;

/* Flag to track whether task playback is enabled */
static bool buzzer_task_enabled = true;

/* Queue for buzzer requests */
static osMessageQueueId_t buzzer_queue = nullptr;
static const osMessageQueueAttr_t buzzer_queue_attrs = {
    .name = "buzzer_queue"
};

static void buzzer_task_loop();

static void buzzer_beep_impl(uint16_t freq, uint16_t duration, bool blocking);
static void buzzer_sequence_impl(buzzer_sequence_t sequence, bool blocking);

static void buzzer_queue_event(buzzer_control_event_t *event, bool blocking);

static void buzzer_play_beep(uint16_t freq, uint16_t duration);
static void buzzer_play_sequence(const buzzer_sequence_element_t *sequence);

HAL_StatusTypeDef buzzer_init(const pam8904e_handle_t *handle)
{
    log_d("buzzer_init");

    if (!handle) {
        log_e("Buzzer handle not initialized");
        return HAL_ERROR;
    }

    memcpy(&buzzer_handle, handle, sizeof(pam8904e_handle_t));

    pam8904e_set_gain(&buzzer_handle, PAM8904E_GAIN_SHUTDOWN);
    pam8904e_stop(&buzzer_handle);
    buzzer_volume = BUZZER_VOLUME_OFF;
    buzzer_frequency = PAM8904E_FREQ_DEFAULT;

    return HAL_OK;
}

void task_buzzer_run(void *argument)
{
    osSemaphoreId_t task_start_semaphore = argument;

    /* Create the queue for DMX task control events */
    buzzer_queue = osMessageQueueNew(5, sizeof(buzzer_control_event_t), &buzzer_queue_attrs);
    if (!buzzer_queue) {
        return;
    }

    buzzer_reset_volume();

    /* Set the initialized flag */
    buzzer_task_started = true;

    /* Release the startup semaphore */
    if (osSemaphoreRelease(task_start_semaphore) != osOK) {
        log_e("Unable to release task_start_semaphore");
        return;
    }

    /* Start the main task loop */
    buzzer_task_loop();
}

[[noreturn]] void buzzer_task_loop()
{
    buzzer_control_event_t buzzer_event = {};

    for (;;) {
        if(osMessageQueueGet(buzzer_queue, &buzzer_event, NULL, portMAX_DELAY) == osOK) {
            if (buzzer_event.event_type == BUZZER_CONTROL_ENABLE) {
                buzzer_task_enabled = buzzer_event.enable;
                buzzer_stop();
            } else if (buzzer_event.event_type == BUZZER_CONTROL_PLAY_BEEP) {
                buzzer_play_beep(buzzer_event.beep.freq, buzzer_event.beep.duration);
            } else if (buzzer_event.event_type == BUZZER_CONTROL_PLAY_SEQUENCE) {
                buzzer_play_sequence(buzzer_event.sequence);
            }

            if (buzzer_event.event_semaphore) {
                xSemaphoreGive(buzzer_event.event_semaphore);
            }
        }
    }
}

void buzzer_set_volume(buzzer_volume_t volume)
{
    switch(volume) {
    case BUZZER_VOLUME_LOW:
        pam8904e_set_gain(&buzzer_handle, PAM8904E_GAIN_1X);
        break;
    case BUZZER_VOLUME_MEDIUM:
        pam8904e_set_gain(&buzzer_handle, PAM8904E_GAIN_2X);
        break;
    case BUZZER_VOLUME_HIGH:
        pam8904e_set_gain(&buzzer_handle, PAM8904E_GAIN_3X);
        break;
    case BUZZER_VOLUME_OFF:
        pam8904e_set_gain(&buzzer_handle, PAM8904E_GAIN_SHUTDOWN);
        break;
    default:
        return;
    }
    buzzer_volume = volume;
}

buzzer_volume_t buzzer_get_volume()
{
    return buzzer_volume;
}

void buzzer_reset_volume()
{
    buzzer_set_volume(settings_get_buzzer_volume());
}

void buzzer_set_frequency(uint16_t freq)
{
    pam8904e_set_frequency(&buzzer_handle, freq);
    buzzer_frequency = freq;
}

uint16_t buzzer_get_frequency()
{
    return buzzer_frequency;
}

void buzzer_start()
{
    pam8904e_start(&buzzer_handle);
}

void buzzer_stop()
{
    pam8904e_stop(&buzzer_handle);
}

void buzzer_task_enable(bool enable)
{
    if (!buzzer_task_started) { return; }

    buzzer_control_event_t event = {
        .event_type = BUZZER_CONTROL_ENABLE,
        .enable = enable
    };

    buzzer_queue_event(&event, true);
}

void buzzer_beep(uint16_t freq, uint16_t duration)
{
    buzzer_beep_impl(freq, duration, false);
}

void buzzer_beep_blocking(uint16_t freq, uint16_t duration)
{
    buzzer_beep_impl(freq, duration, true);
}

void buzzer_beep_impl(uint16_t freq, uint16_t duration, bool blocking)
{
    if (buzzer_task_started) {
        buzzer_control_event_t event = {
            .event_type = BUZZER_CONTROL_PLAY_BEEP,
            .beep = { freq, duration },
        };
        buzzer_queue_event(&event, blocking);
    } else {
        buzzer_play_beep(freq, duration);
    }
}

void buzzer_sequence(buzzer_sequence_t sequence)
{
    buzzer_sequence_impl(sequence, false);
}

void buzzer_sequence_blocking(buzzer_sequence_t sequence)
{
    buzzer_sequence_impl(sequence, true);
}

void buzzer_sequence_impl(buzzer_sequence_t sequence, bool blocking)
{
    if (buzzer_task_started) {
        buzzer_control_event_t event = {
            .event_type = BUZZER_CONTROL_PLAY_SEQUENCE,
            .sequence = BUZZER_SEQ_LIST[sequence],
        };
        buzzer_queue_event(&event, blocking);
    } else {
        buzzer_play_sequence(BUZZER_SEQ_LIST[sequence]);
    }
}

void buzzer_queue_event(buzzer_control_event_t *event, bool blocking)
{
    if (event->event_type == BUZZER_CONTROL_ENABLE) {
        /* Enable events flush the queue before being added themselves */
        osMessageQueueReset(buzzer_queue);
    } else if (!buzzer_task_enabled) {
        /* All other events are only processed if the buzzer task is enabled */
        return;
    }

    if (blocking) {
        StaticSemaphore_t sem_buffer;
        event->event_semaphore = xSemaphoreCreateBinaryStatic(&sem_buffer);
        if (!event->event_semaphore) {
            return;
        }

        if (osMessageQueuePut(buzzer_queue, event, 0, portMAX_DELAY) == osOK) {
            xSemaphoreTake(event->event_semaphore, portMAX_DELAY);
        }

        vSemaphoreDelete(event->event_semaphore);
    } else {
        osMessageQueuePut(buzzer_queue, event, 0, portMAX_DELAY);
    }
}

void buzzer_play_beep(uint16_t freq, uint16_t duration)
{
    const buzzer_sequence_element_t beep_sequence[] = {
        { BUZZER_FREQ,  freq },
        { BUZZER_START, 0 },
        { BUZZER_DELAY, duration },
        { BUZZER_STOP,  0 },
        { BUZZER_END,   0 }
    };
    buzzer_play_sequence(beep_sequence);
}

void buzzer_play_sequence(const buzzer_sequence_element_t *sequence)
{
    if (!sequence) { return; }

    uint16_t current_frequency = buzzer_get_frequency();

    size_t i = 0;
    while (sequence[i].action != BUZZER_END) {
        switch (sequence[i].action) {
        case BUZZER_START:
            buzzer_start();
            break;
        case BUZZER_STOP:
            buzzer_stop();
            break;
        case BUZZER_FREQ:
            buzzer_set_frequency(sequence[i].param);
            break;
        case BUZZER_DELAY:
            osDelay(pdMS_TO_TICKS(sequence[i].param));
            break;
        default:
            break;
        }
        i++;
    }

    buzzer_set_frequency(current_frequency);
}
