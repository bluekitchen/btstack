/*
*********************************************************************************************************
*
*	模块名称 : TIM基本定时中断和PWM驱动模块
*	文件名称 : bsp_tim_pwm.c
*	版    本 : V1.1
*	说    明 : 利用STM32F4内部TIM输出PWM信号， 并实现基本的定时中断
*	修改记录 :
*		版本号  日期        作者     说明
*		V1.0    2013-08-16 armfly  正式发布
*		V1.1	2014-06-15 armfly  完善 bsp_SetTIMOutPWM，当占空比=0和100%时，关闭定时器，GPIO配置为输出
*		V1.2	2015-05-08 armfly  解决TIM8不能输出PWM的问题。
*		V1.3	2015-07-30 armfly  增加反相引脚输出PWM函数 bsp_SetTIMOutPWM_N();
*
*	Copyright (C), 2015-2016, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/*
	可以输出到GPIO的TIM通道:

	PA0  TIM5_CH1
	PA0  TIM5_CH2  TIM2_CH2
	PA2  TIM5_CH3  TIM2_CH3
	PA3  TIM5_CH4  TIM2_CH4
	
	PA6  TIM3_CH1
	PA7  TIM3_CH2
	
	PB0  TIM3_CH3
	PB1  TIM3_CH4

	PE9   TIM1_CH1
	PE11  TIM1_CH2
	PE13  TIM1_CH3

	PE14  TIM1_CH4
	
	PD12	TIM4_CH1
	PD13	TIM4_CH2
	PD14	TIM4_CH3
	PD15	TIM4_CH4	
	
	PC6		TIM8_CH1
	PC7		TIM8_CH2
	PC8		TIM8_CH3
	PC9		TIM8_CH4	
	
	PA8		TIM1_CH1
	PA9		TIM1_CH2
	PA10	TIM1_CH3
	PA11	TIM1_CH4	

	PB3		TIM2_CH2
	PB4		TIM3_CH1
	PB5		TIM3_CH2
	
	PB6		TIM4_CH1
	PB7		TIM4_CH2	
	PB8		TIM4_CH3
	PB9		TIM4_CH4		

	APB1 定时器有 TIM2, TIM3 ,TIM4, TIM5, TIM6, TIM7        --- 36M
	APB2 定时器有 TIM1, TIM8                    ---- 72M
*/

/*
*********************************************************************************************************
*	函 数 名: bsp_GetRCCofGPIO
*	功能说明: 根据GPIO 得到RCC寄存器
*	形    参：无
*	返 回 值: GPIO外设时钟名
*********************************************************************************************************
*/
uint32_t bsp_GetRCCofGPIO(GPIO_TypeDef* GPIOx)
{
	uint32_t rcc = 0;

	if (GPIOx == GPIOA)
	{
		rcc = RCC_APB2Periph_GPIOA;
	}
	else if (GPIOx == GPIOB)
	{
		rcc = RCC_APB2Periph_GPIOB;
	}
	else if (GPIOx == GPIOC)
	{
		rcc = RCC_APB2Periph_GPIOC;
	}
	else if (GPIOx == GPIOD)
	{
		rcc = RCC_APB2Periph_GPIOD;
	}
	else if (GPIOx == GPIOE)
	{
		rcc = RCC_APB2Periph_GPIOE;
	}
	else if (GPIOx == GPIOF)
	{
		rcc = RCC_APB2Periph_GPIOF;
	}
	else if (GPIOx == GPIOG)
	{
		rcc = RCC_APB2Periph_GPIOG;
	}

	return rcc;
}

/*
*********************************************************************************************************
*	函 数 名: bsp_GetRCCofTIM
*	功能说明: 根据TIM 得到RCC寄存器
*	形    参：无
*	返 回 值: TIM外设时钟名
*********************************************************************************************************
*/
uint32_t bsp_GetRCCofTIM(TIM_TypeDef* TIMx)
{
	uint32_t rcc = 0;

	/*
		APB1 定时器有 TIM2, TIM3 ,TIM4, TIM5, TIM6, TIM7, TIM12, TIM13, TIM14
		APB2 定时器有 TIM1, TIM8 ,TIM9, TIM10, TIM11
	*/
	if (TIMx == TIM1)
	{
		rcc = RCC_APB2Periph_TIM1;
	}
	else if (TIMx == TIM8)
	{
		rcc = RCC_APB2Periph_TIM8;
	}
	else if (TIMx == TIM9)
	{
		rcc = RCC_APB2Periph_TIM9;
	}
	else if (TIMx == TIM10)
	{
		rcc = RCC_APB2Periph_TIM10;
	}
	else if (TIMx == TIM11)
	{
		rcc = RCC_APB2Periph_TIM11;
	}
	/* 下面是 APB1时钟 */
	else if (TIMx == TIM2)
	{
		rcc = RCC_APB1Periph_TIM2;
	}
	else if (TIMx == TIM3)
	{
		rcc = RCC_APB1Periph_TIM3;
	}
	else if (TIMx == TIM4)
	{
		rcc = RCC_APB1Periph_TIM4;
	}
	else if (TIMx == TIM5)
	{
		rcc = RCC_APB1Periph_TIM5;
	}
	else if (TIMx == TIM6)
	{
		rcc = RCC_APB1Periph_TIM6;
	}
	else if (TIMx == TIM7)
	{
		rcc = RCC_APB1Periph_TIM7;
	}
	else if (TIMx == TIM12)
	{
		rcc = RCC_APB1Periph_TIM12;
	}
	else if (TIMx == TIM13)
	{
		rcc = RCC_APB1Periph_TIM13;
	}
	else if (TIMx == TIM14)
	{
		rcc = RCC_APB1Periph_TIM14;
	}

	return rcc;
}

/*
*********************************************************************************************************
*	函 数 名: bsp_ConfigTimGpio
*	功能说明: 配置GPIO和TIM时钟， GPIO连接到TIM输出通道
*	形    参: GPIOx
*			 GPIO_PinX
*			 TIMx
*			 _ucChannel
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_ConfigTimGpio(GPIO_TypeDef* GPIOx, uint16_t GPIO_PinX, TIM_TypeDef* TIMx, uint8_t _ucChannel)
{
	GPIO_InitTypeDef  GPIO_InitStructure;

	/* 使能GPIO时钟 */
	RCC_APB2PeriphClockCmd(bsp_GetRCCofGPIO(GPIOx), ENABLE);

  	/* 使能TIM时钟 */
	if ((TIMx == TIM1) || (TIMx == TIM8))
	{
		RCC_APB2PeriphClockCmd(bsp_GetRCCofTIM(TIMx), ENABLE);
	}
	else
	{
		RCC_APB1PeriphClockCmd(bsp_GetRCCofTIM(TIMx), ENABLE);
	}

	/* 配置GPIO */
	GPIO_InitStructure.GPIO_Pin = GPIO_PinX;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;		/* 复用功能 */
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOx, &GPIO_InitStructure); 
}

/*
*********************************************************************************************************
*	函 数 名: bsp_ConfigGpioOut
*	功能说明: 配置GPIO为推挽输出。主要用于PWM输出，占空比为0和100的情况。
*	形    参: GPIOx
*			  GPIO_PinX
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_ConfigGpioOut(GPIO_TypeDef* GPIOx, uint16_t GPIO_PinX)
{
	GPIO_InitTypeDef  GPIO_InitStructure;

	/* 使能GPIO时钟 */
	RCC_APB2PeriphClockCmd(bsp_GetRCCofGPIO(GPIOx), ENABLE);

	/* 配置GPIO */
	GPIO_InitStructure.GPIO_Pin = GPIO_PinX;		/* 带入的形参 */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	/* 输出 */
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOx, &GPIO_InitStructure);
}

/*
*********************************************************************************************************
*	函 数 名: bsp_SetTIMOutPWM
*	功能说明: 设置引脚输出的PWM信号的频率和占空比.  当频率为0，并且占空为0时，关闭定时器，GPIO输出0；
*			  当频率为0，占空比为100%时，GPIO输出1.
*	形    参: _ulFreq : PWM信号频率，单位Hz  (实际测试，最大输出频率为 168M / 4 = 42M）. 0 表示禁止输出
*			  _ulDutyCycle : PWM信号占空比，单位：万分之一。如5000，表示50.00%的占空比
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_SetTIMOutPWM(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, TIM_TypeDef* TIMx, uint8_t _ucChannel,
	 uint32_t _ulFreq, uint32_t _ulDutyCycle)
{
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	TIM_OCInitTypeDef  TIM_OCInitStructure;
	uint16_t usPeriod;
	uint16_t usPrescaler;
	uint32_t uiTIMxCLK;

	if (_ulDutyCycle == 0)
	{		
		TIM_Cmd(TIMx, DISABLE);		/* 关闭PWM输出 */
		bsp_ConfigGpioOut(GPIOx, GPIO_Pin);	/* 配置GPIO为推挽输出 */		
		GPIO_WriteBit(GPIOx, GPIO_Pin, Bit_RESET);	/* PWM = 0 */		
		return;
	}
	else if (_ulDutyCycle == 10000)
	{
		TIM_Cmd(TIMx, DISABLE);		/* 关闭PWM输出 */

		bsp_ConfigGpioOut(GPIOx, GPIO_Pin);	/* 配置GPIO为推挽输出 */		
		GPIO_WriteBit(GPIOx, GPIO_Pin, Bit_SET);	/* PWM = 1 */	
		return;
	}
	

	bsp_ConfigTimGpio(GPIOx, GPIO_Pin, TIMx, _ucChannel);	/* 使能GPIO和TIM时钟，并连接TIM通道到GPIO */
	
    /*-----------------------------------------------------------------------
		system_stm32f4xx.c 文件中 void SetSysClock(void) 函数对时钟的配置如下：

		HCLK = SYSCLK / 1     (AHB1Periph)
		PCLK2 = HCLK / 2      (APB2Periph)
		PCLK1 = HCLK / 4      (APB1Periph)

		因为APB1 prescaler != 1, 所以 APB1上的TIMxCLK = PCLK1 x 2 = SystemCoreClock / 2;
		因为APB2 prescaler != 1, 所以 APB2上的TIMxCLK = PCLK2 x 2 = SystemCoreClock;

		APB1 定时器有 TIM2, TIM3 ,TIM4, TIM5, TIM6, TIM6, TIM12, TIM13,TIM14
		APB2 定时器有 TIM1, TIM8 ,TIM9, TIM10, TIM11

	----------------------------------------------------------------------- */
	if ((TIMx == TIM1) || (TIMx == TIM8) || (TIMx == TIM9) || (TIMx == TIM10) || (TIMx == TIM11))
	{
		/* APB2 定时器 */
		uiTIMxCLK = SystemCoreClock;
	}
	else	/* APB1 定时器 */
	{
		uiTIMxCLK = SystemCoreClock;	// SystemCoreClock / 2;
	}

	if (_ulFreq < 100)
	{
		usPrescaler = 10000 - 1;					/* 分频比 = 10000 */
		usPeriod =  (uiTIMxCLK / 10000) / _ulFreq  - 1;		/* 自动重装的值 */
	}
	else if (_ulFreq < 3000)
	{
		usPrescaler = 100 - 1;					/* 分频比 = 100 */
		usPeriod =  (uiTIMxCLK / 100) / _ulFreq  - 1;		/* 自动重装的值 */
	}
	else	/* 大于4K的频率，无需分频 */
	{
		usPrescaler = 0;					/* 分频比 = 1 */
		usPeriod = uiTIMxCLK / _ulFreq - 1;	/* 自动重装的值 */
	}

	/* Time base configuration */
	TIM_TimeBaseStructure.TIM_Period = usPeriod;
	TIM_TimeBaseStructure.TIM_Prescaler = usPrescaler;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;	
	TIM_TimeBaseInit(TIMx, &TIM_TimeBaseStructure);

	/* PWM1 Mode configuration: Channel1 */
	TIM_OCStructInit(&TIM_OCInitStructure);		/* 初始化结构体成员 */
	
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = (_ulDutyCycle * usPeriod) / 10000;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	
	TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Disable;	/* only for TIM1 and TIM8. */	
	TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;			/* only for TIM1 and TIM8. */		
	TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;		/* only for TIM1 and TIM8. */
	TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Reset;		/* only for TIM1 and TIM8. */
	
	if (_ucChannel == 1)
	{
		TIM_OC1Init(TIMx, &TIM_OCInitStructure);
		TIM_OC1PreloadConfig(TIMx, TIM_OCPreload_Enable);
	}
	else if (_ucChannel == 2)
	{
		TIM_OC2Init(TIMx, &TIM_OCInitStructure);
		TIM_OC2PreloadConfig(TIMx, TIM_OCPreload_Enable);
	}
	else if (_ucChannel == 3)
	{
		TIM_OC3Init(TIMx, &TIM_OCInitStructure);
		TIM_OC3PreloadConfig(TIMx, TIM_OCPreload_Enable);
	}
	else if (_ucChannel == 4)
	{
		TIM_OC4Init(TIMx, &TIM_OCInitStructure);
		TIM_OC4PreloadConfig(TIMx, TIM_OCPreload_Enable);
	}

	TIM_ARRPreloadConfig(TIMx, ENABLE);

	/* TIMx enable counter */
	TIM_Cmd(TIMx, ENABLE);

	/* 下面这句话对于TIM1和TIM8是必须的，对于TIM2-TIM6则不必要 */
	if ((TIMx == TIM1) || (TIMx == TIM8))
	{
		TIM_CtrlPWMOutputs(TIMx, ENABLE);
	}
}

/*
*********************************************************************************************************
*	函 数 名: bsp_SetTIMOutPWM_N
*	功能说明: 设置TIM8_CH1N 等反相引脚输出的PWM信号的频率和占空比.  当频率为0，并且占空为0时，关闭定时器，GPIO输出0；
*			  当频率为0，占空比为100%时，GPIO输出1.
*	形    参: _ulFreq : PWM信号频率，单位Hz  (实际测试，最大输出频率为 168M / 4 = 42M）. 0 表示禁止输出
*			  _ulDutyCycle : PWM信号占空比，单位：万分之一。如5000，表示50.00%的占空比
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_SetTIMOutPWM_N(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, TIM_TypeDef* TIMx, uint8_t _ucChannel,
	 uint32_t _ulFreq, uint32_t _ulDutyCycle)
{
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	TIM_OCInitTypeDef  TIM_OCInitStructure;
	uint16_t usPeriod;
	uint16_t usPrescaler;
	uint32_t uiTIMxCLK;

	if (_ulDutyCycle == 0)
	{		
		TIM_Cmd(TIMx, DISABLE);		/* 关闭PWM输出 */
		bsp_ConfigGpioOut(GPIOx, GPIO_Pin);	/* 配置GPIO为推挽输出 */		
		GPIO_WriteBit(GPIOx, GPIO_Pin, Bit_RESET);	/* PWM = 0 */		
		return;
	}
	else if (_ulDutyCycle == 10000)
	{
		TIM_Cmd(TIMx, DISABLE);		/* 关闭PWM输出 */

		bsp_ConfigGpioOut(GPIOx, GPIO_Pin);	/* 配置GPIO为推挽输出 */		
		GPIO_WriteBit(GPIOx, GPIO_Pin, Bit_SET);	/* PWM = 1 */	
		return;
	}
	

	bsp_ConfigTimGpio(GPIOx, GPIO_Pin, TIMx, _ucChannel);	/* 使能GPIO和TIM时钟，并连接TIM通道到GPIO */
	
    /*-----------------------------------------------------------------------
		system_stm32f4xx.c 文件中 void SetSysClock(void) 函数对时钟的配置如下：

		HCLK = SYSCLK / 1     (AHB1Periph)
		PCLK2 = HCLK / 2      (APB2Periph)
		PCLK1 = HCLK / 4      (APB1Periph)

		因为APB1 prescaler != 1, 所以 APB1上的TIMxCLK = PCLK1 x 2 = SystemCoreClock / 2;
		因为APB2 prescaler != 1, 所以 APB2上的TIMxCLK = PCLK2 x 2 = SystemCoreClock;

		APB1 定时器有 TIM2, TIM3 ,TIM4, TIM5, TIM6, TIM6, TIM12, TIM13,TIM14
		APB2 定时器有 TIM1, TIM8 ,TIM9, TIM10, TIM11

	----------------------------------------------------------------------- */
	if ((TIMx == TIM1) || (TIMx == TIM8) || (TIMx == TIM9) || (TIMx == TIM10) || (TIMx == TIM11))
	{
		/* APB2 定时器 */
		uiTIMxCLK = SystemCoreClock;
	}
	else	/* APB1 定时器 */
	{
		uiTIMxCLK = SystemCoreClock;	// SystemCoreClock / 2;
	}

	if (_ulFreq < 100)
	{
		usPrescaler = 10000 - 1;					/* 分频比 = 10000 */
		usPeriod =  (uiTIMxCLK / 10000) / _ulFreq  - 1;		/* 自动重装的值 */
	}
	else if (_ulFreq < 3000)
	{
		usPrescaler = 100 - 1;					/* 分频比 = 100 */
		usPeriod =  (uiTIMxCLK / 100) / _ulFreq  - 1;		/* 自动重装的值 */
	}
	else	/* 大于4K的频率，无需分频 */
	{
		usPrescaler = 0;					/* 分频比 = 1 */
		usPeriod = uiTIMxCLK / _ulFreq - 1;	/* 自动重装的值 */
	}

	/* Time base configuration */
	TIM_TimeBaseStructure.TIM_Period = usPeriod;
	TIM_TimeBaseStructure.TIM_Prescaler = usPrescaler;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;	
	TIM_TimeBaseInit(TIMx, &TIM_TimeBaseStructure);

	/* PWM1 Mode configuration: Channel1 */
	TIM_OCStructInit(&TIM_OCInitStructure);		/* 初始化结构体成员 */
	
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Disable;		/* 和 bsp_SetTIMOutPWM_N() 不同 */
	TIM_OCInitStructure.TIM_Pulse = (_ulDutyCycle * usPeriod) / 10000;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	
	TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable;		/* only for TIM1 and TIM8. */	
	TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;			/* only for TIM1 and TIM8. */		
	TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;		/* only for TIM1 and TIM8. */
	TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Reset;		/* only for TIM1 and TIM8. */
	
	if (_ucChannel == 1)
	{
		TIM_OC1Init(TIMx, &TIM_OCInitStructure);
		TIM_OC1PreloadConfig(TIMx, TIM_OCPreload_Enable);
	}
	else if (_ucChannel == 2)
	{
		TIM_OC2Init(TIMx, &TIM_OCInitStructure);
		TIM_OC2PreloadConfig(TIMx, TIM_OCPreload_Enable);
	}
	else if (_ucChannel == 3)
	{
		TIM_OC3Init(TIMx, &TIM_OCInitStructure);
		TIM_OC3PreloadConfig(TIMx, TIM_OCPreload_Enable);
	}
	else if (_ucChannel == 4)
	{
		TIM_OC4Init(TIMx, &TIM_OCInitStructure);
		TIM_OC4PreloadConfig(TIMx, TIM_OCPreload_Enable);
	}

	TIM_ARRPreloadConfig(TIMx, ENABLE);

	/* TIMx enable counter */
	TIM_Cmd(TIMx, ENABLE);

	/* 下面这句话对于TIM1和TIM8是必须的，对于TIM2-TIM6则不必要 */
	if ((TIMx == TIM1) || (TIMx == TIM8))
	{
		TIM_CtrlPWMOutputs(TIMx, ENABLE);
	}
}


/*
*********************************************************************************************************
*	函 数 名: bsp_SetTIMforInt
*	功能说明: 配置TIM和NVIC，用于简单的定时中断. 开启定时中断。 中断服务程序由应用程序实现。
*	形    参: TIMx : 定时器
*			  _ulFreq : 定时频率 （Hz）。 0 表示关闭。
*			  _PreemptionPriority : 中断优先级分组
*			  _SubPriority : 子优先级
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_SetTIMforInt(TIM_TypeDef* TIMx, uint32_t _ulFreq, uint8_t _PreemptionPriority, uint8_t _SubPriority)
{
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	uint16_t usPeriod;
	uint16_t usPrescaler;
	uint32_t uiTIMxCLK;

  	/* 使能TIM时钟 */
	if ((TIMx == TIM1) || (TIMx == TIM8))
	{
		RCC_APB2PeriphClockCmd(bsp_GetRCCofTIM(TIMx), ENABLE);
	}
	else
	{
		RCC_APB1PeriphClockCmd(bsp_GetRCCofTIM(TIMx), ENABLE);
	}

	if (_ulFreq == 0)
	{
		TIM_Cmd(TIMx, DISABLE);		/* 关闭定时输出 */

		/* 关闭TIM定时更新中断 (Update) */
		{
			NVIC_InitTypeDef NVIC_InitStructure;	/* 中断结构体在 misc.h 中定义 */
			uint8_t irq = 0;	/* 中断号, 定义在 stm32f4xx.h */

			if (TIMx == TIM1)
				irq = TIM1_UP_IRQn;
			else if (TIMx == TIM2)
				irq = TIM2_IRQn;
			else if (TIMx == TIM3)
				irq = TIM3_IRQn;
			else if (TIMx == TIM4)
				irq = TIM4_IRQn;
			else if (TIMx == TIM5)
				irq = TIM5_IRQn;
			else if (TIMx == TIM6)
				irq = TIM6_IRQn;
			else if (TIMx == TIM7)
				irq = TIM7_IRQn;
			else if (TIMx == TIM8)
				irq = TIM8_UP_IRQn;

			NVIC_InitStructure.NVIC_IRQChannel = irq;
			NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = _PreemptionPriority;
			NVIC_InitStructure.NVIC_IRQChannelSubPriority = _SubPriority;
			NVIC_InitStructure.NVIC_IRQChannelCmd = DISABLE;
			NVIC_Init(&NVIC_InitStructure);
		}		
		return;
	}

    /*-----------------------------------------------------------------------
		system_stm32f4xx.c 文件中 static void SetSysClockToHSE(void) 函数对时钟的配置如下：

			//HCLK = SYSCLK 
			RCC->CFGR |= (uint32_t)RCC_CFGR_HPRE_DIV1;
			  
			//PCLK2 = HCLK
			RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE2_DIV1;
			
			//PCLK1 = HCLK
			RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE1_DIV1;

		APB1 定时器有 TIM2, TIM3 ,TIM4, TIM5, TIM6, TIM7, TIM12, TIM13,TIM14
		APB2 定时器有 TIM1, TIM8 ,TIM9, TIM10, TIM11

	----------------------------------------------------------------------- */
	if ((TIMx == TIM1) || (TIMx == TIM8) || (TIMx == TIM9) || (TIMx == TIM10) || (TIMx == TIM11))
	{
		/* APB2 定时器 */
		uiTIMxCLK = SystemCoreClock;
	}
	else	/* APB1 定时器 .  */
	{
		uiTIMxCLK = SystemCoreClock;	// SystemCoreClock / 2;
	}

	if (_ulFreq < 100)
	{
		usPrescaler = 10000 - 1;					/* 分频比 = 1000 */
		usPeriod =  (uiTIMxCLK / 10000) / _ulFreq  - 1;		/* 自动重装的值 */
	}
	else if (_ulFreq < 3000)
	{
		usPrescaler = 100 - 1;					/* 分频比 = 100 */
		usPeriod =  (uiTIMxCLK / 100) / _ulFreq  - 1;		/* 自动重装的值 */
	}
	else	/* 大于4K的频率，无需分频 */
	{
		usPrescaler = 0;					/* 分频比 = 1 */
		usPeriod = uiTIMxCLK / _ulFreq - 1;	/* 自动重装的值 */
	}

	/* Time base configuration */
	TIM_TimeBaseStructure.TIM_Period = usPeriod;
	TIM_TimeBaseStructure.TIM_Prescaler = usPrescaler;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;

	TIM_TimeBaseInit(TIMx, &TIM_TimeBaseStructure);

	TIM_ARRPreloadConfig(TIMx, ENABLE);

	/* TIM Interrupts enable */
	TIM_ITConfig(TIMx, TIM_IT_Update, ENABLE);

	/* TIMx enable counter */
	TIM_Cmd(TIMx, ENABLE);

	/* 配置TIM定时更新中断 (Update) */
	{
		NVIC_InitTypeDef NVIC_InitStructure;	/* 中断结构体在 misc.h 中定义 */
		uint8_t irq = 0;	/* 中断号, 定义在 stm32f4xx.h */

		if (TIMx == TIM1)
			irq = TIM1_UP_IRQn;
		else if (TIMx == TIM2)
			irq = TIM2_IRQn;
		else if (TIMx == TIM3)
			irq = TIM3_IRQn;
		else if (TIMx == TIM4)
			irq = TIM4_IRQn;
		else if (TIMx == TIM5)
			irq = TIM5_IRQn;
		else if (TIMx == TIM6)
			irq = TIM6_IRQn;
		else if (TIMx == TIM7)
			irq = TIM7_IRQn;
		else if (TIMx == TIM8)
			irq = TIM8_UP_IRQn;

		NVIC_InitStructure.NVIC_IRQChannel = irq;
		NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = _PreemptionPriority;
		NVIC_InitStructure.NVIC_IRQChannelSubPriority = _SubPriority;
		NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
		NVIC_Init(&NVIC_InitStructure);
	}
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
