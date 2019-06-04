/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32wbxx.h"

#define DEBUG_USART                     USART1
#define DEBUG_USART_CLK_ENABLE()        __HAL_RCC_USART1_CLK_ENABLE()
#define DEBUG_GPIO_AF                   GPIO_AF7_USART1
#define DEBUG_USART_TX_Pin              GPIO_PIN_6
#define DEBUG_USART_TX_GPIO_Port        GPIOB
#define DEBUG_USART_RX_Pin              GPIO_PIN_7
#define DEBUG_USART_RX_GPIO_Port        GPIOB
#define DEBUG_USART_PORT_CLK_ENABLE()   __HAL_RCC_GPIOB_CLK_ENABLE()

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
