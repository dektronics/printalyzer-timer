#ifndef STATE_HOME_H
#define STATE_HOME_H

#include <FreeRTOS.h>
#include <stdbool.h>

#include "state_controller.h"
#include "exposure_state.h"

state_t *state_home();
state_t *state_home_change_time_increment();
state_t *state_home_change_mode();
state_t *state_home_adjust_fine();
state_t *state_home_adjust_absolute();

#endif /* STATE_HOME_H */
