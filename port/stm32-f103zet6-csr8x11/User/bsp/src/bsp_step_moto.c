/*
*********************************************************************************************************
*
*	ģ������ : �����������ģ��
*	�ļ����� : bsp_step_moto.c
*	��    �� : V1.02
*	˵    �� : ͨ��GPIO���Ʋ������A B C D ��Ȧ���ε�ͨ�����������ת������ͺ�: 28BYJ48
*			   �������������ԭ��ܼ򵥣�ֻ��Ҫ4�����GPIO���ɡ����������ص��ǲ���Ӳ���жϿ��������ٶȺ�
*			   ������
*			   ������ʹ�� TIM6 ��ΪӲ����ʱ�ж�.
*
*	�޸ļ�¼ :
*		�汾��  ����         ����     ˵��
*		V1.0    2015-07-19   armfly  ��ʽ����
*
*	Copyright (C), 2015-2016, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

/* ����GPIO�˿� */
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
*	�� �� ��: bsp_InitStepMoto
*	����˵��: ���ò�����������IO
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void bsp_InitStepMoto(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* ��GPIOʱ�� */
	RCC_APB2PeriphClockCmd(RCC_MOTO_A | RCC_MOTO_B | RCC_MOTO_C | RCC_MOTO_D, ENABLE);

	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	/* �������ģʽ */
	
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
*	�� �� ��: MOTO_StartWork
*	����˵��: ���Ƶ����ʼ��ת
*	��    ��: _speed ��ת�ٶ�. �����Ƶ�ʡ�Hz
*			 _dir    ��ת���� 0 ��ʾ��ת�� 1��ʾ��ת
*			 _step   ��ת���� 0 ��ʾһֱ��ת
*	�� �� ֵ: ��
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
*	�� �� ��: MOTO_ShangeSpeed
*	����˵��: ���Ƶ���Ĳ����ٶ�
*	��    ��: _speed ��ת�ٶ�. �����Ƶ�ʡ�Hz
*			 _dir    ��ת���� 0 ��ʾ��ת�� 1��ʾ��ת
*			 _step   ��ת���� 0 ��ʾһֱ��ת
*	�� �� ֵ: ��
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
*	�� �� ��: MOTO_Stop
*	����˵��: ���Ƶ��ֹͣ���С�������Ȧ�ͷš�
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void MOTO_Stop(void)
{
	//void bsp_SetTIMforInt(TIM_TypeDef* TIMx, uint32_t _ulFreq, uint8_t _PreemptionPriority, uint8_t _SubPriority)
	bsp_SetTIMforInt(TIM6, 0, 0, 0);
	g_tMoto.Running = 0;
	
	/* ������Ȧͣ�� */
	MOTO_A_1();
	MOTO_B_1();
	MOTO_C_1();
	MOTO_D_1();		
}


/*
*********************************************************************************************************
*	�� �� ��: MOTO_Pause
*	����˵��: ���Ƶ����ͣ��ת�� �������أ���Ȧ��Ȼͨ�硣
*	��    ��: ��
*	�� �� ֵ: ��
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
*	�� �� ��: MOTO_RoudToStep
*	����˵��: Ȧ������Ϊ������28BYJ48 �������Ƕ� = 5.625/64��.
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
uint32_t MOTO_RoudToStep(void)
{
	uint32_t steps;
	
	/* 28BYJ48 �������Ƕ� = 5.625/64��. 
		һȦ 360�ȣ�
		step = 360 / (5.625 / 64)
	*/	
	steps = (360 * 64 * 1000 / 4) / 5625;		// 4096��
	
	return steps;
}


/*
*********************************************************************************************************
*	�� �� ��: MOTO_ISR
*	����˵��: �жϷ������
*	��    ��: ��
*	�� �� ֵ: ��
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
	if (g_tMoto.Dir == 0)	/* ��ת */
	{
		if (++g_tMoto.Pos > n)
		{
			g_tMoto.Pos = 0;
		}
	}
	else	/* ��ת */
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
			TIM_ClearITPendingBit(TIM6, TIM_IT_Update);		/* ����жϱ�־λ */
			
			MOTO_Pause();	/* ��ͣ */
		}
	}	
}

/*
*********************************************************************************************************
*	�� �� ��: TIM6_IRQHandler
*	����˵��: �ⲿ�жϷ������.
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
#ifndef TIM6_ISR_MOVE_OUT		/* bsp.h �ж�����У���ʾ�������Ƶ� stam32fxxx_it.c�� �����ظ����� */
void TIM6_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM6, TIM_IT_Update) != RESET)
	{
		MOTO_ISR();	/* �жϷ������ */

		TIM_ClearITPendingBit(TIM6, TIM_IT_Update);		/* ����жϱ�־λ */
	}
}
#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
