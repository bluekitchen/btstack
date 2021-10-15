/*
*********************************************************************************************************
*
*	模块名称 : RA8875芯片和MCU之间的接口驱动
*	文件名称 : bsp_ra8875_port.h
*	版    本 : V1.0
*	说    明 : 头文件
*
*	Copyright (C), 2010-2011, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/


#ifndef _BSP_RA8875_PORT_H
#define _BSP_RA8875_PORT_H

/* 下面两个宏决定是否编译相关的代码 */
#define	RA_HARD_SPI_EN
#define	RA_HARD_8080_16_EN

/* 软件未使用WAIT引脚。可以通过读取寄存器状态寄存器判断芯片忙 */
#define USE_WAIT_PIN		

#ifdef USE_WAIT_PIN
	/* RA8875芯片的WAIT引脚 */
	#define RCC_WAIT	RCC_APB2Periph_GPIOB
	#define PORT_WAIT	GPIOB
	#define PIN_WAIT	GPIO_Pin_5
#endif

#ifdef RA_HARD_SPI_EN	/* 硬件 SPI 界面 (需要改变RA8875屏上的2个电阻位置) */
	/*
	【1】安富莱STM32-V4开发板,STM32F103ZET6
		PB5/LCD_BUSY		--- 触摸芯片忙    未使用   （RA8875屏是RA8875芯片的忙信号)
		PB1/LCD_PWM			--- LCD背光PWM控制  （RA8875屏无需此脚，背光由RA8875控制)

		PG11/TP_NCS			--- 触摸芯片的片选		(RA8875屏无需SPI接口触摸芯片）
		PA5/SPI3_SCK		--- 触摸芯片SPI时钟		(RA8875屏无需SPI接口触摸芯片）
		PA6/SPI3_MISO		--- 触摸芯片SPI数据线MISO(RA8875屏无需SPI接口触摸芯片）
		PA7/SPI3_MOSI		--- 触摸芯片SPI数据线MOSI(RA8875屏无需SPI接口触摸芯片）

		PC5/TP_INT			--- 触摸芯片中断 （对于RA8875屏，是RA8875输出的中断)
	*/
	#define RA8875_CS_0()	GPIOG->BRR = GPIO_Pin_11
	#define RA8875_CS_1()	GPIOG->BSRR = GPIO_Pin_11

	#define SPI_WRITE_DATA	0x00
	#define SPI_READ_DATA	0x40
	#define SPI_WRITE_CMD	0x80
	#define SPI_READ_STATUS	0xC0

	static uint8_t SPI_ShiftByte(uint8_t _ucByte);
#endif

#ifdef RA_HARD_8080_16_EN		/* 8080硬件总线 （安富莱RA8875屏缺省模式） */
	/*
		安富莱STM32-V4开发板接线方法：

		PD0/FSMC_D2
		PD1/FSMC_D3
		PD4/FSMC_NOE		--- 读控制信号，OE = Output Enable ， N 表示低有效
		PD5/FSMC_NWE		--- 写控制信号，WE = Output Enable ， N 表示低有效
		PD8/FSMC_D13
		PD9/FSMC_D14
		PD10/FSMC_D15
		PD14/FSMC_D0
		PD15/FSMC_D1

		PF0/FSMC_A0		--- 地址 RS
		
		PE4/FSMC_A20		--- 和主片选一起译码
		PE5/FSMC_A21		--- 和主片选一起译码
		PE7/FSMC_D4
		PE8/FSMC_D5
		PE9/FSMC_D6
		PE10/FSMC_D7
		PE11/FSMC_D8
		PE12/FSMC_D9
		PE13/FSMC_D10
		PE14/FSMC_D11
		PE15/FSMC_D12

		PG12/FSMC_NE4		--- 主片选（TFT, OLED 和 AD7606）

		PC3/TP_INT			--- 触摸芯片中断 （对于RA8875屏，是RA8875输出的中断)

		---- 下面是 TFT LCD接口其他信号 （FSMC模式不使用）----
		PB5/LCD_BUSY		--- 触摸芯片忙    未使用   （RA8875屏是RA8875芯片的忙信号)
		PB1/LCD_PWM			--- LCD背光PWM控制  （RA8875屏无需此脚，背光由RA8875控制)
		
		PG11/TP_NCS			--- 触摸芯片的片选		(RA8875屏无需SPI接口触摸芯片）
		PA5/SPI1_SCK		--- 触摸芯片SPI时钟		(RA8875屏无需SPI接口触摸芯片）
		PA6/SPI1_MISO		--- 触摸芯片SPI数据线MISO(RA8875屏无需SPI接口触摸芯片）
		PA7/SPI1_MOSI		--- 触摸芯片SPI数据线MOSI(RA8875屏无需SPI接口触摸芯片）

	*/
	#define RA8875_BASE		((uint32_t)(0x6C000000 | 0x00000000))

	#define RA8875_REG		*(__IO uint16_t *)(RA8875_BASE +  (1 << (0 + 1)))	/* FSMC 16位总线模式下，FSMC_A0口线对应物理地址A1 */
	#define RA8875_RAM		*(__IO uint16_t *)(RA8875_BASE)

	#define RA8875_RAM_ADDR		RA8875_BASE
#endif

/* 定义RA8875的访问接口类别. V5程序仅支持 RA_HARD_SPI 和 RA_HARD_8080_16 */
enum
{
	RA_SOFT_8080_8 = 0,	/* 软件8080接口模式, 8bit */
	RA_SOFT_SPI,	   	/* 软件SPI接口模式 */
	RA_HARD_SPI,	   	/* 硬件SPI接口模式 */
	RA_HARD_8080_16		/* 硬件8080接口,16bit */
};

void RA8875_ConfigGPIO(void);
void RA8875_Delaly1us(void);
void RA8875_Delaly1ms(void);
uint16_t RA8875_ReadID(void);
void RA8875_WriteCmd(uint8_t _ucRegAddr);
void RA8875_WriteData(uint8_t _ucRegValue);
uint8_t RA8875_ReadData(void);
void RA8875_WriteData16(uint16_t _usRGB);
uint16_t RA8875_ReadData16(void);
uint8_t RA8875_ReadStatus(void);
uint32_t RA8875_GetDispMemAddr(void);

/* 四线SPI接口模式 */
void RA8875_InitSPI(void);
void RA8875_HighSpeedSPI(void);
void RA8875_LowSpeedSPI(void);

uint8_t RA8875_ReadBusy(void);
extern uint8_t g_RA8875_IF;

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
