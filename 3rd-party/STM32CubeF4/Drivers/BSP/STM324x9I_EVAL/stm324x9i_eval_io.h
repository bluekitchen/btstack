/**
  ******************************************************************************
  * @file    stm324x9i_eval_io.h
  * @author  MCD Application Team
  * @brief   This file contains the common defines and functions prototypes for
  *          the stm324x9i_eval_io.c driver.
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
#ifndef __STM324x9I_EVAL_IO_H
#define __STM324x9I_EVAL_IO_H

#ifdef __cplusplus
 extern "C" {
#endif   
   
/* Includes ------------------------------------------------------------------*/
#include "stm324x9i_eval.h"
/* Include IO component driver */
#include "../Components/stmpe1600/stmpe1600.h"  
   
/** @addtogroup BSP
  * @{
  */ 

/** @addtogroup STM324x9I_EVAL
  * @{
  */
    
/** @addtogroup STM324x9I_EVAL_IO
  * @{
  */    

/** @defgroup STM324x9I_EVAL_IO_Exported_Types STM324x9I EVAL IO Exported Types
  * @{
  */
typedef struct
{
  uint16_t TouchDetected;
  uint16_t x;
  uint16_t y;
  uint16_t z;
}IO_StateTypeDef;   

typedef enum 
{
  IO_OK       = 0,
  IO_ERROR    = 1,
  IO_TIMEOUT  = 2
}IO_StatusTypeDef;
/**
  * @}
  */

/** @defgroup STM324x9I_EVAL_IO_Exported_Constants IO Exported Constants
  * @{
  */    
#define IO_PIN_0                  0x0001
#define IO_PIN_1                  0x0002
#define IO_PIN_2                  0x0004
#define IO_PIN_3                  0x0008
#define IO_PIN_4                  0x0010
#define IO_PIN_5                  0x0020
#define IO_PIN_6                  0x0040
#define IO_PIN_7                  0x0080
#define IO_PIN_8                  0x0100
#define IO_PIN_9                  0x0200
#define IO_PIN_10                 0x0400
#define IO_PIN_11                 0x0800
#define IO_PIN_12                 0x1000
#define IO_PIN_13                 0x2000
#define IO_PIN_14                 0x4000
#define IO_PIN_15                 0x8000
#define IO_PIN_ALL                0xFFFF  
/**
  * @}
  */

/** @defgroup STM324x9I_EVAL_IO_Exported_Functions STM324x9I EVAL IO Exported Functions
  * @{
  */
uint8_t  BSP_IO_Init(void);
uint8_t  BSP_IO_ITGetStatus(uint16_t IO_Pin);
void     BSP_IO_ITClear(void);
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

#endif /* __STM324x9I_EVAL_IO_H */
