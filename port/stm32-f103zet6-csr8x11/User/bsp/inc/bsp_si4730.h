/*
*********************************************************************************************************
*
*	模块名称 : AM/FM收音机Si4730 驱动模块
*	文件名称 : bsp_Si730.h
*	版    本 : V1.0
*	说    明 : 头文件
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_SI4730_H
#define __BSP_SI4730_H

#define I2C_ADDR_SI4730_W		0x22
#define I2C_ADDR_SI4730_R		0x23

/* Si4730 寄存器、命令定义 */

/* FM/RDS Receiver Command Summary */
enum
{
	SI4730_CMD_POWER_UP		 	= 0x01, /*Power up device and mode selection. */
	SI4730_CMD_GET_REV 			= 0x10, /*Returns revision information on the device. */
	SI4730_CMD_POWER_DOWN 		= 0x11, /*Power down device. */
	SI4730_CMD_SET_PROPERTY 	= 0x12, /*Sets the value of a property. */
	SI4730_CMD_GET_PROPERTY 	= 0x13, /*Retrieves a property’s value. */
	SI4730_CMD_GET_INT_STATUS 	= 0x14, /*Reads interrupt status bits. */
	SI4730_CMD_PATCH_ARGS		= 0x15, /*Reserved command used for patch file downloads. */
	SI4730_CMD_PATCH_DATA		= 0x16, /*Reserved command used for patch file downloads. */
	SI4730_CMD_FM_TUNE_FREQ 	= 0x20, /*Selects the FM tuning frequency. */
	SI4730_CMD_FM_SEEK_START 	= 0x21, /*Begins searching for a valid frequency. */
	SI4730_CMD_FM_TUNE_STATUS 	= 0x22, /*Queries the status of previous FM_TUNE_FREQ or FM_SEEK_START command. */
	SI4730_CMD_FM_RSQ_STATUS 	= 0x23, /*Queries the status of the Received Signal Quality (RSQ) of the current channel. */
	//SI4730_CMD_FM_RDS_STATUS 	= 0x24, /*Returns RDS information for current channel and reads an entry from RDS FIFO. */
	SI4730_CMD_FM_AGC_STATUS 	= 0x27, /*Queries the current AGC settings */
	SI4730_CMD_FM_AGC_OVERRIDE 	= 0x28, /*Override AGC setting by disabling and forcing it to a fixed value */
	SI4730_CMD_GPIO_CTL			= 0x80, /*Configures GPO1, 2, and 3 as output or Hi-Z. */
	SI4730_CMD_GPIO_SET			= 0x81, /*Sets GPO1, 2, and 3 output level (low or high). */
};

uint8_t SI4730_SendCmd(uint8_t *_pCmdBuf, uint8_t _ucCmdLen);
uint32_t SI4730_WaitStatus80(uint32_t _uiTimeOut, uint8_t _ucStopEn);
uint8_t SI4730_PowerUp_FM_Revice(void);
uint8_t SI4730_PowerUp_AM_Revice(void);
uint8_t SI4730_PowerDown(void);

uint8_t SI4730_GetRevision(uint8_t *_ReadBuf);

uint8_t SI4730_SetFMFreq(uint32_t _uiFreq);
uint8_t SI4730_SetAMFreq(uint32_t _uiFreq);
uint8_t SI4730_SetAMFreqCap(uint32_t _uiFreq, uint16_t _usCap);

uint8_t SI4730_GetFMTuneStatus(uint8_t *_ReadBuf);
uint8_t SI4730_GetAMTuneStatus(uint8_t *_ReadBuf);

uint8_t SI4730_GetFMSignalQuality(uint8_t *_ReadBuf);
uint8_t SI4730_GetAMSignalQuality(uint8_t *_ReadBuf);


uint8_t SI4730_SetOutVolume(uint8_t _ucVolume);
uint8_t SI4730_SetProperty(uint16_t _usPropNumber, uint16_t _usPropValue);
uint8_t SI4704_SetFMIntput(uint8_t _ch);

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
