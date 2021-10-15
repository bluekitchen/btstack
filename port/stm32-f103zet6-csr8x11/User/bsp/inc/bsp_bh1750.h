/*
*********************************************************************************************************
*
*	ģ������ : ���նȴ�����BH1750FVI����ģ��
*	�ļ����� : bsp_bh1750.h
*	��    �� : V1.0
*	˵    �� : ͷ�ļ�
*
*	�޸ļ�¼ :
*		�汾��  ����       ����    ˵��
*		v1.0    2012-10-12 armfly  ST�̼���汾 V2.1.0
*
*	Copyright (C), 2012-2013, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_BH1750_H
#define _BSP_BH1750_H

#define BH1750_SLAVE_ADDRESS    0x46		/* I2C�ӻ���ַ */

/* ������ Opercode ���� */
enum
{
	BHOP_POWER_DOWN = 0x00,		/* �������ģʽ��оƬ�ϵ��ȱʡ����PowerDownģʽ */
	BHOP_POWER_ON = 0x01,		/* �ϵ磬�ȴ��������� */
	BHOP_RESET = 0x07,			/* �������ݼĴ��� (Power Down ģʽ��Ч) */
	BHOP_CON_H_RES  = 0x10,		/* �����߷ֱ��ʲ���ģʽ  ������ʱ�� 120ms�� ����� 180ms��*/
	BHOP_CON_H_RES2 = 0x11,		/* �����߷ֱ��ʲ���ģʽ2 ������ʱ�� 120ms��*/
	BHOP_CON_L_RES = 0x13,		/* �����ͷֱ��ʲ���ģʽ ������ʱ�� 16ms��*/

	BHOP_ONE_H_RES  = 0x20,		/* ���θ߷ֱ��ʲ���ģʽ , ֮���Զ�����Power Down */
	BHOP_ONE_H_RES2 = 0x21,		/* ���θ߷ֱ��ʲ���ģʽ2 , ֮���Զ�����Power Down  */
	BHOP_ONE_L_RES = 0x23,		/* ���εͷֱ��ʲ���ģʽ , ֮���Զ�����Power Down  */
};

void bsp_InitBH1750(void);
void BH1750_WriteCmd(uint8_t _ucOpecode);
uint16_t BH1750_ReadData(void);
void BH1750_AdjustSensitivity(uint8_t _ucMTReg);
void BH1750_ChageMode(uint8_t _ucMode);
float BH1750_GetLux(void);

#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
