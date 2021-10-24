/*
*********************************************************************************************************
*
*	模块名称 : TFT液晶显示器驱动模块
*	文件名称 : LCD_SPDF5420.h
*	版    本 : V2.2
*	说    明 : 头文件
*
*	Copyright (C), 2010-2011, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/


#ifndef _LCD_SPFD5420_H
#define _LCD_SPFD5420_H

/* 可供外部模块调用的函数 */
uint16_t SPFD5420_ReadID(void);
void SPFD5420_InitHard(void);
void SPFD5420_DispOn(void);
void SPFD5420_DispOff(void);
void SPFD5420_ClrScr(uint16_t _usColor);
void SPFD5420_PutPixel(uint16_t _usX, uint16_t _usY, uint16_t _usColor);
uint16_t SPFD5420_GetPixel(uint16_t _usX, uint16_t _usY);
void SPFD5420_DrawLine(uint16_t _usX1 , uint16_t _usY1 , uint16_t _usX2 , uint16_t _usY2 , uint16_t _usColor);
void SPFD5420_DrawHLine(uint16_t _usX1 , uint16_t _usY1 , uint16_t _usX2 , uint16_t _usColor);
void SPFD5420_DrawHColorLine(uint16_t _usX1 , uint16_t _usY1, uint16_t _usWidth, const uint16_t *_pColor);
void SPFD5420_DrawHTransLine(uint16_t _usX1 , uint16_t _usY1, uint16_t _usWidth, const uint16_t *_pColor);
void SPFD5420_DrawRect(uint16_t _usX, uint16_t _usY, uint8_t _usHeight, uint16_t _usWidth, uint16_t _usColor);
void SPFD5420_DrawCircle(uint16_t _usX, uint16_t _usY, uint16_t _usRadius, uint16_t _usColor);
void SPFD5420_DrawBMP(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t *_ptr);

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
