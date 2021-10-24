/*
*********************************************************************************************************
*
*	ģ������ : NOR Flash����ģ��
*	�ļ����� : bsp_nor_flash.c
*	��    �� : V1.1
*	˵    �� : ������STM32-V5����������NOR Flash Ϊ S29GL128P10TFI01  ����16M�ֽڣ�16Bit��100ns�ٶ�
*				�����ַ : 0x6400 0000
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2013-02-01  armfly  ��ʽ����
*		V1.1	2015-08-03  armfly  �޸�NOR_ReadBuffer()��BUG
*
*	Copyright (C), 2015-2016, ���������� www.armfly.com
*
*********************************************************************************************************
*/

/*
	S29GL128P �ڴ���֯�ṹ�� ÿ������128K�ֽڣ�һ��128��������������Ϊ 16M�ֽڡ���16Bit���ʡ�

	����STM32�ϣ���Ӧ�������ַ��ΧΪ �� 0x6400 0000 - 0x64FF FFFF.  ֻ�ܰ�16Bitģʽ���ʡ�

*/

#include "bsp.h"

#define ADDR_SHIFT(A) 	(NOR_FLASH_ADDR + (2 * (A)))
#define NOR_WRITE(Address, Data)  (*(__IO uint16_t *)(Address) = (Data))

/* ��æʱ��ִ�����ѭ������ */
#define BlockErase_Timeout    ((uint32_t)0x00A00000)
#define ChipErase_Timeout     ((uint32_t)0x30000000)
#define Program_Timeout       ((uint32_t)0x00001400)

/* PD6 ��NOR Flash�����STM32��æ�ź�, ͨ��GPIO��ѯ��ʽ��æ */
#define NOR_IS_BUSY()	(GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_6) == RESET)

static void NOR_QuitToReadStatus(void);
static uint8_t NOR_GetStatus(uint32_t Timeout);


/*
*********************************************************************************************************
*	�� �� ��: bsp_InitNorFlash
*	����˵��: ���������ⲿNOR Flash��GPIO��FSMC
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitNorFlash(void)
{
	FSMC_NORSRAMInitTypeDef  FSMC_NORSRAMInitStructure;
	FSMC_NORSRAMTimingInitTypeDef  p;
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOE | 
						 RCC_APB2Periph_GPIOF | RCC_APB2Periph_GPIOG, ENABLE);

	/*-- GPIO Configuration ------------------------------------------------------*/
	/* NOR Data lines configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_8 | GPIO_Pin_9 |
								GPIO_Pin_10 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |
								GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 |
								GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	/* NOR Address lines configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 |
								GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_12 | GPIO_Pin_13 |
								GPIO_Pin_14 | GPIO_Pin_15;
	GPIO_Init(GPIOF, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 |
								GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;                            
	GPIO_Init(GPIOG, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	/* NOE and NWE configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	/* NE2 configuration */
	/* armfly : STM32F103ZE-EK ����ʹ��NE3,NE4, �����NOR 4M�ռ����ϻ�����쳣 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_12;
	GPIO_Init(GPIOG, &GPIO_InitStructure);

	/*-- FSMC Configuration ----------------------------------------------------*/
	p.FSMC_AddressSetupTime = 0x05;
	//p.FSMC_AddressSetupTime = 0x07;
	p.FSMC_AddressHoldTime = 0x00;
	p.FSMC_DataSetupTime = 0x07;
	//p.FSMC_DataSetupTime = 0x09;
	p.FSMC_BusTurnAroundDuration = 0x00;
	p.FSMC_CLKDivision = 0x00;
	p.FSMC_DataLatency = 0x00;
	p.FSMC_AccessMode = FSMC_AccessMode_B;

	FSMC_NORSRAMInitStructure.FSMC_Bank = FSMC_Bank1_NORSRAM2;
	FSMC_NORSRAMInitStructure.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
	FSMC_NORSRAMInitStructure.FSMC_MemoryType = FSMC_MemoryType_NOR;
	FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;
	FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
	FSMC_NORSRAMInitStructure.FSMC_WrapMode = FSMC_WrapMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
	FSMC_NORSRAMInitStructure.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
	FSMC_NORSRAMInitStructure.FSMC_ExtendedMode = FSMC_ExtendedMode_Disable;
	//FSMC_NORSRAMInitStructure.FSMC_AsyncWait = FSMC_AsyncWait_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
	FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &p;
	FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct = &p;

	FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);

	/* Enable FSMC Bank1_NOR Bank */
	FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM2, ENABLE);
}

/*
*********************************************************************************************************
*	�� �� ��: NOR_ReadID
*	����˵��: ��ȡNOR Flash������ID
*	��    ��: ��
*	�� �� ֵ: ����ID,32Bit, ��8bit ��Manufacturer_Code, ��24bit������ID
*********************************************************************************************************
*/
uint32_t NOR_ReadID(void)
{
	uint32_t uiID;
	uint8_t id1, id2, id3, id4;

	NOR_WRITE(ADDR_SHIFT(0x0555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x02AA), 0x0055);
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x0090);

	id1 = *(__IO uint16_t *) ADDR_SHIFT(0x0000);
	id2 = *(__IO uint16_t *) ADDR_SHIFT(0x0001);
	id3 = *(__IO uint16_t *) ADDR_SHIFT(0x000E);
	id4 = *(__IO uint16_t *) ADDR_SHIFT(0x000F);

	uiID = ((uint32_t)id1 << 24) | ((uint32_t)id2 << 16)  | ((uint32_t)id3 << 8) | id4;

	NOR_WRITE(NOR_FLASH_ADDR, 0x00F0 );		/* �˳�IDģʽ */

	return uiID;
}

/*
*********************************************************************************************************
*	�� �� ��: NOR_QuitToReadStatus
*	����˵��: ��λNOR���˵���״̬
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void NOR_QuitToReadStatus(void)
{
	NOR_WRITE(ADDR_SHIFT(0x00555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x002AA), 0x0055);
	NOR_WRITE(NOR_FLASH_ADDR, 0x00F0 );
}

/*
*********************************************************************************************************
*	�� �� ��: NOR_GetStatus
*	����˵��: ��ȡNOR�Ĳ���״̬
*	��    ��: ��
*	�� �� ֵ: 0��ʾ�ɹ�.   NOR_SUCCESS, NOR_ERROR, NOR_TIMEOUT
*********************************************************************************************************
*/
static uint8_t NOR_GetStatus(uint32_t Timeout)
{
	uint16_t val1 = 0x00;
	uint16_t val2 = 0x00;
	uint8_t status = NOR_ONGOING;
	uint32_t timeout = Timeout;

	/* �ȴ�NOR���æ�źţ��ߵ�ƽʱ�ȴ�������NOR��æ�źŻ�δ��ӳ��������CPU��ǰ��Ϊ��æ�� */
	while ((!NOR_IS_BUSY()) && (timeout > 0))
	{
		timeout--;
	}

	/* �ȴ�NORæ�źŽ������͵�ƽʱ�ȴ� */
	timeout = Timeout;
	while(NOR_IS_BUSY() && (timeout > 0))
	{
		timeout--;
	}

	/*
		- DQ 6 ���ʱ����
		- DQ 6 �� DQ 2 �ڲ���ʱ����
		- DQ 2 �ڲ�������ʱ����
		- DQ 1 �ڱ�̴���ʱ��1
		- DQ 5 �ڳ�ʱʱ��1
	*/
	/* ͨ����ȡDQ6, DQ5 ������λ�Ƿ���ڷ�ת�����ж�NOR �ڲ������Ƿ���ɡ������æ�����2�ζ��͵�1�ζ������ݲ�ͬ */
	while ((Timeout != 0x00) && (status != NOR_SUCCESS))
	{
		Timeout--;

		/* Read DQ6 */
		val1 = *(__IO uint16_t *)(NOR_FLASH_ADDR);
		val2 = *(__IO uint16_t *)(NOR_FLASH_ADDR);

		/* If DQ6 did not toggle between the two reads then return NOR_Success */
		if ((val1 & 0x0040) == (val2 & 0x0040))
		{
			return NOR_SUCCESS;
		}

		/* Read DQ2 */
		if((val1 & 0x0020) != 0x0020)
		{
			status = NOR_ONGOING;
		}

		val1 = *(__IO uint16_t *)(NOR_FLASH_ADDR);
		val2 = *(__IO uint16_t *)(NOR_FLASH_ADDR);

		if((val1 & 0x0040) == (val2 & 0x0040))
		{
			return NOR_SUCCESS;
		}
		else if ((val1 & 0x0020) == 0x0020)
		{
			status = NOR_ERROR;
			NOR_QuitToReadStatus();
		}
	}

	if (Timeout == 0x00)
	{
		status = NOR_TIMEOUT;
		NOR_QuitToReadStatus();
	}

	/* ���ز���״̬ */
	return (status);
}

/*
*********************************************************************************************************
*	�� �� ��: NOR_EraseChip
*	����˵��: ����NOR Flash����оƬ
*	��    ��: ��
*	�� �� ֵ: 0��ʾ�ɹ�
*********************************************************************************************************
*/
uint8_t NOR_EraseChip(void)
{
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x02AA), 0x0055);
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x0080);
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x02AA), 0x0055);
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x0010);

	return (NOR_GetStatus(ChipErase_Timeout));
}

/*
*********************************************************************************************************
*	�� �� ��: NOR_StartEraseChip
*	����˵��: ��ʼ����NOR Flash����оƬ�� ���ȴ�����
*	��    ��: ��
*	�� �� ֵ: 0��ʾ�ɹ�
*********************************************************************************************************
*/
void NOR_StartEraseChip(void)
{
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x02AA), 0x0055);
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x0080);
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x02AA), 0x0055);
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x0010);
	
	NOR_GetStatus(1000);
}

/*
*********************************************************************************************************
*	�� �� ��: NOR_CheckComplete
*	����˵��: �������Ƿ����
*	��    ��: ��
*	�� �� ֵ: 0��ʾ�ɹ�   NOR_SUCCESS, NOR_ERROR, NOR_TIMEOUT
*********************************************************************************************************
*/
uint8_t NOR_CheckStatus(void)
{
	uint16_t val1 = 0x00;
	uint16_t val2 = 0x00;
	uint8_t status = NOR_ONGOING;
	uint32_t timeout = 10;
	
	/*
		- DQ 6 ���ʱ����
		- DQ 6 �� DQ 2 �ڲ���ʱ����
		- DQ 2 �ڲ�������ʱ����
		- DQ 1 �ڱ�̴���ʱ��1
		- DQ 5 �ڳ�ʱʱ��1
	*/
	/* ͨ����ȡDQ6, DQ5 ������λ�Ƿ���ڷ�ת�����ж�NOR �ڲ������Ƿ���ɡ������æ�����2�ζ��͵�1�ζ������ݲ�ͬ */
	while ((timeout != 0x00) && (status != NOR_SUCCESS))
	{
		timeout--;

		/* Read DQ6 */
		val1 = *(__IO uint16_t *)(NOR_FLASH_ADDR);
		val2 = *(__IO uint16_t *)(NOR_FLASH_ADDR);

		/* If DQ6 did not toggle between the two reads then return NOR_Success */
		if ((val1 & 0x0040) == (val2 & 0x0040))
		{
			return NOR_SUCCESS;
		}

		/* Read DQ2 */
		if((val1 & 0x0020) != 0x0020)
		{
			status = NOR_ONGOING;
		}

		val1 = *(__IO uint16_t *)(NOR_FLASH_ADDR);
		val2 = *(__IO uint16_t *)(NOR_FLASH_ADDR);

		if((val1 & 0x0040) == (val2 & 0x0040))
		{
			return NOR_SUCCESS;
		}
		else if ((val1 & 0x0020) == 0x0020)
		{
			status = NOR_ERROR;
			NOR_QuitToReadStatus();
		}
	}

	if (timeout == 0x00)
	{
		status = NOR_TIMEOUT;
		//NOR_QuitToReadStatus();
	}

	/* ���ز���״̬ */
	return (status);
}

/*
*********************************************************************************************************
*	�� �� ��: NOR_EraseSector
*	����˵��: ����NOR Flashָ��������
*	��    ��: ������ַ
*	�� �� ֵ: NOR_SUCCESS, NOR_ERROR, NOR_TIMEOUT
*********************************************************************************************************
*/
uint8_t NOR_EraseSector(uint32_t _uiBlockAddr)
{
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x02AA), 0x0055);
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x0080);
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x02AA), 0x0055);
	NOR_WRITE((NOR_FLASH_ADDR + _uiBlockAddr), 0x30);

	return (NOR_GetStatus(BlockErase_Timeout));
}

/*
*********************************************************************************************************
*	�� �� ��: NOR_ReadByte
*	����˵��: ��ȡ���ֽ�����
*	��    ��: 	_uiWriteAddr : ƫ�Ƶ�ַ[0, 16*1024*1024 - 2]; ��̵�ַ����Ϊż����Ҳ����Ϊ������
*	�� �� ֵ: ��ȡ��������
*********************************************************************************************************
*/
uint8_t NOR_ReadByte(uint32_t _uiWriteAddr)
{
	uint16_t usHalfWord;

	if (_uiWriteAddr % 2)	/* ������ַ */
	{
		usHalfWord = *(uint16_t *)(NOR_FLASH_ADDR + _uiWriteAddr - 1);
		return (usHalfWord >> 8);	/* ȡ��8Bit */
	}
	else	/* ż����ַ */
	{
		usHalfWord = *(uint16_t *)(NOR_FLASH_ADDR + _uiWriteAddr);
		return usHalfWord;	/* ȡ��8Bit */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: NOR_ReadBuffer
*	����˵��: ������ȡNOR Flash
*	��    ��: 	_pBuf : �ֽ������ݻ����������ڴ�Ŷ���������
*				_uiWriteAddr : ƫ�Ƶ�ַ[0, 16*1024*1024 - 2]; ��̵�ַ����Ϊż����Ҳ����Ϊ������
*				_uiBytes : �ֽڴ�С
*	�� �� ֵ: ��ȡ��������
*********************************************************************************************************
*/
void NOR_ReadBuffer(uint8_t *_pBuf, uint32_t _uiWriteAddr, uint32_t _uiBytes)
{
	uint16_t usHalfWord;
	uint16_t *pNor16;
	uint32_t i;
	uint32_t uiNum;

	uiNum = _uiBytes;
	/* �������ֽ� */
	if (_uiWriteAddr % 2)	/* ������ַ */
	{
		usHalfWord = *(uint16_t *)(NOR_FLASH_ADDR + _uiWriteAddr - 1);
		*_pBuf++ = (usHalfWord >> 8);	/* ȡ��8Bit */
		uiNum--;
		_uiWriteAddr++;		/* ��Ϊż�� */
	}

	/* ����˫�ֽ�ģʽ������ȡNOR������������_pBuf */
	pNor16 = (uint16_t *)(NOR_FLASH_ADDR + _uiWriteAddr);
	// for (i = 0; i < uiNum / 2; i++)
	for (i = 0; i < _uiBytes / 2; i++)
	{
		usHalfWord = *pNor16++;
		*_pBuf++ = usHalfWord;
		*_pBuf++ = usHalfWord >> 8;
		uiNum -= 2;
	}

	/* �������1���ֽ� */
	if (uiNum == 1)
	{
		*_pBuf++ = *pNor16;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: NOR_WriteHalfWord
*	����˵��: ���ֱ��. ���ǰִ�н����������С������Ϻ��Զ��˵���ȡģʽ�����ֱ�̿����������ַ��
*				���ǰ��Ҫ��֤�洢��Ԫ��ȫ0xFF״̬�������ظ������ͬ�����ݡ�
*	��    ��: 	_uiWriteAddr : ƫ�Ƶ�ַ[0, 16*1024*1024 - 2]; ��̵�ַ����Ϊż��
*				_usData      : ���� 16Bit
*
*	�� �� ֵ: NOR_SUCCESS, NOR_ERROR, NOR_TIMEOUT
*********************************************************************************************************
*/
uint8_t NOR_WriteHalfWord(uint32_t _uiWriteAddr, uint16_t _usData)
{
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x02AA), 0x0055);
	NOR_WRITE(ADDR_SHIFT(0x0555), 0x00A0);
	NOR_WRITE(NOR_FLASH_ADDR + _uiWriteAddr, _usData);

	return (NOR_GetStatus(Program_Timeout));
}

/*
*********************************************************************************************************
*	�� �� ��: NOR_WriteByte
*	����˵��: �ֽڱ��. ���ǰ��Ҫ��֤�洢��Ԫ��ȫ0xFF״̬�������ظ������ͬ�����ݡ�
*	��    ��: 	_uiWriteAddr : ƫ�Ƶ�ַ[0, 16*1024*1024 - 1]; ��̵�ַ����Ϊ����Ҳ����Ϊż��
*				_usData      : ���� 16Bit
*
*	�� �� ֵ: NOR_SUCCESS, NOR_ERROR, NOR_TIMEOUT
*********************************************************************************************************
*/
uint8_t NOR_WriteByte(uint32_t _uiWriteAddr, uint8_t _ucByte)
{
	uint16_t usHalfWord;

	if (_uiWriteAddr % 2)	/* ������ַ */
	{
		/* ����2�ֽ����ݣ�Ȼ���д���ֽڣ�ά����ǰ�ĵ��ֽ����ݲ��� */
		usHalfWord = *(uint16_t *)(NOR_FLASH_ADDR + _uiWriteAddr - 1);
		usHalfWord &= 0x00FF;
		usHalfWord |= (_ucByte << 8);
	}
	else
	{
		/* ��ȡNORԭʼ���ݣ��������ֽ� */
		usHalfWord = *(uint16_t *)(NOR_FLASH_ADDR + _uiWriteAddr);
		usHalfWord &= 0xFF00;
		usHalfWord |= _ucByte;
	}
	return NOR_WriteHalfWord(_uiWriteAddr, usHalfWord);
}

/*
*********************************************************************************************************
*	�� �� ��: NOR_WriteInPage.    
*	����˵��: ҳ���ڱ�̣�64�ֽ�һ��ҳ�棩. ���ǰ��Ҫ��֤�洢��Ԫ��ȫ0xFF״̬�������ظ������ͬ�����ݡ�
*	��    ��: 	pBuffer : ���ݴ���ڴ˻�����
*				_uiWriteAddr : ƫ�Ƶ�ַ, ������ż����ʼ
*				_usNumHalfword      : ���ݸ�ʽ��˫�ֽ�Ϊ1����λ. ֵ��: 1-32
*
*	�� �� ֵ: NOR_SUCCESS, NOR_ERROR, NOR_TIMEOUT
*********************************************************************************************************
*/
uint8_t NOR_WriteInPage(uint16_t *pBuffer, uint32_t _uiWriteAddr,  uint16_t _usNumHalfword)
{
	uint32_t lastloadedaddress;
	uint32_t currentaddress;
	uint32_t endaddress;

	/* pdf ��7.7 д�뻺�������

		д�뻺�����������ϵͳ��һ����̲�����д�����32 ���֡����׼�ġ� �֡� ����㷨��ȣ��������Ч��
		�ӿ��ֱ���ٶȡ�
	*/
	
	if (_usNumHalfword > 32)
	{
		return NOR_ERROR;
	}
	
	if ((_uiWriteAddr % 2) != 0)
	{
		return NOR_ERROR;
	}
	
	_uiWriteAddr = _uiWriteAddr / 2;

	currentaddress = _uiWriteAddr;
	endaddress = _uiWriteAddr + _usNumHalfword - 1;
	lastloadedaddress = _uiWriteAddr;

	/* ������������ */
	NOR_WRITE(ADDR_SHIFT(0x00555), 0x00AA);
	NOR_WRITE(ADDR_SHIFT(0x02AA), 0x0055);

	/* Write Write Buffer Load Command */
	NOR_WRITE(ADDR_SHIFT(_uiWriteAddr), 0x0025);
	NOR_WRITE(ADDR_SHIFT(_uiWriteAddr), (_usNumHalfword - 1));

	/*  Load Data into NOR Buffer */
	while (currentaddress <= endaddress)
	{
		/* Store last loaded address & data value (for polling) */
		lastloadedaddress = currentaddress;

		NOR_WRITE(ADDR_SHIFT(currentaddress), *pBuffer++);
		currentaddress += 1;
	}

	NOR_WRITE(ADDR_SHIFT(lastloadedaddress), 0x29);

	return (NOR_GetStatus(Program_Timeout));
}

/*
*********************************************************************************************************
*	�� �� ��: NOR_WriteBuffer
*	����˵��: ������̲�������ȡ���ֱ��ģʽ��
*			  S29GL ֧��64�ֽ�ҳ���С��������̡���������ʱ��֧��ҳ���̡�
*	��    ��: _pBuf : 8λ���ݻ�����
*			 _uiWriteAddr : д��Ĵ洢��Ԫ�׵�ַ, ����Ϊż��
*			 _uiBytes : �ֽڸ���
*	�� �� ֵ: NOR_SUCCESS, NOR_ERROR, NOR_TIMEOUT
*********************************************************************************************************
*/
uint8_t NOR_WriteBuffer(uint8_t *_pBuf, uint32_t _uiWriteAddr, uint32_t _uiBytes)
{
	uint16_t usHalfWord;
	uint32_t i;
	uint32_t uiNum;
	uint8_t ucStatus;

	uiNum = _uiBytes;
	/* �������ֽ� */
	if (_uiWriteAddr % 2)	/* ������ַ */
	{
		/* ����2�ֽ����ݣ�Ȼ���д���ֽڣ�ά����ǰ�ĵ��ֽ����ݲ��� */
		usHalfWord = *(uint16_t *)(NOR_FLASH_ADDR + _uiWriteAddr - 1);
		usHalfWord &= 0x00FF;
		usHalfWord |= ((*_pBuf++) << 8);

		ucStatus = NOR_WriteHalfWord(_uiWriteAddr - 1, usHalfWord);
		if (ucStatus != NOR_SUCCESS)
		{
			goto err_quit;
		}

		uiNum--;
		_uiWriteAddr++;		/* ��Ϊż�� */
	}

	/* ����˫�ֽ�ģʽ�������NOR���� */
	for (i = 0; i < uiNum / 2; i++)
	{
		usHalfWord = *_pBuf++;
		usHalfWord |= ((*_pBuf++) << 8);

		ucStatus = NOR_WriteHalfWord(_uiWriteAddr, usHalfWord);
		if (ucStatus != NOR_SUCCESS)
		{
			goto err_quit;
		}

		_uiWriteAddr += 2;
	}

	/* �������1���ֽ� */
	if (uiNum % 2)
	{
		/* ��ȡNORԭʼ���ݣ��������ֽ� */
		usHalfWord = *(uint16_t *)(NOR_FLASH_ADDR + _uiWriteAddr);
		usHalfWord &= 0xFF00;
		usHalfWord |= (*_pBuf++);

		ucStatus = NOR_WriteHalfWord(_uiWriteAddr, usHalfWord);
		if (ucStatus != NOR_SUCCESS)
		{
			goto err_quit;
		}
	}
	ucStatus = NOR_SUCCESS;
err_quit:
	return 	ucStatus;
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
