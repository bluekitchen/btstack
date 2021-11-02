/*
 * Copyright (C) 2020 BlueKitchen GmbH
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
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at
 * contact@bluekitchen-gmbh.com
 *
 */

#define BTSTACK_FILE__ "usbh_bluetooth.c"

#include "usbh_bluetooth.h"
#include "btstack_debug.h"
#include "hci.h"
#include "btstack_util.h"
#include "bluetooth.h"

typedef struct {
    uint8_t acl_in_ep;
    uint8_t acl_in_pipe;
    uint16_t acl_in_len;
    uint32_t acl_in_frame;
    uint8_t acl_out_ep;
    uint8_t acl_out_pipe;
    uint16_t acl_out_len;
    uint8_t event_in_ep;
    uint8_t event_in_pipe;
    uint16_t event_in_len;
    uint32_t event_in_frame;
} USB_Bluetooth_t;

static enum {
    USBH_OUT_OFF,
    USBH_OUT_IDLE,
    USBH_OUT_CMD,
    USBH_OUT_ACL_SEND,
    USBH_OUT_ACL_POLL,
} usbh_out_state;

static enum {
    USBH_IN_OFF,
    USBH_IN_SUBMIT_REQUEST,
    USBH_IN_POLL,
} usbh_in_state;

// higher-layer callbacks
static void (*usbh_packet_sent)(void);
static void (*usbh_packet_received)(uint8_t packet_type, uint8_t * packet, uint16_t size);

// class state
static USB_Bluetooth_t usb_bluetooth;

// outgoing
static const uint8_t * cmd_packet;
static uint16_t        cmd_len;

static const uint8_t * acl_packet;
static uint16_t        acl_len;

// incoming
static uint16_t hci_event_offset;
static uint8_t hci_event[258];

static uint16_t hci_acl_in_offset;
static uint8_t  hci_acl_in_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + HCI_ACL_BUFFER_SIZE];
static uint8_t  * hci_acl_in_packet = &hci_acl_in_buffer[HCI_INCOMING_PRE_BUFFER_SIZE];


USBH_StatusTypeDef usbh_bluetooth_start_acl_in_transfer(USBH_HandleTypeDef *phost, USB_Bluetooth_t * usb){
    uint16_t acl_in_transfer_size = btstack_min(usb->acl_in_len, HCI_ACL_BUFFER_SIZE - hci_acl_in_offset);
	usb->acl_in_frame = phost->Timer;
    return USBH_BulkReceiveData(phost, &hci_acl_in_packet[hci_acl_in_offset], acl_in_transfer_size, usb->acl_in_pipe);
}

USBH_StatusTypeDef USBH_Bluetooth_InterfaceInit(USBH_HandleTypeDef *phost){
    log_info("USBH_Bluetooth_InterfaceInit");

    // dump everything
    uint8_t interface_index = 0;
    USBH_InterfaceDescTypeDef * interface = &phost->device.CfgDesc.Itf_Desc[interface_index];
    uint8_t num_endpoints = interface->bNumEndpoints;
    uint8_t ep_index;
    int16_t acl_in   = -1;
    int16_t acl_out  = -1;
    int16_t event_in = -1;
    for (ep_index=0;ep_index<num_endpoints;ep_index++){
        USBH_EpDescTypeDef * ep_desc = &interface->Ep_Desc[ep_index];
        printf("Interface %u, endpoint #%u: address 0x%02x, attributes 0x%02x, packet size %u, poll %u\n",
               interface_index, ep_index, ep_desc->bEndpointAddress, ep_desc->bmAttributes, ep_desc->wMaxPacketSize, ep_desc->bInterval);
        // type interrupt, direction incoming
        if  (((ep_desc->bEndpointAddress & USB_EP_DIR_MSK) == USB_EP_DIR_MSK) && (ep_desc->bmAttributes == USB_EP_TYPE_INTR)){
            event_in = ep_index;
            puts("-> HCI Event");
        }
        // type bulk, direction incoming
        if  (((ep_desc->bEndpointAddress & USB_EP_DIR_MSK) == USB_EP_DIR_MSK) && (ep_desc->bmAttributes == USB_EP_TYPE_BULK)){
            acl_in = ep_index;
            puts("-> HCI ACL IN");
        }
        // type bulk, direction incoming
        if  (((ep_desc->bEndpointAddress & USB_EP_DIR_MSK) == 0) && (ep_desc->bmAttributes == USB_EP_TYPE_BULK)){
            acl_out = ep_index;
            puts("-> HCI ACL OUT");
        }
    }

    // all found
    if ((acl_in < 0) && (acl_out < 0) && (event_in < 0)) {
        log_info("Could not find all endpoints");
        return USBH_FAIL;
    }

    // setup
    memset(&usb_bluetooth, 0, sizeof(USB_Bluetooth_t));
    phost->pActiveClass->pData = (void*) &usb_bluetooth;

    // Command
    usbh_out_state = USBH_OUT_OFF;

    // Event In
    USB_Bluetooth_t * usb = &usb_bluetooth;
    usb->event_in_ep =   interface->Ep_Desc[event_in].bEndpointAddress;
    usb->event_in_len =  interface->Ep_Desc[event_in].wMaxPacketSize;
    usb->event_in_pipe = USBH_AllocPipe(phost, usb->event_in_ep);
    usbh_in_state = USBH_IN_OFF;

    /* Open pipe for IN endpoint */
    USBH_OpenPipe(phost, usb->event_in_pipe, usb->event_in_ep, phost->device.address,
                  phost->device.speed, USB_EP_TYPE_INTR, interface->Ep_Desc[event_in].wMaxPacketSize);

    USBH_LL_SetToggle(phost, usb->event_in_ep, 0U);

    // ACL In
    usb->acl_in_ep  =  interface->Ep_Desc[acl_in].bEndpointAddress;
    usb->acl_in_len =  interface->Ep_Desc[acl_in].wMaxPacketSize;
    usb->acl_in_pipe = USBH_AllocPipe(phost, usb->acl_in_ep);
    USBH_OpenPipe(phost, usb->acl_in_pipe, usb->acl_in_ep, phost->device.address, phost->device.speed, USB_EP_TYPE_BULK, usb->acl_in_len);
    USBH_LL_SetToggle(phost, usb->acl_in_pipe, 0U);
    hci_acl_in_offset = 0;
    usbh_bluetooth_start_acl_in_transfer(phost, usb);

    // ACL Out
    usb->acl_out_ep  =  interface->Ep_Desc[acl_out].bEndpointAddress;
    usb->acl_out_len =  interface->Ep_Desc[acl_out].wMaxPacketSize;
    usb->acl_out_pipe = USBH_AllocPipe(phost, usb->acl_out_ep);
    USBH_OpenPipe(phost, usb->acl_out_pipe, usb->acl_out_ep, phost->device.address, phost->device.speed, USB_EP_TYPE_BULK, usb->acl_out_len);
    USBH_LL_SetToggle(phost, usb->acl_out_pipe, 0U);

    return USBH_OK;
}

USBH_StatusTypeDef USBH_Bluetooth_InterfaceDeInit(USBH_HandleTypeDef *phost){
    log_info("USBH_Bluetooth_InterfaceDeInit");
    usbh_out_state = USBH_OUT_OFF;
    usbh_in_state = USBH_IN_OFF;
    return USBH_OK;
}

USBH_StatusTypeDef USBH_Bluetooth_ClassRequest(USBH_HandleTypeDef *phost){
    // ready!
    usbh_out_state = USBH_OUT_IDLE;
    usbh_in_state = USBH_IN_SUBMIT_REQUEST;
    // notify host stack
    (*usbh_packet_sent)();
    return USBH_OK;
}

USBH_StatusTypeDef USBH_Bluetooth_Process(USBH_HandleTypeDef *phost){
    USBH_StatusTypeDef status;
    USBH_URBStateTypeDef urb_state;
    USB_Bluetooth_t * usb = (USB_Bluetooth_t *) phost->pActiveClass->pData;
    uint16_t transfer_size;
    switch (usbh_out_state){
        case USBH_OUT_CMD:
            // just send HCI Reset naively
            phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_RECIPIENT_INTERFACE | USB_REQ_TYPE_CLASS;
            phost->Control.setup.b.bRequest = 0;
            phost->Control.setup.b.wValue.w = 0;
            phost->Control.setup.b.wIndex.w = 0U;
            phost->Control.setup.b.wLength.w = cmd_len;
            status = USBH_CtlReq(phost, (uint8_t *) cmd_packet, cmd_len);
            if (status == USBH_OK) {
                usbh_out_state = USBH_OUT_IDLE;
                // notify host stack
                (*usbh_packet_sent)();
            }
            break;
        case USBH_OUT_ACL_SEND:
            transfer_size = btstack_min(usb->acl_out_len, acl_len);
            USBH_BulkSendData(phost, (uint8_t *) acl_packet, transfer_size, usb->acl_out_pipe, 1);
            usbh_out_state = USBH_OUT_ACL_POLL;
            break;
        case USBH_OUT_ACL_POLL:
            urb_state = USBH_LL_GetURBState(phost, usb->acl_out_pipe);
            switch (urb_state){
                case USBH_URB_IDLE:
                    break;
                case USBH_URB_NOTREADY:
                    usbh_out_state = USBH_OUT_ACL_SEND;
                    break;
                case USBH_URB_DONE:
                    transfer_size = btstack_min(usb->acl_out_len, acl_len);
                    acl_len -= transfer_size;
                    if (acl_len == 0){
                        usbh_out_state = USBH_OUT_IDLE;
                        // notify host stack
                        (*usbh_packet_sent)();
                    } else {
                        acl_packet += transfer_size;
                        usbh_out_state = USBH_OUT_ACL_SEND;
                    }
                    break;
                default:
                    log_info("URB State ACL Out: %02x", urb_state);
                    break;
            }
            break;
        default:
            break;
    }

    uint8_t  event_transfer_size;
    uint16_t event_size;
    switch (usbh_in_state){
        case USBH_IN_SUBMIT_REQUEST:
            event_transfer_size = btstack_min( usb->event_in_len, sizeof(hci_event) - hci_event_offset);
            USBH_InterruptReceiveData(phost, &hci_event[hci_event_offset], event_transfer_size, usb->event_in_pipe);
            usb->event_in_frame = phost->Timer;
            usbh_in_state = USBH_IN_POLL;
            break;
        case USBH_IN_POLL:
            urb_state = USBH_LL_GetURBState(phost, usb->event_in_pipe);
            switch (urb_state){
                case USBH_URB_IDLE:
                    break;
                case USBH_URB_DONE:
                    usbh_in_state = USBH_IN_SUBMIT_REQUEST;
                    event_transfer_size = USBH_LL_GetLastXferSize(phost, usb->event_in_pipe);
                    hci_event_offset += event_transfer_size;
                    if (hci_event_offset < 2) break;
                    event_size = 2 + hci_event[1];
                    // event complete
                    if (hci_event_offset >= event_size){
                        hci_event_offset = 0;
                        (*usbh_packet_received)(HCI_EVENT_PACKET, hci_event, event_size);
                    }
                    break;
                default:
                    log_info("URB State Event: %02x", urb_state);
                    break;
            }
            if ((phost->Timer - usb->event_in_frame) > 2){
                usbh_in_state = USBH_IN_SUBMIT_REQUEST;
            }
            break;
        default:
            break;
    }

    // ACL In
    uint16_t acl_transfer_size;
    uint16_t acl_size;
    urb_state = USBH_LL_GetURBState(phost, usb->acl_in_pipe);
    switch (urb_state){
        case USBH_URB_IDLE:
            // If state stays IDLE for longer than a full frame, something went wrong with submitting the request,
            // just re-submits the request
            if ((phost->Timer - usb->acl_in_frame) > 2){
                status = usbh_bluetooth_start_acl_in_transfer(phost, usb);
                btstack_assert(status == USBH_OK);
            }
            break;
        case USBH_URB_NOTREADY:
            // The original USB Host code re-submits the request when it receives a NAK, resulting in about 80% MCU load
            // With our patch, NOTREADY is returned, which allows to re-submit the request in the next frame.
            if (phost->Timer != usb->acl_in_frame){
                status = usbh_bluetooth_start_acl_in_transfer(phost, usb);
                btstack_assert(status == USBH_OK);
            }            break;
        case USBH_URB_DONE:
            acl_transfer_size = USBH_LL_GetLastXferSize(phost, usb->acl_in_pipe);
            hci_acl_in_offset += acl_transfer_size;
            if (hci_acl_in_offset < 4) break;
            acl_size = 4 + little_endian_read_16(hci_acl_in_packet, 2);
            // acl complete
            if (hci_acl_in_offset > acl_size){
                printf("Extra HCI EVENT!\n");
            }
            if (hci_acl_in_offset >= acl_size){
                (*usbh_packet_received)(HCI_ACL_DATA_PACKET, hci_acl_in_packet, acl_size);
                hci_acl_in_offset = 0;
            }
            usbh_bluetooth_start_acl_in_transfer(phost, usb);
            break;
        default:
            log_info("URB State Event: %02x", urb_state);
            break;
    }

    return USBH_OK;
}

USBH_StatusTypeDef USBH_Bluetooth_SOFProcess(USBH_HandleTypeDef *phost){
    return USBH_OK;
}

void usbh_bluetooth_set_packet_sent(void (*callback)(void)){
    usbh_packet_sent = callback;
}


void usbh_bluetooth_set_packet_received(void (*callback)(uint8_t packet_type, uint8_t * packet, uint16_t size)){
    usbh_packet_received = callback;
}

bool usbh_bluetooth_can_send_now(void){
    return usbh_out_state == USBH_OUT_IDLE;;
}

void usbh_bluetooth_send_cmd(const uint8_t * packet, uint16_t len){
    btstack_assert(usbh_out_state == USBH_OUT_IDLE);
    cmd_packet = packet;
    cmd_len    = len;
    usbh_out_state = USBH_OUT_CMD;
}

void usbh_bluetooth_send_acl(const uint8_t * packet, uint16_t len){
    btstack_assert(usbh_out_state == USBH_OUT_IDLE);
    acl_packet = packet;
    acl_len    = len;
    usbh_out_state = USBH_OUT_ACL_SEND;
}

USBH_ClassTypeDef  Bluetooth_Class = {
    "Bluetooth",
    USB_BLUETOOTH_CLASS,
    USBH_Bluetooth_InterfaceInit,
    USBH_Bluetooth_InterfaceDeInit,
    USBH_Bluetooth_ClassRequest,
    USBH_Bluetooth_Process,
    USBH_Bluetooth_SOFProcess,
    NULL,
};

