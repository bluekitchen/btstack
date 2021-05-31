/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
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

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define B1_Pin GPIO_PIN_13
#define B1_GPIO_Port GPIOC
#define LED_RX_Pin GPIO_PIN_0
#define LED_RX_GPIO_Port GPIOC
#define LED_TX_Pin GPIO_PIN_1
#define LED_TX_GPIO_Port GPIOC
#define RADIO_nRESET_Pin GPIO_PIN_0
#define RADIO_nRESET_GPIO_Port GPIOA
#define USART_TX_Pin GPIO_PIN_2
#define USART_TX_GPIO_Port GPIOA
#define USART_RX_Pin GPIO_PIN_3
#define USART_RX_GPIO_Port GPIOA
#define RADIO_SCK_Pin GPIO_PIN_5
#define RADIO_SCK_GPIO_Port GPIOA
#define RADIO_MISO_Pin GPIO_PIN_6
#define RADIO_MISO_GPIO_Port GPIOA
#define RADIO_MOSI_Pin GPIO_PIN_7
#define RADIO_MOSI_GPIO_Port GPIOA
#define RADIO_NSS_Pin GPIO_PIN_8
#define RADIO_NSS_GPIO_Port GPIOA
#define TMS_Pin GPIO_PIN_13
#define TMS_GPIO_Port GPIOA
#define TCK_Pin GPIO_PIN_14
#define TCK_GPIO_Port GPIOA
#define RADIO_BUSY_Pin GPIO_PIN_3
#define RADIO_BUSY_GPIO_Port GPIOB
#define RADIO_DIO1_Pin GPIO_PIN_4
#define RADIO_DIO1_GPIO_Port GPIOB
#define RADIO_DIO1_EXTI_IRQn EXTI4_IRQn
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
