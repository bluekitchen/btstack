/**
  ******************************************************************************
  * @file    stm324xg_eval_io.h
  * @author  MCD Application Team
  * @brief   This file contains the common defines and functions prototypes for
  *          the stm324xg_eval_io.c driver.
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
#ifndef __STM324xG_EVAL_IO_H
#define __STM324xG_EVAL_IO_H

#ifdef __cplusplus
 extern "C" {
#endif   
   
/* Includes ------------------------------------------------------------------*/
#include "stm324xg_eval.h"
/* Include IO component driver */
#include "../Components/stmpe811/stmpe811.h"
   
/** @addtogroup BSP
  * @{
  */ 

/** @addtogroup STM324xG_EVAL
  * @{
  */
    
/** @defgroup STM324xG_EVAL_IO STM324xG EVAL IO
  * @{
  */    

/** @defgroup STM324xG_EVAL_IO_Exported_Types STM324xG EVAL IO Exported Types
  * @{
  */
typedef enum 
{
  IO_OK       = 0x00,
  IO_ERROR    = 0x01,
  IO_TIMEOUT  = 0x02
}IO_StatusTypeDef;
/**
  * @}
  */ 

/** @defgroup STM324xG_EVAL_IO_Exported_Constants STM324xG EVAL IO Exported Constants
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

/** @defgroup STM324xG_EVAL_IO_Exported_Functions STM324xG EVAL IO Exported Functions
  * @{
  */
uint8_t  BSP_IO_Init(void);
void     BSP_IO_ITClear(uint16_t IO_Pin);
uint8_t  BSP_IO_ITGetStatus(uint16_t IO_Pin);
uint8_t  BSP_IO_ConfigPin(uint16_t IO_Pin, IO_ModeTypedef IO_Mode);
void     BSP_IO_WritePin(uint16_t IO_Pin, uint8_t PinState);
uint16_t BSP_IO_ReadPin(uint16_t IO_Pin);
void     BSP_IO_TogglePin(uint16_t IO_Pin);

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

#endif /* __STM324xG_EVAL_IO_H */
