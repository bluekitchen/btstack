/*
*********************************************************************************************************
*
*	ģ������ : VS1053B mp3������ģ��
*	�ļ����� : bsp_vs1053b.c
*	��    �� : V1.0
*	˵    �� : VS1053BоƬ�ײ�������
*
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2013-07-12 armfly  ��ʽ����
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/*
	������STM32-V4�������VS1053B�Ŀ������ӣ�
		PA5/SPI1_SCK
		PA6/SPI1_MISO
		PA7/SPI1_MOSI

		PB5/VS1053_DREQ
		PF8/VS1053_XDCS
		PF9/VS1053_XCS
*/

#define VS1053_CS_0()	GPIOF->BRR = GPIO_Pin_9
#define VS1053_CS_1()	GPIOF->BSRR = GPIO_Pin_9

#define VS1053_DS_0()	GPIOF->BRR = GPIO_Pin_8
#define VS1053_DS_1()	GPIOF->BSRR = GPIO_Pin_8

/* VS1053_DREQ = PB5 */
#define VS1053_IS_BUSY()	((GPIOB->IDR & GPIO_Pin_5) == 0)

#define DUMMY_BYTE    0xFF		/* �ɶ�������ֵ */

uint8_t vs1053ram[5]={0,0,0,0,250};

const uint16_t plugin[605] = { /* Compressed plugin */
  0x0007, 0x0001, 0x8300, 0x0006, 0x01f2, 0xb080, 0x0024, 0x0007, /*    0 */
  0x9257, 0x3f00, 0x0024, 0x0030, 0x0297, 0x3f00, 0x0024, 0x0006, /*    8 */
  0x0017, 0x3f10, 0x0024, 0x3f00, 0x0024, 0x0000, 0xf6d7, 0xf400, /*   10 */
  0x55c0, 0x0000, 0x0817, 0xf400, 0x57c0, 0x0000, 0x004d, 0x000a, /*   18 */
  0x708f, 0x0000, 0xc44e, 0x280f, 0xe100, 0x0006, 0x2016, 0x0000, /*   20 */
  0x028d, 0x0014, 0x1b01, 0x2800, 0xc795, 0x0015, 0x59c0, 0x0000, /*   28 */
  0xfa0d, 0x0039, 0x324f, 0x0000, 0xd20e, 0x2920, 0x41c0, 0x0000, /*   30 */
  0x0024, 0x000a, 0x708f, 0x0000, 0xc44e, 0x280a, 0xcac0, 0x0000, /*   38 */
  0x028d, 0x6fc2, 0x0024, 0x000c, 0x0981, 0x2800, 0xcad5, 0x0000, /*   40 */
  0x18c2, 0x290c, 0x4840, 0x3613, 0x0024, 0x290c, 0x4840, 0x4086, /*   48 */
  0x184c, 0x6234, 0x0024, 0x0000, 0x0024, 0x2800, 0xcad5, 0x0030, /*   50 */
  0x0317, 0x3f00, 0x0024, 0x280a, 0x71c0, 0x002c, 0x9d40, 0x3613, /*   58 */
  0x0024, 0x3e10, 0xb803, 0x3e14, 0x3811, 0x3e11, 0x3805, 0x3e00, /*   60 */
  0x3801, 0x0007, 0xc390, 0x0006, 0xa011, 0x3010, 0x0444, 0x3050, /*   68 */
  0x4405, 0x6458, 0x0302, 0xff94, 0x4081, 0x0003, 0xffc5, 0x48b6, /*   70 */
  0x0024, 0xff82, 0x0024, 0x42b2, 0x0042, 0xb458, 0x0003, 0x4cd6, /*   78 */
  0x9801, 0xf248, 0x1bc0, 0xb58a, 0x0024, 0x6de6, 0x1804, 0x0006, /*   80 */
  0x0010, 0x3810, 0x9bc5, 0x3800, 0xc024, 0x36f4, 0x1811, 0x36f0, /*   88 */
  0x9803, 0x283e, 0x2d80, 0x0fff, 0xffc3, 0x003e, 0x2d4f, 0x2800, /*   90 */
  0xe380, 0x0000, 0xcb4e, 0x3413, 0x0024, 0x2800, 0xd405, 0xf400, /*   98 */
  0x4510, 0x2800, 0xd7c0, 0x6894, 0x13cc, 0x3000, 0x184c, 0x6090, /*   a0 */
  0x93cc, 0x38b0, 0x3812, 0x3004, 0x4024, 0x0000, 0x0910, 0x3183, /*   a8 */
  0x0024, 0x3100, 0x4024, 0x6016, 0x0024, 0x000c, 0x8012, 0x2800, /*   b0 */
  0xd711, 0xb884, 0x104c, 0x6894, 0x3002, 0x2939, 0xb0c0, 0x3e10, /*   b8 */
  0x93cc, 0x4084, 0x9bd2, 0x4282, 0x0024, 0x0000, 0x0041, 0x2800, /*   c0 */
  0xd9c5, 0x6212, 0x0024, 0x0000, 0x0040, 0x2800, 0xdec5, 0x000c, /*   c8 */
  0x8390, 0x2a00, 0xe240, 0x34c3, 0x0024, 0x3444, 0x0024, 0x3073, /*   d0 */
  0x0024, 0x3053, 0x0024, 0x3000, 0x0024, 0x6092, 0x098c, 0x0000, /*   d8 */
  0x0241, 0x2800, 0xe245, 0x32a0, 0x0024, 0x6012, 0x0024, 0x0000, /*   e0 */
  0x0024, 0x2800, 0xe255, 0x0000, 0x0024, 0x3613, 0x0024, 0x3001, /*   e8 */
  0x3844, 0x2920, 0x0580, 0x3009, 0x3852, 0xc090, 0x9bd2, 0x2800, /*   f0 */
  0xe240, 0x3800, 0x1bc4, 0x000c, 0x4113, 0xb880, 0x2380, 0x3304, /*   f8 */
  0x4024, 0x3800, 0x05cc, 0xcc92, 0x05cc, 0x3910, 0x0024, 0x3910, /*  100 */
  0x4024, 0x000c, 0x8110, 0x3910, 0x0024, 0x39f0, 0x4024, 0x3810, /*  108 */
  0x0024, 0x38d0, 0x4024, 0x3810, 0x0024, 0x38f0, 0x4024, 0x34c3, /*  110 */
  0x0024, 0x3444, 0x0024, 0x3073, 0x0024, 0x3063, 0x0024, 0x3000, /*  118 */
  0x0024, 0x4080, 0x0024, 0x0000, 0x0024, 0x2839, 0x53d5, 0x4284, /*  120 */
  0x0024, 0x3613, 0x0024, 0x2800, 0xe585, 0x6898, 0xb804, 0x0000, /*  128 */
  0x0084, 0x293b, 0x1cc0, 0x3613, 0x0024, 0x000c, 0x8117, 0x3711, /*  130 */
  0x0024, 0x37d1, 0x4024, 0x4e8a, 0x0024, 0x0000, 0x0015, 0x2800, /*  138 */
  0xe845, 0xce9a, 0x0024, 0x3f11, 0x0024, 0x3f01, 0x4024, 0x000c, /*  140 */
  0x8197, 0x408a, 0x9bc4, 0x3f15, 0x4024, 0x2800, 0xea85, 0x4284, /*  148 */
  0x3c15, 0x6590, 0x0024, 0x0000, 0x0024, 0x2839, 0x53d5, 0x4284, /*  150 */
  0x0024, 0x0000, 0x0024, 0x2800, 0xd2d8, 0x458a, 0x0024, 0x2a39, /*  158 */
  0x53c0, 0x3009, 0x3851, 0x3e14, 0xf812, 0x3e12, 0xb817, 0x0006, /*  160 */
  0xa057, 0x3e11, 0x9fd3, 0x0023, 0xffd2, 0x3e01, 0x0024, 0x0006, /*  168 */
  0x0011, 0x3111, 0x0024, 0x6498, 0x07c6, 0x868c, 0x2444, 0x3901, /*  170 */
  0x8e06, 0x0030, 0x0551, 0x3911, 0x8e06, 0x3961, 0x9c44, 0xf400, /*  178 */
  0x44c6, 0xd46c, 0x1bc4, 0x36f1, 0xbc13, 0x2800, 0xf615, 0x36f2, /*  180 */
  0x9817, 0x002b, 0xffd2, 0x3383, 0x188c, 0x3e01, 0x8c06, 0x0006, /*  188 */
  0xa097, 0x468c, 0xbc17, 0xf400, 0x4197, 0x2800, 0xf304, 0x3713, /*  190 */
  0x0024, 0x2800, 0xf345, 0x37e3, 0x0024, 0x3009, 0x2c17, 0x3383, /*  198 */
  0x0024, 0x3009, 0x0c06, 0x468c, 0x4197, 0x0006, 0xa052, 0x2800, /*  1a0 */
  0xf544, 0x3713, 0x2813, 0x2800, 0xf585, 0x37e3, 0x0024, 0x3009, /*  1a8 */
  0x2c17, 0x36f1, 0x8024, 0x36f2, 0x9817, 0x36f4, 0xd812, 0x2100, /*  1b0 */
  0x0000, 0x3904, 0x5bd1, 0x2a00, 0xeb8e, 0x3e11, 0x7804, 0x0030, /*  1b8 */
  0x0257, 0x3701, 0x0024, 0x0013, 0x4d05, 0xd45b, 0xe0e1, 0x0007, /*  1c0 */
  0xc795, 0x2800, 0xfd95, 0x0fff, 0xff45, 0x3511, 0x184c, 0x4488, /*  1c8 */
  0xb808, 0x0006, 0x8a97, 0x2800, 0xfd45, 0x3009, 0x1c40, 0x3511, /*  1d0 */
  0x1fc1, 0x0000, 0x0020, 0xac52, 0x1405, 0x6ce2, 0x0024, 0x0000, /*  1d8 */
  0x0024, 0x2800, 0xfd41, 0x68c2, 0x0024, 0x291a, 0x8a40, 0x3e10, /*  1e0 */
  0x0024, 0x2921, 0xca80, 0x3e00, 0x4024, 0x36f3, 0x0024, 0x3009, /*  1e8 */
  0x1bc8, 0x36f0, 0x1801, 0x2808, 0x9300, 0x3601, 0x5804, 0x0007, /*  1f0 */
  0x0001, 0x802e, 0x0006, 0x0002, 0x2800, 0xf700, 0x0007, 0x0001, /*  1f8 */
  0x8050, 0x0006, 0x0028, 0x3e12, 0x3800, 0x2911, 0xf140, 0x3e10, /*  200 */
  0x8024, 0xf400, 0x4595, 0x3593, 0x0024, 0x35f3, 0x0024, 0x3500, /*  208 */
  0x0024, 0x0021, 0x6d82, 0xd024, 0x44c0, 0x0006, 0xa402, 0x2800, /*  210 */
  0x1815, 0xd024, 0x0024, 0x0000, 0x0000, 0x2800, 0x1815, 0x000b, /*  218 */
  0x6d57, 0x3009, 0x3c00, 0x36f0, 0x8024, 0x36f2, 0x1800, 0x2000, /*  220 */
  0x0000, 0x0000, 0x0024, 0x0007, 0x0001, 0x8030, 0x0006, 0x0002, /*  228 */
  0x2800, 0x1400, 0x0007, 0x0001, 0x8064, 0x0006, 0x001c, 0x3e12, /*  230 */
  0xb817, 0x3e14, 0xf812, 0x3e01, 0xb811, 0x0007, 0x9717, 0x0020, /*  238 */
  0xffd2, 0x0030, 0x11d1, 0x3111, 0x8024, 0x3704, 0xc024, 0x3b81, /*  240 */
  0x8024, 0x3101, 0x8024, 0x3b81, 0x8024, 0x3f04, 0xc024, 0x2808, /*  248 */
  0x4800, 0x36f1, 0x9811, 0x0007, 0x0001, 0x8028, 0x0006, 0x0002, /*  250 */
  0x2a00, 0x190e, 0x000a, 0x0001, 0x0300,
};

/*
*********************************************************************************************************
*	�� �� ��: vs1053_Init
*	����˵��: ��ʼ��vs1053BӲ���豸
*	��    ��: ��
*	�� �� ֵ: 1 ��ʾ��ʼ��������0��ʾ��ʼ��������
*********************************************************************************************************
*/
void vs1053_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/*
		�������������VS1003B�Ŀ������ӣ�
		PA6/SPI1_MISO <== SO
		PA7/SPI1_MOSI ==> SI
		PA5/SPI1_SCK  ==> SCLK
		PF9           ==> XCS\     (Ƭѡ, ��LED4���ã�����VS1003Bʱ��LED4����˸)
		PB5           <== DREQ
		PF8           ==> XDCS/BSYNC   (���ݺ�����ѡ��)
		
	*/
	/* �����ģ���ʱ�� */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB
		| RCC_APB2Periph_GPIOF | RCC_APB2Periph_GPIOF
		| RCC_APB2Periph_AFIO, ENABLE);

	/* ����PB5��ΪVS1003B���������� */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;    /* ���� */
	GPIO_Init(GPIOB,&GPIO_InitStructure);

	/* ����PF8��ΪVS1003B��XDS */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;    /* ������� */
	GPIO_Init(GPIOF,&GPIO_InitStructure);	

	/* ����PF9��ΪVS1003B��XCS */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;    /* ������� */
	GPIO_Init(GPIOF,&GPIO_InitStructure);	

	VS1053_CS_1();
	VS1053_DS_1();

#if 0
	{
		
		/*�����SPIģ�⻹��Ӳ��SPI�� bsp_spi_bus.c �ļ������� ��Ҫ������ط�����SPIӲ�� */		
		/*������SPI1���ߣ�SCK, MISO and MOSI */
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;   /* ����������� */
		GPIO_Init(GPIOA,&GPIO_InitStructure);


		/* ����SPIӲ���������ڷ���MP3������VS1053B */
		/* ��SPIʱ�� */
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
		
		bsp_SPI_Init(SPI_Direction_2Lines_FullDuplex | SPI_Mode_Master | SPI_DataSize_8b
			| SPI_CPOL_Low | SPI_CPHA_1Edge | SPI_NSS_Soft | SPI_BaudRatePrescaler_64 | SPI_FirstBit_MSB);
		
		SPI_Cmd(SPI1, DISABLE);			/* ��ֹSPI  */
		SPI_Cmd(SPI1, ENABLE);			/* ʹ��SPI  */
	}
	//bsp_CfgSPIForVS1053B();	
#endif	
}

/*
*********************************************************************************************************
*	�� �� ��: bsp_CfgSPIForVS1053B
*	����˵��: ����STM32�ڲ�SPIӲ���Ĺ���ģʽ���ٶȵȲ��������ڷ���VS1053B
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_CfgSPIForVS1053B(void)
{
	SPI_InitTypeDef  SPI_InitStructure;

	/* ��SPIʱ�� */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

	/* SPI1 ���� */
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;	/* ѡ��2��ȫ˫��ģʽ */
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;		/* CPU��SPI��Ϊ���豸 */
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;	/* 8������ */
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;			/* CLK���ſ���״̬��ƽ = 0 */
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;		/* ���ݲ����ڵ�1������(������) */
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;  			/* �������Ƭѡ */

	/*
		����SPI1��ʱ��Դ��84M, SPI3��ʱ��Դ��42M��Ϊ�˻�ø�����ٶȣ������ѡ��SPI1��
		pdf page=23 vs1053B SPI����ʱ�� 4��CLKI cycles�� CLKI = 12.288M
		������SPIʱ�� = 12.288 / 4 = 3.072MHz
		��Ҫ 32��Ƶ
	*/
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_64;

	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;	/* ���λ�ȴ��� */
	SPI_InitStructure.SPI_CRCPolynomial = 7;
	SPI_Init(SPI1,&SPI_InitStructure);

	SPI_Cmd(SPI1, DISABLE);			/* �Ƚ�ֹSPI  */

	SPI_Cmd(SPI1, ENABLE);			/* ʹ��SPI  */
}


/*
*********************************************************************************************************
*	�� �� ��: vs1053_SetCS(0)
*	����˵��: ����CS�� ����������SPI����
*	��    ��: ��
	�� �� ֵ: ��
*********************************************************************************************************
*/
static void vs1053_SetCS(uint8_t _level)
{
	if (_level == 0)
	{
		bsp_SpiBusEnter();	/* ռ��SPI���ߣ� �������߹��� */

		#ifdef SOFT_SPI		/* ���SPI */
			bsp_SetSpiSck(0);
			VS1053_CS_0();
		#endif

		#ifdef HARD_SPI		/* Ӳ��SPI */
			VS1053_CS_0();

			bsp_SPI_Init(SPI_Direction_2Lines_FullDuplex | SPI_Mode_Master | SPI_DataSize_8b
				| SPI_CPOL_Low | SPI_CPHA_1Edge | SPI_NSS_Soft | SPI_BaudRatePrescaler_64 | SPI_FirstBit_MSB);
		#endif
	}
	else
	{
		VS1053_CS_1();

		bsp_SpiBusExit();	/* �ͷ�SPI���ߣ� �������߹��� */
	}
}


/*
*********************************************************************************************************
*	�� �� ��: vs1053_SetDS(0)
*	����˵��: ����DS�� ����������SPI����
*	��    ��: ��
	�� �� ֵ: ��
*********************************************************************************************************
*/
static void vs1053_SetDS(uint8_t _level)
{
	if (_level == 0)
	{
		bsp_SpiBusEnter();	/* ռ��SPI���ߣ� �������߹��� */

		#ifdef SOFT_SPI		/* ���SPI */
			bsp_SetSpiSck(0);
			VS1053_DS_0();
		#endif

		#ifdef HARD_SPI		/* Ӳ��SPI */
			VS1053_DS_0();

			bsp_SPI_Init(SPI_Direction_2Lines_FullDuplex | SPI_Mode_Master | SPI_DataSize_8b
				| SPI_CPOL_Low | SPI_CPHA_1Edge | SPI_NSS_Soft | SPI_BaudRatePrescaler_64 | SPI_FirstBit_MSB);
		#endif
	}
	else
	{
		VS1053_DS_1();

		bsp_SpiBusExit();	/* �ͷ�SPI���ߣ� �������߹��� */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: vs1053_WriteCmd
*	����˵��: ��vs1053д����
*	��    ��: _ucAddr �� ��ַ�� 		_usData ������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void vs1053_WriteCmd(uint8_t _ucAddr, uint16_t _usData)
{
	/* �ȴ�оƬ�ڲ�������� */
	if (vs1053_WaitTimeOut())
	{
		return;
	}

	vs1053_SetCS(0);

	bsp_spiWrite0(VS_WRITE_COMMAND);	/* ����vs1053��д���� */
	bsp_spiWrite0(_ucAddr); 			/* �Ĵ�����ַ */
	bsp_spiWrite0(_usData >> 8); 	/* ���͸�8λ */
	bsp_spiWrite0(_usData);	 		/* ���͵�8λ */
	
	vs1053_SetCS(1);
}

/*
*********************************************************************************************************
*	�� �� ��: vs1053_ReqNewData
*	����˵��: �ж�vs1053�Ƿ����������ݡ� vs1053�ڲ���0.5k��������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
uint8_t vs1053_ReqNewData(void)
{
	if (VS1053_IS_BUSY())
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: vs1053_PreWriteData
*	����˵��: ׼����vs1053д���ݣ�����1�μ���
*	��    ��: _��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void vs1053_PreWriteData(void)
{
	VS1053_CS_1();
	VS1053_DS_0();
}

/*
*********************************************************************************************************
*	�� �� ��: vs1053_WriteData
*	����˵��: ��vs1053д����
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void vs1053_WriteData(uint8_t _ucData)
{
	vs1053_SetDS(0);
	bsp_spiWrite0(_ucData);
	vs1053_SetDS(1);
}

/*
*********************************************************************************************************
*	�� �� ��: vs1053_ReadReg
*	����˵��: ��vs1053�ļĴ���
*	��    ��: _ucAddr:�Ĵ�����ַ
*	�� �� ֵ: �Ĵ���ֵ
*********************************************************************************************************
*/
uint16_t vs1053_ReadReg(uint8_t _ucAddr)
{
	uint16_t usTemp;

	/* �ȴ�оƬ�ڲ�������� */
	if (vs1053_WaitTimeOut())
	{
		return 0;
	}

	vs1053_SetCS(0);
	
	bsp_spiWrite0(VS_READ_COMMAND);	/* ����vs1053������ */
	bsp_spiWrite0(_ucAddr);			/* ���͵�ַ */
	usTemp = bsp_spiRead0() << 8;	/* ��ȡ���ֽ� */
	usTemp += bsp_spiRead0();		/* ��ȡ���ֽ� */
	
	vs1053_SetCS(1);
	return usTemp;
}

/*
*********************************************************************************************************
*	�� �� ��: vs1053_ReadChipID
*	����˵��: ��vs1053оƬ�İ汾��ID�� ����ʶ����VS1003���� VS1053
*	��    ��: ��
*	�� �� ֵ: 4bit��оƬID��Version
*********************************************************************************************************
*/
uint8_t vs1053_ReadChipID(void)
{
	uint16_t usStatus;
	/* pdf page 40
		SCI STATUS ״̬�Ĵ����� Bit7:4 ��ʾоƬ�İ汾
		0 for VS1001
		1 for VS1011
		2 for VS1002
		3 for VS1003
		4 for VS1053,
		5 for VS1033,
		7 for VS1103.
	*/
	usStatus = vs1053_ReadReg(SCI_STATUS);

	return ((usStatus >> 4) & 0x000F);
}


/*
*********************************************************************************************************
*	�� �� ��: vs1053_WaitBusy
*	����˵��: �ȴ�оƬ�ڲ���������������DREQ���ߵ�״̬ʶ��оƬ�Ƿ�æ���ú�������ָ��������ӳ١�
*	��    ��: ��
*	�� �� ֵ: 0 ��ʾ��ʱ�� 1��ʾ
*********************************************************************************************************
*/
uint8_t vs1053_WaitTimeOut(void)
{
	uint32_t i;

	for (i = 0; i < 4000000; i++)
	{
		if (!VS1053_IS_BUSY())
		{
			break;
		}
	}

	if (i >= 4000000)
	{
		return 1;	/* ��ʱ��Ӧ��Ӳ���쳣 */
	}

	return 0;	/* �������� */
}

void LoadUserPatch(void)
{
	int i = 0;

	while (i < sizeof(plugin) / sizeof(plugin[0]))
	{
		unsigned short addr, n, val;

		addr = plugin[i++];
		n = plugin[i++];
		if (n & 0x8000U)
		{
			/* RLE run, replicate n samples */
			n &= 0x7FFF;
			val = plugin[i++];
			while (n--)
			{
				vs1053_WriteCmd(addr, val);
			}
		}
		else
		{
			/* Copy run, copy n samples */
			while (n--)
			{
				val = plugin[i++];
				vs1053_WriteCmd(addr, val);
			}
		}
	}
	/* �ȴ�оƬ�ڲ�������� */
	if (vs1053_WaitTimeOut())
	{
		return;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: vs1053_TestRam
*	����˵��: ����vs1053B���ڲ�RAM
*	��    ��: ��
*	�� �� ֵ: 1��ʾOK, 0��ʾ����.
*********************************************************************************************************
*/
uint8_t vs1053_TestRam(void)
{
	uint16_t usRegValue;

 	vs1053_WriteCmd(SCI_MODE, 0x0820);	/* ����vs1053�Ĳ���ģʽ */

	/* �ȴ�оƬ�ڲ�������� */
	if (vs1053_WaitTimeOut())
	{
		return 0;
	}

	vs1053_SetDS(0);
	
	bsp_spiWrite0(0x4d);
	bsp_spiWrite0(0xea);
	bsp_spiWrite0(0x6d);
	bsp_spiWrite0(0x54);
	bsp_spiWrite0(0x00);
	bsp_spiWrite0(0x00);
	bsp_spiWrite0(0x00);
	bsp_spiWrite0(0x00);
	
	vs1053_SetDS(1);

	/* �ȴ�оƬ�ڲ�������� */
	if (vs1053_WaitTimeOut())
	{
		return 0;
	}

	usRegValue = vs1053_ReadReg(SCI_HDAT0); /* ����õ���ֵΪ0x807F�������OK */

	if (usRegValue == 0x807F)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: vs1053_TestSine
*	����˵��: ���Ҳ���
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void vs1053_TestSine(void)
{
	/*
		���Ҳ���ͨ�������8�ֽڳ�ʼ����0x53 0xEF 0x6E n 0 0 0 0
		��Ҫ�˳����Ҳ���ģʽ�Ļ��������������� 0x45 0x78 0x69 0x74 0 0 0 0 .

		�����n������Ϊ���Ҳ���ʹ�ã�����
		���£�
		n bits
		����λ ����
		FsIdx 7��5 ����������
		S 4��0 ���������ٶ�
		�������Ƶ�ʿ�ͨ�������ʽ���㣺F=Fs��(S/128).
		���磺���Ҳ���ֵΪ126 ʱ�����������Ϊ
		0b01111110����FsIdx=0b011=3,����Fs=22050Hz��
		S=0b11110=30, �������յ��������Ƶ��Ϊ
		F=22050Hz��30/128=5168Hz��


		�������Ƶ�ʿ�ͨ�������ʽ���㣺F = Fs��(S/128).
	*/

	vs1053_WriteCmd(0x0b,0x2020);	  	/* ��������	*/
 	vs1053_WriteCmd(SCI_MODE, 0x0820);	/* ����vs1053�Ĳ���ģʽ	*/

 	/* �ȴ�оƬ�ڲ�������� */
	if (vs1053_WaitTimeOut())
	{
		return;
	}

 	/*
 		�������Ҳ���״̬
 		�������У�0x53 0xef 0x6e n 0x00 0x00 0x00 0x00
 		����n = 0x24, �趨vs1053�����������Ҳ���Ƶ��ֵ
 	*/
	vs1053_SetDS(0);
	bsp_spiWrite0(0x53);
	bsp_spiWrite0(0xef);
	bsp_spiWrite0(0x6e);
	bsp_spiWrite0(0x24);	/* 0x24 or 0x44 */
	bsp_spiWrite0(0x00);
	bsp_spiWrite0(0x00);
	bsp_spiWrite0(0x00);
	bsp_spiWrite0(0x00);
	vs1053_SetDS(1);

	/* �˳����Ҳ��� */
    vs1053_SetDS(0);
	bsp_spiWrite0(0x45);
	bsp_spiWrite0(0x78);
	bsp_spiWrite0(0x69);
	bsp_spiWrite0(0x74);
	bsp_spiWrite0(0x00);
	bsp_spiWrite0(0x00);
	bsp_spiWrite0(0x00);
	bsp_spiWrite0(0x00);
	vs1053_SetDS(1);
}

/*
*********************************************************************************************************
*	�� �� ��: vs1053_SoftReset
*	����˵��: ��λvs1053�� �ڸ���֮����Ҫִ�б�������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void vs1053_SoftReset(void)
{
	uint8_t retry;

	/* �ȴ�оƬ�ڲ�������� */
	if (vs1053_WaitTimeOut())
	{
		return;
	}

	bsp_spiWrite0(0X00);//��������
	retry = 0;
	while(vs1053_ReadReg(SCI_MODE) != 0x0804) // �����λ,��ģʽ
	{
		/* �ȴ�����1.35ms  */
		vs1053_WriteCmd(SCI_MODE, 0x0804);// �����λ,��ģʽ

		/* �ȴ�оƬ�ڲ�������� */
		if (vs1053_WaitTimeOut())
		{
			return;
		}

		if (retry++>5)
		{
			break;
		}
	}

#if 0
	vs1053_WriteCmd(SCI_CLOCKF,0x9800);
	vs1053_WriteCmd(SCI_AUDATA,0xBB81); /* ������48k�������� */

	vs1053_WriteCmd(SCI_BASS, 0x0000);	/* */
    vs1053_WriteCmd(SCI_VOL, 0x2020); 	/* ����Ϊ�������,0�����  */

	ResetDecodeTime();	/* ��λ����ʱ��	*/

    /* ��vs1053����4���ֽ���Ч���ݣ���������SPI���� */
    VS1053_DS_0();//ѡ�����ݴ���
	vs1053_WriteByte(0xFF);
	vs1053_WriteByte(0xFF);
	vs1053_WriteByte(0xFF);
	vs1053_WriteByte(0xFF);
	VS1053_DS_1();//ȡ�����ݴ���
#else
	/* Set clock register, doubler etc. */
	vs1053_WriteCmd(SCI_CLOCKF, 0xA000);

	//vs1053_WriteCmd(SCI_BASS, 0x0000);	/* ����������ǿ���ƣ� 0��ʾ������ */
    //vs1053_WriteCmd(SCI_VOL, 0x2020); 	/* ����Ϊ�������,0�����  */

	/* �ȴ�оƬ�ڲ�������� */
	if (vs1053_WaitTimeOut())
	{
		return;
	}
	;
	LoadUserPatch();
#endif
}

/*
*********************************************************************************************************
*	�� �� ��: vs1053_SetVolume
*	����˵��: ����vs1053������0 �Ǿ����� 254���
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void vs1053_SetVolume(uint8_t _ucVol)
{

	/* ���� VS1053�� 0��ʾ���������254��ʾ���� */
	if (_ucVol == 0)
	{
		_ucVol = 254;
	}
	else if (_ucVol == 255)
	{
		_ucVol = 0;
	}
	else
	{
		_ucVol = 254 - _ucVol;
	}

	vs1053_WriteCmd(SCI_VOL, (_ucVol << 8) | _ucVol);
}

/*
*********************************************************************************************************
*	�� �� ��: vs1053_SetBASS
*	����˵��: ���ø�����ǿ�͵�����ǿ
*	��    ��: _cHighAmp     : ������ǿ���� ��-8, 7��  (0�ǹر�)
*			 _usHighFreqCut : ������ǿ��ֹƵ�� ��1000, 15000�� Hz
*			 _ucLowAmp      : ������ǿ���� ��0, 15��  (0�ǹر�)
*			 _usLowFreqCut : ������ǿ��ֹƵ�� ��20, 150�� Hz
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void vs1053_SetBASS(int8_t _cHighAmp, uint16_t _usHighFreqCut, uint8_t _ucLowAmp, uint16_t _usLowFreqCut)
{
	uint16_t usValue;

	/*
		SCI_BASS �Ĵ�������:

		Bit15:12  �������� -8 ... 7  (0�ǹر�)
		Bit11:8   ����Ƶ��,��λ1KHz,  1...15

		Bit7:4    �������� 0...15 (0�ǹر�)
		Bit3:0    ����Ƶ��,��λ10Hz, 2...15
	*/

	/* ������ǿ���� */
	if (_cHighAmp < -8)
	{
		_cHighAmp = -8;
	}
	else if (_cHighAmp > 7)
	{
		_cHighAmp = 7;
	}
	usValue = _cHighAmp << 12;

	/* ������ǿ��ֹƵ�� */
	if (_usHighFreqCut < 1000)
	{
		_usHighFreqCut = 1000;
	}
	else if (_usHighFreqCut > 15000)
	{
		_usHighFreqCut = 15000;
	}
	usValue  += ((_usHighFreqCut / 1000) << 8);

	/* ������ǿ���� */
	if (_ucLowAmp > 15)
	{
		_ucLowAmp = 15;
	}
	usValue  += (_ucLowAmp << 4);

	/* ������ǿ��ֹƵ�� */
	if (_usLowFreqCut < 20)
	{
		_usLowFreqCut = 20;
	}
	else if (_usLowFreqCut > 150)
	{
		_usLowFreqCut = 150;
	}
	usValue  += (_usLowFreqCut / 10);

	vs1053_WriteCmd(SCI_BASS, usValue);
}

/*
*********************************************************************************************************
*	�� �� ��: ResetDecodeTime
*	����˵��: �������ʱ��
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ResetDecodeTime(void)
{
	vs1053_WriteCmd(SCI_DECODE_TIME, 0x0000);
}

/*
*********************************************************************************************************
*	����Ĵ��뻹δ����
*********************************************************************************************************
*/

#if 0

//ram ����
void VsRamTest(void)
{
	uint16_t u16 regvalue ;

	Mp3Reset();
 	vs1053_CMD_Write(SPI_MODE,0x0820);// ����vs1053�Ĳ���ģʽ
	while ((GPIOC->IDR&MP3_DREQ)==0); // �ȴ�DREQΪ��
 	MP3_DCS_SET(0);	       			  // xDCS = 1��ѡ��vs1053�����ݽӿ�
	SPI1_ReadWriteByte(0x4d);
	SPI1_ReadWriteByte(0xea);
	SPI1_ReadWriteByte(0x6d);
	SPI1_ReadWriteByte(0x54);
	SPI1_ReadWriteByte(0x00);
	SPI1_ReadWriteByte(0x00);
	SPI1_ReadWriteByte(0x00);
	SPI1_ReadWriteByte(0x00);
	delay_ms(50);
	MP3_DCS_SET(1);
	regvalue=vs1053_REG_Read(SPI_HDAT0); // ����õ���ֵΪ0x807F���������á�
	printf("regvalueH:%x\n",regvalue>>8);//������
	printf("regvalueL:%x\n",regvalue&0xff);//������
}

//FOR WAV HEAD0 :0X7761 HEAD1:0X7665
//FOR MIDI HEAD0 :other info HEAD1:0X4D54
//FOR WMA HEAD0 :data speed HEAD1:0X574D
//FOR MP3 HEAD0 :data speed HEAD1:ID
//������Ԥ��ֵ
const uint16_t bitrate[2][16]=
{
	{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0},
	{0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0}
};
//����Kbps�Ĵ�С
//�õ�mp3&wma�Ĳ�����
uint16_t GetHeadInfo(void)
{
	unsigned int HEAD0;
	unsigned int HEAD1;

    HEAD0=vs1053_REG_Read(SPI_HDAT0);
    HEAD1=vs1053_REG_Read(SPI_HDAT1);
    switch(HEAD1)
    {
        case 0x7665:return 0;//WAV��ʽ
        case 0X4D54:return 1;//MIDI��ʽ
        case 0X574D://WMA��ʽ
        {
            HEAD1=HEAD0*2/25;
            if((HEAD1%10)>5)return HEAD1/10+1;
            else return HEAD1/10;
        }
        default://MP3��ʽ
        {
            HEAD1>>=3;
            HEAD1=HEAD1&0x03;
            if(HEAD1==3)HEAD1=1;
            else HEAD1=0;
            return bitrate[HEAD1][HEAD0>>12];
        }
    }
}

//�õ�mp3�Ĳ���ʱ��n sec
uint16_t GetDecodeTime(void)
{
    return vs1053_REG_Read(SPI_DECODE_TIME);
}
//����Ƶ�׷����Ĵ��뵽vs1053
void LoadPatch(void)
{
	uint16_t i;

	for (i=0;i<943;i++)vs1053_CMD_Write(atab[i],dtab[i]);
	delay_ms(10);
}
//�õ�Ƶ������
void GetSpec(u8 *p)
{
	u8 byteIndex=0;
	u8 temp;
	vs1053_CMD_Write(SPI_WRAMADDR,0x1804);
	for (byteIndex=0;byteIndex<14;byteIndex++)
	{
		temp=vs1053_REG_Read(SPI_WRAM)&0x63;//ȡС��100����
		*p++=temp;
	}
}

//�趨vs1053���ŵ������͸ߵ���
void set1003(void)
{
    uint8 t;
    uint16_t bass=0; //�ݴ������Ĵ���ֵ
    uint16_t volt=0; //�ݴ�����ֵ
    uint8_t vset=0;  //�ݴ�����ֵ

    vset=255-vs1053ram[4];//ȡ��һ��,�õ����ֵ,��ʾ���ı�ʾ
    volt=vset;
    volt<<=8;
    volt+=vset;//�õ��������ú��С
     //0,henh.1,hfreq.2,lenh.3,lfreq
    for(t=0;t<4;t++)
    {
        bass<<=4;
        bass+=vs1053ram[t];
    }
	vs1053_CMD_Write(SPI_BASS, 0x0000);//BASS
    vs1053_CMD_Write(SPI_VOL, 0x0000); //������
}

#endif
