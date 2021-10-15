/*
*********************************************************************************************************
*
*	ģ������ : AD9833 ����ģ��(��ͨ����16λADC)
*	�ļ����� : bsp_AD9833.c
*	��    �� : V1.0
*	˵    �� : AD9833ģ���CPU֮�����SPI�ӿڡ�����������֧��Ӳ��SPI�ӿں����SPI�ӿڡ�SPI���ߵ� 
*			   SCK �� MOSI ��bsp_spi_bus.c������, ͨ�����л�Ӳ��SPI�������SPI
*
*	�޸ļ�¼ :
*		�汾��  ����         ����     ˵��
*		V1.0   2015-08-09    armfly   �װ�
*
*	Copyright (C), 2015, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/* SPI���ߵ� SCK �� MOSI ��bsp_spi_bus.c������. ֧��Ӳ��SPI�����SPI */

/* AD9833����Ƶ�� = 25MHz */
#define AD9833_SYSTEM_CLOCK 25000000UL 

/*
	AD9833ģ�����ֱ�Ӳ嵽STM32-V4������CN29��ĸ(2*4P 2.54mm)�ӿ��� ��SPI��

	AD9833 ģ��    STM32-V4������
	  GND   ------  GND    
	  VCC   ------  3.3V
	  
	  FSYNC ------  PC3/ADC123_IN13/NRF24L01_CSN    ----- ����SPIƬѡCS
      	  
      SCLK  ------  PA5/SPI1_SCK
      SDATA ------  PA7/SPI1_MOSI
	  
	  SPI��SCKʱ���ٶ������Ե�40MHz 
*/

/* SPIƬѡ CS,  оƬ�ֲ�� FSYNC ֡ͬ��������һ����˼��  */
#define RCC_CS 	RCC_APB2Periph_GPIOC
#define PORT_CS	GPIOC
#define PIN_CS	GPIO_Pin_3
	
/* ���������0����1�ĺ� */
#define AD9833_CS_0()	PORT_CS->BRR = PIN_CS
#define AD9833_CS_1()	PORT_CS->BSRR = PIN_CS

/*���岨�εļĴ���ֵ*/
#define Triangle_Wave  	0x2002
#define Sine_Wave 		0x2028
#define square wave 	0x2000

static void AD9833_ConfigGPIO(void);
static void AD9833_Write16Bits(uint16_t _cmd);

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitAD9833
*	����˵��: ����STM32��GPIO��SPI�ӿڣ��������� AD9833
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitAD9833(void)
{
	AD9833_ConfigGPIO();	/* ����Ƭѡ��SPI������ bsp_spi_bus.c �н������� */
	
	/* ��ʼ������ */
	{
		/* ������������������λ��Ƶ�ʺͿ��ƼĴ����� */
		AD9833_Write16Bits(0x0100);	/* д���ƼĴ�������λ�Ĵ��� RESET = 0 */
		
		AD9833_WriteFreqReg(0, 0);	/* ����Ƶ�ʼĴ��� 0 */
		AD9833_WriteFreqReg(1, 0);	/* ����Ƶ�ʼĴ��� 1 */
		
		AD9833_WritePhaseReg(0, 0);	/* ������λ�Ĵ��� 0 */
		AD9833_WritePhaseReg(1, 0);	/* ������λ�Ĵ��� 1 */		
		
		AD9833_Write16Bits(0x0000);	/* д���ƼĴ�������λ�Ĵ��� RESET = 1 */
	}
	
	AD9833_SelectWave(NONE_WAVE);	/* ֹͣ��� */
}

/*
*********************************************************************************************************
*	�� �� ��: AD9833_ConfigGPIO
*	����˵��: ����GPIO�� ������ SCK  MOSI  MISO �����SPI���ߡ�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void AD9833_ConfigGPIO(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* ��GPIOʱ�� */
	RCC_APB2PeriphClockCmd(RCC_CS, ENABLE);
		
	AD9833_CS_1();

	/* ����ƬѡΪ�������IO */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	/* �������ģʽ */
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	
	
	GPIO_InitStructure.GPIO_Pin = PIN_CS;
	GPIO_Init(PORT_CS, &GPIO_InitStructure);
}

/*
*********************************************************************************************************
*	�� �� ��: AD9833_SetCS
*	����˵��: ����CS1�� ����������SPI����
*	��    ��: ��
	�� �� ֵ: ��
*********************************************************************************************************
*/
void AD9833_SetCS(uint8_t _level)
{
	if (_level == 0)
	{
		bsp_SpiBusEnter();	/* ռ��SPI���ߣ� �������߹��� */

		#ifdef SOFT_SPI		/* ���SPI */
			bsp_SetSpiSck(1);
			AD9833_CS_0();	/* Ƭѡ = 0 */
		#endif

		#ifdef HARD_SPI		/* Ӳ��SPI  -- AD9833 ֧�����40MHz ��SCK���� */
			bsp_SPI_Init(SPI_Direction_2Lines_FullDuplex | SPI_Mode_Master | SPI_DataSize_8b
				| SPI_CPOL_High | SPI_CPHA_1Edge | SPI_NSS_Soft | SPI_BaudRatePrescaler_8 | SPI_FirstBit_MSB);

			AD9833_CS_0();	/* Ƭѡ = 1 */
		#endif
	}
	else
	{
		AD9833_CS_1();	/* Ƭѡ = 1 */

		bsp_SpiBusExit();	/* �ͷ�SPI���ߣ� �������߹��� */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: AD9833_Write16Bits      
*	����˵��: ��SPI���߷���16��bit����   ���Ϳ�����
*	��    ��: _cmd : ����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void AD9833_Write16Bits(uint16_t _cmd)
{	
	AD9833_SetCS(0);

	bsp_spiWrite1(_cmd >> 8);
	bsp_spiWrite1(_cmd);

	AD9833_SetCS(1);
}

/*
*********************************************************************************************************
*	�� �� ��: AD9833_Write16Bits      
*	����˵��: ѡ����������͡�NONE_WAVE��ʾֹͣ�����
*	��    ��: _type : ��������
*					NONE_WAVE  ��ʾֹͣ���
*					TRI_WAVE   ��ʾ������ǲ�
*					SINE_WAVE  ��ʾ������Ҳ�
*					SQU_WAVE   ��ʾ�������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void AD9833_SelectWave(AD9833_WAVE_E _type) 
{	
	/* D5  D1 
		0   0  -- ����
	    0   1  -- ����
	    1   0  -- ����
	*/	
	if (_type == NONE_WAVE)
	{
		AD9833_Write16Bits(0x20C0); /* ����� */
	}
	else if (_type == TRI_WAVE)
	{
		AD9833_Write16Bits(0x2002); /* Ƶ�ʼĴ���������ǲ� */				
	}
	else if (_type == SINE_WAVE)
	{
		AD9833_Write16Bits(0x2000); /* Ƶ�ʼĴ���������Ҳ� */					
	}
	else if (_type == SQU_WAVE)
	{
		AD9833_Write16Bits(0x2028); /* Ƶ�ʼĴ���������� */	
	}
}

/*
*********************************************************************************************************
*	�� �� ��: AD9833_WriteFreqReg      
*	����˵��: дƵ�ʼĴ�����  ��Ҫȷ��ִ�й�һ�� AD9833_Write16Bits(0x2000); B28=1 ��ʾдƵ�ʷ�2��д��
*	��    ��: _mode : 0��ʾƵ�ʼĴ���0�� 1��ʾƵ�ʼĴ���1
*			  _freq_reg : Ƶ�ʼĴ�����ֵ�� 28bit;
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void AD9833_WriteFreqReg(uint8_t _mode, uint32_t _freq_reg)
{
	uint16_t lsb_14bit;
	uint16_t msb_14bit;
	
	lsb_14bit = _freq_reg & 0x3FFF;
	msb_14bit = (_freq_reg >> 14) & 0x3FFF;
	if (_mode == 0)	/* дƵ�ʼĴ���0 */
	{
		AD9833_Write16Bits(0x4000 + lsb_14bit);	/* д2�Σ���1���ǵ�14λ */		
		AD9833_Write16Bits(0x4000 + msb_14bit);	/* д2�Σ���2���Ǹ�14λ */	
	}
	else	/* дƵ�ʼĴ���1 */
	{	
		AD9833_Write16Bits(0x8000 + lsb_14bit);	/* д2�Σ���1���ǵ�14λ */		
		AD9833_Write16Bits(0x8000 + msb_14bit);	/* д2�Σ���2���Ǹ�14λ */	
	}
}

/*
*********************************************************************************************************
*	�� �� ��: AD9833_WritePhaseReg      
*	����˵��: д��λ�Ĵ�����  
*	��    ��: _mode : 0��ʾ��λ�Ĵ���0�� 1��ʾ��λ�Ĵ���1
*			  _phase_reg : ��λ�Ĵ�����ֵ, 12bit
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void AD9833_WritePhaseReg(uint8_t _mode, uint32_t _phase_reg)
{
	_phase_reg &= 0xFFF;
	if (_mode == 0)	/* д��λ�Ĵ���0 */
	{
		AD9833_Write16Bits(0xC000 + _phase_reg);
	}
	else	/* д��λ�Ĵ���1 */
	{	
		AD9833_Write16Bits(0xE000 + _phase_reg);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: AD9833_SetWaveFreq      
*	����˵��: ���ò���Ƶ�ʡ���λ0.1Hz
*	��    ��: _WaveFreq Ƶ�ʵ�λ��0.1Hz��  100��ʾ10.0Hz
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void AD9833_SetWaveFreq(uint32_t _WaveFreq)
{
	uint32_t freq_reg;

	/*
		���Ƶ�� _WaveFreq = (25000000 / 2^28) * freq_reg
		�Ĵ���ֵ freq_reg =  268435456 / 25000000	* _WaveFreq
	
		268435456 / 25000000 = 10.73741824
	*/	
	//freq = 268435456.0 / AD9833_SYSTEM_CLOCK * _freq;
	freq_reg = ((int64_t)_WaveFreq * 268435456) / (10 * AD9833_SYSTEM_CLOCK);	
	
	AD9833_WriteFreqReg(0, freq_reg);	/* дƵ�ʼĴ��� */
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
