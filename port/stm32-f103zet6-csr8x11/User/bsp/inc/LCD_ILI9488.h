/*
*********************************************************************************************************
*
*	模块名称 : TFT液晶显示器驱动模块
*	文件名称 : LCD_ILI9488.h
*	版    本 : V1.0
*	说    明 : 头文件
*
*	Copyright (C), 2014-2015, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/


#ifndef _LCD_ILI9488_H
#define _LCD_ILI9488_H

/* 可供外部模块调用的函数 */
uint32_t ILI9488_ReadID(void);
void ILI9488_InitHard(void);
void ILI9488_DispOn(void);
void ILI9488_DispOff(void);
void ILI9488_ClrScr(uint16_t _usColor);
void ILI9488_PutPixel(uint16_t _usX, uint16_t _usY, uint16_t _usColor);
uint16_t ILI9488_GetPixel(uint16_t _usX, uint16_t _usY);
void ILI9488_DrawLine(uint16_t _usX1 , uint16_t _usY1 , uint16_t _usX2 , uint16_t _usY2 , uint16_t _usColor);
void ILI9488_DrawHLine(uint16_t _usX1 , uint16_t _usY1 , uint16_t _usX2 , uint16_t _usColor);
void ILI9488_DrawHColorLine(uint16_t _usX1 , uint16_t _usY1, uint16_t _usWidth, const uint16_t *_pColor);
void ILI9488_DrawHTransLine(uint16_t _usX1 , uint16_t _usY1, uint16_t _usWidth, const uint16_t *_pColor);
void ILI9488_DrawRect(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t _usColor);
void ILI9488_FillRect(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t _usColor);
void ILI9488_DrawCircle(uint16_t _usX, uint16_t _usY, uint16_t _usRadius, uint16_t _usColor);
void ILI9488_DrawBMP(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t *_ptr);

void ILI9488_SetBackLight(uint8_t _bright);
void ILI9488_SetDirection(uint8_t _ucDir);

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
