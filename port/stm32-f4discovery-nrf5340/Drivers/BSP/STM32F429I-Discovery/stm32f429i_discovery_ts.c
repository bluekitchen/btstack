/**
  ******************************************************************************
  * @file    stm32f429i_discovery_ts.c
  * @author  MCD Application Team
  * @brief   This file provides a set of functions needed to manage Touch
  *          screen available with STMPE811 IO Expander device mounted on
  *          STM32F429I-Discovery Kit.
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
#include "stm32f429i_discovery_ts.h"
#include "stm32f429i_discovery_io.h"

/** @addtogroup BSP
  * @{
  */

/** @addtogroup STM32F429I_DISCOVERY
  * @{
  */

/** @defgroup STM32F429I_DISCOVERY_TS STM32F429I DISCOVERY TS
  * @{
  */

/** @defgroup STM32F429I_DISCOVERY_TS_Private_Types_Definitions STM32F429I DISCOVERY TS Private Types Definitions
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_TS_Private_Defines STM32F429I DISCOVERY TS Private Defines
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_TS_Private_Macros STM32F429I DISCOVERY TS Private Macros
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_TS_Private_Variables STM32F429I DISCOVERY TS Private Variables
  * @{
  */
static TS_DrvTypeDef     *TsDrv;
static uint16_t          TsXBoundary, TsYBoundary;
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_TS_Private_Function_Prototypes STM32F429I DISCOVERY TS Private Function Prototypes
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F429I_DISCOVERY_TS_Private_Functions STM32F429I DISCOVERY TS Private Functions
  * @{
  */

/**
  * @brief  Initializes and configures the touch screen functionalities and
  *         configures all necessary hardware resources (GPIOs, clocks..).
  * @param  XSize: The maximum X size of the TS area on LCD
  * @param  YSize: The maximum Y size of the TS area on LCD
  * @retval TS_OK: if all initializations are OK. Other value if error.
  */
uint8_t BSP_TS_Init(uint16_t XSize, uint16_t YSize)
{
  uint8_t ret = TS_ERROR;

  /* Initialize x and y positions boundaries */
  TsXBoundary = XSize;
  TsYBoundary = YSize;

  /* Read ID and verify if the IO expander is ready */
  if (stmpe811_ts_drv.ReadID(TS_I2C_ADDRESS) == STMPE811_ID)
  {
    /* Initialize the TS driver structure */
    TsDrv = &stmpe811_ts_drv;

    ret = TS_OK;
  }

  if (ret == TS_OK)
  {
    /* Initialize the LL TS Driver */
    TsDrv->Init(TS_I2C_ADDRESS);
    TsDrv->Start(TS_I2C_ADDRESS);
  }

  return ret;
}

/**
  * @brief  Configures and enables the touch screen interrupts.
  * @retval TS_OK: if ITconfig is OK. Other value if error.
  */
uint8_t BSP_TS_ITConfig(void)
{
  /* Enable the TS ITs */
  TsDrv->EnableIT(TS_I2C_ADDRESS);

  return TS_OK;
}

/**
  * @brief  Gets the TS IT status.
  * @retval Interrupt status.
  */
uint8_t BSP_TS_ITGetStatus(void)
{
  /* Return the TS IT status */
  return (TsDrv->GetITStatus(TS_I2C_ADDRESS));
}

/**
  * @brief  Returns status and positions of the touch screen.
  * @param  TsState: Pointer to touch screen current state structure
  */
void BSP_TS_GetState(TS_StateTypeDef *TsState)
{
  static uint32_t _x = 0, _y = 0;
  uint16_t xDiff, yDiff, x, y, xr, yr;

  TsState->TouchDetected = TsDrv->DetectTouch(TS_I2C_ADDRESS);

  if (TsState->TouchDetected)
  {
    TsDrv->GetXY(TS_I2C_ADDRESS, &x, &y);

#if defined (USE_STM32F429I_DISCOVERY_REVD)
    if (y > 3700)
    {
      y = 3700;
    }
    else if (y < 180)
    {
      y = 180;
    }

    /* Y value first correction */
    y = 3700 - y;
#else

    /* Y value first correction */
    y -= 360;

#endif

    /* Y value second correction */
    yr = y / 11;

    /* Return y position value */
    if (yr <= 0)
    {
      yr = 0;
    }
    else if (yr > TsYBoundary)
    {
      yr = TsYBoundary - 1;
    }
    else
    {}
    y = yr;

    /* X value first correction */
    if (x <= 3000)
    {
      x = 3870 - x;
    }
    else
    {
      x = 3800 - x;
    }

    /* X value second correction */
    xr = x / 15;

    /* Return X position value */
    if (xr <= 0)
    {
      xr = 0;
    }
    else if (xr > TsXBoundary)
    {
      xr = TsXBoundary - 1;
    }
    else
    {}

    x = xr;
    xDiff = x > _x ? (x - _x): (_x - x);
    yDiff = y > _y ? (y - _y) : (_y - y);

    if (xDiff + yDiff > 5)
    {
      _x = x;
      _y = y;
    }

    /* Update the X position */
    TsState->X = _x;

    /* Update the Y position */
    TsState->Y = _y;
  }
}

/**
  * @brief  Clears all touch screen interrupts.
  */
void BSP_TS_ITClear(void)
{
  /* Clear TS IT pending bits */
  TsDrv->ClearIT(TS_I2C_ADDRESS);
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
