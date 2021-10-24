/*
*********************************************************************************************************
*
*	ģ������ : ����ң�ؽ���������ģ��
*	�ļ����� : bsp_ir_decode.c
*	��    �� : V1.0
*	˵    �� : ����ң�ؽ��յĺ����ź�����CPU�� PB0/TIM3_CH3.  ����������ʹ��TIM3_CH3ͨ�������벶������
*				Э�����롣 
*				TIM3��֧��˫���ش���, �����Ҫ���ж����л����ԡ�
*				��TIM6��TIM7֮��Ķ�ʱ����ֻ�ܲ��������ػ����½��ز�׽�����ܲ���˫���ز�׽.
*
*	�޸ļ�¼ :
*		�汾��  ����         ����     ˵��
*		V1.0    2015-07-11   armfly  ��ʽ����
*
*	Copyright (C), 2015-2016, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"

#define IR_REPEAT_SEND_EN		1	/* ����ʹ�� */
#define IR_REPEAT_FILTER		10	/* ң����108ms ��������������, ��������1��������ط� */

/* ����GPIO�˿� */
#define RCC_IRD		RCC_APB2Periph_GPIOB
#define PORT_IRD	GPIOB
#define PIN_IRD		GPIO_Pin_0

IRD_T g_tIR;

/*
*********************************************************************************************************
*	�� �� ��: IRD_StartWork
*	����˵��: ����TIM����ʼ����
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void IRD_StartWork(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	/* TIM3 clock enable */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

	/* GPIOB clock enable */
	RCC_APB2PeriphClockCmd(RCC_IRD, ENABLE);

	/* TIM3 chennel3 configuration : PB.0 */
	/* ����Ϊ�������� */
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;	/* ����ģʽ */
	GPIO_InitStructure.GPIO_Pin = PIN_IRD;
	GPIO_Init(PORT_IRD, &GPIO_InitStructure);	
	
	/* Enable the TIM3 global Interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	/* ���ö�ʱ��, ����TIM3_CH3�����жϣ��� TIM3����ж� */
	{
		TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
		TIM_ICInitTypeDef  TIM_ICInitStructure;
		uint16_t PrescalerValue;
		
		/* ���÷�ƵΪ, ���������ֵ�ĵ�λ������ 10us, ��������Ƚϡ� */
		PrescalerValue = 72000000/100000 - 1;
		/* Time base configuration */
		TIM_TimeBaseStructure.TIM_Period = 65535;
		TIM_TimeBaseStructure.TIM_Prescaler = PrescalerValue;
		TIM_TimeBaseStructure.TIM_ClockDivision = 0;
		TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
		TIM_TimeBaseStructure.TIM_RepetitionCounter = 0x0000;
		TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
  
		TIM_ICInitStructure.TIM_Channel = TIM_Channel_3;
		TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Falling;	// ����TIM3��TIM_ICPolarity_BothEdge��������
		TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
		TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;			/* ÿ�����䶼����1�β����¼� */
		TIM_ICInitStructure.TIM_ICFilter = 0x0;	
		TIM_ICInit(TIM3, &TIM_ICInitStructure);
		
		/* TIM enable counter */
		TIM_Cmd(TIM3, ENABLE);

		/* Enable the CC3 Interrupt Request */
		TIM_ITConfig(TIM3, TIM_IT_CC3, ENABLE);
		
		TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);	/* ����ж�ʹ�ܣ����ڳ�ʱͬ������ */
	}
	
	g_tIR.LastCapture = 0;	
	g_tIR.Status = 0;
	g_tIR.WaitFallEdge = 1;	/* 0 ��ʾ�ȴ������أ�1��ʾ�ȴ��½��أ������л����벶���� */
}

/*
*********************************************************************************************************
*	�� �� ��: IRD_StopWork
*	����˵��: ֹͣ�������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void IRD_StopWork(void)
{
	TIM_Cmd(TIM3, DISABLE);
	
	TIM_ITConfig(TIM3, TIM_IT_CC3, DISABLE);	
}

/*
*********************************************************************************************************
*	�� �� ��: IRD_DecodeNec
*	����˵��: ����NEC�����ʽʵʱ����
*	��    ��: _width �����ȣ���λ 10us
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void IRD_DecodeNec(uint16_t _width)
{
	static uint16_t s_LowWidth;
	static uint8_t s_Byte;
	static uint8_t s_Bit;
	uint16_t TotalWitdh;
	
	/* NEC ��ʽ ��5�Σ�
		1��������  9ms�� + 4.5ms��
		2����8λ��ַ��  0=1.125ms  1=2.25ms    bit0�ȴ�
		3����8λ��ַ��  0=1.125ms  1=2.25ms
		4��8λ����      0=1.125ms  1=2.25ms
		5��8Ϊ���뷴��  0=1.125ms  1=2.25ms
	*/

loop1:	
	//bsp_LedToggle(1);		//for DEBUG �۲��Ƿ��ܹ������������ش��������ж�
	switch (g_tIR.Status)
	{
		case 0:			/* 929 �ȴ���������ź�  7ms - 11ms */
			if ((_width > 700) && (_width < 1100))
			{
				g_tIR.Status = 1;
				s_Byte = 0;
				s_Bit = 0;
			}
			else
			{
				static uint8_t sss = 0;
				
				if (sss == 0)
				{
					sss = 1;
				}
				else if (sss == 1)
				{
					sss = 2;
				}				
			}
			break;

		case 1:			/* 413 �ж���������ź�  3ms - 6ms */
			if ((_width > 313) && (_width < 600))	/* ������ 4.5ms */
			{
				g_tIR.Status = 2;
			}
			else if ((_width > 150) && (_width < 250))	/* 2.25ms */
			{
				#ifdef IR_REPEAT_SEND_EN				
					if (g_tIR.RepeatCount >= IR_REPEAT_FILTER)
					{
						bsp_PutKey(g_tIR.RxBuf[2] + IR_KEY_STRAT);	/* ������ */
					}
					else
					{
						g_tIR.RepeatCount++;
					}
				#endif
				g_tIR.Status = 0;	/* ��λ����״̬ */
			}
			else
			{
				/* �쳣���� */
				g_tIR.Status = 0;	/* ��λ����״̬ */
			}
			break;
		
		case 2:			/* �͵�ƽ�ڼ� 0.56ms */
			if ((_width > 10) && (_width < 100))
			{		
				g_tIR.Status = 3;
				s_LowWidth = _width;	/* ����͵�ƽ��� */
			}
			else	/* �쳣���� */
			{
				/* �쳣���� */
				g_tIR.Status = 0;	/* ��λ������״̬ */	
				goto loop1;		/* �����ж�ͬ���ź� */
			}
			break;

		case 3:			/* 85+25, 64+157 ��ʼ��������32bit */						
			TotalWitdh = s_LowWidth + _width;
			/* 0�Ŀ��Ϊ1.125ms��1�Ŀ��Ϊ2.25ms */				
			s_Byte >>= 1;
			if ((TotalWitdh > 92) && (TotalWitdh < 132))
			{
				;					/* bit = 0 */
			}
			else if ((TotalWitdh > 205) && (TotalWitdh < 245))
			{
				s_Byte += 0x80;		/* bit = 1 */
			}	
			else
			{
				/* �쳣���� */
				g_tIR.Status = 0;	/* ��λ������״̬ */	
				goto loop1;		/* �����ж�ͬ���ź� */
			}
			
			s_Bit++;
			if (s_Bit == 8)	/* ����8λ */
			{
				g_tIR.RxBuf[0] = s_Byte;
				s_Byte = 0;
			}
			else if (s_Bit == 16)	/* ����16λ */
			{
				g_tIR.RxBuf[1] = s_Byte;
				s_Byte = 0;
			}
			else if (s_Bit == 24)	/* ����24λ */
			{
				g_tIR.RxBuf[2] = s_Byte;
				s_Byte = 0;
			}
			else if (s_Bit == 32)	/* ����32λ */
			{
				g_tIR.RxBuf[3] = s_Byte;
								
				if (g_tIR.RxBuf[2] + g_tIR.RxBuf[3] == 255)	/* ���У�� */
				{
					bsp_PutKey(g_tIR.RxBuf[2] + IR_KEY_STRAT);	/* ����ֵ����KEY FIFO */
					
					g_tIR.RepeatCount = 0;	/* �ط������� */										
				}
				
				g_tIR.Status = 0;	/* �ȴ���һ����� */
				break;
			}
			g_tIR.Status = 2;	/* ������һ��bit */
			break;						
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TIM3_IRQHandler
*	����˵��: TIM3�жϷ������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TIM3_IRQHandler(void)
{
	uint16_t NowCapture;
	uint16_t Width;
	
	/* ����ж� */
	if (TIM_GetITStatus(TIM3, TIM_IT_Update))
	{
		TIM_ClearITPendingBit(TIM3, TIM_IT_Update);;
		
        /* TIM3 ������ԴƵ��10us, 655360us = 0.655ms; */
        if (g_tIR.TimeOut < 2)
        {
            if (++g_tIR.TimeOut == 2)
            {
                /* ǿ������Ϊ�½��ش��� */
				{
					TIM_ICInitTypeDef  TIM_ICInitStructure;
					
					TIM_ICInitStructure.TIM_Channel = TIM_Channel_3;
					TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Falling;	/* �ȴ��½��� */
					TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
					TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;			/* ÿ�����䶼����1�β����¼� */
					TIM_ICInitStructure.TIM_ICFilter = 0x0;	
					TIM_ICInit(TIM3, &TIM_ICInitStructure);	
					
					g_tIR.WaitFallEdge = 1;
				}
    
                g_tIR.Status = 0;	/* �ȴ���һ����� */
            }
        }
	}
	
	/* ����ͨ��3�����ж� */
	if (TIM_GetITStatus(TIM3, TIM_IT_CC3))
	{
		TIM_ClearITPendingBit(TIM3, TIM_IT_CC3);

		g_tIR.TimeOut = 0;  /* ���㳬ʱ������ */
		
		NowCapture = TIM_GetCapture3(TIM3);	/* ��ȡ����ļ�����ֵ��������ֵ��0-65535ѭ������ */

		/* 	�л�����ļ��� */
		if (g_tIR.WaitFallEdge == 0)
		{
			TIM_ICInitTypeDef  TIM_ICInitStructure;
			
			TIM_ICInitStructure.TIM_Channel = TIM_Channel_3;
			TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Falling;	/* �ȴ��½��� */
			TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
			TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;			/* ÿ�����䶼����1�β����¼� */
			TIM_ICInitStructure.TIM_ICFilter = 0x0;	
			TIM_ICInit(TIM3, &TIM_ICInitStructure);	
			
			g_tIR.WaitFallEdge = 1;
		}			
		else
		{
			TIM_ICInitTypeDef  TIM_ICInitStructure;
			
			TIM_ICInitStructure.TIM_Channel = TIM_Channel_3;
			TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;		/* �ȴ������� */
			TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
			TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;			/* ÿ�����䶼����1�β����¼� */
			TIM_ICInitStructure.TIM_ICFilter = 0x0;	
			TIM_ICInit(TIM3, &TIM_ICInitStructure);	
			
			g_tIR.WaitFallEdge = 0;
		}
		
		if (NowCapture >= g_tIR.LastCapture)
		{
			Width = NowCapture - g_tIR.LastCapture;
		}
		else if (NowCapture < g_tIR.LastCapture)	/* �������ִ���󲢷�ת */
		{
			Width = ((0xFFFF - g_tIR.LastCapture) + NowCapture);
		}			
		
		if ((g_tIR.Status == 0) && (g_tIR.LastCapture == 0))
		{
			g_tIR.LastCapture = NowCapture;
			return;
		}
				
		g_tIR.LastCapture = NowCapture;	/* ���浱ǰ�������������´μ����ֵ */
		
		IRD_DecodeNec(Width);		/* ���� */		
	}
}


/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
