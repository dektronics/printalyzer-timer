/*
 * TCA8418 - I2C Controlled Keypad Scan IC
 */

#ifndef TCA8418_H
#define TCA8418_H

#include <stdbool.h>
#include <stm32f4xx_hal.h>

typedef struct {
    uint8_t rows;   /* ROW[7..0] */
    uint8_t cols_l; /* COL[7..0] */
    uint8_t cols_h; /* COL[9..8] */
} tca8418_pins_t;

/* Configuration register bits */
#define TCA8418_CFG_AI           0x80
#define TCA8418_CFG_GPI_E_CFG    0x40
#define TCA8418_CFG_OVR_FLOW_M   0x20
#define TCA8418_CFG_INT_CFG      0x10
#define TCA8418_CFG_OVR_FLOW_IEN 0x08
#define TCA8418_CFG_K_LCK_IEN    0x04
#define TCA8418_CFG_GPI_IEN      0x02
#define TCA8418_CFG_KE_IEN       0x01

/* Interrupt status bits */
#define TCA8418_INT_STAT_ALL          0x1F
#define TCA8418_INT_STAT_CAD_INT      0x10
#define TCA8418_INT_STAT_OVR_FLOW_INT 0x08
#define TCA8418_INT_STAT_K_LCK_INT    0x04
#define TCA8418_INT_STAT_GPI_INT      0x02
#define TCA8418_INT_STAT_K_INT        0x01

HAL_StatusTypeDef tca8418_init(I2C_HandleTypeDef *hi2c);

/**
 * Set the configuration register.
 */
HAL_StatusTypeDef tca8148_set_config(I2C_HandleTypeDef *hi2c, uint8_t value);

/**
 * Get the value of the interrupt status register.
 */
HAL_StatusTypeDef tca8148_get_interrupt_status(I2C_HandleTypeDef *hi2c, uint8_t *status);

/**
 * Set the value of the interrupt status register.
 */
HAL_StatusTypeDef tca8148_set_interrupt_status(I2C_HandleTypeDef *hi2c, uint8_t status);

/**
 * Get the key event count from the FIFO.
 */
HAL_StatusTypeDef tca8148_get_key_event_count(I2C_HandleTypeDef *hi2c, uint8_t *count);

/**
 * Get the next key event from the FIFO.
 */
HAL_StatusTypeDef tca8148_get_next_key_event(I2C_HandleTypeDef *hi2c, uint8_t *key, bool *pressed);

/**
 * Get the value of the GPIO interrupt status registers
 */
HAL_StatusTypeDef tca8148_get_gpio_interrupt_status(I2C_HandleTypeDef *hi2c, tca8418_pins_t *pins);

/**
 * Get the value of the GPIO data status registers
 */
HAL_StatusTypeDef tca8148_get_gpio_data_status(I2C_HandleTypeDef *hi2c, tca8418_pins_t *pins);

/**
 * GPIO Interrupt enable.
 * A bit value of '0' disables the corresponding pin's ability to generate an
 * interrupt when the state of the input changes.
 * A bit value of 1 enables the corresponding pin's ability to generate an
 * interrupt when the state of the input changes.
 */
HAL_StatusTypeDef tca8418_gpio_interrupt_enable(I2C_HandleTypeDef *hi2c, const tca8418_pins_t *pins);

/**
 * Keypad or GPIO selection.
 * A bit value of '0' puts the corresponding pin in GPIO mode.
 * A bit value of 1 puts the pin in key scan mode and it becomes part of the keypad array.
 */
HAL_StatusTypeDef tca8418_kp_gpio_select(I2C_HandleTypeDef *hi2c, const tca8418_pins_t *pins);

/**
 * GPI Event Mode.
 * A bit value of '0' indicates that the corresponding pin is not part of the event FIFO.
 * A bit value of 1 means the corresponding pin is part of the event FIFO.
 */
HAL_StatusTypeDef tca8418_gpi_event_mode(I2C_HandleTypeDef *hi2c, const tca8418_pins_t *pins);

/**
 * GPIO Data Direction.
 * A bit value of '0' sets the corresponding pin as an input.
 * A 1 in any of these bits sets the pin as an output.
 */
HAL_StatusTypeDef tca8418_gpio_data_direction(I2C_HandleTypeDef *hi2c, const tca8418_pins_t *pins);

/**
 * GPIO pull-up disable.
 * A bit value of '0' will enable the internal pull-up resistors.
 * A bit value of 1 will disable the internal pull-up resistors.
 */
HAL_StatusTypeDef tca8418_gpio_pullup_disable(I2C_HandleTypeDef *hi2c, const tca8418_pins_t *pins);

HAL_StatusTypeDef tca8418_clear_interrupt_status(I2C_HandleTypeDef *hi2c);

#endif /* TCA8418_H */
