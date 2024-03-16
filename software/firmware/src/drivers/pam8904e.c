#include "pam8904e.h"

void pam8904e_set_gain(pam8904e_handle_t *handle, pam8904e_gain_t gain)
{
    HAL_GPIO_WritePin(handle->en1_gpio_port, handle->en1_gpio_pin,
        (gain & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(handle->en2_gpio_port, handle->en2_gpio_pin,
        (gain & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void pam8904e_set_frequency(pam8904e_handle_t *handle, pam8904e_freq_t freq)
{
    switch (freq) {
    case PAM8904E_FREQ_DEFAULT:
        __HAL_TIM_SET_PRESCALER(handle->din_tim, 119);
        break;
    case PAM8904E_FREQ_500HZ:
        __HAL_TIM_SET_PRESCALER(handle->din_tim, 359);
        break;
    case PAM8904E_FREQ_1000HZ:
        __HAL_TIM_SET_PRESCALER(handle->din_tim, 179);
        break;
    case PAM8904E_FREQ_1500HZ:
        __HAL_TIM_SET_PRESCALER(handle->din_tim, 119);
        break;
    case PAM8904E_FREQ_2000HZ:
        __HAL_TIM_SET_PRESCALER(handle->din_tim, 89);
        break;
    case PAM8904E_FREQ_2500HZ:
        __HAL_TIM_SET_PRESCALER(handle->din_tim, 71);
        break;
    case PAM8904E_FREQ_3000HZ:
        __HAL_TIM_SET_PRESCALER(handle->din_tim, 59);
        break;
    case PAM8904E_FREQ_3600HZ:
        __HAL_TIM_SET_PRESCALER(handle->din_tim, 49);
        break;
    case PAM8904E_FREQ_4000HZ:
        __HAL_TIM_SET_PRESCALER(handle->din_tim, 44);
        break;
    case PAM8904E_FREQ_4500HZ:
        __HAL_TIM_SET_PRESCALER(handle->din_tim, 39);
        break;
    default:
        break;
    }
}

void pam8904e_start(pam8904e_handle_t *handle)
{
    HAL_TIM_PWM_Start(handle->din_tim, handle->din_tim_channel);
}

void pam8904e_stop(pam8904e_handle_t *handle)
{
    HAL_TIM_PWM_Stop(handle->din_tim, handle->din_tim_channel);
}
