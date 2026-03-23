/**
  ******************************************************************************
  * @file    stm32446e_eval_ts.h
  * @author  MCD Application Team
  * @brief   This file contains the common defines and functions prototypes for
  *          the stm32446e_eval_ts.c driver.
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
#ifndef __STM32446E_EVAL_TS_H
#define __STM32446E_EVAL_TS_H

#ifdef __cplusplus
 extern "C" {
#endif   
   
/* Includes ------------------------------------------------------------------*/
#include "stm32446e_eval.h"
/* Include IOExpander(MFX) component Driver */ 
#include "../Components/mfxstm32l152/mfxstm32l152.h"
   
/** @addtogroup BSP
  * @{
  */ 

/** @addtogroup STM32446E_EVAL
  * @{
  */
    
/** @defgroup STM32446E_EVAL_TS STM32446E EVAL TS
  * @{
  */    

/** @defgroup STM32446E_EVAL_TS_Exported_Types STM32446E EVAL TS Exported Types
  * @{
  */
typedef struct
{
  uint16_t TouchDetected;
  uint16_t x;
  uint16_t y;
  uint16_t z;
}TS_StateTypeDef;
/**
  * @}
  */ 

/** @defgroup STM32446E_EVAL_TS_Exported_Constants STM32446E EVAL TS Exported Constants
  * @{
  */
#define TS_SWAP_NONE                    ((uint8_t)0x00)
#define TS_SWAP_X                       ((uint8_t)0x01)
#define TS_SWAP_Y                       ((uint8_t)0x02)
#define TS_SWAP_XY                      ((uint8_t)0x04)

typedef enum 
{
  TS_OK       = 0x00,
  TS_ERROR    = 0x01,
  TS_TIMEOUT  = 0x02
}TS_StatusTypeDef;

/**
  * @}
  */ 

/** @defgroup STM32446E_EVAL_TS_Exported_Macros STM32446E EVAL TS Exported Macros
  * @{
  */ 
/**
  * @}
  */ 

/** @defgroup STM32446E_EVAL_TS_Exported_Functions  STM32446E EVAL TS Exported Functions
  * @{
  */
uint8_t BSP_TS_Init(uint16_t xSize, uint16_t ySize);
uint8_t BSP_TS_DeInit(void);
uint8_t BSP_TS_GetState(TS_StateTypeDef *TS_State);
uint8_t BSP_TS_ITEnable(void);
uint8_t BSP_TS_ITDisable(void);
uint8_t BSP_TS_ITConfig(void);
uint8_t BSP_TS_ITGetStatus(void);
void    BSP_TS_ITClear(void);
void    BSP_TS_FIFOClear(void);

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

#endif /* __STM32446E_EVAL_TS_H */
