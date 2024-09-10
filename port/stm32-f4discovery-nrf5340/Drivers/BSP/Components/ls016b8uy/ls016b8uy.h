/**
  ******************************************************************************
  * @file    ls016b8uy.h
  * @author  MCD Application Team
  * @brief   This file contains all the functions prototypes for the ls016b8uy.c
  *          driver.
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
#ifndef __LS016B8UY_H
#define __LS016B8UY_H

#ifdef __cplusplus
 extern "C" {
#endif 

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include "../Common/lcd.h"

/** @addtogroup BSP
  * @{
  */ 

/** @addtogroup Components
  * @{
  */ 
  
/** @addtogroup ls016b8uy
  * @{
  */

/** @defgroup LS016B8UY_Exported_Types
  * @{
  */
   
/**
  * @}
  */ 

/** @defgroup LS016B8UY_Exported_Constants
  * @{
  */
/** 
  * @brief  LS016B8UY ID  
  */  
#define  LS016B8UY_ID    0xFFFF
   
/** 
  * @brief  LS016B8UY Size  
  */  
#define  LS016B8UY_LCD_PIXEL_WIDTH    ((uint16_t)180)
#define  LS016B8UY_LCD_PIXEL_HEIGHT   ((uint16_t)180)
   
/** 
  * @brief  LS016B8UY Registers  
  */ 
#define LCD_CMD_SLEEP_IN           0x10
#define LCD_CMD_SLEEP_OUT          0x11
#define LCD_CMD_DISPLAY_OFF        0x28
#define LCD_CMD_DISPLAY_ON         0x29
#define LCD_CMD_WRITE_RAM          0x2C
#define LCD_CMD_READ_RAM           0x2E
#define LCD_CMD_CASET              0x2A
#define LCD_CMD_RASET              0x2B
#define LCD_CMD_VSYNC_OUTPUT       0x35
#define LCD_CMD_NORMAL_DISPLAY     0x36
#define LCD_CMD_IDLE_MODE_OFF      0x38
#define LCD_CMD_IDLE_MODE_ON       0x39
#define LCD_CMD_COLOR_MODE         0x3A
#define LCD_CMD_PANEL_SETTING_1    0xB0
#define LCD_CMD_PANEL_SETTING_2    0xB1
#define LCD_CMD_OSCILLATOR         0xB3
#define LCD_CMD_PANEL_SETTING_LOCK 0xB4
#define LCD_CMD_PANEL_V_PORCH      0xB7
#define LCD_CMD_PANEL_IDLE_V_PORCH 0xB8
#define LCD_CMD_GVDD               0xC0
#define LCD_CMD_OPAMP              0xC2
#define LCD_CMD_RELOAD_MTP_VCOMH   0xC5
#define LCD_CMD_PANEL_TIMING_1     0xC8
#define LCD_CMD_PANEL_TIMING_2     0xC9
#define LCD_CMD_PANEL_TIMING_3     0xCA
#define LCD_CMD_PANEL_TIMING_4     0xCC
#define LCD_CMD_PANEL_POWER        0xD0
#define LCD_CMD_TEARING_EFFECT     0xDD

/**
  * @}
  */
  
/** @defgroup LS016B8UY_Exported_Functions
  * @{
  */ 
void     ls016b8uy_Init(void);
uint16_t ls016b8uy_ReadID(void);
void     ls016b8uy_WriteReg(uint8_t Command, uint8_t *Parameters, uint8_t NbParameters);
uint8_t  ls016b8uy_ReadReg(uint8_t Command);

void     ls016b8uy_DisplayOn(void);
void     ls016b8uy_DisplayOff(void);
void     ls016b8uy_SetCursor(uint16_t Xpos, uint16_t Ypos);
void     ls016b8uy_WritePixel(uint16_t Xpos, uint16_t Ypos, uint16_t RGBCode);
uint16_t ls016b8uy_ReadPixel(uint16_t Xpos, uint16_t Ypos);

void     ls016b8uy_DrawHLine(uint16_t RGBCode, uint16_t Xpos, uint16_t Ypos, uint16_t Length);
void     ls016b8uy_DrawVLine(uint16_t RGBCode, uint16_t Xpos, uint16_t Ypos, uint16_t Length);
void     ls016b8uy_DrawBitmap(uint16_t Xpos, uint16_t Ypos, uint8_t *pbmp);
void     ls016b8uy_DrawRGBImage(uint16_t Xpos, uint16_t Ypos, uint16_t Xsize, uint16_t Ysize, uint8_t *pdata);

void     ls016b8uy_SetDisplayWindow(uint16_t Xpos, uint16_t Ypos, uint16_t Width, uint16_t Height);


uint16_t ls016b8uy_GetLcdPixelWidth(void);
uint16_t ls016b8uy_GetLcdPixelHeight(void);

/* LCD driver structure */
extern LCD_DrvTypeDef   ls016b8uy_drv;

/* LCD IO functions */
void     LCD_IO_Init(void);
void     LCD_IO_WriteMultipleData(uint16_t *pData, uint32_t Size);
void     LCD_IO_WriteReg(uint8_t Reg);
void     LCD_IO_WriteData(uint16_t RegValue);
uint16_t LCD_IO_ReadData(void);
void     LCD_IO_Delay(uint32_t delay);

/**
  * @}
  */ 
      
#ifdef __cplusplus
}
#endif

#endif /* __LS016B8UY_H */

/**
  * @}
  */ 

/**
  * @}
  */ 

/**
  * @}
  */
