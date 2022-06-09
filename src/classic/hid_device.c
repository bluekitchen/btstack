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

#define BTSTACK_FILE__ "hid_device.c"

#include <string.h>

#include "bluetooth.h"
#include "bluetooth_psm.h"
#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_hid_parser.h"
#include "classic/hid_device.h"
#include "classic/sdp_util.h"
#include "l2cap.h"

// prototypes
static int dummy_write_report(uint16_t hid_cid, hid_report_type_t report_type, uint16_t report_id, int * out_report_size, uint8_t * out_report);
static void dummy_set_report(uint16_t hid_cid, hid_report_type_t report_type, int report_size, uint8_t * report);
static void dummy_report_data(uint16_t hid_cid, hid_report_type_t report_type, uint16_t report_id, int report_size, uint8_t * report);

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
    uint8_t   report_id;
    uint16_t  expected_report_size;
    uint16_t  report_size;
    uint8_t   user_request_can_send_now;

    hid_handshake_param_type_t report_status;
    hid_protocol_mode_t protocol_mode;
} hid_device_t;

// higher layer callbacks
static btstack_packet_handler_t hid_device_callback;
static int  (*hci_device_get_report)   (uint16_t hid_cid, hid_report_type_t report_type, uint16_t report_id, int * out_report_size, uint8_t * out_report) = dummy_write_report;
static void (*hci_device_set_report)   (uint16_t hid_cid, hid_report_type_t report_type, int report_size, uint8_t * report) = dummy_set_report;
static void (*hci_device_report_data)  (uint16_t hid_cid, hid_report_type_t report_type, uint16_t report_id, int report_size, uint8_t * report) = dummy_report_data;

static hid_device_t    hid_device_singleton;

static bool            hid_device_boot_protocol_mode_supported;
static const uint8_t * hid_device_descriptor;
static uint16_t        hid_device_descriptor_len;


static uint16_t hid_device_cid = 0;


static int dummy_write_report(uint16_t hid_cid, hid_report_type_t report_type, uint16_t report_id, int * out_report_size, uint8_t * out_report){
    UNUSED(hid_cid);
    UNUSED(report_type);
    UNUSED(report_id);
    UNUSED(out_report_size);
    UNUSED(out_report);
    return -1;
}

static void dummy_set_report(uint16_t hid_cid, hid_report_type_t report_type, int report_size, uint8_t * report){
    UNUSED(hid_cid);
    UNUSED(report_type);
    UNUSED(report_size);
    UNUSED(report);
}

static void dummy_report_data(uint16_t hid_cid, hid_report_type_t report_type, uint16_t report_id, int report_size, uint8_t * report){
    UNUSED(hid_cid);
    UNUSED(report_type);
    UNUSED(report_id);
    UNUSED(report_size);
    UNUSED(report);
}
static uint16_t hid_device_get_next_cid(void){
    hid_device_cid++;
    if (!hid_device_cid){
        hid_device_cid = 1;
    }
    return hid_device_cid;
}

// TODO: store hid device connection into list
static hid_device_t * hid_device_get_instance_for_l2cap_cid(uint16_t cid){
    if ((hid_device_singleton.control_cid == cid) || (hid_device_singleton.interrupt_cid == cid)){
        return &hid_device_singleton;
    }
    return NULL;
}

static hid_device_t * hid_device_get_instance_for_hid_cid(uint16_t hid_cid){
    if (hid_device_singleton.cid == hid_cid){
        return &hid_device_singleton;
    }
    return NULL;
}

static hid_device_t * hid_device_provide_instance_for_bd_addr(bd_addr_t bd_addr){
    if (!hid_device_singleton.cid){
        (void)memcpy(hid_device_singleton.bd_addr, bd_addr, 6);
        hid_device_singleton.cid = hid_device_get_next_cid();
        hid_device_singleton.protocol_mode = HID_PROTOCOL_MODE_REPORT;
        hid_device_singleton.con_handle = HCI_CON_HANDLE_INVALID;
    }
    return &hid_device_singleton;
}

static hid_device_t * hid_device_create_instance(void){

    return &hid_device_singleton;
}

void hid_create_sdp_record(uint8_t *service, uint32_t service_record_handle, const hid_sdp_record_t * params){
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
            de_add_number(l2cpProtocol,  DE_UINT, DE_SIZE_16, BLUETOOTH_PSM_HID_CONTROL);
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
                de_add_number(l2cpProtocol,  DE_UINT, DE_SIZE_16, BLUETOOTH_PSM_HID_INTERRUPT);
            }
            de_pop_sequence(additionalDescriptorAttribute, l2cpProtocol);

            uint8_t * hidProtocol = de_push_sequence(additionalDescriptorAttribute);
            {
                de_add_number(hidProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_HIDP); 
            }
            de_pop_sequence(additionalDescriptorAttribute, hidProtocol);
        }
        de_pop_sequence(attribute, additionalDescriptorAttribute);
    }
    de_pop_sequence(service, attribute); 

    // 0x0100 "ServiceName"
    de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0100);
    de_add_data(service,  DE_STRING, (uint16_t) strlen(params->device_name), (uint8_t *) params->device_name);

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
    de_add_number(service,  DE_UINT, DE_SIZE_8,  params->hid_device_subclass);
    
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_HID_COUNTRY_CODE);
    de_add_number(service,  DE_UINT, DE_SIZE_8,  params->hid_country_code);

    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_HID_VIRTUAL_CABLE);
    de_add_number(service,  DE_BOOL, DE_SIZE_8,  params->hid_virtual_cable);

    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_HID_RECONNECT_INITIATE); 
    de_add_number(service,  DE_BOOL, DE_SIZE_8,  params->hid_reconnect_initiate); 

    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_HID_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t* hidDescriptor = de_push_sequence(attribute);
        {
            de_add_number(hidDescriptor,  DE_UINT, DE_SIZE_8, 0x22);    // Report Descriptor
            de_add_data(hidDescriptor,  DE_STRING, params->hid_descriptor_size, (uint8_t *) params->hid_descriptor);
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

    // battery power 

    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_HID_REMOTE_WAKE); 
    de_add_number(service,  DE_BOOL, DE_SIZE_8,  params->hid_remote_wake ? 1 : 0);

    // supervision timeout
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_HID_SUPERVISION_TIMEOUT); 
    de_add_number(service,  DE_UINT, DE_SIZE_16, params->hid_supervision_timeout);

    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_HID_NORMALLY_CONNECTABLE); 
    de_add_number(service,  DE_BOOL, DE_SIZE_8,  params->hid_normally_connectable); 

    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_HID_BOOT_DEVICE); 
    de_add_number(service,  DE_BOOL, DE_SIZE_8,  params->hid_boot_device ? 1 : 0);

    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_HIDSSR_HOST_MAX_LATENCY); 
    de_add_number(service,  DE_UINT, DE_SIZE_16, params->hid_ssr_host_max_latency);

    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_HIDSSR_HOST_MIN_TIMEOUT); 
    de_add_number(service,  DE_UINT, DE_SIZE_16, params->hid_ssr_host_min_timeout);
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
    hid_device_callback(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}   

static inline void hid_device_emit_event(hid_device_t * context, uint8_t subevent_type){
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_HID_META;
    pos++;  // skip len
    event[pos++] = subevent_type;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    event[1] = pos - 2;
    hid_device_callback(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}

static int hid_report_size_valid(uint16_t cid, int report_id, hid_report_type_t report_type, int report_size){
    if (!report_size) return 0;
    if (hid_device_in_boot_protocol_mode(cid)){
        switch (report_id){
            case HID_BOOT_MODE_KEYBOARD_ID:
                if (report_size < 8) return 0;
                break;
            case HID_BOOT_MODE_MOUSE_ID:
                if (report_size < 1) return 0;
                break;
            default:
                return 0;
        }
    } else {
        int size =  btstack_hid_get_report_size_for_id(report_id, report_type, hid_device_descriptor_len, hid_device_descriptor);
        if ((size == 0) || (size != report_size)) return 0;
    }
    return 1;
}

static int hid_get_report_size_for_id(uint16_t cid, int report_id, hid_report_type_t report_type, uint16_t descriptor_len, const uint8_t * descriptor){
    if (hid_device_in_boot_protocol_mode(cid)){
        switch (report_id){
            case HID_BOOT_MODE_KEYBOARD_ID:
                return 8;
            case HID_BOOT_MODE_MOUSE_ID:
                return 3;
            default:
                return 0;
        }
    } else {
        return btstack_hid_get_report_size_for_id(report_id, report_type, descriptor_len, descriptor);
    }
}

static hid_report_id_status_t hid_report_id_status(uint16_t cid, uint16_t report_id){
    if (hid_device_in_boot_protocol_mode(cid)){
        switch (report_id){
            case HID_BOOT_MODE_KEYBOARD_ID:
            case HID_BOOT_MODE_MOUSE_ID:
                return HID_REPORT_ID_VALID;
            default:
                return HID_REPORT_ID_INVALID;
        }
    } else {
        return btstack_hid_id_valid(report_id, hid_device_descriptor_len, hid_device_descriptor);
    }
}

static hid_handshake_param_type_t hid_device_set_report_cmd_is_valid(uint16_t cid, hid_report_type_t report_type, int report_size, uint8_t * report){
    int pos = 0;
    int report_id = 0;

    if (btstack_hid_report_id_declared(hid_device_descriptor_len, hid_device_descriptor)){
        report_id = report[pos++];
        hid_report_id_status_t report_id_status = hid_report_id_status(cid, report_id);
        switch (report_id_status){
            case HID_REPORT_ID_INVALID:
                return HID_HANDSHAKE_PARAM_TYPE_ERR_INVALID_REPORT_ID;
            default:
                break;
        }
    }
    
    if (!hid_report_size_valid(cid, report_id, report_type, report_size-pos)){
        // TODO clarify DCT/BI-03c
        return HID_HANDSHAKE_PARAM_TYPE_ERR_INVALID_PARAMETER;
    }
    return HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL;
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
    uint16_t local_cid;
    int pos = 0;
    int report_size;
    uint8_t report[48];
    hid_message_type_t message_type;

    switch (packet_type){
        case L2CAP_DATA_PACKET:
            device = hid_device_get_instance_for_l2cap_cid(channel);
            if (!device) {
                log_error("no device with cid 0x%02x", channel);
                return;
            }
            message_type = (hid_message_type_t)(packet[0] >> 4);
            switch (message_type){
                case HID_MESSAGE_TYPE_GET_REPORT:

                    pos = 0;
                    device->report_type = (hid_report_type_t)(packet[pos++] & 0x03);
                    device->report_id = 0;
                    device->report_status = HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL;
                    device->state = HID_DEVICE_W2_GET_REPORT;

                    switch (device->protocol_mode){
                        case HID_PROTOCOL_MODE_BOOT: 
                            if (packet_size < 2){
                                device->report_status = HID_HANDSHAKE_PARAM_TYPE_ERR_INVALID_PARAMETER;
                                break;
                            }
                            device->report_id = packet[pos++];
                            break;
                        case HID_PROTOCOL_MODE_REPORT:
                            if (!btstack_hid_report_id_declared(hid_device_descriptor_len, hid_device_descriptor)) {
                                if (packet_size < 2) break;
                                if (packet[0] & 0x08){ 
                                    if (packet_size > 2) {
                                        device->report_status = HID_HANDSHAKE_PARAM_TYPE_ERR_INVALID_REPORT_ID;
                                    }
                                } else {
                                    if (packet_size > 1) {
                                        device->report_status = HID_HANDSHAKE_PARAM_TYPE_ERR_INVALID_REPORT_ID;
                                    }
                                }
                                break;
                            }
                            if (packet_size < 2){
                                device->report_status = HID_HANDSHAKE_PARAM_TYPE_ERR_INVALID_PARAMETER;
                                break;
                            }
                            device->report_id = packet[pos++];
                            break;
                        default:
                            btstack_assert(false);
                            break;
                    }
                    if (device->report_status != HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL){
                        l2cap_request_can_send_now_event(device->control_cid);
                        break;
                    } 
                    switch (hid_report_id_status(device->cid, device->report_id)){
                        case HID_REPORT_ID_INVALID:
                            device->report_status = HID_HANDSHAKE_PARAM_TYPE_ERR_INVALID_REPORT_ID;
                            break;
                        default:
                            break;
                    }
                    
                    device->expected_report_size = hid_get_report_size_for_id(device->cid, device->report_id, device->report_type, hid_device_descriptor_len, hid_device_descriptor);
                    report_size =  device->expected_report_size + pos; // add 1 for header size and report id
                    
                    if ((packet[0] & 0x08) && (packet_size >= (pos + 1))){
                        device->report_size = btstack_min(btstack_min(little_endian_read_16(packet, pos), report_size), sizeof(report));
                    } else {
                        device->report_size = btstack_min(btstack_min(l2cap_max_mtu(), report_size), sizeof(report));
                    }

                    l2cap_request_can_send_now_event(device->control_cid);
                    break;

                case HID_MESSAGE_TYPE_SET_REPORT:
                    device->state = HID_DEVICE_W2_SET_REPORT;  
                    device->report_size = l2cap_max_mtu();
                    device->report_type = (hid_report_type_t)(packet[0] & 0x03);
                    if (packet_size < 1){
                        device->report_status = HID_HANDSHAKE_PARAM_TYPE_ERR_INVALID_PARAMETER;
                        break;
                    }

                    switch (device->protocol_mode){
                        case HID_PROTOCOL_MODE_BOOT: 
                            if (packet_size < 3){
                                device->report_status = HID_HANDSHAKE_PARAM_TYPE_ERR_INVALID_PARAMETER;
                                break;
                            }
                            device->report_id = packet[1];
                            device->report_status = hid_device_set_report_cmd_is_valid(device->cid, device->report_type, packet_size - 1, &packet[1]);
                            if (device->report_status != HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL) break;
                            (*hci_device_set_report)(device->cid, device->report_type, packet_size-1, &packet[1]);
                            break;
                        case HID_PROTOCOL_MODE_REPORT:
                            device->report_status = hid_device_set_report_cmd_is_valid(device->cid, device->report_type, packet_size - 1, &packet[1]);
                            if (device->report_status != HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL) break;
                            
                            if (packet_size >= 2){
                                (*hci_device_set_report)(device->cid, device->report_type, packet_size-1, &packet[1]);
                            } else {
                                uint8_t payload[] = {0};
                                (*hci_device_set_report)(device->cid, device->report_type, 1, payload);
                            } 
                            break;
                        default:
                            btstack_assert(false);
                            break;
                    }
                    device->report_type = (hid_report_type_t)(packet[0] & 0x03);
                    l2cap_request_can_send_now_event(device->control_cid);
                    break;
                case HID_MESSAGE_TYPE_GET_PROTOCOL:
                    device->state = HID_DEVICE_W2_GET_PROTOCOL;  
                    if (packet_size != 1) {
                        device->report_status = HID_HANDSHAKE_PARAM_TYPE_ERR_INVALID_PARAMETER;
                        break;
                    }
                    device->report_status = HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL;
                    // hid_device_request_can_send_now_event(channel);
                    l2cap_request_can_send_now_event(device->control_cid);
                    break;

                case HID_MESSAGE_TYPE_SET_PROTOCOL:
                    device->state = HID_DEVICE_W2_SET_PROTOCOL;  
                    if (packet_size != 1) {
                        device->report_status = HID_HANDSHAKE_PARAM_TYPE_ERR_INVALID_PARAMETER;
                        break;
                    }
                    param = packet[0] & 0x01;
                    if (((hid_protocol_mode_t)param == HID_PROTOCOL_MODE_BOOT) && !hid_device_boot_protocol_mode_supported){
                        device->report_status = HID_HANDSHAKE_PARAM_TYPE_ERR_INVALID_PARAMETER;
                        break;
                    }
                    device->protocol_mode = (hid_protocol_mode_t) param;
                    switch (device->protocol_mode){
                        case HID_PROTOCOL_MODE_BOOT: 
                            break;
                        case HID_PROTOCOL_MODE_REPORT:
                            break;
                        default:
                            btstack_assert(false);
                            break;
                    }
                    device->report_status = HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL;
                    l2cap_request_can_send_now_event(device->control_cid);
                    break;

                case HID_MESSAGE_TYPE_HID_CONTROL:
                    param = packet[0] & 0x0F;

                    switch ((hid_control_param_t)param){
                        case HID_CONTROL_PARAM_SUSPEND:
                            hid_device_emit_event(device, HID_SUBEVENT_SUSPEND);
                            break;
                        case HID_CONTROL_PARAM_EXIT_SUSPEND:
                            hid_device_emit_event(device, HID_SUBEVENT_EXIT_SUSPEND);
                            break;
                        case HID_CONTROL_PARAM_VIRTUAL_CABLE_UNPLUG:
                            hid_device_emit_event(device, HID_SUBEVENT_VIRTUAL_CABLE_UNPLUG);
                            break;
                        default:
                            device->state = HID_DEVICE_W2_SEND_UNSUPPORTED_REQUEST;
                            l2cap_request_can_send_now_event(device->control_cid);
                            break;
                    }
                    break;

                case HID_MESSAGE_TYPE_DATA:
                    if (packet_size < 2) {
                        break;
                    }
                    pos = 0;
                    device->report_type = (hid_report_type_t)(packet[pos++] & 0x03);
                    device->report_id = 0;
                    if (btstack_hid_report_id_declared(hid_device_descriptor_len, hid_device_descriptor)){
                        device->report_id = packet[pos++];
                    }
                    
                    if (hid_report_id_status(device->cid, device->report_id) == HID_REPORT_ID_INVALID){
                        log_info("Ignore invalid report data packet");
                        break;
                    }
                    if (!hid_report_size_valid(device->cid, device->report_id, device->report_type, packet_size - pos)){
                        log_info("Ignore invalid report data packet, invalid size");
                        break;
                    }
                    (*hci_device_report_data)(device->cid, device->report_type, device->report_id, packet_size - pos, &packet[pos]);
                    break;
                default:
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
                            device = hid_device_provide_instance_for_bd_addr(address);
                            if (!device) {
                                log_error("L2CAP_EVENT_INCOMING_CONNECTION, cannot create instance for %s", bd_addr_to_str(address));
                                l2cap_decline_connection(channel);
                                break;
                            }
                            if ((device->con_handle == HCI_CON_HANDLE_INVALID) || (l2cap_event_incoming_connection_get_handle(packet) == device->con_handle)){
                                device->con_handle = l2cap_event_incoming_connection_get_handle(packet);
                                device->incoming = 1;
                                l2cap_event_incoming_connection_get_address(packet, device->bd_addr);
                                psm = l2cap_event_incoming_connection_get_psm(packet);
                                switch (psm){
                                    case PSM_HID_CONTROL:
                                        device->control_cid = l2cap_event_incoming_connection_get_local_cid(packet);
                                        break;
                                    case PSM_HID_INTERRUPT:
                                        device->interrupt_cid = l2cap_event_incoming_connection_get_local_cid(packet);
                                    break;
                                    default:
                                        break;
                                }

                                l2cap_accept_connection(channel);
                            } else {
                                l2cap_decline_connection(channel);
                                log_info("L2CAP_EVENT_INCOMING_CONNECTION, decline connection for %s", bd_addr_to_str(address));
                            }
                            break;
                        default:
                            l2cap_decline_connection(channel);
                            break;
                    }
                    break;
                case L2CAP_EVENT_CHANNEL_OPENED:
                    device = hid_device_get_instance_for_l2cap_cid(l2cap_event_channel_opened_get_local_cid(packet));
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

                    // store con_handle
                    if (device->con_handle == HCI_CON_HANDLE_INVALID){
                        device->con_handle  = l2cap_event_channel_opened_get_handle(packet);
                    }

                    // store l2cap cid
                    psm = l2cap_event_channel_opened_get_psm(packet);
                    switch (psm){
                        case PSM_HID_CONTROL:
                            device->control_cid = l2cap_event_channel_opened_get_local_cid(packet);
                            break;
                        case PSM_HID_INTERRUPT:
                            device->interrupt_cid = l2cap_event_channel_opened_get_local_cid(packet);
                            break;
                        default:
                            break;
                    }

                    // connect HID Interrupt for outgoing
                    if ((device->incoming == 0) && (psm == PSM_HID_CONTROL)){
                        status = l2cap_create_channel(packet_handler, device->bd_addr, PSM_HID_INTERRUPT, 48, &device->interrupt_cid);
                        break;
                    }

                    // emit connected if both channels are open
                    connected_before = device->connected;
                    if (!connected_before && device->control_cid && device->interrupt_cid){
                        device->connected = 1;
                        hid_device_emit_connected_event(device, 0);
                    }
                    break;
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    device = hid_device_get_instance_for_l2cap_cid(l2cap_event_channel_closed_get_local_cid(packet));
                    if (!device) return;

                    // connected_before = device->connected;
                    device->incoming  = 0;
                    if (l2cap_event_channel_closed_get_local_cid(packet) == device->interrupt_cid){
                        device->interrupt_cid = 0;
                    }
                    if (l2cap_event_channel_closed_get_local_cid(packet) == device->control_cid){
                        device->control_cid = 0;
                    }
                    if (!device->interrupt_cid && !device->control_cid){
                        device->connected = 0;
                        device->con_handle = HCI_CON_HANDLE_INVALID;
                        device->cid = 0;
                        hid_device_emit_event(device, HID_SUBEVENT_CONNECTION_CLOSED);
                    }
                    break;

                case L2CAP_EVENT_CAN_SEND_NOW:
                    local_cid = l2cap_event_can_send_now_get_local_cid(packet);
                    device = hid_device_get_instance_for_l2cap_cid(local_cid);
                    
                    if (!device) return;
                    switch (device->state){
                        case HID_DEVICE_W2_GET_REPORT:{
                            if (device->report_status != HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL) {
                                report[0] = (HID_MESSAGE_TYPE_HANDSHAKE << 4) | device->report_status;
                                hid_device_send_control_message(device->cid, &report[0], 1);
                                break;
                            }

                            pos = 0;
                            report[pos++] = (HID_MESSAGE_TYPE_DATA << 4) | device->report_type;
                            if (device->report_id){
                                report[pos++] = device->report_id;
                            }
                            
                            report_size = 0;
                            status = (*hci_device_get_report)(device->cid, device->report_type, device->report_id, &report_size, &report[pos]);

                            switch (status){
                                case 0:
                                    device->report_status = HID_HANDSHAKE_PARAM_TYPE_NOT_READY;
                                    break;
                                case 1:
                                    if (report_size == 0){
                                        device->report_status = HID_HANDSHAKE_PARAM_TYPE_ERR_UNSUPPORTED_REQUEST;
                                        break;
                                    }
                                    if (device->expected_report_size != report_size){
                                        log_error("Expected report size of %d bytes, received %d", device->expected_report_size, report_size);
                                        device->report_status = HID_HANDSHAKE_PARAM_TYPE_ERR_UNSUPPORTED_REQUEST;
                                        break;
                                    }
                                    break;
                                default:
                                    device->report_status = HID_HANDSHAKE_PARAM_TYPE_ERR_UNSUPPORTED_REQUEST;
                                    break;
                            }
                            if (device->report_status != HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL){
                                report[0] = (HID_MESSAGE_TYPE_HANDSHAKE << 4) | device->report_status;
                                hid_device_send_control_message(device->cid, &report[0], 1);
                                break;
                            }
                            
                            // if (report_size > l2cap_max_mtu()){
                            //     report[0] = (HID_MESSAGE_TYPE_HANDSHAKE << 4) | HID_HANDSHAKE_PARAM_TYPE_ERR_INVALID_PARAMETER;
                            //     hid_device_send_control_message(device->cid, &report[0], 1);
                            //     break;
                            // }

                            hid_device_send_control_message(device->cid, &report[0], device->report_size);
                            //     device->state = HID_DEVICE_IDLE;
                            break;
                        }
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

                            report[0] = (HID_MESSAGE_TYPE_DATA << 4);
                            report[1] =  device->protocol_mode;
                            hid_device_send_control_message(device->cid, &report[0], 2);
                            break;
                            

                        case HID_DEVICE_W2_SEND_UNSUPPORTED_REQUEST:
                            report[0] = (HID_MESSAGE_TYPE_HANDSHAKE << 4) | HID_HANDSHAKE_PARAM_TYPE_ERR_UNSUPPORTED_REQUEST;
                            hid_device_send_control_message(device->cid, &report[0], 1);
                            break;
                        default:
                            if (device->user_request_can_send_now){
                                device->user_request_can_send_now = 0;
                                hid_device_emit_event(device, HID_SUBEVENT_CAN_SEND_NOW);
                            }
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
void hid_device_init(bool boot_protocol_mode_supported, uint16_t descriptor_len, const uint8_t * descriptor){
    hid_device_boot_protocol_mode_supported = boot_protocol_mode_supported;
    hid_device_descriptor =  descriptor;
    hid_device_descriptor_len = descriptor_len;
    hci_device_get_report = dummy_write_report;
    hci_device_set_report = dummy_set_report;
    hci_device_report_data = dummy_report_data;

    l2cap_register_service(packet_handler, PSM_HID_INTERRUPT, 100, gap_get_security_level());
    l2cap_register_service(packet_handler, PSM_HID_CONTROL,   100, gap_get_security_level());
}

void hid_device_deinit(void){
    hid_device_callback = NULL;
    hci_device_get_report = NULL;
    hci_device_set_report = NULL;
    hci_device_report_data = NULL;

    (void) memset(&hid_device_singleton, 0, sizeof(hid_device_t));

    hid_device_boot_protocol_mode_supported = false;
    hid_device_descriptor = NULL;
    hid_device_descriptor_len = 0;
    hid_device_cid = 0;
}

/**
 * @brief Register callback for the HID Device client. 
 * @param callback
 */
void hid_device_register_packet_handler(btstack_packet_handler_t callback){
    hid_device_callback = callback;
}

void hid_device_register_report_request_callback(int (*callback)(uint16_t hid_cid, hid_report_type_t report_type, uint16_t report_id, int * out_report_size, uint8_t * out_report)){
    if (callback == NULL){
        callback = dummy_write_report;
    }
    hci_device_get_report = callback;
}

void hid_device_register_set_report_callback(void (*callback)(uint16_t hid_cid, hid_report_type_t report_type, int report_size, uint8_t * report)){
    if (callback == NULL){
        callback = dummy_set_report;
    }
    hci_device_set_report = callback;
}

void hid_device_register_report_data_callback(void (*callback)(uint16_t cid, hid_report_type_t report_type, uint16_t report_id, int report_size, uint8_t * report)){
    if (callback == NULL){
        callback = dummy_report_data;
    }
    hci_device_report_data = callback;
}


/**
 * @brief Request can send now event to send HID Report
 * @param hid_cid
 */
void hid_device_request_can_send_now_event(uint16_t hid_cid){
    hid_device_t * hid_device = hid_device_get_instance_for_hid_cid(hid_cid);
    if (!hid_device || !hid_device->interrupt_cid) return;
    hid_device->user_request_can_send_now = 1;
    l2cap_request_can_send_now_event(hid_device->interrupt_cid);
}

/**
 * @brief Send HID message on interrupt channel
 * @param hid_cid
 */
void hid_device_send_interrupt_message(uint16_t hid_cid, const uint8_t * message, uint16_t message_len){
    hid_device_t * hid_device = hid_device_get_instance_for_hid_cid(hid_cid);
    if (!hid_device || !hid_device->interrupt_cid) return;
    l2cap_send(hid_device->interrupt_cid, (uint8_t*) message, message_len);
    if (hid_device->user_request_can_send_now){
        l2cap_request_can_send_now_event((hid_device->interrupt_cid));
    }
}

/**
 * @brief Send HID message on control channel
 * @param hid_cid
 */
void hid_device_send_control_message(uint16_t hid_cid, const uint8_t * message, uint16_t message_len){
    hid_device_t * hid_device = hid_device_get_instance_for_hid_cid(hid_cid);
    if (!hid_device || !hid_device->control_cid) return;
    l2cap_send(hid_device->control_cid, (uint8_t*) message, message_len);
    // request user can send now if pending
    if (hid_device->user_request_can_send_now){
        l2cap_request_can_send_now_event((hid_device->control_cid));
    }
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
    (void)memcpy(hid_device->bd_addr, addr, 6);

    // reset state
    hid_device->cid           = *hid_cid;
    hid_device->incoming      = 0;
    hid_device->connected     = 0;
    hid_device->control_cid   = 0;
    hid_device->interrupt_cid = 0;
    hid_device->con_handle    = HCI_CON_HANDLE_INVALID;

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
    hid_device_t * hid_device = hid_device_get_instance_for_hid_cid(hid_cid);
    if (!hid_device){
        log_error("hid_device_disconnect_interrupt_channel: could not find hid device instace");
        return;
    }
    log_info("Disconnect from interrupt channel HID Host");
    if (hid_device->interrupt_cid){
        l2cap_disconnect(hid_device->interrupt_cid);
    }
}

void hid_device_disconnect_control_channel(uint16_t hid_cid){
    hid_device_t * hid_device = hid_device_get_instance_for_hid_cid(hid_cid);
    if (!hid_device){
        log_error("hid_device_disconnect_control_channel: could not find hid device instace");
        return;
    }
    log_info("Disconnect from control channel HID Host");
    if (hid_device->control_cid){
        l2cap_disconnect(hid_device->control_cid);
    }
}

void hid_device_disconnect(uint16_t hid_cid){
    hid_device_t * hid_device = hid_device_get_instance_for_hid_cid(hid_cid);
    if (!hid_device){
        log_error("hid_device_disconnect: could not find hid device instace");
        return;
    }
    log_info("Disconnect from HID Host");
    if (hid_device->interrupt_cid){
        l2cap_disconnect(hid_device->interrupt_cid);
    }
    if (hid_device->control_cid){
        l2cap_disconnect(hid_device->control_cid);
        }
}

int hid_device_in_boot_protocol_mode(uint16_t hid_cid){
    hid_device_t * hid_device = hid_device_get_instance_for_hid_cid(hid_cid);
    if (!hid_device){
        log_error("hid_device_in_boot_protocol_mode: could not find hid device instace");
        return 0;
    }
    return hid_device->protocol_mode == HID_PROTOCOL_MODE_BOOT;
}
