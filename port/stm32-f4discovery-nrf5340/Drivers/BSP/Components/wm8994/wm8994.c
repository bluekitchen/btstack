/**
  ******************************************************************************
  * @file    wm8994.c
  * @author  MCD Application Team
  * @brief   This file provides the WM8994 Audio Codec driver.
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

/* Includes ------------------------------------------------------------------*/
#include "wm8994.h"

/** @addtogroup BSP
  * @{
  */
  
/** @addtogroup Components
  * @{
  */ 

/** @addtogroup wm8994
  * @brief     This file provides a set of functions needed to drive the 
  *            WM8994 audio codec.
  * @{
  */

/** @defgroup WM8994_Private_Types
  * @{
  */

/**
  * @}
  */ 
  
/** @defgroup WM8994_Private_Defines
  * @{
  */
/* Uncomment this line to enable verifying data sent to codec after each write 
   operation (for debug purpose) */
#if !defined (VERIFY_WRITTENDATA)  
/*#define VERIFY_WRITTENDATA*/
#endif /* VERIFY_WRITTENDATA */
/**
  * @}
  */ 

/** @defgroup WM8994_Private_Macros
  * @{
  */

/**
  * @}
  */ 
  
/** @defgroup WM8994_Private_Variables
  * @{
  */

/* Audio codec driver structure initialization */  
AUDIO_DrvTypeDef wm8994_drv = 
{
  wm8994_Init,
  wm8994_DeInit,
  wm8994_ReadID,

  wm8994_Play,
  wm8994_Pause,
  wm8994_Resume,
  wm8994_Stop,  

  wm8994_SetFrequency,
  wm8994_SetVolume,
  wm8994_SetMute,  
  wm8994_SetOutputMode,

  wm8994_Reset
};

static uint32_t outputEnabled = 0;
static uint32_t inputEnabled = 0;
static uint8_t ColdStartup = 1;

/**
  * @}
  */ 

/** @defgroup WM8994_Function_Prototypes
  * @{
  */
static uint8_t CODEC_IO_Write(uint8_t Addr, uint16_t Reg, uint16_t Value);
/**
  * @}
  */ 


/** @defgroup WM8994_Private_Functions
  * @{
  */ 

/**
  * @brief Initializes the audio codec and the control interface.
  * @param DeviceAddr: Device address on communication Bus.   
  * @param OutputInputDevice: can be OUTPUT_DEVICE_SPEAKER, OUTPUT_DEVICE_HEADPHONE,
  *  OUTPUT_DEVICE_BOTH, OUTPUT_DEVICE_AUTO, INPUT_DEVICE_DIGITAL_MICROPHONE_1,
  *  INPUT_DEVICE_DIGITAL_MICROPHONE_2, INPUT_DEVICE_DIGITAL_MIC1_MIC2, 
  *  INPUT_DEVICE_INPUT_LINE_1 or INPUT_DEVICE_INPUT_LINE_2.
  * @param Volume: Initial volume level (from 0 (Mute) to 100 (Max))
  * @param AudioFreq: Audio Frequency 
  * @retval 0 if correct communication, else wrong communication
  */
uint32_t wm8994_Init(uint16_t DeviceAddr, uint16_t OutputInputDevice, uint8_t Volume, uint32_t AudioFreq)
{
  uint32_t counter = 0;
  uint16_t output_device = OutputInputDevice & 0xFF;
  uint16_t input_device = OutputInputDevice & 0xFF00;
  uint16_t power_mgnt_reg_1 = 0;
  uint16_t tmp;
  
  /* Initialize the Control interface of the Audio Codec */
  AUDIO_IO_Init();
  /* wm8994 Errata Work-Arounds */
  tmp = 0x0003;
  counter += CODEC_IO_Write(DeviceAddr, 0x102, tmp);
  tmp = 0x0000;
  counter += CODEC_IO_Write(DeviceAddr, 0x817, tmp);
  counter += CODEC_IO_Write(DeviceAddr, 0x102, tmp);

  /* Enable VMID soft start (fast), Start-up Bias Current Enabled */
  tmp = 0x006C;
  counter += CODEC_IO_Write(DeviceAddr, WM8994_ANTIPOP2, tmp);

    /* Enable bias generator, Enable VMID */
  if (input_device > 0)
  {
    tmp = 0x0013;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_1, tmp);
  }
  else
  {
    tmp = 0x0003;
	counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_1, tmp);
  }

  /* Add Delay */
  AUDIO_IO_Delay(50);

  /* Path Configurations for output */
  if (output_device > 0)
  {
    outputEnabled = 1;

    switch (output_device)
    {
    case OUTPUT_DEVICE_SPEAKER:
      /* Enable DAC1 (Left), Enable DAC1 (Right),
      Disable DAC2 (Left), Disable DAC2 (Right)*/
      tmp = 0x0C0C;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_5, tmp);

      /* Enable the AIF1 Timeslot 0 (Left) to DAC 1 (Left) mixer path */
      tmp = 0x0000;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_LMR, tmp);

      /* Enable the AIF1 Timeslot 0 (Right) to DAC 1 (Right) mixer path */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_RMR, tmp);

      /* Disable the AIF1 Timeslot 1 (Left) to DAC 2 (Left) mixer path */
      tmp = 0x0002;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC2_LMR, tmp);

      /* Disable the AIF1 Timeslot 1 (Right) to DAC 2 (Right) mixer path */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC2_RMR, tmp);
      break;

    case OUTPUT_DEVICE_HEADPHONE:
      /* Disable DAC1 (Left), Disable DAC1 (Right),
      Enable DAC2 (Left), Enable DAC2 (Right)*/
      tmp = 0x0303;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_5, tmp);

      /* Enable the AIF1 Timeslot 0 (Left) to DAC 1 (Left) mixer path */
      tmp = 0x0001;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_LMR, tmp);

      /* Enable the AIF1 Timeslot 0 (Right) to DAC 1 (Right) mixer path */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_RMR, tmp);

      /* Disable the AIF1 Timeslot 1 (Left) to DAC 2 (Left) mixer path */
      tmp = 0x0000;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC2_LMR, tmp);

      /* Disable the AIF1 Timeslot 1 (Right) to DAC 2 (Right) mixer path */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC2_RMR, tmp);
      break;

    case OUTPUT_DEVICE_BOTH:
      if (input_device == INPUT_DEVICE_DIGITAL_MIC1_MIC2)
      {
        /* Enable DAC1 (Left), Enable DAC1 (Right),
        also Enable DAC2 (Left), Enable DAC2 (Right)*/
    	tmp = 0x0303 | 0x0C0C;
        counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_5, tmp);
        
        /* Enable the AIF1 Timeslot 0 (Left) to DAC 1 (Left) mixer path
        Enable the AIF1 Timeslot 1 (Left) to DAC 1 (Left) mixer path */
        tmp = 0x0003;
        counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_LMR, tmp);
        
        /* Enable the AIF1 Timeslot 0 (Right) to DAC 1 (Right) mixer path
        Enable the AIF1 Timeslot 1 (Right) to DAC 1 (Right) mixer path */
        counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_RMR, tmp);
        
        /* Enable the AIF1 Timeslot 0 (Left) to DAC 2 (Left) mixer path
        Enable the AIF1 Timeslot 1 (Left) to DAC 2 (Left) mixer path  */
        counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC2_LMR, tmp);
        
        /* Enable the AIF1 Timeslot 0 (Right) to DAC 2 (Right) mixer path
        Enable the AIF1 Timeslot 1 (Right) to DAC 2 (Right) mixer path */
        counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC2_RMR, tmp);
      }
      else
      {
        /* Enable DAC1 (Left), Enable DAC1 (Right),
        also Enable DAC2 (Left), Enable DAC2 (Right)*/
        tmp =  0x0303 | 0x0C0C;
        counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_5, tmp);
        
        /* Enable the AIF1 Timeslot 0 (Left) to DAC 1 (Left) mixer path */
        tmp = 0x0001;
        counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_LMR, tmp);
        
        /* Enable the AIF1 Timeslot 0 (Right) to DAC 1 (Right) mixer path */
        counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_RMR, tmp);
        
        /* Enable the AIF1 Timeslot 1 (Left) to DAC 2 (Left) mixer path */
        tmp = 0x0002;
        counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC2_LMR, tmp);
        
        /* Enable the AIF1 Timeslot 1 (Right) to DAC 2 (Right) mixer path */
        counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC2_RMR, tmp);      
      }
      break;

    case OUTPUT_DEVICE_AUTO :
    default:
      /* Disable DAC1 (Left), Disable DAC1 (Right),
      Enable DAC2 (Left), Enable DAC2 (Right)*/
      tmp = 0x0303;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_5, tmp);

      /* Enable the AIF1 Timeslot 0 (Left) to DAC 1 (Left) mixer path */
      tmp = 0x0001;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_LMR, tmp);

      /* Enable the AIF1 Timeslot 0 (Right) to DAC 1 (Right) mixer path */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_RMR, tmp);

      /* Disable the AIF1 Timeslot 1 (Left) to DAC 2 (Left) mixer path */
      tmp = 0x0000;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC2_LMR, tmp);

      /* Disable the AIF1 Timeslot 1 (Right) to DAC 2 (Right) mixer path */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC2_RMR, tmp);
      break;
    }
  }
  else
  {
    outputEnabled = 0;
  }

  /* Path Configurations for input */
  if (input_device > 0)
  {
    inputEnabled = 1;
    switch (input_device)
    {
    case INPUT_DEVICE_DIGITAL_MICROPHONE_2 :
      /* Enable AIF1ADC2 (Left), Enable AIF1ADC2 (Right)
       * Enable DMICDAT2 (Left), Enable DMICDAT2 (Right)
       * Enable Left ADC, Enable Right ADC */
      tmp = 0x0C30;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_4, tmp);

      /* Enable AIF1 DRC2 Signal Detect & DRC in AIF1ADC2 Left/Right Timeslot 1 */
      tmp = 0x00DB;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DRC2, tmp);

      /* Disable IN1L, IN1R, IN2L, IN2R, Enable Thermal sensor & shutdown */
      tmp = 0x6000;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_2, tmp);

      /* Enable the DMIC2(Left) to AIF1 Timeslot 1 (Left) mixer path */
      tmp = 0x0002;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_ADC2_LMR, tmp);

      /* Enable the DMIC2(Right) to AIF1 Timeslot 1 (Right) mixer path */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_ADC2_RMR, tmp);

      /* GPIO1 pin configuration GP1_DIR = output, GP1_FN = AIF1 DRC2 signal detect */
      tmp = 0x000E;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_GPIO1, tmp);
      break;

    case INPUT_DEVICE_INPUT_LINE_1 :
      /* IN1LN_TO_IN1L, IN1LP_TO_VMID, IN1RN_TO_IN1R, IN1RP_TO_VMID */
      tmp = 0x0011;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_INPUT_MIXER_2, tmp);

      /* Disable mute on IN1L_TO_MIXINL and +30dB on IN1L PGA output */
      tmp = 0x0035;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_INPUT_MIXER_3, tmp);

      /* Disable mute on IN1R_TO_MIXINL, Gain = +30dB */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_INPUT_MIXER_4, tmp);

      /* Enable AIF1ADC1 (Left), Enable AIF1ADC1 (Right)
       * Enable Left ADC, Enable Right ADC */
      tmp = 0x0303;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_4, tmp);

      /* Enable AIF1 DRC1 Signal Detect & DRC in AIF1ADC1 Left/Right Timeslot 0 */
      tmp = 0x00DB;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DRC1, tmp);

      /* Enable IN1L and IN1R, Disable IN2L and IN2R, Enable Thermal sensor & shutdown */
      tmp = 0x6350;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_2, tmp);

      /* Enable the ADCL(Left) to AIF1 Timeslot 0 (Left) mixer path */
      tmp = 0x0002;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_ADC1_LMR, tmp);

      /* Enable the ADCR(Right) to AIF1 Timeslot 0 (Right) mixer path */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_ADC1_RMR, tmp);

      /* GPIO1 pin configuration GP1_DIR = output, GP1_FN = AIF1 DRC1 signal detect */
      tmp = 0x000D;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_GPIO1, tmp);
      break;

    case INPUT_DEVICE_DIGITAL_MICROPHONE_1 :
      /* Enable AIF1ADC1 (Left), Enable AIF1ADC1 (Right)
       * Enable DMICDAT1 (Left), Enable DMICDAT1 (Right)
       * Enable Left ADC, Enable Right ADC */
      tmp = 0x030C;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_4, tmp);

      /* Enable AIF1 DRC2 Signal Detect & DRC in AIF1ADC1 Left/Right Timeslot 0 */
      tmp = 0x00DB;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DRC1, tmp);

      /* Disable IN1L, IN1R, IN2L, IN2R, Enable Thermal sensor & shutdown */
      tmp = 0x6350;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_2, tmp);

      /* Enable the DMIC2(Left) to AIF1 Timeslot 0 (Left) mixer path */
      tmp = 0x0002;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_ADC1_LMR, tmp);

      /* Enable the DMIC2(Right) to AIF1 Timeslot 0 (Right) mixer path */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_ADC1_RMR, tmp);

      /* GPIO1 pin configuration GP1_DIR = output, GP1_FN = AIF1 DRC1 signal detect */
      tmp = 0x000D;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_GPIO1, tmp);
      break; 
    case INPUT_DEVICE_DIGITAL_MIC1_MIC2 :
      /* Enable AIF1ADC1 (Left), Enable AIF1ADC1 (Right)
       * Enable DMICDAT1 (Left), Enable DMICDAT1 (Right)
       * Enable Left ADC, Enable Right ADC */
      tmp = 0x0F3C;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_4, tmp);

      /* Enable AIF1 DRC2 Signal Detect & DRC in AIF1ADC2 Left/Right Timeslot 1 */
      tmp = 0x00DB;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DRC2, tmp);
      
      /* Enable AIF1 DRC2 Signal Detect & DRC in AIF1ADC1 Left/Right Timeslot 0 */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DRC1, tmp);

      /* Disable IN1L, IN1R, Enable IN2L, IN2R, Thermal sensor & shutdown */
      tmp = 0x63A0;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_2, tmp);

      /* Enable the DMIC2(Left) to AIF1 Timeslot 0 (Left) mixer path */
      tmp = 0x0002;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_ADC1_LMR, tmp);

      /* Enable the DMIC2(Right) to AIF1 Timeslot 0 (Right) mixer path */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_ADC1_RMR, tmp);

      /* Enable the DMIC2(Left) to AIF1 Timeslot 1 (Left) mixer path */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_ADC2_LMR, tmp);

      /* Enable the DMIC2(Right) to AIF1 Timeslot 1 (Right) mixer path */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_ADC2_RMR, tmp);
      
      /* GPIO1 pin configuration GP1_DIR = output, GP1_FN = AIF1 DRC1 signal detect */
      tmp = 0x000D;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_GPIO1, tmp);
      break;    
    case INPUT_DEVICE_INPUT_LINE_2 :
    default:
      /* Actually, no other input devices supported */
      counter++;
      break;
    }
  }
  else
  {
    inputEnabled = 0;
  }
  
  /*  Clock Configurations */
  switch (AudioFreq)
  {
  case  AUDIO_FREQUENCY_8K:
    /* AIF1 Sample Rate = 8 (KHz), ratio=256 */ 
    tmp = 0x0003;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_RATE, tmp);
    break;
    
  case  AUDIO_FREQUENCY_16K:
    /* AIF1 Sample Rate = 16 (KHz), ratio=256 */ 
    tmp = 0x0033;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_RATE, tmp);
    break;

  case  AUDIO_FREQUENCY_32K:
    /* AIF1 Sample Rate = 32 (KHz), ratio=256 */ 
    tmp = 0x0063;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_RATE, tmp);
    break;
    
  case  AUDIO_FREQUENCY_48K:
    /* AIF1 Sample Rate = 48 (KHz), ratio=256 */ 
    tmp = 0x0083;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_RATE, tmp);
    break;
    
  case  AUDIO_FREQUENCY_96K:
    /* AIF1 Sample Rate = 96 (KHz), ratio=256 */ 
    tmp = 0x00A3;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_RATE, tmp);
    break;
    
  case  AUDIO_FREQUENCY_11K:
    /* AIF1 Sample Rate = 11.025 (KHz), ratio=256 */ 
    tmp = 0x0013;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_RATE, tmp);
    break;
    
  case  AUDIO_FREQUENCY_22K:
    /* AIF1 Sample Rate = 22.050 (KHz), ratio=256 */ 
    tmp = 0x0043;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_RATE, tmp);
    break;
    
  case  AUDIO_FREQUENCY_44K:
    /* AIF1 Sample Rate = 44.1 (KHz), ratio=256 */ 
    tmp = 0x0073;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_RATE, tmp);
    break; 
    
  default:
    /* AIF1 Sample Rate = 48 (KHz), ratio=256 */ 
    tmp = 0x0083;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_RATE, tmp);
    break; 
  }

  if(input_device == INPUT_DEVICE_DIGITAL_MIC1_MIC2)
  {
  /* AIF1 Word Length = 16-bits, AIF1 Format = DSP mode */
  tmp = 0x4018;
  counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_CONTROL1, tmp);    
  }
  else
  {
  /* AIF1 Word Length = 16-bits, AIF1 Format = I2S (Default Register Value) */
  tmp = 0x4010;
  counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_CONTROL1, tmp);
  }
  
  /* slave mode */
  tmp = 0x0000;
  counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_MASTER_SLAVE, tmp);
  
  /* Enable the DSP processing clock for AIF1, Enable the core clock */
  tmp = 0x000A;
  counter += CODEC_IO_Write(DeviceAddr, WM8994_CLOCKING1, tmp);
  
  /* Enable AIF1 Clock, AIF1 Clock Source = MCLK1 pin */
  tmp = 0x0001;
  counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_CLOCKING1, tmp);

  if (output_device > 0)  /* Audio output selected */
  {
    if (output_device == OUTPUT_DEVICE_HEADPHONE)
    {      
      /* Select DAC1 (Left) to Left Headphone Output PGA (HPOUT1LVOL) path */
      tmp = 0x0100;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_OUTPUT_MIXER_1, tmp);
      
      /* Select DAC1 (Right) to Right Headphone Output PGA (HPOUT1RVOL) path */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_OUTPUT_MIXER_2, tmp);    
            
      /* Startup sequence for Headphone */
      if(ColdStartup)
      {
        tmp = 0x8100;
        counter += CODEC_IO_Write(DeviceAddr,WM8994_WRITE_SEQ_CTRL1, tmp);
        
        ColdStartup=0;
        /* Add Delay */
        AUDIO_IO_Delay(300);
      }
      else /* Headphone Warm Start-Up */
      { 
        tmp = 0x8108;
        counter += CODEC_IO_Write(DeviceAddr,WM8994_WRITE_SEQ_CTRL1, tmp);
        /* Add Delay */
        AUDIO_IO_Delay(50);
      }

      /* Soft un-Mute the AIF1 Timeslot 0 DAC1 path L&R */
      tmp = 0x0000;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_FILTER1, tmp);
    }
    /* Analog Output Configuration */

    /* Enable SPKRVOL PGA, Enable SPKMIXR, Enable SPKLVOL PGA, Enable SPKMIXL */
    tmp = 0x0300;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_3, tmp);

    /* Left Speaker Mixer Volume = 0dB */
    tmp = 0x0000;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_SPKMIXL_ATT, tmp);

    /* Speaker output mode = Class D, Right Speaker Mixer Volume = 0dB ((0x23, 0x0100) = class AB)*/
    counter += CODEC_IO_Write(DeviceAddr, WM8994_SPKMIXR_ATT, tmp);

    /* Unmute DAC2 (Left) to Left Speaker Mixer (SPKMIXL) path,
    Unmute DAC2 (Right) to Right Speaker Mixer (SPKMIXR) path */
    tmp = 0x0300;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_SPEAKER_MIXER, tmp);

    /* Enable bias generator, Enable VMID, Enable SPKOUTL, Enable SPKOUTR */
    tmp = 0x3003;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_1, tmp);

    /* Headphone/Speaker Enable */

    if (input_device == INPUT_DEVICE_DIGITAL_MIC1_MIC2)
    {
    /* Enable Class W, Class W Envelope Tracking = AIF1 Timeslots 0 and 1 */
    tmp = 0x0205;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_CLASS_W, tmp);
    }
    else
    {
    /* Enable Class W, Class W Envelope Tracking = AIF1 Timeslot 0 */
    tmp = 0x0005;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_CLASS_W, tmp);      
    }

    /* Enable bias generator, Enable VMID, Enable HPOUT1 (Left) and Enable HPOUT1 (Right) input stages */
    /* idem for Speaker */
    power_mgnt_reg_1 |= 0x0303 | 0x3003;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_1, power_mgnt_reg_1);

    /* Enable HPOUT1 (Left) and HPOUT1 (Right) intermediate stages */
    tmp = 0x0022;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_ANALOG_HP, tmp);

    /* Enable Charge Pump */
    tmp = 0x9F25;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_CHARGE_PUMP1, tmp);

    /* Add Delay */
    AUDIO_IO_Delay(15);

    /* Select DAC1 (Left) to Left Headphone Output PGA (HPOUT1LVOL) path */
    tmp = 0x0001;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_OUTPUT_MIXER_1, tmp);

    /* Select DAC1 (Right) to Right Headphone Output PGA (HPOUT1RVOL) path */
    counter += CODEC_IO_Write(DeviceAddr, WM8994_OUTPUT_MIXER_2, tmp);

    /* Enable Left Output Mixer (MIXOUTL), Enable Right Output Mixer (MIXOUTR) */
    /* idem for SPKOUTL and SPKOUTR */
    tmp = 0x0030 | 0x0300;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_3, tmp);

    /* Enable DC Servo and trigger start-up mode on left and right channels */
    tmp = 0x0033;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_DC_SERVO1, tmp);

    /* Add Delay */
    AUDIO_IO_Delay(257);

    /* Enable HPOUT1 (Left) and HPOUT1 (Right) intermediate and output stages. Remove clamps */
    tmp = 0x00EE;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_ANALOG_HP, tmp);

    /* Unmutes */

    /* Unmute DAC 1 (Left) */
    tmp = 0x00C0;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_DAC1_LEFT_VOL, tmp);

    /* Unmute DAC 1 (Right) */
    counter += CODEC_IO_Write(DeviceAddr, WM8994_DAC1_RIGHT_VOL, tmp);

    /* Unmute the AIF1 Timeslot 0 DAC path */
    tmp = 0x0010;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_FILTER1, tmp);

    /* Unmute DAC 2 (Left) */
    tmp = 0x00C0;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_DAC2_LEFT_VOL, tmp);

    /* Unmute DAC 2 (Right) */
    counter += CODEC_IO_Write(DeviceAddr, WM8994_DAC2_RIGHT_VOL, tmp);

    /* Unmute the AIF1 Timeslot 1 DAC2 path */
    tmp = 0x0010;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC2_FILTER1, tmp);
    
    /* Volume Control */
    wm8994_SetVolume(DeviceAddr, Volume);
  }

  if (input_device > 0) /* Audio input selected */
  {
    if ((input_device == INPUT_DEVICE_DIGITAL_MICROPHONE_1) || (input_device == INPUT_DEVICE_DIGITAL_MICROPHONE_2))
    {
      /* Enable Microphone bias 1 generator, Enable VMID */
      power_mgnt_reg_1 |= 0x0013;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_1, power_mgnt_reg_1);

      /* ADC oversample enable */
      tmp = 0x0002;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_OVERSAMPLING, tmp);

      /* AIF ADC2 HPF enable, HPF cut = voice mode 1 fc=127Hz at fs=8kHz */
      tmp = 0x3800;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_ADC2_FILTERS, tmp);
    }
    else if(input_device == INPUT_DEVICE_DIGITAL_MIC1_MIC2)
    {
      /* Enable Microphone bias 1 generator, Enable VMID */
      power_mgnt_reg_1 |= 0x0013;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_1, power_mgnt_reg_1);

      /* ADC oversample enable */
      tmp = 0x0002;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_OVERSAMPLING, tmp);
    
      /* AIF ADC1 HPF enable, HPF cut = voice mode 1 fc=127Hz at fs=8kHz */
      tmp = 0x1800;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_ADC1_FILTERS, tmp);
      
      /* AIF ADC2 HPF enable, HPF cut = voice mode 1 fc=127Hz at fs=8kHz */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_ADC2_FILTERS, tmp);      
    }    
    else if ((input_device == INPUT_DEVICE_INPUT_LINE_1) || (input_device == INPUT_DEVICE_INPUT_LINE_2))
    {

      /* Disable mute on IN1L, IN1L Volume = +0dB */
      tmp = 0x000B;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_LEFT_LINE_IN12_VOL, tmp);

      /* Disable mute on IN1R, IN1R Volume = +0dB */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_RIGHT_LINE_IN12_VOL, tmp);

      /* AIF ADC1 HPF enable, HPF cut = hifi mode fc=4Hz at fs=48kHz */
      tmp = 0x1800;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_ADC1_FILTERS, tmp);
    }
    /* Volume Control */
    wm8994_SetVolume(DeviceAddr, Volume);
  }
  /* Return communication control value */
  return counter;  
}

/**
  * @brief  Deinitializes the audio codec.
  * @param  None
  * @retval  None
  */
void wm8994_DeInit(void)
{
  /* Deinitialize Audio Codec interface */
  AUDIO_IO_DeInit();
}

/**
  * @brief  Get the WM8994 ID.
  * @param DeviceAddr: Device address on communication Bus.
  * @retval The WM8994 ID 
  */
uint32_t wm8994_ReadID(uint16_t DeviceAddr)
{
  /* Initialize the Control interface of the Audio Codec */
  AUDIO_IO_Init();

  return ((uint32_t)AUDIO_IO_Read(DeviceAddr, WM8994_CHIPID_ADDR));
}

/**
  * @brief Start the audio Codec play feature.
  * @note For this codec no Play options are required.
  * @param DeviceAddr: Device address on communication Bus.   
  * @retval 0 if correct communication, else wrong communication
  */
uint32_t wm8994_Play(uint16_t DeviceAddr, uint16_t* pBuffer, uint16_t Size)
{
  uint32_t counter = 0;
 
  /* Resumes the audio file playing */  
  /* Unmute the output first */
  counter += wm8994_SetMute(DeviceAddr, AUDIO_MUTE_OFF);
  
  return counter;
}

/**
  * @brief Pauses playing on the audio codec.
  * @param DeviceAddr: Device address on communication Bus. 
  * @retval 0 if correct communication, else wrong communication
  */
uint32_t wm8994_Pause(uint16_t DeviceAddr)
{  
  uint32_t counter = 0;
  uint16_t tmp;
 
  /* Pause the audio file playing */
  /* Mute the output first */
  counter += wm8994_SetMute(DeviceAddr, AUDIO_MUTE_ON);
  
  /* Put the Codec in Power save mode */
  tmp = 0x01;
  counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_2, tmp);
 
  return counter;
}

/**
  * @brief Resumes playing on the audio codec.
  * @param DeviceAddr: Device address on communication Bus. 
  * @retval 0 if correct communication, else wrong communication
  */
uint32_t wm8994_Resume(uint16_t DeviceAddr)
{
  uint32_t counter = 0;
 
  /* Resumes the audio file playing */  
  /* Unmute the output first */
  counter += wm8994_SetMute(DeviceAddr, AUDIO_MUTE_OFF);
  
  return counter;
}

/**
  * @brief Stops audio Codec playing. It powers down the codec.
  * @param DeviceAddr: Device address on communication Bus. 
  * @param CodecPdwnMode: selects the  power down mode.
  *          - CODEC_PDWN_SW: only mutes the audio codec. When resuming from this 
  *                           mode the codec keeps the previous initialization
  *                           (no need to re-Initialize the codec registers).
  *          - CODEC_PDWN_HW: Physically power down the codec. When resuming from this
  *                           mode, the codec is set to default configuration 
  *                           (user should re-Initialize the codec in order to 
  *                            play again the audio stream).
  * @retval 0 if correct communication, else wrong communication
  */
uint32_t wm8994_Stop(uint16_t DeviceAddr, uint32_t CodecPdwnMode)
{
  uint32_t counter = 0;
  uint16_t tmp;

  if (outputEnabled != 0)
  {
    /* Mute the output first */
    counter += wm8994_SetMute(DeviceAddr, AUDIO_MUTE_ON);

    if (CodecPdwnMode == CODEC_PDWN_SW)
    {
      /* Only output mute required*/
    }
    else /* CODEC_PDWN_HW */
    {
      /* Mute the AIF1 Timeslot 0 DAC1 path */
      tmp = 0x0200;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_FILTER1, tmp);

      /* Mute the AIF1 Timeslot 1 DAC2 path */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC2_FILTER1, tmp);

      /* Disable DAC1L_TO_HPOUT1L */
      tmp = 0x0000;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_OUTPUT_MIXER_1, tmp);

      /* Disable DAC1R_TO_HPOUT1R */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_OUTPUT_MIXER_2, tmp);

      /* Disable DAC1 and DAC2 */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_5, tmp);

      /* Reset Codec by writing in 0x0000 address register */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_SW_RESET, tmp);

      outputEnabled = 0;
    }
  }
  return counter;
}

/**
  * @brief Sets higher or lower the codec volume level.
  * @param DeviceAddr: Device address on communication Bus.
  * @param Volume: a byte value from 0 to 255 (refer to codec registers 
  *         description for more details).
  * @retval 0 if correct communication, else wrong communication
  */
uint32_t wm8994_SetVolume(uint16_t DeviceAddr, uint8_t Volume)
{
  uint32_t counter = 0;
  uint8_t convertedvol = VOLUME_CONVERT(Volume);
  uint16_t tmp;

  /* Output volume */
  if (outputEnabled != 0)
  {
    if(convertedvol > 0x3E)
    {
      /* Unmute audio codec */
      counter += wm8994_SetMute(DeviceAddr, AUDIO_MUTE_OFF);

      /* Left Headphone Volume */
      tmp = 0x3F | 0x140;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_LEFT_OUTPUT_VOL, tmp);

      /* Right Headphone Volume */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_RIGHT_OUTPUT_VOL, tmp);

      /* Left Speaker Volume */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_SPK_LEFT_VOL, tmp);

      /* Right Speaker Volume */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_SPK_RIGHT_VOL, tmp);
    }
    else if (Volume == 0)
    {
      /* Mute audio codec */
      counter += wm8994_SetMute(DeviceAddr, AUDIO_MUTE_ON);
    }
    else
    {
      /* Unmute audio codec */
      counter += wm8994_SetMute(DeviceAddr, AUDIO_MUTE_OFF);

      /* Left Headphone Volume */
      tmp = convertedvol | 0x140;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_LEFT_OUTPUT_VOL, tmp);

      /* Right Headphone Volume */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_RIGHT_OUTPUT_VOL, tmp);

      /* Left Speaker Volume */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_SPK_LEFT_VOL, tmp);

      /* Right Speaker Volume */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_SPK_RIGHT_VOL, tmp);
    }
  }

  /* Input volume */
  if (inputEnabled != 0)
  {
    convertedvol = VOLUME_IN_CONVERT(Volume);

    /* Left AIF1 ADC1 volume */
    tmp = convertedvol | 0x100;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_ADC1_LEFT_VOL, tmp);

    /* Right AIF1 ADC1 volume */
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_ADC1_RIGHT_VOL, tmp);

    /* Left AIF1 ADC2 volume */
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_ADC2_LEFT_VOL, tmp);

    /* Right AIF1 ADC2 volume */
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_ADC2_RIGHT_VOL, tmp);
  }
  return counter;
}

/**
  * @brief Enables or disables the mute feature on the audio codec.
  * @param DeviceAddr: Device address on communication Bus.   
  * @param Cmd: AUDIO_MUTE_ON to enable the mute or AUDIO_MUTE_OFF to disable the
  *             mute mode.
  * @retval 0 if correct communication, else wrong communication
  */
uint32_t wm8994_SetMute(uint16_t DeviceAddr, uint32_t Cmd)
{
  uint32_t counter = 0;
  uint16_t tmp;
  
  if (outputEnabled != 0)
  {
    /* Set the Mute mode */
    if(Cmd == AUDIO_MUTE_ON)
    {
      /* Soft Mute the AIF1 Timeslot 0 DAC1 path L&R */
      tmp = 0x0200;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_FILTER1, tmp);

      /* Soft Mute the AIF1 Timeslot 1 DAC2 path L&R */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC2_FILTER1, tmp);
    }
    else /* AUDIO_MUTE_OFF Disable the Mute */
    {
      /* Unmute the AIF1 Timeslot 0 DAC1 path L&R */
      tmp = 0x0010;
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_FILTER1, tmp);

      /* Unmute the AIF1 Timeslot 1 DAC2 path L&R */
      counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC2_FILTER1, tmp);
    }
  }
  return counter;
}

/**
  * @brief Switch dynamically (while audio file is played) the output target 
  *         (speaker or headphone).
  * @param DeviceAddr: Device address on communication Bus.
  * @param Output: specifies the audio output target: OUTPUT_DEVICE_SPEAKER,
  *         OUTPUT_DEVICE_HEADPHONE, OUTPUT_DEVICE_BOTH or OUTPUT_DEVICE_AUTO 
  * @retval 0 if correct communication, else wrong communication
  */
uint32_t wm8994_SetOutputMode(uint16_t DeviceAddr, uint8_t Output)
{
  uint32_t counter = 0; 
  uint16_t tmp;

  switch (Output) 
  {
  case OUTPUT_DEVICE_SPEAKER:
    /* Enable DAC1 (Left), Enable DAC1 (Right), 
    Disable DAC2 (Left), Disable DAC2 (Right)*/
    tmp = 0x0C0C;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_5, tmp);
    
    /* Enable the AIF1 Timeslot 0 (Left) to DAC 1 (Left) mixer path */
    tmp = 0x0000;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_LMR, tmp);
    
    /* Enable the AIF1 Timeslot 0 (Right) to DAC 1 (Right) mixer path */
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_RMR, tmp);
    
    /* Disable the AIF1 Timeslot 1 (Left) to DAC 2 (Left) mixer path */
    tmp = 0x0002;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC2_LMR, tmp);
    
    /* Disable the AIF1 Timeslot 1 (Right) to DAC 2 (Right) mixer path */
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC2_RMR, tmp);
    break;
    
  case OUTPUT_DEVICE_HEADPHONE:
    /* Disable DAC1 (Left), Disable DAC1 (Right), 
    Enable DAC2 (Left), Enable DAC2 (Right)*/
    tmp = 0x0303;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_5, tmp);
    
    /* Enable the AIF1 Timeslot 0 (Left) to DAC 1 (Left) mixer path */
    tmp = 0x0001;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_LMR, tmp);
    
    /* Enable the AIF1 Timeslot 0 (Right) to DAC 1 (Right) mixer path */
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_RMR, tmp);
    
    /* Disable the AIF1 Timeslot 1 (Left) to DAC 2 (Left) mixer path */
    tmp = 0x0000;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC2_LMR, tmp);
    
    /* Disable the AIF1 Timeslot 1 (Right) to DAC 2 (Right) mixer path */
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC2_RMR, tmp);
    break;
    
  case OUTPUT_DEVICE_BOTH:
    /* Enable DAC1 (Left), Enable DAC1 (Right), 
    also Enable DAC2 (Left), Enable DAC2 (Right)*/
    tmp = 0x0303 | 0x0C0C;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_5, tmp);
    
    /* Enable the AIF1 Timeslot 0 (Left) to DAC 1 (Left) mixer path */
    tmp = 0x0001;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_LMR, tmp);
    
    /* Enable the AIF1 Timeslot 0 (Right) to DAC 1 (Right) mixer path */
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_RMR, tmp);
    
    /* Enable the AIF1 Timeslot 1 (Left) to DAC 2 (Left) mixer path */
    tmp = 0x0002;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC2_LMR, tmp);
    
    /* Enable the AIF1 Timeslot 1 (Right) to DAC 2 (Right) mixer path */
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC2_RMR, tmp);
    break;
    
  default:
    /* Disable DAC1 (Left), Disable DAC1 (Right), 
    Enable DAC2 (Left), Enable DAC2 (Right)*/
    tmp = 0x0303;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_PWR_MANAGEMENT_5, tmp);
    
    /* Enable the AIF1 Timeslot 0 (Left) to DAC 1 (Left) mixer path */
    tmp = 0x0001;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_LMR, tmp);
    
    /* Enable the AIF1 Timeslot 0 (Right) to DAC 1 (Right) mixer path */
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC1_RMR, tmp);
    
    /* Disable the AIF1 Timeslot 1 (Left) to DAC 2 (Left) mixer path */
    tmp = 0x0000;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC2_LMR, tmp);
    
    /* Disable the AIF1 Timeslot 1 (Right) to DAC 2 (Right) mixer path */
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_DAC2_RMR, tmp);
    break;    
  }  
  return counter;
}

/**
  * @brief Sets new frequency.
  * @param DeviceAddr: Device address on communication Bus.
  * @param AudioFreq: Audio frequency used to play the audio stream.
  * @retval 0 if correct communication, else wrong communication
  */
uint32_t wm8994_SetFrequency(uint16_t DeviceAddr, uint32_t AudioFreq)
{
  uint32_t counter = 0;
  uint16_t tmp;
 
  /*  Clock Configurations */
  switch (AudioFreq)
  {
  case  AUDIO_FREQUENCY_8K:
    /* AIF1 Sample Rate = 8 (KHz), ratio=256 */ 
    tmp = 0x0003;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_RATE, tmp);
    break;
    
  case  AUDIO_FREQUENCY_16K:
    /* AIF1 Sample Rate = 16 (KHz), ratio=256 */ 
    tmp = 0x0033;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_RATE, tmp);
    break;

  case  AUDIO_FREQUENCY_32K:
    /* AIF1 Sample Rate = 32 (KHz), ratio=256 */ 
    tmp = 0x0063;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_RATE, tmp);
    break;
    
  case  AUDIO_FREQUENCY_48K:
    /* AIF1 Sample Rate = 48 (KHz), ratio=256 */ 
    tmp = 0x0083;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_RATE, tmp);
    break;
    
  case  AUDIO_FREQUENCY_96K:
    /* AIF1 Sample Rate = 96 (KHz), ratio=256 */ 
    tmp = 0x00A3;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_RATE, tmp);
    break;
    
  case  AUDIO_FREQUENCY_11K:
    /* AIF1 Sample Rate = 11.025 (KHz), ratio=256 */ 
    tmp = 0x0013;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_RATE, tmp);
    break;
    
  case  AUDIO_FREQUENCY_22K:
    /* AIF1 Sample Rate = 22.050 (KHz), ratio=256 */ 
    tmp = 0x0043;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_RATE, tmp);
    break;
    
  case  AUDIO_FREQUENCY_44K:
    /* AIF1 Sample Rate = 44.1 (KHz), ratio=256 */ 
    tmp = 0x0073;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_RATE, tmp);
    break; 
    
  default:
    /* AIF1 Sample Rate = 48 (KHz), ratio=256 */ 
    tmp = 0x0083;
    counter += CODEC_IO_Write(DeviceAddr, WM8994_AIF1_RATE, tmp);
    break; 
  }
  return counter;
}

/**
  * @brief Resets wm8994 registers.
  * @param DeviceAddr: Device address on communication Bus. 
  * @retval 0 if correct communication, else wrong communication
  */
uint32_t wm8994_Reset(uint16_t DeviceAddr)
{
  uint32_t counter = 0;
  uint16_t tmp;
  
  /* Reset Codec by writing in 0x0000 address register */
  tmp = 0x0000;
  counter = CODEC_IO_Write(DeviceAddr, WM8994_SW_RESET, tmp);
  outputEnabled = 0;
  inputEnabled=0;

  return counter;
}

/**
  * @brief  Writes/Read a single data.
  * @param  Addr: I2C address
  * @param  Reg: Reg address 
  * @param  Value: Data to be written
  * @retval None
  */
static uint8_t CODEC_IO_Write(uint8_t Addr, uint16_t Reg, uint16_t Value)
{
  uint32_t result = 0;
  
 AUDIO_IO_Write(Addr, Reg, Value);
  
#ifdef VERIFY_WRITTENDATA
  /* Verify that the data has been correctly written */
  result = (AUDIO_IO_Read(Addr, Reg) == Value)? 0:1;
#endif /* VERIFY_WRITTENDATA */
  
  return result;
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
