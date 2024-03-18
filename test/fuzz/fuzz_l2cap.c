#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include <btstack_util.h>
#include <btstack.h>
#include <btstack_run_loop_posix.h>
#include "hci.h"

static hci_connection_t hci_connection;

static btstack_linked_list_t hci_connections;

static btstack_packet_handler_t acl_packet_handler;
static btstack_packet_handler_t event_packet_handler;

static uint8_t outgoing_buffer[2000];
static bool outgoing_reserved;

void l2cap_setup_test_channels_fuzz(void);
void l2cap_free_channels_fuzz(void);

void hci_add_event_handler(btstack_packet_callback_registration_t * callback_handler){
    event_packet_handler = callback_handler->callback;
}

void hci_register_acl_packet_handler(btstack_packet_handler_t handler){
    acl_packet_handler = handler;
}

bool hci_can_send_acl_packet_now(hci_con_handle_t con_handle){
    return true;
}

hci_connection_t * hci_connection_for_bd_addr_and_type(const bd_addr_t addr, bd_addr_type_t addr_type){
    return &hci_connection;
}

hci_connection_t * hci_connection_for_handle(hci_con_handle_t con_handle){
    return &hci_connection;
}

void gap_connectable_control(uint8_t enable){
}

void hci_remote_features_query(hci_con_handle_t con_handle){
}

void hci_disconnect_security_block(hci_con_handle_t con_handle){
}

void gap_request_security_level(hci_con_handle_t con_handle, gap_security_level_t requested_level){
}

void gap_set_minimal_service_security_level(gap_security_level_t security_level){
}

void hci_connections_get_iterator(btstack_linked_list_iterator_t *it){
    btstack_linked_list_iterator_init(it, &hci_connections);
}

bool hci_is_le_connection_type(bd_addr_type_t address_type){
    switch (address_type){
        case BD_ADDR_TYPE_LE_PUBLIC:
        case BD_ADDR_TYPE_LE_RANDOM:
        case BD_ADDR_TYPE_LE_PUBLIC_IDENTITY:
        case BD_ADDR_TYPE_LE_RANDOM_IDENTITY:
            return true;
        default:
            return false;
    }
}

bool hci_non_flushable_packet_boundary_flag_supported(void){
    return true;
}

uint16_t hci_automatic_flush_timeout(void){
    return 0;
}

bool hci_can_send_prepared_acl_packet_now(hci_con_handle_t con_handle) {
    return true;
}

bool hci_can_send_acl_classic_packet_now(void){
    return true;
}

bool hci_can_send_acl_le_packet_now(void){
    return true;
}

bool hci_can_send_command_packet_now(void){
    return true;
}

uint8_t hci_send_cmd(const hci_cmd_t * cmd, ...){
    return ERROR_CODE_SUCCESS;
}

uint16_t hci_usable_acl_packet_types(void){
    return 0;
}

uint8_t hci_get_allow_role_switch(void){
    return true;
}

void hci_reserve_packet_buffer(void){
    outgoing_reserved = true;
}

void hci_release_packet_buffer(void){
    outgoing_reserved = false;
}

bool hci_is_packet_buffer_reserved(void){
    return outgoing_reserved;
}

uint8_t* hci_get_outgoing_packet_buffer(void){
    return outgoing_buffer;
}

uint8_t hci_send_acl_packet_buffer(int size){
    outgoing_reserved = false;
    return ERROR_CODE_SUCCESS;
}

uint16_t hci_max_acl_data_packet_length(void){
    return 100;
}

bool hci_authentication_active_for_handle(hci_con_handle_t handle){
    return false;
}

void gap_drop_link_key_for_bd_addr(bd_addr_t addr){
}

void gap_get_connection_parameter_range(le_connection_parameter_range_t * range){
    memset(range, 0, sizeof(le_connection_parameter_range_t));
}

authorization_state_t gap_authorization_state(hci_con_handle_t con_handle){
    return AUTHORIZATION_GRANTED;
}

// TODO: use fuzzer input for level
int gap_connection_parameter_range_included(le_connection_parameter_range_t * existing_range, uint16_t le_conn_interval_min, uint16_t le_conn_interval_max, uint16_t le_conn_latency, uint16_t le_supervision_timeout){
    return true;
}

// TODO: use fuzzer input for level
bool gap_secure_connection(hci_con_handle_t con_handle){
    return true;
}

// TODO: use fuzzer input for level
bool gap_get_secure_connections_only_mode(void){
    return false;
}

// TODO: use fuzzer input for level
gap_connection_type_t gap_get_connection_type(hci_con_handle_t connection_handle){
    return GAP_CONNECTION_ACL;
}

// TODO: use fuzzer input for level
gap_security_level_t gap_get_security_level(void){
    return LEVEL_4;
}

// TODO: use fuzzer input for level
gap_security_level_t gap_security_level(hci_con_handle_t con_handle){
    return LEVEL_4;
}

// TODO: use fuzzer input for level
gap_security_mode_t gap_get_security_mode(void){
    return GAP_SECURITY_MODE_4;
}

// TODO: use fuzzer input for level
bool hci_remote_features_available(hci_con_handle_t handle){
    return true;
}

// TODO: use fuzzer input for level
bool gap_ssp_supported_on_both_sides(hci_con_handle_t handle){
    return true;
}

// TODO: use fuzzer input for level
uint8_t gap_encryption_key_size(hci_con_handle_t con_handle){
    return 16;
}

// TODO: use fuzzer input for level
bool gap_authenticated(hci_con_handle_t con_handle){
    return true;
}

// SM
void sm_add_event_handler(btstack_packet_callback_registration_t * callback_handler){
}
void sm_request_pairing(hci_con_handle_t con_handle){
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static int initialized = 0;
    if (initialized == 0){
        initialized = 1;
        btstack_run_loop_init(btstack_run_loop_posix_get_instance());
        hci_connection.con_handle = 0x0000;
    }

    btstack_memory_init();

    // prepare test data
    if (size < 5) return 0;
    uint8_t  packet_type  = (data[0] & 1) ? HCI_EVENT_PACKET : HCI_ACL_DATA_PACKET;
    uint16_t connection_handle = ((data[0] >> 2) & 0x07); // 0x0000 - 0x0007
    uint8_t  pb_or_ps = (data[0] >> 5) & 0x003;            // 0x00-0x03
    uint16_t cid;
    switch (data[1] & 3){
        case 0:
            cid = 1;
            break;
        case 1:
            cid = 0x41;
            break;
        case 2:
            cid = 0x42;
            break;
        case 3:
            cid = 0x43;
            break;
    }
    size -= 3;
    data += 3;
    uint8_t packet[1000];
    uint16_t packet_len;
    switch (packet_type){
        case HCI_EVENT_PACKET:
            packet[0] = data[0];
            size--;
            data++;
            if (size > 255) return 0;
            packet[1] = size;
            memcpy(&packet[2], data, size);
            packet_len = size + 2;
            break;
        case HCI_ACL_DATA_PACKET:
            little_endian_store_16(packet, 0, (pb_or_ps << 12) | connection_handle);
            little_endian_store_16(packet, 2, size + 4);
            little_endian_store_16(packet, 4, size);
            little_endian_store_16(packet, 6, cid);
            if (size > (sizeof(packet) - 8)) return 0;
            memcpy(&packet[8], data, size);
            packet_len = size + 8;
            break;
        default:
            return 0;
    }

    // init hci mock
    outgoing_reserved = false;
    hci_connections = (btstack_linked_item_t*) &hci_connection;

    // init l2cap
    l2cap_init();
    l2cap_setup_test_channels_fuzz();

    // deliver test data
    switch (packet_type){
        case HCI_EVENT_PACKET:
            (*event_packet_handler)(packet_type, 0, packet, packet_len);
            break;
        case HCI_ACL_DATA_PACKET:
            (*acl_packet_handler)(packet_type, 0, packet, packet_len);
            break;
        default:
            return 0;
    }

    // teardown
    l2cap_free_channels_fuzz();

    btstack_memory_deinit();

    return 0;
}
