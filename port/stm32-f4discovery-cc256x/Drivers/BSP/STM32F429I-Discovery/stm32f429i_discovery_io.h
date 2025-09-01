/**
  ******************************************************************************
  * @file    stm32f429i_discovery_io.h
  * @author  MCD Application Team
  * @brief   This file contains all the functions prototypes for the
  *          stm32f429i_discovery_io.c driver.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32F429I_DISCOVERY_IO_H
#define __STM32F429I_DISCOVERY_IO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f429i_discovery.h"
/* Include IO component driver */
#include "../Components/stmpe811/stmpe811.h"

/** @addtogroup BSP
  * @{
  */

/** @addtogroup STM32F429I_DISCOVERY
  * @{
  */

/** @addtogroup STM32F429I_DISCOVERY_IO
  * @{
  */

/** @defgroup STM32F429I_DISCOVERY_IO_Exported_Types STM32F429I DISCOVERY IO Exported Types
  * @{
  */
typedef enum
{
  IO_OK       = 0,
  IO_ERROR    = 1,
  IO_TIMEOUT  = 2
} IO_StatusTypeDef;
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_IO_Exported_Constants STM32F429I DISCOVERY IO Exported Constants
  * @{
  */
#define IO_PIN_0                     0x01
#define IO_PIN_1                     0x02
#define IO_PIN_2                     0x04
#define IO_PIN_3                     0x08
#define IO_PIN_4                     0x10
#define IO_PIN_5                     0x20
#define IO_PIN_6                     0x40
#define IO_PIN_7                     0x80
#define IO_PIN_ALL                   0xFF
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_IO_Exported_Macros STM32F429I DISCOVERY IO Exported Macros
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_IO_Exported_Functions STM32F429I DISCOVERY IO Exported Functions
  * @{
  */
uint8_t  BSP_IO_Init(void);
uint8_t  BSP_IO_ITGetStatus(uint16_t IoPin);
void     BSP_IO_ITClear(void);
void     BSP_IO_ConfigPin(uint16_t IoPin, IO_ModeTypedef IoMode);
void     BSP_IO_WritePin(uint16_t IoPin, uint8_t PinState);
uint16_t BSP_IO_ReadPin(uint16_t IoPin);
void     BSP_IO_TogglePin(uint16_t IoPin);

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* __STM32F429I_DISCOVERY_IO_H */
