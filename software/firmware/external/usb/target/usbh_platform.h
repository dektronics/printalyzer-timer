/**
  ******************************************************************************
  * @file           : usbh_platform.h
  * @brief          : Header for usbh_platform.c file.
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

#ifndef __USBH_PLATFORM_H__
#define __USBH_PLATFORM_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdint.h>

void MX_DriverVbusHS(uint8_t state);

#ifdef __cplusplus
}
#endif

#endif /* __USBH_PLATFORM_H__ */
