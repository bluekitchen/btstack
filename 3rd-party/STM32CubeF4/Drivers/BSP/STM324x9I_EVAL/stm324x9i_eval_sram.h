/**
  ******************************************************************************
  * @file    stm324x9i_eval_sram.h
  * @author  MCD Application Team
  * @brief   This file contains the common defines and functions prototypes for
  *          the stm324x9i_eval_sram.c driver.
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
#ifndef __STM324x9I_EVAL_SRAM_H
#define __STM324x9I_EVAL_SRAM_H

#ifdef __cplusplus
 extern "C" {
#endif 

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/** @addtogroup BSP
  * @{
  */ 

/** @addtogroup STM324x9I_EVAL
  * @{
  */
    
/** @defgroup STM324x9I_EVAL_SRAM STM324x9I EVAL SRAM
  * @{
  */    

/** @defgroup STM324x9I_EVAL_SRAM_Exported_Constants STM324x9I EVAL SRAM Exported Constants
  * @{
  */ 

/** 
  * @brief  SRAM status structure definition  
  */     
#define   SRAM_OK         0x00
#define   SRAM_ERROR      0x01

#define SRAM_DEVICE_ADDR  ((uint32_t)0x64000000)
#define SRAM_DEVICE_SIZE  ((uint32_t)0x200000)  /* SRAM device size in Bytes */  
  
/* #define SRAM_MEMORY_WIDTH    FMC_NORSRAM_MEM_BUS_WIDTH_8  */
#define SRAM_MEMORY_WIDTH    FMC_NORSRAM_MEM_BUS_WIDTH_16

#define SRAM_BURSTACCESS    FMC_BURST_ACCESS_MODE_DISABLE  
/* #define SRAM_BURSTACCESS    FMC_BURST_ACCESS_MODE_ENABLE*/
  
#define SRAM_WRITEBURST    FMC_WRITE_BURST_DISABLE  
/* #define SRAM_WRITEBURST   FMC_WRITE_BURST_ENABLE */
 
#define CONTINUOUSCLOCK_FEATURE    FMC_CONTINUOUS_CLOCK_SYNC_ONLY 
/* #define CONTINUOUSCLOCK_FEATURE     FMC_CONTINUOUS_CLOCK_SYNC_ASYNC */ 

/* DMA definitions for SRAM DMA transfer */
#define __SRAM_DMAx_CLK_ENABLE            __HAL_RCC_DMA2_CLK_ENABLE
#define SRAM_DMAx_CHANNEL                 DMA_CHANNEL_0
#define SRAM_DMAx_STREAM                  DMA2_Stream0  
#define SRAM_DMAx_IRQn                    DMA2_Stream0_IRQn
#define SRAM_DMAx_IRQHandler              DMA2_Stream0_IRQHandler  
/**
  * @}
  */ 
   
/** @defgroup STM324x9I_EVAL_SRAM_Exported_Functions STM324x9I EVAL SRAM Exported Functions
  * @{
  */    
uint8_t BSP_SRAM_Init(void);
uint8_t BSP_SRAM_ReadData(uint32_t uwStartAddress, uint16_t *pData, uint32_t uwDataSize);
uint8_t BSP_SRAM_ReadData_DMA(uint32_t uwStartAddress, uint16_t *pData, uint32_t uwDataSize);
uint8_t BSP_SRAM_WriteData(uint32_t uwStartAddress, uint16_t *pData, uint32_t uwDataSize);
uint8_t BSP_SRAM_WriteData_DMA(uint32_t uwStartAddress, uint16_t *pData, uint32_t uwDataSize);
void    BSP_SRAM_DMA_IRQHandler(void);
void    BSP_SRAM_MspInit(void);
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

#endif /* __STM324x9I_EVAL_SRAM_H */
