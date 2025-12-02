/**
  ******************************************************************************
  * @file    ampire480272.h
  * @author  MCD Application Team
  * @brief   This file contains all the constants parameters for the ampire480272
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
#ifndef __AMPIRE480272_H
#define __AMPIRE480272_H

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
  
/** @addtogroup ampire480272
  * @{
  */

/** @defgroup AMPIRE480272_Exported_Types
  * @{
  */
   
/**
  * @}
  */ 

/** @defgroup AMPIRE480272_Exported_Constants
  * @{
  */
  
/** 
  * @brief  AMPIRE480272 Size  
  */     
#define  AMPIRE480272_WIDTH    ((uint16_t)480)          /* LCD PIXEL WIDTH            */
#define  AMPIRE480272_HEIGHT   ((uint16_t)272)          /* LCD PIXEL HEIGHT           */

/** 
  * @brief  AMPIRE480272 Timing  
  */     
#define  AMPIRE480272_HSYNC            ((uint16_t)41)   /* Horizontal synchronization */
#define  AMPIRE480272_HBP              ((uint16_t)2)    /* Horizontal back porch      */ 
#define  AMPIRE480272_HFP              ((uint16_t)2)    /* Horizontal front porch     */
#define  AMPIRE480272_VSYNC            ((uint16_t)10)   /* Vertical synchronization   */
#define  AMPIRE480272_VBP              ((uint16_t)2)    /* Vertical back porch        */
#define  AMPIRE480272_VFP              ((uint16_t)2)    /* Vertical front porch       */

/** 
  * @brief  AMPIRE480272 frequency divider  
  */    
#define  AMPIRE480272_FREQUENCY_DIVIDER    5            /* LCD Frequency divider      */
/**
  * @}
  */
  
/** @defgroup AMPIRE480272_Exported_Functions
  * @{
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

/**
  * @}
  */  
#ifdef __cplusplus
}
#endif

#endif /* __AMPIRE480272_H */
