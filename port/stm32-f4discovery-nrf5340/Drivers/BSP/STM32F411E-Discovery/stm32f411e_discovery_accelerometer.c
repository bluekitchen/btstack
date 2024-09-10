/**
  ******************************************************************************
  * @file    stm32f411e_discovery_accelerometer.c
  * @author  MCD Application Team
  * @brief   This file provides a set of functions needed to manage the
  *          MEMS accelerometer available on STM32F411E-Discovery Kit.
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
#include "stm32f411e_discovery_accelerometer.h"

/** @addtogroup BSP
  * @{
  */

/** @addtogroup STM32F411E_DISCOVERY
  * @{
  */

/** @defgroup STM32F411E_DISCOVERY_ACCELEROMETER STM32F411E DISCOVERY ACCELEROMETER
  * @{
  */

/** @defgroup STM32F411E_DISCOVERY_ACCELEROMETER_Private_TypesDefinitions STM32F411E DISCOVERY ACCELEROMETER Private TypesDefinitions
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F411E_DISCOVERY_ACCELEROMETER_Private_Defines STM32F411E DISCOVERY ACCELEROMETER Private Defines
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F411E_DISCOVERY_ACCELEROMETER_Private_Macros STM32F411E DISCOVERY ACCELEROMETER Private Macros
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F411E_DISCOVERY_ACCELEROMETER_Private_Variables STM32F411E DISCOVERY ACCELEROMETER Private Variables
  * @{
  */
static ACCELERO_DrvTypeDef *AccelerometerDrv;
/**
  * @}
  */

/** @defgroup STM32F411E_DISCOVERY_ACCELEROMETER_Private_FunctionPrototypes STM32F411E DISCOVERY ACCELEROMETER Private FunctionPrototypes
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F411E_DISCOVERY_ACCELEROMETER_Private_Functions STM32F411E DISCOVERY ACCELEROMETER Private Functions
  * @{
  */

/**
  * @brief  Set accelerometer Initialization.
  * @retval ACCELERO_OK if no problem during initialization
  */
uint8_t BSP_ACCELERO_Init(void)
{
  uint8_t ret = ACCELERO_ERROR;
  uint16_t ctrl = 0x0000;
  ACCELERO_InitTypeDef         Accelero_InitStructure;
  ACCELERO_FilterConfigTypeDef Accelero_FilterStructure = {0,0,0,0};

  if(Lsm303dlhcDrv.ReadID() == I_AM_LMS303DLHC)
  {
    /* Initialize the accelerometer driver structure */
    AccelerometerDrv = &Lsm303dlhcDrv;

    /* MEMS configuration ----------------------------------------------------*/
    /* Fill the accelerometer structure */
    Accelero_InitStructure.Power_Mode         = LSM303DLHC_NORMAL_MODE;
    Accelero_InitStructure.AccOutput_DataRate = LSM303DLHC_ODR_50_HZ;
    Accelero_InitStructure.Axes_Enable        = LSM303DLHC_AXES_ENABLE;
    Accelero_InitStructure.AccFull_Scale      = LSM303DLHC_FULLSCALE_2G;
    Accelero_InitStructure.BlockData_Update   = LSM303DLHC_BlockUpdate_Continous;
    Accelero_InitStructure.Endianness         = LSM303DLHC_BLE_LSB;
    Accelero_InitStructure.High_Resolution    = LSM303DLHC_HR_ENABLE;

    /* Configure MEMS: data rate, power mode, full scale and axes */
    ctrl |= (Accelero_InitStructure.Power_Mode | Accelero_InitStructure.AccOutput_DataRate | \
             Accelero_InitStructure.Axes_Enable);

    ctrl |= ((Accelero_InitStructure.BlockData_Update | Accelero_InitStructure.Endianness | \
              Accelero_InitStructure.AccFull_Scale    | Accelero_InitStructure.High_Resolution) << 8);

    /* Configure the accelerometer main parameters */
    AccelerometerDrv->Init(ctrl);

    /* Fill the accelerometer LPF structure */
    Accelero_FilterStructure.HighPassFilter_Mode_Selection   = LSM303DLHC_HPM_NORMAL_MODE;
    Accelero_FilterStructure.HighPassFilter_CutOff_Frequency = LSM303DLHC_HPFCF_16;
    Accelero_FilterStructure.HighPassFilter_AOI1             = LSM303DLHC_HPF_AOI1_DISABLE;
    Accelero_FilterStructure.HighPassFilter_AOI2             = LSM303DLHC_HPF_AOI2_DISABLE;

    /* Configure MEMS: mode, cutoff frquency, Filter status, Click, AOI1 and AOI2 */
    ctrl = (uint8_t) (Accelero_FilterStructure.HighPassFilter_Mode_Selection   |\
                      Accelero_FilterStructure.HighPassFilter_CutOff_Frequency |\
                      Accelero_FilterStructure.HighPassFilter_AOI1             |\
                      Accelero_FilterStructure.HighPassFilter_AOI2);

    /* Configure the accelerometer LPF main parameters */
    AccelerometerDrv->FilterConfig(ctrl);

    ret = ACCELERO_OK;
  }
  else if(Lsm303agrDrv.ReadID() == I_AM_LSM303AGR)
  {
    /* Initialize the accelerometer driver structure */
    AccelerometerDrv = &Lsm303agrDrv;

    /* MEMS configuration ----------------------------------------------------*/
    /* Fill the accelerometer structure */
    Accelero_InitStructure.Power_Mode         = LSM303AGR_NORMAL_MODE;
    Accelero_InitStructure.AccOutput_DataRate = LSM303AGR_ODR_50_HZ;
    Accelero_InitStructure.Axes_Enable        = LSM303AGR_AXES_ENABLE;
    Accelero_InitStructure.AccFull_Scale      = LSM303AGR_FULLSCALE_2G;
    Accelero_InitStructure.BlockData_Update   = LSM303AGR_BlockUpdate_Continous;
    Accelero_InitStructure.Endianness         = LSM303AGR_BLE_LSB;
    Accelero_InitStructure.High_Resolution    = LSM303AGR_HR_ENABLE;

    /* Configure MEMS: data rate, power mode, full scale and axes */
    ctrl |= (Accelero_InitStructure.Power_Mode | Accelero_InitStructure.AccOutput_DataRate | \
             Accelero_InitStructure.Axes_Enable);

    ctrl |= ((Accelero_InitStructure.BlockData_Update | Accelero_InitStructure.Endianness | \
              Accelero_InitStructure.AccFull_Scale    | Accelero_InitStructure.High_Resolution) << 8);

    /* Configure the accelerometer main parameters */
    AccelerometerDrv->Init(ctrl);

    /* Fill the accelerometer LPF structure */
    Accelero_FilterStructure.HighPassFilter_Mode_Selection   = LSM303AGR_HPM_NORMAL_MODE;
    Accelero_FilterStructure.HighPassFilter_CutOff_Frequency = LSM303AGR_HPFCF_16;
    Accelero_FilterStructure.HighPassFilter_AOI1             = LSM303AGR_HPF_AOI1_DISABLE;
    Accelero_FilterStructure.HighPassFilter_AOI2             = LSM303AGR_HPF_AOI2_DISABLE;

    /* Configure MEMS: mode, cutoff frquency, Filter status, Click, AOI1 and AOI2 */
    ctrl = (uint8_t) (Accelero_FilterStructure.HighPassFilter_Mode_Selection   |\
                      Accelero_FilterStructure.HighPassFilter_CutOff_Frequency |\
                      Accelero_FilterStructure.HighPassFilter_AOI1             |\
                      Accelero_FilterStructure.HighPassFilter_AOI2);

    /* Configure the accelerometer LPF main parameters */
    AccelerometerDrv->FilterConfig(ctrl);

    ret = ACCELERO_OK;
  }

  return ret;
}

/**
  * @brief  Reboot memory content of Accelerometer.
  */
void BSP_ACCELERO_Reset(void)
{
  if(AccelerometerDrv->Reset != NULL)
  {
    AccelerometerDrv->Reset();
  }
}

/**
  * @brief  Configure accelerometer click IT.
  */
void BSP_ACCELERO_Click_ITConfig(void)
{
  if(AccelerometerDrv->ConfigIT!= NULL)
  {
    AccelerometerDrv->ConfigIT();
  }
}

/**
  * @brief  Get XYZ axes acceleration.
  * @param  pDataXYZ: Pointer to 3 angular acceleration axes.
  *                   pDataXYZ[0] = X axis, pDataXYZ[1] = Y axis, pDataXYZ[2] = Z axis
  */
void BSP_ACCELERO_GetXYZ(int16_t *pDataXYZ)
{
  int16_t SwitchXY = 0;

  if(AccelerometerDrv->GetXYZ!= NULL)
  {
    AccelerometerDrv->GetXYZ(pDataXYZ);

    /* Switch X and Y Axes in case of LSM303DLHC MEMS */
    if(AccelerometerDrv == &Lsm303dlhcDrv)
    {
      SwitchXY  = pDataXYZ[0];
      pDataXYZ[0] = pDataXYZ[1];

      /* Invert Y Axis to be conpliant with LIS3DSH */
      pDataXYZ[1] = -SwitchXY;
    }
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
