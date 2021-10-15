/*
*********************************************************************************************************
*
*	模块名称 : RA8875芯片驱动模块
*	文件名称 : LCD_RA8875.c
*	版    本 : V2.9
*	说    明 : RA8875底层驱动函数集。
*	修改记录 :
*		版本号  日期       作者    说明
*		V1.0    2012-08-07 armfly  发布首版
*		V1.1    2012-10-23 armfly  解决绘制矩形函数的BUG（高度宽度计算错误）；增加SPI-4接口模式；
*								   解决几个绘制2D图形的函数判忙条件不真实的问题。
*								   触摸校准增加4点校准
*		V1.2    2012-11-07 armfly  解决4点校准的BUG
*		V1.3    2012-12-12 armfly  解决7寸屏，长线出现麻点的问题
*		V1.4    2013-05-04 armfly  增加进入绘图模式的函数，用于摄像头直接写屏 RA8875_StartDirectDraw
*									RA8875_GetDispMemAddr, RA8875_QuitDirectDraw
*       V1.5    2013-06-12 armfly  为避免IAR告警，去掉 s_WinX，s_WinY, s_WinHeight,s_WinWidth前的__IO修饰
*		V1.6    2013-06-26 armfly  增加函数  RA8875_CtrlGPO() 控制KOUT0-KOUT3 口线的状态
*		V1.7    2013-06-26 armfly  增加函数  RA8875_CtrlGPO() 进入低功耗
*		V1.8    2013-06-29 armfly  修改 RA8875_GetPixel() 函数BUG，需要读2次。并且设置0x40寄存器为读光标不自增
*		V1.9	2013-07-24 armfly  修改 RA8875_DrawLine() 函数, 当起始点和终止点相同时，可能死机.增加特殊处理
*		V2.0	2013-09-22 armfly  解决 RA8875硬件2D绘图的BUG，修改若干函数，解决触摸冲突问题。
*		V2.1    2013-09-26 armfly  将MCU接口相关的函数分离到 bsp_ra8875_port.c 文件。
*		V2.2    2014-01-31 armfly  增加 RA8875_DispBmpInFlash() 函数,显示RA8875外挂串行Flash中的BMP位图
*		V2.3    2014-06-27 armfly  修改 RA8875_DispBmpInFlash() , 增加对 W25Q128 (16MB) 的支持。
*								   初始化7寸屏的分支代码中，降低RA8875的主频到75M，提高稳定性。
*		V2.4    2014-07-05 armfly  增加屏幕旋转和镜像的函数
*		V2.5 	2014-09-06 armfly  
*					(1) RA8875_FillRect() 函数处理宽度和高度为0的情况.
*					(2) RA8875_WriteReg(); RA8875_ReadReg()函数取消static限制，可供其他模块使用
*					(3) 增加横屏、竖屏切换函数. g_LcdDirection 变量保存方向参数0-3。
*					(4) SPI 模式，RA8875_DispStr() 增加 while(RA8875_ReadBusy()); 8080模式不能用这种方式
*					(5) bsp_touch.c 中需要同步更改方向处理。param.c中参数结构体增加方向变量。
*					(6) 解决 RA8875_DispBmpInFlash(), s_ucRA8875Busy 标志处理冲突的问题。
*		V2.6 	2014-10-21 armfly  
*					(1) g_LcdDirection 变量移到 bsp_tft_lcd.c ，因为添加了3.5寸驱动。
*		V2.7	2014-11-09
*					(1) 优化 RA8875_PutPixel函数
*		V2.8	2015-04-02 armfly  解决 RA8875_DrawBMP()不正确的问题，绘图坐标寄存器不正确。
*		V2.9	2015-04-15 armfly  RA8875_InitHard() 针对7寸屏刷图麻点问题。
*					  (某个批次的串行FLASH速度低), 降低PLL速度，由 81.25M修改为75MHz。
*		V3.0	2015-04-21 armfly  修改 RA8875_SetDirection(uint8_t _ucDir)；reg_40H改为01 解决emwin下的显示问题。
*
*	Copyright (C), 2011-2012, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

#include "bsp_ra8875_port.h"

#define FLASH_DUAL_SPEED_EN			/* 双速模式， MOSI和MISO同时读取字库和图库中的数据. */

#define WRTIE_SF_EN					/* 表示使能写串行FLASH功能。初始化时，会配置MCU的SPI接口以及PWM口线 */

static __IO uint16_t s_usTouchX, s_usTouchY;	/* 触摸屏ADC值 */
static __IO uint8_t s_ucRA8875Busy = 0;			/* 忙标志, 其他模块通过 RA8875_IsBusy()函数判断忙 */
static __IO uint8_t s_reg_40H;		/* 0X40寄存器的值 */
static __IO uint8_t s_reg_22H;		/* 0X22寄存器的值 */

/* 保存当前显示窗口的位置和大小，这几个变量由 RA8875_SetDispWin() 进行设置 */
static uint16_t s_WinX = 0;
static uint16_t s_WinY = 0;
static uint16_t s_WinHeight = 272;
static uint16_t s_WinWidth = 480;

static void RA8875_SetCursor(uint16_t _usX, uint16_t _usY);
static void RA8875_SetReadCursor(uint16_t _usX, uint16_t _usY);
static void RA8875_SetTextCursor(uint16_t _usX, uint16_t _usY);

static void BTE_SetTarBlock(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint8_t _ucLayer);
static void BTE_SetOperateCode(uint8_t _ucOperate);
static void BTE_Start(void);

/*
*********************************************************************************************************
*	函 数 名: RA8875_ReadID
*	功能说明: 读取LCD驱动芯片ID
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
uint16_t RA8875_ReadID(void)
{
	RA8875_ConfigGPIO();
	
	return RA8875_ReadReg(0x00);
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_WriteReg
*	功能说明: 写RA8875寄存器. RA8875的寄存器地址和数据都是8bit的
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_WriteReg(uint8_t _ucRegAddr, uint8_t _ucRegValue)
{
	s_ucRA8875Busy = 1;				/* 设置忙标志，避免触摸中断中的程序访问冲突 */

	RA8875_WriteCmd(_ucRegAddr);
	RA8875_WriteData(_ucRegValue);

	s_ucRA8875Busy = 0;
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_WriteReg
*	功能说明: 读RA8875寄存器. 用在主程序。 RA8875_ReadReg_Int(） 用于中断中调用
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
uint8_t RA8875_ReadReg(uint8_t _ucRegAddr)
{
	uint8_t value;
	
	s_ucRA8875Busy = 1;
	
	RA8875_WriteCmd(_ucRegAddr);
	value = RA8875_ReadData();
	
	s_ucRA8875Busy = 0;
	return value;	
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_ReadReg_Int
*	功能说明: 读RA8875寄存器. 用于中断函数中调用。
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
uint8_t RA8875_ReadReg_Int(uint8_t _ucRegAddr)
{
	RA8875_WriteCmd(_ucRegAddr);
	return RA8875_ReadData();
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_InitHard
*	功能说明: 初始化RA8875。本函数不开启背光，主程序应该在初始化完毕后再调用 RA8875_SetBackLight()函数开背光
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_InitHard(void)
{
	uint8_t ucGPIX;
	
	RA8875_ConfigGPIO();	/* 配置GPIO FSMC等 */

	/* 读取 RA8875 芯片额GPIX引脚的电平状态；1表示4.3寸屏；0表示7寸屏
	【备注】这是安富莱电子为了便于统一测试例程，在LCD模块上做的硬件标识。在做产品时，不必自动识别。
	*/
	ucGPIX = RA8875_ReadReg(0xC7);

	if (ucGPIX == 1)	/* 	GPIX = 1 表示 4.3 寸屏 480x272 */
	{
		/* 初始化PLL.  晶振频率为25M */
		RA8875_WriteCmd(0x88);
		RA8875_Delaly1us();		/* 延迟1us */
		RA8875_WriteData(10);	/* PLLDIVM [7] = 0 ;  PLLDIVN [4:0] = 10 */

	    RA8875_Delaly1ms();

		RA8875_WriteCmd(0x89);
		RA8875_Delaly1us();		/* 延迟1us */
		RA8875_WriteData(2);		/* PLLDIVK[2:0] = 2, 除以4 */

		/* RA8875 的内部系统频率 (SYS_CLK) 是结合振荡电路及PLL 电路所产生，频率计算公式如下 :
		  SYS_CLK = FIN * ( PLLDIVN [4:0] +1 ) / ((PLLDIVM+1 ) * ( 2^PLLDIVK [2:0] ))
		          = 25M * (10 + 1) / ((0 + 1) * (2 ^ 2))
				  = 68.75MHz
		*/

		/* REG[88h]或REG[89h]被设定后，为保证PLL 输出稳定，须等待一段「锁频时间」(< 100us)。*/
	    RA8875_Delaly1ms();
		RA8875_Delaly1ms();

		/*
			配置系统控制寄存器。 中文pdf 第18页:

			bit3:2 色彩深度设定 (Color Depth Setting)
				00b : 8-bpp 的通用TFT 接口， i.e. 256 色。
				1xb : 16-bpp 的通用TFT 接口， i.e. 65K 色。	 【选这个】

			bit1:0 MCUIF 选择
				00b : 8-位MCU 接口。
				1xb : 16-位MCU 接口。 【选这个】
		*/
		if (g_RA8875_IF == RA_HARD_8080_16)
		{
			RA8875_WriteReg(0x10, (1 <<3 ) | (1 << 1));	/* 配置16位MCU并口，65K色 */
		}
		else
		{
			RA8875_WriteReg(0x10, (1 <<3 ) | (0 << 1));	/* SPI接口时，配置8位，65K色 */
		}

		/* REG[04h] Pixel Clock Setting Register   PCSR
			bit7  PCLK Inversion
				0 : PDAT 是在PCLK 正缘上升 (Rising Edge) 时被取样。
				1 : PDAT 是在PCLK 负缘下降 (Falling Edge) 时被取样。
			bit1:0 PCLK 频率周期设定
				Pixel Clock  PCLK 频率周期设定。
				00b: PCLK 频率周期= 系统频率周期。
				01b: PCLK 频率周期= 2 倍的系统频率周期。
				10b: PCLK 频率周期= 4 倍的系统频率周期。
				11b: PCLK 频率周期= 8 倍的系统频率周期。
		*/
	    RA8875_WriteReg(0x04, 0x82);    /* 设置PCLK反相 */
	    RA8875_Delaly1ms();

	    //Horizontal set
	    //HDWR//Horizontal Display Width Setting Bit[6:0]
	 	//Horizontal display width(pixels) = (HDWR + 1)*8
	    RA8875_WriteReg(0x14, 0x3B);
	    RA8875_WriteReg(0x15, 0x00);

	    //HNDR//Horizontal Non-Display Period Bit[4:0]
	    //Horizontal Non-Display Period (pixels) = (HNDR + 1)*8
		RA8875_WriteReg(0x16, 0x01);

	    //HSTR//HSYNC Start Position[4:0]
	    //HSYNC Start Position(PCLK) = (HSTR + 1)*8
		RA8875_WriteReg(0x17, 0x00);

	    //HPWR//HSYNC Polarity ,The period width of HSYNC.
	    //HSYNC Width [4:0]   HSYNC Pulse width(PCLK) = (HPWR + 1)*8
		RA8875_WriteReg(0x18, 0x05);

	    //Vertical set
	    //VDHR0 //Vertical Display Height Bit [7:0]
	    //Vertical pixels = VDHR + 1
		RA8875_WriteReg(0x19, 0x0F);

	    //VDHR1 //Vertical Display Height Bit [8]
	    //Vertical pixels = VDHR + 1
		RA8875_WriteReg(0x1A, 0x01);

	    //VNDR0 //Vertical Non-Display Period Bit [7:0]
	    //Vertical Non-Display area = (VNDR + 1)
		RA8875_WriteReg(0x1B, 0x02);

	    //VNDR1 //Vertical Non-Display Period Bit [8]
	    //Vertical Non-Display area = (VNDR + 1)
		RA8875_WriteReg(0x1C, 0x00);

	    //VSTR0 //VSYNC Start Position[7:0]
	    //VSYNC Start Position(PCLK) = (VSTR + 1)
		RA8875_WriteReg(0x1D, 0x07);

	    //VSTR1 //VSYNC Start Position[8]
	    //VSYNC Start Position(PCLK) = (VSTR + 1)
		RA8875_WriteReg(0x1E, 0x00);

	    //VPWR //VSYNC Polarity ,VSYNC Pulse Width[6:0]
	    //VSYNC Pulse Width(PCLK) = (VPWR + 1)
	    RA8875_WriteReg(0x1F, 0x09);


	    g_LcdHeight = LCD_43_HEIGHT;
		g_LcdWidth = LCD_43_WIDTH;
	}
	else	/* GPIX = 0 表示7寸屏(800x480) */
	{
	    g_LcdHeight = LCD_70_HEIGHT;
		g_LcdWidth = LCD_70_WIDTH;

		#if 1	/* 75MHz */
			RA8875_WriteCmd(0x88);
			RA8875_Delaly1us();		/* 延迟1us */
			RA8875_WriteData(11);	/* PLLDIVM [7] = 0 ;  PLLDIVN [4:0] = 11 */

			RA8875_Delaly1ms();

			RA8875_WriteCmd(0x89);
			RA8875_Delaly1us();		/* 延迟1us */
			RA8875_WriteData(2);		/* PLLDIVK[2:0] = 2, 除以4 */
		#else	/* 81.25M */
			/* 初始化PLL.  晶振频率为25M */
			RA8875_WriteCmd(0x88);
			RA8875_Delaly1us();		/* 延迟1us */
			RA8875_WriteData(12);	/* PLLDIVM [7] = 0 ;  PLLDIVN [4:0] = 12 */

			RA8875_Delaly1ms();

			RA8875_WriteCmd(0x89);
			RA8875_Delaly1us();		/* 延迟1us */
			RA8875_WriteData(2);	/* PLLDIVK[2:0] = 2, 除以4 */

			/* RA8875 的内部系统频率 (SYS_CLK) 是结合振荡电路及PLL 电路所产生，频率计算公式如下 :
			  SYS_CLK = FIN * ( PLLDIVN [4:0] +1 ) / ((PLLDIVM+1 ) * ( 2^PLLDIVK [2:0] ))
					  = 25M * (12 + 1) / ((0 + 1) * (2 ^ 2))
					  = 81.25MHz
			*/
		#endif
		

		/* REG[88h]或REG[89h]被设定后，为保证PLL 输出稳定，须等待一段「锁频时间」(< 100us)。*/
	    RA8875_Delaly1ms();

		/*
			配置系统控制寄存器。 中文pdf 第18页:

			bit3:2 色彩深度设定 (Color Depth Setting)
				00b : 8-bpp 的通用TFT 接口， i.e. 256 色。
				1xb : 16-bpp 的通用TFT 接口， i.e. 65K 色。	 【选这个】

			bit1:0 MCUIF 选择
				00b : 8-位MCU 接口。
				1xb : 16-位MCU 接口。 【选这个】
		*/
		if (g_RA8875_IF == RA_HARD_8080_16)
		{
			RA8875_WriteReg(0x10, (1 <<3 ) | (1 << 1));	/* 配置16位MCU并口，65K色 */
		}
		else
		{
			RA8875_WriteReg(0x10, (1 <<3 ) | (0 << 1));	/* SPI接口时，配置8位，65K色 */
		}

		/* REG[04h] Pixel Clock Setting Register (PCSR)
			bit7  PCLK Inversion
				0 : PDAT 是在PCLK 正缘上升 (Rising Edge) 时被取样。
				1 : PDAT 是在PCLK 负缘下降 (Falling Edge) 时被取样。
			bit1:0 PCLK 频率周期设定
				Pixel Clock ,PCLK 频率周期设定。
				00b: PCLK 频率周期= 系统频率周期。
				01b: PCLK 频率周期= 2 倍的系统频率周期。
				10b: PCLK 频率周期= 4 倍的系统频率周期。
				11b: PCLK 频率周期= 8 倍的系统频率周期。
		*/
	    RA8875_WriteReg(0x04, 0x81);
	    RA8875_Delaly1ms();

	#if 1
		/* OTD9960 & OTA7001 设置 */
		RA8875_WriteReg(0x14, 0x63);
		RA8875_WriteReg(0x15, 0x00);
		RA8875_WriteReg(0x16, 0x03);
		RA8875_WriteReg(0x17, 0x03);
		RA8875_WriteReg(0x18, 0x0B);
		RA8875_WriteReg(0x19, 0xDF);
		RA8875_WriteReg(0x1A, 0x01);
		RA8875_WriteReg(0x1B, 0x1F);
		RA8875_WriteReg(0x1C, 0x00);
		RA8875_WriteReg(0x1D, 0x16);
		RA8875_WriteReg(0x1E, 0x00);
		RA8875_WriteReg(0x1F, 0x01);

	#else	/* AT070TN92  setting */
	    //Horizontal set
	    //HDWR//Horizontal Display Width Setting Bit[6:0]
	 	//Horizontal display width(pixels) = (HDWR + 1)*8
	    RA8875_WriteReg(0x14, 0x4F);
	    RA8875_WriteReg(0x15, 0x05);

	    //HNDR//Horizontal Non-Display Period Bit[4:0]
	    //Horizontal Non-Display Period (pixels) = (HNDR + 1)*8
		RA8875_WriteReg(0x16, 0x0F);

	    //HSTR//HSYNC Start Position[4:0]
	    //HSYNC Start Position(PCLK) = (HSTR + 1)*8
		RA8875_WriteReg(0x17, 0x01);

	    //HPWR//HSYNC Polarity ,The period width of HSYNC.
	    //HSYNC Width [4:0]   HSYNC Pulse width(PCLK) = (HPWR + 1)*8
		RA8875_WriteReg(0x18, 0x00);

	    //Vertical set
	    //VDHR0 //Vertical Display Height Bit [7:0]
	    //Vertical pixels = VDHR + 1
		RA8875_WriteReg(0x19, 0xDF);

	    //VDHR1 //Vertical Display Height Bit [8]
	    //Vertical pixels = VDHR + 1
		RA8875_WriteReg(0x1A, 0x01);

	    //VNDR0 //Vertical Non-Display Period Bit [7:0]
	    //Vertical Non-Display area = (VNDR + 1)
		RA8875_WriteReg(0x1B, 0x0A);

	    //VNDR1 //Vertical Non-Display Period Bit [8]
	    //Vertical Non-Display area = (VNDR + 1)
		RA8875_WriteReg(0x1C, 0x00);

	    //VSTR0 //VSYNC Start Position[7:0]
	    //VSYNC Start Position(PCLK) = (VSTR + 1)
		RA8875_WriteReg(0x1D, 0x0E);

	    //VSTR1 //VSYNC Start Position[8]
	    //VSYNC Start Position(PCLK) = (VSTR + 1)
		RA8875_WriteReg(0x1E, 0x00);

	    //VPWR //VSYNC Polarity ,VSYNC Pulse Width[6:0]
	    //VSYNC Pulse Width(PCLK) = (VPWR + 1)
	    RA8875_WriteReg(0x1F, 0x01);
	#endif
	}

	/* 设置TFT面板的 DISP  引脚为高，使能面板. 安富莱TFT模块的DISP引脚连接到RA8875芯片的GP0X脚 */
	RA8875_WriteReg(0xC7, 0x01);	/* DISP = 1 */

	/* LCD 显示/关闭讯号 (LCD Display on) */
	RA8875_WriteReg(0x01, 0x80);

	/* 	REG[40h] Memory Write Control Register 0 (MWCR0)

		Bit 7	显示模式设定
			0 : 绘图模式。
			1 : 文字模式。

		Bit 6	文字写入光标/内存写入光标设定
			0 : 设定文字/内存写入光标为不显示。
			1 : 设定文字/内存写入光标为显示。

		Bit 5	文字写入光标/内存写入光标闪烁设定
			0 : 游标不闪烁。
			1 : 游标闪烁。

		Bit 4   NA

		Bit 3-2  绘图模式时的内存写入方向
			00b : 左 -> 右，然后上 -> 下。
			01b : 右 -> 左，然后上 -> 下。
			10b : 上 -> 下，然后左 -> 右。
			11b : 下 -> 上，然后左 -> 右。

		Bit 1 	内存写入光标自动增加功能设定
			0 : 当内存写入时光标位置自动加一。
			1 : 当内存写入时光标位置不会自动加一。

		Bit 0 内存读取光标自动增加功能设定
			0 : 当内存读取时光标位置自动加一。
			1 : 当内存读取时光标位置不会自动加一。
	*/
	//RA8875_WriteReg(0x40, 0x00);	/* 选择绘图模式 */	
	RA8875_SetDirection(0);

	/* 	REG[41h] Memory Write Control Register1 (MWCR1)
		写入目的位置，选择图层1
	*/
	RA8875_WriteReg(0x41, 0x00);	/* 选择绘图模式, 目的为CGRAM */

	RA8875_SetDispWin(0, 0, g_LcdHeight, g_LcdWidth);

	if ((g_RA8875_IF == RA_HARD_SPI) || (g_RA8875_IF == RA_SOFT_SPI))
	{
		RA8875_HighSpeedSPI();		/* 配置完毕后，切换SPI到高速模式 */
	}
	else
	{
		#ifdef WRTIE_SF_EN		/* 使能了字库图片芯片写功能，配置CPU的SPI接口和PWM-GPIO */
			bsp_InitRA8875Flash();		
		#endif	
	}
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_SetDirection
*	功能说明: 设置显示方向。
*	形    参:  _ucDir : 显示方向代码 0 横屏正常, 1=横屏180度翻转, 2=竖屏, 3=竖屏180度翻转
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_SetDirection(uint8_t _ucDir)
{
	uint8_t reg20H = 0;
	
	s_reg_40H = 0x01;
	
	if (_ucDir == 0)
	{
		s_reg_40H = 0x01;
		reg20H = (0 << 3) | (0 << 2);
	}
	else if (_ucDir == 1)
	{
		s_reg_40H = 0x01;	/* 2015-04-21 由05修改为 01. 解决emwin下的问题 */
		reg20H = (1 << 3) | (1 << 2);
	}
	else if (_ucDir == 2)
	{
		s_reg_40H = 0x09;
		reg20H = (0 << 3) | (1 << 2);
	}
	else if (_ucDir == 3)
	{
		s_reg_40H = 0x09;
		reg20H = (1 << 3) | (0 << 2);
	}
	RA8875_WriteReg(0x40, s_reg_40H);	

	/* 
		REG[20h] Display Configuration Register (DPCR) 
		Bit  说  明  初始值  Access
		Bit7 
			图层设定(Layer Control) 
			0 : 单图层。
			1 : 双图层。
		Bit3  HDIR 水平扫描方向设定(n = SEG number) 
			0 : 由SEG0 到SEG(n-1)。
			1 : 由SEG(n-1) 到SEG0。
		Bit2 VDIR 垂直扫描方向设定(n = COM number) 
			0 : 由COM0 到COM(n-1)。
			1 : 由COM(n-1) 到COM0。
	*/
	RA8875_WriteReg(0x20, reg20H);	

	if (_ucDir > 1)	/* 竖屏  */
	{
		uint16_t temp;
		
		if (g_LcdHeight < g_LcdWidth)
		{
			temp = g_LcdHeight;
			g_LcdHeight = g_LcdWidth;
			g_LcdWidth = temp;
		}
	}		
	else	/* 横屏 */
	{
		uint16_t temp;
		
		if (g_LcdHeight > g_LcdWidth)
		{
			temp = g_LcdHeight;
			g_LcdHeight = g_LcdWidth;
			g_LcdWidth = temp;
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: BTE_SetTarBlock
*	功能说明: 设置RA8875 BTE目标区块以及目标图层
*	形    参:
*			uint16_t _usX : 水平起点坐标
*			uint16_t _usY : 垂直起点坐标
*			uint16_t _usHeight : 区块高度
*			uint16_t _usWidth : 区块宽度
*			uint8_t _ucLayer : 0 图层1； 1 图层2
*	返 回 值: 无
*********************************************************************************************************
*/
static void BTE_SetTarBlock(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint8_t _ucLayer)
{
	/* 设置起点坐标 */
	RA8875_WriteReg(0x58, _usX);
	RA8875_WriteReg(0x59, _usX >> 8);

	RA8875_WriteReg(0x5A, _usY);
	if (_ucLayer == 0)	/* 图层2 */
	{
		RA8875_WriteReg(0x5B, _usY >> 8);
	}
	else
	{
		RA8875_WriteReg(0x5B, (1 << 7) | (_usY >> 8));	/* Bit7 表示图层， 0 图层1； 1 图层2*/
	}

	/* 设置区块宽度 */
	RA8875_WriteReg(0x5C, _usWidth);
	RA8875_WriteReg(0x5D, _usWidth >> 8);

	/* 设置区块高度 */
	RA8875_WriteReg(0x5E, _usHeight);
	RA8875_WriteReg(0x5F, _usHeight >> 8);
}

/*
*********************************************************************************************************
*	函 数 名: BTE_SetOperateCode
*	功能说明: 设定BTE 操作码和光栅运算码
*	形    参: _ucOperate : 操作码
*	返 回 值: 无
*********************************************************************************************************
*/
static void BTE_SetOperateCode(uint8_t _ucOperate)
{
	/*  设定BTE 操作码和光栅运算码  */
	RA8875_WriteReg(0x51, _ucOperate);
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_SetFrontColor
*	功能说明: 设定前景色
*	形    参: 颜色
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_SetFrontColor(uint16_t _usColor)
{
	RA8875_WriteReg(0x63, (_usColor & 0xF800) >> 11);	/* R5  */
	RA8875_WriteReg(0x64, (_usColor & 0x07E0) >> 5);	/* G6 */
	RA8875_WriteReg(0x65, (_usColor & 0x001F));			/* B5 */
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_SetBackColor
*	功能说明: 设定背景色
*	形    参: 颜色
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_SetBackColor(uint16_t _usColor)
{
	RA8875_WriteReg(0x60, (_usColor & 0xF800) >> 11);	/* R5  */
	RA8875_WriteReg(0x61, (_usColor & 0x07E0) >> 5);	/* G6 */
	RA8875_WriteReg(0x62, (_usColor & 0x001F));			/* B5 */
}


/*
*********************************************************************************************************
*	函 数 名: BTE_Start
*	功能说明: 启动BTE操作
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void BTE_Start(void)
{
	s_ucRA8875Busy = 1;
	/* RA8875_WriteReg(0x50, 0x80);  不能使用这个函数，因为内部已经操作了 s_ucRA8875Busy 标志 */
	RA8875_WriteCmd(0x50);	/* 设置寄存器地址 */
	RA8875_WriteData(0x80);	/* 写入寄存器值 */
}

/*
*********************************************************************************************************
*	函 数 名: BTE_Wait
*	功能说明: 等待BTE操作结束
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void BTE_Wait(void)
{
	uint32_t i;
	
	//while ((RA8875_ReadStatus() & 0x40) == 0x40);
	for (i = 0; i < 100000; i++)
	{
		if ((RA8875_ReadStatus() & 0x40) == 0)
		{
			break;
		}
	}
	s_ucRA8875Busy = 0;
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_IsBusy
*	功能说明: RA8875是否忙
*	形    参: 无
*	返 回 值: 1 表示忙； 0 表示空闲
*********************************************************************************************************
*/
uint8_t RA8875_IsBusy(void)
{
	if (s_ucRA8875Busy == 0)
	{
		return 0;
	}
	return 1;
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_WaitBusy
*	功能说明: 等待RA8875空闲
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_WaitBusy(void)
{
	while ((RA8875_ReadStatus() & 0x80) == 0x80);
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_Layer1Visable
*	功能说明: RA8875 图层1显示打开
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_Layer1Visable(void)
{
	/* 0x52 寄存器的 Bit2:0
		图层显示模式
		000b : 只有图层1 显示。
		001b : 只有图层2 显示。
		010b : 显示图层1 与图层2 的渐进/渐出模式。
		011b : 显示图层1 与图层2 的通透模式。
		100b : Boolean OR。
		101b : Boolean AND。
		110b : 浮动窗口模式 (Floating window mode)。
		111b :保留。
	*/
	RA8875_WriteReg(0x52, RA8875_ReadReg(0x52) & 0xF8);	/* 只有图层1 显示 */
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_Layer2Visable
*	功能说明: 只显示图层2
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_Layer2Visable(void)
{
	/* 0x52 寄存器的 Bit2:0
		图层显示模式
		000b : 只有图层1 显示。
		001b : 只有图层2 显示。
		010b : 显示图层1 与图层2 的渐进/渐出模式。
		011b : 显示图层1 与图层2 的通透模式。
		100b : Boolean OR。
		101b : Boolean AND。
		110b : 浮动窗口模式 (Floating window mode)。
		111b :保留。
	*/
	RA8875_WriteReg(0x52, (RA8875_ReadReg(0x52) & 0xF8) | 0x01);	/* 只有图层2 显示 */
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_TouchInit
*	功能说明: 初始化触摸板
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_TouchInit(void)
{
	RA8875_WriteReg(0xF0, (1 << 2));	/* 开启触控面板中断位 */

	/*
		REG[71h] Touch Panel Control Register 1 (TPCR1)

		Bit7 N/A
		Bit6 触控面板模式设定
			0 : 自动模式。
			1 : 手动模式。
		Bit5 触控面板控制器ADC 参考电压(Vref)来源设定
			0 : 内部产生参考电压。
			1 : 外部输入参考电压，ADC 参考电压准位= 1/2 VDD。
		Bit4-3 N/A
		Bit2 触控中断讯号的消除弹跳电路选择
			0: 关闭消除弹跳电路。
			1: 开启消除弹跳电路。
		Bit1-0 触控面板手动模式之选择位
			00b : 闲置模式。触控控制单元进入闲置模式。
			01b : 侦测触摸事件发生。在此模式控制器会侦测触摸事件的发
				生，事件发生可以引发中断或是由缓存器得知(REG[F1h]
				Bit2)。
			10b : X 轴数据撷取模式。在此模式触摸位置的X 轴数据会被储
				存至 REG[72h] 和REG[74h]。
			11b : Y 轴数据撷取模式。在此模式触摸位置的Y 轴数据会被储
				存至REG[73h] and REG[74h]。
	*/
	RA8875_WriteReg(0x71, (0 << 6) | (0 << 5) | (1 << 2));	/* 选择自动模式 */

	/*
		REG[70h] Touch Panel Control Register 0 (TPCR0)

		Bit7 触控面板功能设定
			0 : 关闭。
			1 : 开启。
		Bit6-4 触控面板控制器取样时间设定
			000b : ADC 取样时间为512 个系统频率周期。
			001b : ADC 取样时间为 1024 个系统频率周期。
			010b : ADC 取样时间为 2048 个系统频率周期。
			011b : ADC 取样时间为 4096 个系统频率周期。
			100b : ADC 取样时间为 8192 个系统频率周期。
			101b : ADC 取样时间为 16384 个系统频率周期。
			110b : ADC 取样时间为 32768 个系统频率周期。
			111b : ADC 取样时间为65536 个系统频率周期。
		Bit3 触控面板唤醒模式
			0 : 关闭触控事件唤醒模式。
			1 : 触控事件可唤醒睡眠模式。
		Bit2-0 触控面板控制器ADC 频率设定
			000b : 系统频率。
			001b : 系统频率 / 2。
			010b : 系统频率 / 4。
			011b : 系统频率 / 8。
			100b : 系统频率 / 16。
			101b : 系统频率 / 32。
			110b : 系统频率 / 64。
			111b : 系统频率 / 128。

		注 : ADC 的输入频率设定不能超过10MHz。
	*/
	RA8875_WriteReg(0x70, (1 << 7) | (3 << 4) | (0 << 3) | (2 << 0));	/* 开启触摸功能； */
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_TouchReadX
*	功能说明: 读取X位置ADC   （这个函数被定时中断服务程序调用，需要避免寄存器访问冲突 )
*	形    参: 无
*	返 回 值: ADC值，10Bit
*********************************************************************************************************
*/
uint16_t RA8875_TouchReadX(void)
{
	uint16_t usAdc;
	uint8_t ucRegValue;
	uint8_t ucReg74;

	/* 软件读取中断事件标志 */
	ucRegValue = RA8875_ReadReg_Int(0xF1);
	if (ucRegValue & (1 << 2))
	{
		ucReg74 = RA8875_ReadReg_Int(0x74);
		usAdc = RA8875_ReadReg_Int(0x72);	/* Bit9-2 */
		usAdc <<= 2;
		usAdc += (ucReg74 & 0x03);

		s_usTouchX = usAdc;

		usAdc = RA8875_ReadReg_Int(0x73);	/* Bit9-2 */
		usAdc <<= 2;
		usAdc += ((ucReg74 & 0x0C) >> 2);

		s_usTouchY = usAdc;
	}
	else
	{
		s_usTouchX = 0;
		s_usTouchY = 0;
	}

	/*
		bit2 写入功能?? 触控面板中断清除位
		0 : 未操作。
		1 : 清除触控面板中断。
		读取功能?? 触控面板中断状态
		0 : 未发生触控面板中断。
		1 : 发生触控面板中断。
	*/

	/*
		不要调用这个函数写寄存器，因为该函数改写了busy标志
		RA8875_WriteReg(0xF1, (1 << 2));	 必须清除， 才会下次采样.
	*/
	RA8875_WriteCmd(0xF1);
	RA8875_WriteData(1 << 2);

	return s_usTouchX;
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_TouchReadY
*	功能说明: 读取Y位置ADC ； 必须先调用 RA8875_TouchReadX()
*	形    参: 无
*	返 回 值: ADC值，10Bit
*********************************************************************************************************
*/
uint16_t RA8875_TouchReadY(void)
{
	return s_usTouchY;
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_DispOn
*	功能说明: 打开显示
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_DispOn(void)
{
	RA8875_WriteReg(0x01, 0x80);
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_DispOff
*	功能说明: 关闭显示
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_DispOff(void)
{
	RA8875_WriteReg(0x01, 0x00);
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_Sleep
*	功能说明: RA8875进入睡眠模式（同时关闭显示器）
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_Sleep(void)
{
	RA8875_SetBackLight(0);		/* 关闭背光 */

	/* 设置TFT面板的 DISP  引脚为高，使能面板. 安富莱TFT模块的DISP引脚连接到RA8875芯片的GP0X脚 */
	RA8875_WriteReg(0xC7, 0x00);	/* DISP = 0  */

	RA8875_WriteReg(0x01, 0x01);
	RA8875_WriteReg(0x01, 0x00);

	RA8875_WriteReg(0x01, 0x02);	/* RA8875 Sleep */
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_PutPixel
*	功能说明: 画1个像素
*	形    参:
*			_usX,_usY : 像素坐标
*			_usColor  :像素颜色
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_PutPixel(uint16_t _usX, uint16_t _usY, uint16_t _usColor)
{	
	if (g_RA8875_IF == RA_HARD_8080_16)	/* 8080硬件总线16bit */
	{
		// 优化 RA8875_SetCursor(_usX, _usY);
		s_ucRA8875Busy = 1;
		{	
			if (g_LcdDirection > 1)	/* 竖屏  */
			{
				RA8875_REG = 0x46; RA8875_RAM = _usY;
				RA8875_REG = 0x47; RA8875_RAM = _usY >> 8;
				RA8875_REG = 0x48; RA8875_RAM = _usX;
				RA8875_REG = 0x49; RA8875_RAM = _usX >> 8;
			}
			else	/* 横屏 */
			{
				RA8875_REG = 0x46; RA8875_RAM = _usX;
				RA8875_REG = 0x47; RA8875_RAM = _usX >> 8;
				RA8875_REG = 0x48; RA8875_RAM = _usY;
				RA8875_REG = 0x49; RA8875_RAM = _usY >> 8;	
			}
		}
		
		// 优化 RA8875_WriteCmd(0x02); 		/* 用于设定RA8875 进入内存(DDRAM或CGRAM)读取/写入模式 */
		// 优化 RA8875_WriteData16(_usColor);
		
		RA8875_REG = 0x02;
		RA8875_RAM = _usColor;

		s_ucRA8875Busy = 0;
	}
	else	/* SPI接口分支 */
	{
		RA8875_SetCursor(_usX, _usY);
		s_ucRA8875Busy = 1;

		RA8875_WriteCmd(0x02); 		/* 用于设定RA8875 进入内存(DDRAM或CGRAM)读取/写入模式 */
		RA8875_WriteData16(_usColor);

		s_ucRA8875Busy = 0;
	}
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_GetPixel
*	功能说明: 读取1个像素
*	形    参:
*			_usX,_usY : 像素坐标
*			_usColor  :像素颜色
*	返 回 值: RGB颜色值
*********************************************************************************************************
*/
uint16_t RA8875_GetPixel(uint16_t _usX, uint16_t _usY)
{
	uint16_t usRGB;

//	RA8875_WriteReg(0x40, (1 << 0));	/* 设置为绘图模式，读取光标不自动加1 */

	RA8875_SetReadCursor(_usX, _usY);	/* 设置读取光标位置 */

	s_ucRA8875Busy = 1;

	RA8875_WriteCmd(0x02);
	usRGB = RA8875_ReadData16();	/* 第1次读取数据丢弃 */
	usRGB = RA8875_ReadData16();
	usRGB = RA8875_ReadData16();

	s_ucRA8875Busy = 0;

	return usRGB;
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_ClrScr
*	功能说明: 根据输入的颜色值清屏.RA8875支持硬件单色填充。该函数仅对当前激活的显示窗口进行清屏. 显示
*			 窗口的位置和大小由 RA8875_SetDispWin() 函数进行设置
*	形    参:  _usColor : 背景色
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_ClrScr(uint16_t _usColor)
{
	/* 也可以通过0x30-0x37寄存器获得获得当前激活的显示窗口 */

	/* 单色填满功能, 中文pdf 第162页
	此功能使用于将选定特定区域画面清除或是塡入给定某种前景色，R8875 填入的单色设定为BTE 前景色。

	操作步骤:
		1. 设定目的图层和位置 REG[58h], [59h], [5Ah], [5Bh]
		2. 设定BTE 宽度和高度 REG[5Ch], [5Dh], [5Eh], [5Fh]
		3. 设定BTE 操作码和光栅运算码  REG[51h] Bit[3:0] = 0Ch
		4. 设定前景色  REG[63h], [64h], [65h]
		5. 开启BTE 功能  REG[50h] Bit7 = 1
		6. 检查状态缓存器 STSR Bit6，确认BTE 是否完成
	*/
	BTE_SetTarBlock(s_WinX, s_WinY, s_WinHeight, s_WinWidth, 0);	/* 设置BTE位置和宽度高度以及目标图层（0或1） */
	BTE_SetOperateCode(0x0C);		/* 设定BTE 操作码和光栅运算码  REG[51h] Bit[3:0] = 0Ch */
	RA8875_SetFrontColor(_usColor);	/* 设置BTE前景色 */
	BTE_Start();					/* 开启BTE 功能 */
	BTE_Wait();						/* 等待操作结束 */
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_DispBmpInFlash
*	功能说明: 显示RA8875外挂串行Flash中的BMP位图.
*			    位图格式:从左到右 从上到下扫描, 每个像素2字节, RGB = 565 格式, 高位在前.
*	形    参:
*			_usX, _usY : 图片的坐标
*			_usHeight  :图片高度
*			_usWidth   :图片宽度
*			_uiFlashAddr       :串行Flash地址
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_DispBmpInFlash(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth,
	uint32_t _uiFlashAddr)
{
	/* pdf 179页 7-10-1 连续内存直接存取模式
		1. 设定工作窗口范围 (REG[30h] ~REG[37h])和内存写入位置 (REG[46h] ~REG[49h])
		2. 设定Serial Flash/ROM 组态 (REG[05h])
		3. 设定 内存直接存取数据来源起始位置 (REG[B0h] ~REG[B2h])
		4. 设定 内存直接存取区块宽度 (REG[B4h] 和REG[B5h])
		5. 设定 内存直接存取区块高度 (REG[B6h] 和 REG[B7h])
		6. 设定内存直接存取来源图片宽度 (REG[B8h] 和 REG[B9h])
		7. 开启内存直接存取为区块搬移模式 (REG[BFh] bit 1)
		8. 开启内存直接存取起始讯号且检查内存直接存取忙碌讯号 (REG[BFh] bit 0)
	*/
	RA8875_SetDispWin(_usX, _usY, _usHeight, _usWidth);	/* REG[30h] ~REG[37h] 和 REG[46h] ~REG[49h] */

	/* 05H  REG[05h] Serial Flash/ROM Configuration Register (SROC)
		7	Serial Flash/ROM I/F # 选择
				0:选择Serial Flash/ROM 0 接口。[安富莱RA8875驱动板图库芯片接在 1 接口]
				1:选择Serial Flash/ROM 1 接口。
		6	Serial Flash/ROM 寻址模式
				0: 24 位寻址模式。
				此位必须设为0。
		5	Serial Flash/ROM 波形模式
				0: 波形模式 0。
				1: 波形模式 3。
		4-3	Serial Flash /ROM 读取周期 (Read Cycle)
			00b: 4 bus ?? 无空周期 (No Dummy Cycle)。
			01b: 5 bus ??1 byte 空周期。
			1Xb: 6 bus ??2 byte 空周期。
		2	Serial Flash /ROM 存取模式 (Access Mode)
			0: 字型模式 。
			1: DMA 模式。
		1-0	Serial Flash /ROM I/F Data Latch 选择模式
			0Xb: 单一模式。
			10b: 双倍模式0。
			11b: 双倍模式1。
	*/
	
	/* 对于铁框屏，一片 W25Q128，前面2MB是字库。后面14MB是图片 */
	if (g_tW25.ChipID == W25Q128)
	{
		_uiFlashAddr += PIC_OFFSET;	/* 前面2MB分配给字库 */	
		#ifdef FLASH_DUAL_SPEED_EN			/* 双速模式， MOSI和MISO同时读取数据 */
			RA8875_WriteReg(0x05, (0 << 7) | (0 << 6) | (1 << 5) | (0 << 3) | (1 << 2) | (2 << 0));
		#else				/* 普通模式，仅 MISO 读取数据 */
			RA8875_WriteReg(0x05, (0 << 7) | (0 << 6) | (1 << 5) | (0 << 3) | (1 << 2) | (0 << 0));
		#endif		
	}	
	else	/* 增强型屏，1片W25Q64(8MB)做字库，1片W25Q64(8MB)做图库 */
	{
		#ifdef FLASH_DUAL_SPEED_EN			/* 双速模式， MOSI和MISO同时读取数据 */
			RA8875_WriteReg(0x05, (1 << 7) | (0 << 6) | (1 << 5) | (0 << 3) | (1 << 2) | (2 << 0));
		#else				/* 普通模式，仅 MISO 读取数据 */
			RA8875_WriteReg(0x05, (1 << 7) | (0 << 6) | (1 << 5) | (0 << 3) | (1 << 2) | (0 << 0));
		#endif	
	}

	/*
		Serial Flash/ROM 频率频率设定
			0xb: SFCL 频率 = 系统频率频率(当DMA 为使能状态，并且色彩深度为256 色，则SFCL 频率
				固定为=系统频率频率/ 2)
			10b: SFCL 频率 =系统频率频率/ 2
			11b: SFCL 频率 =系统频率频率/ 4

		安富莱TFT驱动板。4.3寸系统频率为 68MHz； 7寸系统频率为 81.25MHz。

		8M串行Flash的访问速度:SPI 时钟频率:80MHz(max.)
		因此可以设置为 1 分频
	*/
	RA8875_WriteReg(0x06, (0 << 0));	/* 设置SCL时钟频率 */

	/* 设定源地址
		B0H   DMA 来源开始位置[7:0]
		B1H   DMA 来源开始位置[15:8]
		B2H   DMA 来源开始位置[23:16]
	*/
	RA8875_WriteReg(0xB0, (uint8_t)_uiFlashAddr);
	RA8875_WriteReg(0xB1, (uint8_t)(_uiFlashAddr >> 8));
	RA8875_WriteReg(0xB2, (uint8_t)(_uiFlashAddr >> 16));

	/*
		设置区块大小
		B4   DMA 区块宽度[7:0]
		B5   DMA 区块宽度[9:8]
		B6   DMA 区块高度[7:0]
	    B7   DMA 区块高度[9:8]
	    B8   DMA 来源图片宽度[7:0]
	    B9   DMA 来源图片宽度[9:8]
	*/
	RA8875_WriteReg(0xB4, _usWidth);		/* DMA 区块宽度 */
	RA8875_WriteReg(0xB5, _usWidth >> 8);

	RA8875_WriteReg(0xB6, _usHeight);		/* DMA 区块高度 */
	RA8875_WriteReg(0xB7, _usHeight >> 8);

	RA8875_WriteReg(0xB8, _usWidth);		/* DMA 来源图片宽度 */
	RA8875_WriteReg(0xB9, _usWidth >> 8);

	RA8875_WriteReg(0xBF, 1 << 1);			/* 选择区块模式 */
	RA8875_WriteReg(0xBF, 1 << 1);			/* 选择区块模式 */

	s_ucRA8875Busy = 1;
	RA8875_WriteCmd(0xBF);
	RA8875_WriteData((1 << 1) | (1 << 0));
	while (RA8875_ReadReg_Int(0xBF) & (1 << 0));	/* 等待结束 */
	s_ucRA8875Busy = 0;

	RA8875_QuitWinMode();					/* 退出小窗口绘图模式 */
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_DrawBMP
*	功能说明: 在LCD上显示一个BMP位图，位图点阵扫描次序:从左到右，从上到下
*	形    参:
*			_usX, _usY : 图片的坐标
*			_usHeight  :图片高度
*			_usWidth   :图片宽度
*			_ptr       :图片点阵指针
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_DrawBMP(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t *_ptr)
{
	uint32_t index = 0;
	const uint16_t *p;

	/* 设置图片的位置和大小， 即设置显示窗口 */
	RA8875_SetDispWin(_usX, _usY, _usHeight, _usWidth);

	s_ucRA8875Busy = 1;

	RA8875_WriteCmd(0x02); 		/* 准备读写显存 */

	p = _ptr;
	for (index = 0; index < _usHeight * _usWidth; index++)
	{
		/*
			armfly : 进行优化, 函数就地展开
			RA8875_WriteRAM(_ptr[index]);

			此处可考虑用DMA操作
		*/
		RA8875_WriteData16(*p++);
	}
	s_ucRA8875Busy = 0;

	/* 退出窗口绘图模式 */
	RA8875_QuitWinMode();
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_StartDirectDraw
*	功能说明: 开始直接绘图模式.  不支持SPI模式，仅支持8080模式
*	形    参:
*			_usX, _usY : 显示窗口的坐标
*			_usHeight  : 显示窗口的高度
*			_usWidth   : 显示窗口的宽度
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_StartDirectDraw(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth)
{
	/* 设置图片的位置和大小， 即设置显示窗口 */
	RA8875_SetDispWin(_usX, _usY, _usHeight, _usWidth);

	s_ucRA8875Busy = 1;

	RA8875_WriteCmd(0x02); 		/* 准备读写显存 */

	/* 之后，应用程序可以直接绘图 */
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_QuitDirectDraw
*	功能说明: 退出绘图模式.  不支持SPI模式，仅支持8080模式
*	形    参:
*			_usX, _usY : 显示窗口的坐标
*			_usHeight  : 显示窗口的高度
*			_usWidth   : 显示窗口的宽度
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_QuitDirectDraw(void)
{
	s_ucRA8875Busy = 0;

	/* 退出窗口绘图模式 */
	RA8875_QuitWinMode();
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_DrawLine
*	功能说明: 采用RA8875的硬件功能，在2点间画一条直线。
*	形    参:
*			_usX1, _usY1 :起始点坐标
*			_usX2, _usY2 :终止点Y坐标
*			_usColor     :颜色
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_DrawLine(uint16_t _usX1 , uint16_t _usY1 , uint16_t _usX2 , uint16_t _usY2 , uint16_t _usColor)
{
	/* pdf 第131页
		RA8875 支持直线绘图功能，让使用者以简易或低速的MCU 就可以在TFT 模块上画直线。先设
		定直线的起始点REG[91h~94h] 与结束点REG[95h~98h]，直线的颜色REG[63h~65h]，然后启
		动绘图设定REG[90h] Bit4 = 0, Bit0=0 且REG[90h] Bit7 = 1，RA8875 就会将直线的图形写入
		DDRAM，相对的在TFT 模块上就可以显示所画的直线。
	*/

	if ((_usX1 == _usX2) && (_usY1 == _usY2))
	{
		RA8875_PutPixel(_usX1, _usY1, _usColor);
		return;
	}

	if (g_LcdDirection > 1)	/* 竖屏  */
	{
		uint16_t temp;
		
		temp = _usX1;
		_usX1 = _usY1;
		_usY1 = temp;

		temp = _usX2;
		_usX2 = _usY2;
		_usY2 = temp;		
	}
	
	/* 设置起点坐标 */
	RA8875_WriteReg(0x91, _usX1);
	RA8875_WriteReg(0x92, _usX1 >> 8);
	RA8875_WriteReg(0x93, _usY1);
	RA8875_WriteReg(0x94, _usY1 >> 8);

	/* 设置终点坐标 */
	RA8875_WriteReg(0x95, _usX2);
	RA8875_WriteReg(0x96, _usX2 >> 8);
	RA8875_WriteReg(0x97, _usY2);
	RA8875_WriteReg(0x98, _usY2 >> 8);

	RA8875_SetFrontColor(_usColor);	/* 设置直线的颜色 */

	s_ucRA8875Busy = 1;
//	RA8875_WriteReg(0x90, (1 << 7) | (0 << 4) | (0 << 0));/* 开始画直线 */
	RA8875_WriteCmd(0x90);
	RA8875_WriteData((1 << 7) | (0 << 4) | (0 << 0));
//	while (RA8875_ReadReg(0x90) & (1 << 7));				/* 等待结束 */
	RA8875_WaitBusy();
	s_ucRA8875Busy = 0;
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_DrawRect
*	功能说明: 采用RA8875硬件功能绘制矩形
*	形    参:
*			_usX,_usY:矩形左上角的坐标
*			_usHeight :矩形的高度
*			_usWidth  :矩形的宽度
*			_usColor  :颜色
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_DrawRect(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t _usColor)
{
	/*
		RA8875 支持方形绘图功能，让使用者以简易或低速的MCU 就可以在TFT 模块上画方形。先设
	定方形的起始点REG[91h~94h]与结束点REG[95h~98h]，方形的颜色REG[63h~65h]，然后启
	动绘图设定REG[90h] Bit4=1, Bit0=0 且REG[90h] Bit7 = 1，RA8875 就会将方形的图形写入
	DDRAM，相对的在TFT 模块上就可以显示所画的方形。若设定REG[90h] Bit5 = 1，则可画出一
	实心方形 (Fill)

	 ---------------->---
	|(_usX，_usY)        |
	V                    V  _usHeight
	|                    |
	 ---------------->---
		  _usWidth
	*/

	if (g_LcdDirection > 1)	/* 竖屏  */
	{
		uint16_t temp;
		
		temp = _usX;
		_usX = _usY;
		_usY = temp;

		temp = _usHeight;
		_usHeight = _usWidth;
		_usWidth = temp;		
	}

	/* 设置起点坐标 */
	RA8875_WriteReg(0x91, _usX);
	RA8875_WriteReg(0x92, _usX >> 8);
	RA8875_WriteReg(0x93, _usY);
	RA8875_WriteReg(0x94, _usY >> 8);

	/* 设置终点坐标 */
	RA8875_WriteReg(0x95, _usX + _usWidth - 1);
	RA8875_WriteReg(0x96, (_usX + _usWidth - 1) >> 8);
	RA8875_WriteReg(0x97, _usY + _usHeight - 1);
	RA8875_WriteReg(0x98, (_usY + _usHeight - 1) >> 8);

	RA8875_SetFrontColor(_usColor);	/* 设置颜色 */

	s_ucRA8875Busy = 1;
//	RA8875_WriteReg(0x90, (1 << 7) | (0 << 5) | (1 << 4) | (0 << 0));	/* 开始画矩形  */
	RA8875_WriteCmd(0x90);
	RA8875_WriteData((1 << 7) | (0 << 5) | (1 << 4) | (0 << 0));
//	while (RA8875_ReadReg(0x90) & (1 << 7));							/* 等待结束 */
	RA8875_WaitBusy();
	s_ucRA8875Busy = 0;
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_FillRect
*	功能说明: 采用RA8875硬件功能填充一个矩形为单色
*	形    参:
*			_usX,_usY:矩形左上角的坐标
*			_usHeight :矩形的高度
*			_usWidth  :矩形的宽度
*			_usColor  :颜色
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_FillRect(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth, uint16_t _usColor)
{
	/*
		RA8875 支持方形绘图功能，让使用者以简易或低速的MCU 就可以在TFT 模块上画方形。先设
	定方形的起始点REG[91h~94h]与结束点REG[95h~98h]，方形的颜色REG[63h~65h]，然后启
	动绘图设定REG[90h] Bit4=1, Bit0=0 且REG[90h] Bit7 = 1，RA8875 就会将方形的图形写入
	DDRAM，相对的在TFT 模块上就可以显示所画的方形。若设定REG[90h] Bit5 = 1，则可画出一
	实心方形 (Fill)

	 ---------------->---
	|(_usX，_usY)        |
	V                    V  _usHeight
	|                    |
	 ---------------->---
		  _usWidth
	*/

	if (_usWidth == 0 || _usHeight == 0)
	{
		return;
	}

	if (g_LcdDirection > 1)	/* 竖屏  */
	{
		uint16_t temp;
		
		temp = _usX;
		_usX = _usY;
		_usY = temp;
		
		temp = _usHeight;
		_usHeight = _usWidth;
		_usWidth = temp;
	}
	
	/* 设置起点坐标 */
	RA8875_WriteReg(0x91, _usX);
	RA8875_WriteReg(0x92, _usX >> 8);
	RA8875_WriteReg(0x93, _usY);
	RA8875_WriteReg(0x94, _usY >> 8);

	/* 设置终点坐标 */
	RA8875_WriteReg(0x95, _usX + _usWidth - 1);
	RA8875_WriteReg(0x96, (_usX + _usWidth - 1) >> 8);
	RA8875_WriteReg(0x97, _usY + _usHeight - 1);
	RA8875_WriteReg(0x98, (_usY + _usHeight - 1) >> 8);

	RA8875_SetFrontColor(_usColor);	/* 设置颜色 */

	s_ucRA8875Busy = 1;
//	RA8875_WriteReg(0x90, (1 << 7) | (1 << 5) | (1 << 4) | (0 << 0));	/* 开始填充矩形  */
	RA8875_WriteCmd(0x90);
	RA8875_WriteData((1 << 7) | (1 << 5) | (1 << 4) | (0 << 0));
//	while (RA8875_ReadReg(0x90) & (1 << 7));							/* 等待结束 */
	RA8875_WaitBusy();
	s_ucRA8875Busy = 0;
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_DrawCircle
*	功能说明: 绘制一个圆，笔宽为1个像素
*	形    参:
*			_usX,_usY  :圆心的坐标
*			_usRadius  :圆的半径
*			_usColor  :颜色
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_DrawCircle(uint16_t _usX, uint16_t _usY, uint16_t _usRadius, uint16_t _usColor)
{
	/*
		RA8875 支持圆形绘图功能，让使用者以简易或低速的MCU 就可以在TFT 模块上画圆。先设定
		圆的中心点REG[99h~9Ch]，圆的半径REG[9Dh]，圆的颜色REG[63h~65h]，然后启动绘图
		REG[90h] Bit6 = 1，RA8875 就会将圆的图形写入DDRAM，相对的在TFT 模块上就可以显示所
		画的圆。若设定REG[90h] Bit5 = 1，则可画出一实心圆 (Fill)；若设定REG[90h] Bit5 = 0，则可
		画出空心圆 (Not Fill
	*/
	/* 设置圆的半径 */
	if (_usRadius > 255)
	{
		return;
	}

	if (g_LcdDirection > 1)	/* 竖屏  */
	{
		uint16_t temp;
		
		temp = _usX;
		_usX = _usY;
		_usY = temp;
	}
	
	/* 设置圆心坐标 */
	RA8875_WriteReg(0x99, _usX);
	RA8875_WriteReg(0x9A, _usX >> 8);
	RA8875_WriteReg(0x9B, _usY);
	RA8875_WriteReg(0x9C, _usY >> 8);

	RA8875_WriteReg(0x9D, _usRadius);	/* 设置圆的半径 */

	RA8875_SetFrontColor(_usColor);	/* 设置颜色 */

	s_ucRA8875Busy = 1;
//	RA8875_WriteReg(0x90, (1 << 6) | (0 << 5));				/* 开始画圆, 不填充  */
	RA8875_WriteCmd(0x90);
	RA8875_WriteData( (1 << 6) | (0 << 5));
//	while (RA8875_ReadReg(0x90) & (1 << 6));				/* 等待结束 */
	RA8875_WaitBusy();
	s_ucRA8875Busy = 0;
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_FillCircle
*	功能说明: 填充一个圆
*	形    参:
*			_usX,_usY  :圆心的坐标
*			_usRadius  :圆的半径
*			_usColor   :颜色
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_FillCircle(uint16_t _usX, uint16_t _usY, uint16_t _usRadius, uint16_t _usColor)
{
	/*
		RA8875 支持圆形绘图功能，让使用者以简易或低速的MCU 就可以在TFT 模块上画圆。先设定
		圆的中心点REG[99h~9Ch]，圆的半径REG[9Dh]，圆的颜色REG[63h~65h]，然后启动绘图
		REG[90h] Bit6 = 1，RA8875 就会将圆的图形写入DDRAM，相对的在TFT 模块上就可以显示所
		画的圆。若设定REG[90h] Bit5 = 1，则可画出一实心圆 (Fill)；若设定REG[90h] Bit5 = 0，则可
		画出空心圆 (Not Fill
	*/
	/* 设置圆的半径 */
	if (_usRadius > 255)
	{
		return;
	}

	if (g_LcdDirection > 1)	/* 竖屏  */
	{
		uint16_t temp;
		
		temp = _usX;
		_usX = _usY;
		_usY = temp;
	}
	
	/* 设置圆心坐标 */
	RA8875_WriteReg(0x99, _usX);
	RA8875_WriteReg(0x9A, _usX >> 8);
	RA8875_WriteReg(0x9B, _usY);
	RA8875_WriteReg(0x9C, _usY >> 8);

	RA8875_WriteReg(0x9D, _usRadius);	/* 设置圆的半径 */

	RA8875_SetFrontColor(_usColor);	/* 设置颜色 */

	s_ucRA8875Busy = 1;
//	RA8875_WriteReg(0x90, (1 << 6) | (1 << 5));				/* 开始画圆, 填充  */
	RA8875_WriteCmd(0x90);
	RA8875_WriteData((1 << 6) | (1 << 5));
//	while (RA8875_ReadReg(0x90) & (1 << 6));				/* 等待结束 */
	RA8875_WaitBusy();
	s_ucRA8875Busy = 0;
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_SetFont
*	功能说明: 文本模式，设置文字字体、行距和字距
*	形    参:
*			_ucFontType : 字体类型: RA_FONT_16, RA_FONT_24, RA_FONT_32
*			_ucLineSpace : 行距，像素单位 (0-31)
*			_ucCharSpace : 字距，像素单位 (0-63)
*
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_SetFont(uint8_t _ucFontType, uint8_t _ucLineSpace, uint8_t _ucCharSpace)
{
	/*
		[29H]在文字模式下，用来设定文字间的行距 (单位: 像素) 。
		只有低5个bit有效，0-31
	*/
	if (_ucLineSpace >31)
	{
		_ucLineSpace = 31;
	}
	RA8875_WriteReg(0x29, _ucLineSpace);

	/*
		[2EH] 设置字符间距（像素单位，0-63），和字体（16*16，24*24，32*32）
	*/
	if (_ucCharSpace > 63)
	{
		_ucCharSpace = 63;
	}
	if (_ucFontType > RA_FONT_32)
	{
		_ucFontType = RA_FONT_16;
	}
	RA8875_WriteReg(0x2E, (_ucCharSpace & 0x3F) | (_ucFontType << 6));
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_SetFontZoom
*	功能说明: 文本模式，设置文字的放大模式，1X,2X,3X, 4X
*	形    参:
*			_ucHSize : 文字水平放大倍数，0-3 分别对应 X1、X2、X3、X4
*			_ucVSize : 文字处置放大倍数，0-3 分别对应 X1、X2、X3、X4
*		建议输入参数用枚举:RA_SIZE_X1、RA_SIZE_X2、RA_SIZE_X3、RA_SIZE_X4
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_SetTextZoom(uint8_t _ucHSize, uint8_t _ucVSize)
{
	/*
		pdf 第22页		[22H]
		bit7 用于对齐，一般不用，缺省设0
		bit6 用于通透，一般不用，缺省设0
		bit4 用于旋转90读，一般不用，缺省设0
		bit3-2 水平放大倍数
		bit1-0 垂直放大倍数
	*/
	if (g_LcdDirection > 1)	/* 竖屏  */
	{
		RA8875_WriteReg(0x22, (1 << 4) | ((_ucHSize & 0x03) << 2) | ( _ucVSize & 0x03));
	}
	else	/* 横屏 */
	{
		RA8875_WriteReg(0x22, (0 << 4) | ((_ucHSize & 0x03) << 2) | ( _ucVSize & 0x03));
	}
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_DispAscii
*	功能说明: 显示ASCII字符串，字符点阵来自于RA8875内部ROM中的ASCII字符点阵（8*16），不支持汉字.
*			文字颜色、背景颜色、是否通透由其他的函数进行设置
*			void RA8875_SetFrontColor(uint16_t _usColor);
*			void RA8875_SetBackColor(uint16_t _usColor);
*	形    参:  _usX, _usY : 显示位置（坐标）
*			 _ptr : AascII字符串，已0结束
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_DispAscii(uint16_t _usX, uint16_t _usY, char *_ptr)
{
	/*
		RA8875 内建8x16 点的ASCII 字型ROM，提供使用者更方便的方式用特定编码 (Code) 输入文
		字。内建的字集支持ISO/IEC 8859-1~4 编码标准，此外，使用者可以透过REG[60h~62h] 选择
		文字前景颜色，以及透过REG[63h~65h] 选择背景颜色.

		ISO/IEC 8859-1，又称Latin-1或“西欧语言”，是国际标准化组织内ISO/IEC 8859的第一个8位字符集。
		它以ASCII为基础，在空置的0xA0-0xFF的范围内，加入192个字母及符号，藉以供使用变音符号的拉丁字母语言使用。

		ISO/IEC 8859-2 Latin-2或“中欧语言”，是国际标准化组织内ISO/IEC 8859的其中一个8位字符集 .
		ISO/IEC 8859-3 南欧语言字符集
		ISO/IEC 8859-4 北欧语言字符集
	*/

	/*
	(1) Text mode  REG[40h] bit7=1
	(2) Internal Font ROM Select   REG[21h] bit7=0, bit5=0
	(3) Font foreground and background color Select  REG[63h~65h], REG[60h~62h]
	(4) Write the font Code  CMD_WR[02h]    DATA_WR[font_code]
	*/
	
	RA8875_SetTextCursor(_usX, _usY);

	s_reg_40H |= (1 << 7);
	RA8875_WriteReg(0x40, s_reg_40H);	/* 设置为文本模式 */

	/* 选择CGROM font; 选择内部CGROM; 内部CGROM 编码选择ISO/IEC 8859-1. */
	RA8875_WriteReg(0x2F, 0x00);
	RA8875_WriteReg(0x21, (0 << 7) | (0 << 5) | (0 << 1) | (0 << 0));

	s_ucRA8875Busy = 1;

	RA8875_WriteCmd(0x02); 			/* 用于设定RA8875 进入内存(DDRAM或CGRAM)读取/写入模式 */

	/* 开始循环处理字符 */
	while (*_ptr != 0)
	{
		RA8875_WriteData(*_ptr);
		while ((RA8875_ReadStatus() & 0x80) == 0x80);	/* 必须等待内部写屏操作完成 */
		_ptr++;
	}
	s_ucRA8875Busy = 0;

	s_reg_40H &= 0x7F;
	RA8875_WriteReg(0x40, s_reg_40H);	/* 还原为图形模式 */
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_DispStr
*	功能说明: 显示字符串，字符点阵来自于RA8875外接的字库芯片，支持汉字.
*			文字颜色、背景颜色、是否通透由其他的函数进行设置
*			void RA8875_SetFrontColor(uint16_t _usColor);
*			void RA8875_SetBackColor(uint16_t _usColor);
*	形    参:  _usX, _usY : 显示位置（坐标）
*			 _ptr : AascII字符串，已0结束
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_DispStr(uint16_t _usX, uint16_t _usY, char *_ptr)
{
	/*
		RA8875 透过使用 — 上海集通公司 (Genitop Inc) 外部串行式字体内存 (Font ROM)，可支持各样
		的文字写入到DDRAM 中。RA8875 与上海集通公司 (Genitop Inc) 兼容的产品包含 :
		GT21L16TW/GT21H16T1W 、GT23L16U2W 、GT23L24T3Y/GT23H24T3Y 、GT23L24M1Z 、
		及 GT23L32S4W/GT23H32S4W。这些字体包含16x16, 24x24, 32x32 点 (Dot) 与不同的字

		安富莱RA8875驱动板集成的字库芯片为 集通字库芯片_GT23l32S4W

		GT23L32S4W是一款内含11X12点阵、15X16点、24X24点阵、32X32点阵的汉字库芯片，支持GB2312
		国标汉字（含有国家信标委合法授权）及SCII字符。排列格式为横置横排。用户通过字符内码，利用本手
		册提供的方法计算出该字符点阵在芯片中的地址，可从该地址连续读出字符点阵信息。
	*/

	/* 设置文本显示位置，注意文本模式的写入光标和图形模式的写入光标是不同的寄存器 */
	RA8875_SetTextCursor(_usX, _usY);

	//RA8875_SetTextZoom(0, 0);	/* for test */
	
	s_reg_40H |= (1 << 7);
	RA8875_WriteReg(0x40, s_reg_40H);	/* 设置为文本模式 */

	/*
		Serial Flash/ROM 频率频率设定
			0xb: SFCL 频率 = 系统频率频率(当DMA 为致能状态，并且色彩深度为256 色，则SFCL 频率
				固定为=系统频率频率/ 2)
			10b: SFCL 频率 =系统频率频率/ 2
			11b: SFCL 频率 =系统频率频率/ 4

		安富莱驱动板系统频率为 68MHz

		GT23L32S4W的访问速度:SPI 时钟频率:20MHz(max.)

		因此需要设置 4 分频, 17MHz
	*/
	RA8875_WriteReg(0x06, (3 << 0));	/* 设置为文本模式 */

	/* 选择外部字体ROM */
	RA8875_WriteReg(0x21, (0 << 7) | (1 << 5));

	/* 05H  REG[05h] Serial Flash/ROM Configuration Register (SROC)
		7	Serial Flash/ROM I/F # 选择
				0:选择Serial Flash/ROM 0 接口。[安富莱RA8875驱动板字库芯片接在 0 接口]
				1:选择Serial Flash/ROM 1 接口。
		6	Serial Flash/ROM 寻址模式
				0: 24 位寻址模式。
				此位必须设为0。
		5	Serial Flash/ROM 波形模式
				0: 波形模式 0。
				1: 波形模式 3。
		4-3	Serial Flash /ROM 读取周期 (Read Cycle)
			00b: 4 bus ?? 无空周期 (No Dummy Cycle)。
			01b: 5 bus ??1 byte 空周期。
			1Xb: 6 bus ??2 byte 空周期。
		2	Serial Flash /ROM 存取模式 (Access Mode)
			0: 字型模式 。
			1: DMA 模式。
		1-0	Serial Flash /ROM I/F Data Latch 选择模式
			0Xb: 单一模式。
			10b: 双倍模式0。
			11b: 双倍模式1。
	*/
	RA8875_WriteReg(0x05, (0 << 7) | (0 << 6) | (1 << 5) | (1 << 3) | (0 << 2) | (0 << 1));

	/*
		设置外部字体芯片型号为 GT23L32S4W, 编码为GB2312,

		Bit1:0 决定ASCII字符的格式:
			0 = NORMAL		 [笔画细, 和汉字顶部对齐]
			1 = Arial		 [笔画粗，和汉字底部对齐]
			2 = Roman		 [笔画细, 和汉字底部对齐]
			3 = Bold		 [乱码,不可用]
	 */
	RA8875_WriteReg(0x2F, (4 << 5) | (0 << 2) | (1 << 0));

	s_ucRA8875Busy = 1;
	RA8875_WriteCmd(0x02); 			/* 用于设定RA8875 进入内存(DDRAM或CGRAM)读取/写入模式 */

	/* 开始循环处理字符 */
	while (*_ptr != 0)
	{
		RA8875_WriteData(*_ptr);
		while ((RA8875_ReadStatus() & 0x80) == 0x80);
		//while ((RA8875_ReadStatus() & 0x01) != 0);	
		//while(RA8875_ReadBusy());
		_ptr++;
	}
	s_ucRA8875Busy = 0;

	s_reg_40H &= 0x7F;
	RA8875_WriteReg(0x40, s_reg_40H);	/* 还原为图形模式 */
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_SetDispWin
*	功能说明: 设置显示窗口，进入窗口显示模式。在窗口显示模式，连续写显存，光标会自动在设定窗口内进行递增
*	形    参:
*		_usX : 水平坐标
*		_usY : 垂直坐标
*		_usHeight: 窗口高度
*		_usWidth : 窗口宽度
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_SetDispWin(uint16_t _usX, uint16_t _usY, uint16_t _usHeight, uint16_t _usWidth)
{

	uint16_t usTemp;

	/* 坐标系统示意图: （横屏）
			 -----------------------------
			|(0,0)                        |
			|     --------->              |
			|         |                   |
			|         |                   |
			|         |                   |
			|         V                   |
			|     --------->              |
			|                    (479,271)|
			 -----------------------------

		左上角是坐标零点, 扫描方向，先从左到右，再从上到下。

		如果需要做竖屏方式，你需要进行物理坐标和逻辑坐标的转换
	*/
	RA8875_WriteReg(0x30, _usX);
    RA8875_WriteReg(0x31, _usX >> 8);

	RA8875_WriteReg(0x32, _usY);
    RA8875_WriteReg(0x33, _usY >> 8);

	usTemp = _usWidth + _usX - 1;
	RA8875_WriteReg(0x34, usTemp);
    RA8875_WriteReg(0x35, usTemp >> 8);

	usTemp = _usHeight + _usY - 1;
	RA8875_WriteReg(0x36, usTemp);
    RA8875_WriteReg(0x37, usTemp >> 8);

	RA8875_SetCursor(_usX, _usY);

	/* 保存当前窗口信息，提高以后单色填充操作的效率.
	另外一种做法是通过读取0x30-0x37寄存器获得当前窗口，不过效率较低 */
	s_WinX = _usX;
	s_WinY = _usY;
	s_WinHeight = _usHeight;
	s_WinWidth = _usWidth;
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_SetCursor
*	功能说明: 设置写显存的光标位置（图形模式）
*	形    参:  _usX : X坐标; _usY: Y坐标
*	返 回 值: 无
*********************************************************************************************************
*/
static void RA8875_SetCursor(uint16_t _usX, uint16_t _usY)
{
	if (g_LcdDirection > 1)	/* 竖屏  */
	{
		RA8875_WriteReg(0x46, _usY);
		RA8875_WriteReg(0x47, _usY >> 8);
		RA8875_WriteReg(0x48, _usX);
		RA8875_WriteReg(0x49, _usX >> 8);
	}
	else	/* 横屏 */
	{
		RA8875_WriteReg(0x46, _usX);
		RA8875_WriteReg(0x47, _usX >> 8);
		RA8875_WriteReg(0x48, _usY);
		RA8875_WriteReg(0x49, _usY >> 8);
	}
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_SetReadCursor
*	功能说明: 设置读显存的光标位置。 很多其他的控制器写光标和读光标是相同的寄存器，但是RA8875是不同的。
*	形    参:  _usX : X坐标; _usY: Y坐标
*	返 回 值: 无
*********************************************************************************************************
*/
static void RA8875_SetReadCursor(uint16_t _usX, uint16_t _usY)
{
	if (g_LcdDirection > 1)	/* 竖屏  */
	{
		uint16_t temp;
		
		temp = _usX;
		_usX = _usY;
		_usY = temp;
	}
		
	/* 设置内存读光标的坐标 */
	RA8875_WriteReg(0x4A, _usX);
	RA8875_WriteReg(0x4B, _usX >> 8);
	RA8875_WriteReg(0x4C, _usY);
	RA8875_WriteReg(0x4D, _usY >> 8);
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_SetTextCursor
*	功能说明: 设置文本写入光标位置
*	形    参:  _usX : X坐标; _usY: Y坐标
*	返 回 值: 无
*********************************************************************************************************
*/
static void RA8875_SetTextCursor(uint16_t _usX, uint16_t _usY)
{
	if (g_LcdDirection > 1)	/* 竖屏  */
	{
		uint16_t temp;
		
		temp = _usX;
		_usX = _usY;
		_usY = temp;
	}
			
	/* 设置内存读光标的坐标 */
	RA8875_WriteReg(0x2A, _usX);
	RA8875_WriteReg(0x2B, _usX >> 8);
	RA8875_WriteReg(0x2C, _usY);
	RA8875_WriteReg(0x2D, _usY >> 8);
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_QuitWinMode
*	功能说明: 退出窗口显示模式，变为全屏显示模式
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_QuitWinMode(void)
{
	RA8875_SetDispWin(0, 0, g_LcdHeight, g_LcdWidth);
}


/*
*********************************************************************************************************
*	函 数 名: RA8875_CtrlGPO
*	功能说明: 控制GPO输出状态，也就是KOUT0-KOUT3的状态
*	形    参: _pin : 0-3, 分别对应 GPO0 GPO1 GPO2 GPO3
*			  _value : 0 或 1
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_CtrlGPO(uint8_t _pin, uint8_t _value)
{
	RA8875_WriteReg(0x13, _value << _pin);
}

/*
*********************************************************************************************************
*	函 数 名: RA8875_SetBackLight
*	功能说明: 配置RA8875芯片的PWM1相关寄存器，控制LCD背光
*	形    参:  _bright 亮度，0是灭，255是最亮
*	返 回 值: 无
*********************************************************************************************************
*/
void RA8875_SetBackLight(uint8_t _bright)
{
	if (_bright == 0)
	{
		/* 关闭PWM, PWM1引脚缺省输出LOW  */
		RA8875_WriteReg(0x8A, 0 << 6);
	}
	else if (_bright == BRIGHT_MAX)	/* 最大亮度 */
	{
		/* 关闭PWM, PWM1引脚缺省输出HIGH */
		RA8875_WriteReg(0x8A, 1 << 6);
	}
	else
	{
		/* 使能PWM1, 进行占空比调节 */

		/* 	REG[8Ah] PWM1 Control Register (P1CR)

			Bit7 脉波宽度调变 (PWM1) 设定
				0 : 关闭，此状态下，PWM1 输出准位依照此缓存器Bit6 决定。
				1 : 开启。

			Bit6 PWM1 关闭时的准位
				0 : 当PWM 关闭或于睡眠模式时，PWM1 输出为”Low” 状态。
				1 : 当PWM 关闭或于睡眠模式时，PWM1 输出为”High” 状态。

			Bit5 保留

			Bit4 PWM1 功能选择
				0 : PWM1 功能。
				1 : PWM1 固定输出一频率为外部晶体振荡器Clock (Fin) 频率 1 /16 的Clock

			Bit3-0  PWM1 电路的频率来源选择PWM_CLK	【不是PWM输出频率】
				0000b : SYS_CLK / 1   , 1000b : SYS_CLK / 256
				0001b : SYS_CLK / 2   , 1001b : SYS_CLK / 512
				0010b : SYS_CLK / 4   , 1010b : SYS_CLK / 1024
				0011b : SYS_CLK / 8   , 1011b : SYS_CLK / 2048
				0100b : SYS_CLK / 16  , 1100b : SYS_CLK / 4096
				0101b : SYS_CLK / 32  , 1101b : SYS_CLK / 8192
				0110b : SYS_CLK / 64  , 1110b : SYS_CLK / 16384
				0111b : SYS_CLK / 128 , 1111b : SYS_CLK / 32768

				“SYS_CLK” 代表系统频率， 例如SYS_CLK 为20MHz， 当Bit[3:0] =0001b 时，PWM1 频率来源为10MHz。
				实际输出的PWM频率还需要除以 256，支持8位的分辨率。

				安富莱的4.3寸(480*272)模块，SYS_CLK =  68.75MHz
				安富莱的7.0寸(800*480)模块，SYS_CLK =  81.25MHz

				为了避免音频噪声，PWM频率可以选择
				（1） 低频100Hz
				（2） 高于 20KHz

				比如，Bit3-0为0011b时 SYS_CLK / 8，
					4.3寸 输出的PWM频率为 (68.75MHz / 8) / 256 = 33.56KHz
					7寸 输出的PWM频率为 (75 MHz / 8) / 256 = 36.62KHz
		*/

		// 5寸和7寸新屏可以用 3 ，高频PWM, 4.3寸不行
		RA8875_WriteReg(0x8A, (1 << 7) | 3);   
		// RA8875_WriteReg(0x8A, (1 << 7) | 12);

		/* REG[8Bh] PWM1 Duty Cycle Register (P1DCR) */
		RA8875_WriteReg(0x8B, _bright);
	}
}


/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
