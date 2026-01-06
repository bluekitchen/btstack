/**
  ******************************************************************************
  * @file    i3g4250d.h
  * @author  MCD Application Team
  * @brief   This file contains all the functions prototypes for the i3g4250d.c driver.
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


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __I3G4250D_H
#define __I3G4250D_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "../Common/gyro.h"

/** @addtogroup BSP
  * @{
  */

/** @addtogroup Components
  * @{
  */

/** @addtogroup I3G4250D
  * @{
  */

/** @defgroup I3G4250D_Exported_Constants
  * @{
  */

/******************************************************************************/
/*************************** START REGISTER MAPPING  **************************/
/******************************************************************************/
#define I3G4250D_WHO_AM_I_ADDR          0x0F  /* device identification register */
#define I3G4250D_CTRL_REG1_ADDR         0x20  /* Control register 1 */
#define I3G4250D_CTRL_REG2_ADDR         0x21  /* Control register 2 */
#define I3G4250D_CTRL_REG3_ADDR         0x22  /* Control register 3 */
#define I3G4250D_CTRL_REG4_ADDR         0x23  /* Control register 4 */
#define I3G4250D_CTRL_REG5_ADDR         0x24  /* Control register 5 */
#define I3G4250D_REFERENCE_REG_ADDR     0x25  /* Reference register */
#define I3G4250D_OUT_TEMP_ADDR          0x26  /* Out temp register */
#define I3G4250D_STATUS_REG_ADDR        0x27  /* Status register */
#define I3G4250D_OUT_X_L_ADDR           0x28  /* Output Register X */
#define I3G4250D_OUT_X_H_ADDR           0x29  /* Output Register X */
#define I3G4250D_OUT_Y_L_ADDR           0x2A  /* Output Register Y */
#define I3G4250D_OUT_Y_H_ADDR           0x2B  /* Output Register Y */
#define I3G4250D_OUT_Z_L_ADDR           0x2C  /* Output Register Z */
#define I3G4250D_OUT_Z_H_ADDR           0x2D  /* Output Register Z */
#define I3G4250D_FIFO_CTRL_REG_ADDR     0x2E  /* Fifo control Register */
#define I3G4250D_FIFO_SRC_REG_ADDR      0x2F  /* Fifo src Register */

#define I3G4250D_INT1_CFG_ADDR          0x30  /* Interrupt 1 configuration Register */
#define I3G4250D_INT1_SRC_ADDR          0x31  /* Interrupt 1 source Register */
#define I3G4250D_INT1_TSH_XH_ADDR       0x32  /* Interrupt 1 Threshold X register */
#define I3G4250D_INT1_TSH_XL_ADDR       0x33  /* Interrupt 1 Threshold X register */
#define I3G4250D_INT1_TSH_YH_ADDR       0x34  /* Interrupt 1 Threshold Y register */
#define I3G4250D_INT1_TSH_YL_ADDR       0x35  /* Interrupt 1 Threshold Y register */
#define I3G4250D_INT1_TSH_ZH_ADDR       0x36  /* Interrupt 1 Threshold Z register */
#define I3G4250D_INT1_TSH_ZL_ADDR       0x37  /* Interrupt 1 Threshold Z register */
#define I3G4250D_INT1_DURATION_ADDR     0x38  /* Interrupt 1 DURATION register */

/******************************************************************************/
/**************************** END REGISTER MAPPING  ***************************/
/******************************************************************************/

#define I_AM_I3G4250D                 ((uint8_t)0xD3)

/** @defgroup Power_Mode_selection Power Mode selection
  * @{
  */
#define I3G4250D_MODE_POWERDOWN       ((uint8_t)0x00)
#define I3G4250D_MODE_ACTIVE          ((uint8_t)0x08)
/**
  * @}
  */

/** @defgroup OutPut_DataRate_Selection OutPut DataRate Selection
  * @{
  */
#define I3G4250D_OUTPUT_DATARATE_1    ((uint8_t)0x00)
#define I3G4250D_OUTPUT_DATARATE_2    ((uint8_t)0x40)
#define I3G4250D_OUTPUT_DATARATE_3    ((uint8_t)0x80)
#define I3G4250D_OUTPUT_DATARATE_4    ((uint8_t)0xC0)
/**
  * @}
  */

/** @defgroup Axes_Selection Axes Selection
  * @{
  */
#define I3G4250D_X_ENABLE            ((uint8_t)0x02)
#define I3G4250D_Y_ENABLE            ((uint8_t)0x01)
#define I3G4250D_Z_ENABLE            ((uint8_t)0x04)
#define I3G4250D_AXES_ENABLE         ((uint8_t)0x07)
#define I3G4250D_AXES_DISABLE        ((uint8_t)0x00)
/**
  * @}
  */

/** @defgroup Bandwidth_Selection Bandwidth Selection
  * @{
  */
#define I3G4250D_BANDWIDTH_1         ((uint8_t)0x00)
#define I3G4250D_BANDWIDTH_2         ((uint8_t)0x10)
#define I3G4250D_BANDWIDTH_3         ((uint8_t)0x20)
#define I3G4250D_BANDWIDTH_4         ((uint8_t)0x30)
/**
  * @}
  */

/** @defgroup Full_Scale_Selection Full Scale Selection
  * @{
  */
#define I3G4250D_FULLSCALE_245       ((uint8_t)0x00)
#define I3G4250D_FULLSCALE_500       ((uint8_t)0x10)
#define I3G4250D_FULLSCALE_2000      ((uint8_t)0x20)
#define I3G4250D_FULLSCALE_SELECTION ((uint8_t)0x30)
/**
  * @}
  */

/** @defgroup Full_Scale_Sensitivity Full Scale Sensitivity
  * @{
  */
#define I3G4250D_SENSITIVITY_245DPS  ((float)8.75f)         /*!< gyroscope sensitivity with 250 dps full scale [DPS/LSB]  */
#define I3G4250D_SENSITIVITY_500DPS  ((float)17.50f)        /*!< gyroscope sensitivity with 500 dps full scale [DPS/LSB]  */
#define I3G4250D_SENSITIVITY_2000DPS ((float)70.00f)        /*!< gyroscope sensitivity with 2000 dps full scale [DPS/LSB] */
/**
  * @}
  */


/** @defgroup Block_Data_Update Block Data Update
  * @{
  */
#define I3G4250D_BlockDataUpdate_Continous   ((uint8_t)0x00)
#define I3G4250D_BlockDataUpdate_Single      ((uint8_t)0x80)
/**
  * @}
  */

/** @defgroup Endian_Data_selection Endian Data selection
  * @{
  */
#define I3G4250D_BLE_LSB                     ((uint8_t)0x00)
#define I3G4250D_BLE_MSB                     ((uint8_t)0x40)
/**
  * @}
  */

/** @defgroup High_Pass_Filter_status High Pass Filter status
  * @{
  */
#define I3G4250D_HIGHPASSFILTER_DISABLE      ((uint8_t)0x00)
#define I3G4250D_HIGHPASSFILTER_ENABLE       ((uint8_t)0x10)
/**
  * @}
  */

/** @defgroup INT1_INT2_selection Selection
  * @{
  */
#define I3G4250D_INT1                        ((uint8_t)0x00)
#define I3G4250D_INT2                        ((uint8_t)0x01)
/**
  * @}
  */

/** @defgroup INT1_Interrupt_status Interrupt Status
  * @{
  */
#define I3G4250D_INT1INTERRUPT_DISABLE       ((uint8_t)0x00)
#define I3G4250D_INT1INTERRUPT_ENABLE        ((uint8_t)0x80)
/**
  * @}
  */

/** @defgroup INT2_Interrupt_status Interrupt Status
  * @{
  */
#define I3G4250D_INT2INTERRUPT_DISABLE       ((uint8_t)0x00)
#define I3G4250D_INT2INTERRUPT_ENABLE        ((uint8_t)0x08)
/**
  * @}
  */

/** @defgroup INT1_Interrupt_ActiveEdge Interrupt Active Edge
  * @{
  */
#define I3G4250D_INT1INTERRUPT_LOW_EDGE      ((uint8_t)0x20)
#define I3G4250D_INT1INTERRUPT_HIGH_EDGE     ((uint8_t)0x00)
/**
  * @}
  */

/** @defgroup Boot_Mode_selection Boot Mode Selection
  * @{
  */
#define I3G4250D_BOOT_NORMALMODE             ((uint8_t)0x00)
#define I3G4250D_BOOT_REBOOTMEMORY           ((uint8_t)0x80)
/**
  * @}
  */

/** @defgroup High_Pass_Filter_Mode High Pass Filter Mode
  * @{
  */
#define I3G4250D_HPM_NORMAL_MODE_RES         ((uint8_t)0x00)
#define I3G4250D_HPM_REF_SIGNAL              ((uint8_t)0x10)
#define I3G4250D_HPM_NORMAL_MODE             ((uint8_t)0x20)
#define I3G4250D_HPM_AUTORESET_INT           ((uint8_t)0x30)
/**
  * @}
  */

/** @defgroup High_Pass_CUT OFF_Frequency High Pass CUT OFF Frequency
  * @{
  */
#define I3G4250D_HPFCF_0              0x00
#define I3G4250D_HPFCF_1              0x01
#define I3G4250D_HPFCF_2              0x02
#define I3G4250D_HPFCF_3              0x03
#define I3G4250D_HPFCF_4              0x04
#define I3G4250D_HPFCF_5              0x05
#define I3G4250D_HPFCF_6              0x06
#define I3G4250D_HPFCF_7              0x07
#define I3G4250D_HPFCF_8              0x08
#define I3G4250D_HPFCF_9              0x09
/**
  * @}
  */

/**
  * @}
  */
/** @defgroup I3G4250D_Exported_Functions Exported Functions
  * @{
  */
/* Sensor Configuration Functions */
void    I3G4250D_Init(uint16_t InitStruct);
void    I3G4250D_DeInit(void);
void    I3G4250D_LowPower(uint16_t InitStruct);
uint8_t I3G4250D_ReadID(void);
void    I3G4250D_RebootCmd(void);

/* Interrupt Configuration Functions */
void    I3G4250D_INT1InterruptConfig(uint16_t Int1Config);
void    I3G4250D_EnableIT(uint8_t IntSel);
void    I3G4250D_DisableIT(uint8_t IntSel);

/* High Pass Filter Configuration Functions */
void    I3G4250D_FilterConfig(uint8_t FilterStruct);
void    I3G4250D_FilterCmd(uint8_t HighPassFilterState);
void    I3G4250D_ReadXYZAngRate(float *pfData);
uint8_t I3G4250D_GetDataStatus(void);

/* Gyroscope IO functions */
void    GYRO_IO_Init(void);
void    GYRO_IO_DeInit(void);
void    GYRO_IO_Write(uint8_t *pBuffer, uint8_t WriteAddr, uint16_t NumByteToWrite);
void    GYRO_IO_Read(uint8_t *pBuffer, uint8_t ReadAddr, uint16_t NumByteToRead);

/* Gyroscope driver structure */
extern GYRO_DrvTypeDef I3g4250Drv;

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

#endif /* __I3G4250D_H */
