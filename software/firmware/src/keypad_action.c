#include "keypad_action.h"

#include <string.h>

#define LOG_TAG "keypad_action"
#include <elog.h>

#include "util.h"

#define MAX_KEY_ACTIONS 16
#define MAX_COMBO_ACTIONS 4
#define MAX_LONG_PRESS_COUNTER 100

typedef struct {
    keypad_key_t key;
    uint8_t action_id;
    uint8_t long_press_action_id;
    uint8_t long_press_counter;
    bool allow_repeat;
} keypad_key_action_state_t;

typedef struct {
    keypad_key_t key1;
    keypad_key_t key2;
    uint8_t action_id;
    bool action_pending;
    bool key1_swallow_release;
    bool key2_swallow_release;
} keypad_combo_action_state_t;

typedef struct {
    uint8_t encoder_ccw_action_id;
    uint8_t encoder_cw_action_id;
    keypad_key_action_state_t key_action[MAX_KEY_ACTIONS];
    uint8_t key_action_len;
    keypad_combo_action_state_t combo_action[MAX_COMBO_ACTIONS];
    uint8_t combo_action_len;
} keypad_action_state_t;

static keypad_action_state_t action_state = {0};

osStatus_t keypad_action_add(keypad_key_t key, uint8_t action_id, uint8_t long_press_action_id, bool allow_repeat)
{
    if (action_state.key_action_len >= MAX_KEY_ACTIONS) {
        return osErrorParameter;
    }
    if (key == KEYPAD_ENCODER_CCW || key == KEYPAD_ENCODER_CW) {
        return osErrorParameter;
    }

    keypad_key_action_state_t *key_action = &action_state.key_action[action_state.key_action_len];
    key_action->key = key;
    key_action->action_id = action_id;
    key_action->long_press_action_id = long_press_action_id;
    key_action->allow_repeat = allow_repeat;
    action_state.key_action_len++;
    return osOK;
}

osStatus_t keypad_action_add_encoder(uint8_t ccw_action_id, uint8_t cw_action_id)
{
    action_state.encoder_ccw_action_id = ccw_action_id;
    action_state.encoder_cw_action_id = cw_action_id;
    return osOK;
}

osStatus_t keypad_action_add_combo(keypad_key_t key1, keypad_key_t key2, uint8_t action_id)
{
    if (action_state.combo_action_len >= MAX_COMBO_ACTIONS) {
        return osErrorParameter;
    }
    keypad_combo_action_state_t *combo_action = &action_state.combo_action[action_state.combo_action_len];
    combo_action->key1 = key1;
    combo_action->key2 = key2;
    combo_action->action_id = action_id;
    action_state.combo_action_len++;
    return osOK;
}

void keypad_action_clear()
{
    memset(&action_state, 0, sizeof(keypad_action_state_t));
}

osStatus_t keypad_action_wait(keypad_action_t *action, int msecs_to_wait)
{
    HAL_StatusTypeDef ret = HAL_OK;
    keypad_event_t keypad_event;
    bool handled = false;
    uint8_t i;

    if (!action) {
        return osErrorParameter;
    }

    memset(action, 0, sizeof(keypad_action_t));

    ret = keypad_wait_for_event(&keypad_event, msecs_to_wait);
    if (ret != HAL_OK) {
        return hal_to_os_status(ret);
    }

    /* Handle encoder ticks up-front */
    if (keypad_event.key == KEYPAD_ENCODER_CCW) {
        action->action_id = action_state.encoder_ccw_action_id;
        action->key = keypad_event.key;
        action->count = keypad_event.count;
        handled = true;
    } else if (keypad_event.key == KEYPAD_ENCODER_CW) {
        action->action_id = action_state.encoder_cw_action_id;
        action->key = keypad_event.key;
        action->count = keypad_event.count;
        handled = true;
    }
    if (handled) { return osOK; }

    /* Check combo event pending flags */
    for (i = 0; i < action_state.combo_action_len; i++) {
        keypad_combo_action_state_t *combo_action = &action_state.combo_action[i];
        if (combo_action->action_id == 0) { continue; }
        if (combo_action->action_pending) {
            if (keypad_event.key == combo_action->key1 && !keypad_event.pressed) {
                combo_action->key1_swallow_release = false;
            } else if (keypad_event.key == combo_action->key2 && !keypad_event.pressed) {
                combo_action->key2_swallow_release = false;
            }

            if (!combo_action->key1_swallow_release && !combo_action->key2_swallow_release) {
                combo_action->action_pending = false;
                action->action_id = combo_action->action_id;
                /* Combo actions are not returned with a key ID */
            }
            handled = true;
            break;
        }
    }
    if (handled) { return osOK; }

    /* Check normal keys without long-press actions */
    for (i = 0; i < action_state.key_action_len; i++) {
        keypad_key_action_state_t *key_action = &action_state.key_action[i];
        if (keypad_event.key != key_action->key) {
            continue;
        }
        if (key_action->action_id == 0 || key_action->long_press_action_id != 0) {
            continue;
        }

        if ((key_action->allow_repeat && keypad_is_key_released_or_repeated(&keypad_event, key_action->key))
            || !keypad_event.pressed) {
            action->action_id = key_action->action_id;
            action->key = key_action->key;
            handled = true;
            break;
        }
    }
    if (handled) { return osOK; }

    /* Check normal keys with long-press actions */
    for (i = 0; i < action_state.key_action_len; i++) {
        keypad_key_action_state_t *key_action = &action_state.key_action[i];
        if (keypad_event.key != key_action->key) {
            continue;
        }
        if (key_action->long_press_action_id == 0) {
            continue;
        }

        if (keypad_event.pressed || keypad_event.repeated) {
            if (key_action->long_press_counter < MAX_LONG_PRESS_COUNTER) {
                key_action->long_press_counter++;
            }
        } else {
            action->key = key_action->key;
            if (key_action->long_press_counter > 2) {
                action->action_id = key_action->long_press_action_id;
                action->count = key_action->long_press_counter;
            } else {
                action->action_id = key_action->action_id;
            }
            key_action->long_press_counter = 0;
        }
        handled = true;
    }
    if (handled) { return osOK; }

    /* Set up pending flags for key combos */
    for (i = 0; i < action_state.combo_action_len; i++) {
        keypad_combo_action_state_t *combo_action = &action_state.combo_action[i];
        if (combo_action->action_id == 0) { continue; }
        if (keypad_is_key_combo_pressed(&keypad_event, combo_action->key1, combo_action->key2)) {
            combo_action->action_pending = true;
            combo_action->key1_swallow_release = true;
            combo_action->key2_swallow_release = true;
            break;
        }
    }

    /* Do basic handling of USB keyboard equivalent events */
    if (keypad_event.key == KEYPAD_USB_KEYBOARD && keypad_event.pressed) {
        keypad_key_t key_equiv = keypad_usb_get_keypad_equivalent(&keypad_event);
        for (i = 0; i < action_state.key_action_len; i++) {
            keypad_key_action_state_t *key_action = &action_state.key_action[i];
            if (key_equiv == key_action->key) {
                action->action_id = key_action->action_id;
                action->key = key_equiv;
                break;
            }
        }
    }

    return osOK;
}
