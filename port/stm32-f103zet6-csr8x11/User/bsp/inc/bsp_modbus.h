/*
*********************************************************************************************************
*
*	ģ������ : modbus�ײ���������
*	�ļ����� : bsp_modbus.h
*	��    �� : V1.0
*	˵    �� : ͷ�ļ�
*
*	Copyright (C), 2014-2015, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_MODBUS_H
#define __BSP_MODBUS_H

/* ����KEY_FIFO ��Ϣ֪ͨӦ�ó��� */
//#define MSG_485_RX			0xFE	/* �˴���ֵ��Ҫ�Ͱ������롢����ң�ش���ͬ����룬����Ψһ�� */

/* �����жϳ���Ĺ���ģʽ -- ModbusInitVar() �������β� */
#define WKM_NO_CRC			0		/* ��У��CRC�����ݳ��ȡ�����ASCIIЭ�顣��ʱ��ֱ�Ӹ�Ӧ�ò㴦�� */
#define WKM_MODBUS_HOST		1		/* У��CRC,��У���ַ�� ��Modbus����Ӧ�� */
#define WKM_MODBUS_DEVICE	2		/* У��CRC����У�鱾����ַ�� ��Modbus�豸���ӻ���Ӧ�� */

/* RTU Ӧ����� */
#define RSP_OK				0		/* �ɹ� */
#define RSP_ERR_CMD			0x01	/* ��֧�ֵĹ����� */
#define RSP_ERR_REG_ADDR	0x02	/* �Ĵ�����ַ���� */
#define RSP_ERR_VALUE		0x03	/* ����ֵ����� */
#define RSP_ERR_WRITE		0x04	/* д��ʧ�� */

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

	uint8_t WorkMode;	/* �����жϵĹ���ģʽ�� ASCII, MODBUS������ MODBUS �ӻ� */
}MODBUS_T;

void MODBUS_InitVar(uint32_t _Baud, uint8_t _Para1);
void MODBUS_Poll(void);
void MODBUS_SendWithCRC(uint8_t *_pBuf, uint8_t _ucLen);
void MODBUS_ReciveNew(uint8_t _byte);		/* �����ڽ����жϷ��������� */

extern MODBUS_T g_tModbus;

#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
