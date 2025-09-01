/**
  ******************************************************************************
  * @file    stm324xg_eval_audio.h
  * @author  MCD Application Team
  * @brief   This file contains the common defines and functions prototypes for
  *          the stm324xg_eval_audio.c driver.
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
#ifndef __STM324xG_EVAL_AUDIO_H
#define __STM324xG_EVAL_AUDIO_H

#ifdef __cplusplus
 extern "C" {
#endif 

/* Includes ------------------------------------------------------------------*/
#include "../Components/cs43l22/cs43l22.h"
#include "stm324xg_eval.h"

/** @addtogroup BSP
  * @{
  */ 

/** @addtogroup STM324xG_EVAL
  * @{
  */
    
/** @addtogroup STM324xG_EVAL_AUDIO
  * @{
  */

/** @defgroup STM324xG_EVAL_AUDIO_Exported_Constants STM324xG EVAL AUDIO Exported Constants
  * @{
  */
/* Audio Reset Pin definition */
#define AUDIO_RESET_PIN                     IO_PIN_2
    
/* I2S peripheral configuration defines */
#define AUDIO_I2Sx                          SPI2
#define AUDIO_I2Sx_CLK_ENABLE()             __HAL_RCC_SPI2_CLK_ENABLE()
#define AUDIO_I2Sx_CLK_DISABLE()            __HAL_RCC_SPI2_CLK_DISABLE()   
#define AUDIO_I2Sx_SCK_SD_WS_AF             GPIO_AF5_SPI2
#define AUDIO_I2Sx_SCK_SD_WS_CLK_ENABLE()   __HAL_RCC_GPIOI_CLK_ENABLE()
#define AUDIO_I2Sx_MCK_CLK_ENABLE()         __HAL_RCC_GPIOC_CLK_ENABLE()
#define AUDIO_I2Sx_WS_PIN                   GPIO_PIN_0
#define AUDIO_I2Sx_SCK_PIN                  GPIO_PIN_1
#define AUDIO_I2Sx_SD_PIN                   GPIO_PIN_3
#define AUDIO_I2Sx_MCK_PIN                  GPIO_PIN_6
#define AUDIO_I2Sx_SCK_SD_WS_GPIO_PORT      GPIOI
#define AUDIO_I2Sx_MCK_GPIO_PORT            GPIOC

/* I2S DMA Stream definitions */
#define AUDIO_I2Sx_DMAx_CLK_ENABLE()        __HAL_RCC_DMA1_CLK_ENABLE()
#define AUDIO_I2Sx_DMAx_STREAM              DMA1_Stream4
#define AUDIO_I2Sx_DMAx_CHANNEL             DMA_CHANNEL_0
#define AUDIO_I2Sx_DMAx_IRQ                 DMA1_Stream4_IRQn
#define AUDIO_I2Sx_DMAx_PERIPH_DATA_SIZE    DMA_PDATAALIGN_HALFWORD
#define AUDIO_I2Sx_DMAx_MEM_DATA_SIZE       DMA_MDATAALIGN_HALFWORD
#define DMA_MAX_SZE                         0xFFFF
   
#define AUDIO_I2Sx_DMAx_IRQHandler          DMA1_Stream4_IRQHandler

/*------------------------------------------------------------------------------
             CONFIGURATION: Audio Driver Configuration parameters
------------------------------------------------------------------------------*/
/* Select the interrupt preemption priority for the DMA interrupt */
#define AUDIO_IRQ_PREPRIO           0x0F   /* Select the preemption priority level(0 is the highest) */

#define AUDIODATA_SIZE              2   /* 16-bits audio data size */

/* Audio status definition */     
#define AUDIO_OK         0x00
#define AUDIO_ERROR      0x01
#define AUDIO_TIMEOUT    0x02

/*------------------------------------------------------------------------------
                    OPTIONAL Configuration defines parameters
------------------------------------------------------------------------------*/
/* Delay for the Codec to be correctly reset */
#define CODEC_RESET_DELAY               5

/**
  * @}
  */ 

/** @defgroup STM324xG_EVAL_AUDIO_Exported_Macros STM324xG EVAL AUDIO Exported Macros
  * @{
  */
#define DMA_MAX(x)           (((x) <= DMA_MAX_SZE)? (x):DMA_MAX_SZE)
/**
  * @}
  */ 

/** @defgroup STM324xG_EVAL_AUDIO_Exported_Functions STM324xG EVAL AUDIO Exported Functions
  * @{
  */
uint8_t BSP_AUDIO_OUT_Init(uint16_t OutputDevice, uint8_t Volume, uint32_t AudioFreq);
void    BSP_AUDIO_OUT_DeInit(void);    
uint8_t BSP_AUDIO_OUT_Play(uint16_t *pBuffer, uint32_t Size);
void    BSP_AUDIO_OUT_ChangeBuffer(uint16_t *pData, uint16_t Size);
uint8_t BSP_AUDIO_OUT_Pause(void);
uint8_t BSP_AUDIO_OUT_Resume(void);
uint8_t BSP_AUDIO_OUT_Stop(uint32_t Option);
uint8_t BSP_AUDIO_OUT_SetVolume(uint8_t Volume);
void    BSP_AUDIO_OUT_SetFrequency(uint32_t AudioFreq);
uint8_t BSP_AUDIO_OUT_SetMute(uint32_t Cmd);
uint8_t BSP_AUDIO_OUT_SetOutputMode(uint8_t Output);

/* User Callbacks: user has to implement these functions in his code if they are needed. */
/* This function is called when the requested data has been completely transferred.*/
void    BSP_AUDIO_OUT_TransferComplete_CallBack(void);

/* This function is called when half of the requested buffer has been transferred. */
void    BSP_AUDIO_OUT_HalfTransfer_CallBack(void);

/* This function is called when an Interrupt due to transfer error on or peripheral
   error occurs. */
void    BSP_AUDIO_OUT_Error_CallBack(void);

/* These function can be modified in case the current settings (e.g. DMA stream)
   need to be changed for specific application needs */
void  BSP_AUDIO_OUT_ClockConfig(I2S_HandleTypeDef *hi2s, uint32_t AudioFreq, void *Params);
void  BSP_AUDIO_OUT_MspInit(I2S_HandleTypeDef *hi2s, void *Params);
void  BSP_AUDIO_OUT_MspDeInit(I2S_HandleTypeDef *hi2s, void *Params);

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

#endif /* __STM324xG_EVAL_AUDIO_H */
