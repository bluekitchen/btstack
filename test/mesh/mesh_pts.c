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

#define __BTSTACK_FILE__ "mesh_pts.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"
#include "mesh_pts.h"

// general
static void show_usage(void);
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void mesh_pts_dump_mesh_options(void);

#define MESH_BLUEKITCHEN_MODEL_ID_TEST_SERVER   0x0000u

static btstack_packet_callback_registration_t hci_event_callback_registration;
static int provisioned;

static mesh_model_t                 mesh_vendor_model;

static mesh_model_t                 mesh_generic_on_off_server_model;
static mesh_generic_on_off_state_t  mesh_generic_on_off_state;
static mesh_publication_model_t     generic_on_off_server_publication;

static mesh_model_t                 mesh_generic_level_server_model;
static mesh_generic_level_state_t   mesh_generic_level_state;
static mesh_publication_model_t     generic_level_server_publication;

static mesh_model_t                 mesh_configuration_client_model;

static char gap_name_buffer[30];
static char gap_name_prefix[] = "Mesh ";

// pts add-on
#define PTS_DEFAULT_TTL 10

static int      pts_type;

static mesh_virtual_address_t * pts_virtual_addresss;

const char * pts_device_uuid_string = "001BDC0810210B0E0A0C000B0E0A0C00";

static uint8_t      prov_static_oob_data[16];
static const char * prov_static_oob_string = "00000000000000000102030405060708";

static uint8_t      prov_public_key_data[64];
static const char * prov_public_key_string = "F465E43FF23D3F1B9DC7DFC04DA8758184DBC966204796ECCF0D6CF5E16500CC0201D048BCBBD899EEEFC424164E33C201C2B010CA6B4D43A8A155CAD8ECB279";
static uint8_t      prov_private_key_data[32];
static const char * prov_private_key_string = "529AA0670D72CD6497502ED473502B037E8803B5C60829A5A3CAA219505530BA";

static mesh_transport_key_t pts_application_key;

// pin entry (pts)
static int ui_chars_for_pin; 
static uint8_t ui_pin[17];
static int ui_pin_offset;


static mesh_health_fault_t health_fault;

static void mesh_provisioning_dump(const mesh_provisioning_data_t * data){
    mesh_network_key_t * key = data->network_key;
    printf("UnicastAddr:   0x%02x\n", data->unicast_address);
    printf("DevKey:        "); printf_hexdump(data->device_key, 16);
    printf("IV Index:      0x%08x\n", data->iv_index);
    printf("Flags:               0x%02x\n", data->flags);
    printf("NetKey:        "); printf_hexdump(key->net_key, 16);
    printf("-- Derived from NetKey --\n");
    printf("NID:           0x%02x\n", key->nid);
    printf("NetworkID:     "); printf_hexdump(key->network_id, 8);
    printf("BeaconKey:     "); printf_hexdump(key->beacon_key, 16);
    printf("EncryptionKey: "); printf_hexdump(key->encryption_key, 16);
    printf("PrivacyKey:    "); printf_hexdump(key->privacy_key, 16);
    printf("IdentityKey:   "); printf_hexdump(key->identity_key, 16);
}


static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t addr;
    int i;

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) break;
                    // dump bd_addr in pts format
                    gap_local_bd_addr(addr);
                    printf("Local addr: %s - ", bd_addr_to_str(addr));
                    for (i=0;i<6;i++) {
                        printf("%02x", addr[i]);
                    }
                    printf("\n");

                    // setup gap name
                    strcpy(gap_name_buffer, gap_name_prefix);
                    strcat(gap_name_buffer, bd_addr_to_str(addr));

                    // dump PTS MeshOptions.ini
                    mesh_pts_dump_mesh_options();
                    break;
                default:
                    break;
            }
            break;
    }
}

static void mesh_provisioning_message_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    mesh_provisioning_data_t provisioning_data;

    switch(packet[0]){
        case HCI_EVENT_MESH_META:
            switch(packet[2]){
                case MESH_SUBEVENT_PB_TRANSPORT_LINK_OPEN:
                    printf("Provisioner link opened");
                    break;
                case MESH_SUBEVENT_PB_TRANSPORT_LINK_CLOSED:
                    printf("Provisioner link close");
                    break;
                case MESH_SUBEVENT_ATTENTION_TIMER:
                    printf("Attention Timer: %u\n", mesh_subevent_attention_timer_get_attention_time(packet));
                    break;
                case MESH_SUBEVENT_PB_PROV_INPUT_OOB_REQUEST:
                    printf("Enter passphrase: ");
                    fflush(stdout);
                    ui_chars_for_pin = 1;
                    ui_pin_offset = 0;
                    break;
                case MESH_SUBEVENT_PB_PROV_COMPLETE:
                    printf("Provisioning complete\n");
                    // dump provisioning data
                    provisioning_device_data_get(&provisioning_data);
                    mesh_provisioning_dump(&provisioning_data);
                    provisioned = 1;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void mesh_state_update_message_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
   
    switch(packet[0]){
        case HCI_EVENT_MESH_META:
            switch(packet[2]){
                case MESH_SUBEVENT_STATE_UPDATE_BOOL:
                    printf("State update: model identifier 0x%08x, state identifier 0x%08x, reason %u, state %u\n",
                        mesh_subevent_state_update_bool_get_model_identifier(packet),
                        mesh_subevent_state_update_bool_get_state_identifier(packet),
                        mesh_subevent_state_update_bool_get_reason(packet),
                        mesh_subevent_state_update_bool_get_value(packet));
                    break;
                default:
                    printf("mesh_state_update_message_handler: event not parsed");
                    break;
            }
            break;
        default:
            break;
    }
}

static void mesh_configuration_message_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
   
    switch(packet[0]){
        case HCI_EVENT_MESH_META:
            switch(packet[2]){
                default:
                    printf("mesh_configuration_message_handler: event not parsed");
                    break;
            }
            break;
        default:
            break;
    }
}
// PTS

// helper network layer, temp
static void mesh_pts_received_network_message(mesh_network_callback_type_t callback_type, mesh_network_pdu_t *network_pdu){
    switch (callback_type){
        case MESH_NETWORK_PDU_RECEIVED:
            printf("Received network message. SRC %04x, DST %04x, SEQ %04x\n",
                   mesh_network_src(network_pdu), mesh_network_dst(network_pdu),  mesh_network_seq(network_pdu));
            printf_hexdump(mesh_network_pdu_data(network_pdu), mesh_network_pdu_len(network_pdu));
            mesh_network_message_processed_by_higher_layer(network_pdu);
            break;
        default:
            break;
    }
}

static uint8_t mesh_network_send(uint8_t ttl, uint16_t dest, const uint8_t * transport_pdu_data, uint8_t transport_pdu_len){

    uint16_t netkey_index = 0;
    uint8_t  ctl = 0;
    uint16_t src = mesh_node_get_primary_element_address();
    uint32_t seq = mesh_sequence_number_next();

    // "3.4.5.2: The output filter of the interface connected to advertising or GATT bearers shall drop all messages with TTL value set to 1."
    // if (ttl <= 1) return 0;

    // TODO: check transport_pdu_len depending on ctl

    // lookup network by netkey_index
    const mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (!network_key) return 0;

    // allocate network_pdu
    mesh_network_pdu_t * network_pdu = mesh_network_pdu_get();
    if (!network_pdu) return 0;

    // setup network_pdu
    mesh_network_setup_pdu(network_pdu, netkey_index, network_key->nid, ctl, ttl, seq, src, dest, transport_pdu_data, transport_pdu_len);

    // send network_pdu
    mesh_lower_transport_send_pdu((mesh_pdu_t *) network_pdu);
    return 0;
}

static void printf_hex(const uint8_t * data, uint16_t len){
    while (len){
        printf("%02x", *data);
        data++;
        len--;
    }
    printf("\n");
}

static void mesh_pts_dump_mesh_options(void){
    printf("\nMeshOptions.ini - into 'My Decoders' of Bluetooth Protocol Viewer\n");

    printf("[mesh]\n");

    printf("{IVindex}\n");
    printf("%08x\n", mesh_get_iv_index());

    mesh_network_key_t * network_key = mesh_network_key_list_get(0);
    if (network_key){
        printf("{NetKey}\n");
        printf_hex(network_key->net_key, 16);
    }

    mesh_transport_key_t * transport_key = mesh_transport_key_get(0);
    if (transport_key){
        printf("{AppKey}\n");
        printf_hex(transport_key->key, 16);
    }

    mesh_transport_key_t * device_key = mesh_transport_key_get(MESH_DEVICE_KEY_INDEX);
    if (device_key){
        printf("{DevKey}\n");
        printf_hex(device_key->key, 16);
    }
    printf("\n");
}

static void mesh_unprovisioned_beacon_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != MESH_BEACON_PACKET) return;
    uint8_t  device_uuid[16];
    uint16_t oob;
    memcpy(device_uuid, &packet[1], 16);
    oob = big_endian_read_16(packet, 17);
    printf("received unprovisioned device beacon, oob data %x, device uuid: ", oob);
    printf_hexdump(device_uuid, 16);
    pb_adv_create_link(device_uuid);
}

static int scan_hex_byte(const char * byte_string){
    int upper_nibble = nibble_for_char(*byte_string++);
    if (upper_nibble < 0) return -1;
    int lower_nibble = nibble_for_char(*byte_string);
    if (lower_nibble < 0) return -1;
    return (upper_nibble << 4) | lower_nibble;
}

static int btstack_parse_hex(const char * string, uint16_t len, uint8_t * buffer){
    int i;
    for (i = 0; i < len; i++) {
        int single_byte = scan_hex_byte(string);
        if (single_byte < 0) return 0;
        string += 2;
        buffer[i] = (uint8_t)single_byte;
        // don't check seperator after last byte
        if (i == len - 1) {
            return 1;
        }
        // optional seperator
        char separator = *string;
        if (separator == ':' && separator == '-' && separator == ' ') {
            string++;
        }
    }
    return 1;
}

static void btstack_print_hex(const uint8_t * data, uint16_t len, char separator){
    int i;
    for (i=0;i<len;i++){
        printf("%02x", data[i]);
        if (separator){
            printf("%c", separator);
        }
    }
    printf("\n");
}

static void load_pts_app_key(void){
    // PTS app key
    btstack_parse_hex("3216D1509884B533248541792B877F98", 16, pts_application_key.key);
    pts_application_key.aid = 0x38;
    pts_application_key.internal_index = mesh_transport_key_get_free_index();
    mesh_transport_key_add(&pts_application_key);
    printf("PTS Application Key (AID %02x): ", 0x38);
    printf_hexdump(pts_application_key.key, 16);
}
static void send_pts_network_messsage(const char * dst_type, uint16_t dst_addr, int ttl_type){
    uint8_t ttl;
    switch (ttl_type){
        case 0:
            ttl = 0;
            break;
        case 1:
            ttl = PTS_DEFAULT_TTL;
            break;
        default:
            ttl = 0x7f;
            break;
    }
    printf("%s dst %04x, ttl %u\n", dst_type, dst_addr, ttl);
    int lower_transport_pdu_len = 16;
    uint8_t lower_transport_pdu_data[16];
    memset(lower_transport_pdu_data, 0x55, lower_transport_pdu_len);
    mesh_network_send(ttl, dst_addr, lower_transport_pdu_data, lower_transport_pdu_len);
}

static void send_pts_unsegmented_access_messsage(void){
    uint8_t access_pdu_data[16];

    load_pts_app_key();

    uint16_t src = mesh_node_get_primary_element_address();
    uint16_t dest = 0x0001;
    uint8_t  ttl = PTS_DEFAULT_TTL;

    int access_pdu_len = 1;
    memset(access_pdu_data, 0x55, access_pdu_len);
    uint16_t netkey_index = 0;
    uint16_t appkey_index = 0; // MESH_DEVICE_KEY_INDEX;

    // send as unsegmented access pdu
    mesh_pdu_t * pdu = (mesh_pdu_t*) mesh_network_pdu_get();
    int status = mesh_upper_transport_setup_access_pdu(pdu, netkey_index, appkey_index, ttl, src, dest, 0, access_pdu_data, access_pdu_len);
    if (status) return;
    mesh_access_send_unacknowledged_pdu(pdu);
}

static void send_pts_segmented_access_messsage_unicast(void){
    uint8_t access_pdu_data[20];

    load_pts_app_key();

    uint16_t src = mesh_node_get_primary_element_address();
    uint16_t dest = 0x0001;
    uint8_t  ttl = PTS_DEFAULT_TTL;

    int access_pdu_len = 20;
    memset(access_pdu_data, 0x55, access_pdu_len);
    uint16_t netkey_index = 0;
    uint16_t appkey_index = 0; // MESH_DEVICE_KEY_INDEX;

    // send as segmented access pdu
    mesh_pdu_t * pdu = (mesh_pdu_t *) mesh_transport_pdu_get();
    int status = mesh_upper_transport_setup_access_pdu(pdu, netkey_index, appkey_index, ttl, src, dest, 0, access_pdu_data, access_pdu_len);
    if (status) return;
    mesh_access_send_unacknowledged_pdu(pdu);
}

static void send_pts_segmented_access_messsage_group(void){
    uint8_t access_pdu_data[20];

    load_pts_app_key();

    uint16_t src = mesh_node_get_primary_element_address();
    uint16_t dest = 0xd000;
    uint8_t  ttl = PTS_DEFAULT_TTL;

    int access_pdu_len = 20;
    memset(access_pdu_data, 0x55, access_pdu_len);
    uint16_t netkey_index = 0;
    uint16_t appkey_index = 0;

    // send as segmented access pdu
    mesh_pdu_t * pdu = (mesh_pdu_t *) mesh_transport_pdu_get();
    int status = mesh_upper_transport_setup_access_pdu(pdu, netkey_index, appkey_index, ttl, src, dest, 0, access_pdu_data, access_pdu_len);
    if (status) return;
    mesh_access_send_unacknowledged_pdu(pdu);
}

static void send_pts_segmented_access_messsage_virtual(void){
    uint8_t access_pdu_data[20];

    load_pts_app_key();

    uint16_t src = mesh_node_get_primary_element_address();
    uint16_t dest = pts_virtual_addresss->pseudo_dst;
    uint8_t  ttl = PTS_DEFAULT_TTL;

    int access_pdu_len = 20;
    memset(access_pdu_data, 0x55, access_pdu_len);
    uint16_t netkey_index = 0;
    uint16_t appkey_index = 0;

    // send as segmented access pdu
    mesh_transport_pdu_t * transport_pdu = mesh_transport_pdu_get();
    int status = mesh_upper_transport_setup_access_pdu((mesh_pdu_t*) transport_pdu, netkey_index, appkey_index, ttl, src, dest, 0, access_pdu_data, access_pdu_len);
    if (status) return;
    mesh_access_send_unacknowledged_pdu((mesh_pdu_t*) transport_pdu);
}

static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth Mesh Console at %s ---\n", bd_addr_to_str(iut_address));
    printf("0      - Send Network Message Unicast\n");
    printf("1      - Send Network Message Virtual 9779\n");
    printf("2      - Send Network Message Group   D000\n");
    printf("3      - Send Network Message All Proxies\n");
    printf("4      - Send Network Message All Friends\n");
    printf("5      - Send Network Message All Relays\n");
    printf("6      - Send Network Message Nodes\n");
    printf("7      - Dump Network Messages\n");
    printf("?      - Send Unsegmented Access Message\n");
    printf("?      - Send Segmented Access Message - Unicast\n");
    printf("?      - Send Segmented Access Message - Group   D000\n");
    printf("?      - Send Segmented Access Message - Virtual 9779\n");
    printf("?      - Clear Replay Protection List\n");
    printf("?      - Load PTS App key\n");
    printf("8      - Delete provisioning data\n");
    printf("p      - Enable Public Key OOB \n");
    printf("o      - Enable Output OOB \n");
    printf("i      - Input  Output OOB \n");
    printf("s      - Static Output OOB \n");
    printf("b      - Set Secure Network Beacon %s\n", mesh_foundation_beacon_get() ? "Off" : "On");
#ifdef ENABLE_MESH_PROXY_SERVER
    printf("n      - Start Advertising with Node Identity\n");
    printf("N      - Stop Advertising with Node Identity\n");
#endif
    printf("g      - Generic ON/OFF Server Toggle Value\n");
    printf("f      - Register Battery Low warning\n");
    printf("\n");
}

static void stdin_process(char cmd){
    if (ui_chars_for_pin){
        printf("%c", cmd);
        fflush(stdout);
        if (cmd == '\n'){
            printf("\nSending Pin '%s'\n", ui_pin);
            provisioning_device_input_oob_complete_alphanumeric(1, ui_pin, ui_pin_offset);
            ui_chars_for_pin = 0;
        } else {
            ui_pin[ui_pin_offset++] = cmd;
        }
        return;
    }
    switch (cmd){
        case '0':
            send_pts_network_messsage("Unicast", 0x0001, pts_type++);
            break;
        case '1':
            send_pts_network_messsage("Virtual", pts_virtual_addresss->hash, pts_type++);
            break;
        case '2':
            send_pts_network_messsage("Group", 0xd000, pts_type++);
            break;
        case '3':
            send_pts_network_messsage("All Proxies", MESH_ADDRESS_ALL_PROXIES, pts_type++);
            break;
        case '4':
            send_pts_network_messsage("All Friends", MESH_ADDRESS_ALL_FRIENDS, pts_type++);
            break;
        case '5':
            send_pts_network_messsage("All Relays", MESH_ADDRESS_ALL_RELAYS, pts_type++);
            break;
        case '6':
            send_pts_network_messsage("All Nodes", MESH_ADDRESS_ALL_NODES, pts_type++);
            break;
        case '7':
            printf("Dump Network packets\n");
            mesh_network_set_higher_layer_handler(&mesh_pts_received_network_message);
            break;
        case '8':
            mesh_node_reset();
            printf("Mesh Node Reset!\n");
#ifdef ENABLE_MESH_PROXY_SERVER
            mesh_proxy_start_advertising_unprovisioned_device();
#endif
            break;
        case 'p':
            printf("+ Public Key OOB Enabled\n");
            btstack_parse_hex(prov_public_key_string, 64, prov_public_key_data);
            btstack_parse_hex(prov_private_key_string, 32, prov_private_key_data);
            provisioning_device_set_public_key_oob(prov_public_key_data, prov_private_key_data);
            break;
        case 'o':
            printf("+ Output OOB Enabled\n");
            provisioning_device_set_output_oob_actions(0x08, 0x08);
            break;
        case 'i':
            printf("+ Input OOB Enabled\n");
            provisioning_device_set_input_oob_actions(0x08, 0x08);
            break;
        case 's':
            printf("+ Static OOB Enabled\n");
            btstack_parse_hex(prov_static_oob_string, 16, prov_static_oob_data);
            provisioning_device_set_static_oob(16, prov_static_oob_data);
            break;
        case 'g':
            printf("Generic ON/OFF Server Toggle Value\n");
            mesh_generic_on_off_server_set(&mesh_generic_on_off_server_model, 1-mesh_generic_on_off_server_get(&mesh_generic_on_off_server_model), 0, 0);
            break;
        case 'b':
            printf("Turn Secure Network Beacon %s\n", mesh_foundation_beacon_get() ? "Off" : "On");
            mesh_foundation_beacon_set(1 - mesh_foundation_beacon_get());
            mesh_foundation_state_store();
            break;
#ifdef ENABLE_MESH_PROXY_SERVER
        case 'n':
            printf("Start Advertising with Node ID\n");
            mesh_proxy_start_advertising_with_node_id();
            break;
        case 'N':
            printf("Stop Advertising with Node ID\n");
            mesh_proxy_start_advertising_with_node_id();
            break;
#endif
        case 'f':
            // 0x01 = Battery Low
            mesh_health_server_set_fault(mesh_node_get_health_server(), BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH, 1);
        case ' ':
            show_usage();
            break;
        default:
            printf("Command: '%c' not implemented\n", cmd);
            show_usage();
            break;
    }
}

static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(connection_handle);
    if (att_handle == ATT_CHARACTERISTIC_GAP_DEVICE_NAME_01_VALUE_HANDLE){
        return att_read_callback_handle_blob((const uint8_t *)gap_name_buffer, strlen(gap_name_buffer), offset, buffer, buffer_size);
    }
    return 0;
}

int btstack_main(void);
int btstack_main(void)
{
    // console
    btstack_stdin_setup(stdin_process);

    // crypto
    btstack_crypto_init();

    // l2cap
    l2cap_init();

    // setup le device db
    le_device_db_init();

    // setup ATT server
    att_server_init(profile_data, &att_read_callback, NULL);    

    // 
    sm_init();


    // mesh
    mesh_init();

    // setup connectable advertisments
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    uint8_t adv_type = 0;   // AFV_IND
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    adv_bearer_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);

    // Provisioning in device role
    mesh_register_provisioning_device_packet_handler(&mesh_provisioning_message_handler);

    // Loc - bottom - https://www.bluetooth.com/specifications/assigned-numbers/gatt-namespace-descriptors
    mesh_node_set_element_location(mesh_node_get_primary_element(), 0x103);

    // Setup node info
    mesh_node_set_info(BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH, 0, 0);

    // setup health server
    mesh_health_server_add_fault_state(mesh_node_get_health_server(), BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH, &health_fault);

    // Setup Generic On/Off model
    mesh_generic_on_off_server_model.model_identifier = mesh_model_get_model_identifier_bluetooth_sig(MESH_SIG_MODEL_ID_GENERIC_ON_OFF_SERVER);
    mesh_generic_on_off_server_model.operations = mesh_generic_on_off_server_get_operations();    
    mesh_generic_on_off_server_model.model_data = (void *) &mesh_generic_on_off_state;
    mesh_generic_on_off_server_register_packet_handler(&mesh_generic_on_off_server_model, &mesh_state_update_message_handler);
    mesh_generic_on_off_server_set_publication_model(&mesh_generic_on_off_server_model, &generic_on_off_server_publication);
    mesh_element_add_model(mesh_node_get_primary_element(), &mesh_generic_on_off_server_model);
    
    // Setup Generic On/Off model
    mesh_generic_level_server_model.model_identifier = mesh_model_get_model_identifier_bluetooth_sig(MESH_SIG_MODEL_ID_GENERIC_LEVEL_SERVER);
    mesh_generic_level_server_model.operations = mesh_generic_level_server_get_operations();
    mesh_generic_level_server_model.model_data = (void *) &mesh_generic_level_state;
    mesh_generic_level_server_register_packet_handler(&mesh_generic_level_server_model, &mesh_state_update_message_handler);
    mesh_generic_level_server_set_publication_model(&mesh_generic_level_server_model, &generic_level_server_publication);
    mesh_element_add_model(mesh_node_get_primary_element(), &mesh_generic_level_server_model);

    // Setup our custom model
    mesh_vendor_model.model_identifier = mesh_model_get_model_identifier(BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH, MESH_BLUEKITCHEN_MODEL_ID_TEST_SERVER);
    mesh_element_add_model(mesh_node_get_primary_element(), &mesh_vendor_model);
    
    // Setup Configuration Client model
    mesh_configuration_client_model.model_identifier = mesh_model_get_model_identifier_bluetooth_sig(MESH_SIG_MODEL_ID_GENERIC_LEVEL_SERVER);
    mesh_configuration_client_register_packet_handler(&mesh_configuration_client_model, &mesh_configuration_message_handler);
    mesh_element_add_model(mesh_node_get_primary_element(), &mesh_configuration_client_model);

    // Enable PROXY
    mesh_foundation_gatt_proxy_set(1);
    
    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

#if defined(ENABLE_MESH_ADV_BEARER)
    // setup scanning when supporting ADV Bearer
    gap_set_scan_parameters(0, 0x300, 0x300);
    gap_start_scan();
#endif

    //  PTS add-on

#ifdef ENABLE_MESH_ADV_BEARER
    // Register for Unprovisioned Device Beacons provisioner
    beacon_register_for_unprovisioned_device_beacons(&mesh_unprovisioned_beacon_handler);
#endif

    // Set PTS Device UUID
    uint8_t pts_device_uuid[16];
    btstack_parse_hex(pts_device_uuid_string, 16, pts_device_uuid);
    mesh_node_set_device_uuid(pts_device_uuid);
    btstack_print_hex(pts_device_uuid, 16, 0);

    // PTS Virtual Address Label UUID - without Config Model, PTS uses our device uuid
    uint8_t label_uuid[16];
    btstack_parse_hex("001BDC0810210B0E0A0C000B0E0A0C00", 16, label_uuid);
    pts_virtual_addresss = mesh_virtual_address_register(label_uuid, 0x9779);

    printf("TSPX_iut_model_id    0x1000 (Generic OnOff Server)\n"); 
    printf("TSPX_vendor_model_id 0x%08x\n", mesh_vendor_model.model_identifier);
    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* EXAMPLE_END */
