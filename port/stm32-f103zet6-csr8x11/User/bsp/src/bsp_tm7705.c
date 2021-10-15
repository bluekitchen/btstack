/*
*********************************************************************************************************
*
*	ģ������ : TM7705 ����ģ��(2ͨ����PGA��16λADC)
*	�ļ����� : bsp_tm7705.c
*	��    �� : V1.0
*	˵    �� : TM7705ģ���CPU֮�����SPI�ӿڡ�����������֧��Ӳ��SPI�ӿں����SPI�ӿڡ�
*			  ͨ�����л���
*
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2013-10-20  armfly  ��ʽ����
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/* ͨ��1��ͨ��2������,���뻺�壬���� */
#define __CH1_GAIN_BIPOLAR_BUF	(GAIN_1 | UNIPOLAR | BUF_EN)
#define __CH2_GAIN_BIPOLAR_BUF	(GAIN_1 | UNIPOLAR | BUF_EN)

/*
	TM7705ģ�����ֱ�Ӳ嵽STM32-V4������nRF24L01ģ�����ĸ�ӿ��ϡ�

    TM7705ģ��   STM32-V4������
      SCK   ------  PA5/SPI1_SCK
      DOUT  ------  PA6/SPI1_MISO
      DIN   ------  PA7/SPI1_MOSI
      CS    ------  PC3/ADC123_IN13/NRF24L01_CSN
      DRDY  ------  PB0/MPU-6050_INT/NRF24L01_IRQ
      RST   ------  PB3/TDO/NRF24L01_CE	(��λ RESET)

*/

/* SPI���ߵ�SCK��MOSI��MISO �� bsp_spi_bus.c������  */

/* TM7705 */
#define TM7705_RCC_CS 		RCC_APB2Periph_GPIOC
#define TM7705_PORT_CS		GPIOC
#define TM7705_PIN_CS		GPIO_Pin_3

#define TM7705_CS_1()		TM7705_PORT_CS->BSRR = TM7705_PIN_CS
#define TM7705_CS_0()		TM7705_PORT_CS->BRR = TM7705_PIN_CS

/* RESET */
#define RCC_RESET 	RCC_APB2Periph_GPIOB
#define PORT_RESET	GPIOB
#define PIN_RESET	GPIO_Pin_3

#define RESET_0()	GPIO_ResetBits(PORT_RESET, PIN_RESET)
#define RESET_1()	GPIO_SetBits(PORT_RESET, PIN_RESET)

/* DRDY */
#define RCC_DRDY 	RCC_APB2Periph_GPIOB
#define PORT_DRDY	GPIOB
#define PIN_DRDY	GPIO_Pin_0

#define DRDY_IS_LOW()	(GPIO_ReadInputDataBit(PORT_DRDY, PIN_DRDY) == Bit_RESET)

/* ͨ�żĴ���bit���� */
enum
{
	/* �Ĵ���ѡ��  RS2 RS1 RS0  */
	REG_COMM	= 0x00,	/* ͨ�żĴ��� */
	REG_SETUP	= 0x10,	/* ���üĴ��� */
	REG_CLOCK	= 0x20,	/* ʱ�ӼĴ��� */
	REG_DATA	= 0x30,	/* ���ݼĴ��� */
	REG_ZERO_CH1	= 0x60,	/* CH1 ƫ�ƼĴ��� */
	REG_FULL_CH1	= 0x70,	/* CH1 �����̼Ĵ��� */
	REG_ZERO_CH2	= 0x61,	/* CH2 ƫ�ƼĴ��� */
	REG_FULL_CH2	= 0x71,	/* CH2 �����̼Ĵ��� */

	/* ��д���� */
	WRITE 		= 0x00,	/* д���� */
	READ 		= 0x08,	/* ������ */

	/* ͨ�� */
	CH_1		= 0,	/* AIN1+  AIN1- */
	CH_2		= 1,	/* AIN2+  AIN2- */
	CH_3		= 2,	/* AIN1-  AIN1- */
	CH_4		= 3		/* AIN1-  AIN2- */
};

/* ���üĴ���bit���� */
enum
{
	MD_NORMAL		= (0 << 6),	/* ����ģʽ */
	MD_CAL_SELF		= (1 << 6),	/* ��У׼ģʽ */
	MD_CAL_ZERO		= (2 << 6),	/* У׼0�̶�ģʽ */
	MD_CAL_FULL		= (3 << 6),	/* У׼���̶�ģʽ */

	GAIN_1			= (0 << 3),	/* ���� */
	GAIN_2			= (1 << 3),	/* ���� */
	GAIN_4			= (2 << 3),	/* ���� */
	GAIN_8			= (3 << 3),	/* ���� */
	GAIN_16			= (4 << 3),	/* ���� */
	GAIN_32			= (5 << 3),	/* ���� */
	GAIN_64			= (6 << 3),	/* ���� */
	GAIN_128		= (7 << 3),	/* ���� */

	/* ����˫���Ի��ǵ����Զ����ı��κ������źŵ�״̬����ֻ�ı�������ݵĴ����ת�������ϵ�У׼�� */
	BIPOLAR			= (0 << 2),	/* ˫�������� */
	UNIPOLAR		= (1 << 2),	/* ���������� */

	BUF_NO			= (0 << 1),	/* �����޻��壨�ڲ�������������) */
	BUF_EN			= (1 << 1),	/* �����л��� (�����ڲ�������) */

	FSYNC_0			= 0,
	FSYNC_1			= 1		/* ������ */
};

/* ʱ�ӼĴ���bit���� */
enum
{
	CLKDIS_0	= 0x00,		/* ʱ�����ʹ�� ������Ӿ���ʱ������ʹ�ܲ����񵴣� */
	CLKDIS_1	= 0x10,		/* ʱ�ӽ�ֹ �����ⲿ�ṩʱ��ʱ�����ø�λ���Խ�ֹMCK_OUT�������ʱ����ʡ�� */

	/*
		2.4576MHz��CLKDIV=0 ����Ϊ 4.9152MHz ��CLKDIV=1 ����CLK Ӧ�� ��0����
		1MHz ��CLKDIV=0 ���� 2MHz   ��CLKDIV=1 ����CLK ��λӦ��  ��1��
	*/
	CLK_4_9152M = 0x08,
	CLK_2_4576M = 0x00,
	CLK_1M 		= 0x04,
	CLK_2M 		= 0x0C,

	FS_50HZ		= 0x00,
	FS_60HZ		= 0x01,
	FS_250HZ	= 0x02,
	FS_500HZ	= 0x04,

	/*
		��ʮ�š����ӳ�Ӧ�������TM7705 ���ȵķ���
			��ʹ����ʱ��Ϊ 2.4576MHz ʱ��ǿ�ҽ��齫ʱ�ӼĴ�����Ϊ 84H,��ʱ�������������Ϊ10Hz,��ÿ0.1S ���һ�������ݡ�
			��ʹ����ʱ��Ϊ 1MHz ʱ��ǿ�ҽ��齫ʱ�ӼĴ�����Ϊ80H, ��ʱ�������������Ϊ4Hz, ��ÿ0.25S ���һ��������
	*/
	ZERO_0		= 0x00,
	ZERO_1		= 0x80
};

static void TM7705_SyncSPI(void);
static void TM7705_WriteByte(uint8_t _data);
static void TM7705_Write3Byte(uint32_t _data);
static uint8_t TM7705_ReadByte(void);
static uint16_t TM7705_Read2Byte(void);
static uint32_t TM7705_Read3Byte(void);
static void TM7705_WaitDRDY(void);
static void TM7705_ResetHard(void);

static void TM7705_ConfigGPIO(void);
void TM7705_CfgSpiHard(void);

uint8_t g_TM7705_OK = 0;	/* ȫ�ֱ�־����ʾTM7705оƬ�Ƿ���������  */

uint16_t g_TM7705_Adc1;
uint16_t g_TM7705_Adc2;

/*
*********************************************************************************************************
*	�� �� ��: TM7705_ConfigGPIO
*	����˵��: ����GPIO�� ������ SCK  MOSI  MISO �����SPI���ߡ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void TM7705_ConfigGPIO(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* ��GPIOʱ�� */
	RCC_APB2PeriphClockCmd(RCC_DRDY | RCC_RESET | TM7705_RCC_CS, ENABLE);

	/* ���ü����������IO */
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	/* �������ģʽ */

	GPIO_InitStructure.GPIO_Pin = PIN_RESET;
	GPIO_Init(PORT_RESET, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = TM7705_PIN_CS;
	GPIO_Init(TM7705_PORT_CS, &GPIO_InitStructure);

	/* ����GPIOΪ��������ģʽ(ʵ����CPU��λ���������״̬) */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;	/* ���븡��ģʽ */
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	/* IO������ٶ� */

	GPIO_InitStructure.GPIO_Pin = PIN_DRDY;
	GPIO_Init(PORT_DRDY, &GPIO_InitStructure);
}

/*
*********************************************************************************************************
*	�� �� ��: TM7705_CfgSpiHard
*	����˵��: ����STM32�ڲ�SPIӲ���Ĺ���ģʽ���ٶȵȲ��������ڷ���TM7705
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TM7705_CfgSpiHard(void)
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

	/* ���ò�����Ԥ��Ƶϵ�� SPI_BaudRatePrescaler_64 ʵ��SCK���� 800ns ��12.5MHz */
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_64;

	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;	/* ����λ������򣺸�λ�ȴ� */
	SPI_InitStructure.SPI_CRCPolynomial = 7;			/* CRC����ʽ�Ĵ�������λ��Ϊ7�������̲��� */
	SPI_Init(SPI1, &SPI_InitStructure);

	SPI_Cmd(SPI1, ENABLE);				/* ʹ��SPI  */
}

/*
*********************************************************************************************************
*	�� �� ��: TM7705_SetCS(0)
*	����˵��: ����CS�� ����������SPI����
*	��    ��: ��
	�� �� ֵ: ��
*********************************************************************************************************
*/
void TM7705_SetCS(uint8_t _level)
{
	if (_level == 0)
	{
		bsp_SpiBusEnter();	/* ռ��SPI���ߣ� �������߹��� */

		#ifdef SOFT_SPI		/* ���SPI */
			bsp_SetSpiSck(1);
			TM7705_CS_0();
		#endif

		#ifdef HARD_SPI		/* Ӳ��SPI */
			bsp_SPI_Init(SPI_Direction_2Lines_FullDuplex | SPI_Mode_Master | SPI_DataSize_8b
				| SPI_CPOL_High | SPI_CPHA_2Edge | SPI_NSS_Soft | SPI_BaudRatePrescaler_64 | SPI_FirstBit_MSB);

			TM7705_CS_0();
		#endif
	}
	else
	{
		TM7705_CS_1();

		bsp_SpiBusExit();	/* �ͷ�SPI���ߣ� �������߹��� */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: uint8_t
*	����˵��: ����STM32��GPIO��SPI�ӿڣ��������� TM7705
*	��    ��: ��
*	�� �� ֵ: 0 ��ʾʧ��; 1 ��ʾ�ɹ�
*********************************************************************************************************
*/
void bsp_InitTM7705(void)
{
	TM7705_ConfigGPIO();
	//TM7705_CfgSpiHard();

	bsp_DelayMS(10);

	TM7705_ResetHard();		/* Ӳ����λ */

	/*
		�ڽӿ����ж�ʧ������£������DIN �ߵ�ƽ��д�����������㹻����ʱ�䣨���� 32������ʱ�����ڣ���
		TM7705 ����ص�Ĭ��״̬��
	*/
	bsp_DelayMS(5);

	TM7705_SyncSPI();		/* ͬ��SPI�ӿ�ʱ�� */

	bsp_DelayMS(5);

	/* ��λ֮��, ʱ�ӼĴ���Ӧ���� 0x05 �ٽ���ʱ 08*/
	{
		uint8_t reg;
		
		reg = TM7705_ReadReg(REG_CLOCK);
		if ((reg == 0x05) || (reg == 0x08))
		{
			g_TM7705_OK = 1;
		}
		else
		{
			g_TM7705_OK = 0;
		}
	}

	/* ����ʱ�ӼĴ��� */
	TM7705_WriteByte(REG_CLOCK | WRITE | CH_1);			/* ��дͨ�żĴ�������һ����дʱ�ӼĴ��� */

	TM7705_WriteByte(CLKDIS_0 | CLK_4_9152M | FS_50HZ);	/* ˢ������50Hz */
	//TM7705_WriteByte(CLKDIS_0 | CLK_4_9152M | FS_500HZ);	/* ˢ������500Hz */

	/* ÿ���ϵ����һ����У׼ */
	TM7705_CalibSelf(1);	/* �ڲ���У׼ CH1 */
	bsp_DelayMS(5);
}

/*
*********************************************************************************************************
*	�� �� ��: TM7705_ResetHard
*	����˵��: Ӳ����λ TM7705оƬ
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void TM7705_ResetHard(void)
{
	RESET_1();
	bsp_DelayMS(1);
	RESET_0();
	bsp_DelayMS(2);
	RESET_1();
	bsp_DelayMS(1);
}

/*
*********************************************************************************************************
*	�� �� ��: TM7705_SyncSPI
*	����˵��: ͬ��TM7705оƬSPI�ӿ�ʱ��
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void TM7705_SyncSPI(void)
{
	/* AD7705���нӿ�ʧ�����临λ����λ��Ҫ��ʱ500us�ٷ��� */
	TM7705_SetCS(0);
	bsp_spiWrite1(0xFF);
	bsp_spiWrite1(0xFF);
	bsp_spiWrite1(0xFF);
	bsp_spiWrite1(0xFF);
	TM7705_SetCS(1);
}

/*
*********************************************************************************************************
*	�� �� ��: TM7705_WriteByte
*	����˵��: д��1���ֽڡ���CS����
*	��    ��: _data ����Ҫд�������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void TM7705_WriteByte(uint8_t _data)
{
	TM7705_SetCS(0);
	bsp_spiWrite1(_data);
	TM7705_SetCS(1);
}

/*
*********************************************************************************************************
*	�� �� ��: TM7705_Write3Byte
*	����˵��: д��3���ֽڡ���CS����
*	��    ��: _data ����Ҫд�������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void TM7705_Write3Byte(uint32_t _data)
{
	TM7705_SetCS(0);
	bsp_spiWrite1((_data >> 16) & 0xFF);
	bsp_spiWrite1((_data >> 8) & 0xFF);
	bsp_spiWrite1(_data);
	TM7705_SetCS(1);
}

/*
*********************************************************************************************************
*	�� �� ��: TM7705_ReadByte
*	����˵��: ��ADоƬ��ȡһ���֣�16λ��
*	��    ��: ��
*	�� �� ֵ: ��ȡ���֣�16λ��
*********************************************************************************************************
*/
static uint8_t TM7705_ReadByte(void)
{
	uint8_t read;

	TM7705_SetCS(0);
	read = bsp_spiRead1();
	TM7705_SetCS(1);

	return read;
}

/*
*********************************************************************************************************
*	�� �� ��: TM7705_Read2Byte
*	����˵��: ��2�ֽ�����
*	��    ��: ��
*	�� �� ֵ: ��ȡ�����ݣ�16λ��
*********************************************************************************************************
*/
static uint16_t TM7705_Read2Byte(void)
{
	uint16_t read;

	TM7705_SetCS(0);
	read = bsp_spiRead1();
	read <<= 8;
	read += bsp_spiRead1();
	TM7705_SetCS(1);

	return read;
}

/*
*********************************************************************************************************
*	�� �� ��: TM7705_Read3Byte
*	����˵��: ��3�ֽ�����
*	��    ��: ��
*	�� �� ֵ: ��ȡ�������ݣ�24bit) ��8λ�̶�Ϊ0.
*********************************************************************************************************
*/
static uint32_t TM7705_Read3Byte(void)
{
	uint32_t read;

	TM7705_SetCS(0);
	read = bsp_spiRead1();
	read <<= 8;
	read += bsp_spiRead1();
	read <<= 8;
	read += bsp_spiRead1();
	TM7705_SetCS(1);
	return read;
}

/*
*********************************************************************************************************
*	�� �� ��: TM7705_WaitDRDY
*	����˵��: �ȴ��ڲ�������ɡ� ��У׼ʱ��ϳ�����Ҫ�ȴ���
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void TM7705_WaitDRDY(void)
{
	uint32_t i;

	/* �����ʼ��ʱδ��⵽оƬ����ٷ��أ�����Ӱ����������Ӧ�ٶ� */
	if (g_TM7705_OK == 0)
	{
		return;
	}

	for (i = 0; i < 4000000; i++)
	{
		if (DRDY_IS_LOW())
		{
			break;
		}
	}
	if (i >= 4000000)
	{
		printf("TM7705_WaitDRDY() Time Out ...\r\n");		/* �������. �����Ŵ� */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TM7705_WriteReg
*	����˵��: дָ���ļĴ���
*	��    ��:  _RegID : �Ĵ���ID
*			  _RegValue : �Ĵ���ֵ�� ����8λ�ļĴ�����ȡ32λ�βεĵ�8bit
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TM7705_WriteReg(uint8_t _RegID, uint32_t _RegValue)
{
	uint8_t bits;

	switch (_RegID)
	{
		case REG_COMM:		/* ͨ�żĴ��� */
		case REG_SETUP:		/* ���üĴ��� 8bit */
		case REG_CLOCK:		/* ʱ�ӼĴ��� 8bit */
			bits = 8;
			break;

		case REG_ZERO_CH1:	/* CH1 ƫ�ƼĴ��� 24bit */
		case REG_FULL_CH1:	/* CH1 �����̼Ĵ��� 24bit */
		case REG_ZERO_CH2:	/* CH2 ƫ�ƼĴ��� 24bit */
		case REG_FULL_CH2:	/* CH2 �����̼Ĵ��� 24bit*/
			bits = 24;
			break;

		case REG_DATA:		/* ���ݼĴ��� 16bit */
		default:
			return;
	}

	TM7705_WriteByte(_RegID | WRITE);	/* дͨ�żĴ���, ָ����һ����д��������ָ��д�ĸ��Ĵ��� */

	if (bits == 8)
	{
		TM7705_WriteByte((uint8_t)_RegValue);
	}
	else	/* 24bit */
	{
		TM7705_Write3Byte(_RegValue);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TM7705_ReadReg
*	����˵��: дָ���ļĴ���
*	��    ��:  _RegID : �Ĵ���ID
*			  _RegValue : �Ĵ���ֵ�� ����8λ�ļĴ�����ȡ32λ�βεĵ�8bit
*	�� �� ֵ: �����ļĴ���ֵ�� ����8λ�ļĴ�����ȡ32λ�βεĵ�8bit
*********************************************************************************************************
*/
uint32_t TM7705_ReadReg(uint8_t _RegID)
{
	uint8_t bits;
	uint32_t read;

	switch (_RegID)
	{
		case REG_COMM:		/* ͨ�żĴ��� */
		case REG_SETUP:		/* ���üĴ��� 8bit */
		case REG_CLOCK:		/* ʱ�ӼĴ��� 8bit */
			bits = 8;
			break;

		case REG_ZERO_CH1:	/* CH1 ƫ�ƼĴ��� 24bit */
		case REG_FULL_CH1:	/* CH1 �����̼Ĵ��� 24bit */
		case REG_ZERO_CH2:	/* CH2 ƫ�ƼĴ��� 24bit */
		case REG_FULL_CH2:	/* CH2 �����̼Ĵ��� 24bit*/
			bits = 24;
			break;

		case REG_DATA:		/* ���ݼĴ��� 16bit */
		default:
			return 0xFFFFFFFF;
	}

	TM7705_WriteByte(_RegID | READ);	/* дͨ�żĴ���, ָ����һ����д��������ָ��д�ĸ��Ĵ��� */

	if (bits == 16)
	{
		read = TM7705_Read2Byte();
	}
	else if (bits == 8)
	{
		read = TM7705_ReadByte();
	}
	else	/* 24bit */
	{
		read = TM7705_Read3Byte();
	}
	return read;
}

/*
*********************************************************************************************************
*	�� �� ��: TM7705_CalibSelf
*	����˵��: ������У׼. �ڲ��Զ��̽�AIN+ AIN-У׼0λ���ڲ��̽ӵ�Vref У׼��λ���˺���ִ�й��̽ϳ���
*			  ʵ��Լ 180ms
*	��    ��:  _ch : ADCͨ����1��2
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TM7705_CalibSelf(uint8_t _ch)
{
	if (_ch == 1)
	{
		/* ��У׼CH1 */
		TM7705_WriteByte(REG_SETUP | WRITE | CH_1);	/* дͨ�żĴ�������һ����д���üĴ�����ͨ��1 */
		TM7705_WriteByte(MD_CAL_SELF | __CH1_GAIN_BIPOLAR_BUF | FSYNC_0);/* ������У׼ */
		TM7705_WaitDRDY();	/* �ȴ��ڲ�������� --- ʱ��ϳ���Լ180ms */
	}
	else if (_ch == 2)
	{
		/* ��У׼CH2 */
		TM7705_WriteByte(REG_SETUP | WRITE | CH_2);	/* дͨ�żĴ�������һ����д���üĴ�����ͨ��2 */
		TM7705_WriteByte(MD_CAL_SELF | __CH2_GAIN_BIPOLAR_BUF | FSYNC_0);	/* ������У׼ */
		TM7705_WaitDRDY();	/* �ȴ��ڲ��������  --- ʱ��ϳ���Լ180ms */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TM7705_SytemCalibZero
*	����˵��: ����ϵͳУ׼��λ. �뽫AIN+ AIN-�̽Ӻ�ִ�иú�����У׼Ӧ������������Ʋ�����У׼������
*			 ִ����Ϻ󡣿���ͨ�� TM7705_ReadReg(REG_ZERO_CH1) ��  TM7705_ReadReg(REG_ZERO_CH2) ��ȡУ׼������
*	��    ��: _ch : ADCͨ����1��2
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TM7705_SytemCalibZero(uint8_t _ch)
{
	if (_ch == 1)
	{
		/* У׼CH1 */
		TM7705_WriteByte(REG_SETUP | WRITE | CH_1);	/* дͨ�żĴ�������һ����д���üĴ�����ͨ��1 */
		TM7705_WriteByte(MD_CAL_ZERO | __CH1_GAIN_BIPOLAR_BUF | FSYNC_0);/* ������У׼ */
		TM7705_WaitDRDY();	/* �ȴ��ڲ�������� */
	}
	else if (_ch == 2)
	{
		/* У׼CH2 */
		TM7705_WriteByte(REG_SETUP | WRITE | CH_2);	/* дͨ�żĴ�������һ����д���üĴ�����ͨ��1 */
		TM7705_WriteByte(MD_CAL_ZERO | __CH2_GAIN_BIPOLAR_BUF | FSYNC_0);	/* ������У׼ */
		TM7705_WaitDRDY();	/* �ȴ��ڲ�������� */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TM7705_SytemCalibFull
*	����˵��: ����ϵͳУ׼��λ. �뽫AIN+ AIN-����������ѹԴ��ִ�иú�����У׼Ӧ������������Ʋ�����У׼������
*			 ִ����Ϻ󡣿���ͨ�� TM7705_ReadReg(REG_FULL_CH1) ��  TM7705_ReadReg(REG_FULL_CH2) ��ȡУ׼������
*	��    ��:  _ch : ADCͨ����1��2
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TM7705_SytemCalibFull(uint8_t _ch)
{
	if (_ch == 1)
	{
		/* У׼CH1 */
		TM7705_WriteByte(REG_SETUP | WRITE | CH_1);	/* дͨ�żĴ�������һ����д���üĴ�����ͨ��1 */
		TM7705_WriteByte(MD_CAL_FULL | __CH1_GAIN_BIPOLAR_BUF | FSYNC_0);/* ������У׼ */
		TM7705_WaitDRDY();	/* �ȴ��ڲ�������� */
	}
	else if (_ch == 2)
	{
		/* У׼CH2 */
		TM7705_WriteByte(REG_SETUP | WRITE | CH_2);	/* дͨ�żĴ�������һ����д���üĴ�����ͨ��1 */
		TM7705_WriteByte(MD_CAL_FULL | __CH2_GAIN_BIPOLAR_BUF | FSYNC_0);	/* ������У׼ */
		TM7705_WaitDRDY();	/* �ȴ��ڲ�������� */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TM7705_ReadAdc1
*	����˵��: ��ͨ��1��2��ADC���ݡ� �ú���ִ�н��������ǿ��Զ�����ȷ�����
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
uint16_t TM7705_ReadAdc(uint8_t _ch)
{
	uint8_t i;
	uint16_t read = 0;

	/* Ϊ�˱���ͨ���л���ɶ���ʧЧ����2�� */
	for (i = 0; i < 2; i++)
	{
		TM7705_WaitDRDY();		/* �ȴ�DRDY����Ϊ0 */

		if (_ch == 1)
		{
			TM7705_WriteByte(0x38);
		}
		else if (_ch == 2)
		{
			TM7705_WriteByte(0x39);
		}

		read = TM7705_Read2Byte();
	}
	return read;
}


/*
*********************************************************************************************************
*	�� �� ��: TM7705_Scan2
*	����˵��: ɨ��2��ͨ�������뵽������ѭ��������ʱ�ɼ�ADCֵ�ŵ�ȫ�ֱ����������ȡ��
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TM7705_Scan2(void)
{
	static uint8_t s_ch = 1;

	/* �����ʼ��ʱδ��⵽оƬ����ٷ��أ�����Ӱ����������Ӧ�ٶ� */
	if (g_TM7705_OK == 0)
	{
		return;
	}

	if (DRDY_IS_LOW())
	{
		/* ˫ͨ������� */
		if (s_ch == 1)
		{
			TM7705_WriteByte(0x38);
			g_TM7705_Adc2 = TM7705_Read2Byte();

			s_ch = 2;
		}
		else
		{
			TM7705_WriteByte(0x39);
			g_TM7705_Adc1 = TM7705_Read2Byte();

			s_ch = 1;
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TM7705_Scan1
*	����˵��: ɨ��ͨ��1�����뵽������ѭ��������ʱ�ɼ�ADCֵ�ŵ�ȫ�ֱ����������ȡ��
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TM7705_Scan1(void)
{
	/* �����ʼ��ʱδ��⵽оƬ����ٷ��أ�����Ӱ����������Ӧ�ٶ� */
	if (g_TM7705_OK == 0)
	{
		return;
	}

	if (DRDY_IS_LOW())
	{
		TM7705_WriteByte(0x38);
		g_TM7705_Adc1 = TM7705_Read2Byte();
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TM7705_GetAdc1
*	����˵��: ��ȡɨ��Ľ������Ҫ��� TM7705_Scan2() �� TM7705_Scan1() ʹ��
*	��    ��: ��
*	�� �� ֵ: ͨ��1��ADCֵ
*********************************************************************************************************
*/
uint16_t TM7705_GetAdc1(void)
{
	/* �����ʼ��ʱδ��⵽оƬ����ٷ��أ�����Ӱ����������Ӧ�ٶ� */
	if (g_TM7705_OK == 0)
	{
		return 0;
	}

	return g_TM7705_Adc1;
}

/*
*********************************************************************************************************
*	�� �� ��: TM7705_GetAdc2
*	����˵��: ��ȡɨ��Ľ����
*	��    ��: ��
*	�� �� ֵ: ͨ��2��ADCֵ
*********************************************************************************************************
*/
uint16_t TM7705_GetAdc2(void)
{
	/* �����ʼ��ʱδ��⵽оƬ����ٷ��أ�����Ӱ����������Ӧ�ٶ� */
	if (g_TM7705_OK == 0)
	{
		return 0;
	}

	return g_TM7705_Adc2;
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
