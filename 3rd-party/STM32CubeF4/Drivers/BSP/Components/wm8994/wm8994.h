/**
  ******************************************************************************
  * @file    wm8994.h
  * @author  MCD Application Team
  * @brief   This file contains all the functions prototypes for the 
  *          wm8994.c driver.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __WM8994_H
#define __WM8994_H

/* Includes ------------------------------------------------------------------*/
#include "../Common/audio.h"

/** @addtogroup BSP
  * @{
  */ 

/** @addtogroup Component
  * @{
  */ 
  
/** @addtogroup WM8994
  * @{
  */

/** @defgroup WM8994_Exported_Types
  * @{
  */

/**
  * @}
  */

/** @defgroup WM8994_Exported_Constants
  * @{
  */ 

/******************************************************************************/
/****************************** REGISTER MAPPING ******************************/
/******************************************************************************/
/* SW Reset */
#define WM8994_SW_RESET               0x0000U

/* Power Management */
#define WM8994_PWR_MANAGEMENT_1       0x0001U
#define WM8994_PWR_MANAGEMENT_2       0x0002U
#define WM8994_PWR_MANAGEMENT_3       0x0003U
#define WM8994_PWR_MANAGEMENT_4       0x0004U
#define WM8994_PWR_MANAGEMENT_5       0x0005U
#define WM8994_PWR_MANAGEMENT_6       0x0006U

/* Input mixer */
#define WM8994_INPUT_MIXER_1          0x0015U
/* Input volume */
#define WM8994_LEFT_LINE_IN12_VOL     0x0018U
#define WM8994_LEFT_LINE_IN34_VOL     0x0019U
#define WM8994_RIGHT_LINE_IN12_VOL    0x001AU
#define WM8994_RIGHT_LINE_IN34_VOL    0x001BU

/* L/R Output volumes */
#define WM8994_LEFT_OUTPUT_VOL        0x001CU
#define WM8994_RIGHT_OUTPUT_VOL       0x001DU
#define WM8994_LINE_OUTPUT_VOL        0x001EU
#define WM8994_OUTPUT2_VOL            0x001FU


/* L/R OPGA volumes */
#define WM8994_LEFT_OPGA_VOL          0x0020U
#define WM8994_RIGHT_OPGA_VOL         0x0021U

/* SPKMIXL/R Attenuation */
#define WM8994_SPKMIXL_ATT            0x0022U
#define WM8994_SPKMIXR_ATT            0x0023U
#define WM8994_OUTPUT_MIXER           0x0024U
#define WM8994_CLASS_D                0x0025U
/* L/R Speakers volumes */
#define WM8994_SPK_LEFT_VOL           0x0026U
#define WM8994_SPK_RIGHT_VOL          0x0027U

/* Input mixer */
#define WM8994_INPUT_MIXER_2          0x0028U
#define WM8994_INPUT_MIXER_3          0x0029U
#define WM8994_INPUT_MIXER_4          0x002AU
#define WM8994_INPUT_MIXER_5          0x002BU
#define WM8994_INPUT_MIXER_6          0x002CU

/* Output mixer */
#define WM8994_OUTPUT_MIXER_1         0x002DU
#define WM8994_OUTPUT_MIXER_2         0x002EU
#define WM8994_OUTPUT_MIXER_3         0x002FU
#define WM8994_OUTPUT_MIXER_4         0x0030U
#define WM8994_OUTPUT_MIXER_5         0x0031U
#define WM8994_OUTPUT_MIXER_6         0x0032U
#define WM8994_OUTPUT2_MIXER          0x0033U
#define WM8994_LINE_MIXER_1           0x0034U
#define WM8994_LINE_MIXER_2           0x0035U
#define WM8994_SPEAKER_MIXER          0x0036U
#define WM8994_ADD_CONTROL            0x0037U
/* Antipop */
#define WM8994_ANTIPOP1               0x0038U
#define WM8994_ANTIPOP2               0x0039U
#define WM8994_MICBIAS                0x003AU
#define WM8994_LDO1                   0x003BU
#define WM8994_LDO2                   0x003CU

/* Charge pump */
#define WM8994_CHARGE_PUMP1           0x004CU
#define WM8994_CHARGE_PUMP2           0x004DU

#define WM8994_CLASS_W                0x0051U

#define WM8994_DC_SERVO1              0x0054U
#define WM8994_DC_SERVO2              0x0055U
#define WM8994_DC_SERVO_READBACK      0x0058U
#define WM8994_DC_SERVO_WRITEVAL      0x0059U

/* Analog HP */
#define WM8994_ANALOG_HP              0x0060U

#define WM8994_CHIP_REVISION          0x0100U
#define WM8994_CONTROL_INTERFACE      0x0101U
#define WM8994_WRITE_SEQ_CTRL1        0x0110U
#define WM8994_WRITE_SEQ_CTRL2        0x0111U

/* WM8994 clocking */
#define WM8994_AIF1_CLOCKING1         0x0200U
#define WM8994_AIF1_CLOCKING2         0x0201U
#define WM8994_AIF2_CLOCKING1         0x0204U
#define WM8994_AIF2_CLOCKING2         0x0205U
#define WM8994_CLOCKING1              0x0208U
#define WM8994_CLOCKING2              0x0209U
#define WM8994_AIF1_RATE              0x0210U
#define WM8994_AIF2_RATE              0x0211U
#define WM8994_RATE_STATUS            0x0212U

/* FLL1 Control */
#define WM8994_FLL1_CONTROL1          0x0220U
#define WM8994_FLL1_CONTROL2          0x0221U
#define WM8994_FLL1_CONTROL3          0x0222U
#define WM8994_FLL1_CONTROL4          0x0223U
#define WM8994_FLL1_CONTROL5          0x0224U

/* FLL2 Control */
#define WM8994_FLL2_CONTROL1          0x0240U
#define WM8994_FLL2_CONTROL2          0x0241U
#define WM8994_FLL2_CONTROL3          0x0242U
#define WM8994_FLL2_CONTROL4          0x0243U
#define WM8994_FLL2_CONTROL5          0x0244U


/* AIF1 control */
#define WM8994_AIF1_CONTROL1          0x0300U
#define WM8994_AIF1_CONTROL2          0x0301U
#define WM8994_AIF1_MASTER_SLAVE      0x0302U
#define WM8994_AIF1_BCLK              0x0303U
#define WM8994_AIF1_ADC_LRCLK         0x0304U
#define WM8994_AIF1_DAC_LRCLK         0x0305U
#define WM8994_AIF1_DAC_DELTA         0x0306U
#define WM8994_AIF1_ADC_DELTA         0x0307U

/* AIF2 control */
#define WM8994_AIF2_CONTROL1          0x0310U
#define WM8994_AIF2_CONTROL2          0x0311U
#define WM8994_AIF2_MASTER_SLAVE      0x0312U
#define WM8994_AIF2_BCLK              0x0313U
#define WM8994_AIF2_ADC_LRCLK         0x0314U
#define WM8994_AIF2_DAC_LRCLK         0x0315U
#define WM8994_AIF2_DAC_DELTA         0x0316U
#define WM8994_AIF2_ADC_DELTA         0x0317U

/* AIF1 ADC/DAC LR volumes */
#define WM8994_AIF1_ADC1_LEFT_VOL     0x0400U
#define WM8994_AIF1_ADC1_RIGHT_VOL    0x0401U
#define WM8994_AIF1_DAC1_LEFT_VOL     0x0402U
#define WM8994_AIF1_DAC1_RIGHT_VOL    0x0403U
#define WM8994_AIF1_ADC2_LEFT_VOL     0x0404U
#define WM8994_AIF1_ADC2_RIGHT_VOL    0x0405U
#define WM8994_AIF1_DAC2_LEFT_VOL     0x0406U
#define WM8994_AIF1_DAC2_RIGHT_VOL    0x0407U

/* AIF1 ADC/DAC filters */
#define WM8994_AIF1_ADC1_FILTERS      0x0410U
#define WM8994_AIF1_ADC2_FILTERS      0x0411U
#define WM8994_AIF1_DAC1_FILTER1      0x0420U
#define WM8994_AIF1_DAC1_FILTER2      0x0421U
#define WM8994_AIF1_DAC2_FILTER1      0x0422U
#define WM8994_AIF1_DAC2_FILTER2      0x0423U

/* AIF1 DRC1 registers */
#define WM8994_AIF1_DRC1              0x0440U
#define WM8994_AIF1_DRC1_1            0x0441U
#define WM8994_AIF1_DRC1_2            0x0442U
#define WM8994_AIF1_DRC1_3            0x0443U
#define WM8994_AIF1_DRC1_4            0x0444U
/* AIF1 DRC2 registers */
#define WM8994_AIF1_DRC2              0x0450U
#define WM8994_AIF1_DRC2_1            0x0451U
#define WM8994_AIF1_DRC2_2            0x0452U
#define WM8994_AIF1_DRC2_3            0x0453U
#define WM8994_AIF1_DRC2_4            0x0454U

/* AIF1 DAC1 EQ Gains Bands */
#define WM8994_AIF1_DAC1_EQG_1        0x0480U
#define WM8994_AIF1_DAC1_EQG_2        0x0481U
#define WM8994_AIF1_DAC1_EQG_1A       0x0482U
#define WM8994_AIF1_DAC1_EQG_1B       0x0483U
#define WM8994_AIF1_DAC1_EQG_1PG      0x0484U
#define WM8994_AIF1_DAC1_EQG_2A       0x0485U
#define WM8994_AIF1_DAC1_EQG_2B       0x0486U
#define WM8994_AIF1_DAC1_EQG_2C       0x0487U
#define WM8994_AIF1_DAC1_EQG_2PG      0x0488U
#define WM8994_AIF1_DAC1_EQG_3A       0x0489U
#define WM8994_AIF1_DAC1_EQG_3B       0x048AU
#define WM8994_AIF1_DAC1_EQG_3C       0x048BU
#define WM8994_AIF1_DAC1_EQG_3PG      0x048CU
#define WM8994_AIF1_DAC1_EQG_4A       0x048DU
#define WM8994_AIF1_DAC1_EQG_4B       0x048EU
#define WM8994_AIF1_DAC1_EQG_4C       0x048FU
#define WM8994_AIF1_DAC1_EQG_4PG      0x0490U
#define WM8994_AIF1_DAC1_EQG_5A       0x0491U
#define WM8994_AIF1_DAC1_EQG_5B       0x0492U
#define WM8994_AIF1_DAC1_EQG_5PG      0x0493U

/* AIF1 DAC2 EQ Gains/bands */
#define WM8994_AIF1_DAC2_EQG_1        0x04A0U
#define WM8994_AIF1_DAC2_EQG_2        0x04A1U
#define WM8994_AIF1_DAC2_EQG_1A       0x04A2U
#define WM8994_AIF1_DAC2_EQG_1B       0x04A3U
#define WM8994_AIF1_DAC2_EQG_1PG      0x04A4U
#define WM8994_AIF1_DAC2_EQG_2A       0x04A5U
#define WM8994_AIF1_DAC2_EQG_2B       0x04A6U
#define WM8994_AIF1_DAC2_EQG_2C       0x04A7U
#define WM8994_AIF1_DAC2_EQG_2PG      0x04A8U
#define WM8994_AIF1_DAC2_EQG_3A       0x04A9U
#define WM8994_AIF1_DAC2_EQG_3B       0x04AAU
#define WM8994_AIF1_DAC2_EQG_3C       0x04ABU
#define WM8994_AIF1_DAC2_EQG_3PG      0x04ACU
#define WM8994_AIF1_DAC2_EQG_4A       0x04ADU
#define WM8994_AIF1_DAC2_EQG_4B       0x04AEU
#define WM8994_AIF1_DAC2_EQG_4C       0x04AFU
#define WM8994_AIF1_DAC2_EQG_4PG      0x04B0U
#define WM8994_AIF1_DAC2_EQG_5A       0x04B1U
#define WM8994_AIF1_DAC2_EQG_5B       0x04B2U
#define WM8994_AIF1_DAC2_EQG_5PG      0x04B3U

/* AIF2 ADC/DAC LR volumes */
#define WM8994_AIF2_ADC_LEFT_VOL      0x0500U
#define WM8994_AIF2_ADC_RIGHT_VOL     0x0501U
#define WM8994_AIF2_DAC_LEFT_VOL      0x0502U
#define WM8994_AIF2_DAC_RIGHT_VOL     0x0503U

/* AIF2 ADC/DAC filters */
#define WM8994_AIF2_ADC_FILTERS       0x0510U
#define WM8994_AIF2_DAC_FILTER_1      0x0520U
#define WM8994_AIF2_DAC_FILTER_2      0x0521U

/* AIF2 DRC registers */
#define WM8994_AIF2_DRC_1             0x0540U
#define WM8994_AIF2_DRC_2             0x0541U
#define WM8994_AIF2_DRC_3             0x0542U
#define WM8994_AIF2_DRC_4             0x0543U
#define WM8994_AIF2_DRC_5             0x0544U

/* AIF2 EQ Gains/bands */
#define WM8994_AIF2_EQG_1             0x0580U
#define WM8994_AIF2_EQG_2             0x0581U
#define WM8994_AIF2_EQG_1A            0x0582U
#define WM8994_AIF2_EQG_1B            0x0583U
#define WM8994_AIF2_EQG_1PG           0x0584U
#define WM8994_AIF2_EQG_2A            0x0585U
#define WM8994_AIF2_EQG_2B            0x0586U
#define WM8994_AIF2_EQG_2C            0x0587U
#define WM8994_AIF2_EQG_2PG           0x0588U
#define WM8994_AIF2_EQG_3A            0x0589U
#define WM8994_AIF2_EQG_3B            0x058AU
#define WM8994_AIF2_EQG_3C            0x058BU
#define WM8994_AIF2_EQG_3PG           0x058CU
#define WM8994_AIF2_EQG_4A            0x058DU
#define WM8994_AIF2_EQG_4B            0x058EU
#define WM8994_AIF2_EQG_4C            0x058FU
#define WM8994_AIF2_EQG_4PG           0x0590U
#define WM8994_AIF2_EQG_5A            0x0591U
#define WM8994_AIF2_EQG_5B            0x0592U
#define WM8994_AIF2_EQG_5PG           0x0593U

/* AIF1 DAC1 Mixer volume */
#define WM8994_DAC1_MIXER_VOL         0x0600U
/* AIF1 DAC1 Left Mixer Routing */
#define WM8994_AIF1_DAC1_LMR          0x0601U
/* AIF1 DAC1 Righ Mixer Routing */
#define WM8994_AIF1_DAC1_RMR          0x0602U
/* AIF1 DAC2 Mixer volume */
#define WM8994_DAC2_MIXER_VOL         0x0603U
/* AIF1 DAC2 Left Mixer Routing */
#define WM8994_AIF1_DAC2_LMR          0x0604U
/* AIF1 DAC2 Righ Mixer Routing */
#define WM8994_AIF1_DAC2_RMR          0x0605U
/* AIF1 ADC1 Left Mixer Routing */
#define WM8994_AIF1_ADC1_LMR          0x0606U
/* AIF1 ADC1 Righ Mixer Routing */
#define WM8994_AIF1_ADC1_RMR          0x0607U
/* AIF1 ADC2 Left Mixer Routing */
#define WM8994_AIF1_ADC2_LMR          0x0608U
/* AIF1 ADC2 Righ Mixer Routing */
#define WM8994_AIF1_ADC2_RMR          0x0609U

/* Volume control */
#define WM8994_DAC1_LEFT_VOL          0x0610U
#define WM8994_DAC1_RIGHT_VOL         0x0611U
#define WM8994_DAC2_LEFT_VOL          0x0612U
#define WM8994_DAC2_RIGHT_VOL         0x0613U
#define WM8994_DAC_SOFTMUTE           0x0614U

#define WM8994_OVERSAMPLING           0x0620U
#define WM8994_SIDETONE               0x0621U

/* GPIO */
#define WM8994_GPIO1                  0x0700U
#define WM8994_GPIO2                  0x0701U
#define WM8994_GPIO3                  0x0702U
#define WM8994_GPIO4                  0x0703U
#define WM8994_GPIO5                  0x0704U
#define WM8994_GPIO6                  0x0705U
#define WM8994_GPIO7                  0x0706U
#define WM8994_GPIO8                  0x0707U
#define WM8994_GPIO9                  0x0708U
#define WM8994_GPIO10                 0x0709U
#define WM8994_GPIO11                 0x070AU
/* Pull Contol */
#define WM8994_PULL_CONTROL_1         0x0720U
#define WM8994_PULL_CONTROL_2         0x0721U
/* WM8994 Inturrupts */
#define WM8994_INT_STATUS_1           0x0730U
#define WM8994_INT_STATUS_2           0x0731U
#define WM8994_INT_RAW_STATUS_2       0x0732U
#define WM8994_INT_STATUS1_MASK       0x0738U
#define WM8994_INT_STATUS2_MASK       0x0739U
#define WM8994_INT_CONTROL            0x0740U
#define WM8994_IRQ_DEBOUNCE           0x0748U

/* Write Sequencer registers from 0 to 511 */
#define WM8994_WRITE_SEQUENCER0       0x3000U
#define WM8994_WRITE_SEQUENCER1       0x3001U
#define WM8994_WRITE_SEQUENCER2       0x3002U
#define WM8994_WRITE_SEQUENCER3       0x3003U

#define WM8994_WRITE_SEQUENCER4       0x3508U
#define WM8994_WRITE_SEQUENCER5       0x3509U
#define WM8994_WRITE_SEQUENCER6       0x3510U
#define WM8994_WRITE_SEQUENCER7       0x3511U

/******************************************************************************/
/***************************  Codec User defines ******************************/
/******************************************************************************/
/* Codec output DEVICE */
#define OUTPUT_DEVICE_SPEAKER                 0x0001U
#define OUTPUT_DEVICE_HEADPHONE               0x0002U
#define OUTPUT_DEVICE_BOTH                    0x0003U
#define OUTPUT_DEVICE_AUTO                    0x0004U
#define INPUT_DEVICE_DIGITAL_MICROPHONE_1     0x0100U
#define INPUT_DEVICE_DIGITAL_MICROPHONE_2     0x0200U
#define INPUT_DEVICE_INPUT_LINE_1             0x0300U
#define INPUT_DEVICE_INPUT_LINE_2             0x0400U
#define INPUT_DEVICE_DIGITAL_MIC1_MIC2        0x0800U

/* Volume Levels values */
#define DEFAULT_VOLMIN                0x00U
#define DEFAULT_VOLMAX                0xFFU
#define DEFAULT_VOLSTEP               0x04U

#define AUDIO_PAUSE                   0U
#define AUDIO_RESUME                  1U

/* Codec POWER DOWN modes */
#define CODEC_PDWN_HW                 1U
#define CODEC_PDWN_SW                 2U

/* MUTE commands */
#define AUDIO_MUTE_ON                 1U
#define AUDIO_MUTE_OFF                0U

/* AUDIO FREQUENCY */
#define AUDIO_FREQUENCY_192K          192000U
#define AUDIO_FREQUENCY_96K           96000U
#define AUDIO_FREQUENCY_48K           48000U
#define AUDIO_FREQUENCY_44K           44100U
#define AUDIO_FREQUENCY_32K           32000U
#define AUDIO_FREQUENCY_22K           22050U
#define AUDIO_FREQUENCY_16K           16000U
#define AUDIO_FREQUENCY_11K           11025U
#define AUDIO_FREQUENCY_8K            8000U

#define VOLUME_CONVERT(Volume)        (((Volume) > 100)? 100:((uint8_t)(((Volume) * 63) / 100)))
#define VOLUME_IN_CONVERT(Volume)     (((Volume) >= 100)? 239:((uint8_t)(((Volume) * 240) / 100)))

/******************************************************************************/
/****************************** REGISTER MAPPING ******************************/
/******************************************************************************/
/** 
  * @brief  WM8994 ID  
  */  
#define  WM8994_ID    0x8994U

/**
  * @brief Device ID Register: Reading from this register will indicate device 
  *                            family ID 8994h
  */
#define WM8994_CHIPID_ADDR                  0x00U

/**
  * @}
  */ 

/** @defgroup WM8994_Exported_Macros
  * @{
  */ 
/**
  * @}
  */ 

/** @defgroup WM8994_Exported_Functions
  * @{
  */
    
/*------------------------------------------------------------------------------
                           Audio Codec functions 
------------------------------------------------------------------------------*/
/* High Layer codec functions */
uint32_t wm8994_Init(uint16_t DeviceAddr, uint16_t OutputInputDevice, uint8_t Volume, uint32_t AudioFreq);
void     wm8994_DeInit(void);
uint32_t wm8994_ReadID(uint16_t DeviceAddr);
uint32_t wm8994_Play(uint16_t DeviceAddr, uint16_t* pBuffer, uint16_t Size);
uint32_t wm8994_Pause(uint16_t DeviceAddr);
uint32_t wm8994_Resume(uint16_t DeviceAddr);
uint32_t wm8994_Stop(uint16_t DeviceAddr, uint32_t Cmd);
uint32_t wm8994_SetVolume(uint16_t DeviceAddr, uint8_t Volume);
uint32_t wm8994_SetMute(uint16_t DeviceAddr, uint32_t Cmd);
uint32_t wm8994_SetOutputMode(uint16_t DeviceAddr, uint8_t Output);
uint32_t wm8994_SetFrequency(uint16_t DeviceAddr, uint32_t AudioFreq);
uint32_t wm8994_Reset(uint16_t DeviceAddr);

/* AUDIO IO functions */
void    AUDIO_IO_Init(void);
void    AUDIO_IO_DeInit(void);
void    AUDIO_IO_Write(uint8_t Addr, uint16_t Reg, uint16_t Value);
uint8_t AUDIO_IO_Read(uint8_t Addr, uint16_t Reg);
void    AUDIO_IO_Delay(uint32_t Delay);

/* Audio driver structure */
extern AUDIO_DrvTypeDef   wm8994_drv;

#endif /* __WM8994_H */

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
