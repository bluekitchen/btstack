/*
*********************************************************************************************************
*
*	模块名称 : modbus底层驱动程序
*	文件名称 : bsp_modbus.h
*	版    本 : V1.0
*	说    明 : 头文件
*
*	Copyright (C), 2014-2015, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_MODBUS_H
#define __BSP_MODBUS_H

/* 借用KEY_FIFO 消息通知应用程序 */
//#define MSG_485_RX			0xFE	/* 此代码值需要和按键代码、红外遥控代码同意编码，保持唯一性 */

/* 接收中断程序的工作模式 -- ModbusInitVar() 函数的形参 */
#define WKM_NO_CRC			0		/* 不校验CRC和数据长度。比如ASCII协议。超时后直接给应用层处理 */
#define WKM_MODBUS_HOST		1		/* 校验CRC,不校验地址。 供Modbus主机应用 */
#define WKM_MODBUS_DEVICE	2		/* 校验CRC，并校验本机地址。 供Modbus设备（从机）应用 */

/* RTU 应答代码 */
#define RSP_OK				0		/* 成功 */
#define RSP_ERR_CMD			0x01	/* 不支持的功能码 */
#define RSP_ERR_REG_ADDR	0x02	/* 寄存器地址错误 */
#define RSP_ERR_VALUE		0x03	/* 数据值域错误 */
#define RSP_ERR_WRITE		0x04	/* 写入失败 */

#define MODBUS_RX_SIZE		128
#define MODBUS_TX_SIZE      128

typedef struct
{
	uint8_t RxBuf[MODBUS_RX_SIZE];
	uint8_t RxCount;
	uint8_t RxStatus;
	uint8_t RxNewFlag;

	uint8_t AppRxBuf[MODBUS_RX_SIZE];
	uint8_t AppRxCount;
	uint8_t AppRxAddr;

	uint32_t Baud;

	uint8_t RspCode;

	uint8_t TxBuf[MODBUS_TX_SIZE];
	uint8_t TxCount;

	uint8_t WorkMode;	/* 接收中断的工作模式： ASCII, MODBUS主机或 MODBUS 从机 */
}MODBUS_T;

void MODBUS_InitVar(uint32_t _Baud, uint8_t _Para1);
void MODBUS_Poll(void);
void MODBUS_SendWithCRC(uint8_t *_pBuf, uint8_t _ucLen);
void MODBUS_ReciveNew(uint8_t _byte);		/* 被串口接收中断服务程序调用 */

extern MODBUS_T g_tModbus;

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
