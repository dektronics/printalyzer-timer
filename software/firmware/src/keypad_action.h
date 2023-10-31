#ifndef KEYPAD_ACTION_H
#define KEYPAD_ACTION_H

#include <cmsis_os.h>

#include "keypad.h"

typedef struct {
    uint8_t action_id;
    keypad_key_t key;
    uint8_t count;
} keypad_action_t;

osStatus_t keypad_action_add(keypad_key_t key, uint8_t action_id, uint8_t long_press_action_id, bool allow_repeat);
osStatus_t keypad_action_add_encoder(uint8_t ccw_action_id, uint8_t cw_action_id);
osStatus_t keypad_action_add_combo(keypad_key_t key1, keypad_key_t key2, uint8_t action_id);
void keypad_action_clear();

osStatus_t keypad_action_wait(keypad_action_t *action, int msecs_to_wait);

#endif /* KEYPAD_ACTION_H */
