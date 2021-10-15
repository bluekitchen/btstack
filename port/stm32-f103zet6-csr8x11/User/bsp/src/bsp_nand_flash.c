/*
*********************************************************************************************************
*
*	模块名称 : NAND Flash驱动模块
*	文件名称 : bsp_nand.c
*	版    本 : V1.3

*	说    明 : 提供NAND Flash (HY27UF081G2A， 8bit 128K字节 大页)的底层接口函数。【安富莱原创】
*
*	修改记录 :
*		版本号  日期        作者     说明
*		V1.0    2013-02-01  armfly  正式发布
*		V1.1    2014-05-02  armfly  增加512MB型号: H27U4G8F2DTR,
*					(1) 增加 NAND_ReadONFI() 函数;
*					(2) 增加 NAND_ReadParamPage() 函数;
*					(3) 解决 NAND_IsFreeBlock(uint32_t _ulBlockNo) 函数的BUG。
*					(4) 解决 FSMC_NAND_PageCopyBack() 和 FSMC_NAND_PageCopyBackEx()， A18必须相同的BUG
*					(5) 修改 NAND_FindFreeBlock(), 增加形参，查找奇数或偶数块号
*					(6) 修改 打印坏块的函数，增加标识 1 表示已用块 0 表示空闲块
*					(7) NAND_MarkUsedBlock() 调用的地方给的形参不对。 ulPhyPageNo / NAND_BLOCK_SIZE
*		V1.2	2015-07-23   armfly  增加函数 uint8_t NAND_GetBlockInfo(void); 方便LCD显示。
*		V1.3	2015-08-04   armfly  增加对 H27U1G8F2BTR 芯片的支持。
*
*	Copyright (C), 2015-2016, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/
#include "bsp.h"

/* 定义调试打印语句，用于排错 */
#define printf_err	printf
//#define printf_err(...)

//#define printf_ok	printf
#define printf_ok(...)


#define WRITE_PAGE_VERIFY_EN	/* 写 page 数据时校验 */

/*
	如果在IAR或KEIL的编辑器中阅读，请将编辑器的字体设置为新宋体（9号/五号），缩进的TAB设置为4。
	否则，方框处出现不对齐的问题。

	【待完善的地方】
	（1）在操作NAND Flash时，如下语句是一个死循环。如果硬件出现异常，将导致软件死机
 		while( GPIO_ReadInputDataBit(GPIOG, GPIO_Pin_6) == 0 )

 	（2）没有增加ECC校验功能。ECC可以检查1个或2个bit错误，如果只有1个bit错误，则可以修复这个bit。如果
 		多余2个bit错误，则可能检测不到。

 	（3）正常写文件操作时，会导致重建LUT。目前，重建LUT的代码执行效率还不够高，有待完善。

	【硬件说明】
	安富莱开发板配置的NAND Flahs为海力士的HY27UF081G2A,  439板子配的为 H27U4G8F2DTR
	（1）NAND Flash的片选信号连接到CPU的FSMC_NCE2，这决定了NAND Flash的地址空间为 0x70000000（见CPU的数据
		手册的FSMC章节)
	（2）有FSMC总线上有多个总线设备（如TFT、SRAM、CH374T、NOR），因此必须确保其他总线设备的片选处于禁止
		状态，否则将出现总线冲突问题 （参见本文件初始化FSMC GPIO的函数）


	【NAND Flash 结构定义】
     备用区有16x4字节，每page 2048字节，每512字节一个扇区，每个扇区对应16自己的备用区：

	 每个PAGE的逻辑结构，前面512Bx4是主数据区，后面16Bx4是备用区
	┌──────┐┌──────┐┌──────┐┌──────┐┌──────┐┌──────┐┌──────┐┌──────┐
	│ Main area  ││ Main area  ││ Main area  ││Main area   ││ Spare area ││ Spare area ││ Spare area ││Spare area  │
	│            ││            ││            ││            ││            ││            ││            ││            │
	│   512B     ││    512B    ││    512B    ││    512B    ││    16B     ││     16B    ││     16B    ││    16B     │
	└──────┘└──────┘└──────┘└──────┘└──────┘└──────┘└──────┘└──────┘

	 每16B的备用区的逻辑结构如下:(三星推荐标准）
	┌───┐┌───┐┌──┐┌──┐┌──┐┌───┐┌───┐┌───┐┌──┐┌──┐┌──┐┌──┐┌───┐┌───┐┌───┐┌───┐┌───┐
	│  BI  ││RESER ││LSN0││LSN1││LSN2││RESER ││RESER ││RESER ││ECC0││ECC1││ECC2││ECC0││S-ECC1││S-ECC0││RESER ││RESER ││RESER │
	│      ││ VED  ││    ││    ││    ││ VED  ││ VED  ││ VED  ││    ││    ││    ││    ││      ││      ││ VED  ││ VED  ││ VED  │
	└───┘└───┘└──┘└──┘└──┘└───┘└───┘└───┘└──┘└──┘└──┘└──┘└───┘└───┘└───┘└───┘└───┘

	K9F1G08U0A 和 HY27UF081G2A 是兼容的。芯片出厂时，厂商保证芯片的第1个块是好块。如果是坏块，则在该块的第1个PAGE的第1个字节
	或者第2个PAGE（当第1个PAGE坏了无法标记为0xFF时）的第1个字节写入非0xFF值。坏块标记值是随机的，软件直接判断是否等于0xFF即可。

	注意：网上有些资料说NAND Flash厂商的默认做法是将坏块标记定在第1个PAGE的第6个字节处。这个说法是错误。坏块标记在第6个字节仅针对部分小扇区（512字节）的NAND Flash
	并不是所有的NAND Flash都是这个标准。大家在更换NAND Flash时，请仔细阅读芯片的数据手册。


	为了便于在NAND Flash 上移植Fat文件系统，我们对16B的备用区采用以下分配方案:
	┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌───┐┌───┐┌──┐┌──┐┌──┐┌──┐
	│ BI ││USED││LBN0││LBN1││ECC0││ECC1││ECC2││ECC3││ECC4││ECC5││S-ECC1││S-ECC0││RSVD││RSVD││RSVD││RSVD│
	│    ││    ││    ││    ││    ││    ││    ││    ││    ││    ││      ││      ││    ││    ││    ││    │
	└──┘└──┘└──┘└──┘└──┘└──┘└──┘└──┘└──┘└──┘└───┘└───┘└──┘└──┘└──┘└──┘
    - BI : 坏块标志(Bad Block Identifier)。每个BLOCK的第1个PAGE或者第2个PAGE的第1个字节指示该块是否坏块。0xFF表示好块，不是0xFF表示坏块。
    - USED : 该块使用标志。0xFF表示空闲块；0xF0表示已用块。
    - LBN0 LBN1 : 逻辑块号(Logic Block No) 。从0开始编码。只在每个BLOCK的第1个PAGE有效，其它PAGE该字段固定为0xFF FF
    - ECC0 ~ ECC6 : 512B主数据区的ECC校验 （按照三星提供ECC算法，256字节对应3个字节的ECC)
    - S-ECC1 S-ECC0 : LSN0和LSN2的ECC校验
    - RSVD : 保留字节，Reserved

	【坏块管理 & 磨损平衡】
	(1) 内部全局数组s_usLUT[]按次序保存物理块号。用于物理块和逻辑块的地址映射。
	(2) 格式化时，将98%的好块用于主数据存储。剩余的2%用于备用区（坏块替换）。
	(3) 写扇区（512B)时，如果扇区内容为空，则直接写入，减少不必要的块擦除操作。有效提高NAND Flash的寿命和读写性能。
	(4) 写扇区时，如果扇区内容不为空，则从末尾开始查找一个空闲块替换掉旧块，替换并改写数据完成后，将旧块擦除，并标注为空闲，之后重建LUT。
	(5) 块复制时，充分利用NAND Flash硬件的Copy-Back功能，无需读源页到内存再写入目标页。这样可显著提高读写效率。
	(6) 磨损平衡还存在缺陷，效果不好。ECC校验暂未实现。

*/

/* 定义NAND Flash的物理地址。这个是由硬件决定的 */
#define Bank2_NAND_ADDR    ((uint32_t)0x70000000)
#define Bank_NAND_ADDR     Bank2_NAND_ADDR

/* 定义操作NAND Flash用到3个宏 */
#define NAND_CMD_AREA		*(__IO uint8_t *)(Bank_NAND_ADDR | CMD_AREA)
#define NAND_ADDR_AREA		*(__IO uint8_t *)(Bank_NAND_ADDR | ADDR_AREA)
#define NAND_DATA_AREA		*(__IO uint8_t *)(Bank_NAND_ADDR | DATA_AREA)

/* 逻辑块号映射表。好块总数的2%用于备份区，因此数组维数低于1024。 LUT = Look Up Table */
static uint16_t s_usLUT[NAND_BLOCK_COUNT];

static uint16_t s_usValidDataBlockCount;	/* 有效的数据块个数 */

static uint8_t s_ucTempBuf[NAND_PAGE_TOTAL_SIZE];	/* 大缓冲区，2112字节. 用于读出比较 */

static uint8_t NAND_BuildLUT(void);
static uint8_t FSMC_NAND_GetStatus(void);
static uint16_t NAND_FindFreeBlock (uint32_t _ulSrcPageNo);
static uint8_t NAND_MarkUsedBlock(uint32_t _ulBlockNo);

static uint16_t NAND_AddrToPhyBlockNo(uint32_t _ulMemAddr);
static uint8_t NAND_IsBufOk(uint8_t *_pBuf, uint32_t _ulLen, uint8_t _ucValue);
uint8_t NAND_WriteToNewBlock(uint32_t _ulPhyPageNo, uint8_t *_pWriteBuf, uint16_t _usOffset, uint16_t _usSize);
static uint8_t NAND_IsFreeBlock(uint32_t _ulBlockNo);

static uint16_t NAND_LBNtoPBN(uint32_t _uiLBN);

static uint8_t FSMC_NAND_ReadPage(uint8_t *_pBuffer, uint32_t _ulPageNo, uint16_t _usAddrInPage, uint16_t _usByteCount);
static uint8_t FSMC_NAND_WritePage(uint8_t *_pBuffer, uint32_t _ulPageNo, uint16_t _usAddrInPage, uint16_t _usByteCount);
static uint8_t FSMC_NAND_CompPage(uint8_t *_pBuffer, uint32_t _ulPageNo, uint16_t _usAddrInPage, uint16_t _usByteCount);

static uint8_t FSMC_NAND_EraseBlock(uint32_t _ulBlockNo);

/*
*********************************************************************************************************
*	函 数 名: FSMC_NAND_Init
*	功能说明: 配置FSMC和GPIO用于NAND Flash接口。这个函数必须在读写nand flash前被调用一次。
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
static void FSMC_NAND_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure; 
	FSMC_NANDInitTypeDef FSMC_NANDInitStructure;
	FSMC_NAND_PCCARDTimingInitTypeDef  p;
	
	/* 使能 FSMC 时钟 */
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FSMC, ENABLE);
	
	/* 使能GPIOD、GPIOE、GPIOF、GPIOG 时钟 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOE | 
	                     RCC_APB2Periph_GPIOF | RCC_APB2Periph_GPIOG, ENABLE);
	
	/* GPIO 配置 */
	/* 控制线CLE, ALE, D0-D3, NOE, NWE 和 NCE2  NAND 引脚配置为复用功能（即用于FSMC)  */
	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_14 | GPIO_Pin_15 |  
	                             GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5 | 
	                             GPIO_Pin_7;                                  
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOD, &GPIO_InitStructure); 
	
	/* 数据线 D4-D7 引脚配置为复用功能 */  
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;
	GPIO_Init(GPIOE, &GPIO_InitStructure);
	
	/* NWAIT 引脚配置. 安富莱STM32-V4开发板缺省未使用NWAIT引脚作为忙信号，使用的是INT2
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;   							 
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(GPIOD, &GPIO_InitStructure); 
	*/
	
	/* INT2 引脚配置为内部上来输入，用于忙信号 */  
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;   
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;								 
	GPIO_Init(GPIOG, &GPIO_InitStructure);
	
	/* FSMC 配置 */
	p.FSMC_SetupTime = 0x1;
	p.FSMC_WaitSetupTime = 0x3;
	p.FSMC_HoldSetupTime = 0x2;
	p.FSMC_HiZSetupTime = 0x1;
	
	FSMC_NANDInitStructure.FSMC_Bank = FSMC_Bank2_NAND;							/* 定义FSMC BANK 号 */
	FSMC_NANDInitStructure.FSMC_Waitfeature = FSMC_Waitfeature_Enable;			/* 插入等待时序使能 */
	FSMC_NANDInitStructure.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_8b;		/* 数据宽度 8bit */
	FSMC_NANDInitStructure.FSMC_ECC = FSMC_ECC_Enable;							/* ECC错误检查和纠正功能使能 */
	FSMC_NANDInitStructure.FSMC_ECCPageSize = FSMC_ECCPageSize_2048Bytes;		/* ECC 页面大小 */
	FSMC_NANDInitStructure.FSMC_TCLRSetupTime = 0x00;
	FSMC_NANDInitStructure.FSMC_TARSetupTime = 0x00;
	FSMC_NANDInitStructure.FSMC_CommonSpaceTimingStruct = &p;
	FSMC_NANDInitStructure.FSMC_AttributeSpaceTimingStruct = &p;
	
	FSMC_NANDInit(&FSMC_NANDInitStructure);
	
	/* FSMC NAND Bank 使能 */
	FSMC_NANDCmd(FSMC_Bank2_NAND, ENABLE);
}

/*
*********************************************************************************************************
*	函 数 名: NAND_ReadID
*	功能说明: 读NAND Flash的ID。ID存储到形参指定的结构体变量中。
*	形    参:  无
*	返 回 值: 32bit的NAND Flash ID
*********************************************************************************************************
*/
uint32_t NAND_ReadID(void)
{
	uint32_t data = 0;

	/* 发送命令 Command to the command area */
	NAND_CMD_AREA = 0x90;
	NAND_ADDR_AREA = 0x00;

	/* 顺序读取NAND Flash的ID */
	data = *(__IO uint32_t *)(Bank_NAND_ADDR | DATA_AREA);
	data =  ((data << 24) & 0xFF000000) |
			((data << 8 ) & 0x00FF0000) |
			((data >> 8 ) & 0x0000FF00) |
			((data >> 24) & 0x000000FF) ;
	return data;
}

/*
*********************************************************************************************************
*	函 数 名: FSMC_NAND_PageCopyBack
*	功能说明: 将一页数据复制到另外一个页。源页和目标页所在的block必须同为偶数或同为奇数。即A18必须相同。
*	形    参:  - _ulSrcPageNo: 源页号
*             - _ulTarPageNo: 目标页号
*	返 回 值: 执行结果：
*				- NAND_FAIL 表示失败
*				- NAND_OK 表示成功
*
*	说    明：数据手册推荐：在页复制之前，先校验源页的位校验，否则可能会积累位错误。本函数未实现。
*
*********************************************************************************************************
*/
static uint8_t FSMC_NAND_PageCopyBack(uint32_t _ulSrcPageNo, uint32_t _ulTarPageNo)
{
	uint8_t i;

	if ((_ulSrcPageNo & 0x40) != (_ulTarPageNo & 0x40))
	{
		printf_err("Error : FSMC_NAND_PageCopyBackEx(src=%d, tar=%d) \r\n", _ulSrcPageNo, _ulTarPageNo);
		return NAND_FAIL;
	}

	NAND_CMD_AREA = NAND_CMD_COPYBACK_A;

	/* 发送源页地址 ， 对于 HY27UF081G2A
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		第1字节： A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr 的bit7 - bit0)
		第2字节： 0    0    0    0    A11  A10  A9   A8		(_usPageAddr 的bit11 - bit8, 高4bit必须是0)
		第3字节： A19  A18  A17  A16  A15  A14  A13  A12
		第4字节： A27  A26  A25  A24  A23  A22  A21  A20

		H27U4G8F2DTR (512MB)
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		第1字节： A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr 的bit7 - bit0)
		第2字节： 0    0    0    0    A11  A10  A9   A8		(_usPageAddr 的bit11 - bit8, 高4bit必须是0)
		第3字节： A19  A18  A17  A16  A15  A14  A13  A12
		第4字节： A27  A26  A25  A24  A23  A22  A21  A20
		第5字节： A28  A29  A30  A31  0    0    0    0
	*/
	NAND_ADDR_AREA = 0;
	NAND_ADDR_AREA = 0;
	NAND_ADDR_AREA = _ulSrcPageNo;
	NAND_ADDR_AREA = (_ulSrcPageNo & 0xFF00) >> 8;

	#if NAND_ADDR_5 == 1
		NAND_ADDR_AREA = (_ulSrcPageNo & 0xFF0000) >> 16;
	#endif

	NAND_CMD_AREA = NAND_CMD_COPYBACK_B;

	/* 必须等待，否则读出数据异常, 此处应该判断超时 */
	for (i = 0; i < 20; i++);
	while( GPIO_ReadInputDataBit(GPIOG, GPIO_Pin_6) == 0 );

	NAND_CMD_AREA = NAND_CMD_COPYBACK_C;

	/* 发送目标页地址 ， 对于 HY27UF081G2A
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		第1字节： A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr 的bit7 - bit0)
		第2字节： 0    0    0    0    A11  A10  A9   A8		(_usPageAddr 的bit11 - bit8, 高4bit必须是0)
		第3字节： A19  A18  A17  A16  A15  A14  A13  A12
		第4字节： A27  A26  A25  A24  A23  A22  A21  A20

		H27U4G8F2DTR (512MB)
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		第1字节： A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr 的bit7 - bit0)
		第2字节： 0    0    0    0    A11  A10  A9   A8		(_usPageAddr 的bit11 - bit8, 高4bit必须是0)
		第3字节： A19  A18  A17  A16  A15  A14  A13  A12    --- A18 是plane地址
		第4字节： A27  A26  A25  A24  A23  A22  A21  A20
		第5字节： A28  A29  A30  A31  0    0    0    0

		Source and Destination page in the copy back program sequence must belong to the same device plane （A18）
		源地址和目标地址的 A18必须相同
	*/
	NAND_ADDR_AREA = 0;
	NAND_ADDR_AREA = 0;
	NAND_ADDR_AREA = _ulTarPageNo;
	NAND_ADDR_AREA = (_ulTarPageNo & 0xFF00) >> 8;

	#if NAND_ADDR_5 == 1
		NAND_ADDR_AREA = (_ulTarPageNo & 0xFF0000) >> 16;
	#endif

	NAND_CMD_AREA = NAND_CMD_COPYBACK_D;

	/* 检查操作状态 */
	if (FSMC_NAND_GetStatus() == NAND_READY)
	{
		return NAND_OK;
	}

	printf_err("Error: FSMC_NAND_PageCopyBack(%d, %d)\r\n", _ulSrcPageNo, _ulTarPageNo);
	return NAND_FAIL;
}

/*
*********************************************************************************************************
*	函 数 名: FSMC_NAND_PageCopyBackEx
*	功能说明: 将一页数据复制到另外一个页,并更新目标页中的部分数据。源页和目标页所在的BLOCK必须同为偶数或同为奇数。
*	形    参:  - _ulSrcPageNo: 源页号
*             - _ulTarPageNo: 目标页号
*			  - _usOffset: 页内偏移地址，pBuf的内容将写入这个地址开始单元
*			  - _pBuf: 数据缓冲区
*			  - _usSize: 数据大小
*	返 回 值: 执行结果：
*				- NAND_FAIL 表示失败
*				- NAND_OK 表示成功
*
*	说    明：数据手册推荐：在页复制之前，先校验源页的位校验，否则可能会积累位错误。本函数未实现。
*
*********************************************************************************************************
*/
static uint8_t FSMC_NAND_PageCopyBackEx(uint32_t _ulSrcPageNo, uint32_t _ulTarPageNo, uint8_t *_pBuf, 
	uint16_t _usOffset, uint16_t _usSize)
{
	uint16_t i;

	if ((_ulSrcPageNo & 0x40) != (_ulTarPageNo & 0x40))
	{
		printf_err("Error A18 not same:  FSMC_NAND_PageCopyBackEx(src=%d, tar=%d) \r\n", _ulSrcPageNo, _ulTarPageNo);
		return NAND_FAIL;
	}

	NAND_CMD_AREA = NAND_CMD_COPYBACK_A;

	/* 发送源页地址 ， 对于 HY27UF081G2A
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		第1字节： A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr 的bit7 - bit0)
		第2字节： 0    0    0    0    A11  A10  A9   A8		(_usPageAddr 的bit11 - bit8, 高4bit必须是0)
		第3字节： A19  A18  A17  A16  A15  A14  A13  A12
		第4字节： A27  A26  A25  A24  A23  A22  A21  A20

		H27U4G8F2DTR (512MB)
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		第1字节： A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr 的bit7 - bit0)
		第2字节： 0    0    0    0    A11  A10  A9   A8		(_usPageAddr 的bit11 - bit8, 高4bit必须是0)
		第3字节： A19  A18  A17  A16  A15  A14  A13  A12
		第4字节： A27  A26  A25  A24  A23  A22  A21  A20
		第5字节： A28  A29  A30  A31  0    0    0    0
	*/
	NAND_ADDR_AREA = 0;
	NAND_ADDR_AREA = 0;
	NAND_ADDR_AREA = _ulSrcPageNo;
	NAND_ADDR_AREA = (_ulSrcPageNo & 0xFF00) >> 8;

	#if NAND_ADDR_5 == 1
		NAND_ADDR_AREA = (_ulSrcPageNo & 0xFF0000) >> 16;
	#endif

	NAND_CMD_AREA = NAND_CMD_COPYBACK_B;

	/* 必须等待，否则读出数据异常, 此处应该判断超时 */
	for (i = 0; i < 20; i++);
	while( GPIO_ReadInputDataBit(GPIOG, GPIO_Pin_6) == 0 );

	NAND_CMD_AREA = NAND_CMD_COPYBACK_C;

	/* 发送目标页地址 ， 对于 HY27UF081G2A
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		第1字节： A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr 的bit7 - bit0)
		第2字节： 0    0    0    0    A11  A10  A9   A8		(_usPageAddr 的bit11 - bit8, 高4bit必须是0)
		第3字节： A19  A18  A17  A16  A15  A14  A13  A12
		第4字节： A27  A26  A25  A24  A23  A22  A21  A20

		H27U4G8F2DTR (512MB)
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		第1字节： A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr 的bit7 - bit0)
		第2字节： 0    0    0    0    A11  A10  A9   A8		(_usPageAddr 的bit11 - bit8, 高4bit必须是0)
		第3字节： A19  A18  A17  A16  A15  A14  A13  A12
		第4字节： A27  A26  A25  A24  A23  A22  A21  A20
		第5字节： A28  A29  A30  A31  0    0    0    0
	*/
	NAND_ADDR_AREA = 0;
	NAND_ADDR_AREA = 0;
	NAND_ADDR_AREA = _ulTarPageNo;
	NAND_ADDR_AREA = (_ulTarPageNo & 0xFF00) >> 8;

	#if NAND_ADDR_5 == 1
		NAND_ADDR_AREA = (_ulTarPageNo & 0xFF0000) >> 16;
	#endif

	/* 中间无需带数据, 也无需等待 */

	NAND_CMD_AREA = NAND_CMD_COPYBACK_C;

	NAND_ADDR_AREA = _usOffset;
	NAND_ADDR_AREA = _usOffset >> 8;

	/* 发送数据 */
	for(i = 0; i < _usSize; i++)
	{
		NAND_DATA_AREA = _pBuf[i];
	}

	NAND_CMD_AREA = NAND_CMD_COPYBACK_D;

	/* 检查操作状态 */
	if (FSMC_NAND_GetStatus() == NAND_READY)
	{
		return NAND_OK;
	}

	printf_err("Error: FSMC_NAND_PageCopyBackEx(src=%d, tar=%d, offset=%d, size=%d)\r\n",
			_ulSrcPageNo, _ulTarPageNo, _usOffset, _usSize);
	return NAND_FAIL;
}

/*
*********************************************************************************************************
*	函 数 名: FSMC_NAND_WritePage
*	功能说明: 写一组数据至NandFlash指定页面的指定位置，写入的数据长度不大于一页的大小。
*	形    参:  - _pBuffer: 指向包含待写数据的缓冲区
*             - _ulPageNo: 页号，所有的页统一编码，范围为：0 - 65535
*			  - _usAddrInPage : 页内地址，范围为：0-2111
*             - _usByteCount: 写入的字节个数
*	返 回 值: 执行结果：
*				- NAND_FAIL 表示失败
*				- NAND_OK 表示成功
*********************************************************************************************************
*/
static uint8_t FSMC_NAND_WritePage(uint8_t *_pBuffer, uint32_t _ulPageNo, uint16_t _usAddrInPage, uint16_t _usByteCount)
{
	uint16_t i;

	/* 发送页写命令 */
	NAND_CMD_AREA = NAND_CMD_WRITE0;

	/* 发送页内地址 ， 对于 HY27UF081G2A
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		第1字节： A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr 的bit7 - bit0)
		第2字节： 0    0    0    0    A11  A10  A9   A8		(_usPageAddr 的bit11 - bit8, 高4bit必须是0)
		第3字节： A19  A18  A17  A16  A15  A14  A13  A12
		第4字节： A27  A26  A25  A24  A23  A22  A21  A20

		H27U4G8F2DTR (512MB)
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		第1字节： A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr 的bit7 - bit0)
		第2字节： 0    0    0    0    A11  A10  A9   A8		(_usPageAddr 的bit11 - bit8, 高4bit必须是0)
		第3字节： A19  A18  A17  A16  A15  A14  A13  A12
		第4字节： A27  A26  A25  A24  A23  A22  A21  A20
		第5字节： A28  A29  A30  A31  0    0    0    0
	*/
	NAND_ADDR_AREA = _usAddrInPage;
	NAND_ADDR_AREA = _usAddrInPage >> 8;
	NAND_ADDR_AREA = _ulPageNo;
	NAND_ADDR_AREA = (_ulPageNo & 0xFF00) >> 8;

	#if NAND_ADDR_5 == 1
		NAND_ADDR_AREA = (_ulPageNo & 0xFF0000) >> 16;
	#endif

	/* tADL = 100ns,  Address to Data Loading */
	for (i = 0; i < 20; i++);	/* 需要大于 100ns */

	/* 写数据 */
	for(i = 0; i < _usByteCount; i++)
	{
		NAND_DATA_AREA = _pBuffer[i];
	}
	NAND_CMD_AREA = NAND_CMD_WRITE_TRUE1;

	/* WE High to Busy , 100ns */
	for (i = 0; i < 20; i++);	/* 需要大于 100ns */
	while( GPIO_ReadInputDataBit(GPIOG, GPIO_Pin_6) == 0 );
	
	/* 检查操作状态 */
	if (FSMC_NAND_GetStatus() == NAND_READY)
	{
			/* 读出数据进行校验 */
		#ifdef WRITE_PAGE_VERIFY_EN
			FSMC_NAND_ReadPage (s_ucTempBuf, _ulPageNo, _usAddrInPage, _usByteCount);
			if (memcmp(s_ucTempBuf,  _pBuffer, _usByteCount) != 0)
			{
				printf_err("Error1: FSMC_NAND_WritePage(page=%d, addr=%d, count=%d)\r\n",
					_ulPageNo, _usAddrInPage, _usByteCount);				
				return NAND_FAIL;	
			}			
		#endif
		
		return NAND_OK;
	}

	printf_err("Error2: FSMC_NAND_WritePage(page=%d, addr=%d, count=%d)\r\n",
		_ulPageNo, _usAddrInPage, _usByteCount);
	
	return NAND_FAIL;
}

/*
*********************************************************************************************************
*	函 数 名: FSMC_NAND_ReadPage
*	功能说明: 从NandFlash指定页面的指定位置读一组数据，读出的数据长度不大于一页的大小。
*	形    参:  - _pBuffer: 指向包含待写数据的缓冲区
*             - _ulPageNo: 页号，所有的页统一编码，范围为：0 - 65535
*			  - _usAddrInPage : 页内地址，范围为：0-2111
*             - _usByteCount: 字节个数，  （最大 2048 + 64）
*	返 回 值: 执行结果：
*				- NAND_FAIL 表示失败
*				- NAND_OK 表示成功
*********************************************************************************************************
*/
static uint8_t FSMC_NAND_ReadPage(uint8_t *_pBuffer, uint32_t _ulPageNo, uint16_t _usAddrInPage, uint16_t _usByteCount)
{
	uint16_t i;

    /* 发送页面读命令 */
    NAND_CMD_AREA = NAND_CMD_AREA_A;

	/* 发送页内地址 ， 对于 HY27UF081G2A  (128MB)
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		第1字节： A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr 的bit7 - bit0)
		第2字节： 0    0    0    0    A11  A10  A9   A8		(_usPageAddr 的bit11 - bit8, 高4bit必须是0)
		第3字节： A19  A18  A17  A16  A15  A14  A13  A12
		第4字节： A27  A26  A25  A24  A23  A22  A21  A20

		H27U4G8F2DTR (512MB)
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		第1字节： A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr 的bit7 - bit0)
		第2字节： 0    0    0    0    A11  A10  A9   A8		(_usPageAddr 的bit11 - bit8, 高4bit必须是0)
		第3字节： A19  A18  A17  A16  A15  A14  A13  A12
		第4字节： A27  A26  A25  A24  A23  A22  A21  A20
		第5字节： A28  A29  A30  A31  0    0    0    0
	*/
	NAND_ADDR_AREA = _usAddrInPage;
	NAND_ADDR_AREA = _usAddrInPage >> 8;
	NAND_ADDR_AREA = _ulPageNo;
	NAND_ADDR_AREA = (_ulPageNo & 0xFF00) >> 8;

	#if NAND_ADDR_5 == 1
		NAND_ADDR_AREA = (_ulPageNo & 0xFF0000) >> 16;
	#endif

	NAND_CMD_AREA = NAND_CMD_AREA_TRUE1;

	 /* 必须等待，否则读出数据异常, 此处应该判断超时 */
	for (i = 0; i < 20; i++);
	while( GPIO_ReadInputDataBit(GPIOG, GPIO_Pin_6) == 0);

	/* 读数据到缓冲区pBuffer */
	for(i = 0; i < _usByteCount; i++)
	{
		_pBuffer[i] = NAND_DATA_AREA;
	}

	return NAND_OK;
}

/*
*********************************************************************************************************
*	函 数 名: FSMC_NAND_CompPage
*	功能说明: 比较数据
*	形    参:  - _pBuffer: 指向包含待比较的数据缓冲区
*             - _ulPageNo: 页号，所有的页统一编码，范围为：0 - 65535
*			  - _usAddrInPage : 页内地址，范围为：0-2111
*             - _usByteCount: 字节个数，  （最大 2048 + 64）
*	返 回 值: 执行结果：
*				- NAND_FAIL 表示失败
*				- NAND_OK 表示成功， 相等
*********************************************************************************************************
*/
static uint8_t FSMC_NAND_CompPage(uint8_t *_pBuffer, uint32_t _ulPageNo, uint16_t _usAddrInPage, uint16_t _usByteCount)
{
	uint16_t i;

    /* 发送页面读命令 */
    NAND_CMD_AREA = NAND_CMD_AREA_A;

	/* 发送页内地址 ， 对于 HY27UF081G2A  (128MB)
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		第1字节： A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr 的bit7 - bit0)
		第2字节： 0    0    0    0    A11  A10  A9   A8		(_usPageAddr 的bit11 - bit8, 高4bit必须是0)
		第3字节： A19  A18  A17  A16  A15  A14  A13  A12
		第4字节： A27  A26  A25  A24  A23  A22  A21  A20

		H27U4G8F2DTR (512MB)
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		第1字节： A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr 的bit7 - bit0)
		第2字节： 0    0    0    0    A11  A10  A9   A8		(_usPageAddr 的bit11 - bit8, 高4bit必须是0)
		第3字节： A19  A18  A17  A16  A15  A14  A13  A12
		第4字节： A27  A26  A25  A24  A23  A22  A21  A20
		第5字节： A28  A29  A30  A31  0    0    0    0
	*/
	NAND_ADDR_AREA = _usAddrInPage;
	NAND_ADDR_AREA = _usAddrInPage >> 8;
	NAND_ADDR_AREA = _ulPageNo;
	NAND_ADDR_AREA = (_ulPageNo & 0xFF00) >> 8;

	#if NAND_ADDR_5 == 1
		NAND_ADDR_AREA = (_ulPageNo & 0xFF0000) >> 16;
	#endif

	NAND_CMD_AREA = NAND_CMD_AREA_TRUE1;

	 /* 必须等待，否则读出数据异常, 此处应该判断超时 */
	for (i = 0; i < 20; i++);
	while( GPIO_ReadInputDataBit(GPIOG, GPIO_Pin_6) == 0);

	/* 读数据到缓冲区pBuffer */
	for(i = 0; i < _usByteCount; i++)
	{
		if (_pBuffer[i] != NAND_DATA_AREA)
		{
			return NAND_FAIL;
		}
	}

	return NAND_OK;
}

/*
*********************************************************************************************************
*	函 数 名: FSMC_NAND_WriteSpare
*	功能说明: 向1个PAGE的Spare区写入数据
*	形    参:  - _pBuffer: 指向包含待写数据的缓冲区
*             - _ulPageNo: 页号，所有的页统一编码，范围为：0 - 65535
*			  - _usAddrInSpare : 页内备用区的偏移地址，范围为：0-63
*             - _usByteCount: 写入的字节个数
*	返 回 值: 执行结果：
*				- NAND_FAIL 表示失败
*				- NAND_OK 表示成功
*********************************************************************************************************
*/
static uint8_t FSMC_NAND_WriteSpare(uint8_t *_pBuffer, uint32_t _ulPageNo, uint16_t _usAddrInSpare, uint16_t _usByteCount)
{
	if (_usByteCount > NAND_SPARE_AREA_SIZE)
	{
		printf_err("Error: FSMC_NAND_WriteSpare() %d\r\n",_usByteCount);
		return NAND_FAIL;
	}

	return FSMC_NAND_WritePage(_pBuffer, _ulPageNo, NAND_PAGE_SIZE + _usAddrInSpare, _usByteCount);
}

/*
*********************************************************************************************************
*	函 数 名: FSMC_NAND_ReadSpare
*	功能说明: 读1个PAGE的Spare区的数据
*	形    参:  - _pBuffer: 指向包含待写数据的缓冲区
*             - _ulPageNo: 页号，所有的页统一编码，范围为：0 - 65535
*			  - _usAddrInSpare : 页内备用区的偏移地址，范围为：0-63
*             - _usByteCount: 写入的字节个数
*	返 回 值: 执行结果：
*				- NAND_FAIL 表示失败
*				- NAND_OK 表示成功
*********************************************************************************************************
*/
static uint8_t FSMC_NAND_ReadSpare(uint8_t *_pBuffer, uint32_t _ulPageNo, uint16_t _usAddrInSpare, uint16_t _usByteCount)
{
	if (_usByteCount > NAND_SPARE_AREA_SIZE)
	{
		printf_err("Error: FSMC_NAND_ReadSpare() %d\r\n",_usByteCount);
		return NAND_FAIL;
	}

	return FSMC_NAND_ReadPage(_pBuffer, _ulPageNo, NAND_PAGE_SIZE + _usAddrInSpare, _usByteCount);
}

/*
*********************************************************************************************************
*	函 数 名: FSMC_NAND_WriteData
*	功能说明: 向1个PAGE的主数据区写入数据
*	形    参:  - _pBuffer: 指向包含待写数据的缓冲区
*             - _ulPageNo: 页号，所有的页统一编码，范围为：0 - 65535
*			  - _usAddrInPage : 页内数据区的偏移地址，范围为：0-2047
*             - _usByteCount: 写入的字节个数
*	返 回 值: 执行结果：
*				- NAND_FAIL 表示失败
*				- NAND_OK 表示成功
*********************************************************************************************************
*/
static uint8_t FSMC_NAND_WriteData(uint8_t *_pBuffer, uint32_t _ulPageNo, uint16_t _usAddrInPage, uint16_t _usByteCount)
{
	if (_usByteCount > NAND_PAGE_SIZE)
	{
		printf_err("Error: FSMC_NAND_WriteData() %d\r\n",_usByteCount);
		return NAND_FAIL;
	}

	return FSMC_NAND_WritePage(_pBuffer, _ulPageNo, _usAddrInPage, _usByteCount);
}

/*
*********************************************************************************************************
*	函 数 名: FSMC_NAND_ReadData
*	功能说明: 读1个PAGE的主数据的数据
*	形    参:  - _pBuffer: 指向包含待写数据的缓冲区
*             - _ulPageNo: 页号，所有的页统一编码，范围为：0 - 65535
*			  - _usAddrInPage : 页内数据区的偏移地址，范围为：0-2047
*             - _usByteCount: 写入的字节个数
*	返 回 值: 执行结果：
*				- NAND_FAIL 表示失败
*				- NAND_OK 表示成功
*********************************************************************************************************
*/
static uint8_t FSMC_NAND_ReadData(uint8_t *_pBuffer, uint32_t _ulPageNo, uint16_t _usAddrInPage, uint16_t _usByteCount)
{
	if (_usByteCount > NAND_PAGE_SIZE)
	{
		printf_err("Error: FSMC_NAND_ReadData() %d\r\n",_usByteCount);
		return NAND_FAIL;
	}

	return FSMC_NAND_ReadPage(_pBuffer, _ulPageNo, _usAddrInPage, _usByteCount);
}

/*
*********************************************************************************************************
*	函 数 名: FSMC_NAND_EraseBlock
*	功能说明: 擦除NAND Flash一个块（block）
*	形    参:  - _ulBlockNo: 块号，范围为：0 - 1023,   0-4095
*	返 回 值: NAND操作状态，有如下几种值：
*             - NAND_TIMEOUT_ERROR  : 超时错误
*             - NAND_READY          : 操作成功
*********************************************************************************************************
*/
static uint8_t FSMC_NAND_EraseBlock(uint32_t _ulBlockNo)
{
	/* HY27UF081G2A  (128MB)
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		第1字节： A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr 的bit7 - bit0)
		第2字节： 0    0    0    0    A11  A10  A9   A8		(_usPageAddr 的bit11 - bit8, 高4bit必须是0)
		第3字节： A19  A18  A17  A16  A15  A14  A13  A12    A18以上是块号
		第4字节： A27  A26  A25  A24  A23  A22  A21  A20

		H27U4G8F2DTR (512MB)
				  Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
		第1字节： A7   A6   A5   A4   A3   A2   A1   A0		(_usPageAddr 的bit7 - bit0)
		第2字节： 0    0    0    0    A11  A10  A9   A8		(_usPageAddr 的bit11 - bit8, 高4bit必须是0)
		第3字节： A19  A18  A17  A16  A15  A14  A13  A12    A18以上是块号
		第4字节： A27  A26  A25  A24  A23  A22  A21  A20
		第5字节： A28  A29  A30  A31  0    0    0    0
	*/

	/* 发送擦除命令 */
	NAND_CMD_AREA = NAND_CMD_ERASE0;

	_ulBlockNo <<= 6;	/* 块号转换为页编号 */

	#if NAND_ADDR_5 == 0	/* 128MB的 */
		NAND_ADDR_AREA = _ulBlockNo;
		NAND_ADDR_AREA = _ulBlockNo >> 8;
	#else		/* 512MB的 */
		NAND_ADDR_AREA = _ulBlockNo;
		NAND_ADDR_AREA = _ulBlockNo >> 8;
		NAND_ADDR_AREA = _ulBlockNo >> 16;
	#endif

	NAND_CMD_AREA = NAND_CMD_ERASE1;

	return (FSMC_NAND_GetStatus());
}

/*
*********************************************************************************************************
*	函 数 名: FSMC_NAND_Reset
*	功能说明: 复位NAND Flash
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
static uint8_t FSMC_NAND_Reset(void)
{
	NAND_CMD_AREA = NAND_CMD_RESET;

		/* 检查操作状态 */
	if (FSMC_NAND_GetStatus() == NAND_READY)
	{
		return NAND_OK;
	}

	return NAND_FAIL;
}

/*
*********************************************************************************************************
*	函 数 名: FSMC_NAND_ReadStatus
*	功能说明: 使用Read statuc 命令读NAND Flash内部状态
*	形    参:  - Address: 被擦除的快内任意地址
*	返 回 值: NAND操作状态，有如下几种值：
*             - NAND_BUSY: 内部正忙
*             - NAND_READY: 内部空闲，可以进行下步操作
*             - NAND_ERROR: 先前的命令执行失败
*********************************************************************************************************
*/
static uint8_t FSMC_NAND_ReadStatus(void)
{
	uint8_t ucData;
	uint8_t ucStatus = NAND_BUSY;

	/* 读状态操作 */
	NAND_CMD_AREA = NAND_CMD_STATUS;
	ucData = *(__IO uint8_t *)(Bank_NAND_ADDR);

	if((ucData & NAND_ERROR) == NAND_ERROR)
	{
		ucStatus = NAND_ERROR;
	}
	else if((ucData & NAND_READY) == NAND_READY)
	{
		ucStatus = NAND_READY;
	}
	else
	{
		ucStatus = NAND_BUSY;
	}

	return (ucStatus);
}

/*
*********************************************************************************************************
*	函 数 名: FSMC_NAND_GetStatus
*	功能说明: 获取NAND Flash操作状态
*	形    参:  - Address: 被擦除的快内任意地址
*	返 回 值: NAND操作状态，有如下几种值：
*             - NAND_TIMEOUT_ERROR  : 超时错误
*             - NAND_READY          : 操作成功
*********************************************************************************************************
*/
static uint8_t FSMC_NAND_GetStatus(void)
{
	uint32_t ulTimeout = 0x10000;
	uint8_t ucStatus = NAND_READY;

	ucStatus = FSMC_NAND_ReadStatus();

	/* 等待NAND操作结束，超时后会退出 */
	while ((ucStatus != NAND_READY) &&( ulTimeout != 0x00))
	{
		ucStatus = FSMC_NAND_ReadStatus();
		ulTimeout--;
	}

	if(ulTimeout == 0x00)
	{
		ucStatus =  NAND_TIMEOUT_ERROR;
	}

	/* 返回操作状态 */
	return (ucStatus);
}

/*
*********************************************************************************************************
*	函 数 名: NAND_Init
*	功能说明: 初始化NAND Flash接口
*	形    参:  无
*	返 回 值: 执行结果：
*				- NAND_FAIL 表示失败
*				- NAND_OK 表示成功
*********************************************************************************************************
*/
uint8_t NAND_Init(void)
{
	uint8_t Status;

	FSMC_NAND_Init();			/* 配置FSMC和GPIO用于NAND Flash接口 */

	FSMC_NAND_Reset();			/* 通过复位命令复位NAND Flash到读状态 */

	Status = NAND_BuildLUT();	/* 建立块管理表 LUT = Look up table */
	return Status;
}

/*
*********************************************************************************************************
*	函 数 名: NAND_WriteToNewBlock
*	功能说明: 将旧块的数据复制到新块，并将新的数据段写入这个新块. 
*	形    参:  	_ulPhyPageNo : 源页号
*				_pWriteBuf ： 数据缓冲区
*				_usOffset ： 页内偏移地址
*				_usSize ：数据长度，必须是4字节的整数倍
*	返 回 值: 执行结果：
*				- NAND_FAIL 表示失败
*				- NAND_OK 表示成功
*********************************************************************************************************
*/
uint8_t NAND_WriteToNewBlock(uint32_t _ulPhyPageNo, uint8_t *_pWriteBuf, uint16_t _usOffset, uint16_t _usSize)
{
	uint16_t n, i;
	uint16_t usNewBlock;
	uint16_t ulSrcBlock;
	uint16_t usOffsetPageNo;

	ulSrcBlock = _ulPhyPageNo / NAND_BLOCK_SIZE;		/* 根据物理页号反推块号 */
	usOffsetPageNo = _ulPhyPageNo % NAND_BLOCK_SIZE;	/* 根据物理页号计算物理页号在块内偏移页号 */
	/* 增加循环的目的是处理目标块为坏块的情况 */
	for (n = 0; n < 10; n++)
	{
		/* 如果不是全0xFF， 则需要寻找一个空闲可用块，并将页内的数据全部移到新块中，然后擦除这个块 */
		usNewBlock = NAND_FindFreeBlock(_ulPhyPageNo);	/* 从最后一个Block开始，搜寻一个可用块.  */
		if (usNewBlock >= NAND_BLOCK_COUNT)
		{
			printf_err("Error1: NAND_WriteToNewBlock() %d\r\n", usNewBlock);
			return NAND_FAIL;	/* 查找空闲块失败 */
		}

		printf_ok("NAND_WriteToNewBlock(%d -> %d)\r\n", ulSrcBlock, usNewBlock);
		
		/* 使用page-copy功能，将当前块（usPBN）的数据全部搬移到新块（usNewBlock） */
		for (i = 0; i < NAND_BLOCK_SIZE; i++)
		{
			if (i == usOffsetPageNo)
			{
				/* 如果写入的数据在当前页，则需要使用带随机数据的Copy-Back命令 */
				if (FSMC_NAND_PageCopyBackEx(ulSrcBlock * NAND_BLOCK_SIZE + i, usNewBlock * NAND_BLOCK_SIZE + i,
					_pWriteBuf, _usOffset, _usSize) == NAND_FAIL)
				{
					printf_err("Error2: NAND_WriteToNewBlock() %d\r\n", ulSrcBlock);
					NAND_MarkBadBlock(usNewBlock);	/* 将新块标记为坏块 */
					NAND_BuildLUT();				/* 重建LUT表 */
					break;
				}

			}
			else
			{
				/* 使用NAND Flash 提供的整页Copy-Back功能，可以显著提高操作效率 */
				if (FSMC_NAND_PageCopyBack(ulSrcBlock * NAND_BLOCK_SIZE + i,
					usNewBlock * NAND_BLOCK_SIZE + i) == NAND_FAIL)
				{
					printf_err("Error3: NAND_WriteToNewBlock() %d\r\n", ulSrcBlock);
					
					NAND_MarkBadBlock(usNewBlock);	/* 将新块标记为坏块 */
					NAND_BuildLUT();				/* 重建LUT表 */
					break;
				}
			}
		}
		/* 目标块更新成功 */
		if (i == NAND_BLOCK_SIZE)
		{
			/* 标记新块为已用块 */
			if (NAND_MarkUsedBlock(usNewBlock) == NAND_FAIL)
			{
				NAND_MarkBadBlock(usNewBlock);	/* 将新块标记为坏块 */
				NAND_BuildLUT();				/* 重建LUT表 */
				continue;
			}

			/* 擦除源BLOCK  (如果源块写失败，则会擦除坏块标记) */
			if (FSMC_NAND_EraseBlock(ulSrcBlock) != NAND_READY)
			{
				printf_err("Error4: FSMC_NAND_EraseBlock(), %d\r\n", ulSrcBlock);
				NAND_MarkBadBlock(ulSrcBlock);	/* 将源块标记为坏块 */
				NAND_BuildLUT();				/* 重建LUT表 */
				continue;
			}
			NAND_BuildLUT();				/* 重建LUT表 */
			break;
		}
	}
	if (n == 10)
	{
		printf_err("Error5: FSMC_NAND_EraseBlock() n=%d\r\n", n);
		return NAND_FAIL;
	}

	return NAND_OK;	/* 写入成功 */
}

/*
*********************************************************************************************************
*	函 数 名: NAND_Write
*	功能说明: 写一个扇区
*	形    参:  	_MemAddr : 内存单元偏移地址
*				_pReadbuff ：存放待写数据的缓冲区的指针
*				_usSize ：数据长度，必须是4字节的整数倍
*	返 回 值: 执行结果：
*				- NAND_FAIL 表示失败
*				- NAND_OK 表示成功
*********************************************************************************************************
*/
uint8_t NAND_Write(uint32_t _ulMemAddr, uint32_t *_pWriteBuf, uint16_t _usSize)
{
	uint16_t usPBN;			/* 物理块号 */
	uint32_t ulPhyPageNo;	/* 物理页号 */
	uint16_t usAddrInPage;	/* 页内偏移地址 */
	uint32_t ulTemp;

	/* 数据长度必须是4字节整数倍 */
	if ((_usSize % 4) != 0)
	{
		printf_err("Error:1 NAND_Write()  %d\r\n",_usSize);
		return NAND_FAIL;
	}
	/* 数据长度不能超过512字节(遵循 Fat格式) */
	if (_usSize > 512)
	{
		printf_err("Error:2 NAND_Write() %d\r\n",_usSize);
		//return NAND_FAIL;
	}

	usPBN = NAND_AddrToPhyBlockNo(_ulMemAddr);	/* 查询LUT表获得物理块号 */

	ulTemp = _ulMemAddr % (NAND_BLOCK_SIZE * NAND_PAGE_SIZE);
	ulPhyPageNo = usPBN * NAND_BLOCK_SIZE + ulTemp / NAND_PAGE_SIZE;	/* 计算物理页号 */
	usAddrInPage = ulTemp % NAND_PAGE_SIZE;	/* 计算页内偏移地址 */

	/* 读出扇区的内容，判断是否全FF */
	if (FSMC_NAND_ReadData(s_ucTempBuf, ulPhyPageNo, usAddrInPage, _usSize) == NAND_FAIL)
	{
		return NAND_FAIL;	/* 读NAND Flash失败 */
	}
	/*　如果是全0xFF, 则可以直接写入，无需擦除 */
	if (NAND_IsBufOk(s_ucTempBuf, _usSize, 0xFF) == 1)
	{
		if (FSMC_NAND_WriteData((uint8_t *)_pWriteBuf, ulPhyPageNo, usAddrInPage, _usSize) == NAND_FAIL)
		{
			/* 将数据写入到另外一个块（空闲块） */
			return NAND_WriteToNewBlock(ulPhyPageNo, (uint8_t *)_pWriteBuf, usAddrInPage, _usSize);
		}

		/* 标记该块已用 */
		if (NAND_MarkUsedBlock(ulPhyPageNo / NAND_BLOCK_SIZE) == NAND_FAIL)
		{
			/* 标记失败，将数据写入到另外一个块（空闲块） */
			return NAND_WriteToNewBlock(ulPhyPageNo, (uint8_t *)_pWriteBuf, usAddrInPage, _usSize);
		}
		return NAND_OK;	/* 写入成功 */
	}

	/* 将数据写入到另外一个块（空闲块） */
	return NAND_WriteToNewBlock(ulPhyPageNo, (uint8_t *)_pWriteBuf, usAddrInPage, _usSize);
}

/*
*********************************************************************************************************
*	函 数 名: NAND_Read
*	功能说明: 读一个扇区
*	形    参:  	_MemAddr : 内存单元偏移地址
*				_pReadbuff ：存放读出数据的缓冲区的指针
*				_usSize ：数据长度，必须是4字节的整数倍
*	返 回 值: 执行结果：
*				- NAND_FAIL 表示失败
*				- NAND_OK 表示成功
*********************************************************************************************************
*/
uint8_t NAND_Read(uint32_t _ulMemAddr, uint32_t *_pReadBuf, uint16_t _usSize)
{
	uint16_t usPBN;			/* 物理块号 */
	uint32_t ulPhyPageNo;	/* 物理页号 */
	uint16_t usAddrInPage;	/* 页内偏移地址 */
	uint32_t ulTemp;

	/* 数据长度必须是4字节整数倍 */
	if ((_usSize % 4) != 0)
	{
		printf_err("Error:1 NAND_Read(_usSize)  %d\r\n",_usSize);
		return NAND_FAIL;
	}

	usPBN = NAND_AddrToPhyBlockNo(_ulMemAddr);	/* 查询LUT表获得物理块号 */
	if (usPBN >= NAND_BLOCK_COUNT)
	{
		/* 没有格式化，usPBN = 0xFFFF */
		printf_err("Error:1 NAND_Write() usPBN %d\r\n",usPBN);
		return NAND_FAIL;
	}

	ulTemp = _ulMemAddr % (NAND_BLOCK_SIZE * NAND_PAGE_SIZE);
	ulPhyPageNo = usPBN * NAND_BLOCK_SIZE + ulTemp / NAND_PAGE_SIZE;	/* 计算物理页号 */
	usAddrInPage = ulTemp % NAND_PAGE_SIZE;	/* 计算页内偏移地址 */

	if (FSMC_NAND_ReadData((uint8_t *)_pReadBuf, ulPhyPageNo, usAddrInPage, _usSize) == NAND_FAIL)
	{
		return NAND_FAIL;	/* 读NAND Flash失败 */
	}

	/* 成功 */
	return NAND_OK;
}

/*
*********************************************************************************************************
*	函 数 名: NAND_WriteMultiSectors
*	功能说明: 该函数用于文件系统，连续写多个扇区数据。扇区大小可以是512字节或2048字节
*	形    参:  	_pBuf : 存放数据的缓冲区的指针
*				_SectorNo ：扇区号
*				_SectorSize ：每个扇区的大小 （一般是 512字节）
*				_SectorCount : 扇区个数
*	返 回 值: 执行结果：
*				- NAND_FAIL 表示失败
*				- NAND_OK 表示成功
*********************************************************************************************************
*/
uint8_t NAND_WriteMultiSectors(uint8_t *_pBuf, uint32_t _SectorNo, uint16_t _SectorSize, uint32_t _SectorCount)
{
	uint32_t i;
	uint32_t usLBN;			/* 逻辑块号 */
	uint32_t usPBN;			/* 物理块号 */
	uint32_t uiPhyPageNo;	/* 物理页号 */
	uint16_t usAddrInPage;	/* 页内偏移地址 */
	uint32_t ulTemp;
	uint8_t ucReturn;

	/*
		HY27UF081G2A = 128M Flash.  有 1024个BLOCK, 每个BLOCK包含64个PAGE， 每个PAGE包含2048+64字节，
		擦除最小单位是BLOCK， 编程最小单位是字节。

		每个PAGE在逻辑上可以分为4个512字节扇区。
	*/

	for (i = 0; i < _SectorCount; i++)
	{
		/* 根据逻辑扇区号和扇区大小计算逻辑块号 */
		//usLBN = (_SectorNo * _SectorSize) / (NAND_BLOCK_SIZE * NAND_PAGE_SIZE);
		/* (_SectorNo * _SectorSize) 乘积可能大于32位，因此换下面这种写法 */
		usLBN = (_SectorNo + i) / (NAND_BLOCK_SIZE * (NAND_PAGE_SIZE / _SectorSize));
		usPBN = NAND_LBNtoPBN(usLBN);	/* 查询LUT表获得物理块号 */
		if (usPBN >= NAND_BLOCK_COUNT)
		{
			printf_err("Error1: NAND_WriteMultiSectors(), no format. usLBN=%d, usPBN=%d\r\n", usLBN, usPBN);
			/* 没有格式化，usPBN = 0xFFFF */
			return NAND_FAIL;
		}

		//ulTemp = ((uint64_t)(_SectorNo + i) * _SectorSize) % (NAND_BLOCK_SIZE * NAND_PAGE_SIZE);
		ulTemp = ((_SectorNo + i) % (NAND_BLOCK_SIZE * (NAND_PAGE_SIZE / _SectorSize))) *  _SectorSize;
		uiPhyPageNo = usPBN * NAND_BLOCK_SIZE + ulTemp / NAND_PAGE_SIZE;	/* 计算物理页号 */
		usAddrInPage = ulTemp % NAND_PAGE_SIZE;	/* 计算页内偏移地址 */

		/* 如果 _SectorCount > 0, 并且是页面首地址，则可以进行优化 */
		if (usAddrInPage == 0)
		{
			/* 暂未处理 */
		}
		
		memset(s_ucTempBuf, 0xFF, _SectorSize);

		/*　如果是全0xFF, 则可以直接写入，无需擦除 */
		//if (NAND_IsBufOk(s_ucTempBuf, _SectorSize, 0xFF) == 1)
		if (FSMC_NAND_CompPage(s_ucTempBuf, uiPhyPageNo, usAddrInPage, _SectorSize) == NAND_OK)
		{
			if (FSMC_NAND_WriteData(&_pBuf[i * _SectorSize], uiPhyPageNo, usAddrInPage, _SectorSize) == NAND_FAIL)
			{
				printf_err("Error3: NAND_WriteMultiSectors(), Write Faile\r\n");
				/* 将数据写入到另外一个块（空闲块） */
				ucReturn = NAND_WriteToNewBlock(uiPhyPageNo, &_pBuf[i * _SectorSize], usAddrInPage, _SectorSize);
				if (ucReturn != NAND_OK)
				{
					printf_err("Error4: NAND_WriteMultiSectors(), Write Faile\r\n");
					return NAND_FAIL;	/* 失败 */
				}
				
				/* 标记源块为坏块 */
				NAND_MarkBadBlock(uiPhyPageNo / NAND_BLOCK_SIZE);	/* 将源块标记为坏块 */
				continue;
			}

			/* 标记该块已用 */
			if (NAND_MarkUsedBlock(uiPhyPageNo / NAND_BLOCK_SIZE) == NAND_FAIL)
			{
				/* 标记失败，将数据写入到另外一个块（空闲块） */
				ucReturn = NAND_WriteToNewBlock(uiPhyPageNo, &_pBuf[i * _SectorSize], usAddrInPage, _SectorSize);
				if (ucReturn != NAND_OK)
				{
					return NAND_FAIL;	/* 失败 */
				}
				continue;
			}
		}
		else	/* 目标区域已经有数据，不是全FF, 则直接将数据写入另外一个空闲块 */
		{
			/* 将数据写入到另外一个块（空闲块） */
			ucReturn = NAND_WriteToNewBlock(uiPhyPageNo, &_pBuf[i * _SectorSize], usAddrInPage, _SectorSize);
			if (ucReturn != NAND_OK)
			{
				printf_err("Error5: NAND_WriteMultiSectors(), Write Faile\r\n");
				return NAND_FAIL;	/* 失败 */
			}
			continue;
		}
	}
	return NAND_OK;		/* 成功 */
}

/*
*********************************************************************************************************
*	函 数 名: NAND_ReadMultiSectors
*	功能说明: 该函数用于文件系统，按扇区读数据。读1个或多个扇区，扇区大小可以是512字节或2048字节
*	形    参:  	_pBuf : 存放读出数据的缓冲区的指针
*				_SectorNo ：扇区号
*				_SectorSize ：每个扇区的大小
*				_SectorCount : 扇区个数
*	返 回 值: 执行结果：
*				- NAND_FAIL 表示失败
*				- NAND_OK 表示成功
*********************************************************************************************************
*/
uint8_t NAND_ReadMultiSectors(uint8_t *_pBuf, uint32_t _SectorNo, uint16_t _SectorSize, uint32_t _SectorCount)
{
	uint32_t i;
	uint32_t usLBN;			/* 逻辑块号 */
	uint32_t usPBN;			/* 物理块号 */
	uint32_t uiPhyPageNo;	/* 物理页号 */
	uint16_t usAddrInPage;	/* 页内偏移地址 */
	uint32_t ulTemp;

	/*
		HY27UF081G2A = 128M Flash.  有 1024个BLOCK, 每个BLOCK包含64个PAGE， 每个PAGE包含2048+64字节，
		擦除最小单位是BLOCK， 编程最小单位是字节。

		每个PAGE在逻辑上可以分为4个512字节扇区。
	*/

	for (i = 0; i < _SectorCount; i++)
	{
		/* 根据逻辑扇区号和扇区大小计算逻辑块号 */
		//usLBN = (_SectorNo * _SectorSize) / (NAND_BLOCK_SIZE * NAND_PAGE_SIZE);
		/* (_SectorNo * _SectorSize) 乘积可能大于32位，因此换下面这种写法 */
		usLBN = (_SectorNo + i) / (NAND_BLOCK_SIZE * (NAND_PAGE_SIZE / _SectorSize));
		usPBN = NAND_LBNtoPBN(usLBN);	/* 查询LUT表获得物理块号 */
		if (usPBN >= NAND_BLOCK_COUNT)
		{
			printf_err("Error: NAND_ReadMultiSectors(), not format, usPBN = %d\r\n", usPBN);
			/* 没有格式化，usPBN = 0xFFFF */
			return NAND_FAIL;
		}

		ulTemp = ((uint64_t)(_SectorNo + i) * _SectorSize) % (NAND_BLOCK_SIZE * NAND_PAGE_SIZE);
		uiPhyPageNo = usPBN * NAND_BLOCK_SIZE + ulTemp / NAND_PAGE_SIZE;	/* 计算物理页号 */
		usAddrInPage = ulTemp % NAND_PAGE_SIZE;	/* 计算页内偏移地址 */

		if (FSMC_NAND_ReadData((uint8_t *)&_pBuf[i * _SectorSize], uiPhyPageNo, usAddrInPage, _SectorSize) == NAND_FAIL)
		{
			printf_err("Error: NAND_ReadMultiSectors(), ReadData(page = %d, addr = %d)\r\n", uiPhyPageNo, usAddrInPage);
			return NAND_FAIL;	/* 读NAND Flash失败 */
		}
	}

	/* 成功 */
	return NAND_OK;
}

/*
*********************************************************************************************************
*	函 数 名: NAND_BuildLUT
*	功能说明: 在内存中创建坏块管理表
*	形    参:  ZoneNbr ：区号
*	返 回 值: NAND_OK： 成功； 	NAND_FAIL：失败
*********************************************************************************************************
*/
static uint8_t NAND_BuildLUT(void)
{
	uint16_t i;
	uint8_t buf[VALID_SPARE_SIZE];
	uint16_t usLBN;	/* 逻辑块号 */

	/* */
	for (i = 0; i < NAND_BLOCK_COUNT; i++)
	{
		s_usLUT[i] = 0xFFFF;	/* 填充无效值，用于重建LUT后，判断LUT是否合理 */
	}
	for (i = 0; i < NAND_BLOCK_COUNT; i++)
	{
		/* 读每个块的第1个PAGE，偏移地址为LBN0_OFFSET的数据 */
		FSMC_NAND_ReadSpare(buf, i * NAND_BLOCK_SIZE, 0, VALID_SPARE_SIZE);

		/* 如果是好块，则记录LBN0 LBN1 */
		if (buf[BI_OFFSET] == 0xFF)
		{
			usLBN = buf[LBN0_OFFSET] + buf[LBN1_OFFSET] * 256;	/* 计算读出的逻辑块号 */
			if (usLBN < NAND_BLOCK_COUNT)
			{
				/* 如果已经登记过了，则判定为异常 */
				if (s_usLUT[usLBN] != 0xFFFF)
				{
					return NAND_FAIL;
				}

				s_usLUT[usLBN] = i;	/* 更新LUT表 */
			}
		}
	}

	/* LUT建立完毕，检查是否合理 */
	for (i = 0; i < NAND_BLOCK_COUNT; i++)
	{
		if (s_usLUT[i] >= NAND_BLOCK_COUNT)
		{
			s_usValidDataBlockCount = i;
			break;
		}
	}
	if (s_usValidDataBlockCount < 100)
	{
		/* 错误： 最大的有效逻辑块号小于100。可能是没有格式化 */
		return NAND_FAIL;
	}
	for (; i < s_usValidDataBlockCount; i++)
	{
		if (s_usLUT[i] != 0xFFFF)
		{
			return NAND_FAIL;	/* 错误：LUT表逻辑块号存在跳跃现象，可能是没有格式化 */
		}
	}

	/* 重建LUT正常 */
	return NAND_OK;
}

/*
*********************************************************************************************************
*	函 数 名: NAND_AddrToPhyBlockNo
*	功能说明: 内存逻辑地址转换为物理块号
*	形    参:  _ulMemAddr：逻辑内存地址
*	返 回 值: 物理页号， 如果是 0xFFFFFFFF 则表示错误
*********************************************************************************************************
*/
static uint16_t NAND_AddrToPhyBlockNo(uint32_t _ulMemAddr)
{
	uint16_t usLBN;		/* 逻辑块号 */
	uint16_t usPBN;		/* 物理块号 */

	usLBN = _ulMemAddr / (NAND_BLOCK_SIZE * NAND_PAGE_SIZE);	/* 计算逻辑块号 */
	/* 如果逻辑块号大于有效的数据块个数则固定返回0xFFFF, 调用该函数的代码应该检查出这种错误 */
	if (usLBN >= s_usValidDataBlockCount)
	{
		return 0xFFFF;
	}
	/* 查询LUT表，获得物理块号 */
	usPBN = s_usLUT[usLBN];
	return usPBN;
}

/*
*********************************************************************************************************
*	函 数 名: NAND_LBNtoPBN
*	功能说明: 逻辑块号转换为物理块号
*	形    参: _uiLBN : 逻辑块号 Logic Block No
*	返 回 值: 物理块号， 如果是 0xFFFFFFFF 则表示错误
*********************************************************************************************************
*/
static uint16_t NAND_LBNtoPBN(uint32_t _uiLBN)
{
	uint16_t usPBN;		/* 物理块号 */

	/* 如果逻辑块号大于有效的数据块个数则固定返回0xFFFF, 调用该函数的代码应该检查出这种错误 */
	if (_uiLBN >= s_usValidDataBlockCount)
	{
		return 0xFFFF;
	}
	/* 查询LUT表，获得物理块号 */
	usPBN = s_usLUT[_uiLBN];
	return usPBN;
}

/*
*********************************************************************************************************
*	函 数 名: NAND_FindFreeBlock
*	功能说明: 从最后一个块开始，查找一个可用的块。A18必须相同
*	形    参: _ulSrcPageNo : 源页号
*	返 回 值: 块号，如果是0xFFFF表示失败
*********************************************************************************************************
*/
static uint16_t NAND_FindFreeBlock (uint32_t _ulSrcPageNo)
{
	uint16_t n;

	n = NAND_BLOCK_COUNT - 1;
	if (_ulSrcPageNo & 0x40)	/* 需要奇数块 */
	{
		if ((n & 0x01) == 0)
		{
			n--;
		}
	}
	else	/* 需要偶数块 */
	{
		if (n & 0x01)
		{
			n--;
		}
	}

	while (1)
	{
		if (NAND_IsFreeBlock(n))	/* 是空闲块 */
		{
			return n;
		}

		if (n < 2)
		{
			return 0xFFFF;	/* 没有找到空闲的块 */
		}
		n -= 2;
	}
}

/*
*********************************************************************************************************
*	函 数 名: NAND_IsBufOk
*	功能说明: 判断内存缓冲区的数据是否全部为指定值
*	形    参:  - _pBuf : 输入缓冲区
*			  - _ulLen : 缓冲区长度
*			  - __ucValue : 缓冲区每个单元的正确数值
*	返 回 值: 1 ：全部正确； 0 ：不正确
*********************************************************************************************************
*/
static uint8_t NAND_IsBufOk(uint8_t *_pBuf, uint32_t _ulLen, uint8_t _ucValue)
{
	uint32_t i;

	for (i = 0; i < _ulLen; i++)
	{
		if (_pBuf[i] != _ucValue)
		{
			return 0;
		}
	}

	return 1;
}

/*
*********************************************************************************************************
*	函 数 名: NAND_IsBadBlock
*	功能说明: 根据坏块标记检测NAND Flash指定的块是否坏块
*	形    参: _ulBlockNo ：块号 0 - 1023 （对于128M字节，2K Page的NAND Flash，有1024个块）
*	返 回 值: 0 ：该块可用； 1 ：该块是坏块
*********************************************************************************************************
*/
 uint8_t NAND_IsBadBlock(uint32_t _ulBlockNo)
{
	uint8_t ucFlag;

	/* 如果NAND Flash出厂前已经标注为坏块了，则就认为是坏块 */
	FSMC_NAND_ReadSpare(&ucFlag, _ulBlockNo * NAND_BLOCK_SIZE, BI_OFFSET, 1);
	if (ucFlag != 0xFF)
	{
		return 1;
	}

	FSMC_NAND_ReadSpare(&ucFlag, _ulBlockNo * NAND_BLOCK_SIZE + 1, BI_OFFSET, 1);
	if (ucFlag != 0xFF)
	{
		return 1;
	}
	return 0;	/* 是好块 */
}

/*
*********************************************************************************************************
*	函 数 名: NAND_IsFreeBlock
*	功能说明: 根据坏块标记和USED标志检测是否可用块
*	形    参: _ulBlockNo ：块号 0 - 1023 （对于128M字节，2K Page的NAND Flash，有1024个块）
*	返 回 值: 1 ：该块可用； 0 ：该块是坏块或者已占用
*********************************************************************************************************
*/
static uint8_t NAND_IsFreeBlock(uint32_t _ulBlockNo)
{
	uint8_t ucFlag;

	/* 如果NAND Flash出厂前已经标注为坏块了，则就认为是坏块 */
	if (NAND_IsBadBlock(_ulBlockNo))
	{
		return 0;
	}

	//FSMC_NAND_ReadPage(&ucFlag, _ulBlockNo * NAND_BLOCK_SIZE, USED_OFFSET, 1);  2014-05-03 bug
	FSMC_NAND_ReadSpare(&ucFlag, _ulBlockNo * NAND_BLOCK_SIZE, USED_OFFSET, 1);
	if (ucFlag == 0xFF)
	{
		return 1;
	}
	return 0;
}

/*
*********************************************************************************************************
*	函 数 名: NAND_ScanBlock
*	功能说明: 扫描测试NAND Flash指定的块
*			【扫描测试算法】
*			1) 第1个块（包括主数据区和备用数据区），擦除后检测是否全0xFF, 正确的话继续测试改块，否则该块
				是坏块,函数返回
*			2) 当前块写入全 0x00，然后读取检测，正确的话继续测试改块，否则退出
*			3) 重复第（2）步；如果循环次数达50次都没有发生错误，那么该块正常,函数返回，否则该块是坏块，
*				函数返回
*			【注意】
*			1) 该函数测试完毕后，会删除块内所有数据，即变为全0xFF;
*			2) 该函数除了测试主数据区外，也对备用数据区进行测试。
*			3) 擦写测试循环次数可以宏指定。#define BAD_BALOK_TEST_CYCLE 50
*	形    参:  _ulPageNo ：页号 0 - 65535 （对于128M字节，2K Page的NAND Flash，有1024个块）
*	返 回 值: NAND_OK ：该块可用； NAND_FAIL ：该块是坏块
*********************************************************************************************************
*/
uint8_t NAND_ScanBlock(uint32_t _ulBlockNo)
{
	uint32_t i, k;
	uint32_t ulPageNo;

	#if 0
	/* 如果NAND Flash出厂前已经标注为坏块了，则就认为是坏块 */
	if (NAND_IsBadBlock(_ulBlockNo))
	{
		return NAND_FAIL;
	}
	#endif

	/* 下面的代码将通过反复擦除、编程的方式来测试NAND Flash每个块的可靠性 */
	memset(s_ucTempBuf, 0x00, NAND_PAGE_TOTAL_SIZE);
	for (i = 0; i < BAD_BALOK_TEST_CYCLE; i++)
	{
		/* 第1步：擦除这个块 */
		if (FSMC_NAND_EraseBlock(_ulBlockNo) != NAND_READY)
		{
			return NAND_FAIL;
		}

		/* 第2步：读出块内每个page的数据，并判断是否全0xFF */
		ulPageNo = _ulBlockNo * NAND_BLOCK_SIZE;	/* 计算该块第1个页的页号 */
		for (k = 0; k < NAND_BLOCK_SIZE; k++)
		{
			/* 读出整页数据 */
			FSMC_NAND_ReadPage(s_ucTempBuf, ulPageNo, 0, NAND_PAGE_TOTAL_SIZE);

			/* 判断存储单元是不是全0xFF */
			if (NAND_IsBufOk(s_ucTempBuf, NAND_PAGE_TOTAL_SIZE, 0xFF) == 0)
			{
				return NAND_FAIL;
			}

			ulPageNo++;		/* 继续写下一个页 */
		}

		/* 第2步：写全0，并读回判断是否全0 */
		ulPageNo = _ulBlockNo * NAND_BLOCK_SIZE;	/* 计算该块第1个页的页号 */
		for (k = 0; k < NAND_BLOCK_SIZE; k++)
		{
			/* 填充buf[]缓冲区为全0,并写入NAND Flash */
			memset(s_ucTempBuf, 0x00, NAND_PAGE_TOTAL_SIZE);
			if (FSMC_NAND_WritePage(s_ucTempBuf, ulPageNo, 0, NAND_PAGE_TOTAL_SIZE) != NAND_OK)
			{
				return NAND_FAIL;
			}

			/* 读出整页数据, 判断存储单元是不是全0x00 */
			FSMC_NAND_ReadPage(s_ucTempBuf, ulPageNo, 0, NAND_PAGE_TOTAL_SIZE);
			if (NAND_IsBufOk(s_ucTempBuf, NAND_PAGE_TOTAL_SIZE, 0x00) == 0)
			{
				return NAND_FAIL;
			}

			ulPageNo++;		/* 继续一个页 */
		}
	}

	/* 最后一步：擦除整个块 */
	if (FSMC_NAND_EraseBlock(_ulBlockNo) != NAND_READY)
	{
		return NAND_FAIL;
	}
	ulPageNo = _ulBlockNo * NAND_BLOCK_SIZE;	/* 计算该块第1个页的页号 */
	for (k = 0; k < NAND_BLOCK_SIZE; k++)
	{
		/* 读出整页数据 */
		FSMC_NAND_ReadPage(s_ucTempBuf, ulPageNo, 0, NAND_PAGE_TOTAL_SIZE);

		/* 判断存储单元是不是全0xFF */
		if (NAND_IsBufOk(s_ucTempBuf, NAND_PAGE_TOTAL_SIZE, 0xFF) == 0)
		{
			return NAND_FAIL;
		}

		ulPageNo++;		/* 继续写下一个页 */
	}

	return NAND_OK;

}

/*
*********************************************************************************************************
*	函 数 名: NAND_MarkUsedBlock
*	功能说明: 标记NAND Flash指定的块为已用块
*	形    参: _ulBlockNo ：块号 0 - 1023 （对于128M字节，2K Page的NAND Flash，有1024个块）
*	返 回 值: NAND_OK:标记成功； NAND_FAIL：标记失败，上级软件应该进行坏块处理。
*********************************************************************************************************
*/
static uint8_t NAND_MarkUsedBlock(uint32_t _ulBlockNo)
{
	uint32_t ulPageNo;
	uint8_t ucFlag;

	/* 计算块的第1个页号 */
	ulPageNo = _ulBlockNo * NAND_BLOCK_SIZE;	/* 计算该块第1个页的页号 */

	/* 块内第1个page备用区的第USED_OFFSET个字节写入非0xFF数据表示已用块 */
	ucFlag = NAND_USED_BLOCK_FLAG;
	if (FSMC_NAND_WriteSpare(&ucFlag, ulPageNo, USED_OFFSET, 1) == NAND_FAIL)
	{
		/* 如果标记失败，则需要标注这个块为坏块 */
		return NAND_FAIL;
	}
	return NAND_OK;
}

/*
*********************************************************************************************************
*	函 数 名: NAND_MarkBadBlock
*	功能说明: 标记NAND Flash指定的块为坏块
*	形    参: _ulBlockNo ：块号 0 - 1023 （对于128M字节，2K Page的NAND Flash，有1024个块）
*	返 回 值: 固定NAND_OK
*********************************************************************************************************
*/
void NAND_MarkBadBlock(uint32_t _ulBlockNo)
{
	uint32_t ulPageNo;
	uint8_t ucFlag;

	printf_err("NAND_MarkBadBlock(%d)\r\n", _ulBlockNo);

	/* 计算块的第1个页号 */
	ulPageNo = _ulBlockNo * NAND_BLOCK_SIZE;	/* 计算该块第1个页的页号 */

	/* 块内第1个page备用区的第BI_OFFSET个字节写入非0xFF数据表示坏块 */
	ucFlag = NAND_BAD_BLOCK_FLAG;
	if (FSMC_NAND_WriteSpare(&ucFlag, ulPageNo, BI_OFFSET, 1) == NAND_FAIL)
	{
		/* 如果第1个页标记失败，则在第2个页标记 */
		FSMC_NAND_WriteSpare(&ucFlag, ulPageNo + 1, BI_OFFSET, 1);
	}
}

/*
*********************************************************************************************************
*	函 数 名: NAND_ScanAllBadBlock
*	功能说明: 扫描测试所有的BLOCK, 如果是坏块，则做坏块标记
*	形    参: _ulBlockNo ：块号 0 - 1023 （对于128M字节，2K Page的NAND Flash，有1024个块）
*	返 回 值: 固定NAND_OK
*********************************************************************************************************
*/
void NAND_ScanAllBadBlock(void)
{
	uint32_t i;
	
	for (i = 0; i < NAND_BLOCK_COUNT; i++)
	{
		if (NAND_ScanBlock(i) == NAND_OK)
		{
			printf("Scan Block %d (%d%%), Ok\r\n", i, i * 100 / NAND_BLOCK_COUNT);
		}
		else
		{
			printf("Scan Block %d (%d%%), Err\r\n", i, i * 100 / NAND_BLOCK_COUNT);
			NAND_MarkBadBlock(i);
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: NAND_Format
*	功能说明: NAND Flash格式化，擦除所有的数据，重建LUT
*	形    参:  无
*	返 回 值: NAND_OK : 成功； NAND_Fail ：失败（一般是坏块数量过多导致）
*********************************************************************************************************
*/
uint8_t NAND_Format(void)
{
	uint16_t i, n;
	uint16_t usGoodBlockCount;

	/* 擦除每个块 */
	usGoodBlockCount = 0;
	for (i = 0; i < NAND_BLOCK_COUNT; i++)
	{
		/* 如果是好块，则擦除 */
		if (!NAND_IsBadBlock(i))
		{
			FSMC_NAND_EraseBlock(i);
			usGoodBlockCount++;
		}
	}

	/* 如果好块的数量少于100，则NAND Flash报废 */
	if (usGoodBlockCount < 100)
	{
		return NAND_FAIL;
	}

	usGoodBlockCount = (usGoodBlockCount * 98) / 100;	/* 98%的好块用于存储数据 */

	/* 重新搜索一次 */
	n = 0; /* 统计已标注的好块 */
	for (i = 0; i < NAND_BLOCK_COUNT; i++)
	{
		if (!NAND_IsBadBlock(i))
		{
			/* 如果是好块，则在该块的第1个PAGE的LBN0 LBN1处写入n值 (前面已经执行了块擦除） */
			if (FSMC_NAND_WriteSpare((uint8_t *)&n, i * NAND_BLOCK_SIZE, LBN0_OFFSET, 2) != NAND_OK)
			{
				return NAND_FAIL;
			}
			n++;

			/* 计算并写入每个扇区的ECC值 （暂时未作）*/

			if (n == usGoodBlockCount)
			{
				break;
			}
		}
	}

	NAND_BuildLUT();	/* 初始化LUT表 */
	return NAND_OK;
}

/*
*********************************************************************************************************
*	函 数 名: NAND_FormatCapacity
*	功能说明: NAND Flash格式化后的有效容量. (最大支持4G)
*	形    参:  无
*	返 回 值: NAND_OK : 成功； NAND_Fail ：失败（一般是坏块数量过多导致）
*********************************************************************************************************
*/
uint32_t NAND_FormatCapacity(void)
{
	uint16_t usCount;

	/* 计算用于存储数据的数据块个数，按照总有效块数的98%来计算 */
	usCount = (s_usValidDataBlockCount * DATA_BLOCK_PERCENT) / 100;

	return (usCount * NAND_BLOCK_SIZE * NAND_PAGE_SIZE);
}

/*
*********************************************************************************************************
*	函 数 名: NAND_DispBadBlockInfo
*	功能说明: 通过串口打印出NAND Flash的坏块信息
*	形    参:  无
*	返 回 值: 无
*********************************************************************************************************
*/
void NAND_DispBadBlockInfo(void)
{
	uint32_t id;
	uint32_t i;
	uint32_t n;
	uint32_t used = 0;
	uint16_t bad_no[128];

	FSMC_NAND_Init();	/* 初始化FSMC */

	id = NAND_ReadID();

	printf("NAND Flash ID = 0x%04X, Type = ", id);
	if (id == HY27UF081G2A)
	{
		printf("HY27UF081G2A\r\n  1024 Blocks, 64 pages per block, 2048 + 64 bytes per page\r\n");
	}
	else if (id == K9F1G08U0A)
	{
		printf("K9F1G08U0A\r\n  1024 Blocks, 64 pages per block, 2048 + 64 bytes per page\r\n");
	}
	else if (id == K9F1G08U0B)
	{
		printf("K9F1G08U0B\r\n  1024 Blocks, 64 pages per block, 2048 + 64 bytes per page\r\n");
	}
	else if (id == H27U4G8F2DTR)
	{
		printf("H27U4G8F2DTR\r\n  4096 Blocks, 64 pages per block, 2048 + 64 bytes per page\r\n");
	}
	else if (id == H27U1G8F2BTR)
	{
		printf("H27U1G8F2BTR\r\n  1024 Blocks, 64 pages per block, 2048 + 64 bytes per page\r\n");
	}	
	else
	{
		printf("unkonow\r\n");
		return;
	}

	FSMC_NAND_Reset();

	printf("Block Info : 0 is OK, * is Bad, - is Used\r\n");
	n = 0;	/* 坏块统计 */
	used = 0; /* 已用块统计 */
	for (i = 0; i < NAND_BLOCK_COUNT; i++)
	{
		if (NAND_IsBadBlock(i))
		{
			if (n < sizeof(bad_no) / 2)
			{
				bad_no[n] = i;	/* 记录坏块号 */
			}
			printf("*");
			n++;
		}
		else
		{
			if (NAND_IsFreeBlock(i))
			{
				printf("0");
			}
			else
			{
				printf("-");	/* 已用块 */
				used++;
			}
		}

		if (((i + 1) % 8) == 0)
		{
			printf(" ");
		}

		if (((i + 1) % 64) == 0)
		{
			printf("\r\n");
		}
	}

	if (id == H27U4G8F2DTR)
	{
		printf("Bad Block Count = %d  ( < 80 is OK), Used = %d \r\n", n, used);
	}
	else
	{
		printf("Bad Block Count = %d\r\n", n);
	}
	
	/* 打印坏块序号 */
	if (n > 0)
	{
		for (i = 0; i < n; i++)
		{
			printf("%4d ",  bad_no[i]);
		}
		
		printf("\r\n\r\n");
	}
}

/*
*********************************************************************************************************
*	函 数 名: NAND_GetBlockInfo
*	功能说明: 获得NAND 坏块、已用块信息
*	形    参: 无
*	返 回 值: 1 表示成功识别， 0 表示识别失败
*********************************************************************************************************
*/
uint8_t NAND_GetBlockInfo(NAND_BLOCK_INFO_T *_pInfo)
{
	uint32_t id;
	uint32_t i;
	uint32_t n;
	uint32_t used = 0;
	uint32_t free = 0;

	FSMC_NAND_Init();	/* 初始化FSMC */

	id = NAND_ReadID();
	
	_pInfo->ChipID = id;
	if (id == HY27UF081G2A)
	{
		_pInfo->ChipID = HY27UF081G2A;
		strcpy(_pInfo->ChipName, "HY27UF081G2A");
	}
	else if (id == K9F1G08U0A)
	{
		_pInfo->ChipID = K9F1G08U0A;
		strcpy(_pInfo->ChipName, "K9F1G08U0A");
	}
	else if (id == K9F1G08U0B)
	{
		_pInfo->ChipID = K9F1G08U0B;
		strcpy(_pInfo->ChipName, "K9F1G08U0B");
	}
	else if (id == H27U4G8F2DTR)
	{
		_pInfo->ChipID = H27U4G8F2DTR;
		strcpy(_pInfo->ChipName, "H27U4G8F2DTR");
	}
	else if (id == H27U1G8F2BTR)
	{
		_pInfo->ChipID = H27U1G8F2BTR;
		strcpy(_pInfo->ChipName, "H27U1G8F2BTR");
	}	
	else
	{		
		return 0;
	}

	FSMC_NAND_Reset();

	n = 0;	/* 坏块统计 */
	used = 0; /* 已用块统计 */
	free = 0; /* 未用的好块 */
	for (i = 0; i < NAND_BLOCK_COUNT; i++)
	{
		if (NAND_IsBadBlock(i))
		{
			n++;			/* 记录坏块 */
		}
		else
		{
			if (NAND_IsFreeBlock(i))
			{
				free++;
			}
			else
			{	
				used++;		/* 已用块 */
			}
		}
	}

	_pInfo->Bad = n;
	_pInfo->Used = used;
	_pInfo->Free = free;
	return 1;
}

/*
*********************************************************************************************************
*	函 数 名: NAND_DispPhyPageData
*	功能说明: 通过串口打印出指定页的数据（2048+64）
*	形    参: _uiPhyPageNo ： 物理页号
*	返 回 值: 无
*********************************************************************************************************
*/
void NAND_DispPhyPageData(uint32_t _uiPhyPageNo)
{
	uint32_t i, n;
	uint32_t ulBlockNo;
	uint16_t usOffsetPageNo;

	ulBlockNo = _uiPhyPageNo / NAND_BLOCK_SIZE;		/* 根据物理页号反推块号 */
	usOffsetPageNo = _uiPhyPageNo % NAND_BLOCK_SIZE;	/* 根据物理页号计算物理页号在块内偏移页号 */

	if (NAND_OK != FSMC_NAND_ReadPage(s_ucTempBuf, _uiPhyPageNo, 0, NAND_PAGE_TOTAL_SIZE))
	{
		printf("FSMC_NAND_ReadPage Failed() \r\n");
		return;
	}

	printf("Block = %d, Page = %d\r\n", ulBlockNo, usOffsetPageNo);

	/* 打印前面 2048字节数据，每512字节空一行 */
	for (n = 0; n < 4; n++)
	{
		for (i = 0; i < 512; i++)
		{
			printf(" %02X", s_ucTempBuf[i + n * 512]);

			if ((i & 31) == 31)
			{
				printf("\r\n");	/* 每行显示32字节数据 */
			}
			else if ((i & 31) == 15)
			{
				printf(" - ");
			}
		}
		printf("\r\n");
	}

	/* 打印前面 2048字节数据，每512字节空一行 */
	for (i = 0; i < 64; i++)
	{
		printf(" %02X", s_ucTempBuf[i + 2048]);

		if ((i & 15) == 15)
		{
			printf("\r\n");	/* 每行显示32字节数据 */
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: NAND_DispLogicPageData
*	功能说明: 通过串口打印出指定页的数据（2048+64）
*	形    参: _uiLogicPageNo ： 逻辑页号
*	返 回 值: 无
*********************************************************************************************************
*/
void NAND_DispLogicPageData(uint32_t _uiLogicPageNo)
{
	uint32_t uiPhyPageNo;
	uint16_t usLBN;	/* 逻辑块号 */
	uint16_t usPBN;	/* 物理块号 */

	usLBN = _uiLogicPageNo / NAND_BLOCK_SIZE;
	usPBN = NAND_LBNtoPBN(usLBN);	/* 查询LUT表获得物理块号 */
	if (usPBN >= NAND_BLOCK_COUNT)
	{
		/* 没有格式化，usPBN = 0xFFFF */
		return;
	}

	printf("LogicBlock = %d, PhyBlock = %d\r\n", _uiLogicPageNo, usPBN);

	/* 计算物理页号 */
	uiPhyPageNo = usPBN * NAND_BLOCK_SIZE + _uiLogicPageNo % NAND_BLOCK_SIZE;
	NAND_DispPhyPageData(uiPhyPageNo);	/* 显示指定页数据 */
}

/*
*********************************************************************************************************
*	函 数 名: NAND_ReadONFI
*	功能说明: 读NAND Flash的ONFI信息,  针对 H27U4G8F2DTR
*	形    参: _pBuf 存放结果的缓冲区, 大小4字节。应该固定返回 "ONFI" 字符串
*	返 回 值: 无
*********************************************************************************************************
*/
void NAND_ReadONFI(uint8_t *_pBuf)
{
	uint16_t i;

	/* 发送命令 Command to the command area */
	NAND_CMD_AREA = 0x90;
	NAND_ADDR_AREA = 0x20;

	/* 读数据到缓冲区pBuffer */
	for(i = 0; i < 256; i++)
	{
		_pBuf[i] = NAND_DATA_AREA;
	}
}

/*
*********************************************************************************************************
*	函 数 名: NAND_ReadParamPage
*	功能说明: Read Parameter Page,  针对 H27U4G8F2DTR
*	形    参: _pData 存放结果的缓冲区。
*	返 回 值: 无
*********************************************************************************************************
*/
void NAND_ReadParamPage(PARAM_PAGE_T *_pData)
{
	uint16_t i;
	uint8_t *_pBuf = (uint8_t *)_pData;

	/* 发送命令 Command to the command area */
	NAND_CMD_AREA = 0xEC;
	NAND_ADDR_AREA = 0x00;

	 /* 必须等待，否则读出数据异常, 此处应该判断超时 */
	for (i = 0; i < 20; i++);
	while( GPIO_ReadInputDataBit(GPIOG, GPIO_Pin_6) == 0);

	/* 读数据到缓冲区pBuffer */
	for(i = 0; i < 256; i++)
	{
		_pBuf[i] = NAND_DATA_AREA;
	}

}

/*
*********************************************************************************************************
*	函 数 名: NAND_DispParamPage
*	功能说明: 通过串口打印 Parameter Page Data Structure Definition (参数页数据结构) 针对 H27U4G8F2DTR
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void NAND_DispParamPage(void)
{
	PARAM_PAGE_T tPage;
	char buf[32];

	FSMC_NAND_Reset();

	NAND_ReadParamPage(&tPage);

#if 0
	uint8_t Sign[4];		/* = "ONFI" */
	uint16_t Revision; 		/* Bit1 = 1 表示支持 ONFI Ver 1.0 */
	uint16_t Features;		/* */
	uint16_t OptionalCommands;
#endif

	printf("\r\n");
	printf("Read Parameter Page Data :\r\n");
	printf("Sign = %c%c%c%c\r\n", tPage.Sign[0], tPage.Sign[1], tPage.Sign[2], tPage.Sign[3]);
	printf("Revision = %04X\r\n", tPage.Revision);
	printf("Features = %04X\r\n", tPage.Features);
	printf("OptionalCommands = %04X\r\n", tPage.OptionalCommands);

	/* Manufacturer information block */
	memcpy(buf, tPage.Manufacturer, 12);
	buf[12] = 0;
	printf("Manufacturer = %s\r\n", buf);	/* 制造商 */

	memcpy(buf, tPage.Model, 20);
	buf[20] = 0;
	printf("Model = %s\r\n", buf);	/* 型号 */

	printf("JEDEC_ID = %02X\r\n", tPage.JEDEC_ID);	/* AD */
	printf("DateCode = %04X\r\n", tPage.DateCode);

#if 0
	/* Memory organization block */
	uint32_t PageDataSize;
	uint16_t PageSpareSize;
	uint32_t PartialPageDataSize;
	uint16_t PartialPageSpareSize;
	uint32_t BlockSize;
	uint32_t LogicalUnitSize;
	uint8_t LogicalUnitNumber;
	uint8_t AddressCycles;
	uint8_t CellBits;
	uint16_t BadBlockMax;
	uint16_t BlockEndurance;
	uint8_t ValidBlocksBegin;	/* 最前面保证有效的块个数 */
	uint16_t BlockEndurance2;	/* Block endurance for guaranteed valid blocks */
	uint8_t  ProgramsPerPage;	/* Number of programs per page */
	uint8_t PartialProgram;
	uint8_t ECCcorrectBits;
	uint8_t InterleavedAddrBits;	/* 交错的地址位 */
	uint8_t InterleavedOperaton;
	uint8_t Rsv3[13];
#endif

	printf("\r\n");

}


/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
