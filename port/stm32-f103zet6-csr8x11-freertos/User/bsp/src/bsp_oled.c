/*
*********************************************************************************************************
*
*	模块名称 : OLED显示器驱动模块
*	文件名称 : bsp_oled.c
*	版    本 : V1.0
*	说    明 : OLED 屏(128x64)底层驱动程序，驱动芯片型号为 SSD1306.  本驱动程序适合于STM32-V5开发板.
*				OLED模块挂在FSMC总线上。
*	修改记录 :
*		版本号  日期        作者    说明
*		V1.0    2013-02-01 armfly  正式发布
*
*	Copyright (C), 2010-2012, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"
#include "fonts.h"

/*
	安富莱STM32-V4开发板的OLED显示接口为FSMC总线模式。
*/

/* 下面2个宏任选 1个; 表示显示方向 */
#define DIR_NORMAL			/* 此行表示正常显示方向 */
//#define DIR_180				/* 此行表示翻转180度 */

#ifdef OLED_SPI3_EN
	/*
		SPI 模式接线定义 (只需要6根杜邦线连接)  本例子采用软件模拟SPI时序

	   【OLED模块排针】 【开发板TFT接口（STM32口线）】
	        VCC ----------- 3.3V
	        GND ----------- GND
	         CS ----------- TP_NCS   (PI10)
	        RST ----------- NRESET (也可以不连接)
	     D0/SCK ----------- TP_SCK   (PB3)
	    D1/SDIN ----------- TP_MOSI  (PB5)
	*/
	#define RCC_OLED_PORT (RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOI)		/* GPIO端口时钟 */

	#define OLED_CS_PORT	GPIOI
	#define OLED_CS_PIN		GPIO_Pin_10

	#define OLED_SCK_PORT	GPIOB
	#define OLED_SCK_PIN	GPIO_Pin_3

	#define OLED_SDIN_PORT	GPIOB
	#define OLED_SDIN_PIN	GPIO_Pin_5


	/* 定义IO = 1和 0的代码 （不用更改） */
	#define SSD_CS_1()		OLED_CS_PORT->BSRRL = OLED_CS_PIN
	#define SSD_CS_0()		OLED_CS_PORT->BSRRH = OLED_CS_PIN

	#define SSD_SCK_1()		OLED_SCK_PORT->BSRRL = OLED_SCK_PIN
	#define SSD_SCK_0()		OLED_SCK_PORT->BSRRH = OLED_SCK_PIN

	#define SSD_SDIN_1()	OLED_SDIN_PORT->BSRRL = OLED_SDIN_PIN
	#define SSD_SDIN_0()	OLED_SDIN_PORT->BSRRH = OLED_SDIN_PIN

#else

	/* 定义LCD驱动器的访问地址
		TFT接口中的RS引脚连接FSMC_A0引脚，由于是16bit模式，RS对应A1地址线，因此
		OLED_RAM的地址是+2
	*/
	#define OLED_BASE       ((uint32_t)(0x6C200000))
	#define OLED_CMD		*(__IO uint16_t *)(OLED_BASE)
	#define OLED_DATA		*(__IO uint16_t *)(OLED_BASE + (1 << 1))		/* FSMC_A0接D/C */

	/*
	1.安富莱OLED模块 80XX 模式接线定义 (需要15根杜邦线连接）
	   【OLED模块排针】 【开发板TFT接口（STM32口线）】
		    VCC ----------- 3.3V
		    GND ----------- GND
		     CS ----------- NCS
	        RST ----------- NRESET (也可以不连接)
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

/* 12864 OLED的显存镜像，占用1K字节. 共8行，每行128像素 */
static uint8_t s_ucGRAM[8][128];

/* 为了避免刷屏拉幕感太强，引入刷屏标志。
0 表示显示函数只改写缓冲区，不写屏。1 表示直接写屏（同时写缓冲区） */
static uint8_t s_ucUpdateEn = 1;

static void OLED_ConfigGPIO(void);
static void OLED_WriteCmd(uint8_t _ucCmd);
static void OLED_WriteData(uint8_t _ucData);
static void OLED_BufToPanel(void);

/*
*********************************************************************************************************
*	函 数 名: OLED_InitHard
*	功能说明: 初始化OLED屏
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void OLED_InitHard(void)
{
	OLED_ConfigGPIO();

	/* 上电延迟 */
	bsp_DelayMS(50);

	 /* 模块厂家提供初始化代码 */
	OLED_WriteCmd(0xAE);	/* 关闭OLED面板显示(休眠) */
	OLED_WriteCmd(0x00);	/* 设置列地址低4bit */
	OLED_WriteCmd(0x10);	/* 设置列地址高4bit */
	OLED_WriteCmd(0x40);	/* 设置起始行地址（低5bit 0-63）， 硬件相关*/

	OLED_WriteCmd(0x81);	/* 设置对比度命令(双字节命令），第1个字节是命令，第2个字节是对比度参数0-255 */
	OLED_WriteCmd(0xCF);	/* 设置对比度参数,缺省CF */

#ifdef DIR_NORMAL
	OLED_WriteCmd(0xA0);	/* A0 ：列地址0映射到SEG0; A1 ：列地址127映射到SEG0 */
	OLED_WriteCmd(0xC0);	/* C0 ：正常扫描,从COM0到COM63;  C8 : 反向扫描, 从 COM63至 COM0 */
#endif

#ifdef DIR_180
	OLED_WriteCmd(0xA1);	/* A0 ：列地址0映射到SEG0; A1 ：列地址127映射到SEG0 */
	OLED_WriteCmd(0xC8);	/* C0 ：正常扫描,从COM0到COM63;  C8 : 反向扫描, 从 COM63至 COM0 */
#endif

	OLED_WriteCmd(0xA6);	/* A6 : 设置正常显示模式; A7 : 设置为反显模式 */

	OLED_WriteCmd(0xA8);	/* 设置COM路数 */
	OLED_WriteCmd(0x3F);	/* 1 ->（63+1）路 */

	OLED_WriteCmd(0xD3);	/* 设置显示偏移（双字节命令）*/
	OLED_WriteCmd(0x00);	/* 无偏移 */

	OLED_WriteCmd(0xD5);	/* 设置显示时钟分频系数/振荡频率 */
	OLED_WriteCmd(0x80);	/* 设置分频系数,高4bit是分频系数，低4bit是振荡频率 */

	OLED_WriteCmd(0xD9);	/* 设置预充电周期 */
	OLED_WriteCmd(0xF1);	/* [3:0],PHASE 1; [7:4],PHASE 2; */

	OLED_WriteCmd(0xDA);	/* 设置COM脚硬件接线方式 */
	OLED_WriteCmd(0x12);

	OLED_WriteCmd(0xDB);	/* 设置 vcomh 电压倍率 */
	OLED_WriteCmd(0x40);	/* [6:4] 000 = 0.65 x VCC; 0.77 x VCC (RESET); 0.83 x VCC  */

	OLED_WriteCmd(0x8D);	/* 设置充电泵（和下个命令结合使用） */
	OLED_WriteCmd(0x14);	/* 0x14 使能充电泵， 0x10 是关闭 */
	OLED_WriteCmd(0xAF);	/* 打开OLED面板 */
}

/*
*********************************************************************************************************
*	函 数 名: OLED_DispOn
*	功能说明: 打开显示
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void OLED_DispOn(void)
{
	OLED_WriteCmd(0x8D);	/* 设置充电泵（和下个命令结合使用） */
	OLED_WriteCmd(0x14);	/* 0x14 使能充电泵， 0x10 是关闭 */
	OLED_WriteCmd(0xAF);	/* 打开OLED面板 */
}

/*
*********************************************************************************************************
*	函 数 名: OLED_DispOff
*	功能说明: 关闭显示
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void OLED_DispOff(void)
{
	OLED_WriteCmd(0x8D);	/* 设置充电泵（和下个命令结合使用）*/
	OLED_WriteCmd(0x10);	/* 0x14 使能充电泵，0x10 是关闭 */
	OLED_WriteCmd(0xAE);	/* 打开OLED面板 */
}

/*
*********************************************************************************************************
*	函 数 名: OLED_SetDir
*	功能说明: 设置显示方向
*	形    参: _ucDir = 0 表示正常方向，1表示翻转180度
*	返 回 值: 无
*********************************************************************************************************
*/
void OLED_SetDir(uint8_t _ucDir)
{
	if (_ucDir == 0)
	{
       	OLED_WriteCmd(0xA0);	/* A0 ：列地址0映射到SEG0; A1 ：列地址127映射到SEG0 */
		OLED_WriteCmd(0xC0);	/* C0 ：正常扫描,从COM0到COM63;  C8 : 反向扫描, 从 COM63至 COM0 */
	}
	else
	{
		OLED_WriteCmd(0xA1);	/* A0 ：列地址0映射到SEG0; A1 ：列地址127映射到SEG0 */
		OLED_WriteCmd(0xC8);	/* C0 ：正常扫描,从COM0到COM63;  C8 : 反向扫描, 从 COM63至 COM0 */
	}
}

/*
*********************************************************************************************************
*	函 数 名: OLED_SetContrast
*	功能说明: 设置对比度
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void OLED_SetContrast(uint8_t ucValue)
{
	OLED_WriteCmd(0x81);	/* 设置对比度命令(双字节命令），第1个字节是命令，第2个字节是对比度参数0-255 */
	OLED_WriteCmd(ucValue);	/* 设置对比度参数,缺省CF */
}

/*
*********************************************************************************************************
*	函 数 名: OLED_StartDraw
*	功能说明: 开始绘图。以后绘图函数只改写缓冲区，不改写面板显存
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void OLED_StartDraw(void)
{
	s_ucUpdateEn = 0;
}

/*
*********************************************************************************************************
*	函 数 名: OLED_EndDraw
*	功能说明: 结束绘图。缓冲区的数据刷新到面板显存。 OLED_StartDraw() 和 OLED_EndDraw() 必须成对使用
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void OLED_EndDraw(void)
{
	s_ucUpdateEn = 1;
	OLED_BufToPanel();
}

/*
*********************************************************************************************************
*	函 数 名: OLED_ClrScr
*	功能说明: 清屏
*	形    参:  _ucMode : 0 表示全黑； 0xFF表示全亮
*	返 回 值: 无
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
*	函 数 名: OLED_BufToPanel
*	功能说明: 将缓冲区中的点阵数据写入面板
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
static void OLED_BufToPanel(void)
{
	uint8_t i,j;

	for (i = 0 ; i< 8; i++)
	{
		OLED_WriteCmd (0xB0 + i);	/* 设置页地址（0~7） */
		OLED_WriteCmd (0x00);		/* 设置列地址的低地址 */
		OLED_WriteCmd (0x10);		/* 设置列地址的高地址 */

		for (j = 0 ; j < 128; j++)
		{
			OLED_WriteData(s_ucGRAM[i][j]);
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: OLED_DispStr
*	功能说明: 在屏幕指定坐标（左上角为0，0）显示一个字符串
*	形    参:
*		_usX : X坐标，对于12864屏，范围为【0 - 127】
*		_usY : Y坐标，对于12864屏，范围为【0 - 63】
*		_ptr  : 字符串指针
*		_tFont : 字体结构体，包含颜色、背景色(支持透明)、字体代码、文字间距等参数
*	返 回 值: 无
*********************************************************************************************************
*/
void OLED_DispStr(uint16_t _usX, uint16_t _usY, char *_ptr, FONT_T *_tFont)
{
	uint32_t i;
	uint8_t code1;
	uint8_t code2;
	uint32_t address = 0;
	uint8_t buf[32 * 32 / 8];	/* 最大支持32点阵汉字 */
	uint8_t m, width;
	uint8_t font_width,font_height, font_bytes;
	uint16_t x, y;
	const uint8_t *pAscDot;	

#ifdef USE_SMALL_FONT		
	const uint8_t *pHzDot;
#else	
	uint32_t AddrHZK;
#endif	

	/* 如果字体结构为空指针，则缺省按16点阵 */
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
		/* 缺省是16点阵 */
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

	/* 开始循环处理字符 */
	while (*_ptr != 0)
	{
		code1 = *_ptr;	/* 读取字符串数据， 该数据可能是ascii代码，也可能汉字代码的高字节 */
		if (code1 < 0x80)
		{
			/* 将ascii字符点阵复制到buf */
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

			/* 计算16点阵汉字点阵地址
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
						/* 字库搜索完毕，未找到，则填充全FF */
						memset(buf, 0xFF, font_bytes);
						break;
					}
				}
			#else	/* 用全字库 */
				/* 此处需要根据字库文件存放位置进行修改 */
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
		/* 开始刷LCD */
		for (m = 0; m < font_height; m++)	/* 字符高度 */
		{
			x = _usX;
			for (i = 0; i < width; i++)	/* 字符宽度 */
			{
				if ((buf[m * ((2 * width) / font_width) + i / 8] & (0x80 >> (i % 8 ))) != 0x00)
				{
					OLED_PutPixel(x, y, _tFont->FrontColor);	/* 设置像素颜色为文字色 */
				}
				else
				{
					OLED_PutPixel(x, y, _tFont->BackColor);	/* 设置像素颜色为文字背景色 */
				}

				x++;
			}
			y++;
		}

		if (_tFont->Space > 0)
		{
			/* 如果文字底色按_tFont->usBackColor，并且字间距大于点阵的宽度，那么需要在文字之间填充(暂时未实现) */
		}
		_usX += width + _tFont->Space;	/* 列地址递增 */
		_ptr++;			/* 指向下一个字符 */
	}
}

/*
*********************************************************************************************************
*	函 数 名: OLED_PutPixel
*	功能说明: 画1个像素
*	形    参:
*			_usX,_usY : 像素坐标
*			_ucColor  ：像素颜色
*	返 回 值: 无
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
		OLED_WriteCmd (0xB0 + ucPageAddr);					/* 设置页地址（0~7） */
		OLED_WriteCmd (0x00 + (ucColAddr & 0x0F));			/* 设置列地址的低地址 */
		OLED_WriteCmd (0x10 + ((ucColAddr >> 4) & 0x0F));	/* 设置列地址的高地址 */
		OLED_WriteData(ucValue);
	}
}

/*
*********************************************************************************************************
*	函 数 名: OLED_GetPixel
*	功能说明: 读取1个像素
*	形    参:
*			_usX,_usY : 像素坐标
*	返 回 值: 颜色值 (0, 1)
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
*	函 数 名: OLED_DrawLine
*	功能说明: 采用 Bresenham 算法，在2点间画一条直线。
*	形    参:
*			_usX1, _usY1 ：起始点坐标
*			_usX2, _usY2 ：终止点Y坐标
*			_ucColor     ：颜色
*	返 回 值: 无
*********************************************************************************************************
*/
void OLED_DrawLine(uint16_t _usX1 , uint16_t _usY1 , uint16_t _usX2 , uint16_t _usY2 , uint8_t _ucColor)
{
	int32_t dx , dy ;
	int32_t tx , ty ;
	int32_t inc1 , inc2 ;
	int32_t d , iTag ;
	int32_t x , y ;

	/* 采用 Bresenham 算法，在2点间画一条直线 */

	OLED_PutPixel(_usX1 , _usY1 , _ucColor);

	/* 如果两点重合，结束后面的动作。*/
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

	if ( dx < dy )   /*如果dy为计长方向，则交换纵横坐标。*/
	{
		uint16_t temp;

		iTag = 1 ;
		temp = _usX1; _usX1 = _usY1; _usY1 = temp;
		temp = _usX2; _usX2 = _usY2; _usY2 = temp;
		temp = dx; dx = dy; dy = temp;
	}
	tx = _usX2 > _usX1 ? 1 : -1 ;    /* 确定是增1还是减1 */
	ty = _usY2 > _usY1 ? 1 : -1 ;
	x = _usX1 ;
	y = _usY1 ;
	inc1 = 2 * dy ;
	inc2 = 2 * ( dy - dx );
	d = inc1 - dx ;
	while ( x != _usX2 )     /* 循环画点 */
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
*	函 数 名: OLED_DrawPoints
*	功能说明: 采用 Bresenham 算法，绘制一组点，并将这些点连接起来。可用于波形显示。
*	形    参:
*			x, y     ：坐标数组
*			_ucColor ：颜色
*	返 回 值: 无
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
*	函 数 名: OLED_DrawRect
*	功能说明: 绘制矩形。
*	形    参:
*			_usX,_usY：矩形左上角的坐标
*			_usHeight ：矩形的高度
*			_usWidth  ：矩形的宽度
*	返 回 值: 无
*********************************************************************************************************
*/
void OLED_DrawRect(uint16_t _usX, uint16_t _usY, uint8_t _usHeight, uint16_t _usWidth, uint8_t _ucColor)
{
	/*
	 ---------------->---
	|(_usX，_usY)        |
	V                    V  _usHeight
	|                    |
	 ---------------->---
		  _usWidth
	*/

	OLED_DrawLine(_usX, _usY, _usX + _usWidth - 1, _usY, _ucColor);	/* 顶 */
	OLED_DrawLine(_usX, _usY + _usHeight - 1, _usX + _usWidth - 1, _usY + _usHeight - 1, _ucColor);	/* 底 */

	OLED_DrawLine(_usX, _usY, _usX, _usY + _usHeight - 1, _ucColor);	/* 左 */
	OLED_DrawLine(_usX + _usWidth - 1, _usY, _usX + _usWidth - 1, _usY + _usHeight, _ucColor);	/* 右 */
}

/*
*********************************************************************************************************
*	函 数 名: OLED_DrawCircle
*	功能说明: 绘制一个圆，笔宽为1个像素
*	形    参:
*			_usX,_usY  ：圆心的坐标
*			_usRadius  ：圆的半径
*	返 回 值: 无
*********************************************************************************************************
*/
void OLED_DrawCircle(uint16_t _usX, uint16_t _usY, uint16_t _usRadius, uint8_t _ucColor)
{
	int32_t  D;			/* Decision Variable */
	uint32_t  CurX;		/* 当前 X 值 */
	uint32_t  CurY;		/* 当前 Y 值 */

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
*	函 数 名: OLED_DrawBMP
*	功能说明: 在LCD上显示一个BMP位图，位图点阵扫描次序：从左到右，从上到下
*	形    参:
*			_usX, _usY : 图片的坐标
*			_usHeight  ：图片高度
*			_usWidth   ：图片宽度
*			_ptr       ：单色图片点阵指针，每个像素占用1个字节
*	返 回 值: 无
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
*	函 数 名: OLED_ConfigGPIO
*	功能说明: 配置OLED控制口线，设置为8位80XX总线控制模式或SPI模式
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
static void OLED_ConfigGPIO(void)
{
	/* 12.配置GPIO */
	{
		GPIO_InitTypeDef GPIO_InitStructure;

		RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FSMC, ENABLE);

		/* 使能 FSMC, GPIOD, GPIOE, GPIOF, GPIOG 和 AFIO 时钟 */
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOE |
							 RCC_APB2Periph_GPIOF | RCC_APB2Periph_GPIOG |
							 RCC_APB2Periph_AFIO, ENABLE);

		/* 设置 PD.00(D2), PD.01(D3), PD.04(NOE), PD.05(NWE), PD.08(D13), PD.09(D14),
		 PD.10(D15), PD.14(D0), PD.15(D1) 为复用推挽输出 */
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5 |
									GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_14 |
									GPIO_Pin_15;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
		GPIO_Init(GPIOD, &GPIO_InitStructure);

		/* 设置 PE.07(D4), PE.08(D5), PE.09(D6), PE.10(D7), PE.11(D8), PE.12(D9), PE.13(D10),
		 PE.14(D11), PE.15(D12) 为复用推挽输出 */
		/* PE3,PE4 用于A19, A20, STM32F103ZE-EK(REV 1.0)必须使能 */
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |
									GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 |
									GPIO_Pin_15 | GPIO_Pin_3 | GPIO_Pin_4;
		GPIO_Init(GPIOE, &GPIO_InitStructure);

		/* 设置 PF.00(A0 (RS))  为复用推挽输出 */
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
		GPIO_Init(GPIOF, &GPIO_InitStructure);

		/* 设置 PG.12(NE4 (LCD/CS)) 为复用推挽输出 */
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
		GPIO_Init(GPIOG, &GPIO_InitStructure);
	}

	/* 2.配置FSMC总线参数 */
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
		init.FSMC_AsynchronousWait = FSMC_AsynchronousWait_Disable;	/* 注意旧库无这个成员 */
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
*	函 数 名: OLED_WriteCmd
*	功能说明: 向SSD1306发送一字节命令
*	形    参:  命令字
*	返 回 值: 无
*********************************************************************************************************
*/
static void OLED_WriteCmd(uint8_t _ucCmd)
{
#ifdef OLED_SPI3_EN
	uint8_t i;

	SSD_CS_0();

	SSD_SCK_0();
	SSD_SDIN_0();	/* 0 表示后面传送的是命令 1表示后面传送的数据 */
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
*	函 数 名: OLED_WriteData
*	功能说明: 向SSD1306发送一字节数据
*	形    参:  命令字
*	返 回 值: 无
*********************************************************************************************************
*/
static void OLED_WriteData(uint8_t _ucData)
{
#ifdef OLED_SPI3_EN
	uint8_t i;

	SSD_CS_0();

	SSD_SCK_0();
	SSD_SDIN_1();	/* 0 表示后面传送的是命令 1表示后面传送的数据 */
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

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
