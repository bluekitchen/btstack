/**
  ******************************************************************************
  * @file    ampire640480.h
  * @author  MCD Application Team
  * @brief   This file contains all the constants parameters for the ampire640480
  *          LCD component.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2014 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */ 

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __AMPIRE640480_H
#define __AMPIRE640480_H

#ifdef __cplusplus
 extern "C" {
#endif 

/* Includes ------------------------------------------------------------------*/  

/** @addtogroup BSP
  * @{
  */ 

/** @addtogroup Components
  * @{
  */ 
  
/** @addtogroup ampire640480
  * @{
  */

/** @defgroup AMPIRE640480_Exported_Types
  * @{
  */
   
/**
  * @}
  */ 

/** @defgroup AMPIRE640480_Exported_Constants
  * @{
  */
  
/** 
  * @brief  AMPIRE640480 Size  
  */    
#define  AMPIRE640480_WIDTH    ((uint16_t)640)             /* LCD PIXEL WIDTH            */
#define  AMPIRE640480_HEIGHT   ((uint16_t)480)             /* LCD PIXEL HEIGHT           */

/** 
  * @brief  AMPIRE640480 Timing  
  */    
#define  AMPIRE640480_HSYNC            ((uint16_t)30)      /* Horizontal synchronization */
#define  AMPIRE640480_HBP              ((uint16_t)114)     /* Horizontal back porch      */
#define  AMPIRE640480_HFP              ((uint16_t)16)      /* Horizontal front porch     */
#define  AMPIRE640480_VSYNC            ((uint16_t)3)       /* Vertical synchronization   */
#define  AMPIRE640480_VBP              ((uint16_t)32)      /* Vertical back porch        */
#define  AMPIRE640480_VFP              ((uint16_t)10)      /* Vertical front porch       */

/** 
  * @brief  AMPIRE640480 frequency divider  
  */    
#define  AMPIRE640480_FREQUENCY_DIVIDER     3              /* LCD Frequency divider      */
/**
  * @}
  */
  
/** @defgroup AMPIRE640480_Exported_Functions
  * @{
  */    

/**
  * @}
  */    
#ifdef __cplusplus
}
#endif

#endif /* __AMPIRE640480_H */
/**
  * @}
  */ 

/**
  * @}
  */ 

/**
  * @}
  */
