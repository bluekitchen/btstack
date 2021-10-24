/*
*********************************************************************************************************
*
*	ģ������ : DAC8501 ����ģ��(��ͨ����16λDAC)
*	�ļ����� : bsp_dac8501.c
*	��    �� : V1.0
*	˵    �� : DAC8501ģ���CPU֮�����SPI�ӿڡ�����������֧��Ӳ��SPI�ӿں����SPI�ӿڡ�
*			  ͨ�����л���
*
*	�޸ļ�¼ :
*		�汾��  ����         ����     ˵��
*		V1.0    2014-01-17  armfly  ��ʽ����
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/*
	DAC8501ģ�����ֱ�Ӳ嵽STM32-V4������CN29��ĸ(2*4P 2.54mm)�ӿ���

    DAC8501ģ��    ������
	  VCC   ------  3.3V
	  GND   ------  GND
      SCLK  ------  PA5/SPI1_SCK
      MOSI  ------  PA6/SPI1_MOSI
      CS1   ------  PC3/NRF24L01_CSN
	  CS2   ------  PB3/NRF24L01_CE
			------  PA7/SPI1_MISO
			------  PB0/NRF24L01_IRQ

*/

/*
	DAC8501��������:
	1������2.7 - 5V;  ������ʹ��3.3V��
	4���ο���ѹ2.5V (�Ƽ�ȱʡ�ģ����õģ�

	��SPI��ʱ���ٶ�Ҫ��: �ߴ�30MHz�� �ٶȺܿ�.
	SCLK�½��ض�ȡ����, ÿ�δ���24bit���ݣ� ��λ�ȴ�
*/

#define DAC8501_RCC_CS1 	RCC_APB2Periph_GPIOC
#define DAC8501_PORT_CS1	GPIOC
#define DAC8501_PIN_CS1		GPIO_Pin_3
#define DAC8501_CS1_1()		DAC8501_PORT_CS1->BSRR = DAC8501_PIN_CS1
#define DAC8501_CS1_0()		DAC8501_PORT_CS1->BRR = DAC8501_PIN_CS1

#define DAC8501_RCC_CS2 	RCC_APB2Periph_GPIOB
#define DAC8501_PORT_CS2	GPIOB
#define DAC8501_PIN_CS2		GPIO_Pin_3
#define DAC8501_CS2_1()		DAC8501_PORT_CS2->BSRR = DAC8501_PIN_CS2
#define DAC8501_CS2_0()		DAC8501_PORT_CS2->BRR = DAC8501_PIN_CS2

/* �����ѹ��DACֵ��Ĺ�ϵ�� ����У׼ x��dac y �ǵ�ѹ 0.1mV */
#define X1	100
#define Y1  50

#define X2	65000
#define Y2  49400

static void DAC8501_ConfigGPIO(void);

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitDAC8501
*	����˵��: ����STM32��GPIO��SPI�ӿڣ��������� ADS1256
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitDAC8501(void)
{
	DAC8501_ConfigGPIO();

	DAC8501_SetDacData(0, 0);
	DAC8501_SetDacData(1, 0);
}

/*
*********************************************************************************************************
*	�� �� ��: DAC8501_CfgSpiHard
*	����˵��: ����STM32�ڲ�SPIӲ���Ĺ���ģʽ���ٶȵȲ��������ڷ���TM7705
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void DAC8501_CfgSpiHard(void)
{
	SPI_InitTypeDef  SPI_InitStructure;

	/* ���� SPI1����ģʽ */
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft; 		/* �������Ƭѡ */

	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
	SPI_InitStructure.SPI_CRCPolynomial = 7;
	SPI_Init(SPI1,&SPI_InitStructure);

	/* ʹ�� SPI1 */
	SPI_Cmd(SPI1,ENABLE);
}

/*
*********************************************************************************************************
*	�� �� ��: DAC8501_SetCS1(0)
*	����˵��: ����CS1�� ����������SPI����
*	��    ��: ��
	�� �� ֵ: ��
*********************************************************************************************************
*/
void DAC8501_SetCS1(uint8_t _level)
{
	if (_level == 0)
	{
		bsp_SpiBusEnter();	/* ռ��SPI���ߣ� �������߹��� */

		#ifdef SOFT_SPI		/* ���SPI */
			bsp_SetSpiSck(0);
			DAC8501_CS1_0();
		#endif

		#ifdef HARD_SPI		/* Ӳ��SPI */
			bsp_SPI_Init(SPI_Direction_2Lines_FullDuplex | SPI_Mode_Master | SPI_DataSize_8b
				| SPI_CPOL_Low | SPI_CPHA_1Edge | SPI_NSS_Soft | SPI_BaudRatePrescaler_8 | SPI_FirstBit_MSB);

			DAC8501_CS1_0();
		#endif
	}
	else
	{
		DAC8501_CS1_1();

		bsp_SpiBusExit();	/* �ͷ�SPI���ߣ� �������߹��� */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: DAC8501_SetCS2(0)
*	����˵��: ����CS2�� ����������SPI����
*	��    ��: ��
	�� �� ֵ: ��
*********************************************************************************************************
*/
void DAC8501_SetCS2(uint8_t _level)
{
	if (_level == 0)
	{
		bsp_SpiBusEnter();	/* ռ��SPI���ߣ� �������߹��� */

		#ifdef SOFT_SPI		/* ���SPI */
			bsp_SetSpiSck(0);
			DAC8501_CS2_0();
		#endif

		#ifdef HARD_SPI		/* Ӳ��SPI */
			bsp_SPI_Init(SPI_Direction_2Lines_FullDuplex | SPI_Mode_Master | SPI_DataSize_8b
				| SPI_CPOL_Low | SPI_CPHA_1Edge | SPI_NSS_Soft | SPI_BaudRatePrescaler_8 | SPI_FirstBit_MSB);

			DAC8501_CS2_0();
		#endif
	}
	else
	{
		DAC8501_CS2_1();

		bsp_SpiBusExit();	/* �ͷ�SPI���ߣ� �������߹��� */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: DAC8501_ConfigGPIO
*	����˵��: ����GPIO�� ������ SCK  MOSI  MISO �����SPI���ߡ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void DAC8501_ConfigGPIO(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* ��GPIOʱ�� */
	RCC_APB2PeriphClockCmd(DAC8501_RCC_CS1 | DAC8501_RCC_CS2, ENABLE);

	/* ���ü����������IO */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	/* �������ģʽ */
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	

	GPIO_InitStructure.GPIO_Pin = DAC8501_PIN_CS1;
	GPIO_Init(DAC8501_PORT_CS1, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = DAC8501_PIN_CS2;
	GPIO_Init(DAC8501_PORT_CS2, &GPIO_InitStructure);
}

/*
*********************************************************************************************************
*	�� �� ��: DAC8501_SetDacData
*	����˵��: ����DAC����
*	��    ��: _ch, ͨ��,
*		     _data : ����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void DAC8501_SetDacData(uint8_t _ch, uint16_t _dac)
{
	uint32_t data;

	/*
		DAC8501.pdf page 12 ��24bit����

		DB24:18 = xxxxx ����
		DB17�� PD1
		DB16�� PD0

		DB15��0  16λ����

		���� PD1 PD0 ����4�ֹ���ģʽ
		      0   0  ---> ��������ģʽ
		      0   1  ---> �����1Kŷ��GND
		      1   0  ---> ���100Kŷ��GND
		      1   1  ---> �������
	*/

	data = _dac; /* PD1 PD0 = 00 ����ģʽ */

	if (_ch == 0)
	{
		DAC8501_SetCS1(0);
	}
	else
	{
		DAC8501_SetCS2(0);
	}

	/*��DAC8501 SCLKʱ�Ӹߴ�30M����˿��Բ��ӳ� */
	bsp_spiWrite0(data >> 16);
	bsp_spiWrite0(data >> 8);
	bsp_spiWrite0(data);

	if (_ch == 0)
	{
		DAC8501_SetCS1(1);
	}
	else
	{
		DAC8501_SetCS2(1);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: DAC8501_DacToVoltage
*	����˵��: ��DACֵ����Ϊ��ѹֵ����λ0.1mV
*	��    ��: _dac  16λDAC��
*	�� �� ֵ: ��ѹ����λ0.1mV
*********************************************************************************************************
*/
int32_t DAC8501_DacToVoltage(uint16_t _dac)
{
	int32_t y;

	/* CaculTwoPoint(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x);*/
	y =  CaculTwoPoint(X1, Y1, X2, Y2, _dac);
	if (y < 0)
	{
		y = 0;
	}
	return y;
}

/*
*********************************************************************************************************
*	�� �� ��: DAC8501_DacToVoltage
*	����˵��: ��DACֵ����Ϊ��ѹֵ����λ 0.1mV
*	��    ��: _volt ��ѹ����λ0.1mV
*	�� �� ֵ: 16λDAC��
*********************************************************************************************************
*/
uint32_t DAC8501_VoltageToDac(int32_t _volt)
{
	/* CaculTwoPoint(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x);*/
	return CaculTwoPoint(Y1, X1, Y2, X2, _volt);
}



/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
