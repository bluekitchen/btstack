/*
*********************************************************************************************************
*
*	ģ������ : SPI�ӿڴ���FLASH ��дģ��
*	�ļ����� : bsp_spi_flash.c
*	��    �� : V1.1
*	˵    �� : ֧�� SST25VF016B��MX25L1606E �� W25Q64BVSSIG
*			   SST25VF016B ��д��������AAIָ��������д��Ч�ʡ�
*
*			   ȱʡʹ��STM32F4��Ӳ��SPI1�ӿڣ�ʱ��Ƶ�����Ϊ 42MHz ����Ƶʹ�ã�
*
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2013-02-01 armfly  ��ʽ����
*		V1.1    2015-08-07 armfly  4K�Ļ������ŵ������ڲ���ʹ�ö�ջ�ռ䣬���ȫ�ֱ���
*		
*	Copyright (C), 2015-2016 ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/* ����Flsh��ƬѡGPIO�˿�  */
#define SF_RCC_CS 			RCC_APB2Periph_GPIOF
#define SF_PORT_CS			GPIOF
#define SF_PIN_CS			GPIO_Pin_11
#define SF_CS_0()			SF_PORT_CS->BRR = SF_PIN_CS
#define SF_CS_1()			SF_PORT_CS->BSRR = SF_PIN_CS

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

SFLASH_T g_tSF;

static void sf_WriteEnable(void);
static void sf_WriteStatus(uint8_t _ucValue);
static void sf_WaitForWriteEnd(void);
static uint8_t sf_NeedErase(uint8_t * _ucpOldBuf, uint8_t *_ucpNewBuf, uint16_t _uiLen);
static uint8_t sf_CmpData(uint32_t _uiSrcAddr, uint8_t *_ucpTar, uint32_t _uiSize);
static uint8_t sf_AutoWritePage(uint8_t *_ucpSrc, uint32_t _uiWrAddr, uint16_t _usWrLen);
//static uint8_t s_spiBuf[4*1024];	/* ����д�������ȶ�������page���޸Ļ�������������page��д */

void sf_CfgSpiHard(void);
static void sf_ConfigGPIO(void);
static void sf_SetCS(uint8_t _level);

/*
*********************************************************************************************************
*	�� �� ��: sf_ConfigGPIO
*	����˵��: ���ô���Flash��ƬѡGPIO�� ����Ϊ�������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void sf_ConfigGPIO(void)
{
	/*
		������STM32-V4 ��������߷��䣺  ����Flash�ͺ�Ϊ W25Q64BVSSIG (80MHz)
		PF8/SF_CS
	*/
	GPIO_InitTypeDef GPIO_InitStructure;

	/* ʹ��GPIO ʱ�� */
	RCC_APB2PeriphClockCmd(SF_RCC_CS, ENABLE);

	/* ����Ƭѡ����Ϊ�������ģʽ */
	sf_SetCS(1);		/* Ƭѡ�øߣ���ѡ�� */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = SF_PIN_CS;
	GPIO_Init(SF_PORT_CS, &GPIO_InitStructure);
}

/*
*********************************************************************************************************
*	�� �� ��: sf_CfgSpiHard
*	����˵��: ����STM32�ڲ�SPIӲ���Ĺ���ģʽ���ٶȵȲ��������ڷ���SPI�ӿڵĴ���Flash��
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void sf_CfgSpiHard(void)
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

	/* ���ò�����Ԥ��Ƶϵ�� SPI_BaudRatePrescaler_8 ��ʵ��SCK���� 96ns, 10.4MHz */
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;

	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;	/* ����λ������򣺸�λ�ȴ� */
	SPI_InitStructure.SPI_CRCPolynomial = 7;			/* CRC����ʽ�Ĵ�������λ��Ϊ7�������̲��� */
	SPI_Init(SPI1, &SPI_InitStructure);

	SPI_Cmd(SPI1, ENABLE);				/* ʹ��SPI  */
}

/*
*********************************************************************************************************
*	�� �� ��: sf_SetCS(0)
*	����˵��: ����CS�� ����������SPI����
*	��    ��: ��
	�� �� ֵ: ��
*********************************************************************************************************
*/
static void sf_SetCS(uint8_t _level)
{
	if (_level == 0)
	{
		bsp_SpiBusEnter();	/* ռ��SPI���ߣ� �������߹��� */

		#ifdef SOFT_SPI		/* ���SPI */
			bsp_SetSpiSck(1);
			SF_CS_0();
		#endif

		#ifdef HARD_SPI		/* Ӳ��SPI */

			bsp_SPI_Init(SPI_Direction_2Lines_FullDuplex | SPI_Mode_Master | SPI_DataSize_8b
				| SPI_CPOL_High | SPI_CPHA_2Edge | SPI_NSS_Soft | SPI_BaudRatePrescaler_8 | SPI_FirstBit_MSB);

			SF_CS_0();
		#endif
	}
	else
	{
		SF_CS_1();

		bsp_SpiBusExit();	/* �ͷ�SPI���ߣ� �������߹��� */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitSpiFlash
*	����˵��: ��ʼ������FlashӲ���ӿڣ�����STM32��SPIʱ�ӡ�GPIO)
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitSFlash(void)
{
	sf_ConfigGPIO();			/* ����GPIO */
	
	//sf_CfgSpiHard();

	sf_ReadInfo();				/* �Զ�ʶ��оƬ�ͺ� */

	sf_SetCS(0);				/* �����ʽ��ʹ�ܴ���FlashƬѡ */
	bsp_spiWrite1(CMD_DISWR);		/* ���ͽ�ֹд�������,��ʹ�����д���� */
	sf_SetCS(1);				/* �����ʽ�����ܴ���FlashƬѡ */

	sf_WaitForWriteEnd();		/* �ȴ�����Flash�ڲ�������� */

	sf_WriteStatus(0);			/* �������BLOCK��д���� */
}

/*
*********************************************************************************************************
*	�� �� ��: sf_EraseSector
*	����˵��: ����ָ��������
*	��    ��:  _uiSectorAddr : ������ַ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void sf_EraseSector(uint32_t _uiSectorAddr)
{
	sf_WriteEnable();								/* ����дʹ������ */

	/* ������������ */
	sf_SetCS(0);									/* ʹ��Ƭѡ */
	bsp_spiWrite1(CMD_SE);								/* ���Ͳ������� */
	bsp_spiWrite1((_uiSectorAddr & 0xFF0000) >> 16);	/* ����������ַ�ĸ�8bit */
	bsp_spiWrite1((_uiSectorAddr & 0xFF00) >> 8);		/* ����������ַ�м�8bit */
	bsp_spiWrite1(_uiSectorAddr & 0xFF);				/* ����������ַ��8bit */
	sf_SetCS(1);									/* ����Ƭѡ */

	sf_WaitForWriteEnd();							/* �ȴ�����Flash�ڲ�д������� */
}

/*
*********************************************************************************************************
*	�� �� ��: sf_EraseChip
*	����˵��: ��������оƬ
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void sf_EraseChip(void)
{
	sf_WriteEnable();								/* ����дʹ������ */

	/* ������������ */
	sf_SetCS(0);									/* ʹ��Ƭѡ */
	bsp_spiWrite1(CMD_BE);							/* ������Ƭ�������� */
	sf_SetCS(1);									/* ����Ƭѡ */

	sf_WaitForWriteEnd();							/* �ȴ�����Flash�ڲ�д������� */
}

/*
*********************************************************************************************************
*	�� �� ��: sf_PageWrite
*	����˵��: ��һ��page��д�������ֽڡ��ֽڸ������ܳ���ҳ���С��4K)
*	��    ��:  	_pBuf : ����Դ��������
*				_uiWriteAddr ��Ŀ�������׵�ַ
*				_usSize �����ݸ��������ܳ���ҳ���С
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void sf_PageWrite(uint8_t * _pBuf, uint32_t _uiWriteAddr, uint16_t _usSize)
{
	uint32_t i, j;

	if (g_tSF.ChipID == SST25VF016B_ID)
	{
		/* AAIָ��Ҫ��������ݸ�����ż�� */
		if ((_usSize < 2) && (_usSize % 2))
		{
			return ;
		}

		sf_WriteEnable();								/* ����дʹ������ */

		sf_SetCS(0);									/* ʹ��Ƭѡ */
		bsp_spiWrite1(CMD_AAI);							/* ����AAI����(��ַ�Զ����ӱ��) */
		bsp_spiWrite1((_uiWriteAddr & 0xFF0000) >> 16);	/* ����������ַ�ĸ�8bit */
		bsp_spiWrite1((_uiWriteAddr & 0xFF00) >> 8);		/* ����������ַ�м�8bit */
		bsp_spiWrite1(_uiWriteAddr & 0xFF);				/* ����������ַ��8bit */
		bsp_spiWrite1(*_pBuf++);							/* ���͵�1������ */
		bsp_spiWrite1(*_pBuf++);							/* ���͵�2������ */
		sf_SetCS(1);									/* ����Ƭѡ */

		sf_WaitForWriteEnd();							/* �ȴ�����Flash�ڲ�д������� */

		_usSize -= 2;									/* ����ʣ���ֽ��� */

		for (i = 0; i < _usSize / 2; i++)
		{
			sf_SetCS(0);								/* ʹ��Ƭѡ */
			bsp_spiWrite1(CMD_AAI);						/* ����AAI����(��ַ�Զ����ӱ��) */
			bsp_spiWrite1(*_pBuf++);						/* �������� */
			bsp_spiWrite1(*_pBuf++);						/* �������� */
			sf_SetCS(1);								/* ����Ƭѡ */
			sf_WaitForWriteEnd();						/* �ȴ�����Flash�ڲ�д������� */
		}

		/* ����д����״̬ */
		sf_SetCS(0);
		bsp_spiWrite1(CMD_DISWR);
		sf_SetCS(1);

		sf_WaitForWriteEnd();							/* �ȴ�����Flash�ڲ�д������� */
	}
	else	/* for MX25L1606E �� W25Q64BV */
	{
		for (j = 0; j < _usSize / 256; j++)
		{
			sf_WriteEnable();								/* ����дʹ������ */

			sf_SetCS(0);									/* ʹ��Ƭѡ */
			bsp_spiWrite1(0x02);								/* ����AAI����(��ַ�Զ����ӱ��) */
			bsp_spiWrite1((_uiWriteAddr & 0xFF0000) >> 16);	/* ����������ַ�ĸ�8bit */
			bsp_spiWrite1((_uiWriteAddr & 0xFF00) >> 8);		/* ����������ַ�м�8bit */
			bsp_spiWrite1(_uiWriteAddr & 0xFF);				/* ����������ַ��8bit */

			for (i = 0; i < 256; i++)
			{
				bsp_spiWrite1(*_pBuf++);					/* �������� */
			}

			sf_SetCS(1);								/* ��ֹƬѡ */

			sf_WaitForWriteEnd();						/* �ȴ�����Flash�ڲ�д������� */

			_uiWriteAddr += 256;
		}

		/* ����д����״̬ */
		sf_SetCS(0);
		bsp_spiWrite1(CMD_DISWR);
		sf_SetCS(1);

		sf_WaitForWriteEnd();							/* �ȴ�����Flash�ڲ�д������� */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: sf_ReadBuffer
*	����˵��: ������ȡ�����ֽڡ��ֽڸ������ܳ���оƬ������
*	��    ��:  	_pBuf : ����Դ��������
*				_uiReadAddr ���׵�ַ
*				_usSize �����ݸ���, ���Դ���PAGE_SIZE,���ǲ��ܳ���оƬ������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void sf_ReadBuffer(uint8_t * _pBuf, uint32_t _uiReadAddr, uint32_t _uiSize)
{
	/* �����ȡ�����ݳ���Ϊ0���߳�������Flash��ַ�ռ䣬��ֱ�ӷ��� */
	if ((_uiSize == 0) ||(_uiReadAddr + _uiSize) > g_tSF.TotalSize)
	{
		return;
	}

	/* ������������ */
	sf_SetCS(0);									/* ʹ��Ƭѡ */
	bsp_spiWrite1(CMD_READ);							/* ���Ͷ����� */
	bsp_spiWrite1((_uiReadAddr & 0xFF0000) >> 16);	/* ����������ַ�ĸ�8bit */
	bsp_spiWrite1((_uiReadAddr & 0xFF00) >> 8);		/* ����������ַ�м�8bit */
	bsp_spiWrite1(_uiReadAddr & 0xFF);				/* ����������ַ��8bit */
	while (_uiSize--)
	{
		*_pBuf++ = bsp_spiRead1();			/* ��һ���ֽڲ��洢��pBuf�������ָ���Լ�1 */
	}
	sf_SetCS(1);									/* ����Ƭѡ */
}

/*
*********************************************************************************************************
*	�� �� ��: sf_CmpData
*	����˵��: �Ƚ�Flash������.
*	��    ��:  	_ucpTar : ���ݻ�����
*				_uiSrcAddr ��Flash��ַ
*				_uiSize �����ݸ���, ���Դ���PAGE_SIZE,���ǲ��ܳ���оƬ������
*	�� �� ֵ: 0 = ���, 1 = ����
*********************************************************************************************************
*/
static uint8_t sf_CmpData(uint32_t _uiSrcAddr, uint8_t *_ucpTar, uint32_t _uiSize)
{
	uint8_t ucValue;

	/* �����ȡ�����ݳ���Ϊ0���߳�������Flash��ַ�ռ䣬��ֱ�ӷ��� */
	if ((_uiSrcAddr + _uiSize) > g_tSF.TotalSize)
	{
		return 1;
	}

	if (_uiSize == 0)
	{
		return 0;
	}

	sf_SetCS(0);									/* ʹ��Ƭѡ */
	bsp_spiWrite1(CMD_READ);							/* ���Ͷ����� */
	bsp_spiWrite1((_uiSrcAddr & 0xFF0000) >> 16);		/* ����������ַ�ĸ�8bit */
	bsp_spiWrite1((_uiSrcAddr & 0xFF00) >> 8);		/* ����������ַ�м�8bit */
	bsp_spiWrite1(_uiSrcAddr & 0xFF);					/* ����������ַ��8bit */
	while (_uiSize--)
	{
		/* ��һ���ֽ� */
		ucValue = bsp_spiRead1();
		if (*_ucpTar++ != ucValue)
		{
			sf_SetCS(1);
			return 1;
		}
	}
	sf_SetCS(1);
	return 0;
}

/*
*********************************************************************************************************
*	�� �� ��: sf_NeedErase
*	����˵��: �ж�дPAGEǰ�Ƿ���Ҫ�Ȳ�����
*	��    ��:   _ucpOldBuf �� ������
*			   _ucpNewBuf �� ������
*			   _uiLen �����ݸ��������ܳ���ҳ���С
*	�� �� ֵ: 0 : ����Ҫ������ 1 ����Ҫ����
*********************************************************************************************************
*/
static uint8_t sf_NeedErase(uint8_t * _ucpOldBuf, uint8_t *_ucpNewBuf, uint16_t _usLen)
{
	uint16_t i;
	uint8_t ucOld;

	/*
	�㷨��1����old ��, new ����
	      old    new
		  1101   0101
	~     1111
		= 0010   0101

	�㷨��2��: old �󷴵Ľ���� new λ��
		  0010   old
	&	  0101   new
		 =0000

	�㷨��3��: ���Ϊ0,���ʾ�������. �����ʾ��Ҫ����
	*/

	for (i = 0; i < _usLen; i++)
	{
		ucOld = *_ucpOldBuf++;
		ucOld = ~ucOld;

		/* ע������д��: if (ucOld & (*_ucpNewBuf++) != 0) */
		if ((ucOld & (*_ucpNewBuf++)) != 0)
		{
			return 1;
		}
	}
	return 0;
}

/*
*********************************************************************************************************
*	�� �� ��: sf_AutoWritePage
*	����˵��: д1��PAGE��У��,�������ȷ������д���Ρ��������Զ���ɲ���������
*	��    ��:  	_pBuf : ����Դ��������
*				_uiWriteAddr ��Ŀ�������׵�ַ
*				_usSize �����ݸ��������ܳ���ҳ���С
*	�� �� ֵ: 0 : ���� 1 �� �ɹ�
*********************************************************************************************************
*/
static uint8_t sf_AutoWritePage(uint8_t *_ucpSrc, uint32_t _uiWrAddr, uint16_t _usWrLen)
{
	uint16_t i;
	uint16_t j;					/* ������ʱ */
	uint32_t uiFirstAddr;		/* ������ַ */
	uint8_t ucNeedErase;		/* 1��ʾ��Ҫ���� */
	uint8_t cRet;
	uint8_t s_spiBuf[4*1024];	/* ����д�������ȶ�������page���޸Ļ�������������page��д */	

	/* ����Ϊ0ʱ����������,ֱ����Ϊ�ɹ� */
	if (_usWrLen == 0)
	{
		return 1;
	}

	/* ���ƫ�Ƶ�ַ����оƬ�������˳� */
	if (_uiWrAddr >= g_tSF.TotalSize)
	{
		return 0;
	}

	/* ������ݳ��ȴ����������������˳� */
	if (_usWrLen > g_tSF.PageSize)
	{
		return 0;
	}

	/* ���FLASH�е�����û�б仯,��дFLASH */
	sf_ReadBuffer(s_spiBuf, _uiWrAddr, _usWrLen);
	if (memcmp(s_spiBuf, _ucpSrc, _usWrLen) == 0)
	{
		return 1;
	}

	/* �ж��Ƿ���Ҫ�Ȳ������� */
	/* ����������޸�Ϊ�����ݣ�����λ���� 1->0 ���� 0->0, ���������,���Flash���� */
	ucNeedErase = 0;
	if (sf_NeedErase(s_spiBuf, _ucpSrc, _usWrLen))
	{
		ucNeedErase = 1;
	}

	uiFirstAddr = _uiWrAddr & (~(g_tSF.PageSize - 1));

	if (_usWrLen == g_tSF.PageSize)		/* ������������д */
	{
		for	(i = 0; i < g_tSF.PageSize; i++)
		{
			s_spiBuf[i] = _ucpSrc[i];
		}
	}
	else						/* ��д�������� */
	{
		/* �Ƚ��������������ݶ��� */
		sf_ReadBuffer(s_spiBuf, uiFirstAddr, g_tSF.PageSize);

		/* ���������ݸ��� */
		i = _uiWrAddr & (g_tSF.PageSize - 1);
		memcpy(&s_spiBuf[i], _ucpSrc, _usWrLen);
	}

	/* д��֮�����У�飬�������ȷ����д�����3�� */
	cRet = 0;
	for (i = 0; i < 3; i++)
	{

		/* ����������޸�Ϊ�����ݣ�����λ���� 1->0 ���� 0->0, ���������,���Flash���� */
		if (ucNeedErase == 1)
		{
			sf_EraseSector(uiFirstAddr);		/* ����1������ */
		}

		/* ���һ��PAGE */
		sf_PageWrite(s_spiBuf, uiFirstAddr, g_tSF.PageSize);

		if (sf_CmpData(_uiWrAddr, _ucpSrc, _usWrLen) == 0)
		{
			cRet = 1;
			break;
		}
		else
		{
			if (sf_CmpData(_uiWrAddr, _ucpSrc, _usWrLen) == 0)
			{
				cRet = 1;
				break;
			}

			/* ʧ�ܺ��ӳ�һ��ʱ�������� */
			for (j = 0; j < 10000; j++);
		}
	}

	return cRet;
}

/*
*********************************************************************************************************
*	�� �� ��: sf_WriteBuffer
*	����˵��: д1��������У��,�������ȷ������д���Ρ��������Զ���ɲ���������
*	��    ��:  	_pBuf : ����Դ��������
*				_uiWrAddr ��Ŀ�������׵�ַ
*				_usSize �����ݸ��������ܳ���ҳ���С
*	�� �� ֵ: 1 : �ɹ��� 0 �� ʧ��
*********************************************************************************************************
*/
uint8_t sf_WriteBuffer(uint8_t* _pBuf, uint32_t _uiWriteAddr, uint16_t _usWriteSize)
{
	uint16_t NumOfPage = 0, NumOfSingle = 0, Addr = 0, count = 0, temp = 0;

	Addr = _uiWriteAddr % g_tSF.PageSize;
	count = g_tSF.PageSize - Addr;
	NumOfPage =  _usWriteSize / g_tSF.PageSize;
	NumOfSingle = _usWriteSize % g_tSF.PageSize;

	if (Addr == 0) /* ��ʼ��ַ��ҳ���׵�ַ  */
	{
		if (NumOfPage == 0) /* ���ݳ���С��ҳ���С */
		{
			if (sf_AutoWritePage(_pBuf, _uiWriteAddr, _usWriteSize) == 0)
			{
				return 0;
			}
		}
		else 	/* ���ݳ��ȴ��ڵ���ҳ���С */
		{
			while (NumOfPage--)
			{
				if (sf_AutoWritePage(_pBuf, _uiWriteAddr, g_tSF.PageSize) == 0)
				{
					return 0;
				}
				_uiWriteAddr +=  g_tSF.PageSize;
				_pBuf += g_tSF.PageSize;
			}
			if (sf_AutoWritePage(_pBuf, _uiWriteAddr, NumOfSingle) == 0)
			{
				return 0;
			}
		}
	}
	else  /* ��ʼ��ַ����ҳ���׵�ַ  */
	{
		if (NumOfPage == 0) /* ���ݳ���С��ҳ���С */
		{
			if (NumOfSingle > count) /* (_usWriteSize + _uiWriteAddr) > SPI_FLASH_PAGESIZE */
			{
				temp = NumOfSingle - count;

				if (sf_AutoWritePage(_pBuf, _uiWriteAddr, count) == 0)
				{
					return 0;
				}

				_uiWriteAddr +=  count;
				_pBuf += count;

				if (sf_AutoWritePage(_pBuf, _uiWriteAddr, temp) == 0)
				{
					return 0;
				}
			}
			else
			{
				if (sf_AutoWritePage(_pBuf, _uiWriteAddr, _usWriteSize) == 0)
				{
					return 0;
				}
			}
		}
		else	/* ���ݳ��ȴ��ڵ���ҳ���С */
		{
			_usWriteSize -= count;
			NumOfPage =  _usWriteSize / g_tSF.PageSize;
			NumOfSingle = _usWriteSize % g_tSF.PageSize;

			if (sf_AutoWritePage(_pBuf, _uiWriteAddr, count) == 0)
			{
				return 0;
			}

			_uiWriteAddr +=  count;
			_pBuf += count;

			while (NumOfPage--)
			{
				if (sf_AutoWritePage(_pBuf, _uiWriteAddr, g_tSF.PageSize) == 0)
				{
					return 0;
				}
				_uiWriteAddr +=  g_tSF.PageSize;
				_pBuf += g_tSF.PageSize;
			}

			if (NumOfSingle != 0)
			{
				if (sf_AutoWritePage(_pBuf, _uiWriteAddr, NumOfSingle) == 0)
				{
					return 0;
				}
			}
		}
	}
	return 1;	/* �ɹ� */
}

/*
*********************************************************************************************************
*	�� �� ��: sf_ReadID
*	����˵��: ��ȡ����ID
*	��    ��:  ��
*	�� �� ֵ: 32bit������ID (���8bit��0����ЧIDλ��Ϊ24bit��
*********************************************************************************************************
*/
uint32_t sf_ReadID(void)
{
	uint32_t uiID;
	uint8_t id1, id2, id3;

	sf_SetCS(0);									/* ʹ��Ƭѡ */
	bsp_spiWrite1(CMD_RDID);								/* ���Ͷ�ID���� */
	id1 = bsp_spiRead1();					/* ��ID�ĵ�1���ֽ� */
	id2 = bsp_spiRead1();					/* ��ID�ĵ�2���ֽ� */
	id3 = bsp_spiRead1();					/* ��ID�ĵ�3���ֽ� */
	sf_SetCS(1);									/* ����Ƭѡ */

	uiID = ((uint32_t)id1 << 16) | ((uint32_t)id2 << 8) | id3;

	return uiID;
}

/*
*********************************************************************************************************
*	�� �� ��: sf_ReadInfo
*	����˵��: ��ȡ����ID,�������������
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void sf_ReadInfo(void)
{
	/* �Զ�ʶ����Flash�ͺ� */
	{
		g_tSF.ChipID = sf_ReadID();	/* оƬID */

		switch (g_tSF.ChipID)
		{
			case SST25VF016B_ID:
				strcpy(g_tSF.ChipName, "SST25VF016B");
				g_tSF.TotalSize = 2 * 1024 * 1024;	/* ������ = 2M */
				g_tSF.PageSize = 4 * 1024;			/* ҳ���С = 4K */
				break;

			case MX25L1606E_ID:
				strcpy(g_tSF.ChipName, "MX25L1606E");
				g_tSF.TotalSize = 2 * 1024 * 1024;	/* ������ = 2M */
				g_tSF.PageSize = 4 * 1024;			/* ҳ���С = 4K */
				break;

			case W25Q64_ID:
				strcpy(g_tSF.ChipName, "W25Q64");
				g_tSF.TotalSize = 8 * 1024 * 1024;	/* ������ = 8M */
				g_tSF.PageSize = 4 * 1024;			/* ҳ���С = 4K */
				break;


			case W25Q128_ID:
				strcpy(g_tSF.ChipName, "W25Q128");
				g_tSF.TotalSize = 16 * 1024 * 1024;	/* ������ = 16M */
				g_tSF.PageSize = 4 * 1024;			/* ҳ���С = 4K */
				break;			

			default:
				strcpy(g_tSF.ChipName, "Unknow Flash");
				g_tSF.TotalSize = 2 * 1024 * 1024;
				g_tSF.PageSize = 4 * 1024;
				break;
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: sf_WriteEnable
*	����˵��: ����������дʹ������
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void sf_WriteEnable(void)
{
	sf_SetCS(0);									/* ʹ��Ƭѡ */
	bsp_spiWrite1(CMD_WREN);								/* �������� */
	sf_SetCS(1);									/* ����Ƭѡ */
}

/*
*********************************************************************************************************
*	�� �� ��: sf_WriteStatus
*	����˵��: д״̬�Ĵ���
*	��    ��:  _ucValue : ״̬�Ĵ�����ֵ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void sf_WriteStatus(uint8_t _ucValue)
{

	if (g_tSF.ChipID == SST25VF016B_ID)
	{
		/* ��1������ʹ��д״̬�Ĵ��� */
		sf_SetCS(0);									/* ʹ��Ƭѡ */
		bsp_spiWrite1(CMD_EWRSR);							/* ������� ����д״̬�Ĵ��� */
		sf_SetCS(1);									/* ����Ƭѡ */

		/* ��2������д״̬�Ĵ��� */
		sf_SetCS(0);									/* ʹ��Ƭѡ */
		bsp_spiWrite1(CMD_WRSR);							/* ������� д״̬�Ĵ��� */
		bsp_spiWrite1(_ucValue);							/* �������ݣ�״̬�Ĵ�����ֵ */
		sf_SetCS(1);									/* ����Ƭѡ */
	}
	else
	{
		sf_SetCS(0);									/* ʹ��Ƭѡ */
		bsp_spiWrite1(CMD_WRSR);							/* ������� д״̬�Ĵ��� */
		bsp_spiWrite1(_ucValue);							/* �������ݣ�״̬�Ĵ�����ֵ */
		sf_SetCS(1);									/* ����Ƭѡ */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: sf_WaitForWriteEnd
*	����˵��: ����ѭ����ѯ�ķ�ʽ�ȴ������ڲ�д�������
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void sf_WaitForWriteEnd(void)
{
	sf_SetCS(0);									/* ʹ��Ƭѡ */
	bsp_spiWrite1(CMD_RDSR);							/* ������� ��״̬�Ĵ��� */
	while((bsp_spiRead1() & WIP_FLAG) == SET);	/* �ж�״̬�Ĵ�����æ��־λ */
	sf_SetCS(1);									/* ����Ƭѡ */
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
