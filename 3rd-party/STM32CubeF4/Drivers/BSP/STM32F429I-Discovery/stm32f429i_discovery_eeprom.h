/**
  ******************************************************************************
  * @file    stm32f429i_discovery_eeprom.h
  * @author  MCD Application Team
  * @brief   This file contains all the functions prototypes for
  *          the stm32f429i_discovery_eeprom.c firmware driver.
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
#ifndef __STM32F429I_DISCOVERY_EEPROM_H
#define __STM32F429I_DISCOVERY_EEPROM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f429i_discovery.h"

/** @addtogroup BSP
  * @{
  */

/** @addtogroup STM32F429I_DISCOVERY
  * @{
  */

/** @addtogroup STM32F429I_DISCOVERY_EEPROM STM32F429I DISCOVERY EEPROM
  * @{
  */

/** @defgroup STM32F429I_DISCOVERY_EEPROM_Exported_Types STM32F429I DISCOVERY EEPROM Exported Types
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_EEPROM_Exported_Constants STM32F429I DISCOVERY EEPROM Exported Constants
  * @{
  */
/* EEPROM hardware address and page size */
#define EEPROM_PAGESIZE             4
#define EEPROM_MAX_SIZE             0x2000 /* 64Kbit*/

/* Maximum Timeout values for flags and events waiting loops.
This timeout is based on systick set to 1ms*/
/* Timeout for read based if read all the EEPROM : EEPROM_MAX_SIZE * BSP_I2C_SPEED (640ms)*/
#define EEPROM_READ_TIMEOUT         ((uint32_t)(1000))
/* Timeout for write based on max write which is EEPROM_PAGESIZE bytes: EEPROM_PAGESIZE * BSP_I2C_SPEED (320us)*/
#define EEPROM_WRITE_TIMEOUT         ((uint32_t)(1))

/* Maximum number of trials for EEPROM_WaitEepromStandbyState() function */
#define EEPROM_MAX_TRIALS           300

#define EEPROM_OK                   0
#define EEPROM_FAIL                 1
#define EEPROM_TIMEOUT              2
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_EEPROM_Exported_Macros STM32F429I DISCOVERY EEPROM Exported Macros
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_EEPROM_Exported_Functions STM32F429I DISCOVERY EEPROM Exported Functions
  * @{
  */
uint32_t BSP_EEPROM_Init(void);
uint32_t BSP_EEPROM_ReadBuffer(uint8_t *pBuffer, uint16_t ReadAddr, uint16_t *NumByteToRead);
uint32_t BSP_EEPROM_WritePage(uint8_t *pBuffer, uint16_t WriteAddr, uint8_t *NumByteToWrite);
uint32_t BSP_EEPROM_WriteBuffer(uint8_t *pBuffer, uint16_t WriteAddr, uint16_t NumByteToWrite);
uint32_t BSP_EEPROM_WaitEepromStandbyState(void);

/* USER Callbacks: This function is declared as __weak in EEPROM driver and
   should be implemented into user application.
   BSP_EEPROM_TIMEOUT_UserCallback() function is called whenever a timeout condition
   occurs during communication (waiting on an event that doesn't occur, bus
   errors, busy devices ...). */
void     BSP_EEPROM_TIMEOUT_UserCallback(void);


/* Link function for I2C EEPROM peripheral */
void              EEPROM_IO_Init(void);
HAL_StatusTypeDef EEPROM_IO_WriteData(uint16_t DevAddress, uint16_t MemAddress, uint8_t *pBuffer, uint32_t BufferSize);
HAL_StatusTypeDef EEPROM_IO_ReadData(uint16_t DevAddress, uint16_t MemAddress, uint8_t *pBuffer, uint32_t BufferSize);
HAL_StatusTypeDef EEPROM_IO_IsDeviceReady(uint16_t DevAddress, uint32_t Trials);

#ifdef __cplusplus
}
#endif

#endif /* __STM32F429I_DISCOVERY_EEPROM_H */

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
