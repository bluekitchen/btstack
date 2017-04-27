/*
 * Copyright (C) 2014 BlueKitchen GmbH
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

#define __BTSTACK_FILE__ "hid_device.c"

#include <string.h>

#include "classic/hid_device.h"
#include "classic/sdp_util.h"
#include "bluetooth.h"
#include "bluetooth_sdp.h"
#include "l2cap.h"
#include "btstack_event.h"
#include "btstack_debug.h"

// hid device state
typedef struct hid_device {
    uint16_t  cid;
    bd_addr_t bd_addr;
    hci_con_handle_t con_handle;
    uint16_t  control_cid;
    uint16_t  interrupt_cid;
    uint8_t   incoming;
} hid_device_t;

static hid_device_t _hid_device;
static hid_device_t * hid_device = &_hid_device;

static btstack_packet_handler_t hid_callback;

void hid_create_sdp_record(
    uint8_t *service, 
    uint32_t service_record_handle,
    uint16_t hid_device_subclass,
    uint8_t  hid_country_code,
    uint8_t  hid_virtual_cable,
    uint8_t  hid_reconnect_initiate,
    uint8_t  hid_boot_device,
    const uint8_t * hid_descriptor, uint16_t hid_descriptor_size,
    const char *device_name){
    
    uint8_t * attribute;
    de_create_sequence(service);
    
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_RECORD_HANDLE);
    de_add_number(service, DE_UINT, DE_SIZE_32, service_record_handle);        
    
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST);
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute,  DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_HUMAN_INTERFACE_DEVICE_SERVICE);
    }
    de_pop_sequence(service, attribute);
    
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t * l2cpProtocol = de_push_sequence(attribute);
        {
            de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_L2CAP);
            de_add_number(l2cpProtocol,  DE_UINT, DE_SIZE_16, PSM_HID_CONTROL);
        }
        de_pop_sequence(attribute, l2cpProtocol);

        uint8_t * hidProtocol = de_push_sequence(attribute);
        {
            de_add_number(hidProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_HIDP); 
        }
        de_pop_sequence(attribute, hidProtocol);
    }
    de_pop_sequence(service, attribute);

    // TODO?
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_LANGUAGE_BASE_ATTRIBUTE_ID_LIST);
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x656e);
        de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x006a);
        de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x0100);
    }
    de_pop_sequence(service, attribute);
 
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_ADDITIONAL_PROTOCOL_DESCRIPTOR_LISTS);
    attribute = de_push_sequence(service);
    {
        uint8_t * additionalDescriptorAttribute = de_push_sequence(attribute); 
        {
            uint8_t * l2cpProtocol = de_push_sequence(additionalDescriptorAttribute);
            {
                de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_L2CAP);
                de_add_number(l2cpProtocol,  DE_UINT, DE_SIZE_16, PSM_HID_INTERRUPT);
            }
            de_pop_sequence(additionalDescriptorAttribute, l2cpProtocol);

            uint8_t * hidProtocol = de_push_sequence(attribute);
            {
                de_add_number(hidProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_HIDP); 
            }
            de_pop_sequence(attribute, hidProtocol);
        }
        de_pop_sequence(attribute, additionalDescriptorAttribute);
    }
    de_pop_sequence(service, attribute); 

    // 0x0100 "ServiceName"
    de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0100);
    de_add_data(service,  DE_STRING, strlen(device_name), (uint8_t *) device_name); 

    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_BLUETOOTH_PROFILE_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t * hidProfile = de_push_sequence(attribute);
        {
            de_add_number(hidProfile,  DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_HUMAN_INTERFACE_DEVICE_SERVICE);
            de_add_number(hidProfile,  DE_UINT, DE_SIZE_16, 0x0101);    // Version 1.1
        }
        de_pop_sequence(attribute, hidProfile);
    }
    de_pop_sequence(service, attribute);

    // Deprecated in v1.1.1
    // de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_HID_DEVICE_RELEASE_NUMBER);
    // de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0101);

    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_HID_PARSER_VERSION);
    de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0111);  // v1.1.1

    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_HID_DEVICE_SUBCLASS);
    de_add_number(service,  DE_UINT, DE_SIZE_16, hid_device_subclass);
    
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_HID_COUNTRY_CODE);
    de_add_number(service,  DE_UINT, DE_SIZE_16, hid_country_code); 

    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_HID_VIRTUAL_CABLE);
    de_add_number(service,  DE_BOOL, DE_SIZE_8,  hid_virtual_cable);

    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_HID_RECONNECT_INITIATE); 
    de_add_number(service,  DE_BOOL, DE_SIZE_8,  hid_reconnect_initiate); 

    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_HID_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t* hidDescriptor = de_push_sequence(attribute);
        {
            de_add_number(hidDescriptor,  DE_UINT, DE_SIZE_8, 0x22);    // Report Descriptor
            de_add_data(hidDescriptor,  DE_STRING, hid_descriptor_size, (uint8_t *) hid_descriptor);
        }
        de_pop_sequence(attribute, hidDescriptor);
    }        
    de_pop_sequence(service, attribute);  
    
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_HID_BOOT_DEVICE); 
    de_add_number(service,  DE_BOOL, DE_SIZE_8,  hid_boot_device);
}

static inline void hid_device_emit_connected_event(hid_device_t * context, uint8_t status){
    uint8_t event[15];
    int pos = 0;
    event[pos++] = HCI_EVENT_HID_META;
    pos++;  // skip len
    event[pos++] = HID_SUBEVENT_CONNECTION_OPENED;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    event[pos++] = status;
    memcpy(&event[pos], context->bd_addr, 6);
    pos += 6;
    little_endian_store_16(event,pos,context->con_handle);
    pos += 2;
    event[pos++] = context->incoming;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("hid_device_emit_connected_event size %u", pos);
    hid_callback(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}   

static inline void hid_device_emit_connection_closed_event(hid_device_t * context){
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_HID_META;
    pos++;  // skip len
    event[pos++] = HID_SUBEVENT_CONNECTION_CLOSED;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("hid_device_emit_connection_closed_event size %u", pos);
    hid_callback(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}   

static inline void hid_device_emit_can_send_now_event(hid_device_t * context){
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_HID_META;
    pos++;  // skip len
    event[pos++] = HID_SUBEVENT_CAN_SEND_NOW;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("hid_device_emit_can_send_now_event size %u", pos);
    hid_callback(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}


static int hid_connected(void){
    return hid_device->control_cid && hid_device->interrupt_cid;
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t packet_size){
    UNUSED(channel);
    UNUSED(packet_size);
    int connected_before;
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (packet[0]){
                case L2CAP_EVENT_INCOMING_CONNECTION:
                    switch (l2cap_event_incoming_connection_get_psm(packet)){
                        case PSM_HID_CONTROL:
                        case PSM_HID_INTERRUPT:
                            if (hid_device->con_handle == 0 || l2cap_event_incoming_connection_get_handle(packet) == hid_device->con_handle){
                                hid_device->con_handle = l2cap_event_incoming_connection_get_handle(packet);
                                l2cap_accept_connection(channel);
                            } else {
                                l2cap_decline_connection(channel);
                            }
                            break;
                        default:
                            l2cap_decline_connection(channel);
                            break;
                    }
                    break;
                case L2CAP_EVENT_CHANNEL_OPENED:
                    if (l2cap_event_channel_opened_get_status(packet)) return;
                    connected_before = hid_connected();
                    switch (l2cap_event_channel_opened_get_psm(packet)){
                        case PSM_HID_CONTROL:
                            hid_device->control_cid = l2cap_event_channel_opened_get_local_cid(packet);
                            log_info("HID Control opened, cid 0x%02x", hid_device->control_cid);
                            break;
                        case PSM_HID_INTERRUPT:
                            hid_device->interrupt_cid = l2cap_event_channel_opened_get_local_cid(packet);
                            log_info("HID Interrupt opened, cid 0x%02x", hid_device->interrupt_cid);
                            break;
                        default:
                            break;
                    }
                    if (!connected_before && hid_connected()){
                        hid_device->incoming = 1;
                        log_info("HID Connected");
                        hid_device_emit_connected_event(hid_device, 0);
                    }
                    break;
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    connected_before = hid_connected();
                    if (l2cap_event_channel_closed_get_local_cid(packet) == hid_device->control_cid){
                        log_info("HID Control closed");
                        hid_device->control_cid = 0;
                    }
                    if (l2cap_event_channel_closed_get_local_cid(packet) == hid_device->interrupt_cid){
                        log_info("HID Interrupt closed");
                        hid_device->interrupt_cid = 0;
                    }
                    if (connected_before && !hid_connected()){
                        hid_device->con_handle = 0;
                        log_info("HID Disconnected");
                        hid_device_emit_connection_closed_event(hid_device);
                    }
                    break;
                case L2CAP_EVENT_CAN_SEND_NOW:
                    log_info("HID Can send now, emit event");
                    hid_device_emit_can_send_now_event(hid_device);
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

/**
 * @brief Set up HID Device 
 */
void hid_device_init(void){
    memset(hid_device, 0, sizeof(hid_device_t));
    hid_device->cid = 1;
    l2cap_register_service(packet_handler, PSM_HID_INTERRUPT, 100, LEVEL_0);
    l2cap_register_service(packet_handler, PSM_HID_CONTROL,   100, LEVEL_0);                                      
}

/**
 * @brief Register callback for the HID Device client. 
 * @param callback
 */
void hid_device_register_packet_handler(btstack_packet_handler_t callback){
    hid_callback = callback;
}

/**
 * @brief Request can send now event to send HID Report
 * @param hid_cid
 */
void hid_device_request_can_send_now_event(uint16_t hid_cid){
    UNUSED(hid_cid);
    if (!hid_device->control_cid) return;
    l2cap_request_can_send_now_event(hid_device->control_cid);
}

/**
 * @brief Send HID messageon interrupt channel
 * @param hid_cid
 */
void hid_device_send_interrupt_message(uint16_t hid_cid, const uint8_t * message, uint16_t message_len){
    UNUSED(hid_cid);
    if (!hid_device->interrupt_cid) return;
    l2cap_send(hid_device->interrupt_cid, (uint8_t*) message, message_len);
}

/**
 * @brief Send HID messageon control channel
 * @param hid_cid
 */
void hid_device_send_contro_message(uint16_t hid_cid, const uint8_t * message, uint16_t message_len){
    UNUSED(hid_cid);
    if (!hid_device->control_cid) return;
    l2cap_send(hid_device->control_cid, (uint8_t*) message, message_len);
}

