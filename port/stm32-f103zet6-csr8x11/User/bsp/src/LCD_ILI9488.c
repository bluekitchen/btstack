/*
*********************************************************************************************************
*
*	ģ������ : TFTҺ����ʾ������ģ��
*	�ļ����� : LCD_ILI9488.c
*	��    �� : V1.0
*	˵    �� : ILI9488 ��ʾ���ֱ���Ϊ 480 * 320,  3.5����ͨ����4��3
*	�޸ļ�¼ :
*		�汾��  ����       ����    ˵��
*		v1.0    2014-07-26 armfly  �װ�
*
*	Copyright (C), 2014-2015, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"
#include "fonts.h"

#define ILI9488_BASE       ((uint32_t)(0x6C000000 | 0x00000000))

#define ILI9488_REG		*(__IO uint16_t *)(ILI9488_BASE)
#define ILI9488_RAM		*(__IO uint16_t *)(ILI9488_BASE + (1 << (0 + 1)))	/* FSMC 16λ����ģʽ�£�FSMC_A0���߶�Ӧ�����ַA1 */

static __IO uint8_t s_RGBChgEn = 0;		/* RGBת��ʹ��, 4001��д�Դ������RGB��ʽ��д��Ĳ�ͬ */

static void Init_9488(void);
static void ILI9488_SetDispWin(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth);
static void ILI9488_QuitWinMode(void);
static void ILI9488_SetCursor(uint16_t _usX, uint16_t _usY);

static void ILI9488_WriteCmd(uint8_t _ucCmd);
static void ILI9488_WriteParam(uint8_t _ucParam);

/*
*********************************************************************************************************
*	�� �� ��: ILI9488_InitHard
*	����˵��: ��ʼ��LCD
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ILI9488_InitHard(void)
{
	uint32_t id;

	id = ILI9488_ReadID();

	if (id == 0x548066)
	{
		Init_9488();	/* ��ʼ��5420��4001��Ӳ�� */

		//ILI9488_WriteCmd(0x23);
		//ILI9488_WriteCmd(0x22);

		s_RGBChgEn = 0;

		ILI9488_PutPixel(1,1, 0x12);
		g_ChipID = ILI9488_GetPixel(1,1);

		ILI9488_PutPixel(1,1, 0x34);
		g_ChipID = ILI9488_GetPixel(1,1);

		ILI9488_PutPixel(1,1, 0x56);
		g_ChipID = ILI9488_GetPixel(1,1);

		g_ChipID = IC_9488;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: ILI9488_SetDirection
*	����˵��: ������ʾ����
*	��    ��:  _ucDir : ��ʾ������� 0 ��������, 1=����180�ȷ�ת, 2=����, 3=����180�ȷ�ת
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ILI9488_SetDirection(uint8_t _ucDir)
{
	
	/*
		Memory Access Control (36h)
		This command defines read/write scanning direction of the frame memory.

		These 3 bits control the direction from the MPU to memory write/read.

		Bit  Symbol  Name  Description
		D7   MY  Row Address Order
		D6   MX  Column Address Order
		D5   MV  Row/Column Exchange
		D4   ML  Vertical Refresh Order  LCD vertical refresh direction control. ��

		D3   BGR RGB-BGR Order   Color selector switch control
		     (0 = RGB color filter panel, 1 = BGR color filter panel )
		D2   MH  Horizontal Refresh ORDER  LCD horizontal refreshing direction control.
		D1   X   Reserved  Reserved
		D0   X   Reserved  Reserved
	*/
	ILI9488_WriteCmd(0x36);
	/* 0 ��ʾ����(��������)��1��ʾ����(��������), 2��ʾ����(���������)  3��ʾ���� (�������ұ�) */
	if (_ucDir == 0)
	{
		ILI9488_WriteParam(0xA8);	/* ����(���������) */
		g_LcdHeight = 320;
		g_LcdWidth = 480;
	}
	else if (_ucDir == 1)
	{
		ILI9488_WriteParam(0x68);	/* ���� (�������ұ�) */
		g_LcdHeight = 320;
		g_LcdWidth = 480;
	}
	else if (_ucDir == 2)
	{
		ILI9488_WriteParam(0xC8);	/* ����(��������) */
		g_LcdHeight = 480;
		g_LcdWidth = 320;
	}
	else if (_ucDir == 3)
	{
		ILI9488_WriteParam(0x08);	/* ����(��������) */
		g_LcdHeight = 480;
		g_LcdWidth = 320;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: Init_9488
*	����˵��: ��ʼ��ILI9488������
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void Init_9488(void)
{
	/* ��ʼ��LCD��дLCD�Ĵ����������� */

#if 0
	// VCI=2.8V
	//************* Reset LCD Driver ****************//
	LCD_nRESET = 1;
	Delayms(1); // Delay 1ms
	LCD_nRESET = 0;
	Delayms(10); // Delay 10ms // This delay time is necessary
	LCD_nRESET = 1;
	Delayms(120); // Delay 100 ms
#endif

	//************* Start Initial Sequence **********//
	/* Adjust Control 3 (F7h)  */
	ILI9488_WriteCmd(0XF7);
	ILI9488_WriteParam(0xA9);
	ILI9488_WriteParam(0x51);
	ILI9488_WriteParam(0x2C);
	ILI9488_WriteParam(0x82);	/* DSI write DCS command, use loose packet RGB 666 */

	/* Power Control 1 (C0h)  */
	ILI9488_WriteCmd(0xC0);
	ILI9488_WriteParam(0x11);
	ILI9488_WriteParam(0x09);

	/* Power Control 2 (C1h) */
	ILI9488_WriteCmd(0xC1);
	ILI9488_WriteParam(0x41);

	/* VCOM Control (C5h)  */
	ILI9488_WriteCmd(0XC5);
	ILI9488_WriteParam(0x00);
	ILI9488_WriteParam(0x0A);
	ILI9488_WriteParam(0x80);

	/* Frame Rate Control (In Normal Mode/Full Colors) (B1h) */
	ILI9488_WriteCmd(0xB1);
	ILI9488_WriteParam(0xB0);
	ILI9488_WriteParam(0x11);

	/* Display Inversion Control (B4h) */
	ILI9488_WriteCmd(0xB4);
	ILI9488_WriteParam(0x02);

	/* Display Function Control (B6h)  */
	ILI9488_WriteCmd(0xB6);
	ILI9488_WriteParam(0x02);
	ILI9488_WriteParam(0x22);

	/* Entry Mode Set (B7h)  */
	ILI9488_WriteCmd(0xB7);
	ILI9488_WriteParam(0xc6);

	/* HS Lanes Control (BEh) */
	ILI9488_WriteCmd(0xBE);
	ILI9488_WriteParam(0x00);
	ILI9488_WriteParam(0x04);

	/* Set Image Function (E9h)  */
	ILI9488_WriteCmd(0xE9);
	ILI9488_WriteParam(0x00);

	ILI9488_SetDirection(0);	/* ����(���������) */

	/* Interface Pixel Format (3Ah) */
	ILI9488_WriteCmd(0x3A);
	ILI9488_WriteParam(0x55);	/* 0x55 : 16 bits/pixel  */

	/* PGAMCTRL (Positive Gamma Control) (E0h) */
	ILI9488_WriteCmd(0xE0);
	ILI9488_WriteParam(0x00);
	ILI9488_WriteParam(0x07);
	ILI9488_WriteParam(0x10);
	ILI9488_WriteParam(0x09);
	ILI9488_WriteParam(0x17);
	ILI9488_WriteParam(0x0B);
	ILI9488_WriteParam(0x41);
	ILI9488_WriteParam(0x89);
	ILI9488_WriteParam(0x4B);
	ILI9488_WriteParam(0x0A);
	ILI9488_WriteParam(0x0C);
	ILI9488_WriteParam(0x0E);
	ILI9488_WriteParam(0x18);
	ILI9488_WriteParam(0x1B);
	ILI9488_WriteParam(0x0F);

	/* NGAMCTRL (Negative Gamma Control) (E1h)  */
	ILI9488_WriteCmd(0XE1);
	ILI9488_WriteParam(0x00);
	ILI9488_WriteParam(0x17);
	ILI9488_WriteParam(0x1A);
	ILI9488_WriteParam(0x04);
	ILI9488_WriteParam(0x0E);
	ILI9488_WriteParam(0x06);
	ILI9488_WriteParam(0x2F);
	ILI9488_WriteParam(0x45);
	ILI9488_WriteParam(0x43);
	ILI9488_WriteParam(0x02);
	ILI9488_WriteParam(0x0A);
	ILI9488_WriteParam(0x09);
	ILI9488_WriteParam(0x32);
	ILI9488_WriteParam(0x36);
	ILI9488_WriteParam(0x0F);

	/* Sleep Out (11h */
	ILI9488_WriteCmd(0x11);
	bsp_DelayMS(120);
	ILI9488_WriteCmd(0x29);	/* Display ON (29h) */

#if 1
	/* ������ʾ���� */
	ILI9488_SetDispWin(0, 0, g_LcdHeight, g_LcdWidth);
#endif
}

/*
*********************************************************************************************************
*	�� �� ��: ILI9488_WriteCmd
*	����˵��: ��LCD������оƬ��������
*	��    ��: _ucCmd : �������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void ILI9488_WriteCmd(uint8_t _ucCmd)
{
	ILI9488_REG = _ucCmd;	/* ����CMD */
}


/*
*********************************************************************************************************
*	�� �� ��: ILI9488_WriteParam
*	����˵��: ��LCD������оƬ���Ͳ���(data)
*	��    ��: _ucParam : ��������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void ILI9488_WriteParam(uint8_t _ucParam)
{
	ILI9488_RAM = _ucParam;
}

/*
*********************************************************************************************************
*	�� �� ��: ILI9488_SetDispWin
*	����˵��: ������ʾ���ڣ����봰����ʾģʽ��TFT����оƬ֧�ִ�����ʾģʽ�����Ǻ�һ���12864����LCD���������
*	��    ��:
*		_usX : ˮƽ����
*		_usY : ��ֱ����
*		_usHeight: ���ڸ߶�
*		_usWidth : ���ڿ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void ILI9488_SetDispWin(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth)
{
	ILI9488_WriteCmd(0X2A); 		/* ����X���� */
	ILI9488_WriteParam(_usX >> 8);	/* ��ʼ�� */
	ILI9488_WriteParam(_usX);
	ILI9488_WriteParam((_usX + _usWidth - 1) >> 8);	/* ������ */
	ILI9488_WriteParam(_usX + _usWidth - 1);

	ILI9488_WriteCmd(0X2B); 				  /* ����Y����*/
	ILI9488_WriteParam(_usY >> 8);   /* ��ʼ�� */
	ILI9488_WriteParam(_usY);
	ILI9488_WriteParam((_usY + _usHeight - 1) >>8);		/* ������ */
	ILI9488_WriteParam((_usY + _usHeight - 1));
}

/*
*********************************************************************************************************
*	�� �� ��: ILI9488_SetCursor
*	����˵��: ���ù��λ��
*	��    ��:  _usX : X����; _usY: Y����
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void ILI9488_SetCursor(uint16_t _usX, uint16_t _usY)
{
	ILI9488_WriteCmd(0X2A); 		/* ����X���� */
	ILI9488_WriteParam(_usX >> 8);	/* �ȸ�8λ��Ȼ���8λ */
	ILI9488_WriteParam(_usX);		/* ������ʼ��ͽ�����*/
	ILI9488_WriteParam(_usX >> 8);	/* �ȸ�8λ��Ȼ���8λ */
	ILI9488_WriteParam(_usX);		/* ������ʼ��ͽ�����*/

    ILI9488_WriteCmd(0X2B); 		/* ����Y����*/
	ILI9488_WriteParam(_usY >> 8);
	ILI9488_WriteParam(_usY);
	ILI9488_WriteParam(_usY >> 8);
	ILI9488_WriteParam(_usY);
}

/*
*********************************************************************************************************
*	�� �� ��: ILI9488_QuitWinMode
*	����˵��: �˳�������ʾģʽ����Ϊȫ����ʾģʽ
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void ILI9488_QuitWinMode(void)
{
	ILI9488_SetDispWin(0, 0, g_LcdHeight, g_LcdWidth);
}

/*
*********************************************************************************************************
*	�� �� ��: ILI9488_ReadID
*	����˵��: ��ȡLCD����оƬID�� 4��bit
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
uint32_t ILI9488_ReadID(void)
{
	uint8_t buf[4];

	ILI9488_REG = 0x04;
	buf[0] = ILI9488_RAM;
	buf[1] = ILI9488_RAM;
	buf[2] = ILI9488_RAM;
	buf[3] = ILI9488_RAM;

	return (buf[1] << 16) + (buf[2] << 8) + buf[3];
}

/*
*********************************************************************************************************
*	�� �� ��: ILI9488_DispOn
*	����˵��: ����ʾ
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ILI9488_DispOn(void)
{
	;
}

/*
*********************************************************************************************************
*	�� �� ��: ILI9488_DispOff
*	����˵��: �ر���ʾ
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ILI9488_DispOff(void)
{
	;
}

/*
*********************************************************************************************************
*	�� �� ��: ILI9488_ClrScr
*	����˵��: �����������ɫֵ����
*	��    ��:  _usColor : ����ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ILI9488_ClrScr(uint16_t _usColor)
{
	uint32_t i;
	uint32_t n;

	ILI9488_SetDispWin(0, 0, g_LcdHeight, g_LcdWidth);

	ILI9488_REG = 0x2C; 			/* ׼����д�Դ� */

#if 1		/* �Ż�����ִ���ٶ� */
	n = (g_LcdHeight * g_LcdWidth) / 8;
	for (i = 0; i < n; i++)
	{
		ILI9488_RAM = _usColor;
		ILI9488_RAM = _usColor;
		ILI9488_RAM = _usColor;
		ILI9488_RAM = _usColor;

		ILI9488_RAM = _usColor;
		ILI9488_RAM = _usColor;
		ILI9488_RAM = _usColor;
		ILI9488_RAM = _usColor;
	}
#else
	n = g_LcdHeight * g_LcdWidth;
	for (i = 0; i < n; i++)
	{
		ILI9488_RAM = _usColor;
	}
#endif

}

/*
*********************************************************************************************************
*	�� �� ��: ILI9488_PutPixel
*	����˵��: ��1������
*	��    ��:
*			_usX,_usY : ��������
*			_usColor  ��������ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ILI9488_PutPixel(uint16_t _usX, uint16_t _usY, uint16_t _usColor)
{
	ILI9488_SetCursor(_usX, _usY);	/* ���ù��λ�� */

	/* д�Դ� */
	ILI9488_REG = 0x2C;
	ILI9488_RAM = _usColor;
}

/*
*********************************************************************************************************
*	�� �� ��: ILI9488_GetPixel
*	����˵��: ��ȡ1������
*	��    ��:
*			_usX,_usY : ��������
*			_usColor  ��������ɫ
*	�� �� ֵ: RGB��ɫֵ ��565��
*********************************************************************************************************
*/
uint16_t ILI9488_GetPixel(uint16_t _usX, uint16_t _usY)
{
	uint16_t R = 0, G = 0, B = 0 ;

	ILI9488_SetCursor(_usX, _usY);	/* ���ù��λ�� */

	ILI9488_REG = 0x2E;
	R = ILI9488_RAM; 	/* ��1���ƶ������� */
	R = ILI9488_RAM;
	B = ILI9488_RAM;
	G = ILI9488_RAM;

    return (((R >> 11) << 11) | ((G >> 10 ) << 5) | (B >> 11));
}

/*
*********************************************************************************************************
*	�� �� ��: ILI9488_DrawLine
*	����˵��: ���� Bresenham �㷨����2��仭һ��ֱ�ߡ�
*	��    ��:
*			_usX1, _usY1 ����ʼ������
*			_usX2, _usY2 ����ֹ��Y����
*			_usColor     ����ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ILI9488_DrawLine(uint16_t _usX1 , uint16_t _usY1 , uint16_t _usX2 , uint16_t _usY2 , uint16_t _usColor)
{
	int32_t dx , dy ;
	int32_t tx , ty ;
	int32_t inc1 , inc2 ;
	int32_t d , iTag ;
	int32_t x , y ;

	/* ���� Bresenham �㷨����2��仭һ��ֱ�� */

	ILI9488_PutPixel(_usX1 , _usY1 , _usColor);

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
			ILI9488_PutPixel ( y , x , _usColor) ;
		}
		else
		{
			ILI9488_PutPixel ( x , y , _usColor) ;
		}
		x += tx ;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: ILI9488_DrawHLine
*	����˵��: ����һ��ˮƽ�� ����Ҫ����UCGUI�Ľӿں�����
*	��    ��:  _usX1    ����ʼ��X����
*			  _usY1    ��ˮƽ�ߵ�Y����
*			  _usX2    ��������X����
*			  _usColor : ��ɫ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ILI9488_DrawHLine(uint16_t _usX1 , uint16_t _usY1 , uint16_t _usX2 , uint16_t _usColor)
{
	uint16_t i;

	ILI9488_SetDispWin(_usX1, _usY1, 1, _usX2 - _usX1 + 1);

	ILI9488_REG = 0x2C;

	/* д�Դ� */
	for (i = 0; i <_usX2-_usX1 + 1; i++)
	{
		ILI9488_RAM = _usColor;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: ILI9488_DrawHColorLine
*	����˵��: ����һ����ɫˮƽ�� ����Ҫ����UCGUI�Ľӿں�����
*	��    ��:  _usX1    ����ʼ��X����
*			  _usY1    ��ˮƽ�ߵ�Y����
*			  _usWidth ��ֱ�ߵĿ��
*			  _pColor : ��ɫ������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ILI9488_DrawHColorLine(uint16_t _usX1 , uint16_t _usY1, uint16_t _usWidth, const uint16_t *_pColor)
{
	uint16_t i;

	ILI9488_SetCursor(_usX1, _usY1);

	/* д�Դ� */
	ILI9488_REG = 0x2C;
	for (i = 0; i < _usWidth; i++)
	{
		ILI9488_RAM = *_pColor++;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: ILI9488_DrawHTransLine
*	����˵��: ����һ����ɫ͸����ˮƽ�� ����Ҫ����UCGUI�Ľӿں������� ��ɫֵΪ0��ʾ͸��ɫ
*	��    ��:  _usX1    ����ʼ��X����
*			  _usY1    ��ˮƽ�ߵ�Y����
*			  _usWidth ��ֱ�ߵĿ��
*			  _pColor : ��ɫ������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ILI9488_DrawHTransLine(uint16_t _usX1 , uint16_t _usY1, uint16_t _usWidth, const uint16_t *_pColor)
{
	uint16_t i, j;
	uint16_t Index;

	ILI9488_SetCursor(_usX1, _usY1);

	/* д�Դ� */
	ILI9488_REG = 0x2C;
	for (i = 0,j = 0; i < _usWidth; i++, j++)
	{
		Index = *_pColor++;
	    if (Index)
        {
			 ILI9488_RAM = Index;
		}
		else
		{
			ILI9488_SetCursor(_usX1 + j, _usY1);
			ILI9488_REG = 0x2C;
			ILI9488_RAM = Index;
		}
	}
}

/*
*********************************************************************************************************
*	�� �� ��: ILI9488_DrawRect
*	����˵��: ����ˮƽ���õľ��Ρ�
*	��    ��:
*			_usX,_usY���������Ͻǵ�����
*			_usHeight �����εĸ߶�
*			_usWidth  �����εĿ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ILI9488_DrawRect(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t _usColor)
{
	/*
	 ---------------->---
	|(_usX��_usY)        |
	V                    V  _usHeight
	|                    |
	 ---------------->---
		  _usWidth
	*/

	ILI9488_DrawLine(_usX, _usY, _usX + _usWidth - 1, _usY, _usColor);	/* �� */
	ILI9488_DrawLine(_usX, _usY + _usHeight - 1, _usX + _usWidth - 1, _usY + _usHeight - 1, _usColor);	/* �� */

	ILI9488_DrawLine(_usX, _usY, _usX, _usY + _usHeight - 1, _usColor);	/* �� */
	ILI9488_DrawLine(_usX + _usWidth - 1, _usY, _usX + _usWidth - 1, _usY + _usHeight, _usColor);	/* �� */
}

/*
*********************************************************************************************************
*	�� �� ��: ILI9488_FillRect
*	����˵��: �����Ρ�
*	��    ��:
*			_usX,_usY���������Ͻǵ�����
*			_usHeight �����εĸ߶�
*			_usWidth  �����εĿ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ILI9488_FillRect(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t _usColor)
{
	uint32_t i;

	/*
	 ---------------->---
	|(_usX��_usY)        |
	V                    V  _usHeight
	|                    |
	 ---------------->---
		  _usWidth
	*/

	ILI9488_SetDispWin(_usX, _usY, _usHeight, _usWidth);

	ILI9488_REG = 0x2C;
	for (i = 0; i < _usHeight * _usWidth; i++)
	{
		ILI9488_RAM = _usColor;
	}
}


/*
*********************************************************************************************************
*	�� �� ��: ILI9488_DrawCircle
*	����˵��: ����һ��Բ���ʿ�Ϊ1������
*	��    ��:
*			_usX,_usY  ��Բ�ĵ�����
*			_usRadius  ��Բ�İ뾶
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ILI9488_DrawCircle(uint16_t _usX, uint16_t _usY, uint16_t _usRadius, uint16_t _usColor)
{
	int32_t  D;			/* Decision Variable */
	uint32_t  CurX;		/* ��ǰ X ֵ */
	uint32_t  CurY;		/* ��ǰ Y ֵ */

	D = 3 - (_usRadius << 1);
	CurX = 0;
	CurY = _usRadius;

	while (CurX <= CurY)
	{
		ILI9488_PutPixel(_usX + CurX, _usY + CurY, _usColor);
		ILI9488_PutPixel(_usX + CurX, _usY - CurY, _usColor);
		ILI9488_PutPixel(_usX - CurX, _usY + CurY, _usColor);
		ILI9488_PutPixel(_usX - CurX, _usY - CurY, _usColor);
		ILI9488_PutPixel(_usX + CurY, _usY + CurX, _usColor);
		ILI9488_PutPixel(_usX + CurY, _usY - CurX, _usColor);
		ILI9488_PutPixel(_usX - CurY, _usY + CurX, _usColor);
		ILI9488_PutPixel(_usX - CurY, _usY - CurX, _usColor);

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
*	�� �� ��: ILI9488_DrawBMP
*	����˵��: ��LCD����ʾһ��BMPλͼ��λͼ����ɨ����򣺴����ң����ϵ���
*	��    ��:
*			_usX, _usY : ͼƬ������
*			_usHeight  ��ͼƬ�߶�
*			_usWidth   ��ͼƬ���
*			_ptr       ��ͼƬ����ָ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void ILI9488_DrawBMP(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t *_ptr)
{
	uint32_t index = 0;
	const uint16_t *p;

	/* ����ͼƬ��λ�úʹ�С�� ��������ʾ���� */
	ILI9488_SetDispWin(_usX, _usY, _usHeight, _usWidth);

	p = _ptr;
	for (index = 0; index < _usHeight * _usWidth; index++)
	{
		ILI9488_PutPixel(_usX, _usY, *p++);
	}

	/* �˳����ڻ�ͼģʽ */
	ILI9488_QuitWinMode();
}


/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
