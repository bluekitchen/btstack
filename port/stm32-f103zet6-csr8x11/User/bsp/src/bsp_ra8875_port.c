/*
*********************************************************************************************************
*
*	ģ������ : RA8875оƬ��MCU�ӿ�ģ��
*	�ļ����� : bsp_ra8875_port.c
*	��    �� : V1.6
*	˵    �� : RA8875�ײ�������������
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2013-02-01 armfly  ��ʽ����
*		V1.1    2014-09-04 armfly  ͬʱ֧��SPI��8080���ֽӿڣ��Զ�ʶ�𡣲����ú���ơ�
*
*	Copyright (C), 2014-2015, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"


uint8_t g_RA8875_IF = RA_HARD_8080_16;	/*���ӿ����͡�*/

/*
*********************************************************************************************************
*	�� �� ��: RA8875_ConfigGPIO
*	����˵��: ����CPU���ʽӿڣ���FSMC. SPI��
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_ConfigGPIO(void)
{
	static uint8_t s_run_first = 0;
	
	/* ����Ѿ����й�������ִ�� */
	if (s_run_first == 1)
	{
		return;
	}
	
	s_run_first = 1;
	
	/* FSMC�� bsp_tft_lcd.c���Ѿ����ú� */
	
	
	/* RA8875����SPI�ӿ����ú�ͨ�����߷�ʽ��Ȼ���Զ���0X75����������˲��������Զ�ʶ��SPIģʽ */
	{
		uint8_t value;
		
		g_RA8875_IF = RA_HARD_8080_16;	
		RA8875_WriteReg(0x60, 0x1A);	/* 60H�Ĵ�������ɫ�Ĵ�����ɫ[4:0]��5λ��Ч */
		value = RA8875_ReadReg(0x60);
		if (value != 0x1A)
		{
			RA8875_InitSPI();				/* ���ú�SPI�ӿ�  */
			g_RA8875_IF = RA_HARD_SPI;		/* ʶ��Ϊ SPI���� */
		}
	}
	
	#ifdef USE_WAIT_PIN
	{
		GPIO_InitTypeDef GPIO_InitStructure;
		
		/* ʹ�� GPIOʱ�� */
		RCC_APB2PeriphClockCmd(RCC_WAIT, ENABLE);
		
		/* ���ӵ�RA8875��BUSY���ţ���GPIO ʶ��оƬ��æ */
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;		/*������������*/
		GPIO_InitStructure.GPIO_Pin = PIN_WAIT;
		GPIO_Init(PORT_WAIT, &GPIO_InitStructure);
	}
	#endif
}

#ifdef USE_WAIT_PIN
/*
*********************************************************************************************************
*	�� �� ��: RA8875_ReadBusy
*	����˵��: �ж�RA8875��WAIT����״̬
*	��    ��: ��
*	�� �� ֵ: 1 ��ʾ��æ, 0 ��ʾ�ڲ���æ
*********************************************************************************************************
*/
uint8_t RA8875_ReadBusy(void)
{
	if ((PORT_WAIT->IDR & PIN_WAIT) == 0) 
		return 1;
	else 
		return 0;
}
#endif

/*
*********************************************************************************************************
*	�� �� ��: RA8875_Delaly1us
*	����˵��: �ӳٺ���, ��׼, ��Ҫ����RA8875 PLL����֮ǰ����ָ�����ӳ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_Delaly1us(void)
{
	uint8_t i;

	for (i = 0; i < 10; i++);	/* �ӳ�, ��׼ */
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_Delaly1ms
*	����˵��: �ӳٺ���.  ��Ҫ����RA8875 PLL����֮ǰ����ָ�����ӳ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_Delaly1ms(void)
{
	uint16_t i;

	for (i = 0; i < 5000; i++);	/* �ӳ�, ��׼ */
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_WriteCmd
*	����˵��: дRA8875ָ��Ĵ���
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_WriteCmd(uint8_t _ucRegAddr)
{
	if (g_RA8875_IF == RA_HARD_SPI)	/* Ӳ��SPI�ӿ� */
	{
		RA8875_CS_0();
		SPI_ShiftByte(SPI_WRITE_CMD);
		SPI_ShiftByte(_ucRegAddr);
		RA8875_CS_1();
	}
	else if (g_RA8875_IF == RA_HARD_8080_16)	/* 8080Ӳ������16bit */
	{
		RA8875_REG = _ucRegAddr;	/* ���üĴ�����ַ */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_WriteData
*	����˵��: дRA8875ָ��Ĵ���
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_WriteData(uint8_t _ucRegValue)
{
	if (g_RA8875_IF == RA_HARD_SPI)	/* Ӳ��SPI�ӿ� */
	{
		RA8875_CS_0();
		SPI_ShiftByte(SPI_WRITE_DATA);
		SPI_ShiftByte(_ucRegValue);
		RA8875_CS_1();
	}
	else if (g_RA8875_IF == RA_HARD_8080_16)	/* 8080Ӳ������16bit */
	{
		RA8875_RAM = _ucRegValue;	/* ���üĴ�����ַ */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_ReadData
*	����˵��: ��RA8875�Ĵ���ֵ
*	��    ��: ��
*	�� �� ֵ: �Ĵ���ֵ
*********************************************************************************************************
*/
uint8_t RA8875_ReadData(void)
{
	uint8_t value = 0;
	
	if (g_RA8875_IF == RA_HARD_SPI)	/* Ӳ��SPI�ӿ� */
	{
		RA8875_CS_0();
		SPI_ShiftByte(SPI_READ_DATA);
		value = SPI_ShiftByte(0x00);
		RA8875_CS_1();
	}
	else if (g_RA8875_IF == RA_HARD_8080_16)	/* 8080Ӳ������16bit */
	{
		value = RA8875_RAM;		/* ��ȡ�Ĵ���ֵ */
	}

	return value;	
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_WriteData16
*	����˵��: дRA8875�������ߣ�16bit������RGB�Դ�д��
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_WriteData16(uint16_t _usRGB)
{
	if (g_RA8875_IF == RA_HARD_SPI)	/* Ӳ��SPI�ӿ� */
	{
		RA8875_CS_0();
		SPI_ShiftByte(SPI_WRITE_DATA);
		SPI_ShiftByte(_usRGB >> 8);
		RA8875_CS_1();
		
		/* ��������һЩ�ӳ٣���������д���ؿ��ܳ��� */
		RA8875_CS_1();
		RA8875_CS_1();
		
		RA8875_CS_0();
		SPI_ShiftByte(SPI_WRITE_DATA);
		SPI_ShiftByte(_usRGB);
		RA8875_CS_1();
	}
	else if (g_RA8875_IF == RA_HARD_8080_16)	/* 8080Ӳ������16bit */
	{
		RA8875_RAM = _usRGB;	/* ���üĴ�����ַ */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_ReadData16
*	����˵��: ��RA8875�Դ棬16bit RGB
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
uint16_t RA8875_ReadData16(void)
{
	uint16_t value;
	
	if (g_RA8875_IF == RA_HARD_SPI)	/* Ӳ��SPI�ӿ� */
	{
		RA8875_CS_0();
		SPI_ShiftByte(SPI_READ_DATA);
		value = SPI_ShiftByte(0x00);
		value <<= 8;
		value += SPI_ShiftByte(0x00);
		RA8875_CS_1();
	}
	else if (g_RA8875_IF == RA_HARD_8080_16)	/* 8080Ӳ������16bit */
	{
		value = RA8875_RAM;		/* ��ȡ�Ĵ���ֵ */
	}

	return value;	
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_ReadStatus
*	����˵��: ��RA8875״̬�Ĵ���
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
uint8_t RA8875_ReadStatus(void)
{
	uint8_t value = 0;
	
	if (g_RA8875_IF == RA_HARD_SPI)	/* Ӳ��SPI�ӿ� */
	{
		RA8875_CS_0();
		SPI_ShiftByte(SPI_READ_STATUS);
		value = SPI_ShiftByte(0x00);
		RA8875_CS_1();
	}
	else if (g_RA8875_IF == RA_HARD_8080_16)	/* 8080Ӳ������16bit */
	{
		value = RA8875_REG;
	}
	return value;	
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_GetDispMemAddr
*	����˵��: ����Դ��ַ��
*	��    ��: ��
*	�� �� ֵ: �Դ��ַ
*********************************************************************************************************
*/
uint32_t RA8875_GetDispMemAddr(void)
{
	if (g_RA8875_IF == RA_HARD_SPI)	/* Ӳ��SPI�ӿ� */
	{
		return 0;
	}
	else if (g_RA8875_IF == RA_HARD_8080_16)	/* 8080Ӳ������16bit */
	{
		return RA8875_RAM_ADDR;
	}
	return 0;
}

#ifdef RA_HARD_SPI_EN	   /* ����SPI�ӿ�ģʽ */

/*
*********************************************************************************************************
*	�� �� ��: RA8875_InitSPI
*	����˵��: ����STM32��RA8875��SPI���ߣ�ʹ��Ӳ��SPI1, Ƭѡ���������
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_InitSPI(void)
{
	/*
		������STM32-V5 ��������߷��䣺  ����Flash�ͺ�Ϊ W25Q64BVSSIG (80MHz)
		PA5/SPI1_SCK
		PA6/SPI1_MISO
		PA7/SPI1_MOSI
		PG11/TP_NCS			--- ����оƬ��Ƭѡ		(RA8875������SPI�ӿڴ���оƬ��
	*/
	{
		GPIO_InitTypeDef GPIO_InitStructure;

		/* ʹ��GPIO ʱ�� */
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOG, ENABLE);

		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
		GPIO_Init(GPIOA, &GPIO_InitStructure);

		/* ����Ƭѡ����Ϊ�������ģʽ */
		RA8875_CS_1();		/* Ƭѡ�øߣ���ѡ�� */
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
		GPIO_Init(GPIOG, &GPIO_InitStructure);
	}
	
	RA8875_LowSpeedSPI();
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_LowSpeedSPI
*	����˵��: ����STM32�ڲ�SPIӲ���Ĺ���ģʽ���ٶȵȲ��������ڷ���SPI�ӿڵ�RA8875. ���١�
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_LowSpeedSPI(void)
{
	SPI_InitTypeDef  SPI_InitStructure;

	/* ��SPIʱ�� */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

	/* ����SPIӲ������ */
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;	/* ���ݷ���2��ȫ˫�� */
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;		/* STM32��SPI����ģʽ ������ģʽ */
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;	/* ����λ���� �� 8λ */
	/* SPI_CPOL��SPI_CPHA���ʹ�þ���ʱ�Ӻ����ݲ��������λ��ϵ��
	   ��������: ���߿����Ǹߵ�ƽ,��2�����أ������ز�������)
	*/
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;			/* ʱ�������ز������� */
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;		/* ʱ�ӵĵ�2�����ز������� */
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;			/* Ƭѡ���Ʒ�ʽ��������� */

	/* ���ò�����Ԥ��Ƶϵ�� */
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_64;

	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;	/* ����λ������򣺸�λ�ȴ� */
	SPI_InitStructure.SPI_CRCPolynomial = 7;			/* CRC����ʽ�Ĵ�������λ��Ϊ7�������̲��� */
	SPI_Init(SPI1, &SPI_InitStructure);

	SPI_Cmd(SPI1, DISABLE);			/* �Ƚ�ֹSPI  */

	SPI_Cmd(SPI1, ENABLE);				/* ʹ��SPI  */
}

/*
*********************************************************************************************************
*	�� �� ��: RA8875_HighSpeedSPI
*	����˵��: ����STM32�ڲ�SPIӲ���Ĺ���ģʽ���ٶȵȲ��������ڷ���SPI�ӿڵ�RA8875.
*			  ��ʼ����ɺ󣬿��Խ�SPI�л�������ģʽ�������ˢ��Ч�ʡ�
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void RA8875_HighSpeedSPI(void)
{
	SPI_InitTypeDef  SPI_InitStructure;

	/* ��SPIʱ�� */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

	/* ����SPIӲ������ */
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;	/* ���ݷ���2��ȫ˫�� */
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;		/* STM32��SPI����ģʽ ������ģʽ */
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;	/* ����λ���� �� 8λ */
	/* SPI_CPOL��SPI_CPHA���ʹ�þ���ʱ�Ӻ����ݲ��������λ��ϵ��
	   ��������: ���߿����Ǹߵ�ƽ,��2�����أ������ز�������)
	*/
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;			/* ʱ�������ز������� */
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;		/* ʱ�ӵĵ�2�����ز������� */
	//SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;			/* ʱ�������ز������� */
	//SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;		/* ʱ�ӵĵ�2�����ز������� */	
	
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;			/* Ƭѡ���Ʒ�ʽ��������� */

	/*
	
		ʾ����ʵ��Ƶ�� (STM32F103ZE �ϲ���)
		SPI_BaudRatePrescaler_4 ʱ�� SCK = 18M  (��ʾ����������������)
		SPI_BaudRatePrescaler_8 ʱ�� SCK = 9M   (��ʾ�ʹ���������)
		
		F407 �� SP1ʱ��=84M, ��Ҫ 8��Ƶ = 10.5M
	*/
	
	/* ���ò�����Ԥ��Ƶϵ�� */
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;

	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;	/* ����λ������򣺸�λ�ȴ� */
	SPI_InitStructure.SPI_CRCPolynomial = 7;			/* CRC����ʽ�Ĵ�������λ��Ϊ7�������̲��� */
	SPI_Init(SPI1, &SPI_InitStructure);

	SPI_Cmd(SPI1, DISABLE);			/* �Ƚ�ֹSPI  */

	SPI_Cmd(SPI1, ENABLE);				/* ʹ��SPI  */
}

/*
*********************************************************************************************************
*	�� �� ��: SPI_ShiftByte
*	����˵��: ��SPI���߷���һ���ֽڣ�ͬʱ���ؽ��յ����ֽ�
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static uint8_t SPI_ShiftByte(uint8_t _ucByte)
{
	uint8_t ucRxByte;

	/* �ȴ����ͻ������� */
	while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);

	/* ����һ���ֽ� */
	SPI_I2S_SendData(SPI1, _ucByte);

	/* �ȴ����ݽ������ */
	while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);

	/* ��ȡ���յ������� */
	ucRxByte = SPI_I2S_ReceiveData(SPI1);

	/* ���ض��������� */
	return ucRxByte;
}

#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
