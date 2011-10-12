/*
 * Copyright (C) 2011 by Matthias Ringwald
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 *  hci_transport_h4_ucos.c
 *
 *  @brief BTstack serial H4 transport for uC/OS
 *
 *  Created by Albis Technologies.
 */

#include <btstack/port_ucos.h>

#include "hci.h"
#include "hci_transport.h"
#include "hci_dump.h"
#include "run_loop_ucos.h"

#include "bsp_btuart.h"

DEFINE_THIS_FILE

typedef enum
{
    H4_W4_PACKET_TYPE,
    H4_W4_EVENT_HEADER,
    H4_W4_ACL_HEADER,
    H4_W4_PAYLOAD,
    H4_W4_PICKUP
} H4RxState;

static int h4_reader_process(struct data_source *ds);
static void h4_rx_data(unsigned char *data, unsigned long size);
static void dummyHandler(uint8_t packetType, uint8_t *packet, uint16_t size);
static void (*incomingPacketHandler)(uint8_t a,
                                     uint8_t *b,
                                     uint16_t c) = dummyHandler;
static struct
{
    data_source_t dataSource;
    hci_transport_t transport;
} hciTransportH4;

/*=============================================================================
*  H4 receiver.
*============================================================================*/
#define NOF_RX_BUFFERS                                                      20
#define RX_BUFFER_SIZE                                                    1020

typedef struct
{
    unsigned char data[RX_BUFFER_SIZE];
    unsigned long nofBytes;
} ReceiveBuffer;

static struct
{
    /* H4 Rx state and remaining number of Bytes in this state. */
    H4RxState state;
    unsigned long remainingInState;

    /* H4 packet ring buffer and current buffer in use for Rx data. */
    ReceiveBuffer buffers[NOF_RX_BUFFERS];
    ReceiveBuffer *currBuffer;
    volatile unsigned long enqueueIdx;
    volatile unsigned long dequeueIdx;
    volatile unsigned long nextEnqueueIdx;
}rx;

/*=============================================================================
=============================================================================*/
static void advanceToNextBuffer(void)
{
    rx.nextEnqueueIdx = rx.enqueueIdx + 1;
    if(rx.nextEnqueueIdx >= NOF_RX_BUFFERS)
    {
        rx.nextEnqueueIdx = 0;
    }
    if(rx.nextEnqueueIdx == rx.dequeueIdx)
    {
        /* The ring buffer is full. */
        SYS_ERROR(1);
    }

    /* Prepare current buffer for receiving. */
    rx.currBuffer = &rx.buffers[rx.enqueueIdx];
    rx.currBuffer->nofBytes = 0;
}

/*=============================================================================
=============================================================================*/
static int h4_open(void *config)
{
    /* Initialise H4 receiver. */
    rx.enqueueIdx = 0;
    rx.dequeueIdx = 0;
    advanceToNextBuffer();

    BSP_BTUART_EnableRxCallback(h4_rx_data);

    /* Prepare for 1st H4 packet type ID. */
    rx.state = H4_W4_PACKET_TYPE;
    rx.remainingInState = 1;
    BSP_BTUART_AnounceDmaReceiverSize(rx.remainingInState);

    hciTransportH4.dataSource.process = h4_reader_process;
    run_loop_add_data_source(&hciTransportH4.dataSource);

    return 0;
}

/*=============================================================================
=============================================================================*/
static int h4_close(void *config)
{
    BSP_BTUART_DisableRxCallback();
    return 0;
}

/*=============================================================================
=============================================================================*/
static int h4_send_packet(uint8_t packetType, uint8_t *packet, int size)
{
    hci_dump_packet(packetType, 0, packet, size);

    BSP_BTUART_Transmit(&packetType, 1);
    BSP_BTUART_Transmit(packet, size);

    return 0;
}

/*=============================================================================
=============================================================================*/
static void h4_register_packet_handler(void (*handler)(uint8_t a,
                                                       uint8_t *b,
                                                       uint16_t c))
{
    incomingPacketHandler = handler;
}

/*=============================================================================
=============================================================================*/
static void h4_rx_fsm(void)
{
    switch(rx.state)
    {
        case H4_W4_PACKET_TYPE:
            if(rx.currBuffer->data[0] == HCI_EVENT_PACKET)
            {
                rx.remainingInState = HCI_EVENT_PKT_HDR;
                rx.state = H4_W4_EVENT_HEADER;
            }
            else if(rx.currBuffer->data[0] == HCI_ACL_DATA_PACKET)
            {
                rx.remainingInState = HCI_ACL_DATA_PKT_HDR;
                rx.state = H4_W4_ACL_HEADER;
            }
            else
            {
                rx.currBuffer->nofBytes = 0;
                rx.remainingInState = 1;
            }
            break;

        case H4_W4_EVENT_HEADER:
            rx.remainingInState = rx.currBuffer->data[2];
            rx.state = H4_W4_PAYLOAD;
            break;

        case H4_W4_ACL_HEADER:
            rx.remainingInState = READ_BT_16(rx.currBuffer->data, 3);
            rx.state = H4_W4_PAYLOAD;
            break;

        case H4_W4_PAYLOAD:
            rx.state = H4_W4_PICKUP;
            break;

        default:
            break;
    }

    if(rx.remainingInState)
    {
        BSP_BTUART_AnounceDmaReceiverSize(rx.remainingInState);
    }
}

/*=============================================================================
=============================================================================*/
static int h4_reader_process(struct data_source *ds)
{
    unsigned long pickUpSize;
    unsigned char *pickUpBuffer;
    unsigned long nextDequeueIdx;

    pickUpSize = rx.buffers[rx.dequeueIdx].nofBytes - 1;
    pickUpBuffer = rx.buffers[rx.dequeueIdx].data;

    /* Handle complete incoming HCI packet. */
    SYS_ASSERT(pickUpSize >= 2);

    hci_dump_packet(*pickUpBuffer, 1, pickUpBuffer + 1, pickUpSize);
    incomingPacketHandler(*pickUpBuffer, pickUpBuffer + 1, pickUpSize);

    nextDequeueIdx = rx.dequeueIdx + 1;
    if(nextDequeueIdx >= NOF_RX_BUFFERS)
    {
        nextDequeueIdx = 0;
    }
    rx.dequeueIdx = nextDequeueIdx;

    return 0;
}

/*=============================================================================
=============================================================================*/
static void h4_rx_data(unsigned char *data, unsigned long size)
{
    unsigned long i;

    SYS_ASSERT(size + rx.currBuffer->nofBytes <= RX_BUFFER_SIZE);
    SYS_ASSERT(size <= rx.remainingInState);

    /* Copy from DMA buffer to Rx buffer (no memcpy() or Mem_Copy()). */
    for(i = 0; i < size; ++i)
    {
        rx.currBuffer->data[rx.currBuffer->nofBytes + i] = data[i];
    }

    rx.currBuffer->nofBytes += size;
    rx.remainingInState -= size;

    if(rx.remainingInState == 0)
    {
        h4_rx_fsm();

        if(rx.state == H4_W4_PICKUP)
        {
            /* Advance to next Rx buffer. */
            rx.enqueueIdx = rx.nextEnqueueIdx;
            advanceToNextBuffer();

            /* Prepare for next H4 packet type ID. */
            rx.state = H4_W4_PACKET_TYPE;
            rx.remainingInState = 1;
            BSP_BTUART_AnounceDmaReceiverSize(rx.remainingInState);

            /* Notify complete Rx packet. */
            run_loop_notify_incoming_transport_packet();
        }
    }
}

/*=============================================================================
=============================================================================*/
static int h4_set_baudrate(uint32_t baudrate)
{
    BSP_BTUART_SetBaudrate(baudrate);
    return 0;
}

/*=============================================================================
=============================================================================*/
const char * h4_transport_name(void)
{
    return "H4";
}

/*=============================================================================
=============================================================================*/
static void dummyHandler(uint8_t packetType, uint8_t *packet, uint16_t size)
{
}

/*=============================================================================
*  H4 transport instance.
*============================================================================*/
hci_transport_t * hci_transport_h4_instance(void)
{
    hciTransportH4.transport.open = h4_open;
    hciTransportH4.transport.close = h4_close;
    hciTransportH4.transport.send_packet = h4_send_packet;
    hciTransportH4.transport.register_packet_handler = h4_register_packet_handler;
    hciTransportH4.transport.get_transport_name = h4_transport_name;
    hciTransportH4.transport.set_baudrate = h4_set_baudrate;

    return &hciTransportH4.transport;
}
