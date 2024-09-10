/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : pdm2pcm.h
  * Description        : This file provides code for the configuration
  *                      of the pdm2pcm instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __pdm2pcm_H
#define __pdm2pcm_H
#ifdef __cplusplus
  extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "pdm2pcm_glo.h"

/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/* Global variables ---------------------------------------------------------*/
extern PDM_Filter_Handler_t PDM1_filter_handler;
extern PDM_Filter_Config_t PDM1_filter_config;

/* USER CODE BEGIN 1 */
/* USER CODE END 1 */

/* PDM2PCM init function */
void MX_PDM2PCM_Init(void);

/* USER CODE BEGIN 2 */

/* PDM2PCM process function */
uint8_t MX_PDM2PCM_Process(uint16_t *PDMBuf, uint16_t *PCMBuf);

/* USER CODE END 2 */

/* USER CODE BEGIN 3 */
/* USER CODE END 3 */

#ifdef __cplusplus
}
#endif
#endif /*__pdm2pcm_H */

/**
  * @}
  */
