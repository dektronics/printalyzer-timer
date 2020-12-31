#ifndef STATE_HOME_H
#define STATE_HOME_H

#include <stdbool.h>

#include "state_controller.h"

typedef struct {
    bool change_inc_pending;
    bool change_inc_swallow_release_up;
    bool change_inc_swallow_release_down;
    int encoder_repeat;
    int cancel_repeat;
    bool display_dirty;
} state_home_data_t;

void state_home_init(state_home_data_t *state_data);
state_identifier_t state_home(state_home_data_t *state_data);

state_identifier_t state_home_change_time_increment();
state_identifier_t state_home_adjust_fine(bool state_changed);
state_identifier_t state_home_adjust_absolute(bool state_changed);

#endif /* STATE_HOME_H */
