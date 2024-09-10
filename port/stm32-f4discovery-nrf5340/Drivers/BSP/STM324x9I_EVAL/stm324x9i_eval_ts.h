/**
  ******************************************************************************
  * @file    stm324x9i_eval_ts.h
  * @author  MCD Application Team
  * @brief   This file contains the common defines and functions prototypes for
  *          the stm324x9i_eval_ts.c driver.
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
#ifndef __STM324x9I_EVAL_TS_H
#define __STM324x9I_EVAL_TS_H

#ifdef __cplusplus
 extern "C" {
#endif   
   
/* Includes ------------------------------------------------------------------*/
#include "stm324x9i_eval.h"
/* Include IOExpander(STMPE811) component Driver */ 
#include "../Components/stmpe811/stmpe811.h"
/* Include TouchScreen component drivers */
#include "../Components/ts3510/ts3510.h"
#include "../Components/exc7200/exc7200.h"   
/** @addtogroup BSP
  * @{
  */ 

/** @addtogroup STM324x9I_EVAL
  * @{
  */
    
/** @addtogroup STM324x9I_EVAL_TS
  * @{
  */    

/** @defgroup STM324x9I_EVAL_TS_Exported_Types STM324x9I EVAL TS Exported Types
  * @{
  */
typedef struct
{
  uint16_t TouchDetected;
  uint16_t x;
  uint16_t y;
  uint16_t z;
}TS_StateTypeDef;

typedef enum 
{
  TS_OK       = 0x00,
  TS_ERROR    = 0x01,
  TS_TIMEOUT  = 0x02
}TS_StatusTypeDef;

/**
  * @}
  */ 

/** @defgroup STM324x9I_EVAL_TS_Exported_Constants STM324x9I EVAL TS Exported Constants
  * @{
  */
#define TS_SWAP_NONE                    0x00
#define TS_SWAP_X                       0x01
#define TS_SWAP_Y                       0x02
#define TS_SWAP_XY                      0x04

/* Interrupt sources pins definition */
#define TS_INT_PIN                      0x0010
/**
  * @}
  */ 

/** @defgroup STM324x9I_EVAL_TS_Exported_Functions STM324x9I EVAL TS Exported Functions
  * @{
  */
uint8_t BSP_TS_Init(uint16_t xSize, uint16_t ySize);
uint8_t BSP_TS_DeInit(void);
uint8_t BSP_TS_GetState(TS_StateTypeDef *TS_State);
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

#endif /* __STM324x9I_EVAL_TS_H */
