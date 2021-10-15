/*
*********************************************************************************************************
*
*	ģ������ : TFTҺ����ʾ������ģ��
*	�ļ����� : SPFD5420_SPFD5420.c
*	��    �� : V2.3
*	˵    �� : ����������������TFT��ʾ���ֱ���Ϊ240x400��3.0���������PWM������ڹ��ܡ�
*				֧�ֵ�LCD�ڲ�����оƬ�ͺ��У�SPFD5420A��OTM4001A��R61509V
*				����оƬ�ķ��ʵ�ַΪ:  0x60000000
*	�޸ļ�¼ :
*		�汾��  ����       ����    ˵��
*		v1.0    2011-08-21 armfly  ST�̼���汾 V3.5.0�汾��
*					a) ȡ�����ʼĴ����Ľṹ�壬ֱ�Ӷ���
*		V2.0    2011-10-16 armfly  ����R61509V������ʵ��ͼ����ʾ����
*		V2.1    2012-07-06 armfly  ����RA8875������֧��4.3����
*		V2.2    2012-07-13 armfly  �Ľ�SPFD5420_DispStr������֧��12�����ַ�;�޸�SPFD5420_DrawRect,�����
*								һ����������
*		V2.3	2014-10-20 armfly  ���ñ���ĺ����Ƶ�  bsp_tft_lcd.c
*
*	Copyright (C), 2010-2011, ���������� www.armfly.com
*
*********************************************************************************************************
*/

/*
	������ʾ��
	TFT��������һ���12864������ʾ���Ŀ����������������������˴��ڻ�ͼ�Ļ��ƣ�������ƶ��ڻ��ƾֲ�ͼ��
	�Ƿǳ���Ч�ġ�TFT������ָ��һ����ͼ���ڣ�Ȼ�����еĶ�д�Դ�Ĳ��������������֮�ڣ����������ҪCPU��
	�ڴ��б���������Ļ���������ݡ�
*/

#include "bsp.h"
#include "fonts.h"


/*
	������ STM32-X2 ���İ���߷�����

	LCDģ��� RS ����     �� PD13/FSMC_A18
	LCDģ��� CS Ƭѡ���� �� PD7/FSMC_NE1
	������ NWE, NOE, D15~D0 �Ӷ�Ӧ��FSMC���߼��ɡ�
*/
#define SPFD5420_BASE       ((uint32_t)(0x60000000 | 0x00000000))

#define SPFD5420_REG		*(__IO uint16_t *)(SPFD5420_BASE)
#define SPFD5420_RAM		*(__IO uint16_t *)(SPFD5420_BASE + (1 << 19))

static __IO uint8_t s_RGBChgEn = 0;		/* RGBת��ʹ��, 4001��д�Դ������RGB��ʽ��д��Ĳ�ͬ */

static void SPFD5420_Delaly10ms(void);
static void SPFD5420_WriteReg(__IO uint16_t _usAddr, uint16_t _usValue);
static uint16_t SPFD5420_ReadReg(__IO uint16_t _usAddr);
static void Init_5420_4001(void);
static void Init_61509(void);
static void SPFD5420_SetDispWin(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth);
static void SPFD5420_QuitWinMode(void);
static void SPFD5420_SetCursor(uint16_t _usX, uint16_t _usY);
static uint16_t SPFD5420_BGR2RGB(uint16_t _usRGB);

/*
*********************************************************************************************************
*	�� �� ��: SPFD5420_Delaly10ms
*	����˵��: �ӳٺ�������׼
*	��    ��:
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void SPFD5420_Delaly10ms(void)
{
	uint16_t i;

	for (i = 0; i < 50000; i++);
}

/*
*********************************************************************************************************
*	�� �� ��: SPFD5420_WriteReg
*	����˵��: �޸�LCD�������ļĴ�����ֵ��
*	��    ��:  
*			SPFD5420_Reg ���Ĵ�����ַ;
*			SPFD5420_RegValue : �Ĵ���ֵ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void SPFD5420_WriteReg(__IO uint16_t _usAddr, uint16_t _usValue)
{
	/* Write 16-bit Index, then Write Reg */
	SPFD5420_REG = _usAddr;
	/* Write 16-bit Reg */
	SPFD5420_RAM = _usValue;
}

/*
*********************************************************************************************************
*	�� �� ��: SPFD5420_ReadReg
*	����˵��: ��ȡLCD�������ļĴ�����ֵ��
*	��    ��:  
*			SPFD5420_Reg ���Ĵ�����ַ;
*			SPFD5420_RegValue : �Ĵ���ֵ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static uint16_t SPFD5420_ReadReg(__IO uint16_t _usAddr)
{
	/* Write 16-bit Index (then Read Reg) */
	SPFD5420_REG = _usAddr;
	/* Read 16-bit Reg */
	return (SPFD5420_RAM);
}


/*
*********************************************************************************************************
*	�� �� ��: SPFD5420_SetDispWin
*	����˵��: ������ʾ���ڣ����봰����ʾģʽ��TFT����оƬ֧�ִ�����ʾģʽ�����Ǻ�һ���12864����LCD���������
*	��    ��:  
*		_usX : ˮƽ����
*		_usY : ��ֱ����
*		_usHeight: ���ڸ߶�
*		_usWidth : ���ڿ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void SPFD5420_SetDispWin(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth)
{
	uint16_t px, py;
	/*
		240x400��Ļ��������(px,py)����:
		    R003 = 0x1018                  R003 = 0x1008
		  -------------------          -------------------
		 |(0,0)              |        |(0,0)              |
		 |                   |        |					  |
		 |  ^           ^    |        |   ^           ^   |
		 |  |           |    |        |   |           |   |
		 |  |           |    |        |   |           |   |
		 |  |           |    |        |   |           |   |
		 |  |  ------>  |    |        |   | <------   |   |
		 |  |           |    |        |   |           |   |
		 |  |           |    |        |   |           |   |
		 |  |           |    |        |   |           |   |
		 |  |           |    |        |   |           |   |
		 |                   |        |					  |
		 |                   |        |                   |
		 |      (x=239,y=399)|        |      (x=239,y=399)|
		 |-------------------|        |-------------------|
		 |                   |        |                   |
		  -------------------          -------------------

		���հ�����������LCD�ķ����������������������ɨ�跽�����£�(����ͼ��1���Ǻ�)
		 --------------------------------
		|  |(0,0)                        |
		|  |     --------->              |
		|  |         |                   |
		|  |         |                   |
		|  |         |                   |
		|  |         V                   |
		|  |     --------->              |
		|  |                    (399,239)|
		 --------------------------------
	�����������������ת����ϵ��
		x = 399 - py;
		y = px;

		py = 399 - x;
		px = y;
	*/

	py = 399 - _usX;
	px = _usY;

	/* ������ʾ���� WINDOWS */
	SPFD5420_WriteReg(0x0210, px);						/* ˮƽ��ʼ��ַ */
	SPFD5420_WriteReg(0x0211, px + (_usHeight - 1));		/* ˮƽ�������� */
	SPFD5420_WriteReg(0x0212, py + 1 - _usWidth);		/* ��ֱ��ʼ��ַ */
	SPFD5420_WriteReg(0x0213, py);						/* ��ֱ������ַ */

	SPFD5420_SetCursor(_usX, _usY);
}

/*
*********************************************************************************************************
*	�� �� ��: SPFD5420_SetCursor
*	����˵��: ���ù��λ��
*	��    ��:  _usX : X����; _usY: Y����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void SPFD5420_SetCursor(uint16_t _usX, uint16_t _usY)
{
	/*
		px��py ���������꣬ x��y����������
		ת����ʽ:
		py = 399 - x;
		px = y;
	*/

	SPFD5420_WriteReg(0x0200, _usY);  		/* px */
	SPFD5420_WriteReg(0x0201, 399 - _usX);	/* py */
}

/*
*********************************************************************************************************
*	�� �� ��: SPFD5420_BGR2RGB
*	����˵��: RRRRRGGGGGGBBBBB ��Ϊ BBBBBGGGGGGRRRRR ��ʽ
*	��    ��:  RGB��ɫ����
*	�� �� ֵ: ת�������ɫ����
*********************************************************************************************************
*/
static uint16_t SPFD5420_BGR2RGB(uint16_t _usRGB)
{
	uint16_t  r, g, b, rgb;

	b = (_usRGB >> 0)  & 0x1F;
	g = (_usRGB >> 5)  & 0x3F;
	r = (_usRGB >> 11) & 0x1F;

	rgb = (b<<11) + (g<<5) + (r<<0);

	return( rgb );
}

/*
*********************************************************************************************************
*	�� �� ��: SPFD5420_QuitWinMode
*	����˵��: �˳�������ʾģʽ����Ϊȫ����ʾģʽ
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void SPFD5420_QuitWinMode(void)
{
	SPFD5420_SetDispWin(0, 0, g_LcdHeight, g_LcdWidth);
}

/*
*********************************************************************************************************
*	�� �� ��: SPFD5420_ReadID
*	����˵��: ��ȡLCD����оƬID
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
uint16_t SPFD5420_ReadID(void)
{
	return SPFD5420_ReadReg(0x0000);
}

/*
*********************************************************************************************************
*	�� �� ��: Init_5420_4001
*	����˵��: ��ʼ��5420��4001��
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void Init_5420_4001(void)
{
	/* ��ʼ��LCD��дLCD�Ĵ����������� */
	SPFD5420_WriteReg(0x0000, 0x0000);
	SPFD5420_WriteReg(0x0001, 0x0100);
	SPFD5420_WriteReg(0x0002, 0x0100);

	/*
		R003H �Ĵ����ܹؼ��� Entry Mode ��������ɨ�跽��
		�μ���SPFD5420A.pdf ��15ҳ

		240x400��Ļ��������(px,py)����:
		    R003 = 0x1018                  R003 = 0x1008
		  -------------------          -------------------
		 |(0,0)              |        |(0,0)              |
		 |                   |        |					  |
		 |  ^           ^    |        |   ^           ^   |
		 |  |           |    |        |   |           |   |
		 |  |           |    |        |   |           |   |
		 |  |           |    |        |   |           |   |
		 |  |  ------>  |    |        |   | <------   |   |
		 |  |           |    |        |   |           |   |
		 |  |           |    |        |   |           |   |
		 |  |           |    |        |   |           |   |
		 |  |           |    |        |   |           |   |
		 |                   |        |					  |
		 |                   |        |                   |
		 |      (x=239,y=399)|        |      (x=239,y=399)|
		 |-------------------|        |-------------------|
		 |                   |        |                   |
		  -------------------          -------------------

		���հ�����������LCD�ķ����������������������ɨ�跽�����£�(����ͼ��1���Ǻ�)
		 --------------------------------
		|  |(0,0)                        |
		|  |     --------->              |
		|  |         |                   |
		|  |         |                   |
		|  |         |                   |
		|  |         V                   |
		|  |     --------->              |
		|  |                    (399,239)|
		 --------------------------------

		��������(x,y) �����������ת����ϵ
		x = 399 - py;
		y = px;

		py = 399 - x;
		px = y;

	*/
	SPFD5420_WriteReg(0x0003, 0x1018); /* 0x1018 1030 */

	SPFD5420_WriteReg(0x0008, 0x0808);
	SPFD5420_WriteReg(0x0009, 0x0001);
	SPFD5420_WriteReg(0x000B, 0x0010);
	SPFD5420_WriteReg(0x000C, 0x0000);
	SPFD5420_WriteReg(0x000F, 0x0000);
	SPFD5420_WriteReg(0x0007, 0x0001);
	SPFD5420_WriteReg(0x0010, 0x0013);
	SPFD5420_WriteReg(0x0011, 0x0501);
	SPFD5420_WriteReg(0x0012, 0x0300);
	SPFD5420_WriteReg(0x0020, 0x021E);
	SPFD5420_WriteReg(0x0021, 0x0202);
	SPFD5420_WriteReg(0x0090, 0x8000);
	SPFD5420_WriteReg(0x0100, 0x17B0);
	SPFD5420_WriteReg(0x0101, 0x0147);
	SPFD5420_WriteReg(0x0102, 0x0135);
	SPFD5420_WriteReg(0x0103, 0x0700);
	SPFD5420_WriteReg(0x0107, 0x0000);
	SPFD5420_WriteReg(0x0110, 0x0001);
	SPFD5420_WriteReg(0x0210, 0x0000);
	SPFD5420_WriteReg(0x0211, 0x00EF);
	SPFD5420_WriteReg(0x0212, 0x0000);
	SPFD5420_WriteReg(0x0213, 0x018F);
	SPFD5420_WriteReg(0x0280, 0x0000);
	SPFD5420_WriteReg(0x0281, 0x0004);
	SPFD5420_WriteReg(0x0282, 0x0000);
	SPFD5420_WriteReg(0x0300, 0x0101);
	SPFD5420_WriteReg(0x0301, 0x0B2C);
	SPFD5420_WriteReg(0x0302, 0x1030);
	SPFD5420_WriteReg(0x0303, 0x3010);
	SPFD5420_WriteReg(0x0304, 0x2C0B);
	SPFD5420_WriteReg(0x0305, 0x0101);
	SPFD5420_WriteReg(0x0306, 0x0807);
	SPFD5420_WriteReg(0x0307, 0x0708);
	SPFD5420_WriteReg(0x0308, 0x0107);
	SPFD5420_WriteReg(0x0309, 0x0105);
	SPFD5420_WriteReg(0x030A, 0x0F04);
	SPFD5420_WriteReg(0x030B, 0x0F00);
	SPFD5420_WriteReg(0x030C, 0x000F);
	SPFD5420_WriteReg(0x030D, 0x040F);
	SPFD5420_WriteReg(0x030E, 0x0300);
	SPFD5420_WriteReg(0x030F, 0x0701);
	SPFD5420_WriteReg(0x0400, 0x3500);
	SPFD5420_WriteReg(0x0401, 0x0001);
	SPFD5420_WriteReg(0x0404, 0x0000);
	SPFD5420_WriteReg(0x0500, 0x0000);
	SPFD5420_WriteReg(0x0501, 0x0000);
	SPFD5420_WriteReg(0x0502, 0x0000);
	SPFD5420_WriteReg(0x0503, 0x0000);
	SPFD5420_WriteReg(0x0504, 0x0000);
	SPFD5420_WriteReg(0x0505, 0x0000);
	SPFD5420_WriteReg(0x0600, 0x0000);
	SPFD5420_WriteReg(0x0606, 0x0000);
	SPFD5420_WriteReg(0x06F0, 0x0000);
	SPFD5420_WriteReg(0x07F0, 0x5420);
	SPFD5420_WriteReg(0x07DE, 0x0000);
	SPFD5420_WriteReg(0x07F2, 0x00DF);
	SPFD5420_WriteReg(0x07F3, 0x0810);
	SPFD5420_WriteReg(0x07F4, 0x0077);
	SPFD5420_WriteReg(0x07F5, 0x0021);
	SPFD5420_WriteReg(0x07F0, 0x0000);
	SPFD5420_WriteReg(0x0007, 0x0173);

	/* ������ʾ���� WINDOWS */
	SPFD5420_WriteReg(0x0210, 0);	/* ˮƽ��ʼ��ַ */
	SPFD5420_WriteReg(0x0211, 239);	/* ˮƽ�������� */
	SPFD5420_WriteReg(0x0212, 0);	/* ��ֱ��ʼ��ַ */
	SPFD5420_WriteReg(0x0213, 399);	/* ��ֱ������ַ */
}

/*
*********************************************************************************************************
*	�� �� ��: Init_61509
*	����˵��: ��ʼ��61509��
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void Init_61509(void)
{
	SPFD5420_WriteReg(0x000,0x0000);
	SPFD5420_WriteReg(0x000,0x0000);
	SPFD5420_WriteReg(0x000,0x0000);
	SPFD5420_WriteReg(0x000,0x0000);
	SPFD5420_Delaly10ms();

	SPFD5420_WriteReg(0x008,0x0808);
	SPFD5420_WriteReg(0x010,0x0010);
	SPFD5420_WriteReg(0x400,0x6200);

	SPFD5420_WriteReg(0x300,0x0c06);	/* GAMMA */
	SPFD5420_WriteReg(0x301,0x9d0f);
	SPFD5420_WriteReg(0x302,0x0b05);
	SPFD5420_WriteReg(0x303,0x1217);
	SPFD5420_WriteReg(0x304,0x3333);
	SPFD5420_WriteReg(0x305,0x1712);
	SPFD5420_WriteReg(0x306,0x950b);
	SPFD5420_WriteReg(0x307,0x0f0d);
	SPFD5420_WriteReg(0x308,0x060c);
	SPFD5420_WriteReg(0x309,0x0000);

	SPFD5420_WriteReg(0x011,0x0202);
	SPFD5420_WriteReg(0x012,0x0101);
	SPFD5420_WriteReg(0x013,0x0001);

	SPFD5420_WriteReg(0x007,0x0001);
	SPFD5420_WriteReg(0x100,0x0730);	/* BT,AP 0x0330��*/
	SPFD5420_WriteReg(0x101,0x0237);	/* DC0,DC1,VC */
	SPFD5420_WriteReg(0x103,0x2b00);	/* VDV	//0x0f00 */
	SPFD5420_WriteReg(0x280,0x4000);	/* VCM */
	SPFD5420_WriteReg(0x102,0x81b0);	/* VRH,VCMR,PSON,PON */
	SPFD5420_Delaly10ms();

	SPFD5420_WriteReg(0x001,0x0100);
	SPFD5420_WriteReg(0x002,0x0100);
	/* SPFD5420_WriteReg(0x003,0x1030); */
	SPFD5420_WriteReg(0x003,0x1018);
	SPFD5420_WriteReg(0x009,0x0001);

	SPFD5420_WriteReg(0x0C,0x0000);	/* MCU interface  */
	/*
		SPFD5420_WriteReg(0x0C,0x0110);	//RGB interface 18bit
		SPFD5420_WriteReg(0x0C,0x0111);	//RGB interface 16bit
		SPFD5420_WriteReg(0x0C,0x0020);	//VSYNC interface
	*/

	SPFD5420_WriteReg(0x090,0x8000);
	SPFD5420_WriteReg(0x00f,0x0000);

	SPFD5420_WriteReg(0x210,0x0000);
	SPFD5420_WriteReg(0x211,0x00ef);
	SPFD5420_WriteReg(0x212,0x0000);
	SPFD5420_WriteReg(0x213,0x018f);

	SPFD5420_WriteReg(0x500,0x0000);
	SPFD5420_WriteReg(0x501,0x0000);
	SPFD5420_WriteReg(0x502,0x005f);
	SPFD5420_WriteReg(0x401,0x0001);
	SPFD5420_WriteReg(0x404,0x0000);
	SPFD5420_Delaly10ms();
	SPFD5420_WriteReg(0x007,0x0100);
	SPFD5420_Delaly10ms();
	SPFD5420_WriteReg(0x200,0x0000);
	SPFD5420_WriteReg(0x201,0x0000);
}

/*
*********************************************************************************************************
*	�� �� ��: SPFD5420_InitHard
*	����˵��: ��ʼ��LCD
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SPFD5420_InitHard(void)
{
	uint16_t id;

	id = SPFD5420_ReadReg(0x0000);  	/* ��ȡLCD����оƬID */

	if ((id == 0x5420) || (id == 0x5520))	/* 4001����5420��ͬ��4001�������Դ�RGBʱ����Ҫ����ת����5420���� */
	{
		Init_5420_4001();	/* ��ʼ��5420��4001��Ӳ�� */

		/* ������δ�������ʶ����4001������5420�� */
		{
			uint16_t dummy;

			SPFD5420_WriteReg(0x0200, 0);
			SPFD5420_WriteReg(0x0201, 0);

			SPFD5420_REG = 0x0202;
			SPFD5420_RAM = 0x1234;		/* дһ������ */

			SPFD5420_WriteReg(0x0200, 0);
			SPFD5420_WriteReg(0x0201, 0);
			SPFD5420_REG = 0x0202;
			dummy = SPFD5420_RAM; 		/* ������ɫֵ */
			if (dummy == 0x1234)
			{
				s_RGBChgEn = 0;

				g_ChipID = IC_5420;
			}
			else
			{
				s_RGBChgEn = 1;		/* ������صĺ�д��Ĳ�ͬ������ҪRGBת��, ֻӰ���ȡ���صĺ��� */

				g_ChipID = IC_4001;
			}
			g_LcdHeight = LCD_30_HEIGHT;
			g_LcdWidth = LCD_30_WIDTH;
		}
	}
	else if (id == 0xB509)
	{
		Init_61509();		/* ��ʼ��61509��Ӳ�� */

		s_RGBChgEn = 1;			/* ������صĺ�д��Ĳ�ͬ������ҪRGBת��, ֻӰ���ȡ���صĺ��� */

		g_ChipID = IC_61509;
		g_LcdHeight = LCD_30_HEIGHT;
		g_LcdWidth = LCD_30_WIDTH;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: SPFD5420_DispOn
*	����˵��: ����ʾ
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SPFD5420_DispOn(void)
{
	if (g_ChipID == IC_61509)
	{
		SPFD5420_WriteReg(0x007,0x0100);
	}
	else	/* IC_4001 */
	{
		SPFD5420_WriteReg(7, 0x0173); /* ����262K��ɫ���Ҵ���ʾ */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: SPFD5420_DispOff
*	����˵��: �ر���ʾ
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SPFD5420_DispOff(void)
{
	SPFD5420_WriteReg(7, 0x0000);
}

/*
*********************************************************************************************************
*	�� �� ��: SPFD5420_ClrScr
*	����˵��: �����������ɫֵ����
*	��    ��:  _usColor : ����ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SPFD5420_ClrScr(uint16_t _usColor)
{
	uint32_t i;

	SPFD5420_SetCursor(0, 0);		/* ���ù��λ�� */

	SPFD5420_REG = 0x202; 			/* ׼����д�Դ� */

	for (i = 0; i < g_LcdHeight * g_LcdWidth; i++)
	{
		SPFD5420_RAM = _usColor;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: SPFD5420_PutPixel
*	����˵��: ��1������
*	��    ��:  
*			_usX,_usY : ��������
*			_usColor  ��������ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SPFD5420_PutPixel(uint16_t _usX, uint16_t _usY, uint16_t _usColor)
{
	SPFD5420_SetCursor(_usX, _usY);	/* ���ù��λ�� */

	/* д�Դ� */
	SPFD5420_REG = 0x202;
	/* Write 16-bit GRAM Reg */
	SPFD5420_RAM = _usColor;
}

/*
*********************************************************************************************************
*	�� �� ��: SPFD5420_GetPixel
*	����˵��: ��ȡ1������
*	��    ��:  
*			_usX,_usY : ��������
*			_usColor  ��������ɫ
*	�� �� ֵ: RGB��ɫֵ
*********************************************************************************************************
*/
uint16_t SPFD5420_GetPixel(uint16_t _usX, uint16_t _usY)
{
	uint16_t usRGB;

	SPFD5420_SetCursor(_usX, _usY);	/* ���ù��λ�� */

	{
		/* ׼��д�Դ� */
		SPFD5420_REG = 0x202;

		usRGB = SPFD5420_RAM;

		/* �� 16-bit GRAM Reg */
		if (s_RGBChgEn == 1)
		{
			usRGB = SPFD5420_BGR2RGB(usRGB);
		}
	}

	return usRGB;
}

/*
*********************************************************************************************************
*	�� �� ��: SPFD5420_DrawLine
*	����˵��: ���� Bresenham �㷨����2��仭һ��ֱ�ߡ�
*	��    ��:  
*			_usX1, _usY1 ����ʼ������
*			_usX2, _usY2 ����ֹ��Y����
*			_usColor     ����ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SPFD5420_DrawLine(uint16_t _usX1 , uint16_t _usY1 , uint16_t _usX2 , uint16_t _usY2 , uint16_t _usColor)
{
	int32_t dx , dy ;
	int32_t tx , ty ;
	int32_t inc1 , inc2 ;
	int32_t d , iTag ;
	int32_t x , y ;

	/* ���� Bresenham �㷨����2��仭һ��ֱ�� */

	SPFD5420_PutPixel(_usX1 , _usY1 , _usColor);

	/* ��������غϣ���������Ķ�����*/
	if ( _usX1 == _usX2 && _usY1 == _usY2 )
	{
		return;
	}

	iTag = 0 ;
	/* dx = abs ( _usX2 - _usX1 ); */
	if (_usX2 >= _usX1)
	{
		dx = _usX2 - _usX1;
	}
	else
	{
		dx = _usX1 - _usX2;
	}

	/* dy = abs ( _usY2 - _usY1 ); */
	if (_usY2 >= _usY1)
	{
		dy = _usY2 - _usY1;
	}
	else
	{
		dy = _usY1 - _usY2;
	}

	if ( dx < dy )   /*���dyΪ�Ƴ������򽻻��ݺ����ꡣ*/
	{
		uint16_t temp;

		iTag = 1 ;
		temp = _usX1; _usX1 = _usY1; _usY1 = temp;
		temp = _usX2; _usX2 = _usY2; _usY2 = temp;
		temp = dx; dx = dy; dy = temp;
	}
	tx = _usX2 > _usX1 ? 1 : -1 ;    /* ȷ������1���Ǽ�1 */
	ty = _usY2 > _usY1 ? 1 : -1 ;
	x = _usX1 ;
	y = _usY1 ;
	inc1 = 2 * dy ;
	inc2 = 2 * ( dy - dx );
	d = inc1 - dx ;
	while ( x != _usX2 )     /* ѭ������ */
	{
		if ( d < 0 )
		{
			d += inc1 ;
		}
		else
		{
			y += ty ;
			d += inc2 ;
		}
		if ( iTag )
		{
			SPFD5420_PutPixel ( y , x , _usColor) ;
		}
		else
		{
			SPFD5420_PutPixel ( x , y , _usColor) ;
		}
		x += tx ;
	}
}


/*
*********************************************************************************************************
*	�� �� ��: SPFD5420_DrawHLine
*	����˵��: ����һ��ˮƽ�� ����Ҫ����UCGUI�Ľӿں�����
*	��    ��:  _usX1    ����ʼ��X����
*			  _usY1    ��ˮƽ�ߵ�Y����
*			  _usX2    ��������X����
*			  _usColor : ��ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SPFD5420_DrawHLine(uint16_t _usX1 , uint16_t _usY1 , uint16_t _usX2 , uint16_t _usColor)
{
	uint16_t i;

	/* չ�� SPFD5420_SetCursor(_usX1, _usY1) ���������ִ��Ч�� */
	/*
		px��py ���������꣬ x��y����������
		ת����ʽ:
		py = 399 - x;
		px = y;
	*/
	SPFD5420_WriteReg(0x0200, _usY1);  		/* px */
	SPFD5420_WriteReg(0x0201, 399 - _usX1);	/* py */

	/* д�Դ� */
	SPFD5420_REG = 0x202;
	for (i = 0; i < _usX2 - _usX1 + 1; i++)
	{
		SPFD5420_RAM = _usColor;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: SPFD5420_DrawHColorLine
*	����˵��: ����һ����ɫˮƽ�� ����Ҫ����UCGUI�Ľӿں�����
*	��    ��:  _usX1    ����ʼ��X����
*			  _usY1    ��ˮƽ�ߵ�Y����
*			  _usWidth ��ֱ�ߵĿ��
*			  _pColor : ��ɫ������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SPFD5420_DrawHColorLine(uint16_t _usX1 , uint16_t _usY1, uint16_t _usWidth, const uint16_t *_pColor)
{
	uint16_t i;

	/* չ�� SPFD5420_SetCursor(_usX1, _usY1) ���������ִ��Ч�� */
	/*
		px��py ���������꣬ x��y����������
		ת����ʽ:
		py = 399 - x;
		px = y;
	*/
	SPFD5420_WriteReg(0x0200, _usY1);  		/* px */
	SPFD5420_WriteReg(0x0201, 399 - _usX1);	/* py */

	/* д�Դ� */
	SPFD5420_REG = 0x202;
	for (i = 0; i < _usWidth; i++)
	{
		SPFD5420_RAM = *_pColor++;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: SPFD5420_DrawHTransLine
*	����˵��: ����һ����ɫ͸����ˮƽ�� ����Ҫ����UCGUI�Ľӿں������� ��ɫֵΪ0��ʾ͸��ɫ
*	��    ��:  _usX1    ����ʼ��X����
*			  _usY1    ��ˮƽ�ߵ�Y����
*			  _usWidth ��ֱ�ߵĿ��
*			  _pColor : ��ɫ������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SPFD5420_DrawHTransLine(uint16_t _usX1 , uint16_t _usY1, uint16_t _usWidth, const uint16_t *_pColor)
{
	uint16_t i, j;
	uint16_t Index;

	/* չ�� SPFD5420_SetCursor(_usX1, _usY1) ���������ִ��Ч�� */
	/*
		px��py ���������꣬ x��y����������
		ת����ʽ:
		py = 399 - x;
		px = y;
	*/
	SPFD5420_WriteReg(0x0200, _usY1);  		/* px */
	SPFD5420_WriteReg(0x0201, 399 - _usX1);	/* py */

	/* д�Դ� */
	SPFD5420_REG = 0x202;
	for (i = 0,j = 0; i < _usWidth; i++, j++)
	{
		Index = *_pColor++;
	    if (Index)
        {
			 SPFD5420_RAM = Index;
		}
		else
		{
			SPFD5420_SetCursor(_usX1 + j, _usY1);
			SPFD5420_REG = 0x202;
			SPFD5420_RAM = Index;
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: SPFD5420_DrawRect
*	����˵��: ����ˮƽ���õľ��Ρ�
*	��    ��:  
*			_usX,_usY���������Ͻǵ�����
*			_usHeight �����εĸ߶�
*			_usWidth  �����εĿ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SPFD5420_DrawRect(uint16_t _usX, uint16_t _usY, uint8_t _usHeight, uint16_t _usWidth, uint16_t _usColor)
{
	/*
	 ---------------->---
	|(_usX��_usY)        |
	V                    V  _usHeight
	|                    |
	 ---------------->---
		  _usWidth
	*/

	SPFD5420_DrawLine(_usX, _usY, _usX + _usWidth - 1, _usY, _usColor);	/* �� */
	SPFD5420_DrawLine(_usX, _usY + _usHeight - 1, _usX + _usWidth - 1, _usY + _usHeight - 1, _usColor);	/* �� */

	SPFD5420_DrawLine(_usX, _usY, _usX, _usY + _usHeight - 1, _usColor);	/* �� */
	SPFD5420_DrawLine(_usX + _usWidth - 1, _usY, _usX + _usWidth - 1, _usY + _usHeight, _usColor);	/* �� */
}

/*
*********************************************************************************************************
*	�� �� ��: SPFD5420_DrawCircle
*	����˵��: ����һ��Բ���ʿ�Ϊ1������
*	��    ��:  
*			_usX,_usY  ��Բ�ĵ�����
*			_usRadius  ��Բ�İ뾶
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SPFD5420_DrawCircle(uint16_t _usX, uint16_t _usY, uint16_t _usRadius, uint16_t _usColor)
{
	int32_t  D;			/* Decision Variable */
	uint32_t  CurX;		/* ��ǰ X ֵ */
	uint32_t  CurY;		/* ��ǰ Y ֵ */

	D = 3 - (_usRadius << 1);
	CurX = 0;
	CurY = _usRadius;

	while (CurX <= CurY)
	{
		SPFD5420_PutPixel(_usX + CurX, _usY + CurY, _usColor);
		SPFD5420_PutPixel(_usX + CurX, _usY - CurY, _usColor);
		SPFD5420_PutPixel(_usX - CurX, _usY + CurY, _usColor);
		SPFD5420_PutPixel(_usX - CurX, _usY - CurY, _usColor);
		SPFD5420_PutPixel(_usX + CurY, _usY + CurX, _usColor);
		SPFD5420_PutPixel(_usX + CurY, _usY - CurX, _usColor);
		SPFD5420_PutPixel(_usX - CurY, _usY + CurX, _usColor);
		SPFD5420_PutPixel(_usX - CurY, _usY - CurX, _usColor);

		if (D < 0)
		{
			D += (CurX << 2) + 6;
		}
		else
		{
			D += ((CurX - CurY) << 2) + 10;
			CurY--;
		}
		CurX++;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: SPFD5420_DrawBMP
*	����˵��: ��LCD����ʾһ��BMPλͼ��λͼ����ɨ����򣺴����ң����ϵ���
*	��    ��:  
*			_usX, _usY : ͼƬ������
*			_usHeight  ��ͼƬ�߶�
*			_usWidth   ��ͼƬ���
*			_ptr       ��ͼƬ����ָ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void SPFD5420_DrawBMP(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t *_ptr)
{
	uint32_t index = 0;
	const uint16_t *p;

	/* ����ͼƬ��λ�úʹ�С�� ��������ʾ���� */
	SPFD5420_SetDispWin(_usX, _usY, _usHeight, _usWidth);

	p = _ptr;
	for (index = 0; index < _usHeight * _usWidth; index++)
	{
		SPFD5420_PutPixel(_usX, _usY, *p++);
	}

	/* �˳����ڻ�ͼģʽ */
	SPFD5420_QuitWinMode();
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
