/*=============================================================================
* (C) Copyright Albis Technologies Ltd 2011
*==============================================================================
*                STM32 Example Code
*==============================================================================
* File name:     bsp_btuart.h
*
* Notes:         STM32 Bluetooth UART driver.
*============================================================================*/

#ifndef BSP_BTUART_H
#define BSP_BTUART_H

/*=============================================================================
*  Prototype of receiver callback function.
=============================================================================*/
typedef void(* RxCallbackFunc)(unsigned char *data, unsigned long size);

/*=============================================================================
*  PURPOSE:      Initialising Bluetooth UART.
*
*  PARAMETERS:
*                -
*
*  RETURN VALUE:
*                -
*
*  COMMENTS:     -
=============================================================================*/
void BSP_BTUART_Initialise(void);

/*=============================================================================
*  PURPOSE:      Setting Bluetooth UART baud rate.
*
*  PARAMETERS:
*                baudrate - baud rate to set.
*
*  RETURN VALUE:
*                -
*
*  COMMENTS:     -
=============================================================================*/
void BSP_BTUART_SetBaudrate(unsigned long baudrate);

/*=============================================================================
*  PURPOSE:      Activating BT chip reset.
*
*  PARAMETERS:
*                -
*
*  RETURN VALUE:
*                -
*
*  COMMENTS:     -
=============================================================================*/
void BSP_BTUART_ActivateReset(void);

/*=============================================================================
*  PURPOSE:      Deactivating BT chip reset.
*
*  PARAMETERS:
*                -
*
*  RETURN VALUE:
*                -
*
*  COMMENTS:     -
=============================================================================*/
void BSP_BTUART_DeactivateReset(void);

/*=============================================================================
*  PURPOSE:      Sending on Bluetooth UART.
*
*  PARAMETERS:
*                buffer - pointer to buffer to send.
*                count - number of bytes to send.
*
*  RETURN VALUE:
*                -
*
*  COMMENTS:     -
=============================================================================*/
void BSP_BTUART_Transmit(unsigned char *buffer, unsigned long count);

/*=============================================================================
*  PURPOSE:      Anouncing DMA receiver size.
*
*  PARAMETERS:
*                count - number of bytes to receive with DMA.
*
*  RETURN VALUE:
*                -
*
*  COMMENTS:     -
=============================================================================*/
void BSP_BTUART_AnounceDmaReceiverSize(unsigned long count);

/*=============================================================================
*  PURPOSE:      Receiving on Bluetooth UART without receiver callback.
*
*  PARAMETERS:
*                buffer - pointer to buffer for received bytes.
*                maxCount - maximal number of bytes buffer can hold.
*                rxCount - output: pointer to number of received bytes.
*                timeout - timeout in msec, zero = wait forever.
*
*  RETURN VALUE:
*                -
*
*  COMMENTS:     -
=============================================================================*/
void BSP_BTUART_Receive(unsigned char *buffer,
                        unsigned long maxCount,
                        unsigned long *rxCount,
                        unsigned long timeout);

/*=============================================================================
*  PURPOSE:      Draining bytes already received.
*
*  PARAMETERS:
*                -
*
*  RETURN VALUE:
*                -
*
*  COMMENTS:     -
=============================================================================*/
void BSP_BTUART_DrainReceiver(void);

/*=============================================================================
*  PURPOSE:      Enabling Bluetooth receiver callback.
*
*  PARAMETERS:
*                callbackFunc - receiver callback function.
*
*  RETURN VALUE:
*                -
*
*  COMMENTS:     -
=============================================================================*/
void BSP_BTUART_EnableRxCallback(RxCallbackFunc callbackFunc);

/*=============================================================================
*  PURPOSE:      Disabling Bluetooth receiver callback.
*
*  PARAMETERS:
*                -
*
*  RETURN VALUE:
*                -
*
*  COMMENTS:     -
=============================================================================*/
void BSP_BTUART_DisableRxCallback(void);

#endif /* BSP_BTUART_H */
