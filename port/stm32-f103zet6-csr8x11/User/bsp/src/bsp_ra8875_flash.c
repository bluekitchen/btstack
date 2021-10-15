/*
*********************************************************************************************************
*
*	ģ������ : RA8875оƬ��ҵĴ���Flash����ģ��
*	�ļ����� : bsp_ra8875_flash.c
*	��    �� : V1.1
*	˵    �� : ����RA8875��ҵĴ���Flash ���ֿ�оƬ��ͼ��оƬ����֧�� SST25VF016B��MX25L1606E ��
*			   W25Q64BVSSIG�� ͨ��TFT��ʾ�ӿ���SPI���ߺ�PWM���߿���7�������ϵĴ���Flash��
*				����ע�� RA8875����֧����Ҵ���Flash��д�������������Ӷ���ĵ��ӿ��ص�·����ʵ�֡�
*	�޸ļ�¼ :
*		�汾��  ����       ����    ˵��
*		V1.0    2012-06-25 armfly  �����װ�
*		V1.1	2014-06-27 armfly  ����֧�� W25Q128  (16M�ֽ�)
*		V1.2    2014-06-28 armfly  ��ʼ��������� w25_ReadInfo(), ʶ����FLASH��LCD_RA8875.c�е�
*							RA8875_DispBmpInFlash ���� ��ʾͼ��оƬ��ͼƬ�ĺ������õ�����FLASH�ͺ�
*
*	Copyright (C), 2015-2016, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/*
	������STM32-V5 ������TFT�ӿ��е�SPI���߷��䣺 ����Flash�ͺ�Ϊ W25Q64BVSSIG (80MHz)
	PA5/SPI1_SCK
	PA6/SPI1_MISO
	PA7/SPI1_MOSI
	PG11/TP_NCS
	PB1/PWM	

	����SPI1��ʱ��Դ��84M
*/

/* CS �� PWM ���߶�Ӧ��GPIOʱ�� */
#define	RCC_W25		(RCC_APB2Periph_GPIOG | RCC_APB2Periph_GPIOB)

/* ƬѡGPIO�˿�  */
#define W25_CS_GPIO			GPIOG
#define W25_CS_PIN			GPIO_Pin_11

#define W25_PWM_GPIO		GPIOB
#define W25_PWM_PIN			GPIO_Pin_1

/* Ƭѡ�����õ�ѡ��  */
#define W25_CS_0()     W25_CS_GPIO->BRR = W25_CS_PIN
/* Ƭѡ�����ø߲�ѡ�� */
#define W25_CS_1()     W25_CS_GPIO->BSRR = W25_CS_PIN

/*
	PWM�����õ�ѡ��
	PWM = 1  ���ģʽ֧��STM32��дRA8875��ҵĴ���Flash
	PWM = 0 ������������ģʽ����RA8875 DMA��ȡ��ҵĴ���Flash
*/
#define W25_PWM_0()     W25_PWM_GPIO->BRR = W25_PWM_PIN
#define W25_PWM_1()     W25_PWM_GPIO->BSRR = W25_PWM_PIN


#define CMD_AAI       0xAD  	/* AAI �������ָ��(FOR SST25VF016B) */
#define CMD_DISWR	  0x04		/* ��ֹд, �˳�AAI״̬ */
#define CMD_EWRSR	  0x50		/* ����д״̬�Ĵ��������� */
#define CMD_WRSR      0x01  	/* д״̬�Ĵ������� */
#define CMD_WREN      0x06		/* дʹ������ */
#define CMD_READ      0x03  	/* ������������ */
#define CMD_RDSR      0x05		/* ��״̬�Ĵ������� */
#define CMD_RDID      0x9F		/* ������ID���� */
#define CMD_SE        0x20		/* ������������ */
#define CMD_BE        0xC7		/* ������������ */
#define DUMMY_BYTE    0xA5		/* ���������Ϊ����ֵ�����ڶ����� */

#define WIP_FLAG      0x01		/* ״̬�Ĵ����е����ڱ�̱�־��WIP) */

W25_T g_tW25;

void w25_ReadInfo(void);
static void w25_WriteEnable(void);
static void w25_WriteStatus(uint8_t _ucValue);
static void w25_WaitForWriteEnd(void);
void bsp_CfgSPIForW25(void);
static void w25_ConfigGPIO(void);
void w25_SetCS(uint8_t _level);

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitRA8875Flash
*	����˵��: ��ʼ������FlashӲ���ӿڣ�����STM32��SPIʱ�ӡ�GPIO)
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitRA8875Flash(void)
{
	w25_ConfigGPIO();

	/* ����SPIӲ���������ڷ��ʴ���Flash , �� bsp_spi_bus.c ������*/
	//bsp_CfgSPIForW25();

	/* ʶ����FLASH�ͺ� */
	w25_CtrlByMCU();	/* (������ִ��w25_CtrlByMCU()����PWM=1�л�SPI����Ȩ)  */
	w25_ReadInfo();
	w25_CtrlByRA8875();
}

/*
*********************************************************************************************************
*	�� �� ��: w25_ConfigGPIO
*	����˵��: ����GPIO�� ������ SCK  MOSI  MISO �����SPI���ߡ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void w25_ConfigGPIO(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* ʹ��GPIO ʱ�� */
	RCC_APB2PeriphClockCmd(RCC_W25, ENABLE);

	/* ����Ƭѡ����Ϊ�������ģʽ */
	w25_SetCS(1);		/* Ƭѡ�øߣ���ѡ�� */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	
	GPIO_InitStructure.GPIO_Pin = W25_CS_PIN;
	GPIO_Init(W25_CS_GPIO, &GPIO_InitStructure);

	/* ����TFT�ӿ��е�PWM��ΪΪ�������ģʽ��PWM = 1ʱ ����дRA8875��ҵĴ���Flash */
	/* PF6/LCD_PWM  �����ڵ���RA8875���ı��� */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	
	GPIO_InitStructure.GPIO_Pin = W25_PWM_PIN;
	GPIO_Init(W25_PWM_GPIO, &GPIO_InitStructure);
}


/*
*********************************************************************************************************
*	�� �� ��: bsp_CfgSPIForW25
*	����˵��: ����STM32�ڲ�SPIӲ���Ĺ���ģʽ���ٶȵȲ��������ڷ���SPI�ӿڵĴ���Flash��
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_CfgSPIForW25(void)
{
	SPI_InitTypeDef  SPI_InitStructure;

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
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;

	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;	/* ����λ������򣺸�λ�ȴ� */
	SPI_InitStructure.SPI_CRCPolynomial = 7;			/* CRC����ʽ�Ĵ�������λ��Ϊ7�������̲��� */
	SPI_Init(SPI1, &SPI_InitStructure);

	SPI_Cmd(SPI1, DISABLE);			/* �Ƚ�ֹSPI  */

	SPI_Cmd(SPI1, ENABLE);				/* ʹ��SPI  */
}


/*
*********************************************************************************************************
*	�� �� ��: w25_SetCS
*	����˵��: ����CS�� ����������SPI����
*	��    ��: ��
	�� �� ֵ: ��
*********************************************************************************************************
*/
void w25_SetCS(uint8_t _level)
{
	if (_level == 0)
	{
		bsp_SpiBusEnter();	/* ռ��SPI���ߣ� �������߹��� */

		#ifdef SOFT_SPI		/* ���SPI */
			bsp_SetSpiSck(1);
			W25_CS_0();
		#endif

		#ifdef HARD_SPI		/* Ӳ��SPI */
			bsp_SPI_Init(SPI_Direction_2Lines_FullDuplex | SPI_Mode_Master | SPI_DataSize_8b
				| SPI_CPOL_High | SPI_CPHA_2Edge | SPI_NSS_Soft | SPI_BaudRatePrescaler_8 | SPI_FirstBit_MSB);

			W25_CS_0();
		#endif
	}
	else
	{
		W25_CS_1();

		bsp_SpiBusExit();	/* �ͷ�SPI���ߣ� �������߹��� */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: w25_CtrlByMCU
*	����˵��: ����Flash����Ȩ����MCU ��STM32��
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void w25_CtrlByMCU(void)
{
	/*
		PWM�����õ�ѡ��
		PWM = 1  ���ģʽ֧��STM32��дRA8875��ҵĴ���Flash
		PWM = 0 ������������ģʽ����RA8875 DMA��ȡ��ҵĴ���Flash
	*/
	W25_PWM_1();
}

/*
*********************************************************************************************************
*	�� �� ��: w25_CtrlByRA8875
*	����˵��: ����Flash����Ȩ����RA8875
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void w25_CtrlByRA8875(void)
{
	/*
		PWM�����õ�ѡ��
		PWM = 1  ���ģʽ֧��STM32��дRA8875��ҵĴ���Flash
		PWM = 0 ������������ģʽ����RA8875 DMA��ȡ��ҵĴ���Flash
	*/
	W25_PWM_0();
}

/*
*********************************************************************************************************
*	�� �� ��: w25_SelectChip
*	����˵��: ѡ�񼴽�������оƬ
*	��    ��: _idex = FONT_CHIP ��ʾ�ֿ�оƬ;  idex = BMP_CHIP ��ʾͼ��оƬ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void w25_SelectChip(uint8_t _idex)
{
	/*
		PWM = 1, KOUT3 = 0 д�ֿ�оƬ
		PWM = 1, KOUT3 = 1 дͼ��оƬ
	*/
	#if 1
		if (_idex == FONT_CHIP)
		{
			RA8875_CtrlGPO(3, 0);	/* RA8875 �� KOUT3 = 0 */
		}
		else	/* BMPͼƬоƬ */
		{
			RA8875_CtrlGPO(3, 1);	/* RA8875 �� KOUT3 = 1 */
		}
	#else
		/* ����1Ƭ W25Q128����������ҪRA8875��KOUT ���� */
		/* �������� ͼƬ���а�ĵ���������ҪRA8875��KOUT0 - KOUT3 �ĸ����� */
	#endif


	w25_ReadInfo();				/* �Զ�ʶ��оƬ�ͺ� */

	w25_SetCS(0);				/* �����ʽ��ʹ�ܴ���FlashƬѡ */
	bsp_spiWrite1(CMD_DISWR);		/* ���ͽ�ֹд�������,��ʹ�����д���� */
	w25_SetCS(1);				/* �����ʽ�����ܴ���FlashƬѡ */

	w25_WaitForWriteEnd();		/* �ȴ�����Flash�ڲ�������� */

	w25_WriteStatus(0);			/* �������BLOCK��д���� */
}

/*
*********************************************************************************************************
*	�� �� ��: w25_EraseSector
*	����˵��: ����ָ��������
*	��    ��:  _uiSectorAddr : ������ַ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void w25_EraseSector(uint32_t _uiSectorAddr)
{
	w25_WriteEnable();								/* ����дʹ������ */

	/* ������������ */
	w25_SetCS(0);									/* ʹ��Ƭѡ */
	bsp_spiWrite1(CMD_SE);								/* ���Ͳ������� */
	bsp_spiWrite1((_uiSectorAddr & 0xFF0000) >> 16);	/* ����������ַ�ĸ�8bit */
	bsp_spiWrite1((_uiSectorAddr & 0xFF00) >> 8);		/* ����������ַ�м�8bit */
	bsp_spiWrite1(_uiSectorAddr & 0xFF);				/* ����������ַ��8bit */
	w25_SetCS(1);									/* ����Ƭѡ */

	w25_WaitForWriteEnd();							/* �ȴ�����Flash�ڲ�д������� */
}

/*
*********************************************************************************************************
*	�� �� ��: w25_EraseChip
*	����˵��: ��������оƬ
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void w25_EraseChip(void)
{
	w25_WriteEnable();								/* ����дʹ������ */

	/* ������������ */
	w25_SetCS(0);									/* ʹ��Ƭѡ */
	bsp_spiWrite1(CMD_BE);							/* ������Ƭ�������� */
	w25_SetCS(1);									/* ����Ƭѡ */

	w25_WaitForWriteEnd();							/* �ȴ�����Flash�ڲ�д������� */
}

/*
*********************************************************************************************************
*	�� �� ��: w25_WritePage
*	����˵��: ��һ��page��д�������ֽڡ��ֽڸ������ܳ���ҳ���С��4K)
*	��    ��:  	_pBuf : ����Դ��������
*				_uiWriteAddr ��Ŀ�������׵�ַ
*				_usSize �����ݸ��������ܳ���ҳ���С
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void w25_WritePage(uint8_t * _pBuf, uint32_t _uiWriteAddr, uint16_t _usSize)
{
	uint32_t i, j;

	if (g_tW25.ChipID == SST25VF016B)
	{
		/* AAIָ��Ҫ��������ݸ�����ż�� */
		if ((_usSize < 2) && (_usSize % 2))
		{
			return ;
		}

		w25_WriteEnable();								/* ����дʹ������ */

		w25_SetCS(0);									/* ʹ��Ƭѡ */
		bsp_spiWrite1(CMD_AAI);							/* ����AAI����(��ַ�Զ����ӱ��) */
		bsp_spiWrite1((_uiWriteAddr & 0xFF0000) >> 16);	/* ����������ַ�ĸ�8bit */
		bsp_spiWrite1((_uiWriteAddr & 0xFF00) >> 8);		/* ����������ַ�м�8bit */
		bsp_spiWrite1(_uiWriteAddr & 0xFF);				/* ����������ַ��8bit */
		bsp_spiWrite1(*_pBuf++);							/* ���͵�1������ */
		bsp_spiWrite1(*_pBuf++);							/* ���͵�2������ */
		w25_SetCS(1);									/* ����Ƭѡ */

		w25_WaitForWriteEnd();							/* �ȴ�����Flash�ڲ�д������� */

		_usSize -= 2;									/* ����ʣ���ֽ��� */

		for (i = 0; i < _usSize / 2; i++)
		{
			w25_SetCS(0);								/* ʹ��Ƭѡ */
			bsp_spiWrite1(CMD_AAI);						/* ����AAI����(��ַ�Զ����ӱ��) */
			bsp_spiWrite1(*_pBuf++);						/* �������� */
			bsp_spiWrite1(*_pBuf++);						/* �������� */
			w25_SetCS(1);								/* ����Ƭѡ */
			w25_WaitForWriteEnd();						/* �ȴ�����Flash�ڲ�д������� */
		}

		/* ����д����״̬ */
		w25_SetCS(0);
		bsp_spiWrite1(CMD_DISWR);
		w25_SetCS(1);

		w25_WaitForWriteEnd();							/* �ȴ�����Flash�ڲ�д������� */
	}
	else	/* for MX25L1606E �� W25Q64BV */
	{
		for (j = 0; j < _usSize / 256; j++)
		{
			w25_WriteEnable();								/* ����дʹ������ */

			w25_SetCS(0);									/* ʹ��Ƭѡ */
			bsp_spiWrite1(0x02);								/* ����AAI����(��ַ�Զ����ӱ��) */
			bsp_spiWrite1((_uiWriteAddr & 0xFF0000) >> 16);	/* ����������ַ�ĸ�8bit */
			bsp_spiWrite1((_uiWriteAddr & 0xFF00) >> 8);		/* ����������ַ�м�8bit */
			bsp_spiWrite1(_uiWriteAddr & 0xFF);				/* ����������ַ��8bit */

			for (i = 0; i < 256; i++)
			{
				bsp_spiWrite1(*_pBuf++);					/* �������� */
			}

			w25_SetCS(1);								/* ��ֹƬѡ */

			w25_WaitForWriteEnd();						/* �ȴ�����Flash�ڲ�д������� */

			_uiWriteAddr += 256;
		}

		/* ����д����״̬ */
		w25_SetCS(0);
		bsp_spiWrite1(CMD_DISWR);
		w25_SetCS(1);

		w25_WaitForWriteEnd();							/* �ȴ�����Flash�ڲ�д������� */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: w25_ReadBuffer
*	����˵��: ������ȡ�����ֽڡ��ֽڸ������ܳ���оƬ������
*	��    ��:  	_pBuf : ����Դ��������
*				_uiReadAddr ���׵�ַ
*				_usSize �����ݸ���, ���Դ���PAGE_SIZE,���ǲ��ܳ���оƬ������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void w25_ReadBuffer(uint8_t * _pBuf, uint32_t _uiReadAddr, uint32_t _uiSize)
{
	/* �����ȡ�����ݳ���Ϊ0���߳�������Flash��ַ�ռ䣬��ֱ�ӷ��� */
	if ((_uiSize == 0) ||(_uiReadAddr + _uiSize) > g_tW25.TotalSize)
	{
		return;
	}

	/* ������������ */
	w25_SetCS(0);									/* ʹ��Ƭѡ */
	bsp_spiWrite1(CMD_READ);							/* ���Ͷ����� */
	bsp_spiWrite1((_uiReadAddr & 0xFF0000) >> 16);	/* ����������ַ�ĸ�8bit */
	bsp_spiWrite1((_uiReadAddr & 0xFF00) >> 8);		/* ����������ַ�м�8bit */
	bsp_spiWrite1(_uiReadAddr & 0xFF);				/* ����������ַ��8bit */
	while (_uiSize--)
	{
		*_pBuf++ = bsp_spiRead1();			/* ��һ���ֽڲ��洢��pBuf�������ָ���Լ�1 */
	}
	w25_SetCS(1);									/* ����Ƭѡ */
}

/*
*********************************************************************************************************
*	�� �� ��: w25_ReadID
*	����˵��: ��ȡ����ID
*	��    ��:  ��
*	�� �� ֵ: 32bit������ID (���8bit��0����ЧIDλ��Ϊ24bit��
*********************************************************************************************************
*/
uint32_t w25_ReadID(void)
{
	uint32_t uiID;
	uint8_t id1, id2, id3;

	w25_SetCS(0);									/* ʹ��Ƭѡ */
	bsp_spiWrite1(CMD_RDID);								/* ���Ͷ�ID���� */
	id1 = bsp_spiRead1();					/* ��ID�ĵ�1���ֽ� */
	id2 = bsp_spiRead1();					/* ��ID�ĵ�2���ֽ� */
	id3 = bsp_spiRead1();					/* ��ID�ĵ�3���ֽ� */
	w25_SetCS(1);									/* ����Ƭѡ */

	uiID = ((uint32_t)id1 << 16) | ((uint32_t)id2 << 8) | id3;

	return uiID;
}

/*
*********************************************************************************************************
*	�� �� ��: w25_ReadInfo
*	����˵��: ��ȡ����ID,�������������
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void w25_ReadInfo(void)
{
	/* �Զ�ʶ����Flash�ͺ� */
	{
		g_tW25.ChipID = w25_ReadID();	/* оƬID */

		switch (g_tW25.ChipID)
		{
			case SST25VF016B:
				g_tW25.TotalSize = 2 * 1024 * 1024;	/* ������ = 2M */
				g_tW25.PageSize = 4 * 1024;			/* ҳ���С = 4K */
				break;

			case MX25L1606E:
				g_tW25.TotalSize = 2 * 1024 * 1024;	/* ������ = 2M */
				g_tW25.PageSize = 4 * 1024;			/* ҳ���С = 4K */
				break;

			case W25Q64BV:
				g_tW25.TotalSize = 8 * 1024 * 1024;	/* ������ = 8M */
				g_tW25.PageSize = 4 * 1024;			/* ҳ���С = 4K */
				break;

			case W25Q128:
				g_tW25.TotalSize = 16 * 1024 * 1024;	/* ������ = 16M */
				g_tW25.PageSize = 4 * 1024;			/* ҳ���С = 4K */
				break;

			default:		/* ��ͨ�ֿⲻ֧��ID��ȡ */
				g_tW25.TotalSize = 2 * 1024 * 1024;
				g_tW25.PageSize = 4 * 1024;
				break;
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: w25_WriteEnable
*	����˵��: ����������дʹ������
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void w25_WriteEnable(void)
{
	w25_SetCS(0);									/* ʹ��Ƭѡ */
	bsp_spiWrite1(CMD_WREN);								/* �������� */
	w25_SetCS(1);									/* ����Ƭѡ */
}

/*
*********************************************************************************************************
*	�� �� ��: w25_WriteStatus
*	����˵��: д״̬�Ĵ���
*	��    ��:  _ucValue : ״̬�Ĵ�����ֵ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void w25_WriteStatus(uint8_t _ucValue)
{

	if (g_tW25.ChipID == SST25VF016B)
	{
		/* ��1������ʹ��д״̬�Ĵ��� */
		w25_SetCS(0);									/* ʹ��Ƭѡ */
		bsp_spiWrite1(CMD_EWRSR);							/* ������� ����д״̬�Ĵ��� */
		w25_SetCS(1);									/* ����Ƭѡ */

		/* ��2������д״̬�Ĵ��� */
		w25_SetCS(0);									/* ʹ��Ƭѡ */
		bsp_spiWrite1(CMD_WRSR);							/* ������� д״̬�Ĵ��� */
		bsp_spiWrite1(_ucValue);							/* �������ݣ�״̬�Ĵ�����ֵ */
		w25_SetCS(1);									/* ����Ƭѡ */
	}
	else
	{
		w25_SetCS(0);									/* ʹ��Ƭѡ */
		bsp_spiWrite1(CMD_WRSR);							/* ������� д״̬�Ĵ��� */
		bsp_spiWrite1(_ucValue);							/* �������ݣ�״̬�Ĵ�����ֵ */
		w25_SetCS(1);									/* ����Ƭѡ */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: w25_WaitForWriteEnd
*	����˵��: ����ѭ����ѯ�ķ�ʽ�ȴ������ڲ�д�������
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void w25_WaitForWriteEnd(void)
{
	w25_SetCS(0);									/* ʹ��Ƭѡ */
	bsp_spiWrite1(CMD_RDSR);							/* ������� ��״̬�Ĵ��� */
	while((bsp_spiRead1() & WIP_FLAG) == SET);	/* �ж�״̬�Ĵ�����æ��־λ */
	w25_SetCS(1);									/* ����Ƭѡ */
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
