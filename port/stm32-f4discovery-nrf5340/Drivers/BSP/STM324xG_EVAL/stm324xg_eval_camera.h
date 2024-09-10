/**
  ******************************************************************************
  * @file    stm324xg_eval_camera.h
  * @author  MCD Application Team
  * @brief   This file contains all the functions prototypes for the 
  *          stm324xg_eval_camera.c driver.
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
#ifndef __STM324xG_EVAL_CAMERA_H
#define __STM324xG_EVAL_CAMERA_H

#ifdef __cplusplus
 extern "C" {
#endif 

/* Includes ------------------------------------------------------------------*/
#include "stm324xg_eval.h"
#include "stm324xg_eval_io.h"
   
/* Include Camera component Driver */  
#include "../Components/ov2640/ov2640.h"   
   
/** @addtogroup BSP
  * @{
  */

/** @addtogroup STM324xG_EVAL
  * @{
  */
    
/** @addtogroup STM324xG_EVAL_CAMERA
  * @{
  */ 

/** @defgroup STM324xG_EVAL_CAMERA_Exported_Types STM324xG EVAL CAMERA Exported Types
  * @{
  */
   
/** 
  * @brief Camera status structure definition  
  */   
typedef enum 
{
  CAMERA_OK       = 0x00,
  CAMERA_ERROR    = 0x01,
  CAMERA_TIMEOUT  = 0x02
}Camera_StatusTypeDef;

#define RESOLUTION_R160x120      CAMERA_R160x120  /* QQVGA Resolution */
#define RESOLUTION_R320x240      CAMERA_R320x240  /* QVGA Resolution */
        
/**
  * @}
  */ 

/** @defgroup STM324xG_EVAL_CAMERA_Exported_Constants STM324xG EVAL CAMERA Exported Constants
  * @{
  */
#define CAMERA_I2C_ADDRESS 0x60  
/**
  * @}
  */  
       
/** @defgroup STM324xG_EVAL_CAMERA_Exported_Functions STM324xG EVAL CAMERA Exported Functions
  * @{
  */        
uint8_t BSP_CAMERA_Init(uint32_t Resolution);
void    BSP_CAMERA_ContinuousStart(uint8_t *buff);
void    BSP_CAMERA_SnapshotStart(uint8_t *buff);
void    BSP_CAMERA_Suspend(void);
void    BSP_CAMERA_Resume(void); 
uint8_t BSP_CAMERA_Stop(void);
void    BSP_CAMERA_LineEventCallback(void);
void    BSP_CAMERA_VsyncEventCallback(void);
void    BSP_CAMERA_FrameEventCallback(void);
void    BSP_CAMERA_ErrorCallback(void);
void    BSP_CAMERA_MspInit(void);

/* Camera features functions prototype */
void    BSP_CAMERA_ContrastBrightnessConfig(uint32_t contrast_level, uint32_t brightness_level);
void    BSP_CAMERA_BlackWhiteConfig(uint32_t Mode);
void    BSP_CAMERA_ColorEffectConfig(uint32_t Effect);

/* To be called in DCMI_IRQHandler function */
void    BSP_CAMERA_IRQHandler(void);
/* To be called in DMA2_Stream1_IRQHandler function */
void    BSP_CAMERA_DMA_IRQHandler(void);

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

#endif /* __STM324xG_EVAL_CAMERA_H */
