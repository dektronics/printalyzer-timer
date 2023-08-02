/*
 * Enlarger Control Functions
 *
 * Since enlarger configuration possibilities have now made the process
 * of actually turning the enlarger on and off a bit more complex,
 * the functions here attempt to wrap that complexity behind a relatively
 * simple common interface.
 */
#ifndef ENLARGER_CONTROL_H
#define ENLARGER_CONTROL_H

#include "enlarger_config.h"
#include "contrast.h"

/**
 * The list of possible enlarger states.
 *
 * Not all configurations will support all states, and certain states
 * may not differ under all configurations. This is intended to capture
 * the totality of possibilities.
 */
typedef enum {
    ENLARGER_CONTROL_STATE_OFF = 0, /*!< Enlarger is turned off */
    ENLARGER_CONTROL_STATE_FOCUS,   /*!< Enlarger is in white-light focus mode */
    ENLARGER_CONTROL_STATE_SAFE,    /*!< Enlarger is in red-light safe mode */
    ENLARGER_CONTROL_STATE_EXPOSURE /*!< Enlarger is in exposure mode */
} enlarger_control_state_t;

/**
 * Set the enlarger to the desired state.
 *
 * This function assumes any prerequisites for controlling the enlarger,
 * such as an active DMX interface, have already been met.
 *
 * If the current enlarger does not support the requested state, then this
 * function will successfully return without doing anything.
 *
 * Blocking mode is intended for use in normal exposure cases, where
 * predictable timing matters. Non-blocking mode is intended for less
 * critical and more responsive situations, such as diagnostics.
 * This only really makes a difference for more complex control
 * mechanisms such as DMX.
 *
 * @param enlarger_control The control portion of the active enlarger configuration
 * @param state The enlarger state to set
 * @param grade The selected contrast grade (only used in the exposure state, and only with certain enlarger configurations)
 * @param channel_red Explicit red channel value (only used in the exposure state, when contrast is unset)
 * @param channel_green Explicit green channel value (only used in the exposure state, when contrast is unset)
 * @param channel_blue Explicit blue channel value (only used in the exposure state, when contrast is unset)
 * @param blocking True to block until the change has taken effect, false otherwise
 */
osStatus_t enlarger_control_set_state(const enlarger_control_t *enlarger_control,
    enlarger_control_state_t state, contrast_grade_t grade,
    uint16_t channel_red, uint16_t channel_green, uint16_t channel_blue,
    bool blocking);

osStatus_t enlarger_control_set_state_off(const enlarger_control_t *enlarger_control, bool blocking);
osStatus_t enlarger_control_set_state_focus(const enlarger_control_t *enlarger_control, bool blocking);
osStatus_t enlarger_control_set_state_safe(const enlarger_control_t *enlarger_control, bool blocking);

#endif /* ENLARGER_CONTROL_H */
