/*
*********************************************************************************************************
*
*	ģ������ : OLED��ʾ������ģ��
*	�ļ����� : bsp_oled.c
*	��    �� : V1.0
*	˵    �� : OLED ��(128x64)�ײ�������������оƬ�ͺ�Ϊ SSD1306.  �����������ʺ���STM32-V5������.
*				OLEDģ�����FSMC�����ϡ�
*	�޸ļ�¼ :
*		�汾��  ����        ����    ˵��
*		V1.0    2013-02-01 armfly  ��ʽ����
*
*	Copyright (C), 2010-2012, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"
#include "fonts.h"

/*
	������STM32-V4�������OLED��ʾ�ӿ�ΪFSMC����ģʽ��
*/

/* ����2������ѡ 1��; ��ʾ��ʾ���� */
#define DIR_NORMAL			/* ���б�ʾ������ʾ���� */
//#define DIR_180				/* ���б�ʾ��ת180�� */

#ifdef OLED_SPI3_EN
	/*
		SPI ģʽ���߶��� (ֻ��Ҫ6���Ű�������)  �����Ӳ������ģ��SPIʱ��

	   ��OLEDģ�����롿 ��������TFT�ӿڣ�STM32���ߣ���
	        VCC ----------- 3.3V
	        GND ----------- GND
	         CS ----------- TP_NCS   (PI10)
	        RST ----------- NRESET (Ҳ���Բ�����)
	     D0/SCK ----------- TP_SCK   (PB3)
	    D1/SDIN ----------- TP_MOSI  (PB5)
	*/
	#define RCC_OLED_PORT (RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOI)		/* GPIO�˿�ʱ�� */

	#define OLED_CS_PORT	GPIOI
	#define OLED_CS_PIN		GPIO_Pin_10

	#define OLED_SCK_PORT	GPIOB
	#define OLED_SCK_PIN	GPIO_Pin_3

	#define OLED_SDIN_PORT	GPIOB
	#define OLED_SDIN_PIN	GPIO_Pin_5


	/* ����IO = 1�� 0�Ĵ��� �����ø��ģ� */
	#define SSD_CS_1()		OLED_CS_PORT->BSRRL = OLED_CS_PIN
	#define SSD_CS_0()		OLED_CS_PORT->BSRRH = OLED_CS_PIN

	#define SSD_SCK_1()		OLED_SCK_PORT->BSRRL = OLED_SCK_PIN
	#define SSD_SCK_0()		OLED_SCK_PORT->BSRRH = OLED_SCK_PIN

	#define SSD_SDIN_1()	OLED_SDIN_PORT->BSRRL = OLED_SDIN_PIN
	#define SSD_SDIN_0()	OLED_SDIN_PORT->BSRRH = OLED_SDIN_PIN

#else

	/* ����LCD�������ķ��ʵ�ַ
		TFT�ӿ��е�RS��������FSMC_A0���ţ�������16bitģʽ��RS��ӦA1��ַ�ߣ����
		OLED_RAM�ĵ�ַ��+2
	*/
	#define OLED_BASE       ((uint32_t)(0x6C200000))
	#define OLED_CMD		*(__IO uint16_t *)(OLED_BASE)
	#define OLED_DATA		*(__IO uint16_t *)(OLED_BASE + (1 << 1))		/* FSMC_A0��D/C */

	/*
	1.������OLEDģ�� 80XX ģʽ���߶��� (��Ҫ15���Ű������ӣ�
	   ��OLEDģ�����롿 ��������TFT�ӿڣ�STM32���ߣ���
		    VCC ----------- 3.3V
		    GND ----------- GND
		     CS ----------- NCS
	        RST ----------- NRESET (Ҳ���Բ�����)
		    D/C ----------- RS   (FSMC_A18)
		     WE ----------- NWE  (FSMC_NWE)
		     OE ----------- NOE  (FSMC_NOE)
	     D0/SCK ----------- DB0  (FSMC_D0)
		D1/SDIN ----------- DB1  (FSMC_D1)
		     D2 ----------- DB2  (FSMC_D2)
		     D3 ----------- DB3  (FSMC_D3)
		     D4 ----------- DB4  (FSMC_D4)
		     D5 ----------- DB5  (FSMC_D5)
		     D6 ----------- DB6  (FSMC_D6)
		     D7 ----------- DB7  (FSMC_D7)
	*/


#endif

/* 12864 OLED���Դ澵��ռ��1K�ֽ�. ��8�У�ÿ��128���� */
static uint8_t s_ucGRAM[8][128];

/* Ϊ�˱���ˢ����Ļ��̫ǿ������ˢ����־��
0 ��ʾ��ʾ����ֻ��д����������д����1 ��ʾֱ��д����ͬʱд�������� */
static uint8_t s_ucUpdateEn = 1;

static void OLED_ConfigGPIO(void);
static void OLED_WriteCmd(uint8_t _ucCmd);
static void OLED_WriteData(uint8_t _ucData);
static void OLED_BufToPanel(void);

/*
*********************************************************************************************************
*	�� �� ��: OLED_InitHard
*	����˵��: ��ʼ��OLED��
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void OLED_InitHard(void)
{
	OLED_ConfigGPIO();

	/* �ϵ��ӳ� */
	bsp_DelayMS(50);

	 /* ģ�鳧���ṩ��ʼ������ */
	OLED_WriteCmd(0xAE);	/* �ر�OLED�����ʾ(����) */
	OLED_WriteCmd(0x00);	/* �����е�ַ��4bit */
	OLED_WriteCmd(0x10);	/* �����е�ַ��4bit */
	OLED_WriteCmd(0x40);	/* ������ʼ�е�ַ����5bit 0-63���� Ӳ�����*/

	OLED_WriteCmd(0x81);	/* ���öԱȶ�����(˫�ֽ��������1���ֽ��������2���ֽ��ǶԱȶȲ���0-255 */
	OLED_WriteCmd(0xCF);	/* ���öԱȶȲ���,ȱʡCF */

#ifdef DIR_NORMAL
	OLED_WriteCmd(0xA0);	/* A0 ���е�ַ0ӳ�䵽SEG0; A1 ���е�ַ127ӳ�䵽SEG0 */
	OLED_WriteCmd(0xC0);	/* C0 ������ɨ��,��COM0��COM63;  C8 : ����ɨ��, �� COM63�� COM0 */
#endif

#ifdef DIR_180
	OLED_WriteCmd(0xA1);	/* A0 ���е�ַ0ӳ�䵽SEG0; A1 ���е�ַ127ӳ�䵽SEG0 */
	OLED_WriteCmd(0xC8);	/* C0 ������ɨ��,��COM0��COM63;  C8 : ����ɨ��, �� COM63�� COM0 */
#endif

	OLED_WriteCmd(0xA6);	/* A6 : ����������ʾģʽ; A7 : ����Ϊ����ģʽ */

	OLED_WriteCmd(0xA8);	/* ����COM·�� */
	OLED_WriteCmd(0x3F);	/* 1 ->��63+1��· */

	OLED_WriteCmd(0xD3);	/* ������ʾƫ�ƣ�˫�ֽ����*/
	OLED_WriteCmd(0x00);	/* ��ƫ�� */

	OLED_WriteCmd(0xD5);	/* ������ʾʱ�ӷ�Ƶϵ��/��Ƶ�� */
	OLED_WriteCmd(0x80);	/* ���÷�Ƶϵ��,��4bit�Ƿ�Ƶϵ������4bit����Ƶ�� */

	OLED_WriteCmd(0xD9);	/* ����Ԥ������� */
	OLED_WriteCmd(0xF1);	/* [3:0],PHASE 1; [7:4],PHASE 2; */

	OLED_WriteCmd(0xDA);	/* ����COM��Ӳ�����߷�ʽ */
	OLED_WriteCmd(0x12);

	OLED_WriteCmd(0xDB);	/* ���� vcomh ��ѹ���� */
	OLED_WriteCmd(0x40);	/* [6:4] 000 = 0.65 x VCC; 0.77 x VCC (RESET); 0.83 x VCC  */

	OLED_WriteCmd(0x8D);	/* ���ó��ã����¸�������ʹ�ã� */
	OLED_WriteCmd(0x14);	/* 0x14 ʹ�ܳ��ã� 0x10 �ǹر� */
	OLED_WriteCmd(0xAF);	/* ��OLED��� */
}

/*
*********************************************************************************************************
*	�� �� ��: OLED_DispOn
*	����˵��: ����ʾ
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void OLED_DispOn(void)
{
	OLED_WriteCmd(0x8D);	/* ���ó��ã����¸�������ʹ�ã� */
	OLED_WriteCmd(0x14);	/* 0x14 ʹ�ܳ��ã� 0x10 �ǹر� */
	OLED_WriteCmd(0xAF);	/* ��OLED��� */
}

/*
*********************************************************************************************************
*	�� �� ��: OLED_DispOff
*	����˵��: �ر���ʾ
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void OLED_DispOff(void)
{
	OLED_WriteCmd(0x8D);	/* ���ó��ã����¸�������ʹ�ã�*/
	OLED_WriteCmd(0x10);	/* 0x14 ʹ�ܳ��ã�0x10 �ǹر� */
	OLED_WriteCmd(0xAE);	/* ��OLED��� */
}

/*
*********************************************************************************************************
*	�� �� ��: OLED_SetDir
*	����˵��: ������ʾ����
*	��    ��: _ucDir = 0 ��ʾ��������1��ʾ��ת180��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void OLED_SetDir(uint8_t _ucDir)
{
	if (_ucDir == 0)
	{
       	OLED_WriteCmd(0xA0);	/* A0 ���е�ַ0ӳ�䵽SEG0; A1 ���е�ַ127ӳ�䵽SEG0 */
		OLED_WriteCmd(0xC0);	/* C0 ������ɨ��,��COM0��COM63;  C8 : ����ɨ��, �� COM63�� COM0 */
	}
	else
	{
		OLED_WriteCmd(0xA1);	/* A0 ���е�ַ0ӳ�䵽SEG0; A1 ���е�ַ127ӳ�䵽SEG0 */
		OLED_WriteCmd(0xC8);	/* C0 ������ɨ��,��COM0��COM63;  C8 : ����ɨ��, �� COM63�� COM0 */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: OLED_SetContrast
*	����˵��: ���öԱȶ�
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void OLED_SetContrast(uint8_t ucValue)
{
	OLED_WriteCmd(0x81);	/* ���öԱȶ�����(˫�ֽ��������1���ֽ��������2���ֽ��ǶԱȶȲ���0-255 */
	OLED_WriteCmd(ucValue);	/* ���öԱȶȲ���,ȱʡCF */
}

/*
*********************************************************************************************************
*	�� �� ��: OLED_StartDraw
*	����˵��: ��ʼ��ͼ���Ժ��ͼ����ֻ��д������������д����Դ�
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void OLED_StartDraw(void)
{
	s_ucUpdateEn = 0;
}

/*
*********************************************************************************************************
*	�� �� ��: OLED_EndDraw
*	����˵��: ������ͼ��������������ˢ�µ�����Դ档 OLED_StartDraw() �� OLED_EndDraw() ����ɶ�ʹ��
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void OLED_EndDraw(void)
{
	s_ucUpdateEn = 1;
	OLED_BufToPanel();
}

/*
*********************************************************************************************************
*	�� �� ��: OLED_ClrScr
*	����˵��: ����
*	��    ��:  _ucMode : 0 ��ʾȫ�ڣ� 0xFF��ʾȫ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void OLED_ClrScr(uint8_t _ucMode)
{
	uint8_t i,j;

	for (i = 0 ; i < 8; i++)
	{
		for (j = 0 ; j < 128; j++)
		{
			s_ucGRAM[i][j] = _ucMode;
		}
	}

	if (s_ucUpdateEn == 1)
	{
		OLED_BufToPanel();
	}
}

/*
*********************************************************************************************************
*	�� �� ��: OLED_BufToPanel
*	����˵��: ���������еĵ�������д�����
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void OLED_BufToPanel(void)
{
	uint8_t i,j;

	for (i = 0 ; i< 8; i++)
	{
		OLED_WriteCmd (0xB0 + i);	/* ����ҳ��ַ��0~7�� */
		OLED_WriteCmd (0x00);		/* �����е�ַ�ĵ͵�ַ */
		OLED_WriteCmd (0x10);		/* �����е�ַ�ĸߵ�ַ */

		for (j = 0 ; j < 128; j++)
		{
			OLED_WriteData(s_ucGRAM[i][j]);
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: OLED_DispStr
*	����˵��: ����Ļָ�����꣨���Ͻ�Ϊ0��0����ʾһ���ַ���
*	��    ��:
*		_usX : X���꣬����12864������ΧΪ��0 - 127��
*		_usY : Y���꣬����12864������ΧΪ��0 - 63��
*		_ptr  : �ַ���ָ��
*		_tFont : ����ṹ�壬������ɫ������ɫ(֧��͸��)��������롢���ּ��Ȳ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void OLED_DispStr(uint16_t _usX, uint16_t _usY, char *_ptr, FONT_T *_tFont)
{
	uint32_t i;
	uint8_t code1;
	uint8_t code2;
	uint32_t address = 0;
	uint8_t buf[32 * 32 / 8];	/* ���֧��32������ */
	uint8_t m, width;
	uint8_t font_width,font_height, font_bytes;
	uint16_t x, y;
	const uint8_t *pAscDot;	

#ifdef USE_SMALL_FONT		
	const uint8_t *pHzDot;
#else	
	uint32_t AddrHZK;
#endif	

	/* �������ṹΪ��ָ�룬��ȱʡ��16���� */
	if (_tFont->FontCode == FC_ST_12)
	{
		font_height = 12;
		font_width = 12;
		font_bytes = 24;
		pAscDot = g_Ascii12;
	#ifdef USE_SMALL_FONT		
		pHzDot = g_Hz12;
	#else		
		AddrHZK = HZK12_ADDR;
	#endif		
	}
	else
	{
		/* ȱʡ��16���� */
		font_height = 16;
		font_width = 16;
		font_bytes = 32;
		pAscDot = g_Ascii16;
	#ifdef USE_SMALL_FONT		
		pHzDot = g_Hz16;
	#else
		AddrHZK = HZK16_ADDR;
	#endif
	}

	/* ��ʼѭ�������ַ� */
	while (*_ptr != 0)
	{
		code1 = *_ptr;	/* ��ȡ�ַ������ݣ� �����ݿ�����ascii���룬Ҳ���ܺ��ִ���ĸ��ֽ� */
		if (code1 < 0x80)
		{
			/* ��ascii�ַ������Ƶ�buf */
			memcpy(buf, &pAscDot[code1 * (font_bytes / 2)], (font_bytes / 2));
			width = font_width / 2;
		}
		else
		{
			code2 = *++_ptr;
			if (code2 == 0)
			{
				break;
			}

			/* ����16�����ֵ����ַ
				ADDRESS = [(code1-0xa1) * 94 + (code2-0xa1)] * 32
				;
			*/
			#ifdef USE_SMALL_FONT
				m = 0;
				while(1)
				{
					address = m * (font_bytes + 2);
					m++;
					if ((code1 == pHzDot[address + 0]) && (code2 == pHzDot[address + 1]))
					{
						address += 2;
						memcpy(buf, &pHzDot[address], font_bytes);
						break;
					}
					else if ((pHzDot[address + 0] == 0xFF) && (pHzDot[address + 1] == 0xFF))
					{
						/* �ֿ�������ϣ�δ�ҵ��������ȫFF */
						memset(buf, 0xFF, font_bytes);
						break;
					}
				}
			#else	/* ��ȫ�ֿ� */
				/* �˴���Ҫ�����ֿ��ļ����λ�ý����޸� */
				if (code1 >=0xA1 && code1 <= 0xA9 && code2 >=0xA1)
				{
					address = ((code1 - 0xA1) * 94 + (code2 - 0xA1)) * font_bytes + AddrHZK;
				}
				else if (code1 >=0xB0 && code1 <= 0xF7 && code2 >=0xA1)
				{
					address = ((code1 - 0xB0) * 94 + (code2 - 0xA1) + 846) * font_bytes + AddrHZK;
				}
				memcpy(buf, (const uint8_t *)address, font_bytes);
			#endif

				width = font_width;
		}

		y = _usY;
		/* ��ʼˢLCD */
		for (m = 0; m < font_height; m++)	/* �ַ��߶� */
		{
			x = _usX;
			for (i = 0; i < width; i++)	/* �ַ���� */
			{
				if ((buf[m * ((2 * width) / font_width) + i / 8] & (0x80 >> (i % 8 ))) != 0x00)
				{
					OLED_PutPixel(x, y, _tFont->FrontColor);	/* ����������ɫΪ����ɫ */
				}
				else
				{
					OLED_PutPixel(x, y, _tFont->BackColor);	/* ����������ɫΪ���ֱ���ɫ */
				}

				x++;
			}
			y++;
		}

		if (_tFont->Space > 0)
		{
			/* ������ֵ�ɫ��_tFont->usBackColor�������ּ����ڵ���Ŀ�ȣ���ô��Ҫ������֮�����(��ʱδʵ��) */
		}
		_usX += width + _tFont->Space;	/* �е�ַ���� */
		_ptr++;			/* ָ����һ���ַ� */
	}
}

/*
*********************************************************************************************************
*	�� �� ��: OLED_PutPixel
*	����˵��: ��1������
*	��    ��:
*			_usX,_usY : ��������
*			_ucColor  ��������ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void OLED_PutPixel(uint16_t _usX, uint16_t _usY, uint8_t _ucColor)
{
	uint8_t ucValue;
	uint8_t ucPageAddr;
	uint8_t ucColAddr;

	const uint8_t aOrTab[8]  = {0x01, 0x02, 0x04, 0x08,0x10,0x20,0x40,0x80};
	const uint8_t aAndTab[8] = {0xFE, 0xFD, 0xFB, 0xF7,0xEF,0xDF,0xBF,0x7F};

	ucPageAddr = _usY / 8;
	ucColAddr = _usX;

	ucValue = s_ucGRAM[ucPageAddr][ucColAddr];
	if (_ucColor == 0)
	{
		ucValue &= aAndTab[_usY % 8];
	}
	else
	{
		ucValue |= aOrTab[_usY % 8];
	}
	s_ucGRAM[ucPageAddr][ucColAddr] = ucValue;

	if (s_ucUpdateEn == 1)
	{
		OLED_WriteCmd (0xB0 + ucPageAddr);					/* ����ҳ��ַ��0~7�� */
		OLED_WriteCmd (0x00 + (ucColAddr & 0x0F));			/* �����е�ַ�ĵ͵�ַ */
		OLED_WriteCmd (0x10 + ((ucColAddr >> 4) & 0x0F));	/* �����е�ַ�ĸߵ�ַ */
		OLED_WriteData(ucValue);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: OLED_GetPixel
*	����˵��: ��ȡ1������
*	��    ��:
*			_usX,_usY : ��������
*	�� �� ֵ: ��ɫֵ (0, 1)
*********************************************************************************************************
*/
uint8_t OLED_GetPixel(uint16_t _usX, uint16_t _usY)
{
	uint8_t ucValue;
	uint8_t ucPageAddr;
	uint8_t ucColAddr;

	ucPageAddr = _usY / 8;
	ucColAddr = _usX;

	ucValue = s_ucGRAM[ucPageAddr][ucColAddr];
	if (ucValue & (_usY % 8))
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
*	�� �� ��: OLED_DrawLine
*	����˵��: ���� Bresenham �㷨����2��仭һ��ֱ�ߡ�
*	��    ��:
*			_usX1, _usY1 ����ʼ������
*			_usX2, _usY2 ����ֹ��Y����
*			_ucColor     ����ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void OLED_DrawLine(uint16_t _usX1 , uint16_t _usY1 , uint16_t _usX2 , uint16_t _usY2 , uint8_t _ucColor)
{
	int32_t dx , dy ;
	int32_t tx , ty ;
	int32_t inc1 , inc2 ;
	int32_t d , iTag ;
	int32_t x , y ;

	/* ���� Bresenham �㷨����2��仭һ��ֱ�� */

	OLED_PutPixel(_usX1 , _usY1 , _ucColor);

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
			OLED_PutPixel ( y , x , _ucColor) ;
		}
		else
		{
			OLED_PutPixel ( x , y , _ucColor) ;
		}
		x += tx ;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: OLED_DrawPoints
*	����˵��: ���� Bresenham �㷨������һ��㣬������Щ�����������������ڲ�����ʾ��
*	��    ��:
*			x, y     ����������
*			_ucColor ����ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void OLED_DrawPoints(uint16_t *x, uint16_t *y, uint16_t _usSize, uint8_t _ucColor)
{
	uint16_t i;

	for (i = 0 ; i < _usSize - 1; i++)
	{
		OLED_DrawLine(x[i], y[i], x[i + 1], y[i + 1], _ucColor);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: OLED_DrawRect
*	����˵��: ���ƾ��Ρ�
*	��    ��:
*			_usX,_usY���������Ͻǵ�����
*			_usHeight �����εĸ߶�
*			_usWidth  �����εĿ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void OLED_DrawRect(uint16_t _usX, uint16_t _usY, uint8_t _usHeight, uint16_t _usWidth, uint8_t _ucColor)
{
	/*
	 ---------------->---
	|(_usX��_usY)        |
	V                    V  _usHeight
	|                    |
	 ---------------->---
		  _usWidth
	*/

	OLED_DrawLine(_usX, _usY, _usX + _usWidth - 1, _usY, _ucColor);	/* �� */
	OLED_DrawLine(_usX, _usY + _usHeight - 1, _usX + _usWidth - 1, _usY + _usHeight - 1, _ucColor);	/* �� */

	OLED_DrawLine(_usX, _usY, _usX, _usY + _usHeight - 1, _ucColor);	/* �� */
	OLED_DrawLine(_usX + _usWidth - 1, _usY, _usX + _usWidth - 1, _usY + _usHeight, _ucColor);	/* �� */
}

/*
*********************************************************************************************************
*	�� �� ��: OLED_DrawCircle
*	����˵��: ����һ��Բ���ʿ�Ϊ1������
*	��    ��:
*			_usX,_usY  ��Բ�ĵ�����
*			_usRadius  ��Բ�İ뾶
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void OLED_DrawCircle(uint16_t _usX, uint16_t _usY, uint16_t _usRadius, uint8_t _ucColor)
{
	int32_t  D;			/* Decision Variable */
	uint32_t  CurX;		/* ��ǰ X ֵ */
	uint32_t  CurY;		/* ��ǰ Y ֵ */

	D = 3 - (_usRadius << 1);
	CurX = 0;
	CurY = _usRadius;

	while (CurX <= CurY)
	{
		OLED_PutPixel(_usX + CurX, _usY + CurY, _ucColor);
		OLED_PutPixel(_usX + CurX, _usY - CurY, _ucColor);
		OLED_PutPixel(_usX - CurX, _usY + CurY, _ucColor);
		OLED_PutPixel(_usX - CurX, _usY - CurY, _ucColor);
		OLED_PutPixel(_usX + CurY, _usY + CurX, _ucColor);
		OLED_PutPixel(_usX + CurY, _usY - CurX, _ucColor);
		OLED_PutPixel(_usX - CurY, _usY + CurX, _ucColor);
		OLED_PutPixel(_usX - CurY, _usY - CurX, _ucColor);

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
*	�� �� ��: OLED_DrawBMP
*	����˵��: ��LCD����ʾһ��BMPλͼ��λͼ����ɨ����򣺴����ң����ϵ���
*	��    ��:
*			_usX, _usY : ͼƬ������
*			_usHeight  ��ͼƬ�߶�
*			_usWidth   ��ͼƬ���
*			_ptr       ����ɫͼƬ����ָ�룬ÿ������ռ��1���ֽ�
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void OLED_DrawBMP(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint8_t *_ptr)
{
	uint16_t x, y;

	for (x = 0; x < _usWidth; x++)
	{
		for (y = 0; y < _usHeight; y++)
		{
			OLED_PutPixel(_usX + x, _usY + y, *_ptr);
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: OLED_ConfigGPIO
*	����˵��: ����OLED���ƿ��ߣ�����Ϊ8λ80XX���߿���ģʽ��SPIģʽ
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void OLED_ConfigGPIO(void)
{
	/* 12.����GPIO */
	{
		GPIO_InitTypeDef GPIO_InitStructure;

		RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FSMC, ENABLE);

		/* ʹ�� FSMC, GPIOD, GPIOE, GPIOF, GPIOG �� AFIO ʱ�� */
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOE |
							 RCC_APB2Periph_GPIOF | RCC_APB2Periph_GPIOG |
							 RCC_APB2Periph_AFIO, ENABLE);

		/* ���� PD.00(D2), PD.01(D3), PD.04(NOE), PD.05(NWE), PD.08(D13), PD.09(D14),
		 PD.10(D15), PD.14(D0), PD.15(D1) Ϊ����������� */
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5 |
									GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_14 |
									GPIO_Pin_15;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
		GPIO_Init(GPIOD, &GPIO_InitStructure);

		/* ���� PE.07(D4), PE.08(D5), PE.09(D6), PE.10(D7), PE.11(D8), PE.12(D9), PE.13(D10),
		 PE.14(D11), PE.15(D12) Ϊ����������� */
		/* PE3,PE4 ����A19, A20, STM32F103ZE-EK(REV 1.0)����ʹ�� */
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |
									GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 |
									GPIO_Pin_15 | GPIO_Pin_3 | GPIO_Pin_4;
		GPIO_Init(GPIOE, &GPIO_InitStructure);

		/* ���� PF.00(A0 (RS))  Ϊ����������� */
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
		GPIO_Init(GPIOF, &GPIO_InitStructure);

		/* ���� PG.12(NE4 (LCD/CS)) Ϊ����������� */
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
		GPIO_Init(GPIOG, &GPIO_InitStructure);
	}

	/* 2.����FSMC���߲��� */
	{
		FSMC_NORSRAMInitTypeDef  init;
		FSMC_NORSRAMTimingInitTypeDef  timing;

		/*-- FSMC Configuration ------------------------------------------------------*/
		/*----------------------- SRAM Bank 4 ----------------------------------------*/
		/* FSMC_Bank1_NORSRAM4 configuration */
		timing.FSMC_AddressSetupTime = 1;
		timing.FSMC_AddressHoldTime = 0;
		timing.FSMC_DataSetupTime = 2;
		timing.FSMC_BusTurnAroundDuration = 0;
		timing.FSMC_CLKDivision = 0;
		timing.FSMC_DataLatency = 0;
		timing.FSMC_AccessMode = FSMC_AccessMode_A;

		/*
		 LCD configured as follow:
			- Data/Address MUX = Disable
			- Memory Type = SRAM
			- Data Width = 16bit
			- Write Operation = Enable
			- Extended Mode = Enable
			- Asynchronous Wait = Disable
		*/
		init.FSMC_Bank = FSMC_Bank1_NORSRAM4;
		init.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
		init.FSMC_MemoryType = FSMC_MemoryType_SRAM;
		init.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;
		init.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
		init.FSMC_AsynchronousWait = FSMC_AsynchronousWait_Disable;	/* ע��ɿ��������Ա */
		init.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
		init.FSMC_WrapMode = FSMC_WrapMode_Disable;
		init.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
		init.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
		init.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
		init.FSMC_ExtendedMode = FSMC_ExtendedMode_Disable;
		init.FSMC_WriteBurst = FSMC_WriteBurst_Disable;

		init.FSMC_ReadWriteTimingStruct = &timing;
		init.FSMC_WriteTimingStruct = &timing;

		FSMC_NORSRAMInit(&init);

		/* - BANK 3 (of NOR/SRAM Bank 1~4) is enabled */
		FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM4, ENABLE);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: OLED_WriteCmd
*	����˵��: ��SSD1306����һ�ֽ�����
*	��    ��:  ������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void OLED_WriteCmd(uint8_t _ucCmd)
{
#ifdef OLED_SPI3_EN
	uint8_t i;

	SSD_CS_0();

	SSD_SCK_0();
	SSD_SDIN_0();	/* 0 ��ʾ���洫�͵������� 1��ʾ���洫�͵����� */
	SSD_SCK_1();

	for (i = 0; i < 8; i++)
	{
		if (_ucCmd & 0x80)
		{
			SSD_SDIN_1();
		}
		else
		{
			SSD_SDIN_0();
		}
		SSD_SCK_0();
		_ucCmd <<= 1;
		SSD_SCK_1();
	}

	SSD_CS_1();
#else
	OLED_CMD = _ucCmd;
#endif
}

/*
*********************************************************************************************************
*	�� �� ��: OLED_WriteData
*	����˵��: ��SSD1306����һ�ֽ�����
*	��    ��:  ������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void OLED_WriteData(uint8_t _ucData)
{
#ifdef OLED_SPI3_EN
	uint8_t i;

	SSD_CS_0();

	SSD_SCK_0();
	SSD_SDIN_1();	/* 0 ��ʾ���洫�͵������� 1��ʾ���洫�͵����� */
	SSD_SCK_1();

	for (i = 0; i < 8; i++)
	{
		if (_ucData & 0x80)
		{
			SSD_SDIN_1();
		}
		else
		{
			SSD_SDIN_0();
		}
		SSD_SCK_0();
		_ucData <<= 1;
		SSD_SCK_1();
	}

	SSD_CS_1();
#else
	OLED_DATA = _ucData;
#endif
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
