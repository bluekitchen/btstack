/*
*********************************************************************************************************
*
*	ģ������ : ����������ģ��
*	�ļ����� : bsp_beep.c
*	��    �� : V1.0
*	˵    �� : ����������.
*
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2014-10-20 armfly  ��ʽ����
*
*	Copyright (C), 2014-2015, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

//#define BEEP_HAVE_POWER		/* ������б�ʾ��Դ��������ֱ��ͨ��GPIO����, ����PWM */

#ifdef	BEEP_HAVE_POWER		/* ��Դ������ */

	/* PA8 */
	#define GPIO_RCC_BEEP   RCC_APB2Periph_GPIOA
	#define GPIO_PORT_BEEP	GPIOA
	#define GPIO_PIN_BEEP	GPIO_Pin_8

	#define BEEP_ENABLE()	GPIO_PORT_BEEP->BSRR = GPIO_PIN_BEEP		/* ʹ�ܷ��������� */
	#define BEEP_DISABLE()	GPIO_PORT_BEEP->BRR = GPIO_PIN_BEEP			/* ��ֹ���������� */
#else		/* ��Դ������ */
	/* PA8/TIM1_CH1 ---> TIM1_CH1 */

	/* 1500��ʾƵ��1.5KHz��5000��ʾ50.00%��ռ�ձ� */
	#define BEEP_ENABLE()	bsp_SetTIMOutPWM(GPIOA, GPIO_Pin_8, TIM1, 1, 1500, 5000);

	/* ��ֹ���������� */
	#define BEEP_DISABLE()	bsp_SetTIMOutPWM(GPIOA, GPIO_Pin_8, TIM1, 1, 1500, 0);
#endif

static BEEP_T g_tBeep;		/* ���������ȫ�ֽṹ����� */

/*
*********************************************************************************************************
*	�� �� ��: BEEP_InitHard
*	����˵��: ��ʼ��������Ӳ��
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void BEEP_InitHard(void)
{
#ifdef	BEEP_HAVE_POWER		/* ��Դ������ */
	GPIO_InitTypeDef GPIO_InitStructure;

	/* ��GPIO��ʱ�� */
	RCC_APB2PeriphClockCmd(GPIO_RCC_BEEP, ENABLE);

	BEEP_DISABLE();

	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	/* �������ģʽ */	
	GPIO_InitStructure.GPIO_Pin = GPIO_PIN_BEEP;
	GPIO_Init(GPIO_PORT_BEEP, &GPIO_InitStructure);
#endif
}

/*
*********************************************************************************************************
*	�� �� ��: BEEP_Start
*	����˵��: ������������
*	��    �Σ�_usBeepTime : ����ʱ�䣬��λ10ms; 0 ��ʾ������
*			  _usStopTime : ֹͣʱ�䣬��λ10ms; 0 ��ʾ��������
*			 _usCycle : ���д����� 0 ��ʾ��������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void BEEP_Start(uint16_t _usBeepTime, uint16_t _usStopTime, uint16_t _usCycle)
{
	if (_usBeepTime == 0)
	{
		return;
	}

	g_tBeep.usBeepTime = _usBeepTime;
	g_tBeep.usStopTime = _usStopTime;
	g_tBeep.usCycle = _usCycle;
	g_tBeep.usCount = 0;
	g_tBeep.usCycleCount = 0;
	g_tBeep.ucState = 0;
	g_tBeep.ucEnalbe = 1;	/* ������ȫ�ֲ�������ʹ�ܷ�����־ */

	BEEP_ENABLE();			/* ��ʼ���� */
}

/*
*********************************************************************************************************
*	�� �� ��: BEEP_Stop
*	����˵��: ֹͣ��������
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void BEEP_Stop(void)
{
	g_tBeep.ucEnalbe = 0;

	BEEP_DISABLE();	/* ����������Ʊ�־����ֹͣ����������ֹͣ�����ж����ֿ��� */
}

/*
*********************************************************************************************************
*	�� �� ��: BEEP_KeyTone
*	����˵��: ���Ͱ�����
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void BEEP_KeyTone(void)
{
	BEEP_Start(5, 1, 1);	/* ����50ms��ͣ10ms�� 1�� */
}

/*
*********************************************************************************************************
*	�� �� ��: BEEP_Pro
*	����˵��: ÿ��10ms����1�θú��������ڿ��Ʒ������������ú����� bsp_timer.c �б����á�
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void BEEP_Pro(void)
{
	if ((g_tBeep.ucEnalbe == 0) || (g_tBeep.usStopTime == 0))
	{
		return;
	}

	if (g_tBeep.ucState == 0)
	{
		if (g_tBeep.usStopTime > 0)	/* ��Ϸ��� */
		{
			if (++g_tBeep.usCount >= g_tBeep.usBeepTime)
			{
				BEEP_DISABLE();		/* ֹͣ���� */
				g_tBeep.usCount = 0;
				g_tBeep.ucState = 1;
			}
		}
		else
		{
			;	/* �����κδ����������� */
		}
	}
	else if (g_tBeep.ucState == 1)
	{
		if (++g_tBeep.usCount >= g_tBeep.usStopTime)
		{
			/* ��������ʱ��ֱ������stopֹͣΪֹ */
			if (g_tBeep.usCycle > 0)
			{
				if (++g_tBeep.usCycleCount >= g_tBeep.usCycle)
				{
					/* ѭ����������ֹͣ���� */
					g_tBeep.ucEnalbe = 0;
				}

				if (g_tBeep.ucEnalbe == 0)
				{
					g_tBeep.usStopTime = 0;
					return;
				}
			}

			g_tBeep.usCount = 0;
			g_tBeep.ucState = 0;

			BEEP_ENABLE();			/* ��ʼ���� */
		}
	}
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
