#include <stdint.h>
#include <stddef.h>

#include "ble/gatt_client.h"
#include "btstack_run_loop_posix.h"
#include "btstack_memory.h"

static  void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

static int hci_transport_fuzz_set_baudrate(uint32_t baudrate){
    return 0;
}

static int hci_transport_fuzz_can_send_now(uint8_t packet_type){
    return 1;
}

static int hci_transport_fuzz_send_packet(uint8_t packet_type, uint8_t * packet, int size){
    return 0;
}

static void hci_transport_fuzz_init(const void * transport_config){
}

static int hci_transport_fuzz_open(void){
    return 0;
}

static int hci_transport_fuzz_close(void){
    return 0;
}

static void hci_transport_fuzz_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    packet_handler = handler;
}

static const hci_transport_t hci_transport_fuzz = {
        /* const char * name; */                                        "FUZZ",
        /* void   (*init) (const void *transport_config); */            &hci_transport_fuzz_init,
        /* int    (*open)(void); */                                     &hci_transport_fuzz_open,
        /* int    (*close)(void); */                                    &hci_transport_fuzz_close,
        /* void   (*register_packet_handler)(void (*handler)(...); */   &hci_transport_fuzz_register_packet_handler,
        /* int    (*can_send_packet_now)(uint8_t packet_type); */       &hci_transport_fuzz_can_send_now,
        /* int    (*send_packet)(...); */                               &hci_transport_fuzz_send_packet,
        /* int    (*set_baudrate)(uint32_t baudrate); */                &hci_transport_fuzz_set_baudrate,
        /* void   (*reset_link)(void); */                               NULL,
        /* void   (*set_sco_config)(uint16_t voice_setting, int num_connections); */ NULL,
};

static void gatt_client_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){
}

void set_gatt_service_uuid16(gatt_client_service_t * service, const uint8_t *data, size_t size){
    service->start_group_handle = 0x0001;
    service->end_group_handle = 0xffff;
    memset(service->uuid128, 0, 16);
    service->uuid16 = big_endian_read_16(data, 0);
}

void set_gatt_service_uuid128(gatt_client_service_t * service, const uint8_t *data, size_t size){
    service->start_group_handle = 0x0001;
    service->end_group_handle = 0xffff;
    service->uuid16 = 0;
    memcpy(service->uuid128, data, 16);
}

void set_gatt_characteristic_uuid16(gatt_client_characteristic_t * characteristic, const uint8_t *data, size_t size){
    characteristic->start_handle = 0x0001;
    characteristic->value_handle = 0x0002;
    characteristic->end_handle = 0xffff;
    characteristic->uuid16 = big_endian_read_16(data, 0);
    memset(characteristic->uuid128, 0, 16);
}

void set_gatt_characteristic_uuid128(gatt_client_characteristic_t * characteristic, const uint8_t *data, size_t size){
    characteristic->start_handle = 0x0001;
    characteristic->value_handle = 0x0002;
    characteristic->end_handle = 0xffff;
    characteristic->uuid16 = 0;
    memcpy(characteristic->uuid128, data, 16);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    const hci_con_handle_t ble_handle = 0x0005;

    static bool gatt_client_initiated = false;
    if (!gatt_client_initiated){
        btstack_memory_init();
        btstack_run_loop_init(btstack_run_loop_posix_get_instance());
        // init hci, simulate connection
        hci_init(&hci_transport_fuzz, NULL);
        hci_setup_test_connections_fuzz();

        gatt_client_init();
        gatt_client_mtu_enable_auto_negotiation(0);
        gatt_client_initiated = true;
    }

    // use first byte of random data to pick gatt_client request / set gatt client state
    // then, only use data from second byte as response
    // prepare test data
    if (size < 1) return 0;
    uint8_t  cmd_type  = (data[0] & 0x1F); 
    size--;
    data++;

    uint8_t  uuid128[16];
    uint16_t uuid16;
    int offset = 0;
    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic;
    gatt_client_characteristic_descriptor_t descriptor;

    uint8_t response_type = 0;
    switch (cmd_type){
        case 1:
            gatt_client_discover_primary_services(gatt_client_packet_handler, ble_handle);
            response_type = ATT_READ_BY_GROUP_TYPE_RESPONSE;
            break;
        case 2:
            offset = 2;
            if (size < offset) return 0;
            uuid16 = big_endian_read_16(data, 0);
            gatt_client_discover_primary_services_by_uuid16(gatt_client_packet_handler, ble_handle, uuid16);
            response_type = ATT_FIND_BY_TYPE_VALUE_RESPONSE;
            break;
        case 3:
            offset = 16;
            if (size < offset) return 0;
            memcpy(uuid128, data, 16);
            gatt_client_discover_primary_services_by_uuid128(gatt_client_packet_handler, ble_handle, uuid128);
            response_type = ATT_FIND_BY_TYPE_VALUE_RESPONSE;
            break;
        case 4:
            offset = 2;
            if (size < offset) return 0;
            set_gatt_service_uuid16(&service, data, size);
            gatt_client_find_included_services_for_service(gatt_client_packet_handler, ble_handle, &service);
            response_type = ATT_READ_BY_TYPE_RESPONSE;
            break;
        case 5:
            offset = 2;
            if (size < offset) return 0;
            set_gatt_service_uuid16(&service, data, size);
            gatt_client_discover_characteristics_for_service(gatt_client_packet_handler, ble_handle, &service);
            response_type = ATT_READ_BY_TYPE_RESPONSE;
            break;
        case 6:
            offset = 2;
            if (size < offset) return 0;
            uuid16 = big_endian_read_16(data, 0);
            gatt_client_discover_characteristics_for_handle_range_by_uuid16(gatt_client_packet_handler, ble_handle, 0x0001, 0xffff, uuid16);
            response_type = ATT_READ_BY_TYPE_RESPONSE;
            break;
        case 7:
            offset = 16;
            if (size < offset) return 0;
            memcpy(uuid128, data, 16);
            gatt_client_discover_characteristics_for_handle_range_by_uuid128(gatt_client_packet_handler, ble_handle, 0x0001, 0xffff, uuid128);
            response_type = ATT_READ_BY_TYPE_RESPONSE;
            break;
        case 8:
            offset = 4;
            if (size < offset) return 0;
            set_gatt_service_uuid16(&service, data, size);
            uuid16 = big_endian_read_16(data, 2);
            gatt_client_discover_characteristics_for_service_by_uuid16(gatt_client_packet_handler, ble_handle, &service, uuid16);
            response_type = ATT_READ_BY_TYPE_RESPONSE;
            break;
        case 9:
            offset = 18;
            if (size < offset) return 0;
            set_gatt_service_uuid16(&service, data, size);
            memcpy(uuid128, data + 2, 16);
            gatt_client_discover_characteristics_for_service_by_uuid128(gatt_client_packet_handler, ble_handle, &service, uuid128);
            response_type = ATT_READ_BY_TYPE_RESPONSE;
            break;
        case 10:
            offset = 2;
            if (size < offset) return 0;
            set_gatt_characteristic_uuid16(&characteristic, data, size);
            gatt_client_discover_characteristic_descriptors(gatt_client_packet_handler, ble_handle, &characteristic);
            response_type = ATT_FIND_INFORMATION_REPLY;
            break;
        case 11:
            offset = 2;
            if (size < offset) return 0;
            set_gatt_characteristic_uuid16(&characteristic, data, size);
            gatt_client_read_value_of_characteristic(gatt_client_packet_handler, ble_handle, &characteristic);
            response_type = ATT_READ_RESPONSE;
            break;
        case 12:
            offset = 2;
            if (size < offset) return 0;
            set_gatt_characteristic_uuid16(&characteristic, data, size);
            gatt_client_read_value_of_characteristics_by_uuid16(gatt_client_packet_handler, ble_handle, characteristic.start_handle, characteristic.end_handle, characteristic.uuid16);
            response_type = ATT_READ_BY_TYPE_RESPONSE;
            break;
        case 13:
            offset = 16;
            if (size < offset) return 0;
            set_gatt_characteristic_uuid128(&characteristic, data, size);
            gatt_client_read_value_of_characteristics_by_uuid128(gatt_client_packet_handler, ble_handle, characteristic.start_handle, characteristic.end_handle, characteristic.uuid128);
            response_type = ATT_READ_BY_TYPE_RESPONSE;
            break;
        case 14:
            offset = 2;
            if (size < offset) return 0;
            set_gatt_characteristic_uuid16(&characteristic, data, size);
            gatt_client_read_long_value_of_characteristic(gatt_client_packet_handler, ble_handle, &characteristic);
            response_type = ATT_READ_BLOB_RESPONSE;
            break;
        case 15:
            offset = 4;
            if (size < offset) return 0;
            set_gatt_characteristic_uuid16(&characteristic, data, size);
            gatt_client_read_long_value_of_characteristic_using_value_handle_with_offset(gatt_client_packet_handler, ble_handle, characteristic.value_handle, big_endian_read_16(data, 2));
            response_type = ATT_READ_BLOB_RESPONSE;
            break;
        case 16: 
            gatt_client_read_multiple_characteristic_values(gatt_client_packet_handler, ble_handle, 0, NULL);
            response_type = ATT_READ_MULTIPLE_RESPONSE;
            break;
        case 17:
            gatt_client_write_value_of_characteristic(gatt_client_packet_handler, ble_handle, 5, 0, NULL);
            response_type = ATT_WRITE_RESPONSE;
            break;
        case 18:
            gatt_client_write_long_value_of_characteristic(gatt_client_packet_handler, ble_handle, 5, 0, NULL);
            response_type = ATT_PREPARE_WRITE_RESPONSE;
            break;
        case 19:
            gatt_client_reliable_write_long_value_of_characteristic(gatt_client_packet_handler, ble_handle, 5, 0, NULL);
            response_type = ATT_PREPARE_WRITE_RESPONSE;
            break;
        case 20:
            gatt_client_read_characteristic_descriptor_using_descriptor_handle(gatt_client_packet_handler, ble_handle, 5);
            response_type = ATT_READ_RESPONSE;
            break;
        case 21:
            gatt_client_read_long_characteristic_descriptor_using_descriptor_handle(gatt_client_packet_handler, ble_handle, 5);
            response_type = ATT_READ_BLOB_RESPONSE;
            break;
        case 22:
            gatt_client_write_characteristic_descriptor_using_descriptor_handle(gatt_client_packet_handler, ble_handle, 5, 0, NULL);
            response_type = ATT_PREPARE_WRITE_RESPONSE;
            break;
        case 23:
            gatt_client_write_long_characteristic_descriptor_using_descriptor_handle(gatt_client_packet_handler, ble_handle, 5, 0, NULL);
            response_type = ATT_PREPARE_WRITE_RESPONSE;
            break;
        case 24:
            offset = 2;
            if (size < offset) return 0;
            set_gatt_characteristic_uuid16(&characteristic, data, size);
            gatt_client_write_client_characteristic_configuration(gatt_client_packet_handler, ble_handle, &characteristic, 1);
#ifdef ENABLE_GATT_FIND_INFORMATION_FOR_CCC_DISCOVERY
            response_type = ATT_FIND_INFORMATION_REPLY;
#else
            response_type = ATT_READ_BY_TYPE_RESPONSE;
#endif
            break;
        case 25:
            gatt_client_prepare_write(gatt_client_packet_handler, ble_handle, 5, 0, 0, NULL);
            response_type = ATT_PREPARE_WRITE_RESPONSE;
            break;

#if 0
        // TODO: won't work as only single packet is simulate
        case 26:
            gatt_client_prepare_write(gatt_client_packet_handler, ble_handle, 5, 0, 0, NULL);
            response_type = ATT_PREPARE_WRITE_RESPONSE;
            gatt_client_execute_write(gatt_client_packet_handler, ble_handle);
            break;
        case 27:
            gatt_client_prepare_write(gatt_client_packet_handler, ble_handle, 5, 0, 0, NULL);
            response_type = ATT_PREPARE_WRITE_RESPONSE;
            gatt_client_cancel_write(gatt_client_packet_handler, ble_handle);
            break;
#endif
        default:
            return 0;
    }

    data += offset;
    size -= offset;

    uint8_t response_buffer[256];
    response_buffer[0] = response_type;
    uint32_t bytes_to_copy = btstack_min(size, sizeof(response_buffer)-1);
    memcpy(&response_buffer[1], data, bytes_to_copy);
    // send test response
    gatt_client_att_packet_handler_fuzz(ATT_DATA_PACKET, ble_handle, response_buffer, bytes_to_copy+1);
    return 0;
}
