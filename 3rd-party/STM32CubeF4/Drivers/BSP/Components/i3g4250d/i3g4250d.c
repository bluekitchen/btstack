/**
  ******************************************************************************
  * @file    i3g4250d.c
  * @author  MCD Application Team
  * @brief   This file provides a set of functions needed to manage the I3G4250D,
  *          ST MEMS motion sensor, 3-axis digital output gyroscope.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "i3g4250d.h"

/** @addtogroup BSP
  * @{
  */

/** @addtogroup Components
  * @{
  */

/** @addtogroup I3G4250D
  * @{
  */

/** @defgroup I3G4250D_Private_TypesDefinitions Private Types Definitions
  * @{
  */

/**
  * @}
  */

/** @defgroup I3G4250D_Private_Defines Private Defines
  * @{
  */

/**
  * @}
  */

/** @defgroup I3G4250D_Private_Macros Private Macros
  * @{
  */

/**
  * @}
  */

/** @defgroup I3G4250D_Private_Variables Private Variables
  * @{
  */
GYRO_DrvTypeDef I3g4250Drv =
{
  I3G4250D_Init,
  I3G4250D_DeInit,
  I3G4250D_ReadID,
  I3G4250D_RebootCmd,
  I3G4250D_LowPower,
  I3G4250D_INT1InterruptConfig,
  I3G4250D_EnableIT,
  I3G4250D_DisableIT,
  0,
  0,
  I3G4250D_FilterConfig,
  I3G4250D_FilterCmd,
  I3G4250D_ReadXYZAngRate
};

/**
  * @}
  */

/** @defgroup I3G4250D_Private_FunctionPrototypes Private Function Prototypes
  * @{
  */

/**
  * @}
  */

/** @defgroup I3G4250D_Private_Functions Private Functions
  * @{
  */

/**
  * @brief  Set I3G4250D Initialization.
  * @param  I3G4250D_InitStruct: pointer to a I3G4250D_InitTypeDef structure
  *         that contains the configuration setting for the I3G4250D.
  * @retval None
  */
void I3G4250D_Init(uint16_t InitStruct)
{
  uint8_t ctrl = 0x00;

  /* Configure the low level interface */
  GYRO_IO_Init();

  /* Write value to MEMS CTRL_REG1 register */
  ctrl = (uint8_t) InitStruct;
  GYRO_IO_Write(&ctrl, I3G4250D_CTRL_REG1_ADDR, 1);

  /* Write value to MEMS CTRL_REG4 register */
  ctrl = (uint8_t)(InitStruct >> 8);
  GYRO_IO_Write(&ctrl, I3G4250D_CTRL_REG4_ADDR, 1);
}



/**
  * @brief I3G4250D De-initialization
  * @param  None
  * @retval None
  */
void I3G4250D_DeInit(void)
{
}

/**
  * @brief  Read ID address of I3G4250D
  * @param  None
  * @retval ID name
  */
uint8_t I3G4250D_ReadID(void)
{
  uint8_t tmp;

  /* Configure the low level interface */
  GYRO_IO_Init();

  /* Read WHO I AM register */
  GYRO_IO_Read(&tmp, I3G4250D_WHO_AM_I_ADDR, 1);

  /* Return the ID */
  return (uint8_t)tmp;
}

/**
  * @brief  Reboot memory content of I3G4250D
  * @param  None
  * @retval None
  */
void I3G4250D_RebootCmd(void)
{
  uint8_t tmpreg;

  /* Read CTRL_REG5 register */
  GYRO_IO_Read(&tmpreg, I3G4250D_CTRL_REG5_ADDR, 1);

  /* Enable or Disable the reboot memory */
  tmpreg |= I3G4250D_BOOT_REBOOTMEMORY;

  /* Write value to MEMS CTRL_REG5 register */
  GYRO_IO_Write(&tmpreg, I3G4250D_CTRL_REG5_ADDR, 1);
}

/**
  * @brief  Set I3G4250D in low-power mode
  * @param  I3G4250D_InitStruct: pointer to a I3G4250D_InitTypeDef structure
  *         that contains the configuration setting for the I3G4250D.
  * @retval None
  */
void I3G4250D_LowPower(uint16_t InitStruct)
{
  uint8_t ctrl = 0x00;

  /* Write value to MEMS CTRL_REG1 register */
  ctrl = (uint8_t) InitStruct;
  GYRO_IO_Write(&ctrl, I3G4250D_CTRL_REG1_ADDR, 1);
}

/**
  * @brief  Set I3G4250D Interrupt INT1 configuration
  * @param  Int1Config: the configuration setting for the I3G4250D Interrupt.
  * @retval None
  */
void I3G4250D_INT1InterruptConfig(uint16_t Int1Config)
{
  uint8_t ctrl_cfr = 0x00, ctrl3 = 0x00;

  /* Read INT1_CFG register */
  GYRO_IO_Read(&ctrl_cfr, I3G4250D_INT1_CFG_ADDR, 1);

  /* Read CTRL_REG3 register */
  GYRO_IO_Read(&ctrl3, I3G4250D_CTRL_REG3_ADDR, 1);

  ctrl_cfr &= 0x80;
  ctrl_cfr |= ((uint8_t) Int1Config >> 8);

  ctrl3 &= 0xDF;
  ctrl3 |= ((uint8_t) Int1Config);

  /* Write value to MEMS INT1_CFG register */
  GYRO_IO_Write(&ctrl_cfr, I3G4250D_INT1_CFG_ADDR, 1);

  /* Write value to MEMS CTRL_REG3 register */
  GYRO_IO_Write(&ctrl3, I3G4250D_CTRL_REG3_ADDR, 1);
}

/**
  * @brief  Enable INT1 or INT2 interrupt
  * @param  IntSel: choice of INT1 or INT2
  *      This parameter can be:
  *        @arg I3G4250D_INT1
  *        @arg I3G4250D_INT2
  * @retval None
  */
void I3G4250D_EnableIT(uint8_t IntSel)
{
  uint8_t tmpreg;

  /* Read CTRL_REG3 register */
  GYRO_IO_Read(&tmpreg, I3G4250D_CTRL_REG3_ADDR, 1);

  if (IntSel == I3G4250D_INT1)
  {
    tmpreg &= 0x7F;
    tmpreg |= I3G4250D_INT1INTERRUPT_ENABLE;
  }
  else if (IntSel == I3G4250D_INT2)
  {
    tmpreg &= 0xF7;
    tmpreg |= I3G4250D_INT2INTERRUPT_ENABLE;
  }

  /* Write value to MEMS CTRL_REG3 register */
  GYRO_IO_Write(&tmpreg, I3G4250D_CTRL_REG3_ADDR, 1);
}

/**
  * @brief  Disable  INT1 or INT2 interrupt
  * @param  IntSel: choice of INT1 or INT2
  *      This parameter can be:
  *        @arg I3G4250D_INT1
  *        @arg I3G4250D_INT2
  * @retval None
  */
void I3G4250D_DisableIT(uint8_t IntSel)
{
  uint8_t tmpreg;

  /* Read CTRL_REG3 register */
  GYRO_IO_Read(&tmpreg, I3G4250D_CTRL_REG3_ADDR, 1);

  if (IntSel == I3G4250D_INT1)
  {
    tmpreg &= 0x7F;
    tmpreg |= I3G4250D_INT1INTERRUPT_DISABLE;
  }
  else if (IntSel == I3G4250D_INT2)
  {
    tmpreg &= 0xF7;
    tmpreg |= I3G4250D_INT2INTERRUPT_DISABLE;
  }

  /* Write value to MEMS CTRL_REG3 register */
  GYRO_IO_Write(&tmpreg, I3G4250D_CTRL_REG3_ADDR, 1);
}

/**
  * @brief  Set High Pass Filter Modality
  * @param  FilterStruct: contains the configuration setting for the L3GD20.
  * @retval None
  */
void I3G4250D_FilterConfig(uint8_t FilterStruct)
{
  uint8_t tmpreg;

  /* Read CTRL_REG2 register */
  GYRO_IO_Read(&tmpreg, I3G4250D_CTRL_REG2_ADDR, 1);

  tmpreg &= 0xC0;

  /* Configure MEMS: mode and cutoff frequency */
  tmpreg |= FilterStruct;

  /* Write value to MEMS CTRL_REG2 register */
  GYRO_IO_Write(&tmpreg, I3G4250D_CTRL_REG2_ADDR, 1);
}

/**
  * @brief  Enable or Disable High Pass Filter
  * @param  HighPassFilterState: new state of the High Pass Filter feature.
  *      This parameter can be:
  *         @arg: I3G4250D_HIGHPASSFILTER_DISABLE
  *         @arg: I3G4250D_HIGHPASSFILTER_ENABLE
  * @retval None
  */
void I3G4250D_FilterCmd(uint8_t HighPassFilterState)
{
  uint8_t tmpreg;

  /* Read CTRL_REG5 register */
  GYRO_IO_Read(&tmpreg, I3G4250D_CTRL_REG5_ADDR, 1);

  tmpreg &= 0xEF;

  tmpreg |= HighPassFilterState;

  /* Write value to MEMS CTRL_REG5 register */
  GYRO_IO_Write(&tmpreg, I3G4250D_CTRL_REG5_ADDR, 1);
}

/**
  * @brief  Get status for I3G4250D data
  * @param  None
  * @retval Data status in a I3G4250D Data
  */
uint8_t I3G4250D_GetDataStatus(void)
{
  uint8_t tmpreg;

  /* Read STATUS_REG register */
  GYRO_IO_Read(&tmpreg, I3G4250D_STATUS_REG_ADDR, 1);

  return tmpreg;
}

/**
* @brief  Calculate the I3G4250D angular data.
* @param  pfData: Data out pointer
* @retval None
*/
void I3G4250D_ReadXYZAngRate(float *pfData)
{
  uint8_t tmpbuffer[6] = {0};
  int16_t RawData[3] = {0};
  uint8_t tmpreg = 0;
  float sensitivity = 0;
  int i = 0;

  GYRO_IO_Read(&tmpreg, I3G4250D_CTRL_REG4_ADDR, 1);

  GYRO_IO_Read(tmpbuffer, I3G4250D_OUT_X_L_ADDR, 6);

  /* check in the control register 4 the data alignment (Big Endian or Little Endian)*/
  if (!(tmpreg & I3G4250D_BLE_MSB))
  {
    for (i = 0; i < 3; i++)
    {
      RawData[i] = (int16_t)(((uint16_t)tmpbuffer[2 * i + 1] << 8) + tmpbuffer[2 * i]);
    }
  }
  else
  {
    for (i = 0; i < 3; i++)
    {
      RawData[i] = (int16_t)(((uint16_t)tmpbuffer[2 * i] << 8) + tmpbuffer[2 * i + 1]);
    }
  }

  /* Switch the sensitivity value set in the CRTL4 */
  switch (tmpreg & I3G4250D_FULLSCALE_SELECTION)
  {
    case I3G4250D_FULLSCALE_245:
      sensitivity = I3G4250D_SENSITIVITY_245DPS;
      break;

    case I3G4250D_FULLSCALE_500:
      sensitivity = I3G4250D_SENSITIVITY_500DPS;
      break;

    case I3G4250D_FULLSCALE_2000:
      sensitivity = I3G4250D_SENSITIVITY_2000DPS;
      break;
  }
  /* Multiplied by sensitivity */
  for (i = 0; i < 3; i++)
  {
    pfData[i] = (float)(RawData[i] * sensitivity);
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
