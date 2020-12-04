#include "pam8904e.h"

void pam8904e_set_gain(pam8904e_handle_t *handle, pam8904e_gain_t gain)
{
    HAL_GPIO_WritePin(handle->en1_gpio_port, handle->en1_gpio_pin,
        (gain & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(handle->en2_gpio_port, handle->en2_gpio_pin,
        (gain & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void pam8904e_start(pam8904e_handle_t *handle)
{
    HAL_TIM_PWM_Start(handle->din_tim, handle->din_tim_channel);
}

void pam8904e_stop(pam8904e_handle_t *handle)
{
    HAL_TIM_PWM_Stop(handle->din_tim, handle->din_tim_channel);
}
