
// *****************************************************************************
//
// minimal setup for SDP client over USB or UART
//
// *****************************************************************************

#include "btstack-config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>
#include <termios.h>

#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>

#include "hci.h"
#include "gap.h"
#include "btstack_memory.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "rfcomm.h"
#include "sdp.h"
#include "sdp_query_rfcomm.h"
#include "sm.h"

void show_usage();

// static bd_addr_t remote = {0x04,0x0C,0xCE,0xE4,0x85,0xD3};
static bd_addr_t remote = {0x84, 0x38, 0x35, 0x65, 0xD1, 0x15};
static bd_addr_t remote_rfcomm = {0x00, 0x00, 0x91, 0xE0, 0xD4, 0xC7};

static uint8_t rfcomm_channel_nr = 1;

static int gap_discoverable = 0;
static int gap_connectable = 0;
// static int gap_pagable = 0;
static int gap_bondable = 0;
static int gap_dedicated_bonding_mode = 0;
static int gap_mitm_protection = 0;
static uint8_t gap_auth_req = 0;
static char * gap_io_capabilities;

static int ui_passkey = 0;
static int ui_digits_for_passkey = 0;
static int ui_chars_for_pin = 0;
static uint8_t ui_pin[17];
static int ui_pin_offset = 0;

static uint16_t handle;
static uint16_t local_cid;

// SPP / RFCOMM
#define RFCOMM_SERVER_CHANNEL 1
#define HEARTBEAT_PERIOD_MS 1000
static uint16_t  rfcomm_channel_id;
static uint8_t   spp_service_buffer[150];
static uint8_t   dummy_service_buffer[150];
static uint8_t   dummy_uuid128[] = { 1,1,1,1, 1,1,1,1,  1,1,1,1, 1,1,1,1, 1,1,1,1};
static uint16_t  mtu;

// GAP INQUIRY

#define MAX_DEVICES 10
enum DEVICE_STATE { REMOTE_NAME_REQUEST, REMOTE_NAME_INQUIRED, REMOTE_NAME_FETCHED };
struct device {
    bd_addr_t  address;
    uint16_t   clockOffset;
    uint32_t   classOfDevice;
    uint8_t    pageScanRepetitionMode;
    uint8_t    rssi;
    enum DEVICE_STATE  state; 
};

#define INQUIRY_INTERVAL 5
struct device devices[MAX_DEVICES];
int deviceCount = 0;


enum STATE {INIT, W4_INQUIRY_MODE_COMPLETE, ACTIVE} ;
enum STATE state = INIT;


int getDeviceIndexForAddress( bd_addr_t addr){
    int j;
    for (j=0; j< deviceCount; j++){
        if (BD_ADDR_CMP(addr, devices[j].address) == 0){
            return j;
        }
    }
    return -1;
}

void start_scan(void){
    printf("Starting inquiry scan..\n");
    hci_send_cmd(&hci_inquiry, HCI_INQUIRY_LAP, INQUIRY_INTERVAL, 0);
}

int has_more_remote_name_requests(void){
    int i;
    for (i=0;i<deviceCount;i++) {
        if (devices[i].state == REMOTE_NAME_REQUEST) return 1;
    }
    return 0;
}

void do_next_remote_name_request(void){
    int i;
    for (i=0;i<deviceCount;i++) {
        // remote name request
        if (devices[i].state == REMOTE_NAME_REQUEST){
            devices[i].state = REMOTE_NAME_INQUIRED;
            printf("Get remote name of %s...\n", bd_addr_to_str(devices[i].address));
            hci_send_cmd(&hci_remote_name_request, devices[i].address,
                        devices[i].pageScanRepetitionMode, 0, devices[i].clockOffset | 0x8000);
            return;
        }
    }
}

static void continue_remote_names(){
    // don't get remote names for testing
    if (has_more_remote_name_requests()){
        do_next_remote_name_request();
        return;
    } 
    // start_scan();
    // accept first device
    if (deviceCount){
        memcpy(remote, devices[0].address, 6);
        printf("Inquiry scan over, using %s for outgoing connections\n", bd_addr_to_str(remote));
    } else {
        printf("Inquiry scan over but no devices found\n" );
    }
}

static void inquiry_packet_handler (uint8_t packet_type, uint8_t *packet, uint16_t size){
    bd_addr_t addr;
    int i;
    int numResponses;
    int index;

    // printf("packet_handler: pt: 0x%02x, packet[0]: 0x%02x\n", packet_type, packet[0]);
    if (packet_type != HCI_EVENT_PACKET) return;

    uint8_t event = packet[0];

    switch(event){
        case HCI_EVENT_INQUIRY_RESULT:
        case HCI_EVENT_INQUIRY_RESULT_WITH_RSSI:{
            numResponses = packet[2];
            int offset = 3;
            for (i=0; i<numResponses && deviceCount < MAX_DEVICES;i++){
                bt_flip_addr(addr, &packet[offset]);
                offset += 6;
                index = getDeviceIndexForAddress(addr);
                if (index >= 0) continue;   // already in our list
                memcpy(devices[deviceCount].address, addr, 6);

                devices[deviceCount].pageScanRepetitionMode = packet[offset];
                offset += 1;

                if (event == HCI_EVENT_INQUIRY_RESULT){
                    offset += 2; // Reserved + Reserved
                    devices[deviceCount].classOfDevice = READ_BT_24(packet, offset);
                    offset += 3;
                    devices[deviceCount].clockOffset =   READ_BT_16(packet, offset) & 0x7fff;
                    offset += 2;
                    devices[deviceCount].rssi  = 0;
                } else {
                    offset += 1; // Reserved
                    devices[deviceCount].classOfDevice = READ_BT_24(packet, offset);
                    offset += 3;
                    devices[deviceCount].clockOffset =   READ_BT_16(packet, offset) & 0x7fff;
                    offset += 2;
                    devices[deviceCount].rssi  = packet[offset];
                    offset += 1;
                }
                devices[deviceCount].state = REMOTE_NAME_REQUEST;
                printf("Device found: %s with COD: 0x%06x, pageScan %d, clock offset 0x%04x, rssi 0x%02x\n", bd_addr_to_str(addr),
                        devices[deviceCount].classOfDevice, devices[deviceCount].pageScanRepetitionMode,
                        devices[deviceCount].clockOffset, devices[deviceCount].rssi);
                deviceCount++;
            }

            break;
        }
        case HCI_EVENT_INQUIRY_COMPLETE:
            for (i=0;i<deviceCount;i++) {
                // retry remote name request
                if (devices[i].state == REMOTE_NAME_INQUIRED)
                    devices[i].state = REMOTE_NAME_REQUEST;
            }
            continue_remote_names();
            break;

        case BTSTACK_EVENT_REMOTE_NAME_CACHED:
            bt_flip_addr(addr, &packet[3]);
            printf("Cached remote name for %s: '%s'\n", bd_addr_to_str(addr), &packet[9]);
            break;

        case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
            bt_flip_addr(addr, &packet[3]);
            index = getDeviceIndexForAddress(addr);
            if (index >= 0) {
                if (packet[2] == 0) {
                    printf("Name: '%s'\n", &packet[9]);
                    devices[index].state = REMOTE_NAME_FETCHED;
                } else {
                    printf("Failed to get name: page timeout\n");
                }
            }
            continue_remote_names();
            break;

        default:
            break;
    }
}
// GAP INQUIRY END


static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){

    uint16_t psm;
    uint32_t passkey;

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (packet[0]) {
        case HCI_EVENT_INQUIRY_RESULT:
        case HCI_EVENT_INQUIRY_RESULT_WITH_RSSI:
        case HCI_EVENT_INQUIRY_COMPLETE:
        case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
            inquiry_packet_handler(packet_type, packet, size);
            break;

        case BTSTACK_EVENT_STATE:
            // bt stack activated, get started 
            if (packet[2] == HCI_STATE_WORKING){
                printf("BTstack Bluetooth Classic Test Ready\n");
                hci_send_cmd(&hci_write_inquiry_mode, 0x01); // with RSSI
                show_usage();
            }
            break;
        case GAP_DEDICATED_BONDING_COMPLETED:
            printf("GAP Dedicated Bonding Complete, status %u\n", packet[2]);
            break;

        case HCI_EVENT_CONNECTION_COMPLETE:
            if (!packet[2]){
                handle = READ_BT_16(packet, 3);
                bt_flip_addr(remote, &packet[5]);
            }
            break;

        case HCI_EVENT_USER_PASSKEY_REQUEST:
            bt_flip_addr(remote, &packet[2]);
            printf("GAP User Passkey Request for %s\nPasskey:", bd_addr_to_str(remote));
            fflush(stdout);
            ui_digits_for_passkey = 6;
            break;

        case HCI_EVENT_USER_CONFIRMATION_REQUEST:
            bt_flip_addr(remote, &packet[2]);
            passkey = READ_BT_32(packet, 8);
            printf("GAP User Confirmation Request for %s, number '%06u'\n", bd_addr_to_str(remote),passkey);
            break;

        case HCI_EVENT_PIN_CODE_REQUEST:
            bt_flip_addr(remote, &packet[2]);
            printf("GAP Legacy PIN Request for %s (press ENTER to send)\nPasskey:", bd_addr_to_str(remote));
            fflush(stdout);
            ui_chars_for_pin = 1;
            break;

        case L2CAP_EVENT_CHANNEL_OPENED:
            // inform about new l2cap connection
            bt_flip_addr(remote, &packet[3]);
            psm = READ_BT_16(packet, 11); 
            local_cid = READ_BT_16(packet, 13); 
            handle = READ_BT_16(packet, 9);
            if (packet[2] == 0) {
                printf("L2CAP Channel successfully opened: %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
                       bd_addr_to_str(remote), handle, psm, local_cid,  READ_BT_16(packet, 15));
            } else {
                printf("L2CAP connection to device %s failed. status code %u\n", bd_addr_to_str(remote), packet[2]);
            }
            break;

        case L2CAP_EVENT_INCOMING_CONNECTION: {
            // data: event (8), len(8), address(48), handle (16), psm (16), local_cid(16), remote_cid (16) 
            psm = READ_BT_16(packet, 10);
            // uint16_t l2cap_cid  = READ_BT_16(packet, 12);
            printf("L2CAP incoming connection request on PSM %u\n", psm); 
            // l2cap_accept_connection_internal(l2cap_cid);
            break;
        }

        case RFCOMM_EVENT_INCOMING_CONNECTION:
            // data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
            bt_flip_addr(remote, &packet[2]); 
            rfcomm_channel_nr = packet[8];
            rfcomm_channel_id = READ_BT_16(packet, 9);
            printf("RFCOMM channel %u requested for %s\n\r", rfcomm_channel_nr, bd_addr_to_str(remote));
            rfcomm_accept_connection_internal(rfcomm_channel_id);
            break;
            
        case RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE:
            // data: event(8), len(8), status (8), address (48), server channel(8), rfcomm_cid(16), max frame size(16)
            if (packet[2]) {
                printf("RFCOMM channel open failed, status %u\n\r", packet[2]);
            } else {
                rfcomm_channel_id = READ_BT_16(packet, 12);
                mtu = READ_BT_16(packet, 14);
                if (mtu > 60){
                    printf("BTstack libusb hack: using reduced MTU for sending instead of %u\n", mtu);
                    mtu = 60;
                }
                printf("\n\rRFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n\r", rfcomm_channel_id, mtu);
            }
            break;
            
        case RFCOMM_EVENT_CHANNEL_CLOSED:
            rfcomm_channel_id = 0;
            break;

        default:
            break;
    }
}

static void packet_handler2 (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    packet_handler(packet_type, 0, packet, size);
}


static void update_auth_req(){
    gap_auth_req = 0;
    if (gap_mitm_protection){
        gap_auth_req |= 1;  // MITM Flag
    }
    if (gap_dedicated_bonding_mode){
        gap_auth_req |= 2;  // Dedicated bonding
    } else if (gap_bondable){
        gap_auth_req |= 4;  // General bonding
    }
    printf("Authentication Requirements: %u\n", gap_auth_req);
    hci_ssp_set_authentication_requirement(gap_auth_req);
}

void handle_found_service(char * name, uint8_t port){
    printf("SDP: Service name: '%s', RFCOMM port %u\n", name, port);
    rfcomm_channel_nr = port;
}

void handle_query_rfcomm_event(sdp_query_event_t * event, void * context){
    sdp_query_rfcomm_service_event_t * ve;
            
    switch (event->type){
        case SDP_QUERY_RFCOMM_SERVICE:
            ve = (sdp_query_rfcomm_service_event_t*) event;
            handle_found_service((char*) ve->service_name, ve->channel_nr);
            break;
        case SDP_QUERY_COMPLETE:
            printf("SDP SPP Query complete\n");
            break;
        default: 
            break;
    }
}

void  heartbeat_handler(struct timer *ts){
    if (rfcomm_channel_id){
        static int counter = 0;
        char lineBuffer[30];
        sprintf(lineBuffer, "BTstack counter %04u\n\r", ++counter);
        puts(lineBuffer);
        if (rfcomm_can_send_packet_now(rfcomm_channel_id)) {
            int err = rfcomm_send_internal(rfcomm_channel_id, (uint8_t*) lineBuffer, strlen(lineBuffer));
            if (err) printf("rfcomm_send_internal -> error 0X%02x", err); 
        }   
    }
    
    run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    run_loop_add_timer(ts);
} 

void show_usage(){

    printf("\n--- Bluetooth Classic Test Console ---\n");
    printf("GAP: discoverable %u, connectable %u, bondable %u, MITM %u, dedicated bonding %u, auth_req 0x0%u, %s\n",
        gap_discoverable, gap_connectable, gap_bondable, gap_mitm_protection, gap_dedicated_bonding_mode, gap_auth_req, gap_io_capabilities);
    printf("---\n");
    printf("b/B - bondable off/on\n");
    printf("c/C - connectable off/on\n");
    printf("d/D - discoverable off/on\n");
    printf("</> - dedicated bonding off/on\n");
    printf("m/M - MITM protection off/on\n");
    // printf("a/A - pageable off/on\n");
    printf("---\n");
    printf("e - IO_CAPABILITY_DISPLAY_ONLY\n");
    printf("f - IO_CAPABILITY_DISPLAY_YES_NO\n");
    printf("g - IO_CAPABILITY_NO_INPUT_NO_OUTPUT\n");
    printf("h - IO_CAPABILITY_KEYBOARD_ONLY\n");
    printf("---\n");
    printf("i - perform inquiry and remote name request\n");
    printf("j - perform dedicated bonding to %s, MITM = %u\n", bd_addr_to_str(remote), gap_mitm_protection);
    printf("z - perform dedicated bonding to %s using legacy pairing\n", bd_addr_to_str(remote));
    printf("t - terminate HCI connection with handle 0x%04x\n", handle);
    printf("y - disable SSP\n");
    printf("---\n");
    printf("k - query %s for RFCOMM channel\n", bd_addr_to_str(remote_rfcomm));
    printf("l - create RFCOMM connection to %s using channel #%u\n",  bd_addr_to_str(remote_rfcomm), rfcomm_channel_nr);
    printf("n - send RFCOMM data\n");
    printf("u - send RFCOMM Remote Line Status Indication indicating Framing Error\n");
    printf("v - send RFCOMM Remote Port Negotiation to select 115200 baud\n");
    printf("w - query RFCOMM Remote Port Negotiation\n");
    printf("o - close RFCOMM connection\n");
    printf("---\n");
    printf("p - create L2CAP channel to SDP at addr %s\n", bd_addr_to_str(remote));
    // printf("u - create L2CAP channel to PSM #3 (RFCOMM) at addr %s\n", bd_addr_to_str(remote));
    printf("q - send L2CAP data\n");
    printf("r - send L2CAP ECHO request\n");
    printf("s - close L2CAP channel\n");
    printf("x - require SSP for outgoing SDP L2CAP channel\n");
    printf("+ - initate SSP on current connection\n");
    printf("* - send SSP User Confirm YES\n");
    printf("= - delete link key\n");
    printf("---\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

int  stdin_process(struct data_source *ds){
    char buffer;
    read(ds->fd, &buffer, 1);

    // passkey input
    if (ui_digits_for_passkey){
        if (buffer < '0' || buffer > '9') return 0;
        printf("%c", buffer);
        fflush(stdout);
        ui_passkey = ui_passkey * 10 + buffer - '0';
        ui_digits_for_passkey--;
        if (ui_digits_for_passkey == 0){
            printf("\nSending Passkey '%06u'\n", ui_passkey);
            hci_send_cmd(&hci_user_passkey_request_reply, remote, ui_passkey);
        }
        return 0;
    }
    if (ui_chars_for_pin){
        printf("%c", buffer);
        fflush(stdout);
        if (buffer == '\n'){
            printf("\nSending Pin '%s'\n", ui_pin);
            hci_send_cmd(&hci_pin_code_request_reply, &remote, ui_pin_offset, ui_pin);
        } else {
            ui_pin[ui_pin_offset++] = buffer;
        }
        return 0;
    }

    switch (buffer){
        case 'c':
            gap_connectable = 0;
            hci_connectable_control(0);
            show_usage();
            break;
        case 'C':
            gap_connectable = 1;
            hci_connectable_control(1);
            show_usage();
            break;
        case 'd':
            gap_discoverable = 0;
            hci_discoverable_control(0);
            show_usage();
            break;
        case 'D':
            gap_discoverable = 1;
            hci_discoverable_control(1);
            show_usage();
            break;
        case 'b':
            gap_bondable = 0;
            // gap_set_bondable_mode(0);
            update_auth_req();
            show_usage();
            break;
        case 'B':
            gap_bondable = 1;
            // gap_set_bondable_mode(1);
            update_auth_req();
            show_usage();
            break;
        case 'm':
            gap_mitm_protection = 0;
            update_auth_req();
            show_usage();
            break;
        case 'M':
            gap_mitm_protection = 1;
            update_auth_req();
            show_usage();
            break;

        case '<':
            gap_dedicated_bonding_mode = 0;
            update_auth_req();
            show_usage();
            break;
        case '>':
            gap_dedicated_bonding_mode = 1;
            update_auth_req();
            show_usage();
            break;

        case 'e':
            gap_io_capabilities = "IO_CAPABILITY_DISPLAY_ONLY";
            hci_ssp_set_io_capability(IO_CAPABILITY_DISPLAY_ONLY);
            show_usage();
            break;
        case 'f':
            gap_io_capabilities = "IO_CAPABILITY_DISPLAY_YES_NO";
            hci_ssp_set_io_capability(IO_CAPABILITY_DISPLAY_YES_NO);
            show_usage();
            break;
        case 'g':
            gap_io_capabilities = "IO_CAPABILITY_NO_INPUT_NO_OUTPUT";
            hci_ssp_set_io_capability(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
            show_usage();
            break;
        case 'h':
            gap_io_capabilities = "IO_CAPABILITY_KEYBOARD_ONLY";
            hci_ssp_set_io_capability(IO_CAPABILITY_KEYBOARD_ONLY);
            show_usage();
            break;

        case 'i':
            start_scan();
            break;

        case 'j':
            printf("Start dedicated bonding to %s using MITM %u\n", bd_addr_to_str(remote), gap_mitm_protection);
            gap_dedicated_bonding(remote, gap_mitm_protection);
            break;

        case 'z':
            printf("Start dedicated bonding to %s using legacy pairing\n", bd_addr_to_str(remote));
            gap_dedicated_bonding(remote, gap_mitm_protection);
            break;

        case 'y':
            printf("Disabling SSP for this session\n");
            hci_send_cmd(&hci_write_simple_pairing_mode, 0);
            break;

        case 'k':
            printf("Start SDP query for SPP service\n");
            sdp_query_rfcomm_channel_and_name_for_uuid(remote_rfcomm, 0x1101);
            break;

        case 't':
            printf("Terminate connection with handle 0x%04x\n", handle);
            hci_send_cmd(&hci_disconnect, handle, 0x13);  // remote closed connection
            break;

        case 'p':
            printf("Creating L2CAP Connection to %s, PSM SDP\n", bd_addr_to_str(remote));
            l2cap_create_channel_internal(NULL, packet_handler, remote, PSM_SDP, 100);
            break;
        // case 'u':
        //     printf("Creating L2CAP Connection to %s, PSM 3\n", bd_addr_to_str(remote));
        //     l2cap_create_channel_internal(NULL, packet_handler, remote, 3, 100);
        //     break;
        case 'q':
            printf("Send L2CAP Data\n");
            l2cap_send_internal(local_cid, (uint8_t *) "0123456789", 10);
       break;
        case 'r':
            printf("Send L2CAP ECHO Request\n");
            l2cap_send_echo_request(handle, (uint8_t *)  "Hello World!", 13);
            break;
        case 's':
            printf("L2CAP Channel Closed\n");
            l2cap_disconnect_internal(local_cid, 0);
            break;
        case 'x':
            printf("Outgoing L2CAP Channels to SDP will also require SSP\n");
            l2cap_require_security_level_2_for_outgoing_sdp();
            break;

        case 'l':
            printf("Creating RFCOMM Channel to %s #%u\n", bd_addr_to_str(remote_rfcomm), rfcomm_channel_nr);
             rfcomm_create_channel_internal(NULL, &remote_rfcomm, rfcomm_channel_nr);
            break;
        case 'n':
            printf("Send RFCOMM Data\n");   // mtu < 60 
            rfcomm_send_internal(rfcomm_channel_id, (uint8_t *) "012345678901234567890123456789012345678901234567890123456789", mtu);
            break;
        case 'u':
            printf("Sending RLS indicating framing error\n");   // mtu < 60 
            rfcomm_send_local_line_status(rfcomm_channel_id, 9);
            break;
        case 'v':
            printf("Sending RPN CMD to select 115200 baud\n");   // mtu < 60 
            rfcomm_send_port_configuration(rfcomm_channel_id, RPN_BAUD_115200, RPN_DATA_BITS_8, RPN_STOP_BITS_1_0, RPN_PARITY_NONE, 0);
            break;
        case 'w':
            printf("Sending RPN REQ to query remote port settings\n");   // mtu < 60 
            rfcomm_query_port_configuration(rfcomm_channel_id);
            break;
        case 'o':
            printf("RFCOMM Channel Closed\n");
            rfcomm_disconnect_internal(rfcomm_channel_id);
            rfcomm_channel_id = 0;
            break;

        case '+':
            printf("Initiate SSP on current connection\n");
            gap_request_security_level(handle, LEVEL_2);
            break;

        case '*':
            printf("Sending SSP User Confirmation for %s\n", bd_addr_to_str(remote));
            hci_send_cmd(&hci_user_confirmation_request_reply, remote);
            break;

        case '=':
            printf("Deleting Link Key for %s\n", bd_addr_to_str(remote));
            hci_drop_link_key_for_bd_addr(&remote);
            break;

        default:
            show_usage();
            break;

    }
    return 0;
}

void sdp_create_dummy_service(uint8_t *service, const char *name){
    
    uint8_t* attribute;
    de_create_sequence(service);
    
    // 0x0000 "Service Record Handle"
    de_add_number(service, DE_UINT, DE_SIZE_16, SDP_ServiceRecordHandle);
    de_add_number(service, DE_UINT, DE_SIZE_32, 0x10002);
    
    // 0x0001 "Service Class ID List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_ServiceClassIDList);
    attribute = de_push_sequence(service);
    {
        de_add_uuid128(attribute, &dummy_uuid128[0] );
    }
    de_pop_sequence(service, attribute);
    
    // 0x0004 "Protocol Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_ProtocolDescriptorList);
    attribute = de_push_sequence(service);
    {
        uint8_t* l2cpProtocol = de_push_sequence(attribute);
        {
            de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, 0x0100);
        }
        de_pop_sequence(attribute, l2cpProtocol);
    }
    de_pop_sequence(service, attribute);
    
    // 0x0005 "Public Browse Group"
    de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_BrowseGroupList); // public browse group
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute,  DE_UUID, DE_SIZE_16, 0x1002 );
    }
    de_pop_sequence(service, attribute);
    
    // 0x0006
    de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_LanguageBaseAttributeIDList);
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x656e);
        de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x006a);
        de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x0100);
    }
    de_pop_sequence(service, attribute);
    
    // 0x0009 "Bluetooth Profile Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_BluetoothProfileDescriptorList);
    attribute = de_push_sequence(service);
    {
        uint8_t *sppProfile = de_push_sequence(attribute);
        {
            de_add_number(sppProfile,  DE_UINT, DE_SIZE_16, 0x0100);
        }
        de_pop_sequence(attribute, sppProfile);
    }
    de_pop_sequence(service, attribute);
    
    // 0x0100 "ServiceName"
    de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0100);
    de_add_data(service,  DE_STRING, strlen(name), (uint8_t *) name);
}


static data_source_t stdin_source;
void setup_cli(){

    struct termios term = {0};
    if (tcgetattr(0, &term) < 0)
            perror("tcsetattr()");
    term.c_lflag &= ~ICANON;
    term.c_lflag &= ~ECHO;
    term.c_cc[VMIN] = 1;
    term.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &term) < 0)
            perror("tcsetattr ICANON");

    stdin_source.fd = 0; // stdin
    stdin_source.process = &stdin_process;
    run_loop_add_data_source(&stdin_source);
}

static void btstack_setup(){
    printf("Starting up..\n");
    /// GET STARTED ///
    btstack_memory_init();
    run_loop_init(RUN_LOOP_POSIX);
    
    hci_dump_open("/tmp/hci_dump.pklg", HCI_DUMP_PACKETLOGGER);
   
    hci_transport_t    * transport = hci_transport_usb_instance();
    hci_uart_config_t  * config = NULL;
    bt_control_t       * control   = NULL;    

    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
    hci_init(transport, config, control, remote_db);
    hci_set_class_of_device(0x200404);
    hci_disable_l2cap_timeout_check();
    hci_ssp_set_io_capability(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    gap_io_capabilities =  "IO_CAPABILITY_NO_INPUT_NO_OUTPUT";
    hci_ssp_set_authentication_requirement(0);
    hci_ssp_set_auto_accept(0);
    // gap_set_bondable_mode(0);

    l2cap_init();
    l2cap_register_packet_handler(&packet_handler2);
    
    rfcomm_init();
    rfcomm_register_packet_handler(packet_handler2);
    rfcomm_register_service_internal(NULL, RFCOMM_SERVER_CHANNEL, 150);  // reserved channel, mtu=100

    // init SDP, create record for SPP and register with SDP
    sdp_init();
    memset(spp_service_buffer, 0, sizeof(spp_service_buffer));
    sdp_create_spp_service( spp_service_buffer, RFCOMM_SERVER_CHANNEL, "SPP Counter");
    de_dump_data_element(spp_service_buffer);
    printf("SDP service record size: %u\n\r", de_get_len(spp_service_buffer));
    sdp_register_service_internal(NULL, spp_service_buffer);
    memset(dummy_service_buffer, 0, sizeof(dummy_service_buffer));
    sdp_create_dummy_service(dummy_service_buffer, "UUID128 Test");
    de_dump_data_element(dummy_service_buffer);
    printf("Dummy service record size: %u\n\r", de_get_len(dummy_service_buffer));
    sdp_register_service_internal(NULL, dummy_service_buffer);

    sdp_query_rfcomm_register_callback(handle_query_rfcomm_event, NULL);
    
    hci_discoverable_control(0);
    hci_connectable_control(0);

    // turn on!
    hci_power_control(HCI_POWER_ON);
}

int main(void){
    btstack_setup();
    setup_cli();

    // set one-shot timer
    // timer_source_t heartbeat;
    // heartbeat.process = &heartbeat_handler;
    // run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
    // run_loop_add_timer(&heartbeat);

    run_loop_execute(); 
    return 0;
}
