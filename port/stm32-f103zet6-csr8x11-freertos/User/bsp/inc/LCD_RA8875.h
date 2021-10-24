/*
*********************************************************************************************************
*
*	模块名称 : RA8875芯片驱动模块器驱动模块
*	文件名称 : LCD_RA8875.h
*	版    本 : V1.6
*	说    明 : 头文件
*
*	Copyright (C), 2010-2011, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/


#ifndef _LCD_RA8875_H
#define _LCD_RA8875_H

enum
{
	RA_FONT_16 = 0,		/* RA8875 字体 16点阵 */
	RA_FONT_24 = 1,		/* RA8875 字体 24点阵 */
	RA_FONT_32 = 2		/* RA8875 字体 32点阵 */
};

/* 文字放大参数 */
enum
{
	RA_SIZE_X1 = 0,		/* 原始大小 */
	RA_SIZE_X2 = 1,		/* 放大2倍 */
	RA_SIZE_X3 = 2,		/* 放大3倍 */
	RA_SIZE_X4 = 3		/* 放大4倍 */
};

/* 可供外部模块调用的函数 */
void RA8875_InitHard(void);
void RA8875_WriteReg(uint8_t _ucRegAddr, uint8_t _ucRegValue);
uint8_t RA8875_ReadReg(uint8_t _ucRegAddr);
uint16_t RA8875_ReadID(void);
uint8_t RA8875_IsBusy(void);
void RA8875_Layer1Visable(void);
void RA8875_Layer2Visable(void);
void RA8875_DispOn(void);
void RA8875_DispOff(void);
void RA8875_PutPixel(uint16_t _usX, uint16_t _usY, uint16_t _usColor);
uint16_t RA8875_GetPixel(uint16_t _usX, uint16_t _usY);
void RA8875_SetFrontColor(uint16_t _usColor);
void RA8875_SetBackColor(uint16_t _usColor);
void RA8875_SetFont(uint8_t _ucFontType, uint8_t _ucLineSpace, uint8_t _ucCharSpace);
void RA8875_SetTextZoom(uint8_t _ucHSize, uint8_t _ucVSize);
void RA8875_DispAscii(uint16_t _usX, uint16_t _usY, char *_ptr);
void RA8875_DispStr(uint16_t _usX, uint16_t _usY, char *_ptr);
void RA8875_ClrScr(uint16_t _usColor);
void RA8875_DispBmpInFlash(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth,
	uint32_t _uiFlashAddr);
void RA8875_DrawBMP(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t *_ptr);
void RA8875_DrawLine(uint16_t _usX1 , uint16_t _usY1 , uint16_t _usX2 , uint16_t _usY2 , uint16_t _usColor);
void RA8875_DrawRect(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t _usColor);
void RA8875_FillRect(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t _usColor);
void RA8875_DrawCircle(uint16_t _usX, uint16_t _usY, uint16_t _usRadius, uint16_t _usColor);
void RA8875_FillCircle(uint16_t _usX, uint16_t _usY, uint16_t _usRadius, uint16_t _usColor);
void RA8875_SetDispWin(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth);
void RA8875_QuitWinMode(void);
void RA8875_CtrlGPO(uint8_t _pin, uint8_t _value);
void RA8875_SetBackLight(uint8_t _bright);

void RA8875_TouchInit(void);
uint16_t RA8875_TouchReadX(void);
uint16_t RA8875_TouchReadY(void);

void RA8875_InitSPI(void);
void RA8875_Sleep(void);

/* 下面3个函数用于直接写显存，比如摄像头直接DMA到显示器. 不支持SPI接口模式 */
void RA8875_StartDirectDraw(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth);
void RA8875_QuitDirectDraw(void);

void RA8875_SetDirection(uint8_t _ucDir);

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
