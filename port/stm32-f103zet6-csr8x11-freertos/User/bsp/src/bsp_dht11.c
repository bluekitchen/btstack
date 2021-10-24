/*
*********************************************************************************************************
*
*	ģ������ : DHT11 ����ģ��(1-wire �����¶ȴ�������
*	�ļ����� : bsp_dht11.c
*	��    �� : V1.0
*	˵    �� : DHT11��CPU֮�����1��GPIO�ӿڡ�������� DHT11_ReadData()��Ƶ�ʲ�Ҫ����1Hz��
*
*	�޸ļ�¼ :
*		�汾��  ����         ����     ˵��
*		V1.0    2014-01-24  armfly  ��ʽ����
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/*
	DHT11 �����öŰ������ӵ������塣

     DHT11      STM32F407������
	  VCC   ------  3.3V
	  DQ    ------  PG8   (���������� 4.7K ��������. �� USER ������)
	  GND   ------  GND
*/

/* ����GPIO�˿� */
#define RCC_DQ		RCC_APB2Periph_GPIOG
#define PORT_DQ		GPIOG
#define PIN_DQ		GPIO_Pin_2

#define DQ_0()		PORT_DQ->BRR = PIN_DQ
#define DQ_1()		PORT_DQ->BSRR = PIN_DQ

/* �ж�DQ�����Ƿ�Ϊ�� */
#define DQ_IS_LOW()	((PORT_DQ->IDR & PIN_DQ) == 0)


static uint8_t DHT11_ReadByte(void);

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitDHT11
*	����˵��: ����STM32��GPIO��SPI�ӿڣ��������� DHT11
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitDHT11(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* ��GPIOʱ�� */
	RCC_APB2PeriphClockCmd(RCC_DQ, ENABLE);

	DQ_1();

	/* ����DQΪ��©��� */
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;	/* ��©���ģʽ */

	GPIO_InitStructure.GPIO_Pin = PIN_DQ;
	GPIO_Init(PORT_DQ, &GPIO_InitStructure);
}

/*
*********************************************************************************************************
*	�� �� ��: DHT11_ReadData
*	����˵��: ��λDHT11�� ����DQΪ�ͣ���������480us��Ȼ��ȴ�
*	��    ��: ��
*	�� �� ֵ: 0 ʧ�ܣ� 1 ��ʾ�ɹ�
*********************************************************************************************************
*/
uint8_t DHT11_ReadData(DHT11_T *_pDTH)
{
	/*
		ʱ��:
		1. MCU����QD����ʱ����� 18ms, Ȼ���ͷ�QD = 1
	*/
	uint8_t i;
	uint8_t k;
	uint8_t sum;

	/* 1. ����������ʼ�ź�, DQ����ʱ�� �� 18ms */
	DQ_0();		/* DQ = 0 */
	bsp_DelayMS(20);
	DQ_1();		/* DQ = 1 */

	bsp_DelayUS(15);	/* �ȴ�15us */

	/* 2. �ȴ�DQ��ƽ��� ( ��ʱ100us) */
	for (k = 0; k < 10; k++)
	{
		if (DQ_IS_LOW())
		{
			break;
		}
		bsp_DelayUS(10);
	}
	if (k >= 10)
	{
		goto quit;		/* ��ʱ��Ӧ��ʧ�� */
	}

	/* 3.�ȴ�DQ��ƽ��� (��ʱ100us) */
	for (k = 0; k < 10; k++)
	{
		if (!DQ_IS_LOW())
		{
			break;
		}
		bsp_DelayUS(10);
	}
	if (k >= 10)
	{
		goto quit;		/* ��ʱ��Ӧ��ʧ�� */
	}

	/* 4.�ȴ�DQ��ƽ��� (��ʱ100us) */
	for (k = 0; k < 10; k++)
	{
		if (DQ_IS_LOW())
		{
			break;
		}
		bsp_DelayUS(10);
	}
	if (k >= 10)
	{
		goto quit;		/* ��ʱ��Ӧ��ʧ�� */
	}

	/* ��40bit ���� */
	for (i = 0; i < 5; i++)
	{
		_pDTH->Buf[i] = DHT11_ReadByte();
	}
	bsp_DelayUS(100);

	/* ����У��� */
	sum = _pDTH->Buf[0] + _pDTH->Buf[1] + _pDTH->Buf[2] + _pDTH->Buf[3];
	if (sum == _pDTH->Buf[4])
	{
		_pDTH->Temp = _pDTH->Buf[2];	/* �¶��������� */
		_pDTH->Hum = _pDTH->Buf[0];	/* ʪ���������� */
		return 1;
	}
quit:
	return 0;
}

/*
*********************************************************************************************************
*	�� �� ��: DHT11_ReadByte
*	����˵��: ��DHT11��ȡ1�ֽ�����
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static uint8_t DHT11_ReadByte(void)
{
	/*
		д����ʱ��, ��DHT11 page 16
	*/
	uint8_t i,k;
	uint8_t read = 0;

	for (i = 0; i < 8; i++)
	{
		read <<= 1;
		/* �ȴ�DQ��ƽ��� (��ʱ100us) */
		for (k = 0; k < 10; k++)
		{
			if (!DQ_IS_LOW())
			{
				break;
			}
			bsp_DelayUS(10);
		}
		if (k >= 10)
		{
			goto quit;		/* ��ʱ��Ӧ��ʧ�� */
		}

		/* �ȴ�DQ��ƽ��ͣ�ͳ��DQ�ߵ�ƽʱ�� (��ʱ100us) */
		for (k = 0; k < 10; k++)
		{
			if (DQ_IS_LOW())
			{
				break;
			}
			bsp_DelayUS(10);
		}

		if (k > 3)		/* ���������ʱ�����40us ����Ϊ��1��������0 */
		{
			read++;
		}
	}

	return read;

quit:
	return 0xFF;
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
