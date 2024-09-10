/**
  ******************************************************************************
  * @file    stm32f429i_discovery_ts.h
  * @author  MCD Application Team
  * @brief   This file contains all the functions prototypes for the
  *          stm32f429i_discovery_ts.c driver.
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
#ifndef __STM32F429I_DISCOVERY_TS_H
#define __STM32F429I_DISCOVERY_TS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f429i_discovery.h"
/* Include TouchScreen component driver */
#include "../Components/stmpe811/stmpe811.h"

/** @addtogroup BSP
  * @{
  */

/** @addtogroup STM32F429I_DISCOVERY
  * @{
  */

/** @addtogroup STM32F429I_DISCOVERY_TS
  * @{
  */

/** @defgroup STM32F429I_DISCOVERY_TS_Exported_Types STM32F429I DISCOVERY TS Exported Types
  * @{
  */
typedef struct
{
  uint16_t TouchDetected;
  uint16_t X;
  uint16_t Y;
  uint16_t Z;
} TS_StateTypeDef;
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_TS_Exported_Constants STM32F429I DISCOVERY TS Exported Constants
  * @{
  */
#define TS_SWAP_NONE                    0x00
#define TS_SWAP_X                       0x01
#define TS_SWAP_Y                       0x02
#define TS_SWAP_XY                      0x04

typedef enum
{
  TS_OK       = 0x00,
  TS_ERROR    = 0x01,
  TS_TIMEOUT  = 0x02
} TS_StatusTypeDef;
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_TS_Exported_Macros STM32F429I DISCOVERY TS Exported Macros
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_TS_Exported_Functions STM32F429I DISCOVERY TS Exported Functions
  * @{
  */
uint8_t BSP_TS_Init(uint16_t XSize, uint16_t YSize);
void    BSP_TS_GetState(TS_StateTypeDef *TsState);
uint8_t BSP_TS_ITConfig(void);
uint8_t BSP_TS_ITGetStatus(void);
void    BSP_TS_ITClear(void);

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

#endif /* __STM32F429I_DISCOVERY_TS_H */
