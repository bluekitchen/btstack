/*
*********************************************************************************************************
*
*	ģ������ : DAC8562 ����ģ��(��ͨ����16λDAC)
*	�ļ����� : bsp_dac8562.c
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_DAC8562_H
#define _BSP_DAC8562_H

void bsp_InitDAC8562(void);
void DAC8562_SetDacData(uint8_t _ch, uint16_t _dac);
void DAC8562_WriteCmd(uint32_t _cmd);

int32_t DAC8562_DacToVoltage(uint16_t _dac);
uint32_t DAC8562_VoltageToDac(int32_t _volt);

#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
