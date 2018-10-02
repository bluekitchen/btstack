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

typedef enum {
    HID_DEVICE_IDLE,
    HID_DEVICE_CONNECTED,
    HID_DEVICE_W2_GET_REPORT,
    HID_DEVICE_W2_SET_REPORT,
    HID_DEVICE_W2_GET_PROTOCOL,
    HID_DEVICE_W2_SET_PROTOCOL,
    HID_DEVICE_W2_ANSWER_SET_PROTOCOL,
    HID_DEVICE_W2_SEND_UNSUPPORTED_REQUEST,
} hid_device_state_t;

// hid device state
typedef struct hid_device {
    uint16_t  cid;
    bd_addr_t bd_addr;
    hci_con_handle_t con_handle;
    uint16_t  control_cid;
    uint16_t  interrupt_cid;
    uint8_t   incoming;
    uint8_t   connected;
    hid_device_state_t state;
    hid_report_type_t report_type;
    uint16_t  report_id;
    uint16_t  num_received_bytes;
    
    hid_handshake_param_type_t report_status;
    hid_protocol_mode_t protocol_mode;
} hid_device_t;

static hid_device_t _hid_device;
static uint8_t hid_boot_protocol_mode_supported;

static hid_handshake_param_type_t dummy_write_report(uint16_t hid_cid, hid_report_type_t report_type, uint16_t report_id, uint8_t report_max_size, int * out_report_size, uint8_t * out_report){
    UNUSED(hid_cid);
    UNUSED(report_type);
    UNUSED(report_id);
    UNUSED(report_max_size);
    UNUSED(out_report_size);
    UNUSED(out_report);
    return HID_HANDSHAKE_PARAM_TYPE_ERR_UNKNOWN;
}
static hid_handshake_param_type_t dummy_set_report(uint16_t hid_cid, hid_report_type_t report_type, int report_size, uint8_t * report){
    UNUSED(hid_cid);
    UNUSED(report_type);
    UNUSED(report_size);
    UNUSED(report);
    return HID_HANDSHAKE_PARAM_TYPE_ERR_UNKNOWN;
}
static hid_handshake_param_type_t (*hci_device_write_report) (uint16_t hid_cid, hid_report_type_t report_type, uint16_t report_id, uint8_t report_max_size, int * out_report_size, uint8_t * out_report) = dummy_write_report;
static hid_handshake_param_type_t (*hci_device_set_report)   (uint16_t hid_cid, hid_report_type_t report_type, int report_size, uint8_t * report) = dummy_set_report;

static btstack_packet_handler_t hid_callback;

static uint16_t hid_device_cid = 0;

static uint16_t hid_device_get_next_cid(void){
    hid_device_cid++;
    if (!hid_device_cid){
        hid_device_cid = 1;
    }
    return hid_device_cid;
}

// TODO: store hid device connection into list
static hid_device_t * hid_device_get_instance_for_cid(uint16_t cid){
    // printf("control_cid 0x%02x, interrupt_cid 0x%02x, query_cid 0x%02x \n", _hid_device.control_cid,  _hid_device.interrupt_cid, cid);
    if (_hid_device.cid == cid || _hid_device.control_cid == cid || _hid_device.interrupt_cid == cid){
        return &_hid_device;
    }
    return NULL;
}

static hid_device_t * hid_device_provide_instance_for_bt_addr(bd_addr_t bd_addr){
    if (!_hid_device.cid){
        memcpy(_hid_device.bd_addr, bd_addr, 6);
        _hid_device.cid = hid_device_get_next_cid();
        _hid_device.protocol_mode = HID_PROTOCOL_MODE_REPORT;
    }
    return &_hid_device;
}

static hid_device_t * hid_device_get_instance_for_con_handle(uint16_t con_handle){
    UNUSED(con_handle);
    return &_hid_device;
}

static hid_device_t * hid_device_create_instance(void){

    return &_hid_device;
}

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
    de_add_number(service,  DE_UINT, DE_SIZE_8,  hid_device_subclass);
    
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_HID_COUNTRY_CODE);
    de_add_number(service,  DE_UINT, DE_SIZE_8,  hid_country_code);

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
    
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_HIDLANGID_BASE_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t* hig_lang_base = de_push_sequence(attribute);
        {
            // see: http://www.usb.org/developers/docs/USB_LANGIDs.pdf
            de_add_number(hig_lang_base,  DE_UINT, DE_SIZE_16, 0x0409);    // HIDLANGID = English (US)
            de_add_number(hig_lang_base,  DE_UINT, DE_SIZE_16, 0x0100);    // HIDLanguageBase = 0x0100 default
        }
        de_pop_sequence(attribute, hig_lang_base);
    }
    de_pop_sequence(service, attribute);

    uint8_t hid_remote_wake = 1;
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_HID_REMOTE_WAKE); 
    de_add_number(service,  DE_BOOL, DE_SIZE_8,  hid_remote_wake);

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
    reverse_bd_addr(context->bd_addr, &event[pos]);
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

static inline void hid_device_emit_event(hid_device_t * context, uint8_t subevent_type){
    uint8_t event[4];
    int pos = 0;
    event[pos++] = HCI_EVENT_HID_META;
    pos++;  // skip len
    event[pos++] = subevent_type;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("hid_device_emit_event size %u", pos);
    hid_callback(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t packet_size){
    UNUSED(channel);
    UNUSED(packet_size);
    int connected_before;
    uint16_t psm;
    uint8_t status;
    hid_device_t * device = NULL;
    uint8_t param;
    bd_addr_t address;
    
    int report_size;
    uint8_t report[20];

    switch (packet_type){
        case L2CAP_DATA_PACKET:
            device = hid_device_get_instance_for_cid(channel);
            if (!device) {
                log_error("no device with cid 0x%02x", channel);
                return;
            }
            hid_message_type_t message_type = packet[0] >> 4;
            // printf("L2CAP_DATA_PACKET message_type %d  \n", message_type);
            switch (message_type){
                case HID_MESSAGE_TYPE_GET_REPORT:
                    device->report_type = packet[0] & 0x03;
                    device->report_id = 0;
                    if (packet_size > 1){
                        device->report_id = packet[1];
                    }
                    if ((packet[0] & 0x08) && packet_size >= 4){
                        device->num_received_bytes = little_endian_read_16(packet, 2);
                    } else {
                        device->num_received_bytes = l2cap_max_mtu();
                    }
                    device->state = HID_DEVICE_W2_GET_REPORT;
                    l2cap_request_can_send_now_event(device->control_cid);
                    break;
                case HID_MESSAGE_TYPE_SET_REPORT:
                    device->state = HID_DEVICE_W2_SET_REPORT;  
                    if (packet_size < 1 || packet_size > l2cap_max_mtu() - 1) {
                        device->report_status = HID_HANDSHAKE_PARAM_TYPE_ERR_INVALID_PARAMETER;
                        break;
                    }
                    device->report_status = HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL;
                    device->report_type = packet[0] & 0x03;
                    device->num_received_bytes = packet_size - 1;
                    device->report_status = (*hci_device_set_report)(device->cid, device->report_type, device->num_received_bytes, &packet[1]);
                    l2cap_request_can_send_now_event(device->control_cid);
                    break;
                case HID_MESSAGE_TYPE_GET_PROTOCOL:
                    device->state = HID_DEVICE_W2_GET_PROTOCOL;  
                    if (packet_size != 1) {
                        device->report_status = HID_HANDSHAKE_PARAM_TYPE_ERR_INVALID_PARAMETER;
                        break;
                    }
                    device->report_status = HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL;
                    l2cap_request_can_send_now_event(device->control_cid);
                
                case HID_MESSAGE_TYPE_SET_PROTOCOL:
                    device->state = HID_DEVICE_W2_SET_PROTOCOL;  
                    if (packet_size != 1) {
                        device->report_status = HID_HANDSHAKE_PARAM_TYPE_ERR_INVALID_PARAMETER;
                        break;
                    }
                    param = packet[0] & 0x01;
                    if (param == HID_PROTOCOL_MODE_BOOT && !hid_boot_protocol_mode_supported){
                        device->report_status = HID_HANDSHAKE_PARAM_TYPE_ERR_INVALID_PARAMETER;
                        break;
                    }
                    device->protocol_mode = param;
                    device->report_status = HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL;
                    l2cap_request_can_send_now_event(device->control_cid);
                    break;
                case HID_MESSAGE_TYPE_HID_CONTROL:
                    param = packet[0] & 0x0F;
                    switch (param){
                        case HID_CONTROL_PARAM_SUSPEND:
                            hid_device_emit_event(device, HID_SUBEVENT_SUSPEND);
                            break;
                        case HID_CONTROL_PARAM_EXIT_SUSPEND:
                            hid_device_emit_event(device, HID_SUBEVENT_EXIT_SUSPEND);
                            break;
                        default:
                            device->state = HID_DEVICE_W2_SEND_UNSUPPORTED_REQUEST;
                            l2cap_request_can_send_now_event(device->control_cid);
                            break;
                    }
                    break;
                default:
                    // printf("HID_DEVICE_W2_SEND_UNSUPPORTED_REQUEST %d  \n", message_type);
                    device->state = HID_DEVICE_W2_SEND_UNSUPPORTED_REQUEST;
                    l2cap_request_can_send_now_event(device->control_cid);
                    break;
            }
            break;
        case HCI_EVENT_PACKET:
            switch (packet[0]){
                case L2CAP_EVENT_INCOMING_CONNECTION:
                    switch (l2cap_event_incoming_connection_get_psm(packet)){
                        case PSM_HID_CONTROL:
                        case PSM_HID_INTERRUPT:
                            l2cap_event_incoming_connection_get_address(packet, address); 
                            device = hid_device_provide_instance_for_bt_addr(address);
                            if (!device) {
                                log_error("L2CAP_EVENT_INCOMING_CONNECTION, no hid device for con handle 0x%02x", l2cap_event_incoming_connection_get_handle(packet));
                                l2cap_decline_connection(channel);
                                break;
                            }
                            if (device->con_handle == 0 || l2cap_event_incoming_connection_get_handle(packet) == device->con_handle){
                                device->con_handle = l2cap_event_incoming_connection_get_handle(packet);
                                device->incoming = 1;
                                l2cap_event_incoming_connection_get_address(packet, device->bd_addr);
                                l2cap_accept_connection(channel);
                            } else {
                                l2cap_decline_connection(channel);
                                log_error("L2CAP_EVENT_INCOMING_CONNECTION, decline connection for con handle 0x%02x", l2cap_event_incoming_connection_get_handle(packet));
                            }
                            break;
                        default:
                            l2cap_decline_connection(channel);
                            break;
                    }
                    break;
                case L2CAP_EVENT_CHANNEL_OPENED:
                    device = hid_device_get_instance_for_con_handle(l2cap_event_channel_opened_get_handle(packet));
                    if (!device) {
                        log_error("L2CAP_EVENT_CHANNEL_OPENED, no hid device for local cid 0x%02x", l2cap_event_channel_opened_get_local_cid(packet));
                        return;
                    }
                    status = l2cap_event_channel_opened_get_status(packet);
                    if (status) {
                        if (device->incoming == 0){
                            // report error for outgoing connection
                            hid_device_emit_connected_event(device, status);
                        }
                        return;
                    }
                    psm = l2cap_event_channel_opened_get_psm(packet);
                    connected_before = device->connected;
                    switch (psm){
                        case PSM_HID_CONTROL:
                            device->control_cid = l2cap_event_channel_opened_get_local_cid(packet);
                            log_info("HID Control opened, cid 0x%02x", device->control_cid);
                            break;
                        case PSM_HID_INTERRUPT:
                            device->interrupt_cid = l2cap_event_channel_opened_get_local_cid(packet);
                            log_info("HID Interrupt opened, cid 0x%02x", device->interrupt_cid);
                            break;
                        default:
                            break;
                    }
                    
                    // connect HID Interrupt for outgoing
                    if (device->incoming == 0 && psm == PSM_HID_CONTROL){
                        log_info("Create outgoing HID Interrupt");
                        status = l2cap_create_channel(packet_handler, device->bd_addr, PSM_HID_INTERRUPT, 48, &device->interrupt_cid);
                        break;
                    }
                    if (!connected_before && device->control_cid && device->interrupt_cid){
                        device->connected = 1;
                        log_info("HID Connected");
                        hid_device_emit_connected_event(device, 0);
                    }
                    break;
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    device = hid_device_get_instance_for_cid(l2cap_event_channel_closed_get_local_cid(packet));
                    if (!device) return;

                    // connected_before = device->connected;
                    device->incoming  = 0;
                    if (l2cap_event_channel_closed_get_local_cid(packet) == device->interrupt_cid){
                        log_info("HID Interrupt closed");
                        device->interrupt_cid = 0;
                    }
                    if (l2cap_event_channel_closed_get_local_cid(packet) == device->control_cid){
                        log_info("HID Control closed");
                        device->control_cid = 0;
                    }
                    if (!device->interrupt_cid && !device->control_cid){
                        device->connected = 0;
                        device->con_handle = 0;
                        device->cid = 0;
                        log_info("HID Disconnected");
                        hid_device_emit_connection_closed_event(device);
                    }
                    break;

                case L2CAP_EVENT_CAN_SEND_NOW:
                    device = hid_device_get_instance_for_cid(l2cap_event_can_send_now_get_local_cid(packet));
                    if (!device) return;
                    switch (device->state){
                        case HID_DEVICE_W2_GET_REPORT:
                            device->report_status = (*hci_device_write_report)(device->cid, device->report_type, device->report_id, (uint16_t) sizeof(report) - 1, &report_size, &report[1]);
                            report_size += 1;

                            if (device->report_status != HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL){
                                report[0] = (HID_MESSAGE_TYPE_HANDSHAKE << 4) | device->report_status;
                                hid_device_send_control_message(device->cid, &report[0], 1);
                                break;
                            }

                            if (report_size > l2cap_max_mtu()){
                                report[0] = (HID_MESSAGE_TYPE_HANDSHAKE << 4) | HID_HANDSHAKE_PARAM_TYPE_ERR_INVALID_PARAMETER;
                                hid_device_send_control_message(device->cid, &report[0], 1);
                                break;
                            }

                            report[0] = (HID_MESSAGE_TYPE_DATA << 4) | device->report_type;
                            hid_device_send_control_message(device->cid, &report[0], btstack_min(report_size, device->num_received_bytes));
                            //     device->state = HID_DEVICE_IDLE;
                            break;
                        case HID_DEVICE_W2_SET_REPORT:
                        case HID_DEVICE_W2_SET_PROTOCOL:
                            report[0] = (HID_MESSAGE_TYPE_HANDSHAKE << 4) | device->report_status;
                            hid_device_send_control_message(device->cid, &report[0], 1);
                            break;
                        case HID_DEVICE_W2_GET_PROTOCOL:
                            if (device->report_status != HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL){
                                report[0] = (HID_MESSAGE_TYPE_HANDSHAKE << 4) | device->report_status;
                                hid_device_send_control_message(device->cid, &report[0], 1);
                                break;
                            }
                            report[0] = (HID_MESSAGE_TYPE_DATA << 4) | device->protocol_mode;
                            hid_device_send_control_message(device->cid, &report[0], 1);
                            break;
                            

                        case HID_DEVICE_W2_SEND_UNSUPPORTED_REQUEST:
                            report[0] = (HID_MESSAGE_TYPE_HANDSHAKE << 4) | HID_HANDSHAKE_PARAM_TYPE_ERR_UNSUPPORTED_REQUEST;
                            hid_device_send_control_message(device->cid, &report[0], 1);
                            break;
                        default:
                            log_info("HID Can send now, emit event");
                            hid_device_emit_can_send_now_event(device);
                            // device->state = HID_DEVICE_IDLE;
                            break;
                    }
                    device->state = HID_DEVICE_IDLE;
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
void hid_device_init(uint8_t boot_protocol_mode_supported){
    hid_boot_protocol_mode_supported = boot_protocol_mode_supported;
    l2cap_register_service(packet_handler, PSM_HID_INTERRUPT, 100, LEVEL_2);
    l2cap_register_service(packet_handler, PSM_HID_CONTROL,   100, LEVEL_2);                                      
}

/**
 * @brief Register callback for the HID Device client. 
 * @param callback
 */
void hid_device_register_packet_handler(btstack_packet_handler_t callback){
    hid_callback = callback;
}



void hid_device_register_report_request_callback(hid_handshake_param_type_t (*callback) (uint16_t hid_cid, hid_report_type_t report_type, uint16_t report_id, uint8_t report_max_size, int * out_report_size, uint8_t * out_report)){
    if (callback == NULL){
        callback = dummy_write_report;
    }
    hci_device_write_report = callback;
}

void hid_device_register_set_report_callback(hid_handshake_param_type_t (*callback) (uint16_t hid_cid, hid_report_type_t report_type, int report_size, uint8_t * report)){
    if (callback == NULL){
        callback = dummy_set_report;
    }
    hci_device_set_report = callback;
}

/**
 * @brief Request can send now event to send HID Report
 * @param hid_cid
 */
void hid_device_request_can_send_now_event(uint16_t hid_cid){
    hid_device_t * hid_device = hid_device_get_instance_for_cid(hid_cid);
    if (!hid_device || !hid_device->control_cid) return;
    l2cap_request_can_send_now_event(hid_device->control_cid);
}

/**
 * @brief Send HID messageon interrupt channel
 * @param hid_cid
 */
void hid_device_send_interrupt_message(uint16_t hid_cid, const uint8_t * message, uint16_t message_len){
    hid_device_t * hid_device = hid_device_get_instance_for_cid(hid_cid);
    if (!hid_device || !hid_device->interrupt_cid) return;
    l2cap_send(hid_device->interrupt_cid, (uint8_t*) message, message_len);
}

/**
 * @brief Send HID messageon control channel
 * @param hid_cid
 */
void hid_device_send_control_message(uint16_t hid_cid, const uint8_t * message, uint16_t message_len){
    hid_device_t * hid_device = hid_device_get_instance_for_cid(hid_cid);
    if (!hid_device || !hid_device->control_cid) return;
    l2cap_send(hid_device->control_cid, (uint8_t*) message, message_len);
}

/*
 * @brief Create HID connection to HID Host
 * @param addr
 * @param hid_cid to use for other commands
 * @result status
 */
uint8_t hid_device_connect(bd_addr_t addr, uint16_t * hid_cid){
    hid_device_t * hid_device = hid_device_create_instance();
    if (!hid_device){
        log_error("hid_device_connect: could not create a hid device instace");
        return BTSTACK_MEMORY_ALLOC_FAILED; 
    }
    // assign hic_cid
    *hid_cid = hid_device_get_next_cid();
    // store address
    memcpy(hid_device->bd_addr, addr, 6);

    // reset state
    hid_device->cid           = *hid_cid;
    hid_device->incoming      = 0;
    hid_device->connected     = 0;
    hid_device->control_cid   = 0;
    hid_device->interrupt_cid = 0;
    // create l2cap control using fixed HID L2CAP PSM
    log_info("Create outgoing HID Control");
    uint8_t status = l2cap_create_channel(packet_handler, hid_device->bd_addr, PSM_HID_CONTROL, 48, &hid_device->control_cid);
    return status;
}

/*
 * @brief Disconnect from HID Host
 * @param hid_cid
 * @result status
 */
void hid_device_disconnect_interrupt_channel(uint16_t hid_cid){
    hid_device_t * hid_device = hid_device_get_instance_for_cid(hid_cid);
    if (!hid_device){
        log_error("hid_device_disconnect_interrupt_channel: could not find hid device instace");
        return;
    }
    log_info("Disconnect from interrupt channel HID Host");
    if (hid_device->interrupt_cid){
        l2cap_disconnect(hid_device->interrupt_cid, 0);  // reason isn't used
    }
}

void hid_device_disconnect_control_channel(uint16_t hid_cid){
    hid_device_t * hid_device = hid_device_get_instance_for_cid(hid_cid);
    if (!hid_device){
        log_error("hid_device_disconnect_control_channel: could not find hid device instace");
        return;
    }
    log_info("Disconnect from control channel HID Host");
    if (hid_device->control_cid){
        l2cap_disconnect(hid_device->control_cid, 0);  // reason isn't used
    }
}

void hid_device_disconnect(uint16_t hid_cid){
    hid_device_t * hid_device = hid_device_get_instance_for_cid(hid_cid);
    if (!hid_device){
        log_error("hid_device_disconnect: could not find hid device instace");
        return;
    }
    log_info("Disconnect from HID Host");
    if (hid_device->interrupt_cid){
        l2cap_disconnect(hid_device->interrupt_cid, 0);  // reason isn't used
    }
    if (hid_device->control_cid){
        l2cap_disconnect(hid_device->control_cid, 0); // reason isn't used
    }
}
