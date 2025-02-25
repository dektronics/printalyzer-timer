#ifndef STATE_CONTROLLER_H
#define STATE_CONTROLLER_H

#include <stdbool.h>
#include "enlarger_config.h"
#include "exposure_state.h"

#define STATE_KEYPAD_WAIT 200

typedef enum {
    STATE_HOME = 0,
    STATE_HOME_CHANGE_TIME_INCREMENT,
    STATE_HOME_CHANGE_MODE,
    STATE_HOME_ADJUST_FINE,
    STATE_HOME_ADJUST_ABSOLUTE,
    STATE_DENSITOMETER,
    STATE_TIMER,
    STATE_TEST_STRIP,
    STATE_EDIT_ADJUSTMENT,
    STATE_LIST_ADJUSTMENTS,
    STATE_MENU,
    STATE_MAX
} state_identifier_t;

typedef struct __state_controller_t state_controller_t;

typedef struct __state_t state_t;

typedef void (*state_entry_func_t)(state_t *state, state_controller_t *controller, state_identifier_t prev_state, uint32_t param);
typedef bool (*state_process_func_t)(state_t *state, state_controller_t *controller);
typedef void (*state_exit_func_t)(state_t *state, state_controller_t *controller, state_identifier_t next_state);

struct __state_t {
    /**
     * Function called on entry into the state.
     */
    state_entry_func_t state_entry;

    /**
     * Function called on each processing loop within the state.
     *
     * Returns true if an event was processed, or false if it fell through
     * due to a timeout or non-event. This result is used to schedule
     * inactivity behaviors inside the controller.
     */
    state_process_func_t state_process;

    /**
     * Function called on exit from the state.
     */
    state_exit_func_t state_exit;
};

void state_controller_init();
void state_controller_loop();

void state_controller_set_next_state(state_controller_t *controller, state_identifier_t next_state, uint32_t param);
exposure_state_t *state_controller_get_exposure_state(state_controller_t *controller);
const enlarger_config_t *state_controller_get_enlarger_config(state_controller_t *controller);
void state_controller_set_enlarger_focus(state_controller_t *controller, bool enabled);
bool state_controller_is_enlarger_focus(const state_controller_t *controller);
void state_controller_set_enable_meter_probe(state_controller_t *controller, bool enabled);
bool state_controller_is_enable_meter_probe(const state_controller_t *controller);
void state_controller_reload_enlarger_config(state_controller_t *controller);
void state_controller_reload_paper_profile(state_controller_t *controller, bool use_default);
void state_controller_start_focus_timeout(state_controller_t *controller);
void state_controller_stop_focus_timeout(state_controller_t *controller);

#endif /* STATE_CONTROLLER_H */
