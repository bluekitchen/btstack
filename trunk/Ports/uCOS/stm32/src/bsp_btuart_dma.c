/*=============================================================================
* (C) Copyright Albis Technologies Ltd 2011
*==============================================================================
*                STM32 Example Code
*==============================================================================
* File name:     bsp_btuart_dma.c
*
* Notes:         STM32 Bluetooth UART driver with DMA.
*============================================================================*/

#include "bsp.h"
#include "bsp_btuart.h"
#include "bsp_debug.h"

#include "stm32f10x_dma.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_usart.h"

DEFINE_THIS_FILE

/* OS version depending macros. */
#define USE_OS_NO_ERROR     OS_NO_ERR
#define USE_OS_TIMEOUT      OS_TIMEOUT
#define USE_OS_Q_FULL       OS_Q_FULL

/* PD2 is BT chipset reset. */
#define BT_RESET_PIN        (1UL << 2)

/* BT UART is USART2 (remapped). */
#define BTUART_INST_IN_USE  USART2

/* Convert msec to OS ticks. */
#define MSEC_TO_TICKS(ms) \
    ((ms > 0u) ? (((ms * OS_TICKS_PER_SEC) + 1000u - 1u) / 1000u) : 0u)

/* USART instance in use. */
static USART_TypeDef *usartInst = 0;

/* UART receiver with callback. */
static RxCallbackFunc rxCallbackFunc = 0;

/* DMA channels for Rx/Tx. */
static OS_EVENT *dmaRxComplete = 0;
static OS_EVENT *dmaTxComplete = 0;
static DMA_Channel_TypeDef *dmaRx;
static DMA_Channel_TypeDef *dmaTx;
static unsigned long dmaRxDataLength;
static unsigned char dmaRxData[1024];

/*=============================================================================
=============================================================================*/
static void isr_usart_rx_dma(void)
{
    INT8U err;

    DMA_ClearITPendingBit(DMA1_IT_TC6);
    DMA_Cmd(dmaRx, DISABLE);

    if(rxCallbackFunc)
    {
        rxCallbackFunc(dmaRxData, dmaRxDataLength);
    }
    else
    {
        err = OSSemPost(dmaRxComplete);
        SYS_ASSERT(err == USE_OS_NO_ERROR);
    }
}

/*=============================================================================
=============================================================================*/
static void isr_usart_tx_dma(void)
{
    INT8U err;

    DMA_ClearITPendingBit(DMA1_IT_TC7);

    err = OSSemPost(dmaTxComplete);
    SYS_ASSERT(err == USE_OS_NO_ERROR);
}

/*=============================================================================
=============================================================================*/
void BSP_BTUART_Initialise(void)
{
    DMA_InitTypeDef dmaInit;
    GPIO_InitTypeDef gpioInit;
    NVIC_InitTypeDef nvicInit;
    USART_ClockInitTypeDef usartClkInit;

    /* UART instance in use. */
    usartInst = BTUART_INST_IN_USE;

    /* DMA Rx/Tx complete. */
    dmaRxComplete = OSSemCreate(0);
    SYS_ASSERT(dmaRxComplete);
    dmaTxComplete = OSSemCreate(0);
    SYS_ASSERT(dmaTxComplete);

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);

    /* Common DMA channel settings. */
    DMA_StructInit(&dmaInit);
    dmaInit.DMA_PeripheralBaseAddr = (unsigned long)&usartInst->DR;
    dmaInit.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dmaInit.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dmaInit.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dmaInit.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    dmaInit.DMA_Mode = DMA_Mode_Normal;
    dmaInit.DMA_M2M = DMA_M2M_Disable;
    dmaInit.DMA_BufferSize = 1;

    /* Init DMA Tx channel MEM -> USART. */
    dmaTx = DMA1_Channel7;
    dmaInit.DMA_Priority = DMA_Priority_Low;
    dmaInit.DMA_DIR = DMA_DIR_PeripheralDST;
    BSP_IntVectSet(BSP_INT_ID_DMA1_CH7, isr_usart_tx_dma);
    DMA_Init(dmaTx, &dmaInit);
    DMA_ITConfig(dmaTx, DMA_IT_TC, ENABLE);
    USART_DMACmd(usartInst, USART_DMAReq_Tx, ENABLE);

    /* Init DMA Rx channel USART -> MEM. */
    dmaRx = DMA1_Channel6;
    dmaInit.DMA_Priority = DMA_Priority_Medium;
    dmaInit.DMA_DIR = DMA_DIR_PeripheralSRC;
    BSP_IntVectSet(BSP_INT_ID_DMA1_CH6, isr_usart_rx_dma);
    DMA_Init(dmaRx, &dmaInit);
    DMA_ITConfig(dmaRx, DMA_IT_TC, ENABLE);
    USART_DMACmd(usartInst, USART_DMAReq_Rx, ENABLE);

    nvicInit.NVIC_IRQChannelPreemptionPriority = 0;
    nvicInit.NVIC_IRQChannelSubPriority = 0;
    nvicInit.NVIC_IRQChannelCmd = ENABLE;

    nvicInit.NVIC_IRQChannel = DMA1_Channel6_IRQn;
    NVIC_Init(&nvicInit);

    nvicInit.NVIC_IRQChannel = DMA1_Channel7_IRQn;
    NVIC_Init(&nvicInit);

    USART_ClockStructInit(&usartClkInit);
    usartClkInit.USART_Clock = USART_Clock_Disable;
    usartClkInit.USART_CPOL = USART_CPOL_Low;
    usartClkInit.USART_CPHA = USART_CPHA_2Edge;
    usartClkInit.USART_LastBit = USART_LastBit_Disable;

    BSP_PeriphEn(BSP_PERIPH_ID_USART2);

    BSP_PeriphEn(BSP_PERIPH_ID_IOPD);
    BSP_PeriphEn(BSP_PERIPH_ID_AFIO);
    GPIO_PinRemapConfig(GPIO_Remap_USART2, ENABLE);

    /* Configure GPIOD.5 (TX) as push-pull. */
    gpioInit.GPIO_Pin = GPIO_Pin_5;
    gpioInit.GPIO_Speed = GPIO_Speed_50MHz;
    gpioInit.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOD, &gpioInit);

    /* Configure GPIOD.4 (RTS) as push-pull. */
    gpioInit.GPIO_Pin = GPIO_Pin_4;
    gpioInit.GPIO_Speed = GPIO_Speed_50MHz;
    gpioInit.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOD, &gpioInit);

    /* Configure GPIOD.6 (RX) as input floating. */
    gpioInit.GPIO_Pin = GPIO_Pin_6;
    gpioInit.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOD, &gpioInit);

    /* Configure GPIOD.3 (CTS) as input floating. */
    gpioInit.GPIO_Pin = GPIO_Pin_3;
    gpioInit.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOD, &gpioInit);

    BSP_BTUART_SetBaudrate(115200);
    USART_ClockInit(usartInst, &usartClkInit);
    USART_Cmd(usartInst, ENABLE);

    /* GPO for BT chipset reset. */
    gpioInit.GPIO_Pin = BT_RESET_PIN;
    gpioInit.GPIO_Speed = GPIO_Speed_50MHz;
    gpioInit.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOD, &gpioInit);

    BSP_BTUART_ActivateReset();
}

/*=============================================================================
=============================================================================*/
void BSP_BTUART_SetBaudrate(unsigned long baudrate)
{
    USART_InitTypeDef usartInit;

    USART_StructInit(&usartInit);
    usartInit.USART_BaudRate = baudrate / 2;
    usartInit.USART_WordLength = USART_WordLength_8b;
    usartInit.USART_StopBits = USART_StopBits_1;
    usartInit.USART_Parity = USART_Parity_No;
    usartInit.USART_HardwareFlowControl = USART_HardwareFlowControl_RTS_CTS;
    usartInit.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(usartInst, &usartInit);
}

/*=============================================================================
=============================================================================*/
void BSP_BTUART_ActivateReset(void)
{
    GPIO_ResetBits(GPIOD, BT_RESET_PIN);
}

/*=============================================================================
=============================================================================*/
void BSP_BTUART_DeactivateReset(void)
{
    GPIO_SetBits(GPIOD, BT_RESET_PIN);
}

/*=============================================================================
=============================================================================*/
void BSP_BTUART_Transmit(unsigned char *buffer, unsigned long count)
{
    INT8U err;

    dmaTx->CMAR = (unsigned long)buffer;
    dmaTx->CNDTR = count;

    DMA_Cmd(dmaTx, ENABLE);

    OSSemPend(dmaTxComplete, MSEC_TO_TICKS(2000), &err);
    if(err == USE_OS_TIMEOUT)
    {
        DMA_Cmd(dmaTx, DISABLE);
        return;
    }
    SYS_ASSERT(err == USE_OS_NO_ERROR);

    DMA_Cmd(dmaTx, DISABLE);
}

/*=============================================================================
=============================================================================*/
void BSP_BTUART_AnounceDmaReceiverSize(unsigned long count)
{
    dmaRx->CMAR = (unsigned long)(&dmaRxData[0]);
    dmaRx->CNDTR = count;
    dmaRxDataLength = count;

    DMA_Cmd(dmaRx, ENABLE);
}

/*=============================================================================
=============================================================================*/
void BSP_BTUART_Receive(unsigned char *buffer,
                        unsigned long maxCount,
                        unsigned long *rxCount,
                        unsigned long timeout)
{
    INT8U err;
    unsigned short timeoutTicks = MSEC_TO_TICKS(timeout);

    rxCallbackFunc = 0;

    BSP_BTUART_AnounceDmaReceiverSize(maxCount);

    OSSemPend(dmaRxComplete, timeoutTicks, &err);
    if(err == USE_OS_TIMEOUT)
    {
        *rxCount = 0;
        return;
    }
    SYS_ASSERT(err == USE_OS_NO_ERROR);

    Mem_Copy(buffer, dmaRxData, dmaRxDataLength);
    *rxCount = dmaRxDataLength;
}

/*=============================================================================
=============================================================================*/
void BSP_BTUART_DrainReceiver(void)
{
    /* Nothing to do. */
}

/*=============================================================================
=============================================================================*/
void BSP_BTUART_EnableRxCallback(RxCallbackFunc callbackFunc)
{
    rxCallbackFunc = callbackFunc;
}

/*=============================================================================
=============================================================================*/
void BSP_BTUART_DisableRxCallback(void)
{
    rxCallbackFunc = 0;
}
