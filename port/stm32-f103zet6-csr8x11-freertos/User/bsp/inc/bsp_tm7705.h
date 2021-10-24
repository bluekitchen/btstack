/*
*********************************************************************************************************
*
*	ģ������ : TM7705 ����ģ��(2ͨ����PGA��16λADC)
*	�ļ����� : bsp_tm7705.h
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_TM7705_H
#define _BSP_TM7705_H

void bsp_InitTM7705(void);
void TM7705_CalibSelf(uint8_t _ch);
void TM7705_SytemCalibZero(uint8_t _ch);
void TM7705_SytemCalibFull(uint8_t _ch);
uint16_t TM7705_ReadAdc(uint8_t _ch);

void TM7705_WriteReg(uint8_t _RegID, uint32_t _RegValue);
uint32_t TM7705_ReadReg(uint8_t _RegID);

void TM7705_Scan2(void);
void TM7705_Scan1(void);
uint16_t TM7705_GetAdc1(void);
uint16_t TM7705_GetAdc2(void);

extern uint8_t g_TM7705_OK;		/* ȫ�ֱ�־����ʾTM7705оƬ�Ƿ���������  */

#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
