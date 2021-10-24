/*
*********************************************************************************************************
*
*	模块名称 : 步进电机驱动模块
*	文件名称 : bsp_step_moto.c
*	版    本 : V1.02
*	说    明 : 通过GPIO控制步进电机A B C D 线圈依次导通，驱动电机旋转。电机型号: 28BYJ48
*			   步进电机的驱动原理很简单，只需要4个输出GPIO即可。本驱动的特点是采用硬件中断控制脉冲速度和
*			   个数。
*			   本驱动使用 TIM6 作为硬件定时中断.
*
*	修改记录 :
*		版本号  日期         作者     说明
*		V1.0    2015-07-19   armfly  正式发布
*
*	Copyright (C), 2015-2016, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/* 定义GPIO端口 */
#define RCC_MOTO_A   RCC_APB2Periph_GPIOC
#define PORT_MOTO_A  GPIOC
#define PIN_MOTO_A   GPIO_Pin_12

#define RCC_MOTO_B   RCC_APB2Periph_GPIOC
#define PORT_MOTO_B  GPIOC
#define PIN_MOTO_B   GPIO_Pin_7

#define RCC_MOTO_C   RCC_APB2Periph_GPIOC
#define PORT_MOTO_C  GPIOC
#define PIN_MOTO_C   GPIO_Pin_8

#define RCC_MOTO_D   RCC_APB2Periph_GPIOC
#define PORT_MOTO_D  GPIOC
#define PIN_MOTO_D   GPIO_Pin_9

#define MOTO_A_0()		PORT_MOTO_A->BRR = PIN_MOTO_A
#define MOTO_A_1()		PORT_MOTO_A->BSRR = PIN_MOTO_A

#define MOTO_B_0()		PORT_MOTO_B->BRR = PIN_MOTO_B
#define MOTO_B_1()		PORT_MOTO_B->BSRR = PIN_MOTO_B

#define MOTO_C_0()		PORT_MOTO_C->BRR = PIN_MOTO_C
#define MOTO_C_1()		PORT_MOTO_C->BSRR = PIN_MOTO_C

#define MOTO_D_0()		PORT_MOTO_D->BRR = PIN_MOTO_D
#define MOTO_D_1()		PORT_MOTO_D->BSRR = PIN_MOTO_D

MOTO_T g_tMoto;

/*
*********************************************************************************************************
*	函 数 名: bsp_InitStepMoto
*	功能说明: 配置步进电器驱动IO
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitStepMoto(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* 打开GPIO时钟 */
	RCC_APB2PeriphClockCmd(RCC_MOTO_A | RCC_MOTO_B | RCC_MOTO_C | RCC_MOTO_D, ENABLE);

	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	/* 推挽输出模式 */
	
	GPIO_InitStructure.GPIO_Pin = PIN_MOTO_A;
	GPIO_Init(PORT_MOTO_A, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = PIN_MOTO_B;
	GPIO_Init(PORT_MOTO_B, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = PIN_MOTO_C;
	GPIO_Init(PORT_MOTO_C, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = PIN_MOTO_D;
	GPIO_Init(PORT_MOTO_D, &GPIO_InitStructure);	
	
	g_tMoto.Dir = 0;
	g_tMoto.StepFreq = 0;
	g_tMoto.StepCount = 0;
	g_tMoto.Running = 0;
	g_tMoto.Pos = 0;
}

/*
*********************************************************************************************************
*	函 数 名: MOTO_StartWork
*	功能说明: 控制电机开始旋转
*	形    参: _speed 旋转速度. 换相的频率。Hz
*			 _dir    旋转方向 0 表示正转， 1表示反转
*			 _step   旋转步数 0 表示一直旋转
*	返 回 值: 无
*********************************************************************************************************
*/
void MOTO_Start(uint32_t _speed, uint8_t _dir, uint32_t _stpes)
{
	g_tMoto.Dir = _dir;
	g_tMoto.StepFreq = _speed;
	g_tMoto.StepCount = _stpes;
	g_tMoto.Running = 1;
	
	//void bsp_SetTIMforInt(TIM_TypeDef* TIMx, uint32_t _ulFreq, uint8_t _PreemptionPriority, uint8_t _SubPriority)
	bsp_SetTIMforInt(TIM6, _speed, 2, 2);
}

/*
*********************************************************************************************************
*	函 数 名: MOTO_ShangeSpeed
*	功能说明: 控制电机的步进速度
*	形    参: _speed 旋转速度. 换相的频率。Hz
*			 _dir    旋转方向 0 表示正转， 1表示反转
*			 _step   旋转步数 0 表示一直旋转
*	返 回 值: 无
*********************************************************************************************************
*/
void MOTO_ShangeSpeed(uint32_t _speed)
{
	g_tMoto.StepFreq = _speed;
	
	if (g_tMoto.Running == 1)
	{
		//void bsp_SetTIMforInt(TIM_TypeDef* TIMx, uint32_t _ulFreq, uint8_t _PreemptionPriority, uint8_t _SubPriority)
		bsp_SetTIMforInt(TIM6, _speed, 2, 2);
	}
}

/*
*********************************************************************************************************
*	函 数 名: MOTO_Stop
*	功能说明: 控制电机停止运行。所有线圈释放。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void MOTO_Stop(void)
{
	//void bsp_SetTIMforInt(TIM_TypeDef* TIMx, uint32_t _ulFreq, uint8_t _PreemptionPriority, uint8_t _SubPriority)
	bsp_SetTIMforInt(TIM6, 0, 0, 0);
	g_tMoto.Running = 0;
	
	/* 所有线圈停电 */
	MOTO_A_1();
	MOTO_B_1();
	MOTO_C_1();
	MOTO_D_1();		
}


/*
*********************************************************************************************************
*	函 数 名: MOTO_Pause
*	功能说明: 控制电机暂停旋转。 保持力矩，线圈依然通电。
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void MOTO_Pause(void)
{
	//void bsp_SetTIMforInt(TIM_TypeDef* TIMx, uint32_t _ulFreq, uint8_t _PreemptionPriority, uint8_t _SubPriority)
	bsp_SetTIMforInt(TIM6, 0, 0, 0);
	g_tMoto.Running = 0;
}

/*
*********************************************************************************************************
*	函 数 名: MOTO_RoudToStep
*	功能说明: 圈数换算为步数。28BYJ48 电机步距角度 = 5.625/64度.
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
uint32_t MOTO_RoudToStep(void)
{
	uint32_t steps;
	
	/* 28BYJ48 电机步距角度 = 5.625/64度. 
		一圈 360度；
		step = 360 / (5.625 / 64)
	*/	
	steps = (360 * 64 * 1000 / 4) / 5625;		// 4096步
	
	return steps;
}


/*
*********************************************************************************************************
*	函 数 名: MOTO_ISR
*	功能说明: 中断服务程序
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void MOTO_ISR(void)
{
	uint8_t n;
	
	if (g_tMoto.Running == 0)
	{
		return;
	}

#if 1	/* A - AB - B - BC - C - CD - D - DA */
	switch (g_tMoto.Pos)
	{
		case 0:			
			MOTO_A_0();
			MOTO_B_1();
			MOTO_C_1();
			MOTO_D_1();
			break;
		
		case 1:
			MOTO_A_0();
			MOTO_B_0();
			MOTO_C_1();
			MOTO_D_1();	
			break;	

		case 2:
			MOTO_A_1();
			MOTO_B_0();
			MOTO_C_1();
			MOTO_D_1();	
			break;			

		case 3:
			MOTO_A_1();
			MOTO_B_0();
			MOTO_C_0();
			MOTO_D_1();	
			break;					

		case 4:
			MOTO_A_1();
			MOTO_B_1();
			MOTO_C_0();
			MOTO_D_1();	
			break;		

		case 5:
			MOTO_A_1();
			MOTO_B_1();
			MOTO_C_0();
			MOTO_D_0();	
			break;		

		case 6:
			MOTO_A_1();
			MOTO_B_1();
			MOTO_C_1();
			MOTO_D_0();	
			break;									
			
		case 7:
			MOTO_A_0();
			MOTO_B_1();
			MOTO_C_1();
			MOTO_D_0();	
			break;							
	}
	n = 7;
#else
	/* A -  B -  C  - D  */
	switch (g_tMoto.Pos)
	{
		case 0:			
			MOTO_A_0();
			MOTO_B_1();
			MOTO_C_1();
			MOTO_D_1();
			break;
		
		case 1:
			MOTO_A_1();
			MOTO_B_0();
			MOTO_C_1();
			MOTO_D_1();	
			break;	

		case 2:
			MOTO_A_1();
			MOTO_B_1();
			MOTO_C_0();
			MOTO_D_1();	
			break;			

		case 3:
			MOTO_A_1();
			MOTO_B_1();
			MOTO_C_1();
			MOTO_D_0();	
			break;											
	}	
	n = 3;
#endif	
	if (g_tMoto.Dir == 0)	/* 正转 */
	{
		if (++g_tMoto.Pos > n)
		{
			g_tMoto.Pos = 0;
		}
	}
	else	/* 反转 */
	{
		if (g_tMoto.Pos == 0)
		{
			g_tMoto.Pos = n;
		}
		else
		{
			g_tMoto.Pos--;
		}
	}
	
	if (g_tMoto.StepCount > 0)
	{		
		g_tMoto.StepCount--;
		
		if (g_tMoto.StepCount == 0)
		{		
			TIM_ClearITPendingBit(TIM6, TIM_IT_Update);		/* 清除中断标志位 */
			
			MOTO_Pause();	/* 暂停 */
		}
	}	
}

/*
*********************************************************************************************************
*	函 数 名: TIM6_IRQHandler
*	功能说明: 外部中断服务程序.
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
#ifndef TIM6_ISR_MOVE_OUT		/* bsp.h 中定义此行，表示本函数移到 stam32fxxx_it.c。 避免重复定义 */
void TIM6_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM6, TIM_IT_Update) != RESET)
	{
		MOTO_ISR();	/* 中断服务程序 */

		TIM_ClearITPendingBit(TIM6, TIM_IT_Update);		/* 清除中断标志位 */
	}
}
#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
