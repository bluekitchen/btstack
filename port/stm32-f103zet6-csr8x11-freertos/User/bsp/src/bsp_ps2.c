/*
*********************************************************************************************************
*
*	模块名称 : PS/2键盘鼠标驱动模块
*	文件名称 : bsp_ps2.c
*	版    本 : V1.0
*	说    明 :
*
*	修改记录 :
*		版本号  日期         作者     说明
*		V1.0    2014-02-12   armfly  正式发布
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

/*
	使用说明:
	1. 开机执行1次 bsp_InitPS2() 配置GPIO
	2. 执行 PS2_StartWork() 打开PS2中断
	3. 每ms调用 PS2_Timer().  在bsp.c 中bsp_RunPer10ms() 函数中增加调用即可。
	4. 不需要PS2功能时, 调用 PS2_StopWork() 关闭中断。	
	
	PC13 启用了EXIT 下降沿中断。
	
	RCC_APB2Periph_SYSCFG时钟忘记打开，导致PC、PE端口中断混乱（目前测试发现PC、PE有这个问题）
*/

#include "bsp.h"

/* 打印调试语句 */
#define ps2_printf printf
//#define ps2_printf(...)

/* 定义GPIO端口  PC13 = PS2_CLK   PG8 = PS2_DATA */
#define RCC_PS2_CLK   RCC_APB2Periph_GPIOC
#define PORT_PS2_CLK  GPIOC
#define PIN_PS2_CLK   GPIO_Pin_13

#define RCC_PS2_DATA  RCC_APB2Periph_GPIOG
#define PORT_PS2_DATA GPIOG
#define PIN_PS2_DATA  GPIO_Pin_8

#define PS2_DATA_IS_HIGH()	((PORT_PS2_DATA->IDR & PIN_PS2_DATA) != 0)

#define PS2_CLK_1()  PORT_PS2_CLK->BSRR = PIN_PS2_CLK				/* CLK = 1 */
#define PS2_CLK_0()  PORT_PS2_CLK->BRR = PIN_PS2_CLK				/* CLK = 0 */

#define PS2_DATA_1() PORT_PS2_DATA->BSRR = PIN_PS2_DATA			/* DATA = 1 */
#define PS2_DATA_0() PORT_PS2_DATA->BRR = PIN_PS2_DATA			/* DATA = 0 */


typedef struct
{
	uint32_t code;
	char * str;
}KB_STR_T;

static const KB_STR_T s_KeyNameTab[] = 
{	
	{0xEEEEEEEE, ""},

    {KB_A, "A"},
    {KB_B, "B"},
    {KB_C, "C"},
    {KB_D, "D"},
    {KB_E, "E"},
    {KB_F, "F"},
    {KB_G, "G"},
    {KB_H, "H"},
    {KB_I, "I"},
    {KB_J, "J"},
    {KB_K, "K"},
    {KB_L, "L"},
    {KB_M, "M"},
    {KB_N, "N"},
    {KB_O, "O"},
    {KB_P, "P"},
    {KB_Q, "Q"},
    {KB_R, "R"},
    {KB_S, "S"},
    {KB_T, "T"},
    {KB_U, "U"},
    {KB_V, "V"},
    {KB_W, "W"},
    {KB_X, "X"},
    {KB_Y, "Y"},
    {KB_Z, "Z"},
    {KB_0, "0"},
    {KB_1, "1"},
    {KB_2, "2"},
    {KB_3, "3"},
    {KB_4, "4"},
    {KB_5, "5"},
    {KB_6, "6"},
    {KB_7, "7"},
    {KB_8, "8"},
    {KB_9, "9"},
    {KB_PIE, "`"},	/* 撇，键盘左上角 */
    {KB_SUB, "-"},	/* 中杠，减号 */
    {KB_EQU, "="},	/* 等号 */
    {KB_FXG, "\\"},	/* 反斜杠 */
    {KB_BKSP,"Backspace"},
    {KB_SPACE, "Space"},
    {KB_TAB, "Tab"},
    {KB_CAPS, "CapsLk"},
    {KB_L_SHFT, "Shift Left"},
    {KB_L_CTRL, "Ctrl Left"},
    {KB_L_GUI, "GUI Left"},
    {KB_L_ALT, "Alt Left"},
    {KB_R_SHFT,"Shift Right"},
    {KB_R_CTRL, "Ctrl Right"},
    {KB_R_GUI, "Gui Right"},
    {KB_R_ALT, "Alt Right"},
    {KB_APPS, "Apps"},
    {KB_ENTER, "Enter"},
    {KB_ESC, "ESC"},
    {KB_F1, "F1"},
    {KB_F2, "F2"},
    {KB_F3, "F3"},
    {KB_F4, "F4"},
    {KB_F5, "F5"},
    {KB_F6, "F6"},
    {KB_F7, "F7"},
    {KB_F8, "F8"},
    {KB_F9, "F9"},
    {KB_F10, "F10"},
    {KB_F11, "F11"},
    {KB_F12, "F12"},
    {KB_PRNT_SCRN, "Print Screen/SysRq"},
    {KB_SCROLL, "Scroll Lock"},
    {KB_PAUSE, "Pause/Break"},
    {KB_ZZKH, "["},
    {KB_INSERT, "Insert"},
    {KB_HOME, "Home"},
    {KB_PGUP, "Page Up"},
    {KB_DELETE, "Delete"},
    {KB_END, "End"},
    {KB_PGDN, "Page Down"},
    {KB_U_ARROW, "Up Arrow"},
    {KB_L_ARROW, "Left Arrow"},
    {KB_D_ARROW, "Down Arrow"},
    {KB_R_ARROW, "Right Arrow"},
    {KB_NUM, "Num Lock"},
    {KB_KP_DIV, "KP /"},    /* 小键盘除号  KP 表示小键盘 */
    {KB_KP_MULT, "KP *"},	/* 小键盘乘号 */
    {KB_KP_SUB, "KP -"},	/* - */
    {KB_KP_ADD, "KP +"},
    {KB_KP_ENTER, "KP Enter"},
    {KB_KP_DOT, "KP ."},	/* 小数点 */
    {KB_KP_0, "KP 0"},
    {KB_KP_1, "KP 1"},
    {KB_KP_2, "KP 2"},
    {KB_KP_3, "KP 3"},
    {KB_KP_4, "KP 4"},
    {KB_KP_5, "KP 5"},
    {KB_KP_6, "KP 6"},
    {KB_KP_7, "KP 7"},
    {KB_KP_8, "KP 8"},
    {KB_KP_9, "KP 9"},
    {KB_YZKH, "]"}, 	/* ] 右中括号 */
    {KB_SEMICOLON, ";"},/* ; 分号 */
    {KB_QUOTES, "'"},	/* 单引号 */
    {KB_COMMA, ","},	/* 逗号 */
    {KB_DOT, "."},		/* 小数点 */
    {KB_DIV, "/"},		/* 除号 */
	
	{0, ""}		/* 查表结束标志 */
};

PS2_T g_tPS2;

static void PS2_SendCmd(uint8_t _byte);
static uint8_t PS2_WaitMsg(uint32_t *_rsp, uint16_t _timeout);
static uint8_t PS2_HookKeyboard(uint32_t _msg);

/*
*********************************************************************************************************
*	函 数 名: bsp_InitPS2
*	功能说明: 配置STM32的GPIO,用于PS2
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitPS2(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* 打开GPIO时钟 */
	RCC_APB2PeriphClockCmd(RCC_PS2_CLK | RCC_PS2_DATA, ENABLE);

	PS2_CLK_1();
	PS2_DATA_1();

	/* 配置CLK 和 DATA 为输出开漏引脚 */
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;

	GPIO_InitStructure.GPIO_Pin = PIN_PS2_CLK;
	GPIO_Init(PORT_PS2_CLK, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = PIN_PS2_DATA;
	GPIO_Init(PORT_PS2_DATA, &GPIO_InitStructure);
	
	g_tPS2.TxTimeOut = 0;
	g_tPS2.RxTimeOut = 0;
}

/*
*********************************************************************************************************
*	函 数 名: PS2_StartWork
*	功能说明: 配置中断，开始解码
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void PS2_StartWork(void)
{
	EXTI_InitTypeDef   EXTI_InitStructure;
	NVIC_InitTypeDef   NVIC_InitStructure;

	/* 使能SYSCFG时钟 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);  /* Enable AFIO clock */

	/* Connect EXTI13 Line to PC13 pin */
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource13);	

	/* 配置 EXTI LineXXX */
	EXTI_InitStructure.EXTI_Line = EXTI_Line13;
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;		/* 下降沿(等待 DRDY 由1变0的时刻) */
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure);

	/* 设置NVIC优先级分组 */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

	/* 中断优先级配置 最低优先级 这里一定要分开的设置中断，不能够合并到一个里面设置 */
	NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x03;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x03;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	PS2_CLK_1();
	PS2_DATA_1();

	g_tPS2.Status = 0;

	PS2_ClearBuf();		/* 复位FIFO，清除缓冲区 */
}

/*
*********************************************************************************************************
*	函 数 名: PS2_StopWork
*	功能说明: 停止解码
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void PS2_StopWork(void)
{
	EXTI_InitTypeDef   EXTI_InitStructure;
//	NVIC_InitTypeDef   NVIC_InitStructure;

	/* 配置 EXTI LineXXX */
	EXTI_InitStructure.EXTI_Line = EXTI_Line13;
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;	/* 下降沿(等待 DRDY 由1变0的时刻) */
	EXTI_InitStructure.EXTI_LineCmd = DISABLE;		/* 禁止 */
	EXTI_Init(&EXTI_InitStructure);

#if 0
	/* 中断优先级配置 最低优先级 这里一定要分开的设置中断，不能够合并到一个里面设置 */
	NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x03;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x03;
	NVIC_InitStructure.NVIC_IRQChannelCmd = DISABLE;		/* 禁止 */
	NVIC_Init(&NVIC_InitStructure);
#endif

	PS2_ClearBuf();
}

/*
*********************************************************************************************************
*	函 数 名: PS2_Timer
*	功能说明: 此函数必须每ms被执行1次， 加入到 bsp.c 中的 bsp_RunPer1ms() 函数
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void PS2_Timer(void)
{
	if (g_tPS2.TxTimeOut > 0)
	{
		if (--g_tPS2.TxTimeOut == 0)
		{
			/* 释放 DATA线 */
			PS2_DATA_1();
		}
	}
	if (g_tPS2.RxTimeOut > 0)
	{
		g_tPS2.RxTimeOut--;

		if (g_tPS2.RxTimeOut == 0)
		{
			if (g_tPS2.Len > 0)
			{
				uint32_t value;

				if (g_tPS2.Len == 1)
				{
					value = g_tPS2.CodeBuf[0];
				}
				else if (g_tPS2.Len == 2)
				{
					value = (g_tPS2.CodeBuf[0] << 8) + g_tPS2.CodeBuf[1];
				}
				else if (g_tPS2.Len == 3)
				{
					value = (g_tPS2.CodeBuf[0] << 16) + (g_tPS2.CodeBuf[1] << 8) + g_tPS2.CodeBuf[2];
				}
				else
				{
					value = (g_tPS2.CodeBuf[0] << 24) + (g_tPS2.CodeBuf[1] << 16) +
							(g_tPS2.CodeBuf[2] << 8) + g_tPS2.CodeBuf[3];
				}

				g_tPS2.Len = 0;

				if (PS2_HookKeyboard(value))
				{
					if (g_tPS2.LedReq < 2)	/* 主机给键盘发送命令阶段 不将键盘的应答消息送往应用层 */
					{					
						PS2_PutMsg(value);	/* 将按键代码放入FIFO, 交给应用层处理 */
					}
				}
			}
		}
	}
	
	/* 给键盘发送命令，控制键盘的指示灯 */
	if (g_tPS2.LedReq > 0)
	{
		g_tPS2.LedReq++;
		if (g_tPS2.LedReq == 2)
		{			
			PS2_SendCmd(0xED);				/* 发送指令第1字节 -- 修改LED状态  */
		}
		else if (g_tPS2.LedReq == 7)
		{
			PS2_SendCmd(g_tPS2.LedData);	/* 延迟5ms 发送指令第2字节 -- 指示灯状态 */
		}		
		else if (g_tPS2.LedReq == 17)		/* 再延迟10ms, 等键盘的应答完成后退出 */
		{
			g_tPS2.LedReq = 0;
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: PS2_HookKeyboard
*	功能说明: 键盘钩子函数，交给应用程序之前进行预处理。
*	形    参: _msg  键值代码
*	返 回 值: 0 表示内部处理，不交给应用层。1表示还需要交给应用层处理。
*********************************************************************************************************
*/
static uint8_t PS2_HookKeyboard(uint32_t _msg)
{
	uint8_t reput = 1;	/* 正常使用时可以设置为0. CTRL , ALT ,SHIFT 按键事件无需上报应用层处理 */

	/* 处理PS2键盘状态 */
	switch (_msg)
	{
		case KB_L_SHFT:		/* Shift 键按下 */
		case KB_R_SHFT:
			g_tPS2.ksShift = 1;
			break;

		case BREAK_L_SHFT:	/* Shift 键松开 */
		case BREAK_R_SHFT:
			g_tPS2.ksShift = 0;
			break;

		case KB_L_CTRL:		/* Ctrl 键按下 */
		case KB_R_CTRL:
			g_tPS2.ksCtrl = 1;
			break;

		case BREAK_L_CTRL:	/* Ctrl 键松开 */
		case BREAK_R_CTRL:
			g_tPS2.ksCtrl = 0;
			break;

		case KB_L_ALT:		/* Alt 键按下 */
		case KB_R_ALT:
			g_tPS2.ksAlt = 1;
			break;

		case BREAK_L_ALT:	/* Alt 键松开 */
		case BREAK_R_ALT:
			g_tPS2.ksAlt = 0;
			break;

		case KB_CAPS:		/* 大小写切换 */
			if (g_tPS2.ksCapsLock == 0)
			{
				g_tPS2.ksCapsLock = 1;
				//ps2_printf(" 开Caps Lock灯\r\n");
			}
			else
			{
				g_tPS2.ksCapsLock = 0;
				//ps2_printf(" 关Caps Lock灯\r\n");
			}
			PS2_SetKeyboardLed(LED_CapsLock, g_tPS2.ksCapsLock);
			break;

		case KB_NUM:		/* 数字小键盘使能 */
			if (g_tPS2.KsNumLock == 0)
			{
				g_tPS2.KsNumLock = 1;
				//ps2_printf(" 开Num Lock灯\r\n");
			}
			else
			{
				g_tPS2.KsNumLock = 0;
				//ps2_printf(" 关Num Lock灯\r\n");
			}
			PS2_SetKeyboardLed(LED_NumLock, g_tPS2.KsNumLock);
			break;

		case KB_SCROLL:		/* */
			if (g_tPS2.KsScrollLock == 0)
			{
				g_tPS2.KsScrollLock = 1;
				//ps2_printf(" 开Scroll Lock灯\r\n");
			}
			else
			{
				g_tPS2.KsScrollLock = 0;
				//ps2_printf(" 关Scroll Lock灯\r\n");
			}
			PS2_SetKeyboardLed(LED_ScrollLock, g_tPS2.KsScrollLock);
			break;

		default:
			reput = 1;
			break;
	}

	return reput;
}

/*
*********************************************************************************************************
*	函 数 名: GetNameOfKey
*	功能说明: 获得按键名字
*	形    参: 无
*	返 回 值: 字符串指针
*********************************************************************************************************
*/
const char * GetNameOfKey(uint32_t _code)
{
	uint16_t i = 0;

	
	while (1)
	{		
		if (s_KeyNameTab[i].code == 0)
		{
			break;
		}
		
		if (_code == s_KeyNameTab[i].code)
		{
			return s_KeyNameTab[i].str;
		}
		i++;
	}
	
	return "";
}


/*
*********************************************************************************************************
*	函 数 名: PS2_PutMsg
*	功能说明: 将1个键值压入按键FIFO缓冲区。可用于模拟一个按键。
*	形    参:  _KeyCode : 按键代码
*	返 回 值: 无
*********************************************************************************************************
*/
void PS2_PutMsg(uint32_t _KeyCode)
{
	g_tPS2.Buf[g_tPS2.Write] = _KeyCode;

	if (++g_tPS2.Write  >= KEY_FIFO_SIZE)
	{
		g_tPS2.Write = 0;
	}
}

/*
*********************************************************************************************************
*	函 数 名: bsp_GetMsg
*	功能说明: 从按键FIFO缓冲区读取一个键值。
*	形    参:  无
*	返 回 值: 按键代码
*********************************************************************************************************
*/
uint32_t PS2_GetMsg(void)
{
	uint32_t ret;

	if (g_tPS2.Read == g_tPS2.Write)
	{
		return PS2_NONE;
	}
	else
	{
		ret = g_tPS2.Buf[g_tPS2.Read];

		if (++g_tPS2.Read >= KEY_FIFO_SIZE)
		{
			g_tPS2.Read = 0;
		}
		return ret;
	}
}

/*
*********************************************************************************************************
*	函 数 名: PS2_ClearBuf
*	功能说明: 清除按键缓冲区
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void PS2_ClearBuf(void)
{
	/* 对按键FIFO读写指针清零 */
	g_tPS2.Read = 0;
	g_tPS2.Write = 0;

	g_tPS2.Len = 0;
	g_tPS2.TxTimeOut = 0;
	g_tPS2.RxTimeOut = 0;
}

/*
*********************************************************************************************************
*	函 数 名: PS2_SendCmd
*	功能说明: 向鼠标或键盘发送1字节数据
*	形    参: _byte : 待发送的数据
*	返 回 值: 无
*********************************************************************************************************
*/
static void PS2_SendCmd(uint8_t _byte)
{
	/*
		主机必须按下面的步骤发送数据到PS/2设备：
		1)  把Clock线拉低至少100us；
		2)  把Data线拉低；
		3)  释放Clock线；
		4)  等待PS/2设备把Clock线拉低；
		5)  设置/复位Data线发送第一个数据位；
		6)  等待PS/2设备把时钟拉高；
		7)  等待PS/2设备把时钟拉低；
		8)  重复5-7步发送剩下的7个数据位和校验位；
		9)  释放Data线，即发送停止位(1)；
		10) 等待PS/2设备把Clock线拉高； //此步可省略,因为下一步PS/2设备还是会把Data线拉低的
		11) 等待PS/2设备把Data线拉低；
		12) 等待PS/2设备把Clock线拉低；
		13) 等待PS/2设备释放Clock线和Data线。
	*/

	g_tPS2.Sending = 1;
	g_tPS2.Cmd = _byte;
	g_tPS2.Status = 0;	/*  */

	PS2_CLK_0();		/* 把Clock线拉低至少100us --- 设置低后会触发CLK下降沿中断（中断服务程序应该丢弃这个事件） */
	bsp_DelayUS(100);

	PS2_DATA_0();		/* 把Data线拉低 */
	g_tPS2.TxTimeOut = 2;		/* 避免没插PS2设备时，DATA线一直被拉低 */

	bsp_DelayUS(50);

	PS2_CLK_1();		/* 释放Clock线 */

	/* 后面的过程有CLK下降沿中断服务程序完成 */
	
}

/*
*********************************************************************************************************
*	函 数 名: PS2_WaitMsg
*	功能说明: 等待应答.
*	形    参: _rsp : 存放结果
*			 _timeout : 超时时间，必须大于0。单位 ms
*	返 回 值: 0 表示无数据， 1表示成功
*********************************************************************************************************
*/
static uint8_t PS2_WaitMsg(uint32_t *_rsp, uint16_t _timeout)
{
	uint32_t read;
	uint16_t i;

	for (i = 0; i < _timeout; i++)
	{
		read = PS2_GetMsg();
		if (read != PS2_NONE)
		{
			*_rsp = read;
			break;
		}
		bsp_DelayUS(1000);
	}

	g_tPS2.Sending = 0;

	if (i == _timeout)
	{
		return 0;	/* 超时未读到数据 */
	}
	return 1;	/* 成功读到数据 */
}


/*
*********************************************************************************************************
*	函 数 名: PS2_GetDevceType
*	功能说明: 自动识别设备类型（鼠标还是键盘）
*	形    参: 无
*	返 回 值: 0 表示失败， 1表示成功
*********************************************************************************************************
*/
uint8_t PS2_GetDevceType(void)
{
	uint32_t Rsp;
	uint8_t dev;
	uint8_t i;

	/* 识别方法为：发送0xFF指令，应答FA    AA 的是键盘，应答 FA     AA 00 的是鼠标。 */

	dev = PS2_UNKNOW_DEVICE;

	for (i = 0; i < 3; i++)
	{
		/* 主板和PS2设备同时上电时，设备内部可能正在初始化，因此需要等待 */		
		PS2_SendCmd(0xFF);
		ps2_printf("   Host   : %X\r\n", 0xFF);
		if (PS2_WaitMsg(&Rsp, 20))	/* 20ms 超时 */
		{
			ps2_printf("   Device : %X\r\n", Rsp);			
			if (Rsp == 0xFA)
			{
				if (PS2_WaitMsg(&Rsp, 1000))
				{
					ps2_printf("   Device : %X\r\n", Rsp);

					if (Rsp == 0xAA00)
					{
						dev = PS2_MOUSE;	/* 鼠标 */
						break;
					}
					else if (Rsp == 0xAA)
					{
						dev = PS2_KEYBOARD;	/* 键盘 */
						break;
					}
				}
			}
		}
	}
	return dev;
}

/*
*********************************************************************************************************
*	函 数 名: PS2_SetKeyboardLed
*	功能说明: 设置键盘指示灯
*	形    参: _id : 哪个指示灯。LED_CpasLock , LED_NumLock,	LED_ScrollLock
*			  _on : 0 表示灭, 1 表示亮
*	返 回 值: 0 表示失败， 1表示成功
*********************************************************************************************************
*/
void PS2_SetKeyboardLed(uint8_t _id, uint8_t _on)
{
	/*
		0xED (Set/Reset LEDs) 主机在本命令后跟随一个参数字节用于指示键盘上Num Lock, Caps Lock,
		and Scroll Lock LED 的状态 这个参数字节的定义如下

		bin7      bin6      bin5      bin4      bin3      bin2       bin1      bin0
		Always 0  Always 0  Always 0  Always 0  Always 0  Caps Lock  Num Lock  Scroll Lock
		  "Scroll Lock" - Scroll Lock LED off(0)/on(1)
		  "Num Lock" - Num Lock LED off(0)/on(1)
		  "Caps Lock" - Caps Lock LED off(0)/on(1)
	*/
	uint8_t data;
	//uint32_t Rsp;

	if (_id == LED_CapsLock)
	{
		g_tPS2.ksCapsLock = _on;
	}
	else if (_id == LED_NumLock)
	{
		g_tPS2.KsNumLock = _on;
	}
	else if (_id == LED_ScrollLock)
	{
		g_tPS2.KsScrollLock = _on;
	}

	data = 0;
	if (g_tPS2.ksCapsLock == 1)
	{
		data |= (1 << 2);
	}
	if (g_tPS2.KsNumLock == 1)
	{
		data |= (1 << 1);
	}
	if (g_tPS2.KsScrollLock == 1)
	{
		data |= (1 << 0);
	}

#if 1	/* 在 systick ms中断服务程序中被执行 */	
	g_tPS2.LedReq = 1;
	g_tPS2.LedData = data;
#else	
	ps2_printf("Host : %02X",0xED);
	PS2_SendCmd(0xED);
	if (PS2_WaitMsg(&Rsp, 20))
	{
		ps2_printf("   KeyBoard : %X\r\n",Rsp);
	}

	PS2_SendCmd(data);
	if (PS2_WaitMsg(&Rsp, 20))
	{
		ps2_printf("   KeyBoard : %X\r\n",Rsp);
	}
#endif	
}

/*
*********************************************************************************************************
*	函 数 名: PS2_InitKeyboard
*	功能说明: 初始化键盘
*	形    参: 无
*	返 回 值: 0 表示失败， 1表示成功
*********************************************************************************************************
*/
uint8_t PS2_InitKeyboard(void)
{
	uint32_t Rsp;	/* 鼠标给主机的应答数据 */
	uint16_t i;
	uint8_t err = 0;

	const uint8_t ucCmdList[] = {
		0xFF,	// Reset command    ->  Mouse: FA AA 00

		0xED,	// 点亮3个指示灯
		0x07,

		0xED,	// 关闭3个指示灯
		0x00,
	};

	g_tPS2.ksShift = 0;					/* 1表示 Shift 键被按下 */
	g_tPS2.ksCtrl = 0;					/* 1表示 Ctrl 键被按下 */
	g_tPS2.ksAlt = 0;					/* 1表示 Alt 键被按下 */
	g_tPS2.ksCapsLock = 0;				/* 1表示 CapsLock 状态指示灯亮 */
	g_tPS2.KsNumLock = 0;				/* 1表示 NumLock 状态指示灯亮 */
	g_tPS2.KsScrollLock = 0;			/* 1表示 ScrollLock 状态指示灯亮 */

	ps2_printf("Init Keyboard ...\r\n");
	for (i = 0; i < sizeof(ucCmdList); i++)
	{
		ps2_printf("Host : %02X",ucCmdList[i]);
		PS2_SendCmd(ucCmdList[i]);
		if (PS2_WaitMsg(&Rsp, 20))
		{
			ps2_printf("   KeyBoard : %X\r\n",Rsp);

			if (ucCmdList[i] == 0xFF)	/* 如果是复位指令，则等到鼠标内部自检，然后返回0xAA 00 */
			{
				ps2_printf("   KeyBoard resetting ...\r\n");
				if (PS2_WaitMsg(&Rsp, 1000))
				{
					ps2_printf("   KeyBoard : %X\r\n",Rsp);
				}
			}
		}
		else
		{
			ps2_printf("   KeyBoard : 无应答\r\n");
			err = 1;
		}
	}

	if (err == 1)
	{
		return 0;
	}
	return 1;
}

/*
*********************************************************************************************************
*	函 数 名: PS2_InitMouse
*	功能说明: 初始化鼠标
*	形    参: 无
*	返 回 值: 0 表示失败， 1表示成功
*********************************************************************************************************
*/
uint8_t PS2_InitMouse(void)
{
	uint32_t Rsp;	/* 鼠标给主机的应答数据 */
	uint16_t i;
	const uint8_t ucCmdList[] = {
		0xFF,	// Reset command    ->  Mouse: FA AA 00

		0xF3,	// Set Sample Rate:Attempt to Enter Microsoft
				// Mouse: FA  Acknowledge : Scrolling Mouse mode

		0xC8,	// Decimal 200  ->  Mouse: FA  Acknowledge       :

		0xF3,	// Set Sample Rate -> Mouse: FA  Acknowledge

		0x64,	// decimal 100 -> Mouse: FA  Acknowledge       :
		0xF3,	// Set Sample Rate -> Mouse: FA  Acknowledge       :
		0x50,	// decimal 80 -> Mouse: FA  Acknowledge       :

		0xF2,	// Read Device Type ->	Mouse: FA  Acknowledge       :
									//  Mouse: 00  Mouse ID  Response 03 if microsoft scrolling mouse
		0xF3,	// Set Sample Rate ->	Mouse: FA  Acknowledge
		0x0A,	// decimal 10  -> Mouse: FA  Acknowledge
		0xF2,	// Read Device Type  ->  Mouse: FA  Acknowledge
				//						 Mouse: 00  Mouse ID

		0xE8,	// Set resolution ->  Mouse: FA  Acknowledge
		0x03,	// 8 Counts/mm  -> 	Mouse: FA  Acknowledge
		0xE6,	// Set Scaling 1:1  -> Mouse: FA  Acknowledge
		0xF3,	// Set Sample Rate  -> Mouse: FA  Acknowledge
		0x28,	// decimal 40  ->  Mouse: FA  Acknowledge
		0xF4,	// Enable  ->  Mouse: FA  Acknowledge
	};

	ps2_printf("Init Mouse ...\r\n");
	for (i = 0; i < sizeof(ucCmdList); i++)
	{
		ps2_printf("Host : %02X",ucCmdList[i]);
		PS2_SendCmd(ucCmdList[i]);
		if (PS2_WaitMsg(&Rsp, 20))
		{
			ps2_printf("   Mouse : %X\r\n",Rsp);

			if (ucCmdList[i] == 0xFF)	/* 如果是复位指令，则等到鼠标内部自检，然后返回0xAA 00 */
			{
				ps2_printf("   Mouse resetting ...\r\n");
				if (PS2_WaitMsg(&Rsp, 1000))
				{
					ps2_printf("   Mouse : %X\r\n",Rsp);
				}
			}
		}
		else
		{
			ps2_printf("   Mouse : 无应答\r\n");
		}
	}
	return 1;
}

/*
*********************************************************************************************************
*	函 数 名: PS2_IsMousePacket
*	功能说明: 判断数据包是不是位移数据包
*	形    参: _input:4字节数据
*	返 回 值: 1 表示是鼠标位移数据包  0 表示不是
*********************************************************************************************************
*/
uint8_t PS2_IsMousePacket(uint32_t _input)
{
	/* 鼠标发送的第1个字节 */
	if (_input & 0x08000000)
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
*	函 数 名: PS2_DecodeMouse
*	功能说明: 解码鼠标数据包
*	形    参: _input: 鼠标发送的数据包（3字节或4字节）
*			  _res: 解码结果
*	返 回 值: 无
*********************************************************************************************************
*/
void PS2_DecodeMouse(uint32_t _input, MOUSE_PACKET_T *_res)
{
	uint32_t data;

	data = _input;
	if ((_input & 0xFF000000) == 0)		/* 标准鼠标，3字节格式 */
	{
		data <<= 8;			/* 左移1个字节, 便于统一3字节数据包和4字节数据包共用解码语句 */
		_res->Intelli = 0;	/* 0表示标准鼠标 */
	}
	else
	{
		_res->Intelli = 1;	/* 1表示智能鼠标鼠标 */
	}

	/* 智能鼠标， 4字节格式 */
		/*
		        Bit7       Bit6       Bit5      Bit4     Bit3      Bit 2      Bit 1      Bit 0
		Byte 1  Yoverflow  Xoverflow  YsignBit  XsignBit Always 1  MiddleBtn  RightBtn   LeftBtn
		Byte 2       X Movement
		Byte 3       Y Movement
		Byte 4  Always 0   Always 0   5th Btn   4th Btn  Z3        Z2         Z1         Z0
		*/
	/* 标准鼠标，3字节格式，前3字节和智能鼠标格式相同，没有第4字节 */


	/* Y值溢出 */
	if (data & 0x80000000)
	{
		_res->Yoverflow = 1;
	}
	else
	{
		_res->Yoverflow = 0;
	}

	/* X值溢出 */
	if (data & 0x40000000)
	{
		_res->Xoverflow = 1;
	}
	else
	{
		_res->Xoverflow = 0;
	}

	/* 鼠标X轴位移 */
	_res->Xmove = ((data >> 16) & 0xFF);
	if (data & 0x10000000)
	{
		_res->Xmove |= 0xFF00;	/* 扩展符号位 */
	}

	/* 鼠标Y轴位移 */
	_res->Ymove = ((data >> 8) & 0xFF);
	if (data & 0x20000000)
	{
		_res->Ymove |= 0xFF00;	/* 扩展符号位 */
	}

	/* 鼠标中键 */
	if (data & 0x04000000)
	{
		_res->BtnMid = 1;
	}
	else
	{
		_res->BtnMid = 0;
	}

	/* 鼠标右键 */
	if (data & 0x02000000)
	{
		_res->BtnRight = 1;
	}
	else
	{
		_res->BtnRight = 0;
	}

	/* 鼠标左键 */
	if (data & 0x01000000)
	{
		_res->BtnLeft = 1;
	}
	else
	{
		_res->BtnLeft = 0;
	}

	/*---------------- 下面的代码仅对智能鼠标有用 ----------------*/

	/* 鼠标第5个键 */
	if (data & 0x00000020)
	{
		_res->Btn5 = 1;
	}
	else
	{
		_res->Btn5 = 0;
	}

	/* 鼠标第4个键 */
	if (data & 0x00000010)
	{
		_res->Btn4 = 1;
	}
	else
	{
		_res->Btn4 = 0;
	}

	/*
		鼠标滚轮。对于有两个滚轮(1个垂直的+1个水平的)鼠标。
			+1 表示垂直滚轮向上滚动，-1表示垂直滚轮向下滚动；
			+2 表示水平滚轮向右滚动，-2表示水平滚轮向做滚动。
	*/
	_res->Zmove = (data & 0xF);
	if (data & 0x00000008)
	{
		_res->Zmove |= 0xFFF0;
	}
}

/*
*********************************************************************************************************
*	函 数 名: PS2_ISR
*	功能说明: 中断服务程序
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void PS2_ISR(void)
{
	static uint8_t s_Byte;
	static uint8_t s_Pos;
	static uint8_t s_1Bits;
	uint8_t data;
	int32_t time;
	static int32_t s_last_time = 0;

	if (g_tPS2.Sending == 0)	/* 主机接收键盘和鼠标数据的状态 */
	{
		if (PS2_DATA_IS_HIGH())	/* 读取 PS2_DATA 口线的电平 */
		{
			data = 1;
		}
		else
		{
			data = 0;
		}

		/* 下面的代码用于超时同步. 如果2次中断的时间间隔大于2ms 则同步起始位 */
		time = bsp_GetRunTime();
		if (time - s_last_time > 2)
		{
			g_tPS2.Status = 0;
		}
		s_last_time = time;

		/* 解码状态机 */
		switch (g_tPS2.Status)
		{
			case 0:			/* 起始位0 */
				if (data == 0)
				{
					g_tPS2.Status = 1;

					s_Pos = 0;
					s_Byte = 0;
					s_1Bits = 0;	/* 字节中1个个数 */
				}
				break;

			case 1:			/* 8位数据位 （bit0 先传） */
				s_Byte >>= 1;
				if (data == 1)
				{
					s_Byte += 0x80;
					s_1Bits++;		/* 统计1的个数 */
				}
				s_Pos++;
				if (s_Pos == 8)
				{
					g_tPS2.Status = 2;	/* 进入下一个状态接收校验位 */
				}
				break;

			case 2:			/* 奇校验位 */
				s_1Bits += data;
				if (s_1Bits % 2)
				{
					g_tPS2.Status = 3;	/* 正确，进入下一个状态接收停止位 */
				}
				else
				{
					/* 校验不正确, 异常 */
					g_tPS2.Status = 0;
				}
				break;

			case 3:			/* 处理停止位 */
				if (data == 1)
				{
					if (g_tPS2.Len < PS2_MAX_LEN)
					{
						g_tPS2.CodeBuf[g_tPS2.Len++] = s_Byte;
					}
					g_tPS2.RxTimeOut = 6;		/* 6ms 超时判断结束 */
				}
				g_tPS2.Status = 0;	/* 继续下一个bit */
				break;
		}
	}
	else		/* g_tPS2.Sending == 1, 主机发送命令的状态 */
	{
		/* 先传Bit0 */
		switch (g_tPS2.Status)
		{
			case 0:					/* 起始位 */
				g_tPS2.TxTimeOut = 0;
			
				g_tPS2.Status = 2;
				s_1Bits = 0;
				s_Pos = 0;
				s_Byte = g_tPS2.Cmd;
				break;

			case 2:					/* 发送8个数据位 */
				if (s_Byte & 0x01)
				{
					PS2_DATA_1();	/* 把Data线拉高 */
					s_1Bits++;		/* 统计1的个数 */
				}
				else
				{
					PS2_DATA_0();	/* 把Data线拉低 */
				}
				s_Byte >>= 1;
				if (++s_Pos >= 8)
				{
					g_tPS2.Status = 3;
				}
				break;

			case 3:					/* 发送奇校验位 */
				if (s_1Bits % 2)
				{
					PS2_DATA_0();	/* 把Data线拉低 */
				}
				else
				{
					PS2_DATA_1();	/* 把Data线拉高 */
				}
				g_tPS2.Status = 4;
				break;

			case 4:					/* 发送停止位 */
				PS2_DATA_1();	/* 把Data线拉高 */
				g_tPS2.Status = 5;
				break;

			case 5:					/* 设备此时会拉低 DATA， 表示ACK应答信号 */
				if (PS2_DATA_IS_HIGH())	/* 读取 PS2_DATA 口线的电平 */
				{
					g_tPS2.Ack = 1;
				}
				else
				{
					g_tPS2.Ack = 0;
				}

				/* 进入接收状态 */
				g_tPS2.Sending = 0;
				g_tPS2.Status = 0;
				break;
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: EXTI9_5_IRQHandler
*	功能说明: 外部中断服务程序.
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
#ifndef EXTI15_10_ISR_MOVE_OUT		/* bsp.h 中定义此行，表示本函数移到 stam32f1xx_it.c。 避免重复定义 */
void EXTI15_10_IRQHandler(void)
{
	if (EXTI_GetITStatus(EXTI_Line13) != RESET)
	{
		PS2_ISR();	/* PS2键盘中断服务程序 */
		
		EXTI_ClearFlag(EXTI_Line13);
		EXTI_ClearITPendingBit(EXTI_Line13);		/* 清除中断标志位 */
	}
}
#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
