#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include <btstack_util.h>
#include <btstack.h>
#include <btstack_run_loop_posix.h>
#include "hci.h"

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

static void l2cap_packet_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            break;
        case HCI_SCO_DATA_PACKET:
            break;
        case HCI_ACL_DATA_PACKET:
            break;
        default:
            break;
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static int initialized = 0;
    if (initialized == 0){
        initialized = 1;
        btstack_memory_init();
        btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    }

    // prepare test data
    if (size < 3) return 0;
    uint8_t  packet_type  = (data[0] & 3) + 1;             // only 1-4
    uint16_t connection_handle = ((data[0] >> 2) & 0x07); // 0x0000 - 0x0007
    uint8_t  pb_or_ps = (data[0] >> 5) & 0x003;            // 0x00-0x03
    size--;
    data++;
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
        case HCI_SCO_DATA_PACKET:
            little_endian_store_16(packet, 0, (pb_or_ps << 12) | connection_handle);
            if (size > 255) return 0;
            packet[2] = size;
            memcpy(&packet[3], data, size);
            packet_len = size + 3;
            break;
        case HCI_ACL_DATA_PACKET:
            little_endian_store_16(packet, 0, (pb_or_ps << 12) | connection_handle);
            little_endian_store_16(packet, 2, size);
            if (size > (sizeof(packet) - 4)) return 0;
            memcpy(&packet[4], data, size);
            packet_len = size + 4;
            break;
        default:
            return 0;
    }

    // init hci
    hci_init(&hci_transport_fuzz, NULL);
    hci_setup_test_connections_fuzz();

    // deliver test data
    (*packet_handler)(packet_type, packet, packet_len);

    // teardown
    hci_free_connections_fuzz();
    return 0;
}
