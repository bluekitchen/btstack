/*
*********************************************************************************************************
*
*	模块名称 : DAC8562 驱动模块(单通道带16位DAC)
*	文件名称 : bsp_dac8562.c
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
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

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
