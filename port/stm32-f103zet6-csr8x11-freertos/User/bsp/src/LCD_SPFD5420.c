/*
*********************************************************************************************************
*
*	模块名称 : TFT液晶显示器驱动模块
*	文件名称 : SPFD5420_SPFD5420.c
*	版    本 : V2.3
*	说    明 : 安富莱开发板标配的TFT显示器分辨率为240x400，3.0寸宽屏，带PWM背光调节功能。
*				支持的LCD内部驱动芯片型号有：SPFD5420A、OTM4001A、R61509V
*				驱动芯片的访问地址为:  0x60000000
*	修改记录 :
*		版本号  日期       作者    说明
*		v1.0    2011-08-21 armfly  ST固件库版本 V3.5.0版本。
*					a) 取消访问寄存器的结构体，直接定义
*		V2.0    2011-10-16 armfly  增加R61509V驱动，实现图标显示函数
*		V2.1    2012-07-06 armfly  增加RA8875驱动，支持4.3寸屏
*		V2.2    2012-07-13 armfly  改进SPFD5420_DispStr函数，支持12点阵字符;修改SPFD5420_DrawRect,解决差
*								一个像素问题
*		V2.3	2014-10-20 armfly  设置背光的函数移到  bsp_tft_lcd.c
*
*	Copyright (C), 2010-2011, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

/*
	友情提示：
	TFT控制器和一般的12864点阵显示器的控制器最大的区别在于引入了窗口绘图的机制，这个机制对于绘制局部图形
	是非常有效的。TFT可以先指定一个绘图窗口，然后所有的读写显存的操作均在这个窗口之内，因此它不需要CPU在
	内存中保存整个屏幕的像素数据。
*/

#include "bsp.h"
#include "fonts.h"


/*
	安富莱 STM32-X2 核心板接线方法：

	LCD模块的 RS 引脚     接 PD13/FSMC_A18
	LCD模块的 CS 片选引脚 接 PD7/FSMC_NE1
	其他的 NWE, NOE, D15~D0 接对应的FSMC口线即可。
*/
#define SPFD5420_BASE       ((uint32_t)(0x60000000 | 0x00000000))

#define SPFD5420_REG		*(__IO uint16_t *)(SPFD5420_BASE)
#define SPFD5420_RAM		*(__IO uint16_t *)(SPFD5420_BASE + (1 << 19))

static __IO uint8_t s_RGBChgEn = 0;		/* RGB转换使能, 4001屏写显存后读会的RGB格式和写入的不同 */

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
*	函 数 名: SPFD5420_Delaly10ms
*	功能说明: 延迟函数，不准
*	形    参:
*	返 回 值: 无
*********************************************************************************************************
*/
static void SPFD5420_Delaly10ms(void)
{
	uint16_t i;

	for (i = 0; i < 50000; i++);
}

/*
*********************************************************************************************************
*	函 数 名: SPFD5420_WriteReg
*	功能说明: 修改LCD控制器的寄存器的值。
*	形    参:  
*			SPFD5420_Reg ：寄存器地址;
*			SPFD5420_RegValue : 寄存器值
*	返 回 值: 无
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
*	函 数 名: SPFD5420_ReadReg
*	功能说明: 读取LCD控制器的寄存器的值。
*	形    参:  
*			SPFD5420_Reg ：寄存器地址;
*			SPFD5420_RegValue : 寄存器值
*	返 回 值: 无
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
*	函 数 名: SPFD5420_SetDispWin
*	功能说明: 设置显示窗口，进入窗口显示模式。TFT驱动芯片支持窗口显示模式，这是和一般的12864点阵LCD的最大区别。
*	形    参:  
*		_usX : 水平坐标
*		_usY : 垂直坐标
*		_usHeight: 窗口高度
*		_usWidth : 窗口宽度
*	返 回 值: 无
*********************************************************************************************************
*/
static void SPFD5420_SetDispWin(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth)
{
	uint16_t px, py;
	/*
		240x400屏幕物理坐标(px,py)如下:
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

		按照安富莱开发板LCD的方向，我们期望的虚拟坐标和扫描方向如下：(和上图第1个吻合)
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
	虚拟坐标和物理坐标转换关系：
		x = 399 - py;
		y = px;

		py = 399 - x;
		px = y;
	*/

	py = 399 - _usX;
	px = _usY;

	/* 设置显示窗口 WINDOWS */
	SPFD5420_WriteReg(0x0210, px);						/* 水平起始地址 */
	SPFD5420_WriteReg(0x0211, px + (_usHeight - 1));		/* 水平结束坐标 */
	SPFD5420_WriteReg(0x0212, py + 1 - _usWidth);		/* 垂直起始地址 */
	SPFD5420_WriteReg(0x0213, py);						/* 垂直结束地址 */

	SPFD5420_SetCursor(_usX, _usY);
}

/*
*********************************************************************************************************
*	函 数 名: SPFD5420_SetCursor
*	功能说明: 设置光标位置
*	形    参:  _usX : X坐标; _usY: Y坐标
*	返 回 值: 无
*********************************************************************************************************
*/
static void SPFD5420_SetCursor(uint16_t _usX, uint16_t _usY)
{
	/*
		px，py 是物理坐标， x，y是虚拟坐标
		转换公式:
		py = 399 - x;
		px = y;
	*/

	SPFD5420_WriteReg(0x0200, _usY);  		/* px */
	SPFD5420_WriteReg(0x0201, 399 - _usX);	/* py */
}

/*
*********************************************************************************************************
*	函 数 名: SPFD5420_BGR2RGB
*	功能说明: RRRRRGGGGGGBBBBB 改为 BBBBBGGGGGGRRRRR 格式
*	形    参:  RGB颜色代码
*	返 回 值: 转化后的颜色代码
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
*	函 数 名: SPFD5420_QuitWinMode
*	功能说明: 退出窗口显示模式，变为全屏显示模式
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
static void SPFD5420_QuitWinMode(void)
{
	SPFD5420_SetDispWin(0, 0, g_LcdHeight, g_LcdWidth);
}

/*
*********************************************************************************************************
*	函 数 名: SPFD5420_ReadID
*	功能说明: 读取LCD驱动芯片ID
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
uint16_t SPFD5420_ReadID(void)
{
	return SPFD5420_ReadReg(0x0000);
}

/*
*********************************************************************************************************
*	函 数 名: Init_5420_4001
*	功能说明: 初始化5420和4001屏
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
static void Init_5420_4001(void)
{
	/* 初始化LCD，写LCD寄存器进行配置 */
	SPFD5420_WriteReg(0x0000, 0x0000);
	SPFD5420_WriteReg(0x0001, 0x0100);
	SPFD5420_WriteReg(0x0002, 0x0100);

	/*
		R003H 寄存器很关键， Entry Mode ，决定了扫描方向
		参见：SPFD5420A.pdf 第15页

		240x400屏幕物理坐标(px,py)如下:
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

		按照安富莱开发板LCD的方向，我们期望的虚拟坐标和扫描方向如下：(和上图第1个吻合)
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

		虚拟坐标(x,y) 和物理坐标的转换关系
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

	/* 设置显示窗口 WINDOWS */
	SPFD5420_WriteReg(0x0210, 0);	/* 水平起始地址 */
	SPFD5420_WriteReg(0x0211, 239);	/* 水平结束坐标 */
	SPFD5420_WriteReg(0x0212, 0);	/* 垂直起始地址 */
	SPFD5420_WriteReg(0x0213, 399);	/* 垂直结束地址 */
}

/*
*********************************************************************************************************
*	函 数 名: Init_61509
*	功能说明: 初始化61509屏
*	形    参:  无
*	返 回 值: 无
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
	SPFD5420_WriteReg(0x100,0x0730);	/* BT,AP 0x0330　*/
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
*	函 数 名: SPFD5420_InitHard
*	功能说明: 初始化LCD
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void SPFD5420_InitHard(void)
{
	uint16_t id;

	id = SPFD5420_ReadReg(0x0000);  	/* 读取LCD驱动芯片ID */

	if ((id == 0x5420) || (id == 0x5520))	/* 4001屏和5420相同，4001屏读回显存RGB时，需要进行转换，5420无需 */
	{
		Init_5420_4001();	/* 初始化5420和4001屏硬件 */

		/* 下面这段代码用于识别是4001屏还是5420屏 */
		{
			uint16_t dummy;

			SPFD5420_WriteReg(0x0200, 0);
			SPFD5420_WriteReg(0x0201, 0);

			SPFD5420_REG = 0x0202;
			SPFD5420_RAM = 0x1234;		/* 写一个像素 */

			SPFD5420_WriteReg(0x0200, 0);
			SPFD5420_WriteReg(0x0201, 0);
			SPFD5420_REG = 0x0202;
			dummy = SPFD5420_RAM; 		/* 读回颜色值 */
			if (dummy == 0x1234)
			{
				s_RGBChgEn = 0;

				g_ChipID = IC_5420;
			}
			else
			{
				s_RGBChgEn = 1;		/* 如果读回的和写入的不同，则需要RGB转换, 只影响读取像素的函数 */

				g_ChipID = IC_4001;
			}
			g_LcdHeight = LCD_30_HEIGHT;
			g_LcdWidth = LCD_30_WIDTH;
		}
	}
	else if (id == 0xB509)
	{
		Init_61509();		/* 初始化61509屏硬件 */

		s_RGBChgEn = 1;			/* 如果读回的和写入的不同，则需要RGB转换, 只影响读取像素的函数 */

		g_ChipID = IC_61509;
		g_LcdHeight = LCD_30_HEIGHT;
		g_LcdWidth = LCD_30_WIDTH;
	}
}

/*
*********************************************************************************************************
*	函 数 名: SPFD5420_DispOn
*	功能说明: 打开显示
*	形    参:  无
*	返 回 值: 无
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
		SPFD5420_WriteReg(7, 0x0173); /* 设置262K颜色并且打开显示 */
	}
}

/*
*********************************************************************************************************
*	函 数 名: SPFD5420_DispOff
*	功能说明: 关闭显示
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void SPFD5420_DispOff(void)
{
	SPFD5420_WriteReg(7, 0x0000);
}

/*
*********************************************************************************************************
*	函 数 名: SPFD5420_ClrScr
*	功能说明: 根据输入的颜色值清屏
*	形    参:  _usColor : 背景色
*	返 回 值: 无
*********************************************************************************************************
*/
void SPFD5420_ClrScr(uint16_t _usColor)
{
	uint32_t i;

	SPFD5420_SetCursor(0, 0);		/* 设置光标位置 */

	SPFD5420_REG = 0x202; 			/* 准备读写显存 */

	for (i = 0; i < g_LcdHeight * g_LcdWidth; i++)
	{
		SPFD5420_RAM = _usColor;
	}
}

/*
*********************************************************************************************************
*	函 数 名: SPFD5420_PutPixel
*	功能说明: 画1个像素
*	形    参:  
*			_usX,_usY : 像素坐标
*			_usColor  ：像素颜色
*	返 回 值: 无
*********************************************************************************************************
*/
void SPFD5420_PutPixel(uint16_t _usX, uint16_t _usY, uint16_t _usColor)
{
	SPFD5420_SetCursor(_usX, _usY);	/* 设置光标位置 */

	/* 写显存 */
	SPFD5420_REG = 0x202;
	/* Write 16-bit GRAM Reg */
	SPFD5420_RAM = _usColor;
}

/*
*********************************************************************************************************
*	函 数 名: SPFD5420_GetPixel
*	功能说明: 读取1个像素
*	形    参:  
*			_usX,_usY : 像素坐标
*			_usColor  ：像素颜色
*	返 回 值: RGB颜色值
*********************************************************************************************************
*/
uint16_t SPFD5420_GetPixel(uint16_t _usX, uint16_t _usY)
{
	uint16_t usRGB;

	SPFD5420_SetCursor(_usX, _usY);	/* 设置光标位置 */

	{
		/* 准备写显存 */
		SPFD5420_REG = 0x202;

		usRGB = SPFD5420_RAM;

		/* 读 16-bit GRAM Reg */
		if (s_RGBChgEn == 1)
		{
			usRGB = SPFD5420_BGR2RGB(usRGB);
		}
	}

	return usRGB;
}

/*
*********************************************************************************************************
*	函 数 名: SPFD5420_DrawLine
*	功能说明: 采用 Bresenham 算法，在2点间画一条直线。
*	形    参:  
*			_usX1, _usY1 ：起始点坐标
*			_usX2, _usY2 ：终止点Y坐标
*			_usColor     ：颜色
*	返 回 值: 无
*********************************************************************************************************
*/
void SPFD5420_DrawLine(uint16_t _usX1 , uint16_t _usY1 , uint16_t _usX2 , uint16_t _usY2 , uint16_t _usColor)
{
	int32_t dx , dy ;
	int32_t tx , ty ;
	int32_t inc1 , inc2 ;
	int32_t d , iTag ;
	int32_t x , y ;

	/* 采用 Bresenham 算法，在2点间画一条直线 */

	SPFD5420_PutPixel(_usX1 , _usY1 , _usColor);

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
*	函 数 名: SPFD5420_DrawHLine
*	功能说明: 绘制一条水平线 （主要用于UCGUI的接口函数）
*	形    参:  _usX1    ：起始点X坐标
*			  _usY1    ：水平线的Y坐标
*			  _usX2    ：结束点X坐标
*			  _usColor : 颜色
*	返 回 值: 无
*********************************************************************************************************
*/
void SPFD5420_DrawHLine(uint16_t _usX1 , uint16_t _usY1 , uint16_t _usX2 , uint16_t _usColor)
{
	uint16_t i;

	/* 展开 SPFD5420_SetCursor(_usX1, _usY1) 函数，提高执行效率 */
	/*
		px，py 是物理坐标， x，y是虚拟坐标
		转换公式:
		py = 399 - x;
		px = y;
	*/
	SPFD5420_WriteReg(0x0200, _usY1);  		/* px */
	SPFD5420_WriteReg(0x0201, 399 - _usX1);	/* py */

	/* 写显存 */
	SPFD5420_REG = 0x202;
	for (i = 0; i < _usX2 - _usX1 + 1; i++)
	{
		SPFD5420_RAM = _usColor;
	}
}

/*
*********************************************************************************************************
*	函 数 名: SPFD5420_DrawHColorLine
*	功能说明: 绘制一条彩色水平线 （主要用于UCGUI的接口函数）
*	形    参:  _usX1    ：起始点X坐标
*			  _usY1    ：水平线的Y坐标
*			  _usWidth ：直线的宽度
*			  _pColor : 颜色缓冲区
*	返 回 值: 无
*********************************************************************************************************
*/
void SPFD5420_DrawHColorLine(uint16_t _usX1 , uint16_t _usY1, uint16_t _usWidth, const uint16_t *_pColor)
{
	uint16_t i;

	/* 展开 SPFD5420_SetCursor(_usX1, _usY1) 函数，提高执行效率 */
	/*
		px，py 是物理坐标， x，y是虚拟坐标
		转换公式:
		py = 399 - x;
		px = y;
	*/
	SPFD5420_WriteReg(0x0200, _usY1);  		/* px */
	SPFD5420_WriteReg(0x0201, 399 - _usX1);	/* py */

	/* 写显存 */
	SPFD5420_REG = 0x202;
	for (i = 0; i < _usWidth; i++)
	{
		SPFD5420_RAM = *_pColor++;
	}
}

/*
*********************************************************************************************************
*	函 数 名: SPFD5420_DrawHTransLine
*	功能说明: 绘制一条彩色透明的水平线 （主要用于UCGUI的接口函数）， 颜色值为0表示透明色
*	形    参:  _usX1    ：起始点X坐标
*			  _usY1    ：水平线的Y坐标
*			  _usWidth ：直线的宽度
*			  _pColor : 颜色缓冲区
*	返 回 值: 无
*********************************************************************************************************
*/
void SPFD5420_DrawHTransLine(uint16_t _usX1 , uint16_t _usY1, uint16_t _usWidth, const uint16_t *_pColor)
{
	uint16_t i, j;
	uint16_t Index;

	/* 展开 SPFD5420_SetCursor(_usX1, _usY1) 函数，提高执行效率 */
	/*
		px，py 是物理坐标， x，y是虚拟坐标
		转换公式:
		py = 399 - x;
		px = y;
	*/
	SPFD5420_WriteReg(0x0200, _usY1);  		/* px */
	SPFD5420_WriteReg(0x0201, 399 - _usX1);	/* py */

	/* 写显存 */
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
*	函 数 名: SPFD5420_DrawRect
*	功能说明: 绘制水平放置的矩形。
*	形    参:  
*			_usX,_usY：矩形左上角的坐标
*			_usHeight ：矩形的高度
*			_usWidth  ：矩形的宽度
*	返 回 值: 无
*********************************************************************************************************
*/
void SPFD5420_DrawRect(uint16_t _usX, uint16_t _usY, uint8_t _usHeight, uint16_t _usWidth, uint16_t _usColor)
{
	/*
	 ---------------->---
	|(_usX，_usY)        |
	V                    V  _usHeight
	|                    |
	 ---------------->---
		  _usWidth
	*/

	SPFD5420_DrawLine(_usX, _usY, _usX + _usWidth - 1, _usY, _usColor);	/* 顶 */
	SPFD5420_DrawLine(_usX, _usY + _usHeight - 1, _usX + _usWidth - 1, _usY + _usHeight - 1, _usColor);	/* 底 */

	SPFD5420_DrawLine(_usX, _usY, _usX, _usY + _usHeight - 1, _usColor);	/* 左 */
	SPFD5420_DrawLine(_usX + _usWidth - 1, _usY, _usX + _usWidth - 1, _usY + _usHeight, _usColor);	/* 右 */
}

/*
*********************************************************************************************************
*	函 数 名: SPFD5420_DrawCircle
*	功能说明: 绘制一个圆，笔宽为1个像素
*	形    参:  
*			_usX,_usY  ：圆心的坐标
*			_usRadius  ：圆的半径
*	返 回 值: 无
*********************************************************************************************************
*/
void SPFD5420_DrawCircle(uint16_t _usX, uint16_t _usY, uint16_t _usRadius, uint16_t _usColor)
{
	int32_t  D;			/* Decision Variable */
	uint32_t  CurX;		/* 当前 X 值 */
	uint32_t  CurY;		/* 当前 Y 值 */

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
*	函 数 名: SPFD5420_DrawBMP
*	功能说明: 在LCD上显示一个BMP位图，位图点阵扫描次序：从左到右，从上到下
*	形    参:  
*			_usX, _usY : 图片的坐标
*			_usHeight  ：图片高度
*			_usWidth   ：图片宽度
*			_ptr       ：图片点阵指针
*	返 回 值: 无
*********************************************************************************************************
*/
void SPFD5420_DrawBMP(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t *_ptr)
{
	uint32_t index = 0;
	const uint16_t *p;

	/* 设置图片的位置和大小， 即设置显示窗口 */
	SPFD5420_SetDispWin(_usX, _usY, _usHeight, _usWidth);

	p = _ptr;
	for (index = 0; index < _usHeight * _usWidth; index++)
	{
		SPFD5420_PutPixel(_usX, _usY, *p++);
	}

	/* 退出窗口绘图模式 */
	SPFD5420_QuitWinMode();
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
