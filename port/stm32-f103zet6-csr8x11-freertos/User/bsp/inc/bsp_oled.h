/*
*********************************************************************************************************
*
*	ģ������ : TFTҺ����ʾ������ģ��
*	�ļ����� : LCD_tft_lcd.h
*	��    �� : V1.0
*	˵    �� : ͷ�ļ�
*
*	Copyright (C), 2010-2011, ���������� www.armfly.com
*
*********************************************************************************************************
*/


#ifndef _BSP_OLED_H
#define _BSP_OLED_H

#if 1
	#include "bsp_tft_lcd.h"	/* ��ʹ�������е� FONT_T �ṹ�� */
#else
	typedef struct
	{
		uint16_t FontCode;	/* ������� 0 ��ʾ16���� */
		uint16_t FrontColor;/* ������ɫ */
		uint16_t BackColor;	/* ���ֱ�����ɫ��͸�� */
		uint16_t Space;		/* ���ּ�࣬��λ = ���� */
	}FONT_T;
#endif

/* �ɹ��ⲿģ����õĺ��� */
void OLED_InitHard(void);
void OLED_DispOn(void);
void OLED_DispOff(void);
void OLED_SetDir(uint8_t _ucDir);
void OLED_SetContrast(uint8_t ucValue);
void OLED_StartDraw(void);
void OLED_EndDraw(void);
void OLED_ClrScr(uint8_t _ucMode);
void OLED_DispStr(uint16_t _usX, uint16_t _usY, char *_ptr, FONT_T *_tFont);
void OLED_PutPixel(uint16_t _usX, uint16_t _usY, uint8_t _ucColor);
uint8_t OLED_GetPixel(uint16_t _usX, uint16_t _usY);
void OLED_DrawLine(uint16_t _usX1 , uint16_t _usY1 , uint16_t _usX2 , uint16_t _usY2 , uint8_t _ucColor);
void OLED_DrawPoints(uint16_t *x, uint16_t *y, uint16_t _usSize, uint8_t _ucColor);
void OLED_DrawRect(uint16_t _usX, uint16_t _usY, uint8_t _usHeight, uint16_t _usWidth, uint8_t _ucColor);
void OLED_DrawCircle(uint16_t _usX, uint16_t _usY, uint16_t _usRadius, uint8_t _ucColor);
void OLED_DrawBMP(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint8_t *_ptr);

#endif


