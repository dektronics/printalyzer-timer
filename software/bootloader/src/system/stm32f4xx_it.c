/**
  ******************************************************************************
  * @file    stm32f4xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#include "stm32f4xx_hal.h"
#include "stm32f4xx_it.h"
#include "board_config.h"

#include <stdio.h>

extern HCD_HandleTypeDef hhcd_USB_OTG_HS;
extern SMBUS_HandleTypeDef hsmbus2;
extern TIM_HandleTypeDef htim1;

/******************************************************************************/
/*           Cortex-M4 Processor Interruption and Exception Handlers          */
/******************************************************************************/

/**
 * @brief This function handles Non maskable interrupt.
 */
void NMI_Handler(void)
{
    while (1) {
    }
}

/**
 * @brief This function handles Hard fault interrupt.
 */
void HardFault_Handler(void)
{
    while (1) {
    }
}

/**
 * @brief This function handles Memory management fault.
 */
void MemManage_Handler(void)
{
    while (1) {
    }
}

/**
 * @brief This function handles Pre-fetch fault, memory access fault.
 */
void BusFault_Handler(void)
{
    while (1) {
    }
}

/**
 * @brief This function handles Undefined instruction or illegal state.
 */
void UsageFault_Handler(void)
{
    while (1) {
    }
}

/**
 * @brief This function handles System service call via SWI instruction.
 */
void SVC_Handler(void)
{
}

/**
 * @brief This function handles Debug monitor.
 */
void DebugMon_Handler(void)
{
}

/**
 * @brief This function handles Pendable request for system service.
 */
void PendSV_Handler(void)
{
}

/**
 * @brief This function handles System tick timer.
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/******************************************************************************/
/* STM32F4xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f4xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles EXTI line[9:5] interrupts.
  */
void EXTI9_5_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(KEY_INT_Pin);
}

/**
  * @brief This function handles TIM1 update interrupt and TIM10 global interrupt.
  */
void TIM1_UP_TIM10_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim1);
}

/**
  * @brief This function handles TIM1 trigger and commutation interrupts and TIM11 global interrupt.
  */
void TIM1_TRG_COM_TIM11_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim1);
}

/**
  * @brief This function handles TIM1 capture compare interrupt.
  */
void TIM1_CC_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim1);
}

/**
  * @brief This function handles I2C2 event interrupt.
  */
void I2C2_EV_IRQHandler(void)
{
    HAL_SMBUS_EV_IRQHandler(&hsmbus2);
}

/**
  * @brief This function handles I2C2 error interrupt.
  */
void I2C2_ER_IRQHandler(void)
{
    HAL_SMBUS_ER_IRQHandler(&hsmbus2);
}

/**
  * @brief This function handles USB On The Go HS global interrupt.
  */
void OTG_HS_IRQHandler(void)
{
    HAL_HCD_IRQHandler(&hhcd_USB_OTG_HS);
}
