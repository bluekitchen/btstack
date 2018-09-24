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
    HID_DEVICE_W2_SEND_REPORT,
    HID_DEVICE_W2_SEND_UNSUPPORTED_REQUEST
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
    uint16_t          report_id;
} hid_device_t;

static hid_device_t _hid_device;

static void dummy_write_report(uint16_t hid_cid, hid_report_type_t report_type, uint16_t report_id, uint8_t report_max_size, int * out_report_size, uint8_t * out_report){
    UNUSED(hid_cid);
    UNUSED(report_type);
    UNUSED(report_id);
    UNUSED(report_max_size);
    UNUSED(out_report_size);
    UNUSED(out_report);
}

static void (*hci_device_write_report) (uint16_t hid_cid, hid_report_type_t report_type, uint16_t report_id, uint8_t report_max_size, int * out_report_size, uint8_t * out_report) = dummy_write_report;

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

static hid_device_t * hid_device_create_instance_for_con_handle(uint16_t con_handle){
    UNUSED(con_handle);
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

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t packet_size){
    UNUSED(channel);
    UNUSED(packet_size);
    int connected_before;
    uint16_t psm;
    uint8_t status;
    hid_device_t * device = NULL;
    uint8_t report[20];
    int report_size;

    switch (packet_type){
        case L2CAP_DATA_PACKET:
            device = hid_device_get_instance_for_cid(channel);
            if (!device) {
                log_error("no device with cid 0x%02x", channel);
                return;
            }
            hid_message_type_t message_type = packet[0] >> 4;
            switch (message_type){
                case HID_MESSAGE_TYPE_GET_REPORT:
                    device->report_type = packet[0] & 0x0F;
                    device->report_id = 0;
                    if (packet_size == 2){
                        device->report_id = packet[1];
                    }
                    device->state = HID_DEVICE_W2_SEND_REPORT;
                    printf(" answer get report type %d report_type\n", device->report_type);
                    l2cap_request_can_send_now_event(device->control_cid);
                    break;
                default:
                    printf("L2CAP_DATA_PACKET %d  \n", message_type);
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
                            device = hid_device_create_instance_for_con_handle(l2cap_event_incoming_connection_get_handle(packet));
                            if (!device) {
                                log_error("L2CAP_EVENT_INCOMING_CONNECTION, no hid device for con handle 0x%02x", l2cap_event_incoming_connection_get_handle(packet));
                                l2cap_decline_connection(channel);
                                break;
                            }
                            if (device->con_handle == 0 || l2cap_event_incoming_connection_get_handle(packet) == device->con_handle){
                                device->con_handle = l2cap_event_incoming_connection_get_handle(packet);
                                device->incoming = 1;
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

                    device->incoming  = 0;
                    device->connected = 0;
                    connected_before = device->connected;
                    if (l2cap_event_channel_closed_get_local_cid(packet) == device->control_cid){
                        log_info("HID Control closed");
                        device->control_cid = 0;
                    }
                    if (l2cap_event_channel_closed_get_local_cid(packet) == device->interrupt_cid){
                        log_info("HID Interrupt closed");
                        device->interrupt_cid = 0;
                    }
                    if (connected_before && !device->connected){
                        device->con_handle = 0;
                        log_info("HID Disconnected");
                        hid_device_emit_connection_closed_event(device);
                    }
                    break;
                case L2CAP_EVENT_CAN_SEND_NOW:

                    device = hid_device_get_instance_for_cid(l2cap_event_can_send_now_get_local_cid(packet));
                    if (!device) return;
                    switch (device->state){
                        case HID_DEVICE_W2_SEND_REPORT:
                            (*hci_device_write_report)(device->cid, device->report_type, device->report_id, (uint16_t) sizeof(report), &report_size, report );
                            hid_device_send_control_message(device->cid, &report[0], report_size);
                            break;
                        case HID_DEVICE_W2_SEND_UNSUPPORTED_REQUEST:
                            report[0] = (HID_MESSAGE_TYPE_HANDSHAKE << 4) | HID_HANDSHAKE_PARAM_TYPE_ERR_INVALID_PARAMETER;
                            hid_device_send_control_message(device->cid, &report[0], report_size);
                            break;
                        default:
                            log_info("HID Can send now, emit event");
                            hid_device_emit_can_send_now_event(device);
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
void hid_device_init(void){
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



void hid_device_register_report_request_callback(void (*callback) (uint16_t hid_cid, hid_report_type_t report_type, uint16_t report_id, uint8_t report_max_size, int * out_report_size, uint8_t * out_report)){
    if (callback == NULL){
        callback = dummy_write_report;
    }
    hci_device_write_report = callback;
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
    hid_device->incoming      = 0;
    hid_device->connected     = 0;
    hid_device->control_cid   = 0;
    hid_device->interrupt_cid = 0;

    // create l2cap control using fixed HID L2CAP PSM
    log_info("Create outgoing HID Control");
    uint8_t status = l2cap_create_channel(packet_handler, hid_device->bd_addr, PSM_HID_CONTROL, 48, &hid_device->control_cid);

    return status;
}
