#ifndef STATE_TEST_STRIP_H
#define STATE_TEST_STRIP_H

#include "state_controller.h"

#include <stdint.h>
#include <stdbool.h>

state_identifier_t state_test_strip();
bool state_test_strip_countdown(uint32_t patch_time_ms, bool last_patch);

#endif /* STATE_TEST_STRIP_H */
