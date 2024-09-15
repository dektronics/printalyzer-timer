/**
 ******************************************************************************
 * @file         stm32f4xx_hal_msp.c
 * @brief        This file provides code for the MSP Initialization
 *               and de-Initialization codes.
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
#include "board_config.h"


/**
 * Initializes the Global MSP.
 */
void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();

    /* System interrupt init*/
    /* PendSV_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(PendSV_IRQn, 15, 0);
}

/**
 * CRC MSP Initialization
 * @param hcrc: CRC handle pointer
 * @retval None
 */
void HAL_CRC_MspInit(CRC_HandleTypeDef* hcrc)
{
    if (hcrc->Instance == CRC) {
        /* Peripheral clock enable */
        __HAL_RCC_CRC_CLK_ENABLE();
    }
}

/**
 * CRC MSP De-Initialization
 * @param hcrc: CRC handle pointer
 * @retval None
 */
void HAL_CRC_MspDeInit(CRC_HandleTypeDef* hcrc)
{
    if (hcrc->Instance == CRC) {
        /* Peripheral clock disable */
        __HAL_RCC_CRC_CLK_DISABLE();
    }
}

/**
 * I2C MSP Initialization
 * @param hi2c: I2C handle pointer
 * @retval None
 */
void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if (hi2c->Instance == I2C1) {
        __HAL_RCC_GPIOB_CLK_ENABLE();

        /*
         * I2C1 GPIO Configuration
         * PB7     ------> I2C1_SDA
         * PB8     ------> I2C1_SCL
         */
        GPIO_InitStruct.Pin = GPIO_PIN_7|GPIO_PIN_8;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        /* Peripheral clock enable */
        __HAL_RCC_I2C1_CLK_ENABLE();
    }
}

/**
 * SMBUS MSP Initialization
 * @param hsmbus: SMBUS handle pointer
 * @retval None
 */
void HAL_SMBUS_MspInit(SMBUS_HandleTypeDef* hsmbus)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if (hsmbus->Instance == I2C2) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();

        /*
         * I2C2 GPIO Configuration
         * PB10     ------> I2C2_SCL
         * PC12     ------> I2C2_SDA
         */
        GPIO_InitStruct.Pin = GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_12;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        /* Peripheral clock enable */
        __HAL_RCC_I2C2_CLK_ENABLE();

        /* I2C2 interrupt Init */
        HAL_NVIC_SetPriority(I2C2_EV_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(I2C2_EV_IRQn);
        HAL_NVIC_SetPriority(I2C2_ER_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(I2C2_ER_IRQn);
    }
}

/**
 * I2C MSP De-Initialization
 * @param hi2c: I2C handle pointer
 * @retval None
 */
void HAL_I2C_MspDeInit(I2C_HandleTypeDef* hi2c)
{
    if (hi2c->Instance == I2C1) {
        /* Peripheral clock disable */
        __HAL_RCC_I2C1_CLK_DISABLE();

        /*
         * I2C1 GPIO Configuration
         * PB7     ------> I2C1_SDA
         * PB8     ------> I2C1_SCL
         */
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_7);
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_8);
    }
}

/**
 * SMBUS MSP De-Initialization
 * @param hsmbus: SMBUS handle pointer
 * @retval None
 */
void HAL_SMBUS_MspDeInit(SMBUS_HandleTypeDef* hsmbus)
{
    if (hsmbus->Instance == I2C2) {
        /* Peripheral clock disable */
        __HAL_RCC_I2C2_CLK_DISABLE();

        /*
         * I2C2 GPIO Configuration
         * PB10     ------> I2C2_SCL
         * PC12     ------> I2C2_SDA
         */
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10);
        HAL_GPIO_DeInit(GPIOC, GPIO_PIN_12);

        /* I2C2 interrupt DeInit */
        HAL_NVIC_DisableIRQ(I2C2_EV_IRQn);
        HAL_NVIC_DisableIRQ(I2C2_ER_IRQn);
    }
}

/**
 * SPI MSP Initialization
 * @param hspi: SPI handle pointer
 * @retval None
 */
void HAL_SPI_MspInit(SPI_HandleTypeDef* hspi)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if (hspi->Instance == SPI1) {
        /* Peripheral clock enable */
        __HAL_RCC_SPI1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /*
         * SPI1 GPIO Configuration
         * PA5     ------> SPI1_SCK
         * PA7     ------> SPI1_MOSI
         */
        GPIO_InitStruct.Pin = DISP_SCK_Pin|DISP_MOSI_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

/**
 * SPI MSP De-Initialization
 * @param hspi: SPI handle pointer
 * @retval None
 */
void HAL_SPI_MspDeInit(SPI_HandleTypeDef* hspi)
{
    if (hspi->Instance == SPI1) {
        /* Peripheral clock disable */
        __HAL_RCC_SPI1_CLK_DISABLE();

        /*
         * SPI1 GPIO Configuration
         * PA5     ------> SPI1_SCK
         * PA7     ------> SPI1_MOSI
         */
        HAL_GPIO_DeInit(GPIOA, DISP_SCK_Pin|DISP_MOSI_Pin);
    }
}

/**
 * UART MSP Initialization
 * @param huart: UART handle pointer
 * @retval None
 */
void HAL_UART_MspInit(UART_HandleTypeDef* huart)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if (huart->Instance == USART1) {
        /* Peripheral clock enable */
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        /*
         * USART1 GPIO Configuration
         * PA10     ------> USART1_RX
         * PB6      ------> USART1_TX
         */
        GPIO_InitStruct.Pin = GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_6;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }
}

/**
 * UART MSP De-Initialization
 * @param huart: UART handle pointer
 * @retval None
 */
void HAL_UART_MspDeInit(UART_HandleTypeDef* huart)
{
    if (huart->Instance == USART1) {
        /* Peripheral clock disable */
        __HAL_RCC_USART1_CLK_DISABLE();

        /*
         * USART1 GPIO Configuration
         * PA10     ------> USART1_RX
         * PB6      ------> USART1_TX
         */
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_10);
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6);
    }
}
