/*
*********************************************************************************************************
*
*	模块名称 : DAC8501 驱动模块(单通道带16位DAC)
*	文件名称 : bsp_dac8501.c
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_DAC8501_H
#define _BSP_DAC8501_H

void bsp_InitDAC8501(void);
void DAC8501_SetDacData(uint8_t _ch, uint16_t _dac);
int32_t DAC8501_DacToVoltage(uint16_t _dac);
uint32_t DAC8501_VoltageToDac(int32_t _volt);

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
