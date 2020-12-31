#ifndef STATE_CONTROLLER_H
#define STATE_CONTROLLER_H

typedef enum {
    STATE_HOME,
    STATE_HOME_CHANGE_TIME_INCREMENT,
    STATE_HOME_ADJUST_FINE,
    STATE_HOME_ADJUST_ABSOLUTE,
    STATE_TIMER,
    STATE_TEST_STRIP,
    STATE_ADD_ADJUSTMENT,
    STATE_MENU
} state_identifier_t;

void state_controller_init();
void state_controller_loop();

#endif /* STATE_CONTROLLER_H */
