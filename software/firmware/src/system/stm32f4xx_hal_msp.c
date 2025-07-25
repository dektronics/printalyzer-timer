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

extern DMA_HandleTypeDef hdma_usart6_tx;
extern void Error_Handler(void);

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

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
 * TIM_Encoder MSP Initialization
 * @param htim_encoder: TIM_Encoder handle pointer
 * @retval None
 */
void HAL_TIM_Encoder_MspInit(TIM_HandleTypeDef* htim_encoder)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (htim_encoder->Instance == TIM1) {
        /* Peripheral clock enable */
        __HAL_RCC_TIM1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /*
         * TIM1 GPIO Configuration
         * PA8     ------> TIM1_CH1
         * PA9     ------> TIM1_CH2
         */
        GPIO_InitStruct.Pin = ENC_CH1_Pin|ENC_CH2_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* TIM1 interrupt Init */
        HAL_NVIC_SetPriority(TIM1_UP_TIM10_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);
        HAL_NVIC_SetPriority(TIM1_TRG_COM_TIM11_IRQn, 15, 0);
        HAL_NVIC_EnableIRQ(TIM1_TRG_COM_TIM11_IRQn);
        HAL_NVIC_SetPriority(TIM1_CC_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(TIM1_CC_IRQn);
    }
}

/**
* TIM_OC MSP Initialization
* @param htim_oc: TIM_OC handle pointer
* @retval None
*/
void HAL_TIM_OC_MspInit(TIM_HandleTypeDef *htim_oc)
{
    if (htim_oc->Instance == TIM4) {
        /* Peripheral clock enable */
        __HAL_RCC_TIM4_CLK_ENABLE();

        /* TIM4 interrupt Init */
        HAL_NVIC_SetPriority(TIM4_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(TIM4_IRQn);
    }
}

/**
 * TIM_PWM MSP Initialization
 * @param htim_pwm: TIM_PWM handle pointer
 * @retval None
 */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef* htim_pwm)
{
    if (htim_pwm->Instance == TIM8) {
        /* Peripheral clock enable */
        __HAL_RCC_TIM8_CLK_ENABLE();

        /* TIM8 interrupt Init */
        HAL_NVIC_SetPriority(TIM8_BRK_TIM12_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(TIM8_BRK_TIM12_IRQn);
        HAL_NVIC_SetPriority(TIM8_CC_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(TIM8_CC_IRQn);

    } else if(htim_pwm->Instance == TIM9) {
        /* Peripheral clock enable */
        __HAL_RCC_TIM9_CLK_ENABLE();
    }
}


/**
 * TIM_Base MSP Initialization
 * @param htim_base: TIM_Base handle pointer
 * @retval None
 */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* htim_base)
{
    if (htim_base->Instance == TIM10) {
        /* Peripheral clock enable */
        __HAL_RCC_TIM10_CLK_ENABLE();

        /* TIM10 interrupt Init */
        HAL_NVIC_SetPriority(TIM1_UP_TIM10_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);
    }
}

void HAL_TIM_MspPostInit(TIM_HandleTypeDef* htim)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
	
    if (htim->Instance == TIM9) {
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /*
         * TIM9 GPIO Configuration
         * PA2     ------> TIM9_CH1
         */
        GPIO_InitStruct.Pin = BUZZ_DIN_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF3_TIM9;
        HAL_GPIO_Init(BUZZ_DIN_GPIO_Port, &GPIO_InitStruct);
    }
}

/**
 * TIM_Encoder MSP De-Initialization
 * @param htim_encoder: TIM_Encoder handle pointer
 * @retval None
 */
void HAL_TIM_Encoder_MspDeInit(TIM_HandleTypeDef* htim_encoder)
{
    if (htim_encoder->Instance == TIM1) {
        /* Peripheral clock disable */
        __HAL_RCC_TIM1_CLK_DISABLE();

        /*
         * TIM1 GPIO Configuration
         * PA8     ------> TIM1_CH1
         * PA9     ------> TIM1_CH2
         */
        HAL_GPIO_DeInit(GPIOA, ENC_CH1_Pin|ENC_CH2_Pin);

        /* TIM1 interrupt DeInit */
        /**
         * Uncomment the line below to disable the "TIM1_UP_TIM10_IRQn" interrupt
         * Be aware, disabling shared interrupt may affect other IPs
         */
        /* HAL_NVIC_DisableIRQ(TIM1_UP_TIM10_IRQn); */

        HAL_NVIC_DisableIRQ(TIM1_TRG_COM_TIM11_IRQn);
        HAL_NVIC_DisableIRQ(TIM1_CC_IRQn);
    }
}

/**
* TIM_OC MSP De-Initialization
* @param htim_oc: TIM_OC handle pointer
* @retval None
*/
void HAL_TIM_OC_MspDeInit(TIM_HandleTypeDef *htim_oc)
{
    if (htim_oc->Instance == TIM4) {
        /* Peripheral clock disable */
        __HAL_RCC_TIM4_CLK_DISABLE();

        /* TIM4 interrupt DeInit */
        HAL_NVIC_DisableIRQ(TIM4_IRQn);
    }
}

/**
 * TIM_PWM MSP De-Initialization
 * @param htim_pwm: TIM_PWM handle pointer
 * @retval None
 */
void HAL_TIM_PWM_MspDeInit(TIM_HandleTypeDef* htim_pwm)
{
    if(htim_pwm->Instance == TIM8) {
        /* Peripheral clock disable */
        __HAL_RCC_TIM8_CLK_DISABLE();

        /* TIM8 interrupt DeInit */
        HAL_NVIC_DisableIRQ(TIM8_BRK_TIM12_IRQn);
        HAL_NVIC_DisableIRQ(TIM8_CC_IRQn);

    } else if(htim_pwm->Instance == TIM9) {
        /* Peripheral clock disable */
        __HAL_RCC_TIM9_CLK_DISABLE();
    }
}

/**
 * TIM_Base MSP De-Initialization
 * @param htim_base: TIM_Base handle pointer
 * @retval None
 */
void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef* htim_base)
{
    if (htim_base->Instance == TIM10) {
        /* Peripheral clock disable */
        __HAL_RCC_TIM10_CLK_DISABLE();

        /* TIM10 interrupt DeInit */
        /**
         * Uncomment the line below to disable the "TIM1_UP_TIM10_IRQn" interrupt
         * Be aware, disabling shared interrupt may affect other IPs
         */
        /* HAL_NVIC_DisableIRQ(TIM1_UP_TIM10_IRQn); */
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

    } else if (huart->Instance == USART6) {
        /* Peripheral clock enable */
        __HAL_RCC_USART6_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();

        /*
         * USART6 GPIO Configuration
         * PC6     ------> USART6_TX
         * PC7     ------> USART6_RX
         */
        GPIO_InitStruct.Pin = DMX512_TX_Pin|DMX512_RX_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        /* USART6 DMA Init */
        /* USART6_TX Init */
        hdma_usart6_tx.Instance = DMA2_Stream6;
        hdma_usart6_tx.Init.Channel = DMA_CHANNEL_5;
        hdma_usart6_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hdma_usart6_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_usart6_tx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_usart6_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart6_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_usart6_tx.Init.Mode = DMA_NORMAL;
        hdma_usart6_tx.Init.Priority = DMA_PRIORITY_LOW;
        hdma_usart6_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        if (HAL_DMA_Init(&hdma_usart6_tx) != HAL_OK) {
            Error_Handler();
        }

        __HAL_LINKDMA(huart,hdmatx,hdma_usart6_tx);

        /* USART6 interrupt Init */
        HAL_NVIC_SetPriority(USART6_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(USART6_IRQn);
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

    } else if(huart->Instance == USART6) {
        /* Peripheral clock disable */
        __HAL_RCC_USART6_CLK_DISABLE();

        /*
         * USART6 GPIO Configuration
         * PC6     ------> USART6_TX
         * PC7     ------> USART6_RX
         */
        HAL_GPIO_DeInit(GPIOC, DMX512_TX_Pin|DMX512_RX_Pin);

        /* USART6 DMA DeInit */
        HAL_DMA_DeInit(huart->hdmatx);

        /* USART6 interrupt DeInit */
        HAL_NVIC_DisableIRQ(USART6_IRQn);
    }
}

/**
 * HCD MSP Initialization
 * @param hhcd: HCD handle pointer
 * @retval None
 */
void HAL_HCD_MspInit(HCD_HandleTypeDef* hhcd)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    if (hhcd->Instance == USB_OTG_HS) {
        /* Initialize the peripherals clock */
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_CLK48;
        PeriphClkInitStruct.PLLSAI.PLLSAIM = 8;
        PeriphClkInitStruct.PLLSAI.PLLSAIN = 96;
        PeriphClkInitStruct.PLLSAI.PLLSAIQ = 2;
        PeriphClkInitStruct.PLLSAI.PLLSAIP = RCC_PLLSAIP_DIV4;
        PeriphClkInitStruct.PLLSAIDivQ = 1;
        PeriphClkInitStruct.Clk48ClockSelection = RCC_CLK48CLKSOURCE_PLLSAIP;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
            Error_Handler();
        }

        __HAL_RCC_GPIOB_CLK_ENABLE();

        /*
         * USB_OTG_HS GPIO Configuration
         * PB14     ------> USB_OTG_HS_DM
         * PB15     ------> USB_OTG_HS_DP
         */
        GPIO_InitStruct.Pin = GPIO_PIN_14|GPIO_PIN_15;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF12_OTG_HS_FS;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        /* Peripheral clock enable */
        __HAL_RCC_USB_OTG_HS_CLK_ENABLE();

        /* Peripheral interrupt init */
        HAL_NVIC_SetPriority(OTG_HS_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(OTG_HS_IRQn);
    }
}

/**
 * HCD MSP De-Initialization
 * @param hhcd: HCD handle pointer
 * @retval None
 */
void HAL_HCD_MspDeInit(HCD_HandleTypeDef* hhcd)
{
    if (hhcd->Instance == USB_OTG_HS) {
        /* Peripheral clock disable */
        __HAL_RCC_USB_OTG_HS_CLK_DISABLE();

        /**
         * USB_OTG_HS GPIO Configuration
         * PB14     ------> USB_OTG_HS_DM
         * PB15     ------> USB_OTG_HS_DP
         */
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_14|GPIO_PIN_15);

        /* Peripheral interrupt deinit */
        HAL_NVIC_DisableIRQ(OTG_HS_IRQn);
    }
}
