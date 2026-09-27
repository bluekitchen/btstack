#include <assert.h>
#include <stdio.h>
#include <string.h>

// Keep the received PDU owned by the test while exercising the real handlers.
#define mesh_access_message_processed test_message_processed
#include "mesh/mesh_configuration_client.c"
#undef mesh_access_message_processed

static unsigned processed;
static unsigned event_count;
static uint8_t events[256][14];
static uint16_t event_sizes[256];

void test_message_processed(mesh_pdu_t * pdu){
    (void)pdu;
    processed++;
}

static void receive_event(uint8_t type, uint16_t channel, uint8_t * event, uint16_t size){
    (void)channel;
    assert(type == HCI_EVENT_PACKET);
    assert(size <= sizeof(events[0]));
    assert(size == event[1] + 2u);
    assert(event_count < 256);
    memcpy(events[event_count], event, size);
    event_sizes[event_count++] = size;
}

typedef void (*handler_t)(mesh_model_t *, mesh_pdu_t *);
static void run(handler_t handler, const uint8_t * parameters, uint16_t len){
    mesh_access_pdu_t pdu = {0};
    mesh_model_t model = {0};
    pdu.pdu_header.pdu_type = MESH_PDU_TYPE_ACCESS;
    pdu.src = 0x1234;
    pdu.data[0] = 0x01; // Valid one-byte opcode; dispatch is tested directly.
    memcpy(pdu.data + 1, parameters, len);
    pdu.len = len + 1;
    model.model_packet_handler = receive_event;
    processed = 0;
    event_count = 0;
    handler(&model, (mesh_pdu_t *)&pdu);
    assert(processed == 1);
}

static void test_key_lists(void){
    const uint8_t packed[] = { 0x23, 0xc1, 0xab, 0xff, 0x0f }; // 0x123, 0xabc, 0xfff
    const uint16_t expected[] = { 0x123, 0xabc, 0xfff };
    const handler_t handlers[] = {
        mesh_configuration_client_netkey_list_handler,
        mesh_configuration_client_appkey_list_handler,
        mesh_configuration_client_sig_model_app_list_handler,
        mesh_configuration_client_vendor_model_app_list_handler,
    };
    const unsigned header_sizes[] = { 0, 3, 5, 7 };
    for (unsigned type = 0; type < 4; type++){
        uint8_t parameters[16] = { 7, 0x34, 0x12, 0x78, 0x56, 0xbc, 0x9a };
        unsigned header = header_sizes[type];
        memcpy(parameters + header, packed, sizeof(packed));
        for (unsigned len = 0; len <= sizeof(packed); len++){
            run(handlers[type], parameters, header + len);
            unsigned count = len % 3 == 1 ? 0 : len / 3 * 2 + len % 3 / 2;
            assert(event_count == count);
            for (unsigned i = 0; i < count; i++){
                unsigned count_pos = type < 2 ? 6 : 10;
                unsigned key_pos = type == 0 ? 8 : type == 1 ? 10 : 12;
                assert(events[i][5] == (type == 0 ? 0 : 7));
                assert(events[i][count_pos] == count);
                assert(events[i][count_pos + 1] == i);
                assert(little_endian_read_16(events[i], key_pos) == expected[i]);
                if (type == 1) assert(little_endian_read_16(events[i], 8) == 0x234);
            }
        }
        for (unsigned len = 0; len < header; len++){
            run(handlers[type], parameters, len);
            assert(event_count == 0);
        }
    }
}

static void test_subscription_lists(void){
    const handler_t handlers[] = {
        mesh_configuration_client_sig_model_subscription_handler,
        mesh_configuration_client_vendor_model_subscription_handler,
    };
    const uint8_t addresses[] = { 0x01, 0xc0, 0x02, 0xc0, 0x03, 0xc0 };
    for (unsigned type = 0; type < 2; type++){
        unsigned header = type == 0 ? 5 : 7;
        uint8_t parameters[16] = { 7, 0x34, 0x12, 0x78, 0x56, 0xbc, 0x9a };
        memcpy(parameters + header, addresses, sizeof(addresses));
        for (unsigned len = 0; len <= header + sizeof(addresses); len++){
            run(handlers[type], parameters, len);
            unsigned count = len < header || (len - header) % 2 ? 0 : (len - header) / 2;
            assert(event_count == count);
            for (unsigned i = 0; i < count; i++){
                assert(event_sizes[i] == 14);
                assert(events[i][10] == count);
                assert(events[i][11] == i);
                assert(little_endian_read_16(events[i], 12) == 0xc001 + i);
            }
        }
    }
}

int main(void){
    test_key_lists();
    test_subscription_lists();
    puts("PASS: Mesh configuration lists, packed indexes, truncated headers and invalid tails");
    return 0;
}
