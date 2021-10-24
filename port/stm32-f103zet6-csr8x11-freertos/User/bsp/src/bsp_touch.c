/*
*********************************************************************************************************
*
*	ģ������ : ����ʽ����������ģ��
*	�ļ����� : bsp_touch.c
*	��    �� : V1.7
*	˵    �� : ����TS2046оƬ �� RA8875���ô���
*	�޸ļ�¼ :
*		�汾��  ����        ����    ˵��
*       v1.0    2012-07-06 armfly  ST�̼���V3.5.0�汾��
*		v1.1    2012-10-22 armfly  ����4��У׼
*		v1.2    2012-11-07 armfly  ���4��У׼��XY������֧��bug
*		v1.3    2012-12-17 armfly  ����У׼����������ڲ���:�ȴ�ʱ��
*		V1.4    2013-07-26 armfly  ���� TOUCH_DataFilter() �˲��㷨
*		V1.5    2013-07-32 armfly  �޸�TOUCH_WaitRelease(),��������Ҫ����
*		V1.6    2014-10-20 armfly
*					(1) �޸� TOUCH_PutKey() ������ʵ�ִ������ĺ�����������̬�л�.
*					(2) param �ṹ����У׼ʱ��ǰ����Ļ������� TouchDirection
*					(3) ����3.5��Ĵ���оƬ���޸�SPI������ú�����
*					(4) ���ڴ���оƬTSC2046�ʹ���FLASH,NRF24L01,MP3��ģ�鹲��SPI���ߡ������Ҫ
*						�ڴ����жϺ������ж����߳�ͻ. ���Ӻ��� bsp_SpiBusBusy() ��æ.
*					(5) TSC2046�������ģ��SPI (���ģ�ⷽʽ����SPI�豸����)
*		V1.7    2015-01-02 armfly  �ƻ�������ɨ����1msһ���޸�Ϊ10msһ�Ρ�δ����
*				2015-04-21 armfly �޸� TOUCH_InitHard() ������GT811_InitHard() ִ�к�ֱ��return
*
*	Copyright (C), 2014-2015, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"
#include "param.h"	/* ���������洢ģ�� */

/*
��1��������STM32-V5������ + 3.5����ʾģ�飬 ��ʾģ���ϵĴ���оƬΪ TSC2046�������оƬ
	PI10   --> TP_CS
	PD3/LCD_BUSY   --> TP_BUSY
	PB3/SPI3_SCK   --> TP_SCK
	PB4/SPI3_MISO   --> TP_MISO
	PB5/SPI3_MOSI   --> TP_MOSI
	PI3/TP_INT   --> TP_PEN_INT

��2��������STM32������ + 4.3���7����ʾģ�飨����RA8875оƬ)
	RA8875���ô������ӿڣ����ֱ��ͨ��FSMC����RA8875��ؼĴ������ɡ�


	������δʹ�ô����жϹ��ܡ���1ms���ڵ� Systick��ʱ�жϷ�������жԴ���ADCֵ���в�����
	�˲����������ʰ��³���40ms�󣬿�ʼ�ɼ�ADCֵ��ÿ1ms�ɼ�1�Σ������ɼ�10�Σ���Ȼ���
	��10�������������򣬶���2ͷ�����������м�һ������������Ͳ�����ƽ��ֵ��

	����2��У׼������У׼�󱣴�2��У׼���ADCֵ��ʵ�ʹ���ʱ������2��ֱ�߷��̼�����Ļ���ꡣ
	У׼�����б���ӿڣ���������Ҫ������ʾ��δ�����湦�ܡ�
	��ҿ����Լ��޸�  TOUCH_SaveParam() �� TOUCH_LoadParam() ��������ʵ�ֱ��湦�ܡ�

*/
/* TSC2046 �ڲ�ADCͨ���� */
#define ADC_CH_X	1		/* Xͨ��������Xλ�� */
#define ADC_CH_Y	5		/* Yͨ��������Yλ�� */

/* ÿ1msɨ��һ������ */
#define DOWN_VALID		30	/* ����30ms ��, ��ʼͳ��ADC */
#define SAMPLE_COUNT	10	/* ���º������ɼ�40������ */

/*
������У׼�������Ļ�����Ľǵ�ƫ������
��1���� �� x1 = CALIB_OFFSET, y1 = CALIB_OFFSET
��2���� �� x2 = LCD_GetWidth() - CALIB_OFFSET, y2 = LCD_GetHeight() - CALIB_OFFSET
*/
#define CALIB_OFFSET	20
#define TP_X1	CALIB_OFFSET
#define TP_Y1	CALIB_OFFSET

#define TP_X2	(LCD_GetWidth() - CALIB_OFFSET)
#define TP_Y2	(LCD_GetHeight() - CALIB_OFFSET)

#define TP_X3	CALIB_OFFSET
#define TP_Y3	(LCD_GetHeight() - CALIB_OFFSET)

#define TP_X4	(LCD_GetWidth() - CALIB_OFFSET)
#define TP_Y4	CALIB_OFFSET

/* ��ЧADCֵ���ж�����. ̫�ӽ�ADC�ٽ�ֵ��������Ϊ��Ч */
#define ADC_VALID_OFFSET	2

//#define WaitTPReady() while(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_5))
#define WaitTPReady() {}

/* ����ģ���õ���ȫ�ֱ��� */
TOUCH_T g_tTP;

static uint8_t	TOUCH_PressValid(uint16_t _usX, uint16_t _usY);
static uint16_t TOUCH_DataFilter(uint16_t *_pBuf, uint8_t _ucCount);
static void TOUCH_LoadParam(void);
static void TOUCH_SaveParam(void);
static int32_t CalTwoPoint(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x);
static int16_t TOUCH_TransX(uint16_t _usAdcX, uint16_t _usAdcY);
static int16_t TOUCH_TransY(uint16_t _usAdcX, uint16_t _usAdcY);
int32_t TOUCH_Abs(int32_t x);

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitTouch
*	����˵��: ����STM32�ʹ�����صĿ��ߣ�ʹ��Ӳ��SPI1, Ƭѡ���������
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TOUCH_InitHard(void)
{
	uint16_t i;
	
    g_tTP.Enable = 0;

	/* 50ms���ȴ�GT811��λ����������̽��GT811оƬ ID */
	for (i = 0; i < 5; i++)
	{
		if (i2c_CheckDevice(GT811_I2C_ADDR) == 0)
		{
			GT811_InitHard();
			return;		/* 2015-04-21 break �޸�Ϊ return */
		}
		bsp_DelayMS(10);
	}
		
	if (g_ChipID == IC_8875)
	{
		RA8875_TouchInit();

		g_tTP.usMaxAdc = 1023;	/* 10λADC */
	}
	else
	{	
		TSC2046_InitHard();

		g_tTP.usMaxAdc = 4095;	/* 12λADC */
	}

	TOUCH_LoadParam();	/* ��ȡУ׼���� */

    g_tTP.Write = g_tTP.Read = 0;

	g_tTP.Enable = 1;
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_ReadAdcX
*	����˵��: ��ô�����X����ADC����ֵ�� �ѽ����˲�����
*	��    ��:  ��
*	�� �� ֵ: X ����ADCֵ
*********************************************************************************************************
*/
uint16_t TOUCH_ReadAdcX(void)
{
	uint16_t usAdc;

	__set_PRIMASK(1);  		/* ���ж� */
	usAdc = g_tTP.usAdcNowX;
	__set_PRIMASK(0);  		/* ���ж� */

	return usAdc;
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_ReadAdcY
*	����˵��: ��ô�����Y����ADC����ֵ�� �ѽ����˲�����
*	��    ��:  ��
*	�� �� ֵ: Y ����ֵ������ֵ
*********************************************************************************************************
*/
uint16_t TOUCH_ReadAdcY(void)
{
	uint16_t usAdc;

	__set_PRIMASK(1);  		/* ���ж� */
	usAdc = g_tTP.usAdcNowY;
	__set_PRIMASK(0);  		/* ���ж� */

	return usAdc;
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_PutKey
*	����˵��: ��1������������ֵѹ�봥��FIFO�����������ڵ��败������
*	��    ��: _usX, _usY ����ֵ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TOUCH_PutKey(uint8_t _ucEvent, uint16_t _usX, uint16_t _usY)
{
	uint16_t xx, yy;
	uint16_t x = 0, y = 0;

	g_tTP.Event[g_tTP.Write] = _ucEvent;

	if (g_GT811.Enable == 1)	/* ������ (����У׼) */
	{
		xx = _usX;
		yy = _usY;
	}
	else	/* ������ */
	{
		xx = TOUCH_TransX(_usX, _usY);
		yy = TOUCH_TransY(_usX, _usY);
	}
	
	/* ��������������ʶ�� */
	switch (g_tParam.TouchDirection)
	{
		case 0:	/* У׼����ʱ����Ļ����Ϊ0 */
			if (g_LcdDirection == 0)		/* ���� */
			{
				x = xx;
				y = yy;
			}
			else if (g_LcdDirection == 1)	/* ����180��*/
			{
				x = g_LcdWidth - xx - 1;
				y = g_LcdHeight - yy - 1;
			}
			else if (g_LcdDirection == 2)	/* ���� */
			{
				y = xx;
				x = g_LcdWidth - yy - 1;
			}
			else if (g_LcdDirection == 3)	/* ����180�� */
			{
				y = g_LcdHeight - xx - 1;
				x = yy;
			}
			break;

		case 1:	/* У׼����ʱ����Ļ����Ϊ1 */
			if (g_LcdDirection == 0)		/* ���� */
			{
				x = g_LcdWidth - xx - 1;
				y = g_LcdHeight - yy - 1;
			}
			else if (g_LcdDirection == 1)	/* ����180��*/
			{
				x = xx;
				y = yy;
			}
			else if (g_LcdDirection == 2)	/* ���� */
			{
				y = g_LcdHeight - xx - 1;
				x = yy;
			}
			else if (g_LcdDirection == 3)	/* ����180�� */
			{
				y = xx;
				x = g_LcdWidth - yy - 1;
			}
			break;

		case 2:	/* У׼����ʱ����Ļ����Ϊ2 */
			if (g_LcdDirection == 0)		/* ���� */
			{
				y = xx;
				x = g_LcdWidth - yy - 1;
			}
			else if (g_LcdDirection == 1)	/* ����180��*/
			{
				y = g_LcdHeight - xx - 1;
				x = yy;
			}
			else if (g_LcdDirection == 2)	/* ���� */
			{
				x = xx;
				y = yy;
			}
			else if (g_LcdDirection == 3)	/* ����180�� */
			{
				x = g_LcdWidth - xx - 1;
				y = g_LcdHeight - yy - 1;
			}
			break;

		case 3:	/* У׼����ʱ����Ļ����Ϊ3 */
			if (g_LcdDirection == 0)		/* ���� */
			{
				y = xx;
				x = g_LcdWidth - yy - 1;
			}
			else if (g_LcdDirection == 1)	/* ����180��*/
			{
				y = g_LcdHeight - xx - 1;
				x = yy;
			}
			else if (g_LcdDirection == 2)	/* ���� */
			{
				x = g_LcdWidth - xx - 1;
				y = g_LcdHeight - yy - 1;
			}
			else if (g_LcdDirection == 3)	/* ����180�� */
			{
				x = xx;
				y = yy;
			}
			break;

		default:
			g_tParam.TouchDirection = 0;	/* ���������Чʱ������Ϊȱʡ�ĺ��� */
			break;
	}

	g_tTP.XBuf[g_tTP.Write] = x;
	g_tTP.YBuf[g_tTP.Write] = y;

	if (++g_tTP.Write  >= TOUCH_FIFO_SIZE)
	{
		g_tTP.Write = 0;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_GetKey
*	����˵��: �Ӵ���FIFO��������ȡһ������ֵ��
*	��    ��:  ��
*	�� �� ֵ:
*			TOUCH_NONE      ��ʾ���¼�
*			TOUCH_DOWN      ����
*			TOUCH_MOVE      �ƶ�
*			TOUCH_RELEASE	�ͷ�
*********************************************************************************************************
*/
uint8_t TOUCH_GetKey(int16_t *_pX, int16_t *_pY)
{
	uint8_t ret;

	if (g_tTP.Read == g_tTP.Write)
	{
		return TOUCH_NONE;
	}
	else
	{
		ret = g_tTP.Event[g_tTP.Read];
		*_pX = g_tTP.XBuf[g_tTP.Read];
		*_pY = g_tTP.YBuf[g_tTP.Read];

		if (++g_tTP.Read >= TOUCH_FIFO_SIZE)
		{
			g_tTP.Read = 0;
		}
		return ret;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_CelarFIFO
*	����˵��: �������FIFO������
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TOUCH_CelarFIFO(void)
{
	__set_PRIMASK(1);  		/* ���ж� */
	g_tTP.Write = g_tTP.Read;
	__set_PRIMASK(0);  		/* ���ж� */
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_InRect
*	����˵��: �жϵ�ǰ�����Ƿ�λ�ھ��ο���
*	��    ��:  _usX, _usY: ��������
*			_usRectX,_usRectY: �������
*			_usRectH��_usRectW : ���θ߶ȺͿ��
*	�� �� ֵ: 1 ��ʾ�ڷ�Χ��
*********************************************************************************************************
*/
uint8_t TOUCH_InRect(uint16_t _usX, uint16_t _usY,
	uint16_t _usRectX, uint16_t _usRectY, uint16_t _usRectH, uint16_t _usRectW)
{
	if ((_usX > _usRectX) && (_usX < _usRectX + _usRectW)
		&& (_usY > _usRectY) && (_usY < _usRectY + _usRectH))
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
*	�� �� ��: TOUCH_MoveValid
*	����˵��: �жϵ�ǰ������ϴ������Ƿ�ƫ��̫��
*	��    ��:  _usX1, _usY1: ����1
*			  _usX2, _usY2: ����2
*	�� �� ֵ: 1 ��ʾ��Ч�㣬 0 ��ʾ�ɵ�
*********************************************************************************************************
*/
uint8_t TOUCH_MoveValid(uint16_t _usX1, uint16_t _usY1, uint16_t _usX2, uint16_t _usY2)
{
	int16_t iX, iY;
	static uint8_t s_invalid_count = 0;

	iX = TOUCH_Abs(_usX1 - _usX2);
	iY = TOUCH_Abs(_usY1 - _usY2);

	if ((iX < 25) && (iY < 25))
	{
		s_invalid_count = 0;
		return 1;
	}
	else
	{
		if (++s_invalid_count >= 3)
		{
			s_invalid_count = 0;
			return 1;
		}
		return 0;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_Scan
*	����˵��: �������¼������򡣸ú����������Ե��ã�ÿms����1��. �� bsp_Timer.c
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TOUCH_Scan(void)
{
	uint16_t usAdcX;
	uint16_t usAdcY;
	static uint16_t s_usXBuf[SAMPLE_COUNT];
	static uint16_t s_usYBuf[SAMPLE_COUNT];
	static uint8_t s_ucPos = 0;
	static uint8_t s_count = 0;
	static uint8_t s_down = 0;
	static uint16_t s_usSaveAdcX, s_usSaveAdcY; /* ���ڴ���̧���¼������水�º��ƶ���������ֵ */

	if (g_tTP.Enable == 0)
	{
		return;
	}

	/* ���ԭʼ��ADCֵ��δ�˲� */
	if (g_ChipID == IC_8875)
	{
		/* ������������ڷ���RA8875,�������δ����ɼ�������Ӱ����ʾ */
		if (RA8875_IsBusy())
		{
			return;
		}

		usAdcX = RA8875_TouchReadX();
		usAdcY = RA8875_TouchReadY();
	}
	else
	{
		if (TSC2046_PenInt())
		{
			/* ������������ڷ�������SPI���裬�������δ����ɼ� */
			if (bsp_SpiBusBusy())
			{
				return;
			}

			usAdcX = TSC2046_ReadAdc(ADC_CH_X);
			usAdcY = TSC2046_ReadAdc(ADC_CH_Y);
		}
		else
		{
			usAdcX = 0;
			usAdcY = 0;
		}
	}

	if (TOUCH_PressValid(usAdcX, usAdcY))
	{
		/* ��ѹ30ms֮��ſ�ʼ�ɼ����� */
		if (s_count >= DOWN_VALID)
		{
			s_usXBuf[s_ucPos] = usAdcX;
			s_usYBuf[s_ucPos] = usAdcY;

			/* �ɼ�30ms���ݽ����˲� */
			if (++s_ucPos >= SAMPLE_COUNT)
			{
				s_ucPos = 0;

			   	/* ��ADC����ֵ��������˲� */
				g_tTP.usAdcNowX = TOUCH_DataFilter(s_usXBuf, SAMPLE_COUNT);
				g_tTP.usAdcNowY = TOUCH_DataFilter(s_usYBuf, SAMPLE_COUNT);

				if (s_down == 0)
				{
					s_down = 1;
					/* ���������¼� */
					TOUCH_PutKey(TOUCH_DOWN, g_tTP.usAdcNowX, g_tTP.usAdcNowY);

					s_usSaveAdcX = g_tTP.usAdcNowX;
					s_usSaveAdcY = g_tTP.usAdcNowY;
				}
				else
				{

					if (TOUCH_MoveValid(s_usSaveAdcX, s_usSaveAdcY, g_tTP.usAdcNowX, g_tTP.usAdcNowY))
					{
						/* �����ƶ��¼� */
						TOUCH_PutKey(TOUCH_MOVE, g_tTP.usAdcNowX, g_tTP.usAdcNowY);

						s_usSaveAdcX = g_tTP.usAdcNowX;
						s_usSaveAdcY = g_tTP.usAdcNowY;
					}
					else
					{
						g_tTP.usAdcNowX = 0; /* for debug stop */
					}
				}
			}
		}
		else
		{
			s_count++;
		}
	}
	else
	{
		if (s_count > 0)
		{
			if (--s_count == 0)
			{
				/* �����ͷ��¼� */
				//TOUCH_PutKey(TOUCH_RELEASE, g_tTP.usAdcNowX, g_tTP.usAdcNowY);
				TOUCH_PutKey(TOUCH_RELEASE, s_usSaveAdcX, s_usSaveAdcY);

				g_tTP.usAdcNowX = 0;
				g_tTP.usAdcNowY = 0;

				s_count = 0;
				s_down = 0;
			}
		}
		s_ucPos = 0;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: CalTwoPoint
*	����˵��: ����2��ֱ�߷��̣�����Yֵ
*	��    ��:  2����������x������
*	�� �� ֵ: x��Ӧ��yֵ
*********************************************************************************************************
*/
static int32_t CalTwoPoint(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x)
{
	return y1 + ((int32_t)(y2 - y1) * (x - x1)) / (x2 - x1);
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_TransX
*	����˵��: ������ADCֵת��Ϊ��������
*	��    ��:  ��
*	�� �� ֵ: X ����ֵ������ֵ
*********************************************************************************************************
*/
static int16_t TOUCH_TransX(uint16_t _usAdcX, uint16_t _usAdcY)
{
#if CALIB_POINT_COUNT == 2	/* 2��У׼ */
	uint16_t x;
	int32_t y;

	if (g_tTP.XYChange == 0)
	{
		x = _usAdcX;
		if (x == 0)
		{
			y = 0;
		}
		else
		{
			//y = CalTwoPoint(g_tTP.usAdcX1, TP_X1, g_tTP.usAdcX2, TP_X2, x);
			y = CalTwoPoint(g_tTP.usAdcX1, g_tTP.usLcdX1, g_tTP.usAdcX2, g_tTP.usLcdX2, x);
		}
	}
	else
	{
		x = _usAdcY;
		if (x == 0)
		{
			y = 0;
		}
		else
		{
			//y = CalTwoPoint(g_tTP.usAdcY1, TP_X1, g_tTP.usAdcY2, TP_X2, x);
			y = CalTwoPoint(g_tTP.usAdcY1, g_tTP.usLcdX1, g_tTP.usAdcY2, g_tTP.usLcdX2, x);
		}
	}
	return y;
#else		/* 4��У׼ */
	uint16_t x, x1, x2;
	int32_t y;

	if (g_tTP.XYChange == 0)	/* X Y ���겻���� */
	{
		x = _usAdcX;

		/* ���� Y ADC ʵʱ����ֱ�߷��̵Ĳο���x1, x2
			if  _usAdcY = usAdcY1 then  ȡ�� = (AdcX1, TP_X1, AdcX4, TP_X4, _usAdcY)
			if  _usAdcY = usAdcY2 then  ȡ�� = (AdcX3, TP_X3, AdcX2, TP_X2, _usAdcY)

			���� TP_X1 = TP_X3;  TP_X4 = TP_X1 , ���ǳ����趨��У׼λ�õ���������, �ǹ̶��ġ�
			���ǽ���Ҫ��̬����Ե�1���͵�3��������ͬ������2��ֱ�߷��̼��㡣
		*/
		x1 = CalTwoPoint(g_tTP.usAdcY1, g_tTP.usAdcX1, g_tTP.usAdcY2,  g_tTP.usAdcX3, _usAdcY);
		x2 = CalTwoPoint(g_tTP.usAdcY1, g_tTP.usAdcX4, g_tTP.usAdcY2,  g_tTP.usAdcX2, _usAdcY);
	}
	else						/* X Y ���꽻�� */
	{
		x = _usAdcY;

		/* ���� X ADC ʵʱ����ֱ�߷��̵Ĳο���x1, x2
			if  _usAdcX = usAdcX1 then  ȡ�� = (AdcY1, TP_X1, AdcY4, TP_X4, _usAdcX)
			if  _usAdcX = usAdcX2 then  ȡ�� = (AdcY3, TP_X3, AdcY2, TP_X2, _usAdcX)

			���� TP_X1 = TP_X3;  TP_X4 = TP_X1 , ���ǳ����趨��У׼λ�õ���������, �ǹ̶��ġ�
			���ǽ���Ҫ��̬����Ե�1���͵�3��������ͬ������2��ֱ�߷��̼��㡣
		*/
		x1 = CalTwoPoint(g_tTP.usAdcX1, g_tTP.usAdcY1, g_tTP.usAdcX2,  g_tTP.usAdcY3, _usAdcX);
		x2 = CalTwoPoint(g_tTP.usAdcX1, g_tTP.usAdcY4, g_tTP.usAdcX2,  g_tTP.usAdcY2, _usAdcX);
	}

	if (x == 0)
	{
		y = 0;
	}
	else
	{
		/* ����2��ֱ�߷��̣��������� */
		//y = CalTwoPoint(x1, TP_X1, x2, TP_X2, x);
		CalTwoPoint(x1, g_tTP.usLcdX1, x2, g_tTP.usLcd2, x);
	}
	return y;
#endif
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_TransY
*	����˵��: ������ADCֵת��Ϊ��������
*	��    ��:  ��
*	�� �� ֵ: Y ����ֵ������ֵ
*********************************************************************************************************
*/
static int16_t TOUCH_TransY(uint16_t _usAdcX, uint16_t _usAdcY)
{
#if CALIB_POINT_COUNT == 2	/* 2��У׼ */
	int32_t x;
	int32_t y;

	if (g_tTP.XYChange == 0)
	{
		x = _usAdcY;
		if (x == 0)
		{
			y = 0;
		}
		else
		{
			//y = CalTwoPoint(g_tTP.usAdcY1, TP_Y1, g_tTP.usAdcY2, TP_Y2, x);
			y = CalTwoPoint(g_tTP.usAdcY1, g_tTP.usLcdY1, g_tTP.usAdcY2, g_tTP.usLcdY2, x);
		}
	}
	else
	{
		x = _usAdcX;
		if (x == 0)
		{
			y = 0;
		}
		else
		{
			//y = CalTwoPoint(g_tTP.usAdcX1, TP_Y1, g_tTP.usAdcX2, TP_Y2, x);
			y = CalTwoPoint(g_tTP.usAdcX1, g_tTP.usLcdY1, g_tTP.usAdcX2, g_tTP.usLcdY2, x);
		}
	}
	return y;
#else		/* 4��У׼ */
	int32_t x, x1, x2;
	int32_t y;

	if (g_tTP.XYChange == 0)	/* X Y ���겻���� */
	{
		x = _usAdcY;

		/* ���� X ADC ʵʱ����ֱ�߷��̵Ĳο���x1, x2
			if  _usAdcX = usAdcX1 then  ȡ�� = (AdcY1, TP_Y1, AdcY3, TP_Y3, _usAdcX)
			if  _usAdcX = usAdcX2 then  ȡ�� = (AdcY4, TP_Y4, AdcY2, TP_Y2, _usAdcX)

			���� TP_Y1 = TP_Y4;  TP_Y3 = TP_Y2 , ���ǳ����趨��У׼λ�õ���������, �ǹ̶��ġ�
			���ǽ���Ҫ��̬����Ե�1���͵�3��������ͬ������2��ֱ�߷��̼��㡣
		*/
		x1 = CalTwoPoint(g_tTP.usAdcX1, g_tTP.usAdcY1, g_tTP.usAdcX2,  g_tTP.usAdcY4, _usAdcX);
		x2 = CalTwoPoint(g_tTP.usAdcX1, g_tTP.usAdcY3, g_tTP.usAdcX2,  g_tTP.usAdcY2, _usAdcX);
	}
	else						/* X Y ���꽻�� */
	{
		x = _usAdcX;

		/* ���� X ADC ʵʱ����ֱ�߷��̵Ĳο���x1, x2
			if  _usAdcY = usAdcY1 then  ȡ�� = (AdcX1, TP_Y1, AdcX3, TP_Y3, _usAdcY)
			if  _usAdcY = usAdcY2 then  ȡ�� = (AdcX4, TP_Y4, AdcX2, TP_Y2, _usAdcY)

			���� TP_Y1 = TP_Y3;  TP_Y4 = TP_Y2 , ���ǳ����趨��У׼λ�õ���������, �ǹ̶��ġ�
			���ǽ���Ҫ��̬����Ե�1���͵�3��������ͬ������2��ֱ�߷��̼��㡣
		*/
		x1 = CalTwoPoint(g_tTP.usAdcY1, g_tTP.usAdcX1, g_tTP.usAdcY2,  g_tTP.usAdcX4, _usAdcY);
		x2 = CalTwoPoint(g_tTP.usAdcY1, g_tTP.usAdcX3, g_tTP.usAdcY2,  g_tTP.usAdcX2, _usAdcY);
	}

	if (x == 0)
	{
		y = 0;
	}
	else
	{
		/* ����2��ֱ�߷��̣��������� */
		//y = CalTwoPoint(x1, TP_Y1, x2, TP_Y2, x);
		y = CalTwoPoint(x1, g_tTP.usLcdY1, x2, g_tTP.usLcdY2, x);
	}
	return y;
#endif
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_GetX
*	����˵��: ��õ�ǰ�ĵĴ�������X
*	��    ��:  ��
*	�� �� ֵ: X ����ֵ������ֵ
*********************************************************************************************************
*/
int16_t TOUCH_GetX(void)
{
	return TOUCH_TransX(TOUCH_ReadAdcX(), TOUCH_ReadAdcY());
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_GetY
*	����˵��: ��õ�ǰ�ĵĴ�������Y
*	��    ��:  ��
*	�� �� ֵ: Y ����ֵ������ֵ
*********************************************************************************************************
*/
int16_t TOUCH_GetY(void)
{
	return TOUCH_TransY(TOUCH_ReadAdcX(), TOUCH_ReadAdcY());
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_DataFilter
*	����˵��: �Բ������ݽ����˲�
*	��    ��:  ��
*	�� �� ֵ: X ����ֵ������ֵ
*********************************************************************************************************
*/
static uint16_t TOUCH_DataFilter(uint16_t *_pBuf, uint8_t _ucCount)
{
#if 0
	uint8_t i;
	uint32_t uiSum;

	uiSum = 0;
	for (i = 0; i < _ucCount; i++)
	{
		uiSum += _pBuf[i];
	}
	return uiSum / _ucCount;
#else
	uint8_t flag;
	uint8_t i;
	uint16_t usTemp;
	uint32_t uiSum;

	/* �������� */
    do
	{
		flag = 0;
		for (i = 0; i < _ucCount - 1; i++)
		{
			if (_pBuf[i] > _pBuf[i+1])
			{
				usTemp = _pBuf[i + 1];
				_pBuf[i+1] = _pBuf[i];
				_pBuf[i] = usTemp;
				flag = 1;
			}
		}
	}while(flag);

	uiSum = 0;
	for (i = 0; i < _ucCount / 3; i++)
	{
		uiSum += _pBuf[_ucCount / 3 + i];
	}
	usTemp = uiSum / (_ucCount / 3);
	return usTemp;
#endif
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_DispPoint1
*	����˵��: ��ʾ��1��У׼��
*	��    ��:  _ucIndex = 0 : ��ʾ��1���㣻 _ucIndex = 1 ��ʾ��2����;
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void TOUCH_DispPoint(uint8_t _ucIndex)
{
	FONT_T tFont16;			/* ����һ������ṹ���������������������� */

	/* ����������� */
	{
		tFont16.FontCode = FC_ST_16;	/* ������� 16���� */
		tFont16.FrontColor = CL_WHITE;		/* ������ɫ 0 �� 1 */
		tFont16.BackColor = CL_BLUE;	/* ���ֱ�����ɫ */
		tFont16.Space = 0;			/* ���ּ�࣬��λ = ���� */
	}

/*
	��1���� �� x1 = CALIB_OFFSET, y1 = CALIB_OFFSET
	��2���� �� x2 = LCD_GetHeight() - CALIB_OFFSET, y2 = LCD_GetWidth - CALIB_OFFSET
*/
	if (_ucIndex == 0)
	{
		LCD_ClrScr(CL_BLUE);  		/* ������������ɫ */

		/* ����Ļ���ػ���2�����ο�(���ڼ������Ե�����Ƿ�����) */
		LCD_DrawRect(0, 0, LCD_GetHeight(), LCD_GetWidth(), CL_WHITE);
		LCD_DrawRect(2, 2, LCD_GetHeight() - 4, LCD_GetWidth() - 4, CL_YELLOW);

		LCD_DispStr(50, 10, "У׼������", &tFont16);		/* ��(8,3)���괦��ʾһ������ */

		LCD_DrawCircle(TP_X1, TP_Y1, 6, CL_WHITE);
	}
	else if (_ucIndex == 1)
	{
		LCD_DrawCircle(TP_X1, TP_Y1, 6, CL_BLUE);			/* ������1���� */

		LCD_DrawCircle(TP_X2, TP_Y2, 6, CL_WHITE);
	}
	else if (_ucIndex == 2)
	{
		LCD_DrawCircle(TP_X2, TP_Y2, 6, CL_BLUE);			/* ������2���� */

		LCD_DrawCircle(TP_X3, TP_Y3, 6, CL_WHITE);
	}
	else
	{
		LCD_DrawCircle(TP_X3, TP_Y3, 6, CL_BLUE);			/* ������3���� */

		LCD_DrawCircle(TP_X4, TP_Y4, 6, CL_WHITE);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_PressValid
*	����˵��: �жϰ�ѹ�Ƿ���Ч������X, Y��ADCֵ���д����ж�
*	��    ��:  ��
*	�� �� ֵ: 1 ��ʾ��Ч�� 0 ��ʾ��Ч
*********************************************************************************************************
*/
static uint8_t	TOUCH_PressValid(uint16_t _usX, uint16_t _usY)
{
	if ((_usX <= ADC_VALID_OFFSET) || (_usY <= ADC_VALID_OFFSET)
		|| (_usX >= g_tTP.usMaxAdc - ADC_VALID_OFFSET)
		|| (_usY >= g_tTP.usMaxAdc - ADC_VALID_OFFSET))
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_WaitRelease
*	����˵��: �ȴ������ͷ�
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void TOUCH_WaitRelease(void)
{
	uint8_t usCount = 0;

	for (;;)
	{
		if (TOUCH_PressValid(TOUCH_ReadAdcX(), TOUCH_ReadAdcY()) == 0)
		{
			if (++usCount > 5)
			{
				break;
			}
		}
		else
		{
			usCount = 0;
		}
		bsp_DelayMS(10);
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_Abs
*	����˵��: �������ֵ
*	��    ��: x : �з�������
*	�� �� ֵ: ������
*********************************************************************************************************
*/
int32_t TOUCH_Abs(int32_t x)
{
	if (x >= 0)
	{
		return x;
	}
	else
	{
		return -x;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_Calibration
*	����˵��: ������У׼
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TOUCH_Calibration(void)
{
	uint16_t usAdcX;
	uint16_t usAdcY;
	uint8_t usCount;
	uint8_t i;
	uint32_t n;

	TOUCH_CelarFIFO();		/* �����Ч�Ĵ����¼� */

	for (i = 0; i < CALIB_POINT_COUNT; i++)
	{
		TOUCH_DispPoint(i);		/* ��ʾУ׼�� */

		TOUCH_WaitRelease(); 	/* �ȴ������ͷ� */

		usCount = 0;
		for (n = 0; n < 500; n++)
		{
			usAdcX = TOUCH_ReadAdcX();
			usAdcY = TOUCH_ReadAdcY();

			if (TOUCH_PressValid(usAdcX, usAdcY))
			{
				if (++usCount > 5)
				{
					/* ��ѹ��Ч, ����У׼��ADC����ֵ */
					if (i == 0)
					{
						g_tTP.usAdcX1 = usAdcX;
						g_tTP.usAdcY1 = usAdcY;
					}
					else if (i == 1)
					{
						g_tTP.usAdcX2 = usAdcX;
						g_tTP.usAdcY2 = usAdcY;
					}
					else if (i == 2)
					{
						g_tTP.usAdcX3 = usAdcX;
						g_tTP.usAdcY3 = usAdcY;
					}
					else
					{
						g_tTP.usAdcX4 = usAdcX;
						g_tTP.usAdcY4 = usAdcY;
					}
					break;
				}
			}
			else
			{
				usCount = 0;
			}
			bsp_DelayMS(10);
		}
		if (n == 500)
		{
			return;
		}
	}

	TOUCH_WaitRelease(); 	/* �ȴ������ͷ� */

	/* ʶ������ X, Y �� ��ʾ���� X��Y �Ƿ���Ҫ���� */
	g_tTP.XYChange = 0;		/* 1��ʾX Y��Ҫ���� */
	if (LCD_GetHeight() < LCD_GetWidth())
	{
		if (TOUCH_Abs(g_tTP.usAdcX1 - g_tTP.usAdcX2) < TOUCH_Abs(g_tTP.usAdcY1 - g_tTP.usAdcY2))
		{
			g_tTP.XYChange = 1;
		}
	}
	else
	{
		if (TOUCH_Abs(g_tTP.usAdcX1 - g_tTP.usAdcX2) > TOUCH_Abs(g_tTP.usAdcY1 - g_tTP.usAdcY2))
		{
			g_tTP.XYChange = 1;
		}
	}

	g_tTP.usLcdX1 = TP_X1;
	g_tTP.usLcdY1 = TP_Y1;
	g_tTP.usLcdX2 = TP_X2;
	g_tTP.usLcdY2 = TP_Y2;
	g_tTP.usLcdX3 = TP_X3;
	g_tTP.usLcdY3 = TP_Y3;
	g_tTP.usLcdX4 = TP_X3;
	g_tTP.usLcdY4 = TP_Y3;

	/* �����һ�������Խ�У׼����������Flash ����EEPROM */
	TOUCH_SaveParam();
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_SaveParam
*	����˵��: ����У׼����	s_usAdcX1 s_usAdcX2 s_usAdcY1 s_usAdcX2
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
extern void SaveParam(void);
static void TOUCH_SaveParam(void)
{
#if 1
	g_tParam.usAdcX1 = g_tTP.usAdcX1;
	g_tParam.usAdcY1 = g_tTP.usAdcY1;
	g_tParam.usAdcX2 = g_tTP.usAdcX2;
	g_tParam.usAdcY2 = g_tTP.usAdcY2;
	g_tParam.usAdcX3 = g_tTP.usAdcX3;
	g_tParam.usAdcY3 = g_tTP.usAdcY3;
	g_tParam.usAdcX4 = g_tTP.usAdcX4;
	g_tParam.usAdcY4 = g_tTP.usAdcY4;

	g_tParam.usLcdX1 = g_tTP.usLcdX1;
	g_tParam.usLcdY1 = g_tTP.usLcdY1;
	g_tParam.usLcdX2 = g_tTP.usLcdX2;
	g_tParam.usLcdY2 = g_tTP.usLcdY2;
	g_tParam.usLcdX3 = g_tTP.usLcdX3;
	g_tParam.usLcdY3 = g_tTP.usLcdY3;
	g_tParam.usLcdX4 = g_tTP.usLcdX4;
	g_tParam.usLcdY4 = g_tTP.usLcdY4;

	g_tParam.XYChange = g_tTP.XYChange;

	g_tParam.TouchDirection = g_LcdDirection;	/* 2014-09-11 �����Ļ����, ������Ļ��תʱ�����ٴ�У׼ */

	SaveParam();	/* ������д��Flash */
#endif
}

/*
*********************************************************************************************************
*	�� �� ��: TOUCH_LoadParam
*	����˵��: ��ȡУ׼����
*	��    ��:  ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
extern void LoadParam(void);
static void TOUCH_LoadParam(void)
{
#if 1
	LoadParam();	/* ��Flash�ж�ȡ���� */

	g_tTP.usAdcX1 = g_tParam.usAdcX1;
	g_tTP.usAdcY1 = g_tParam.usAdcY1;
	g_tTP.usAdcX2 = g_tParam.usAdcX2;
	g_tTP.usAdcY2 = g_tParam.usAdcY2;
	g_tTP.usAdcX3 = g_tParam.usAdcX3;
	g_tTP.usAdcY3 = g_tParam.usAdcY3;
	g_tTP.usAdcX4 = g_tParam.usAdcX4;
	g_tTP.usAdcY4 = g_tParam.usAdcY4;

	g_tTP.usLcdX1 = g_tParam.usLcdX1;
	g_tTP.usLcdY1 = g_tParam.usLcdY1;
	g_tTP.usLcdX2 = g_tParam.usLcdX2;
	g_tTP.usLcdY2 = g_tParam.usLcdY2;
	g_tTP.usLcdX3 = g_tParam.usLcdX3;
	g_tTP.usLcdY3 = g_tParam.usLcdY3;
	g_tTP.usLcdX4 = g_tParam.usLcdX4;
	g_tTP.usLcdY4 = g_tParam.usLcdY4;

	g_tTP.XYChange = g_tParam.XYChange;
#endif
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
