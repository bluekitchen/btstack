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
 * Please inquire about commercial licensing options at
 * contact@bluekitchen-gmbh.com
 *
 */

#define BTSTACK_FILE__ "usbh_bluetooth.c"

#include "usbh_bluetooth.h"

#define log_info puts

USBH_StatusTypeDef USBH_Bluetooth_InterfaceInit(USBH_HandleTypeDef *phost){
    log_info("USBH_Bluetooth_InterfaceInit");
    // dump everything
    uint8_t num_interfaces = phost->device.CfgDesc.bNumInterfaces;
    uint8_t interface_index;
    for (interface_index=0;interface_index<num_interfaces;interface_index++){
        USBH_InterfaceDescTypeDef * interface = &phost->device.CfgDesc.Itf_Desc[interface_index];
        uint8_t num_endpoints = interface->bNumEndpoints;
        uint8_t ep_index;
        for (ep_index=0;ep_index<num_endpoints;ep_index++){
            USBH_EpDescTypeDef * ep_desc = &interface->Ep_Desc[ep_index];
            printf("Interface %u, endpoint #%u: address 0x%02x, attributes 0x%02x\n", interface_index, ep_index, ep_desc->bEndpointAddress, ep_desc->bmAttributes);
            // type interrupt, direction incoming
            if  (((ep_desc->bEndpointAddress & USB_EP_DIR_MSK) == USB_EP_DIR_MSK) && (ep_desc->bmAttributes == USB_EP_TYPE_INTR)){
                puts("-> HCI Event");
            }
            // type bulk, direction incoming
            if  (((ep_desc->bEndpointAddress & USB_EP_DIR_MSK) == USB_EP_DIR_MSK) && (ep_desc->bmAttributes == USB_EP_TYPE_BULK)){
                puts("-> HCI ACL IN");
            }
            // type bulk, direction incoming
            if  (((ep_desc->bEndpointAddress & USB_EP_DIR_MSK) == 0) && (ep_desc->bmAttributes == USB_EP_TYPE_BULK)){
                puts("-> HCI ACL OUT");
            }
        }
    }
    return USBH_OK;
}
USBH_StatusTypeDef USBH_Bluetooth_InterfaceDeInit(USBH_HandleTypeDef *phost){
    log_info("USBH_Bluetooth_InterfaceDeInit");
    return USBH_OK;
}
USBH_StatusTypeDef USBH_Bluetooth_ClassRequest(USBH_HandleTypeDef *phost){
    log_info("USBH_Bluetooth_ClassRequest");
    return USBH_OK;
}
USBH_StatusTypeDef USBH_Bluetooth_Process(USBH_HandleTypeDef *phost){
    // log_info("USBH_Bluetooth_Process");
    return USBH_OK;
}
USBH_StatusTypeDef USBH_Bluetooth_SOFProcess(USBH_HandleTypeDef *phost){
    // log_info("USBH_Bluetooth_SOFProcess");
    return USBH_OK;
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

