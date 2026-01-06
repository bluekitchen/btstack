/**
  ******************************************************************************
  * @file    s5k5cag.h
  * @author  MCD Application Team
  * @brief   This file contains all the functions prototypes for the s5k5cag.c
  *          driver.
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
#ifndef __S5K5CAG_H
#define __S5K5CAG_H

#ifdef __cplusplus
 extern "C" {
#endif 

/* Includes ------------------------------------------------------------------*/
#include "../Common/camera.h"
   
/** @addtogroup BSP
  * @{
  */ 

/** @addtogroup Components
  * @{
  */ 
  
/** @addtogroup s5k5cag
  * @{
  */

/** @defgroup S5K5CAG_Exported_Types
  * @{
  */
     
/**
  * @}
  */ 

/** @defgroup S5K5CAG_Exported_Constants
  * @{
  */
/** 
  * @brief  S5K5CAG ID
  */  
#define  S5K5CAG_ID                      ((uint16_t)0x05CA)
/** 
  * @brief  S5K5CAG Registers
  */
#define S5K5CAG_INFO_CHIPID1             ((uint16_t)0x0040)
#define S5K5CAG_INFO_CHIPID2             ((uint16_t)0x0042)
#define S5K5CAG_INFO_SVNVERSION          ((uint16_t)0x0048)
#define S5K5CAG_INFO_DATE                ((uint16_t)0x004E)

/** 
 * @brief  S5K5CAG Features Parameters
 */
#define S5K5CAG_BRIGHTNESS_LEVEL0        ((uint16_t)0xFF00)  /* Brightness level -2         */
#define S5K5CAG_BRIGHTNESS_LEVEL1        ((uint16_t)0xFFC0)  /* Brightness level -1         */
#define S5K5CAG_BRIGHTNESS_LEVEL2        ((uint16_t)0x0000)  /* Brightness level 0          */
#define S5K5CAG_BRIGHTNESS_LEVEL3        ((uint16_t)0x0050)  /* Brightness level +1         */
#define S5K5CAG_BRIGHTNESS_LEVEL4        ((uint16_t)0x0080)  /* Brightness level +2         */

#define S5K5CAG_BLACK_WHITE_BW           ((uint16_t)0x0001)  /* Black and white effect      */
#define S5K5CAG_BLACK_WHITE_NEGATIVE     ((uint16_t)0x0003)  /* Negative effect             */
#define S5K5CAG_BLACK_WHITE_BW_NEGATIVE  ((uint16_t)0x0002)  /* BW and Negative effect      */
#define S5K5CAG_BLACK_WHITE_NORMAL       ((uint16_t)0x0000)  /* Normal effect               */

#define S5K5CAG_CONTRAST_LEVEL0          ((uint16_t)0xFF80)  /* Contrast level -2           */
#define S5K5CAG_CONTRAST_LEVEL1          ((uint16_t)0xFFC0)  /* Contrast level -1           */
#define S5K5CAG_CONTRAST_LEVEL2          ((uint16_t)0x0000)  /* Contrast level 0            */
#define S5K5CAG_CONTRAST_LEVEL3          ((uint16_t)0x0050)  /* Contrast level -1           */
#define S5K5CAG_CONTRAST_LEVEL4          ((uint16_t)0x0080)  /* Contrast level -2           */

#define S5K5CAG_COLOR_EFFECT_NONE        ((uint16_t)0x0000)  /* No color effect             */
#define S5K5CAG_COLOR_EFFECT_ANTIQUE     ((uint16_t)0x0004)  /* Antique effect              */
#define S5K5CAG_COLOR_EFFECT_BLUE        ((uint16_t)0x0001)  /* Blue effect                 */
#define S5K5CAG_COLOR_EFFECT_GREEN       ((uint16_t)0x0002)  /* Green effect                */
#define S5K5CAG_COLOR_EFFECT_RED         ((uint16_t)0x0003)  /* Red effect                  */
/**
  * @}
  */
  
/** @defgroup S5K5CAG_Exported_Functions
  * @{
  */ 
void     s5k5cag_Init(uint16_t DeviceAddr, uint32_t resolution);
void     s5k5cag_Config(uint16_t DeviceAddr, uint32_t feature, uint32_t value, uint32_t BR_value);
uint16_t s5k5cag_ReadID(uint16_t DeviceAddr);

void     CAMERA_IO_Init(void);
void     CAMERA_IO_Write(uint8_t addr, uint16_t reg, uint16_t value);
uint16_t CAMERA_IO_Read(uint8_t addr, uint16_t reg);
void     CAMERA_Delay(uint32_t delay);

/* CAMERA driver structure */
extern CAMERA_DrvTypeDef   s5k5cag_drv;
/**
  * @}
  */    
#ifdef __cplusplus
}
#endif

#endif /* __S5K5CAG_H */
/**
  * @}
  */ 

/**
  * @}
  */ 

/**
  * @}
  */
