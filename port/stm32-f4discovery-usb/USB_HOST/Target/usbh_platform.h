/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usbh_platform.h
  * @brief          : Header for usbh_platform.c file.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBH_PLATFORM_H__
#define __USBH_PLATFORM_H__

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usb_host.h"

/* USER CODE BEGIN INCLUDE */

/* USER CODE END INCLUDE */

void MX_DriverVbusFS(uint8_t state);

#ifdef __cplusplus
}
#endif

#endif /* __USBH_PLATFORM_H__ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
