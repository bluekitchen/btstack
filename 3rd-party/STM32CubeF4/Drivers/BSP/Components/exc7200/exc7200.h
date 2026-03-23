/**
  ******************************************************************************
  * @file    exc7200.h
  * @author  MCD Application Team
  * @brief   This file contains all the functions prototypes for the
  *          exc7200.c IO expander driver.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2015 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */ 

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __EXC7200_H
#define __EXC7200_H

#ifdef __cplusplus
 extern "C" {
#endif   
   
/* Includes ------------------------------------------------------------------*/
#include "../Common/ts.h"

/** @addtogroup BSP
  * @{
  */ 

/** @addtogroup Component
  * @{
  */
    
/** @defgroup EXC7200
  * @{
  */    

/* Exported types ------------------------------------------------------------*/

/** @defgroup EXC7200_Exported_Types
  * @{
  */ 

/* Exported constants --------------------------------------------------------*/
  
/** @defgroup EXC7200_Exported_Constants
  * @{
  */ 

/*  */   
#define EXC7200_READ_CMD                             0x09  

/**
  * @}
  */ 
  
/* Exported macro ------------------------------------------------------------*/
   
/** @defgroup exc7200_Exported_Macros
  * @{
  */ 

/* Exported functions --------------------------------------------------------*/
  
/** @defgroup exc7200_Exported_Functions
  * @{
  */

/** 
  * @brief exc7200 Control functions
  */
void     exc7200_Init(uint16_t DeviceAddr);
void     exc7200_Reset(uint16_t DeviceAddr);
uint16_t exc7200_ReadID(uint16_t DeviceAddr);
void     exc7200_TS_Start(uint16_t DeviceAddr);
uint8_t  exc7200_TS_DetectTouch(uint16_t DeviceAddr);
void     exc7200_TS_GetXY(uint16_t DeviceAddr, uint16_t *X, uint16_t *Y);
void     exc7200_TS_EnableIT(uint16_t DeviceAddr);
void     exc7200_TS_DisableIT(uint16_t DeviceAddr);
uint8_t  exc7200_TS_ITStatus (uint16_t DeviceAddr);
void     exc7200_TS_ClearIT (uint16_t DeviceAddr);

void     IOE_Init(void);
void     IOE_Delay(uint32_t delay);
uint8_t  IOE_Read(uint8_t addr, uint8_t reg);
uint16_t IOE_ReadMultiple(uint8_t addr, uint8_t reg, uint8_t *buffer, uint16_t length);
void     IOE_WriteMultiple(uint8_t addr, uint8_t reg, uint8_t *buffer, uint16_t length);

/* Touch screen driver structure */
extern TS_DrvTypeDef exc7200_ts_drv;

#ifdef __cplusplus
}
#endif
#endif /* __EXC7200_H */


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
