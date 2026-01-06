/**
  ******************************************************************************
  * @file    stm32f429i_discovery_io.c
  * @author  MCD Application Team
  * @brief   This file provides a set of functions needed to manage the STMPE811
  *          IO Expander device mounted on STM32F429I-Discovery Kit.
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

/* Includes ------------------------------------------------------------------*/
#include "stm32f429i_discovery_io.h"

/** @addtogroup BSP
  * @{
  */

/** @addtogroup STM32F429I_DISCOVERY
  * @{
  */

/** @defgroup STM32F429I_DISCOVERY_IO STM32F429I DISCOVERY IO
  * @{
  */

/** @defgroup STM32F429I_DISCOVERY_IO_Private_Types_Definitions STM32F429I DISCOVERY IO Private Types Definitions
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_IO_Private_Defines STM32F429I DISCOVERY IO Private Defines
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_IO_Private_Macros STM32F429I DISCOVERY IO Private Macros
  * @{
  */
/**
  * @}
  */


/** @defgroup STM32F429I_DISCOVERY_IO_Private_Variables STM32F429I DISCOVERY IO Private Variables
  * @{
  */
static IO_DrvTypeDef *IoDrv;

/**
  * @}
  */


/** @defgroup STM32F429I_DISCOVERY_IO_Private_Function_Prototypes STM32F429I DISCOVERY IO Private Function Prototypes
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_IO_Private_Functions STM32F429I DISCOVERY IO Private Functions
  * @{
  */

/**
  * @brief  Initializes and configures the IO functionalities and configures all
  *         necessary hardware resources (GPIOs, clocks..).
  * @note   BSP_IO_Init() is using HAL_Delay() function to ensure that stmpe811
  *         IO Expander is correctly reset. HAL_Delay() function provides accurate
  *         delay (in milliseconds) based on variable incremented in SysTick ISR.
  *         This implies that if BSP_IO_Init() is called from a peripheral ISR process,
  *         then the SysTick interrupt must have higher priority (numerically lower)
  *         than the peripheral interrupt. Otherwise the caller ISR process will be blocked.
  * @retval IO_OK if all initializations done correctly. Other value if error.
  */
uint8_t BSP_IO_Init(void)
{
  uint8_t ret = IO_ERROR;

  /* Read ID and verify the IO expander is ready */
  if (stmpe811_io_drv.ReadID(IO_I2C_ADDRESS) == STMPE811_ID)
  {
    /* Initialize the IO driver structure */
    IoDrv = &stmpe811_io_drv;
    ret = IO_OK;
  }

  if (ret == IO_OK)
  {
    IoDrv->Init(IO_I2C_ADDRESS);
    IoDrv->Start(IO_I2C_ADDRESS, IO_PIN_ALL);
  }
  return ret;
}

/**
  * @brief  Gets the selected pins IT status.
  * @param  IoPin: The selected pins to check the status.
  *         This parameter could be any combination of the IO pins.
  * @retval Status of IO Pin checked.
  */
uint8_t BSP_IO_ITGetStatus(uint16_t IoPin)
{
  /* Return the IO Pin IT status */
  return (IoDrv->ITStatus(IO_I2C_ADDRESS, IoPin));
}

/**
  * @brief  Clears all the IO IT pending bits
  */
void BSP_IO_ITClear(void)
{
  /* Clear all IO IT pending bits */
  IoDrv->ClearIT(IO_I2C_ADDRESS, IO_PIN_ALL);
}

/**
  * @brief  Configures the IO pin(s) according to IO mode structure value.
  * @param  IoPin: IO pin(s) to be configured.
  *         This parameter could be any combination of the following values:
  *   @arg  STMPE811_PIN_x: where x can be from 0 to 7.
  * @param  IoMode: The IO pin mode to configure, could be one of the following values:
  *   @arg  IO_MODE_INPUT
  *   @arg  IO_MODE_OUTPUT
  *   @arg  IO_MODE_IT_RISING_EDGE
  *   @arg  IO_MODE_IT_FALLING_EDGE
  *   @arg  IO_MODE_IT_LOW_LEVEL
  *   @arg  IO_MODE_IT_HIGH_LEVEL
  */
void BSP_IO_ConfigPin(uint16_t IoPin, IO_ModeTypedef IoMode)
{
  /* Configure the selected IO pin(s) mode */
  IoDrv->Config(IO_I2C_ADDRESS, IoPin, IoMode);
}

/**
  * @brief  Sets the selected pins state.
  * @param  IoPin: The selected pins to write.
  *         This parameter could be any combination of the IO pins.
  * @param  PinState: the new pins state to write
  */
void BSP_IO_WritePin(uint16_t IoPin, uint8_t PinState)
{
  /* Set the Pin state */
  IoDrv->WritePin(IO_I2C_ADDRESS, IoPin, PinState);
}

/**
  * @brief  Gets the selected pins current state.
  * @param  IoPin: The selected pins to read.
  *         This parameter could be any combination of the IO pins.
  * @retval The current pins state
  */
uint16_t BSP_IO_ReadPin(uint16_t IoPin)
{
  return (IoDrv->ReadPin(IO_I2C_ADDRESS, IoPin));
}

/**
  * @brief  Toggles the selected pins state.
  * @param  IoPin: The selected pins to toggle.
  *         This parameter could be any combination of the IO pins.
  */
void BSP_IO_TogglePin(uint16_t IoPin)
{
  /* Toggle the current pin state */
  if (IoDrv->ReadPin(IO_I2C_ADDRESS, IoPin) == 1 /* Set */)
  {
    IoDrv->WritePin(IO_I2C_ADDRESS, IoPin, 0 /* Reset */);
  }
  else
  {
    IoDrv->WritePin(IO_I2C_ADDRESS, IoPin, 1 /* Set */);
  }
}

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
