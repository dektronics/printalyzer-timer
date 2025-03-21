/*
 * Hardware configuration constants
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "stm32f4xx_hal.h"

/* Pin mapping for USB port */
#define USB_DRIVE_VBUS_Pin       GPIO_PIN_13
#define USB_DRIVE_VBUS_GPIO_Port GPIOB
#define USB_HUB_CLK_Pin          GPIO_PIN_9
#define USB_HUB_CLK_GPIO_Port    GPIOC
#define USB_HUB_RESET_Pin        GPIO_PIN_12
#define USB_HUB_RESET_GPIO_Port  GPIOB

/* Pin mapping for piezo driver */
#define BUZZ_EN1_Pin             GPIO_PIN_0
#define BUZZ_EN1_GPIO_Port       GPIOA
#define BUZZ_EN2_Pin             GPIO_PIN_1
#define BUZZ_EN2_GPIO_Port       GPIOA
#define BUZZ_DIN_Pin             GPIO_PIN_2
#define BUZZ_DIN_GPIO_Port       GPIOA

/* Pin mapping for display module */
#define DISP_CS_Pin              GPIO_PIN_4
#define DISP_CS_GPIO_Port        GPIOA
#define DISP_SCK_Pin             GPIO_PIN_5
#define DISP_SCK_GPIO_Port       GPIOA
#define DISP_DC_Pin              GPIO_PIN_6
#define DISP_DC_GPIO_Port        GPIOA
#define DISP_MOSI_Pin            GPIO_PIN_7
#define DISP_MOSI_GPIO_Port      GPIOA
#define DISP_RESET_Pin           GPIO_PIN_4
#define DISP_RESET_GPIO_Port     GPIOC

/* Pin mapping for LED driver */
#define LED_RESET_Pin            GPIO_PIN_2
#define LED_RESET_GPIO_Port      GPIOC

/* Pin mapping for relay driver */
#define RELAY_SFLT_Pin           GPIO_PIN_11
#define RELAY_SFLT_GPIO_Port     GPIOC
#define RELAY_ENLG_Pin           GPIO_PIN_2
#define RELAY_ENLG_GPIO_Port     GPIOD

/* Pin mapping for rotary encoder */
#define ENC_CH1_Pin              GPIO_PIN_8
#define ENC_CH1_GPIO_Port        GPIOA
#define ENC_CH2_Pin              GPIO_PIN_9
#define ENC_CH2_GPIO_Port        GPIOA

/* Pin mapping for keypad controller */
#define KEY_INT_Pin              GPIO_PIN_5
#define KEY_INT_GPIO_Port        GPIOB
#define KEY_INT_EXTI_IRQn        EXTI9_5_IRQn
#define KEY_RESET_Pin            GPIO_PIN_5
#define KEY_RESET_GPIO_Port      GPIOC

/* Pin mapping for the DMX512 UART port */
#define DMX512_TX_Pin            GPIO_PIN_6
#define DMX512_TX_GPIO_Port      GPIOC
#define DMX512_RX_Pin            GPIO_PIN_7
#define DMX512_RX_GPIO_Port      GPIOC
#define DMX512_TX_EN_Pin         GPIO_PIN_8
#define DMX512_TX_EN_GPIO_Port   GPIOC
#define DMX512_RX_EN_Pin         GPIO_PIN_11
#define DMX512_RX_EN_GPIO_Port   GPIOA

#endif /* BOARD_CONFIG_H */
