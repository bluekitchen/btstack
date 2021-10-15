/*
*********************************************************************************************************
*
*	模块名称 : VS1053B mp3解码器模块
*	文件名称 : bsp_vs1053b.h
*	说    明 : VS1053B芯片底层驱动。
*
*	Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_VS1053B_H
#define __BSP_VS1053B_H

/* 芯片版本，也就是芯片型号识别 */
enum
{
	VS1001 = 0,
	VS1011 = 1,
	VS1002 = 2,
	VS1003 = 3,
	VS1053 = 4,
	VS1033 = 5,
	VS1103 = 7
};

#define VS_WRITE_COMMAND 	0x02
#define VS_READ_COMMAND 	0x03

/* 寄存器定义 */
#define SCI_MODE        	0x00
#define SCI_STATUS      	0x01
#define SCI_BASS        	0x02
#define SCI_CLOCKF      	0x03
#define SCI_DECODE_TIME 	0x04
#define SCI_AUDATA      	0x05
#define SCI_WRAM        	0x06
#define SCI_WRAMADDR    	0x07
#define SCI_HDAT0       	0x08
#define SCI_HDAT1       	0x09

#define SCI_AIADDR      	0x0a
#define SCI_VOL         	0x0b
#define SCI_AICTRL0     	0x0c
#define SCI_AICTRL1     	0x0d
#define SCI_AICTRL2     	0x0e
#define SCI_AICTRL3     	0x0f
#define SM_DIFF         	0x01
#define SM_JUMP         	0x02
#define SM_RESET        	0x04
#define SM_OUTOFWAV     	0x08
#define SM_PDOWN        	0x10
#define SM_TESTS        	0x20
#define SM_STREAM       	0x40
#define SM_PLUSV        	0x80
#define SM_DACT         	0x100
#define SM_SDIORD       	0x200
#define SM_SDISHARE     	0x400
#define SM_SDINEW       	0x800
#define SM_ADPCM        	0x1000
#define SM_ADPCM_HP     	0x2000

/* 用于音量调节函数形参的宏，已取254的模 */
#define VS_VOL_MUTE			0    /* 静音 */
#define VS_VOL_MAX			254  /* 最大 */
#define VS_VOL_MIN			0    /* 最小 */

void vs1053_Init(void);
uint8_t vs1053_TestRam(void);
void vs1053_TestSine(void);
void vs1053_SoftReset(void);
void ResetDecodeTime(void);

uint8_t vs1053_ReqNewData(void);
void vs1053_WriteData(uint8_t _ucData);

void vs1053_SetVolume(uint8_t _ucVol);
uint8_t vs1053_ReqNewData(void);

void bsp_CfgSPIForVS1053B(void);
uint8_t vs1053_ReadChipID(void);
uint8_t vs1053_WaitTimeOut(void);

void vs1053_SetBASS(int8_t _cHighAmp, uint16_t _usHighFreqCut, uint8_t _ucLowAmp, uint16_t _usLowFreqCut);

#endif

















