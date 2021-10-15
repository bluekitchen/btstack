/*
*********************************************************************************************************
*
*	ģ������ : TFTҺ����ʾ������ģ��
*	�ļ����� : LCD_SPDF5420.h
*	��    �� : V2.2
*	˵    �� : ͷ�ļ�
*
*	Copyright (C), 2010-2011, ���������� www.armfly.com
*
*********************************************************************************************************
*/


#ifndef _LCD_SPFD5420_H
#define _LCD_SPFD5420_H

/* �ɹ��ⲿģ����õĺ��� */
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

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
