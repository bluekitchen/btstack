/*
*********************************************************************************************************
*
*	ģ������ :  RA8875оƬ��ҵĴ���Flash����ģ��
*	�ļ����� : bsp_ra8875_flash.h
*	��    �� : V1.1
*	˵    �� : ͷ�ļ�
*
*	Copyright (C), 2014-2015, ���������� www.armfly.com
*
*********************************************************************************************************
*/


#ifndef _BSP_RA8875_FLASH_H
#define _BSP_RA8875_FLASH_H

/* ���崮��Flash ID */
enum
{
	SST25VF016B = 0xBF2541,
	MX25L1606E  = 0xC22015,
	W25Q64BV    = 0xEF4017,
	W25Q128		= 0xEF4018
};

typedef struct
{
	uint32_t ChipID;		/* оƬID */
	uint32_t TotalSize;		/* ������ */
	uint16_t PageSize;		/* ҳ���С */
}W25_T;

enum
{
	FONT_CHIP = 0,			/* �ֿ�оƬ */
	BMP_CHIP  = 1			/* ͼ��оƬ */
};

/*
	����1Ƭ W25Q128����ʾ��
*/
#define PIC_OFFSET	(2*1024*1024)	/* ͼƬ��������ʼ��ַ */

void bsp_InitRA8875Flash(void);

void w25_CtrlByMCU(void);
void w25_CtrlByRA8875(void);
void w25_SelectChip(uint8_t _idex);

uint32_t w25_ReadID(void);
void w25_EraseChip(void);
void w25_EraseSector(uint32_t _uiSectorAddr);
void s25_WritePage(uint8_t * _pBuf, uint32_t _uiWriteAddr, uint16_t _usSize);
void w25_WritePage(uint8_t * _pBuf, uint32_t _uiWriteAddr, uint16_t _usSize);
void w25_ReadBuffer(uint8_t * _pBuf, uint32_t _uiReadAddr, uint32_t _uiSize);

extern W25_T g_tW25;

#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
