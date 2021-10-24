/*
*********************************************************************************************************
*
*	模块名称 : TM7705 驱动模块(2通道带PGA的16位ADC)
*	文件名称 : bsp_tm7705.h
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
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

extern uint8_t g_TM7705_OK;		/* 全局标志，表示TM7705芯片是否连接正常  */

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
