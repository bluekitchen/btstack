/*
*********************************************************************************************************
*
*	模块名称 : TFT液晶显示器驱动模块
*	文件名称 : bsp_tft_lcd.c
*	版    本 : V4.1
*	说    明 : 支持3.0， 3.5， 4.3， 5.0， 7.0寸显示模块.
*			  3.0寸的支持的LCD内部驱动芯片型号有: SPFD5420A、OTM4001A、R61509V
*	修改记录 :
*		版本号  日期       作者    说明
*		v1.0    2011-08-21 armfly  ST固件库版本 V3.5.0版本。
*					a) 取消访问寄存器的结构体，直接定义
*		V2.0    2011-10-16 armfly  增加R61509V驱动，实现图标显示函数
*		V2.1    2012-07-06 armfly  增加RA8875驱动，支持4.3寸屏
*		V2.2    2012-07-13 armfly  改进LCD_DispStr函数，支持12点阵字符;修改LCD_DrawRect,解决差一个像素问题
*		V2.3    2012-08-08 armfly  将底层芯片寄存器操作相关的函数放到单独的文件，支持RA8875
*   	V3.0    2013-05-20 增加图标结构; 修改	LCD_DrawIconActive  修改DispStr函数支持文本透明
*		V3.1    2013-06-12 解决LCD_DispStr()函数BUG，如果内嵌字库中汉字个数多于256，则出现死循环。
*		V3.2    2013-06-28 完善Label控件, 当显示字符串比之前短时，自动清除多余的文字
*		V3.3    2013-06-29 FSMC初始化时，配置时序，写时序和读时序分开设置。 LCD_FSMCConfig 函数。
*		V3.4    2013-07-06 增加显示32位带Alpha图标的函数 LCD_DrawIcon32
*		V3.5    2013-07-24 增加显示32位带Alpha图片的函数 LCD_DrawBmp32
*		V3.6    2013-07-30 修改 DispEdit() 支持12点阵汉字对齐
*		V3.7    2014-09-06 修改 LCD_InitHard() 同时支持 RA8875-SPI接口和8080接口
*		V3.8    2014-09-15 增加若干函数:
*					（1） LCD_DispStrEx() 可以自动对齐自动填白的显示字符串函数
*					（2） LCD_GetStrWidth() 计算字符串的像素宽度
*		V3.9    2014-10-18
*					(1) 增加 LCD_ButtonTouchDown() LCD_ButtonTouchRelease 判断触摸坐标并重绘按钮
*					(2) 增加3.5寸LCD驱动
*					(3) 增加 LCD_SetDirection() 函数，设置显示屏方向（横屏 竖屏动态切换）
*		V4.0   2015-04-04 
*				(1) 按钮、编辑框控件增加RA8875字体，内嵌字库和RA8875字库统一编码。字体代码增加 
*				    FC_RA8875_16, FC_RA8875_24,	FC_RA8875_32
*				(2) FONT_T结构体成员FontCode的类型由 uint16_t 修改为 FONT_CODE_E枚举，便于编译器查错;
*				(3) 修改 LCD_DispStrEx(), 将读点阵的语句独立到函数：_LCD_ReadAsciiDot(), _LCD_ReadHZDot()
*				(4) LCD_DispStr() 函数简化，直接调用 LCD_DispStrEx() 实现。
*				(5) LCD_DispStrEx() 函数支持 RA8875字体。
*				(6) LCD_ButtonTouchDown() 增加按键提示音
*		V4.1   2015-04-18 
*				(1) 添加RA885 ASCII字体的宽度表。LCD_DispStrEx() 函数可以支持RA8875 ASCII变长宽度计算。
*				(2) 添加 LCD_HardReset(）函数，支持LCD复位由GPIO控制的产品。STM32-V5 不需要GPIO控制。
*
*	Copyright (C), 2015-2016, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"
#include "fonts.h"

/* 下面3个变量，主要用于使程序同时支持不同的屏 */
uint16_t g_ChipID = IC_4001;		/* 驱动芯片ID */
uint16_t g_LcdHeight = 240;			/* 显示屏分辨率-高度 */
uint16_t g_LcdWidth = 400;			/* 显示屏分辨率-宽度 */
uint8_t s_ucBright;					/* 背光亮度参数 */
uint8_t g_LcdDirection;				/* 显示方向.0，1，2，3 */

static void LCD_CtrlLinesConfig(void);
static void LCD_FSMCConfig(void);
static void LCD_HardReset(void);

void LCD_SetPwmBackLight(uint8_t _bright);


/*
*********************************************************************************************************
*	函 数 名: LCD_InitHard
*	功能说明: 初始化LCD
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_InitHard(void)
{
	uint32_t id;

	/* 配置LCD控制口线GPIO */
	LCD_CtrlLinesConfig();

	/* 配置FSMC接口，数据总线 */
	LCD_FSMCConfig();

	LCD_HardReset();	/* 硬件复位 （STM32-V5 无需），针对其他GPIO控制LCD复位的产品 */
	
	/* FSMC重置后必须加延迟才能访问总线设备  */
	bsp_DelayMS(20);

	id = ILI9488_ReadID();
	if (id == 0x548066)		/* 3.5寸屏 */
	{
		g_ChipID = IC_9488;
		ILI9488_InitHard();
	}
	else
	{
		id = SPFD5420_ReadID();  	/* 读取LCD驱动芯片ID */
		if ((id == 0x5420) || (id ==  0xB509) || (id == 0x5520))
		{
			SPFD5420_InitHard();	/* 初始化5420和4001屏硬件 */
			/* g_ChipID 在函数内部设置了 */
		}
		else
		{
			g_ChipID = IC_8875;
			RA8875_InitHard();	/* 初始化RA8875芯片 */
		}
	}

	LCD_SetDirection(0);

	LCD_ClrScr(CL_BLACK);	/* 清屏，显示全黑 */

	LCD_SetBackLight(BRIGHT_DEFAULT);	 /* 打开背光，设置为缺省亮度 */
}

/*
*********************************************************************************************************
*	函 数 名: LCD_HardReset
*	功能说明: 硬件复位. 针对复位口线由GPIO控制的产品。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_HardReset(void)
{
#if 0	
	GPIO_InitTypeDef GPIO_InitStructure;

	/* 使能 GPIO时钟 */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	
	/* 配置背光GPIO为推挽输出模式 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_ResetBits(GPIOB, GPIO_Pin_1);
	bsp_DelayMS(20);
	GPIO_SetBits(GPIOB, GPIO_Pin_1);
#endif	
}

/*
*********************************************************************************************************
*	函 数 名: LCD_GetChipDescribe
*	功能说明: 读取LCD驱动芯片的描述符号，用于显示
*	形    参: char *_str : 描述符字符串填入此缓冲区
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_GetChipDescribe(char *_str)
{
	switch (g_ChipID)
	{
		case IC_5420:
			strcpy(_str, CHIP_STR_5420);
			break;

		case IC_4001:
			strcpy(_str, CHIP_STR_4001);
			break;

		case IC_61509:
			strcpy(_str, CHIP_STR_61509);
			break;

		case IC_8875:
			strcpy(_str, CHIP_STR_8875);
			break;

		case IC_9488:
			strcpy(_str, CHIP_STR_9488);
			break;

		default:
			strcpy(_str, "Unknow");
			break;
	}
}

/*
*********************************************************************************************************
*	函 数 名: LCD_GetHeight
*	功能说明: 读取LCD分辨率之高度
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
uint16_t LCD_GetHeight(void)
{
	return g_LcdHeight;
}

/*
*********************************************************************************************************
*	函 数 名: LCD_GetWidth
*	功能说明: 读取LCD分辨率之宽度
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
uint16_t LCD_GetWidth(void)
{
	return g_LcdWidth;
}

/*
*********************************************************************************************************
*	函 数 名: LCD_DispOn
*	功能说明: 打开显示
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_DispOn(void)
{
	if (g_ChipID == IC_8875)
	{
		RA8875_DispOn();
	}
	else if (g_ChipID == IC_9488)
	{
		ILI9488_DispOn();
	}
	else	/* 61509, 5420, 4001 */
	{
		SPFD5420_DispOn();
	}
}

/*
*********************************************************************************************************
*	函 数 名: LCD_DispOff
*	功能说明: 关闭显示
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_DispOff(void)
{
	if (g_ChipID == IC_8875)
	{
		RA8875_DispOff();
	}
	else if (g_ChipID == IC_9488)
	{
		ILI9488_DispOff();
	}
	else	/* 61509, 5420, 4001 */
	{
		SPFD5420_DispOff();
	}
}

/*
*********************************************************************************************************
*	函 数 名: LCD_ClrScr
*	功能说明: 根据输入的颜色值清屏
*	形    参: _usColor : 背景色
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_ClrScr(uint16_t _usColor)
{
	if (g_ChipID == IC_8875)	/* RA8875 分支 */
	{
		RA8875_ClrScr(_usColor);
	}
	else if (g_ChipID == IC_9488)
	{
		ILI9488_ClrScr(_usColor);
	}
	else	/* 5420，4001，61509 分支 */
	{
		SPFD5420_ClrScr(_usColor);
	}
}

/*
*********************************************************************************************************
*	函 数 名: LCD_DispStr
*	功能说明: 在LCD指定坐标（左上角）显示一个字符串
*	形    参:
*		_usX : X坐标
*		_usY : Y坐标
*		_ptr  : 字符串指针
*		_tFont : 字体结构体，包含颜色、背景色(支持透明)、字体代码、文字间距等参数
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_DispStr(uint16_t _usX, uint16_t _usY, char *_ptr, FONT_T *_tFont)
{
	LCD_DispStrEx(_usX, _usY, _ptr, _tFont, 0, 0);
}

/*
*********************************************************************************************************
*	函 数 名: LCD_GetFontWidth
*	功能说明: 读取字体的宽度（像素单位)
*	形    参:
*		_tFont : 字体结构体，包含颜色、背景色(支持透明)、字体代码、文字间距等参数
*	返 回 值: 字体的宽度（像素单位)
*********************************************************************************************************
*/
uint16_t LCD_GetFontWidth(FONT_T *_tFont)
{
	uint16_t font_width = 16;

	switch (_tFont->FontCode)
	{
		case FC_ST_12:
			font_width = 12;
			break;

		case FC_ST_16:
		case FC_RA8875_16:			
			font_width = 16;
			break;
			
		case FC_RA8875_24:			
		case FC_ST_24:
			font_width = 24;
			break;
			
		case FC_ST_32:
		case FC_RA8875_32:	
			font_width = 32;
			break;			
	}
	return font_width;
}

/*
*********************************************************************************************************
*	函 数 名: LCD_GetFontHeight
*	功能说明: 读取字体的高度（像素单位)
*	形    参:
*		_tFont : 字体结构体，包含颜色、背景色(支持透明)、字体代码、文字间距等参数
*	返 回 值: 字体的宽度（像素单位)
*********************************************************************************************************
*/
uint16_t LCD_GetFontHeight(FONT_T *_tFont)
{
	uint16_t height = 16;

	switch (_tFont->FontCode)
	{
		case FC_ST_12:
			height = 12;
			break;

		case FC_ST_16:
		case FC_RA8875_16:			
			height = 16;
			break;
			
		case FC_RA8875_24:			
		case FC_ST_24:
			height = 24;
			break;
			
		case FC_ST_32:
		case FC_RA8875_32:	
			height = 32;
			break;			
	}
	return height;
}

/*
*********************************************************************************************************
*	函 数 名: LCD_GetStrWidth
*	功能说明: 计算字符串宽度(像素单位)
*	形    参:
*		_ptr  : 字符串指针
*		_tFont : 字体结构体，包含颜色、背景色(支持透明)、字体代码、文字间距等参数
*	返 回 值: 无
*********************************************************************************************************
*/
uint16_t LCD_GetStrWidth(char *_ptr, FONT_T *_tFont)
{
	char *p = _ptr;
	uint16_t width = 0;
	uint8_t code1, code2;
	uint16_t font_width;

	font_width = LCD_GetFontWidth(_tFont);

	while (*p != 0)
	{
		code1 = *p;	/* 读取字符串数据， 该数据可能是ascii代码，也可能汉字代码的高字节 */
		if (code1 < 0x80)	/* ASCII */
		{
			switch(_tFont->FontCode)
			{
				case FC_RA8875_16:
					font_width = g_RA8875_Ascii16_width[code1 - 0x20];
					break;
				
				case FC_RA8875_24:
					font_width = g_RA8875_Ascii24_width[code1 - 0x20];
					break;
				
				case FC_RA8875_32:
					font_width = g_RA8875_Ascii32_width[code1 - 0x20];
					break;
				
				case FC_ST_12:
					font_width = 6;
					break;

				case FC_ST_16:		
					font_width = 8;
					break;
					
				case FC_ST_24:			
					font_width = 12;
					break;
					
				case FC_ST_32:
					font_width = 16;
					break;
				
				default:
					font_width = 8;
					break;					
			}
			
		}
		else	/* 汉字 */
		{
			code2 = *++p;
			if (code2 == 0)
			{
				break;
			}
			font_width = LCD_GetFontWidth(_tFont);
			
		}
		width += font_width;
		p++;
	}

	return width;
}

/*
*********************************************************************************************************
*	函 数 名: _LCD_ReadAsciiDot
*	功能说明: 读取1个ASCII字符的点阵数据
*	形    参:
*		_code : ASCII字符的编码，1字节。1-128
*		_fontcode ：字体代码
*		_pBuf : 存放读出的字符点阵数据
*	返 回 值: 文字宽度
*********************************************************************************************************
*/
static void _LCD_ReadAsciiDot(uint8_t _code, uint8_t _fontcode, uint8_t *_pBuf)
{
	const uint8_t *pAscDot;
	uint8_t font_bytes = 0;

	pAscDot = 0;
	switch (_fontcode)
	{
		case FC_ST_12:		/* 12点阵 */
			font_bytes = 24;
			pAscDot = g_Ascii12;	
			break;
		
		case FC_ST_24:
		case FC_ST_32:
		case FC_ST_16:
			/* 缺省是16点阵 */
			font_bytes = 32;
			pAscDot = g_Ascii16;
			break;
		
		case FC_RA8875_16:
		case FC_RA8875_24:
		case FC_RA8875_32:
			return;
	}	

	/* 将CPU内部Flash中的ascii字符点阵复制到buf */
	memcpy(_pBuf, &pAscDot[_code * (font_bytes / 2)], (font_bytes / 2));	
}

/*
*********************************************************************************************************
*	函 数 名: _LCD_ReadHZDot
*	功能说明: 读取1个汉字的点阵数据
*	形    参:
*		_code1, _cod2 : 汉字内码. GB2312编码
*		_fontcode ：字体代码
*		_pBuf : 存放读出的字符点阵数据
*	返 回 值: 无
*********************************************************************************************************
*/
static void _LCD_ReadHZDot(uint8_t _code1, uint8_t _code2,  uint8_t _fontcode, uint8_t *_pBuf)
{
	#ifdef USE_SMALL_FONT	/* 使用CPU 内部Flash 小字库 */
		uint8_t *pDot;
		uint8_t font_bytes = 0;
		uint32_t address;
		uint16_t m;

		pDot = 0;	/* 仅仅用于避免告警 */
		switch (_fontcode)
		{
			case FC_ST_12:		/* 12点阵 */
				font_bytes = 24;
				pDot = (uint8_t *)g_Hz12;	
				break;
			
			case FC_ST_16:
				font_bytes = 32;
				pDot = (uint8_t *)g_Hz16;
				break;
	
			case FC_ST_24:
				font_bytes = 72;
				pDot = (uint8_t *)g_Hz24;
				break;			
				
			case FC_ST_32:	
				font_bytes = 128;
				pDot = (uint8_t *)g_Hz32;
				break;						
			
			case FC_RA8875_16:
			case FC_RA8875_24:
			case FC_RA8875_32:
				return;
		}	

		m = 0;
		while(1)
		{
			address = m * (font_bytes + 2);
			m++;
			if ((_code1 == pDot[address + 0]) && (_code2 == pDot[address + 1]))
			{
				address += 2;
				memcpy(_pBuf, &pDot[address], font_bytes);
				break;
			}
			else if ((pDot[address + 0] == 0xFF) && (pDot[address + 1] == 0xFF))
			{
				/* 字库搜索完毕，未找到，则填充全FF */
				memset(_pBuf, 0xFF, font_bytes);
				break;
			}
		}
	#else	/* 用全字库 */
		uint8_t *pDot = 0;
		uint8_t font_bytes = 0;
			
		switch (_fontcode)
		{
			case FC_ST_12:		/* 12点阵 */
				font_bytes = 24;
				pDot = (uint8_t *)HZK12_ADDR;	
				break;
			
			case FC_ST_16:
				font_bytes = 32;
				pDot = (uint8_t *)HZK16_ADDR;
				break;
	
			case FC_ST_24:
				font_bytes = 72;
				pDot = (uint8_t *)HZK24_ADDR;
				break;			
				
			case FC_ST_32:	
				font_bytes = 128;
				pDot = (uint8_t *)HZK32_ADDR;
				break;						
			
			case FC_RA8875_16:
			case FC_RA8875_24:
			case FC_RA8875_32:
				return;
		}			
	
		/* 此处需要根据字库文件存放位置进行修改 */
		if (_code1 >=0xA1 && _code1 <= 0xA9 && _code2 >=0xA1)
		{
			pDot += ((_code1 - 0xA1) * 94 + (_code2 - 0xA1)) * font_bytes;
		}
		else if (_code1 >=0xB0 && _code1 <= 0xF7 && _code2 >=0xA1)
		{
			pDot += ((_code1 - 0xB0) * 94 + (_code2 - 0xA1) + 846) * font_bytes;
		}
		memcpy(_pBuf, pDot, font_bytes);
	#endif
}
			
/*
*********************************************************************************************************
*	函 数 名: LCD_DispStrEx
*	功能说明: 在LCD指定坐标（左上角）显示一个字符串。 增强型函数。支持左\中\右对齐，支持定长清屏。
*	形    参:
*		_usX : X坐标
*		_usY : Y坐标
*		_ptr  : 字符串指针
*		_tFont : 字体结构体，包含颜色、背景色(支持透明)、字体代码、文字间距等参数。可以指定RA8875字库显示汉字
*		_Width : 字符串显示区域的宽度. 0 表示不处理留白区域，此时_Align无效
*		_Align :字符串在显示区域的对齐方式，
*				ALIGN_LEFT = 0,
*				ALIGN_CENTER = 1,
*				ALIGN_RIGHT = 2
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_DispStrEx(uint16_t _usX, uint16_t _usY, char *_ptr, FONT_T *_tFont, uint16_t _Width,
	uint8_t _Align)
{
	uint32_t i;
	uint8_t code1;
	uint8_t code2;
	uint8_t buf[32 * 32 / 8];	/* 最大支持24点阵汉字 */
	uint8_t width;
	uint16_t m;
	uint8_t font_width = 0;
	uint8_t font_height = 0;
	uint16_t x, y;
	uint16_t offset;
	uint16_t str_width;	/* 字符串实际宽度  */
	uint8_t ra8875_use = 0;
	uint8_t ra8875_font_code = 0;

	switch (_tFont->FontCode)
	{
		case FC_ST_12:		/* 12点阵 */
			font_height = 12;
			font_width = 12;
			break;
		
		case FC_ST_16:
			font_height = 16;
			font_width = 16;
			break;

		case FC_ST_24:
			font_height = 24;
			font_width = 24;
			break;
						
		case FC_ST_32:	
			font_height = 32;
			font_width = 32;
			break;					
		
		case FC_RA8875_16:
			ra8875_font_code = RA_FONT_16;
			ra8875_use = 1;	/* 表示用RA8875字库 */
			break;
			
		case FC_RA8875_24:
			ra8875_font_code = RA_FONT_24;
			ra8875_use = 1;	/* 表示用RA8875字库 */
			break;
						
		case FC_RA8875_32:
			ra8875_font_code = RA_FONT_32;
			ra8875_use = 1;	/* 表示用RA8875字库 */
			break;
		
		default:
			return;
	}
	
	str_width = LCD_GetStrWidth(_ptr, _tFont);	/* 计算字符串实际宽度(RA8875内部ASCII点阵宽度为变长 */
	offset = 0;
	if (_Width > str_width)
	{
		if (_Align == ALIGN_RIGHT)	/* 右对齐 */
		{
			offset = _Width - str_width;
		}
		else if (_Align == ALIGN_CENTER)	/* 左对齐 */
		{
			offset = (_Width - str_width) / 2;
		}
		else	/* 左对齐 ALIGN_LEFT */
		{
			;
		}
	}

	/* 左侧填背景色, 中间对齐和右边对齐  */
	if (offset > 0)
	{
		LCD_Fill_Rect(_usX, _usY, LCD_GetFontHeight(_tFont), offset,  _tFont->BackColor);
		_usX += offset;
	}
	
	/* 右侧填背景色 */
	if (_Width > str_width)
	{
		LCD_Fill_Rect(_usX + str_width, _usY, LCD_GetFontHeight(_tFont), _Width - str_width - offset,  _tFont->BackColor);
	}
	
	if (ra8875_use == 1)	/* 使用RA8875外挂的字库芯片 */
	{
		RA8875_SetFrontColor(_tFont->FrontColor);			/* 设置字体前景色 */
		RA8875_SetBackColor(_tFont->BackColor);				/* 设置字体背景色 */
		RA8875_SetFont(ra8875_font_code, 0, _tFont->Space);	/* 字体代码，行间距，字间距 */
		RA8875_DispStr(_usX, _usY, _ptr);
	}
	else	/* 使用CPU内部字库. 点阵信息由CPU读取 */
	{
		/* 开始循环处理字符 */
		while (*_ptr != 0)
		{
			code1 = *_ptr;	/* 读取字符串数据， 该数据可能是ascii代码，也可能汉字代码的高字节 */
			if (code1 < 0x80)
			{
				/* 将ascii字符点阵复制到buf */
				//memcpy(buf, &pAscDot[code1 * (font_bytes / 2)], (font_bytes / 2));
				_LCD_ReadAsciiDot(code1, _tFont->FontCode, buf);	/* 读取ASCII字符点阵 */
				width = font_width / 2;
			}
			else
			{
				code2 = *++_ptr;
				if (code2 == 0)
				{
					break;
				}
				/* 读1个汉字的点阵 */
				_LCD_ReadHZDot(code1, code2, _tFont->FontCode, buf);
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
						LCD_PutPixel(x, y, _tFont->FrontColor);	/* 设置像素颜色为文字色 */
					}
					else
					{
						if (_tFont->BackColor != CL_MASK)	/* 透明色 */
						{
							LCD_PutPixel(x, y, _tFont->BackColor);	/* 设置像素颜色为文字背景色 */
						}
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
}

/*
*********************************************************************************************************
*	函 数 名: LCD_PutPixel
*	功能说明: 画1个像素
*	形    参:
*			_usX,_usY : 像素坐标
*			_usColor  : 像素颜色
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_PutPixel(uint16_t _usX, uint16_t _usY, uint16_t _usColor)
{
	if (g_ChipID == IC_8875)
	{
		RA8875_PutPixel(_usX, _usY, _usColor);
	}
	else if (g_ChipID == IC_9488)
	{
		ILI9488_PutPixel(_usX, _usY, _usColor);
	}
	else
	{
		SPFD5420_PutPixel(_usX, _usY, _usColor);
	}
}

/*
*********************************************************************************************************
*	函 数 名: LCD_GetPixel
*	功能说明: 读取1个像素
*	形    参:
*			_usX,_usY : 像素坐标
*			_usColor  : 像素颜色
*	返 回 值: RGB颜色值
*********************************************************************************************************
*/
uint16_t LCD_GetPixel(uint16_t _usX, uint16_t _usY)
{
	uint16_t usRGB;

	if (g_ChipID == IC_8875)
	{
		usRGB = RA8875_GetPixel(_usX, _usY);
	}
	else if (g_ChipID == IC_9488)
	{
		usRGB = ILI9488_GetPixel(_usX, _usY);
	}
	else
	{
		usRGB = SPFD5420_GetPixel(_usX, _usY);
	}

	return usRGB;
}

/*
*********************************************************************************************************
*	函 数 名: LCD_DrawLine
*	功能说明: 采用 Bresenham 算法，在2点间画一条直线。
*	形    参:
*			_usX1, _usY1 : 起始点坐标
*			_usX2, _usY2 : 终止点Y坐标
*			_usColor     : 颜色
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_DrawLine(uint16_t _usX1 , uint16_t _usY1 , uint16_t _usX2 , uint16_t _usY2 , uint16_t _usColor)
{
	if (g_ChipID == IC_8875)
	{
		RA8875_DrawLine(_usX1 , _usY1 , _usX2, _usY2 , _usColor);
	}
	else if (g_ChipID == IC_9488)
	{
		ILI9488_DrawLine(_usX1 , _usY1 , _usX2, _usY2 , _usColor);
	}
	else
	{
		SPFD5420_DrawLine(_usX1 , _usY1 , _usX2, _usY2 , _usColor);
	}
}

/*
*********************************************************************************************************
*	函 数 名: LCD_DrawPoints
*	功能说明: 采用 Bresenham 算法，绘制一组点，并将这些点连接起来。可用于波形显示。
*	形    参:
*			x, y     : 坐标数组
*			_usColor : 颜色
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_DrawPoints(uint16_t *x, uint16_t *y, uint16_t _usSize, uint16_t _usColor)
{
	uint16_t i;

	for (i = 0 ; i < _usSize - 1; i++)
	{
		LCD_DrawLine(x[i], y[i], x[i + 1], y[i + 1], _usColor);
	}
}

/*
*********************************************************************************************************
*	函 数 名: LCD_DrawRect
*	功能说明: 绘制水平放置的矩形。
*	形    参:
*			_usX,_usY: 矩形左上角的坐标
*			_usHeight : 矩形的高度
*			_usWidth  : 矩形的宽度
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_DrawRect(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t _usColor)
{
	if (g_ChipID == IC_8875)
	{
		RA8875_DrawRect(_usX, _usY, _usHeight, _usWidth, _usColor);
	}
	else if (g_ChipID == IC_9488)
	{
		ILI9488_DrawRect(_usX, _usY, _usHeight, _usWidth, _usColor);
	}
	else
	{
		SPFD5420_DrawRect(_usX, _usY, _usHeight, _usWidth, _usColor);
	}
}

/*
*********************************************************************************************************
*	函 数 名: LCD_Fill_Rect
*	功能说明: 用一个颜色值填充一个矩形。【emWin 中有同名函数 LCD_FillRect，因此加了下划线区分】
*	形    参:
*			_usX,_usY: 矩形左上角的坐标
*			_usHeight : 矩形的高度
*			_usWidth  : 矩形的宽度
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_Fill_Rect(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t _usColor)
{
	if (g_ChipID == IC_8875)
	{
		RA8875_FillRect(_usX, _usY, _usHeight, _usWidth, _usColor);
	}
	else if (g_ChipID == IC_9488)
	{
		ILI9488_FillRect(_usX, _usY, _usHeight, _usWidth, _usColor);
	}
	else
	{
		uint32_t i;
		for (i = 0; i < _usHeight; i++)
		{
			SPFD5420_DrawHLine(_usX, _usY + i, _usX + _usWidth - 1, _usColor);
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: LCD_DrawCircle
*	功能说明: 绘制一个圆，笔宽为1个像素
*	形    参:
*			_usX,_usY  : 圆心的坐标
*			_usRadius  : 圆的半径
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_DrawCircle(uint16_t _usX, uint16_t _usY, uint16_t _usRadius, uint16_t _usColor)
{
	if (g_ChipID == IC_8875)
	{
		RA8875_DrawCircle(_usX, _usY, _usRadius, _usColor);
	}
	else if (g_ChipID == IC_9488)
	{
		ILI9488_DrawCircle(_usX, _usY, _usRadius, _usColor);
	}
	else
	{
		SPFD5420_DrawCircle(_usX, _usY, _usRadius, _usColor);
	}
}

/*
*********************************************************************************************************
*	函 数 名: LCD_DrawBMP
*	功能说明: 在LCD上显示一个BMP位图，位图点阵扫描次序: 从左到右，从上到下
*	形    参:
*			_usX, _usY : 图片的坐标
*			_usHeight  : 图片高度
*			_usWidth   : 图片宽度
*			_ptr       : 图片点阵指针
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_DrawBMP(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t *_ptr)
{
	if (g_ChipID == IC_8875)
	{
		RA8875_DrawBMP(_usX, _usY, _usHeight, _usWidth, _ptr);
	}
	else if (g_ChipID == IC_9488)
	{
		ILI9488_DrawBMP(_usX, _usY, _usHeight, _usWidth, _ptr);
	}
	else
	{
		SPFD5420_DrawBMP(_usX, _usY, _usHeight, _usWidth, _ptr);
	}
}

/*
*********************************************************************************************************
*	函 数 名: LCD_DrawWin
*	功能说明: 在LCD上绘制一个窗口
*	形    参: 结构体指针
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_DrawWin(WIN_T *_pWin)
{
#if 1
	uint16_t TitleHegiht;

	TitleHegiht = 20;

	/* 绘制窗口外框 */
	LCD_DrawRect(_pWin->Left, _pWin->Top, _pWin->Height, _pWin->Width, WIN_BORDER_COLOR);
	LCD_DrawRect(_pWin->Left + 1, _pWin->Top + 1, _pWin->Height - 2, _pWin->Width - 2, WIN_BORDER_COLOR);

	/* 窗口标题栏 */
	LCD_Fill_Rect(_pWin->Left + 2, _pWin->Top + 2, TitleHegiht, _pWin->Width - 4, WIN_TITLE_COLOR);

	/* 窗体填充 */
	LCD_Fill_Rect(_pWin->Left + 2, _pWin->Top + TitleHegiht + 2, _pWin->Height - 4 - TitleHegiht,
		_pWin->Width - 4, WIN_BODY_COLOR);

	LCD_DispStr(_pWin->Left + 3, _pWin->Top + 2, _pWin->pCaption, _pWin->Font);
#else
	if (g_ChipID == IC_8875)
	{
		uint16_t TitleHegiht;

		TitleHegiht = 28;

		/* 绘制窗口外框 */
		RA8875_DrawRect(_pWin->Left, _pWin->Top, _pWin->Height, _pWin->Width, WIN_BORDER_COLOR);
		RA8875_DrawRect(_pWin->Left + 1, _pWin->Top + 1, _pWin->Height - 2, _pWin->Width - 2, WIN_BORDER_COLOR);

		/* 窗口标题栏 */
		RA8875_FillRect(_pWin->Left + 2, _pWin->Top + 2, TitleHegiht, _pWin->Width - 4, WIN_TITLE_COLOR);

		/* 窗体填充 */
		RA8875_FillRect(_pWin->Left + 2, _pWin->Top + TitleHegiht + 2, _pWin->Height - 4 - TitleHegiht, _pWin->Width - 4, WIN_BODY_COLOR);

		//RA8875_SetFont(_pWin->Font.FontCode, 0, 0);
		RA8875_SetFont(RA_FONT_24, 0, 0);

		RA8875_SetBackColor(WIN_TITLE_COLOR);
		RA8875_SetFrontColor(WIN_CAPTION_COLOR);
		RA8875_DispStr(_pWin->Left + 3, _pWin->Top + 2, _pWin->Caption);
	}
	else
	{
		;
	}
#endif
}


/*
*********************************************************************************************************
*	函 数 名: LCD_DrawIcon
*	功能说明: 在LCD上绘制一个图标，四角自动切为弧脚
*	形    参: _pIcon : 图标结构
*			  _tFont : 字体属性
*			  _ucFocusMode : 焦点模式。0 表示正常图标  1表示选中的图标
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_DrawIcon(const ICON_T *_tIcon, FONT_T *_tFont, uint8_t _ucFocusMode)
{
	const uint16_t *p;
	uint16_t usNewRGB;
	uint16_t x, y;		/* 用于记录窗口内的相对坐标 */

	p = _tIcon->pBmp;
	for (y = 0; y < _tIcon->Height; y++)
	{
		for (x = 0; x < _tIcon->Width; x++)
		{
			usNewRGB = *p++;	/* 读取图标的颜色值后指针加1 */
			/* 将图标的4个直角切割为弧角，弧角外是背景图标 */
			if ((y == 0 && (x < 6 || x > _tIcon->Width - 7)) ||
				(y == 1 && (x < 4 || x > _tIcon->Width - 5)) ||
				(y == 2 && (x < 3 || x > _tIcon->Width - 4)) ||
				(y == 3 && (x < 2 || x > _tIcon->Width - 3)) ||
				(y == 4 && (x < 1 || x > _tIcon->Width - 2)) ||
				(y == 5 && (x < 1 || x > _tIcon->Width - 2))	||

				(y == _tIcon->Height - 1 && (x < 6 || x > _tIcon->Width - 7)) ||
				(y == _tIcon->Height - 2 && (x < 4 || x > _tIcon->Width - 5)) ||
				(y == _tIcon->Height - 3 && (x < 3 || x > _tIcon->Width - 4)) ||
				(y == _tIcon->Height - 4 && (x < 2 || x > _tIcon->Width - 3)) ||
				(y == _tIcon->Height - 5 && (x < 1 || x > _tIcon->Width - 2)) ||
				(y == _tIcon->Height - 6 && (x < 1 || x > _tIcon->Width - 2))
				)
			{
				;
			}
			else
			{
				if (_ucFocusMode != 0)	/* 1表示选中的图标 */
				{
					/* 降低原始像素的亮度，实现图标被激活选中的效果 */
					uint16_t R,G,B;
					uint16_t bright = 15;

					/* rrrr rggg gggb bbbb */
					R = (usNewRGB & 0xF800) >> 11;
					G = (usNewRGB & 0x07E0) >> 5;
					B =  usNewRGB & 0x001F;
					if (R > bright)
					{
						R -= bright;
					}
					else
					{
						R = 0;
					}
					if (G > 2 * bright)
					{
						G -= 2 * bright;
					}
					else
					{
						G = 0;
					}
					if (B > bright)
					{
						B -= bright;
					}
					else
					{
						B = 0;
					}
					usNewRGB = (R << 11) + (G << 5) + B;
				}

				LCD_PutPixel(x + _tIcon->Left, y + _tIcon->Top, usNewRGB);
			}
		}
	}

	/* 绘制图标下的文字 */
	{
		uint16_t len;
		uint16_t width;

		len = strlen(_tIcon->Text);

		if  (len == 0)
		{
			return;	/* 如果图标文本长度为0，则不显示 */
		}

		/* 计算文本的总宽度 */
		if (_tFont->FontCode == FC_ST_12)		/* 12点阵 */
		{
			width = 6 * (len + _tFont->Space);
		}
		else	/* FC_ST_16 */
		{
			width = 8 * (len + _tFont->Space);
		}


		/* 水平居中 */
		x = (_tIcon->Left + _tIcon->Width / 2) - width / 2;
		y = _tIcon->Top + _tIcon->Height + 2;
		LCD_DispStr(x, y, (char *)_tIcon->Text, _tFont);
	}
}

/*
*********************************************************************************************************
*	函 数 名: LCD_Blend565
*	功能说明: 对像素透明化 颜色混合
*	形    参: src : 原始像素
*			  dst : 混合的颜色
*			  alpha : 透明度 0-32
*	返 回 值: 无
*********************************************************************************************************
*/
uint16_t LCD_Blend565(uint16_t src, uint16_t dst, uint8_t alpha)
{
	uint32_t src2;
	uint32_t dst2;

	src2 = ((src << 16) |src) & 0x07E0F81F;
	dst2 = ((dst << 16) | dst) & 0x07E0F81F;
	dst2 = ((((dst2 - src2) * alpha) >> 5) + src2) & 0x07E0F81F;
	return (dst2 >> 16) | dst2;
}

/*
*********************************************************************************************************
*	函 数 名: LCD_DrawIcon32
*	功能说明: 在LCD上绘制一个图标, 带有透明信息的位图(32位， RGBA). 图标下带文字
*	形    参: _pIcon : 图标结构
*			  _tFont : 字体属性
*			  _ucFocusMode : 焦点模式。0 表示正常图标  1表示选中的图标
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_DrawIcon32(const ICON_T *_tIcon, FONT_T *_tFont, uint8_t _ucFocusMode)
{
	const uint8_t *p;
	uint16_t usOldRGB, usNewRGB;
	int16_t x, y;		/* 用于记录窗口内的相对坐标 */
	uint8_t R1,G1,B1,A;	/* 新像素色彩分量 */
	uint8_t R0,G0,B0;	/* 旧像素色彩分量 */

	p = (const uint8_t *)_tIcon->pBmp;
	p += 54;		/* 直接指向图像数据区 */

	/* 按照BMP位图次序，从左至右，从上至下扫描 */
	for (y = _tIcon->Height - 1; y >= 0; y--)
	{
		for (x = 0; x < _tIcon->Width; x++)
		{
			B1 = *p++;
			G1 = *p++;
			R1 = *p++;
			A = *p++;	/* Alpha 值(透明度)，0-255, 0表示透明，1表示不透明, 中间值表示透明度 */

			if (A == 0x00)	/* 需要透明,显示背景 */
			{
				;	/* 不用刷新背景 */
			}
			else if (A == 0xFF)	/* 完全不透明， 显示新像素 */
			{
				usNewRGB = RGB(R1, G1, B1);
				if (_ucFocusMode == 1)
				{
					usNewRGB = LCD_Blend565(usNewRGB, CL_YELLOW, 10);
				}
				LCD_PutPixel(x + _tIcon->Left, y + _tIcon->Top, usNewRGB);
			}
			else 	/* 半透明 */
			{
				/* 计算公式： 实际显示颜色 = 前景颜色 * Alpha / 255 + 背景颜色 * (255-Alpha) / 255 */
				usOldRGB = LCD_GetPixel(x + _tIcon->Left, y + _tIcon->Top);
				//usOldRGB = 0xFFFF;
				R0 = RGB565_R(usOldRGB);
				G0 = RGB565_G(usOldRGB);
				B0 = RGB565_B(usOldRGB);

				R1 = (R1 * A) / 255 + R0 * (255 - A) / 255;
				G1 = (G1 * A) / 255 + G0 * (255 - A) / 255;
				B1 = (B1 * A) / 255 + B0 * (255 - A) / 255;
				usNewRGB = RGB(R1, G1, B1);
				if (_ucFocusMode == 1)
				{
					usNewRGB = LCD_Blend565(usNewRGB, CL_YELLOW, 10);
				}
				LCD_PutPixel(x + _tIcon->Left, y + _tIcon->Top, usNewRGB);
			}
		}
	}

	/* 绘制图标下的文字 */
	{
		uint16_t len;
		uint16_t width;

		len = strlen(_tIcon->Text);

		if  (len == 0)
		{
			return;	/* 如果图标文本长度为0，则不显示 */
		}

		/* 计算文本的总宽度 */
		if (_tFont->FontCode == FC_ST_12)		/* 12点阵 */
		{
			width = 6 * (len + _tFont->Space);
		}
		else	/* FC_ST_16 */
		{
			width = 8 * (len + _tFont->Space);
		}


		/* 水平居中 */
		x = (_tIcon->Left + _tIcon->Width / 2) - width / 2;
		y = _tIcon->Top + _tIcon->Height + 2;
		LCD_DispStr(x, y, (char *)_tIcon->Text, _tFont);
	}
}

/*
*********************************************************************************************************
*	函 数 名: LCD_DrawBmp32
*	功能说明: 在LCD上绘制一个32位的BMP图, 带有透明信息的位图(32位， RGBA)
*	形    参: _usX, _usY : 显示坐标
*			  _usHeight, _usWidth : 图片高度和宽度
*			  _pBmp : 图片数据（带BMP文件头）
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_DrawBmp32(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint8_t *_pBmp)
{
	const uint8_t *p;
	uint16_t usOldRGB, usNewRGB;
	int16_t x, y;		/* 用于记录窗口内的相对坐标 */
	uint8_t R1,G1,B1,A;	/* 新像素色彩分量 */
	uint8_t R0,G0,B0;	/* 旧像素色彩分量 */

	p = (const uint8_t *)_pBmp;
	p += 54;		/* 直接指向图像数据区 */

	/* 按照BMP位图次序，从左至右，从上至下扫描 */
	for (y = _usHeight - 1; y >= 0; y--)
	{
		for (x = 0; x < _usWidth; x++)
		{
			B1 = *p++;
			G1 = *p++;
			R1 = *p++;
			A = *p++;	/* Alpha 值(透明度)，0-255, 0表示透明，1表示不透明, 中间值表示透明度 */

			if (A == 0x00)	/* 需要透明,显示背景 */
			{
				;	/* 不用刷新背景 */
			}
			else if (A == 0xFF)	/* 完全不透明， 显示新像素 */
			{
				usNewRGB = RGB(R1, G1, B1);
				//if (_ucFocusMode == 1)
				//{
				//	usNewRGB = Blend565(usNewRGB, CL_YELLOW, 10);
				//}
				LCD_PutPixel(x + _usX, y + _usY, usNewRGB);
			}
			else 	/* 半透明 */
			{
				/* 计算公式： 实际显示颜色 = 前景颜色 * Alpha / 255 + 背景颜色 * (255-Alpha) / 255 */
				usOldRGB = LCD_GetPixel(x + _usX, y + _usY);
				R0 = RGB565_R(usOldRGB);
				G0 = RGB565_G(usOldRGB);
				B0 = RGB565_B(usOldRGB);

				R1 = (R1 * A) / 255 + R0 * (255 - A) / 255;
				G1 = (G1 * A) / 255 + G0 * (255 - A) / 255;
				B1 = (B1 * A) / 255 + B0 * (255 - A) / 255;
				usNewRGB = RGB(R1, G1, B1);
				//if (_ucFocusMode == 1)
				//{
				//	usNewRGB = Blend565(usNewRGB, CL_YELLOW, 10);
				//}
				LCD_PutPixel(x + _usX, y + _usY, usNewRGB);
			}
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: LCD_DrawLabel
*	功能说明: 绘制一个文本标签
*	形    参: 结构体指针
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_DrawLabel(LABEL_T *_pLabel)
{
#if 1
	char dispbuf[256];
	uint16_t i;
	uint16_t NewLen;

	NewLen = strlen(_pLabel->pCaption);

	if (NewLen > _pLabel->MaxLen)
	{
		LCD_DispStr(_pLabel->Left, _pLabel->Top, _pLabel->pCaption, _pLabel->Font);
		_pLabel->MaxLen = NewLen;
	}
	else
	{
		for (i = 0; i < NewLen; i++)
		{
			dispbuf[i] = _pLabel->pCaption[i];
		}
		for (; i < _pLabel->MaxLen; i++)
		{
			dispbuf[i] = ' ';		/* 末尾填充空格 */
		}
		dispbuf[i] = 0;
		LCD_DispStr(_pLabel->Left, _pLabel->Top, dispbuf, _pLabel->Font);
	}
#else
	if (g_ChipID == IC_8875)
	{
		RA8875_SetFont(_pLabel->Font->FontCode, 0, 0);	/* 设置32点阵字体，行间距=0，字间距=0 */

		RA8875_SetBackColor(_pLabel->Font->BackColor);
		RA8875_SetFrontColor(_pLabel->Font->FrontColor);

		RA8875_DispStr(_pLabel->Left, _pLabel->Top, _pLabel->Caption);
	}
	else
	{

	}
#endif
}

/*
*********************************************************************************************************
*	函 数 名: LCD_DrawCheckBox
*	功能说明: 绘制一个检查框
*	形    参: 结构体指针
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_DrawCheckBox(CHECK_T *_pCheckBox)
{
#if 1
	uint16_t x, y;

	/* 目前只做了16点阵汉字的大小 */

	/* 绘制外框 */
	x = _pCheckBox->Left;
	LCD_DrawRect(x, _pCheckBox->Top, CHECK_BOX_H, CHECK_BOX_W, CHECK_BOX_BORDER_COLOR);
	LCD_DrawRect(x + 1, _pCheckBox->Top + 1, CHECK_BOX_H - 2, CHECK_BOX_W - 2, CHECK_BOX_BORDER_COLOR);
	LCD_Fill_Rect(x + 2, _pCheckBox->Top + 2, CHECK_BOX_H - 4, CHECK_BOX_W - 4, CHECK_BOX_BACK_COLOR);

	/* 绘制文本标签 */
	x = _pCheckBox->Left + CHECK_BOX_W + 2;
	y = _pCheckBox->Top + CHECK_BOX_H / 2 - 8;
	LCD_DispStr(x, y, _pCheckBox->pCaption, _pCheckBox->Font);

	if (_pCheckBox->Checked)
	{
		FONT_T font;

	    font.FontCode = FC_ST_16;
		font.BackColor = CL_MASK;
		font.FrontColor = CHECK_BOX_CHECKED_COLOR;	/* 钩的颜色 */
		font.Space = 0;
		x = _pCheckBox->Left;
		LCD_DispStr(x + 3, _pCheckBox->Top + 3, "√", &font);
	}
#else
	if (g_ChipID == IC_8875)
	{
		uint16_t x;

		RA8875_SetFont(_pCheckBox->Font.FontCode, 0, 0);	/* 设置32点阵字体，行间距=0，字间距=0 */

		/* 绘制标签 */
		//RA8875_SetBackColor(_pCheckBox->Font.BackColor);
		RA8875_SetBackColor(WIN_BODY_COLOR);
		RA8875_SetFrontColor(_pCheckBox->Font.FrontColor);
		RA8875_DispStr(_pCheckBox->Left, _pCheckBox->Top, _pCheckBox->Caption);

		/* 绘制外框 */
		x = _pCheckBox->Left + _pCheckBox->Width - CHECK_BOX_W;
		RA8875_DrawRect(x, _pCheckBox->Top, CHECK_BOX_H, CHECK_BOX_W, CHECK_BOX_BORDER_COLOR);
		RA8875_DrawRect(x + 1, _pCheckBox->Top + 1, CHECK_BOX_H - 2, CHECK_BOX_W - 2, CHECK_BOX_BORDER_COLOR);
		RA8875_FillRect(x + 2, _pCheckBox->Top + 2, CHECK_BOX_H - 4, CHECK_BOX_W - 4, CHECK_BOX_BACK_COLOR);

		if (_pCheckBox->Checked)
		{
			RA8875_SetBackColor(CHECK_BOX_BACK_COLOR);
			RA8875_SetFrontColor(CL_RED);
			RA8875_DispStr(x + 3, _pCheckBox->Top + 3, "√");
		}
	}
	else
	{

	}
#endif

}

/*
*********************************************************************************************************
*	函 数 名: LCD_DrawEdit
*	功能说明: 在LCD上绘制一个编辑框
*	形    参: _pEdit 编辑框结构体指针
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_DrawEdit(EDIT_T *_pEdit)
{
#if 1
	uint16_t len, x, y;
	uint8_t width;

	/* 仿XP风格，平面编辑框 */
	LCD_DrawRect(_pEdit->Left, _pEdit->Top, _pEdit->Height, _pEdit->Width, EDIT_BORDER_COLOR);
	LCD_Fill_Rect(_pEdit->Left + 1, _pEdit->Top + 1, _pEdit->Height - 2, _pEdit->Width - 2, EDIT_BACK_COLOR);

	/* 文字居中 */
	if (_pEdit->Font->FontCode == FC_ST_12)
	{
		width = 6;
	}
	else
	{
		width = 8;
	}
	len = strlen(_pEdit->pCaption);
	x = _pEdit->Left +  (_pEdit->Width - len * width) / 2;
	y = _pEdit->Top + _pEdit->Height / 2 - width;

	LCD_DispStr(x, y, _pEdit->pCaption, _pEdit->Font);
#else
	if (g_ChipID == IC_8875)
	{
		uint16_t len, x;

		/* 仿XP风格，平面编辑框 */
		RA8875_DrawRect(_pEdit->Left, _pEdit->Top, _pEdit->Height, _pEdit->Width, EDIT_BORDER_COLOR);
		RA8875_FillRect(_pEdit->Left + 1, _pEdit->Top + 1, _pEdit->Height - 2, _pEdit->Width - 2, EDIT_BACK_COLOR);

		RA8875_SetFont(_pEdit->Font.FontCode, 0, 0);	/* 设置32点阵字体，行间距=0，字间距=0 */
		RA8875_SetFrontColor(_pEdit->Font.FrontColor);
		RA8875_SetBackColor(EDIT_BACK_COLOR);

		/* 文字居中 */
		len = strlen(_pEdit->Caption);
		x = (_pEdit->Width - len * 16) / 2;

		RA8875_DispStr(_pEdit->Left + x, _pEdit->Top + 3, _pEdit->Caption);
	}
	else
	{
		//SPFD5420_DrawCircle(_usX, _usY, _usRadius, _usColor);
	}
#endif
}

/*
*********************************************************************************************************
*	函 数 名: LCD_DrawEdit
*	功能说明: 在LCD上绘制一个编辑框
*	形    参:
*			_usX, _usY : 图片的坐标
*			_usHeight  : 图片高度
*			_usWidth   : 图片宽度
*			_ptr       : 图片点阵指针
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_DrawButton(BUTTON_T *_pBtn)
{
#if 1
	uint16_t x, y, h;
	FONT_T font;	/* 按钮激活时，需要更改字体设置，因此需要一个变量来保存 */

	font.FontCode = _pBtn->Font->FontCode;
	font.FrontColor = _pBtn->Font->FrontColor;
	font.BackColor = _pBtn->Font->BackColor;
	font.Space = _pBtn->Font->Space;	
			
	if (_pBtn->Focus == 1)
	{
		font.BackColor = BUTTON_ACTIVE_COLOR;
	}
	else
	{
		/* 按钮的背景色统一管理，不能单独指定 */
		font.BackColor = BUTTON_BACK_COLOR;
	}
	
	/* 仿XP风格，平面编辑框 */
	LCD_DrawRect(_pBtn->Left, _pBtn->Top, _pBtn->Height, _pBtn->Width, BUTTON_BORDER_COLOR);
	LCD_DrawRect(_pBtn->Left + 1, _pBtn->Top + 1, _pBtn->Height - 2, _pBtn->Width - 2, BUTTON_BORDER1_COLOR);
	LCD_DrawRect(_pBtn->Left + 2, _pBtn->Top + 2, _pBtn->Height - 4, _pBtn->Width - 4, BUTTON_BORDER2_COLOR);

	h =  LCD_GetFontHeight(&font);
	x = _pBtn->Left + 3;
	y = _pBtn->Top + _pBtn->Height / 2 - h / 2;		
	
	LCD_Fill_Rect(_pBtn->Left + 3, _pBtn->Top + 3, _pBtn->Height - 6, _pBtn->Width - 6, font.BackColor);	/* 选中后的底色 */
	LCD_DispStrEx(x, y, _pBtn->pCaption, &font, _pBtn->Width - 6, ALIGN_CENTER);	/* 水平居中 */		

#else
	if (g_ChipID == IC_8875)
	{
		uint16_t len, x, y;

		if (_pBtn->Focus == 1)
		{
			/* 仿XP风格，平面编辑框 */
			RA8875_DrawRect(_pBtn->Left, _pBtn->Top, _pBtn->Height, _pBtn->Width, BUTTON_BORDER_COLOR);
			RA8875_DrawRect(_pBtn->Left + 1, _pBtn->Top + 1, _pBtn->Height - 2, _pBtn->Width - 2, BUTTON_BORDER1_COLOR);
			RA8875_DrawRect(_pBtn->Left + 2, _pBtn->Top + 2, _pBtn->Height - 4, _pBtn->Width - 4, BUTTON_BORDER2_COLOR);

			RA8875_FillRect(_pBtn->Left + 3, _pBtn->Top + 3, _pBtn->Height - 6, _pBtn->Width - 6, BUTTON_ACTIVE_COLOR);

			RA8875_SetBackColor(BUTTON_ACTIVE_COLOR);
		}
		else
		{
			/* 仿XP风格，平面编辑框 */
			RA8875_DrawRect(_pBtn->Left, _pBtn->Top, _pBtn->Height, _pBtn->Width, BUTTON_BORDER_COLOR);
			RA8875_DrawRect(_pBtn->Left + 1, _pBtn->Top + 1, _pBtn->Height - 2, _pBtn->Width - 2, BUTTON_BORDER1_COLOR);
			RA8875_DrawRect(_pBtn->Left + 2, _pBtn->Top + 2, _pBtn->Height - 4, _pBtn->Width - 4, BUTTON_BORDER2_COLOR);

			RA8875_FillRect(_pBtn->Left + 3, _pBtn->Top + 3, _pBtn->Height - 6, _pBtn->Width - 6, BUTTON_BACK_COLOR);

			RA8875_SetBackColor(BUTTON_BACK_COLOR);
		}

		#if 1	/* 按钮文字字体和颜色固定 */
			if (strcmp(_pBtn->Caption, "←") == 0)	/* 退格键特殊处理 */
			{
				/* 退格键符号是单像素笔画，太细了不协调，因此特殊处理 */
				RA8875_SetFont(RA_FONT_16, 0, 0);
				RA8875_SetFrontColor(CL_BLACK);
				RA8875_SetTextZoom(RA_SIZE_X2, RA_SIZE_X2);	/* 放大2倍 */
			}
			else
			{
				RA8875_SetFont(RA_FONT_16, 0, 0);
				RA8875_SetFrontColor(CL_BLACK);
				RA8875_SetTextZoom(RA_SIZE_X1, RA_SIZE_X1);	/* 放大1倍 */
			}
		#else	/* 按钮文字字体和颜色有应用程序指定 */
			RA8875_SetFont(_pBtn->Font.FontCode, 0, 0);
			RA8875_SetFrontColor(_pBtn->Font.FrontColor);
		#endif

		/* 文字居中 */
		len = strlen(_pBtn->Caption);

		/* 此处统计不等宽字符有问题。暂时特殊处理下 */
		if (len != 3)
		{
			x = _pBtn->Left + (_pBtn->Width - len * 16) / 2;
		}
		else
		{
			x = _pBtn->Left + (_pBtn->Width - len * 20) / 2;
		}

		/* 对特殊字符特殊处理 */
		if ((len == 1) && (_pBtn->Caption[0] == '.'))
		{
			y = _pBtn->Top + 3;
			x += 3;
		}
		else
		{
			y = _pBtn->Top + 3;
		}

		RA8875_DispStr(x, y, _pBtn->Caption);

		RA8875_SetTextZoom(RA_SIZE_X1, RA_SIZE_X1);	/* 还原放大1倍 */
	}
#endif
}

/*
*********************************************************************************************************
*	函 数 名: LCD_DrawGroupBox
*	功能说明: 在LCD上绘制一个分组框
*	形    参: _pBox 分组框
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_DrawGroupBox(GROUP_T *_pBox)
{
	uint16_t x, y;

	/* 画阴影线 */
	LCD_DrawRect(_pBox->Left + 1, _pBox->Top + 5, _pBox->Height, _pBox->Width - 1, CL_BOX_BORDER2);

	/* 画主框线 */
	LCD_DrawRect(_pBox->Left, _pBox->Top + 4, _pBox->Height, _pBox->Width - 1, CL_BOX_BORDER1);

	/* 显示分组框标题（文字在左上角） */
	x = _pBox->Left + 9;
	y = _pBox->Top;
	LCD_DispStr(x, y, _pBox->pCaption, _pBox->Font);
}

/*
*********************************************************************************************************
*	函 数 名: LCD_DispControl
*	功能说明: 绘制控件
*	形    参: _pControl 控件指针
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_DispControl(void *_pControl)
{
	uint8_t id;

	id = *(uint8_t *)_pControl;	/* 读取ID */

	switch (id)
	{
		case ID_ICON:
			//void LCD_DrawIcon(const ICON_T *_tIcon, FONT_T *_tFont, uint8_t _ucFocusMode);
			break;

		case ID_WIN:
			LCD_DrawWin((WIN_T *)_pControl);
			break;

		case ID_LABEL:
			LCD_DrawLabel((LABEL_T *)_pControl);
			break;

		case ID_BUTTON:
			LCD_DrawButton((BUTTON_T *)_pControl);
			break;

		case ID_CHECK:
			LCD_DrawCheckBox((CHECK_T *)_pControl);
			break;

		case ID_EDIT:
			LCD_DrawEdit((EDIT_T *)_pControl);
			break;

		case ID_GROUP:
			LCD_DrawGroupBox((GROUP_T *)_pControl);
			break;
	}
}

/*
*********************************************************************************************************
*	函 数 名: LCD_CtrlLinesConfig
*	功能说明: 配置LCD控制口线，FSMC管脚设置为复用功能
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void LCD_CtrlLinesConfig(void)
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

/*
*********************************************************************************************************
*	函 数 名: LCD_FSMCConfig
*	功能说明: 配置FSMC并口访问时序
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void LCD_FSMCConfig(void)
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


/*
*********************************************************************************************************
*	函 数 名: LCD_SetPwmBackLight
*	功能说明: 初始化控制LCD背景光的GPIO,配置为PWM模式。
*			当关闭背光时，将CPU IO设置为浮动输入模式（推荐设置为推挽输出，并驱动到低电平)；将TIM3关闭 省电
*	形    参:  _bright 亮度，0是灭，255是最亮
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_SetPwmBackLight(uint8_t _bright)
{
	/* STM32-V4开发板，PB1/TIM3_CH4/TIM8_CH3N 控制背光PWM ； 因为 TIM3用于红外解码。因此用TIM8_CH3N做背光PWM */
	//bsp_SetTIMOutPWM(GPIOB, GPIO_Pin_1, TIM3, 4, 100, (_bright * 10000) /255);	// TIM3_CH4
	bsp_SetTIMOutPWM_N(GPIOB, GPIO_Pin_1, TIM8, 3, 100, (_bright * 10000) /255);	// TIM8_CH3N
}

/*
*********************************************************************************************************
*	函 数 名: LCD_SetBackLight
*	功能说明: 初始化控制LCD背景光的GPIO,配置为PWM模式。
*			当关闭背光时，将CPU IO设置为浮动输入模式（推荐设置为推挽输出，并驱动到低电平)；将TIM3关闭 省电
*	形    参: _bright 亮度，0是灭，255是最亮
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_SetBackLight(uint8_t _bright)
{
	s_ucBright =  _bright;	/* 保存背光值 */

	if (g_ChipID == IC_8875)
	{
		RA8875_SetBackLight(_bright);
	}
	else
	{
		LCD_SetPwmBackLight(_bright);
	}
}

/*
*********************************************************************************************************
*	函 数 名: LCD_GetBackLight
*	功能说明: 获得背光亮度参数
*	形    参: 无
*	返 回 值: 背光亮度参数
*********************************************************************************************************
*/
uint8_t LCD_GetBackLight(void)
{
	return s_ucBright;
}
/*
*********************************************************************************************************
*	函 数 名: LCD_SetDirection
*	功能说明: 设置显示屏显示方向（横屏 竖屏）
*	形    参: 显示方向代码 0 横屏正常, 1=横屏180度翻转, 2=竖屏, 3=竖屏180度翻转
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_SetDirection(uint8_t _dir)
{
	g_LcdDirection =  _dir;		/* 保存在全局变量 */

	if (g_ChipID == IC_8875)
	{
		RA8875_SetDirection(_dir);
	}
	else
	{
		ILI9488_SetDirection(_dir);
	}
}


/*
*********************************************************************************************************
*	函 数 名: LCD_ButtonTouchDown
*	功能说明: 判断按钮是否被按下. 检查触摸坐标是否在按钮的范围之内。并重绘按钮。
*	形    参:  _btn : 按钮对象
*			  _usX, _usY: 触摸坐标
*	返 回 值: 1 表示在范围内
*********************************************************************************************************
*/
uint8_t LCD_ButtonTouchDown(BUTTON_T *_btn, uint16_t _usX, uint16_t _usY)
{
	if ((_usX > _btn->Left) && (_usX < _btn->Left + _btn->Width)
		&& (_usY > _btn->Top) && (_usY < _btn->Top + _btn->Height))
	{
		BUTTON_BEEP();	/* 按键提示音 bsp_tft_lcd.h 文件开头可以使能和关闭 */
		_btn->Focus = 1;
		LCD_DrawButton(_btn);
		return 1;
	}
	else
	{
		return 0;
	}
}

/*
*********************************************************************************************************
*	函 数 名: LCD_ButtonTouchRelease
*	功能说明: 判断按钮是否被触摸释放. 并重绘按钮。在触摸释放事件中被调用。
*	形    参:  _btn : 按钮对象
*			  _usX, _usY: 触摸坐标
*	返 回 值: 1 表示在范围内
*********************************************************************************************************
*/
uint8_t LCD_ButtonTouchRelease(BUTTON_T *_btn, uint16_t _usX, uint16_t _usY)
{
	_btn->Focus = 0;
	LCD_DrawButton(_btn);

	if ((_usX > _btn->Left) && (_usX < _btn->Left + _btn->Width)
		&& (_usY > _btn->Top) && (_usY < _btn->Top + _btn->Height))
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
*	函 数 名: LCD_InitButton
*	功能说明: 初始化按钮结构体成员。
*	形    参:  _x, _y : 坐标
*			  _h, _w : 高度和宽度
*			  _pCaption : 按钮文字
*			  _pFont : 按钮字体
*	返 回 值: 无
*********************************************************************************************************
*/
void LCD_InitButton(BUTTON_T *_btn, uint16_t _x, uint16_t _y, uint16_t _h, uint16_t _w, char *_pCaption, FONT_T *_pFont)
{
	_btn->Left = _x;
	_btn->Top = _y;
	_btn->Height = _h;
	_btn->Width = _w;
	_btn->pCaption = _pCaption;	
	_btn->Font = _pFont;
	_btn->Focus = 0;
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
