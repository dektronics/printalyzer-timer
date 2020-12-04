/*
 * PAM8904E Piezo Sounder Driver
 */

#ifndef PAM8904E_H
#define PAM8904E_H

#include "stm32f4xx_hal.h"

typedef struct __pam8904e_handle_t {
    GPIO_TypeDef *en1_gpio_port;
    uint16_t en1_gpio_pin;
    GPIO_TypeDef *en2_gpio_port;
    uint16_t en2_gpio_pin;
    TIM_HandleTypeDef *din_tim;
    uint32_t din_tim_channel;
} pam8904e_handle_t;

typedef enum {
    PAM8904E_GAIN_SHUTDOWN = 0,
    PAM8904E_GAIN_1X = 1,
    PAM8904E_GAIN_2X = 2,
    PAM8904E_GAIN_3X = 3
} pam8904e_gain_t;

void pam8904e_set_gain(pam8904e_handle_t *handle, pam8904e_gain_t gain);
void pam8904e_start(pam8904e_handle_t *handle);
void pam8904e_stop(pam8904e_handle_t *handle);

#endif /* PAM8904E_H */
