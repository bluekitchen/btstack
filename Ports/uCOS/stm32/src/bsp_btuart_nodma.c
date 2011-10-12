/*=============================================================================
* (C) Copyright Albis Technologies Ltd 2011
*==============================================================================
*                STM32 Example Code
*==============================================================================
* File name:     bsp_btuart_nodma.c
*
* Notes:         STM32 Bluetooth UART driver without DMA.
*============================================================================*/

#include "bsp.h"
#include "bsp_btuart.h"
#include "bsp_debug.h"

#include "stm32f10x_gpio.h"
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

/* UART receiver without callback. */
#define RX_QUEUE_BUFFER_SIZE 512
static OS_EVENT *rxQueue = 0;
static void * rxQueueBuffer[RX_QUEUE_BUFFER_SIZE];

/* UART receiver with callback. */
static RxCallbackFunc rxCallbackFunc = 0;

//=============================================================================
//=============================================================================
static void isr_bluetooth_usart(void)
{
    INT8U err;
    CPU_INT32U rxData;

    if(USART_GetITStatus(usartInst, USART_IT_RXNE) != RESET)
    {
        rxData = USART_ReceiveData(usartInst) & 0xff;

        if(rxCallbackFunc)
        {
            rxCallbackFunc((unsigned char *)&rxData, 1);
        }
        else
        {
            err = OSQPost(rxQueue, (void *)rxData);
            if(err == USE_OS_NO_ERROR)
            {
                // Ok.
            }
            else if(err == USE_OS_Q_FULL)
            {
                printos("Tossing BT data!\r\n");
            }
            else
            {
                SYS_ERROR(1);
            }
        }

        USART_ClearITPendingBit(usartInst, USART_IT_RXNE);
    }
    else
    {
        SYS_ERROR(1);
    }
}

/*=============================================================================
=============================================================================*/
void BSP_BTUART_Initialise(void)
{
    GPIO_InitTypeDef gpioInit;
    USART_ClockInitTypeDef usartClkInit;

    /* UART instance in use. */
    usartInst = BTUART_INST_IN_USE;

    /* Create Rx queue. */
    rxQueue = OSQCreate(rxQueueBuffer, RX_QUEUE_BUFFER_SIZE);
    SYS_ASSERT(rxQueue);

    USART_ClockStructInit(&usartClkInit);
    usartClkInit.USART_Clock = USART_Clock_Disable;
    usartClkInit.USART_CPOL = USART_CPOL_Low;
    usartClkInit.USART_CPHA = USART_CPHA_2Edge;
    usartClkInit.USART_LastBit = USART_LastBit_Disable;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
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

    BSP_IntVectSet(BSP_INT_ID_USART2, isr_bluetooth_usart);
    BSP_IntEn(BSP_INT_ID_USART2);
    USART_ITConfig(usartInst, USART_IT_RXNE, ENABLE);

    /* GPO for BT chipset reset. */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
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
    unsigned long i;

    for(i = 0; i < count; ++i)
    {
        while(USART_GetFlagStatus(usartInst, USART_FLAG_TXE) != SET);
        USART_SendData(usartInst, buffer[i]);
    }
}

/*=============================================================================
=============================================================================*/
void BSP_BTUART_AnounceDmaReceiverSize(unsigned long count)
{
    /* Nothing to do: no DMA. */
}

/*=============================================================================
=============================================================================*/
void BSP_BTUART_Receive(unsigned char *buffer,
                        unsigned long maxCount,
                        unsigned long *rxCount,
                        unsigned long timeout)
{
    INT8U err;
    void *event = 0;
    unsigned long rxCounter = 0;
    unsigned short timeoutTicks = MSEC_TO_TICKS(timeout);

    for(;;)
    {
        event = OSQPend(rxQueue, timeoutTicks, &err);

        if(err == USE_OS_NO_ERROR)
        {
            buffer[rxCounter++] = (unsigned char)(unsigned long)event;

            if(rxCounter >= maxCount)
            {
                // Received expected number of bytes.
                break;
            }
        }
        else if(err == USE_OS_TIMEOUT)
        {
            break;
        }
        else
        {
            SYS_ERROR(1);
        }
    }

    if(rxCount)
    {
        *rxCount = rxCounter;
    }
}

/*=============================================================================
=============================================================================*/
void BSP_BTUART_DrainReceiver(void)
{
    INT8U err;

    do
    {
        OSQPend(rxQueue, 1, &err);
    } while(err == OS_ERR_NONE);
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
