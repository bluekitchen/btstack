/*
*********************************************************************************************************
*
*	模块名称 :  RA8875芯片外挂的串行Flash驱动模块
*	文件名称 : bsp_ra8875_flash.h
*	版    本 : V1.1
*	说    明 : 头文件
*
*	Copyright (C), 2014-2015, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/


#ifndef _BSP_RA8875_FLASH_H
#define _BSP_RA8875_FLASH_H

/* 定义串行Flash ID */
enum
{
	SST25VF016B = 0xBF2541,
	MX25L1606E  = 0xC22015,
	W25Q64BV    = 0xEF4017,
	W25Q128		= 0xEF4018
};

typedef struct
{
	uint32_t ChipID;		/* 芯片ID */
	uint32_t TotalSize;		/* 总容量 */
	uint16_t PageSize;		/* 页面大小 */
}W25_T;

enum
{
	FONT_CHIP = 0,			/* 字库芯片 */
	BMP_CHIP  = 1			/* 图库芯片 */
};

/*
	对于1片 W25Q128的显示器
*/
#define PIC_OFFSET	(2*1024*1024)	/* 图片数据区起始地址 */

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

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
