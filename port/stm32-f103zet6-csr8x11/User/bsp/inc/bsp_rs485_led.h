/*
*********************************************************************************************************
*
*	ģ������ : ������LED-485-XXXϵ������ܵ���������
*	�ļ����� : bsp_rs485_led.h
*	��    �� : V1.0
*	˵    �� : ͷ�ļ�
*
*	Copyright (C), 2014-2015, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_RS485_LED_H
#define __BSP_RS485_LED_H

/* ����LED-485-xxx �������ʾģ���API���� */

void LED485_SetProtRTU(uint8_t _addr);
void LED485_SetProtAscii(uint8_t _addr);
void LED485_DispNumber(uint8_t _addr, int16_t _iNumber);
void LED485_SetDispDot(uint8_t _addr, uint8_t _dot);
void LED485_SetBright(uint8_t _addr, uint8_t _bright);
void LED485_ModifyAddr(uint8_t _addr, uint8_t _NewAddr);
void LED485_DispNumberWithDot(uint8_t _addr, int16_t _iNumber, uint8_t _dot);
void LED485_DispStr(uint8_t _addr, char *_str);

void LED485_TestOk(uint8_t _addr);
void LED485_ReadModel(uint8_t _addr);
void LED485_ReadVersion(uint8_t _addr);
void LED485_ReadBright(uint8_t _addr);
void LED485_SetBrightA(uint8_t _addr, uint8_t _bright);
void LED485_ModifyAddrA(uint8_t _addr, uint8_t _NewAddr);

void LED485_DispNumberA(uint8_t _addr, int16_t _iNumber);
void LED485_DispStrA(uint8_t _addr, char *_str);

#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
