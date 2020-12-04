/*
 * Hardware configuration constants
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "stm32f4xx_hal.h"

/* Pin mapping for USB port */
#define USB_DRIVE_VBUS_Pin       GPIO_PIN_1
#define USB_DRIVE_VBUS_GPIO_Port GPIOC
#define USB_VBUS_OC_Pin          GPIO_PIN_2
#define USB_VBUS_OC_GPIO_Port    GPIOC

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
#define DISP_RES_Pin             GPIO_PIN_4
#define DISP_RES_GPIO_Port       GPIOC

/* Pin mapping for meter sensor interrupt */
#define SENSOR_INT_Pin           GPIO_PIN_1
#define SENSOR_INT_GPIO_Port     GPIOB

/* Pin mapping for LED driver */
#define LED_LE_Pin               GPIO_PIN_14
#define LED_LE_GPIO_Port         GPIOB
#define LED_SDI_Pin              GPIO_PIN_15
#define LED_SDI_GPIO_Port        GPIOB
#define LED_OE_Pin               GPIO_PIN_6
#define LED_OE_GPIO_Port         GPIOC
#define LED_CLK_Pin              GPIO_PIN_7
#define LED_CLK_GPIO_Port        GPIOC

/* Pin mapping for relay driver */
#define RELAY_SFLT_Pin           GPIO_PIN_8
#define RELAY_SFLT_GPIO_Port     GPIOC
#define RELAY_ENLG_Pin           GPIO_PIN_9
#define RELAY_ENLG_GPIO_Port     GPIOC

/* Pin mapping for rotary encoder */
#define ENC_CH1_Pin              GPIO_PIN_8
#define ENC_CH1_GPIO_Port        GPIOA
#define ENC_CH2_Pin              GPIO_PIN_9
#define ENC_CH2_GPIO_Port        GPIOA

/* Pin mapping for keypad controller */
#define KEY_INT_Pin              GPIO_PIN_5
#define KEY_INT_GPIO_Port        GPIOB
#define KEY_INT_EXTI_IRQn        EXTI9_5_IRQn

#endif /* BOARD_CONFIG_H */
