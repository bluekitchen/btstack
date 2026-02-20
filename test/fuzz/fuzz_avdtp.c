#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "classic/avdtp.h"
#include "btstack_run_loop_posix.h"
#include "btstack_memory.h"
#include "classic/a2dp_sink.h"

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

static void avdtp_client_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){
}

static uint8_t  a2dp_local_seid;
static uint8_t  media_sbc_codec_configuration[4];
static uint8_t media_sbc_codec_capabilities[] = {
        0xFF,//(AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
        0xFF,//(AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
        2, 53
};

static uint8_t consume_u8(const uint8_t ** data, size_t * size, uint8_t fallback){
    if (*size == 0) return fallback;
    uint8_t value = (*data)[0];
    (*data)++;
    (*size)--;
    return value;
}

static uint16_t build_signaling_packet(uint8_t * out, uint16_t out_size, uint8_t transaction_label, avdtp_packet_type_t packet_type,
                                       avdtp_message_type_t message_type, uint8_t signal_identifier, uint8_t num_packets,
                                       const uint8_t * payload, uint16_t payload_size){
    if (out_size == 0) return 0;

    uint16_t pos = 0;
    out[pos++] = (transaction_label << 4) | (((uint8_t) packet_type & 3) << 2) | ((uint8_t) message_type & 3);

    if (packet_type == AVDTP_START_PACKET){
        if (pos >= out_size) return pos;
        out[pos++] = num_packets;
    }

    if (pos >= out_size) return pos;
    out[pos++] = signal_identifier & 0x3f;

    uint16_t to_copy = payload_size;
    if ((uint16_t)(out_size - pos) < to_copy){
        to_copy = out_size - pos;
    }
    if (to_copy > 0){
        memcpy(&out[pos], payload, to_copy);
        pos += to_copy;
    }
    return pos;
}

static void send_synthesized_packet(const uint8_t ** data, size_t * size, avdtp_packet_type_t packet_type, uint8_t transaction_label,
                                    avdtp_message_type_t message_type, uint8_t signal_identifier, uint8_t num_packets){
    uint8_t payload[64];
    uint8_t packet[80];

    uint8_t payload_size = consume_u8(data, size, 0) & 0x3f;
    for (uint16_t i = 0; i < payload_size; i++){
        payload[i] = consume_u8(data, size, (uint8_t)(i + signal_identifier + ((uint8_t) packet_type << 1)));
    }

    uint16_t packet_size = build_signaling_packet(packet, sizeof(packet), transaction_label, packet_type, message_type,
                                                  signal_identifier, num_packets, payload, payload_size);
    if (packet_size > 0){
        avdtp_packet_handler_fuzz(packet, packet_size);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    static bool avdtp_initialized = false;
    if (!avdtp_initialized){
        avdtp_initialized = true;
        btstack_memory_init();
        btstack_run_loop_init(btstack_run_loop_posix_get_instance());
        hci_init(&hci_transport_fuzz, NULL);
        l2cap_init();
        avdtp_init();
        avdtp_sink_init();
        avdtp_source_init();
        avdtp_sink_register_packet_handler(&avdtp_client_packet_handler);
        avdtp_source_register_packet_handler(&avdtp_client_packet_handler);
        avdtp_stream_endpoint_t * local_stream_endpoint = a2dp_sink_create_stream_endpoint(AVDTP_AUDIO,
                                                                                           AVDTP_CODEC_SBC, media_sbc_codec_capabilities, sizeof(media_sbc_codec_capabilities),
                                                                                           media_sbc_codec_configuration, sizeof(media_sbc_codec_configuration));
        UNUSED(local_stream_endpoint);
        avdtp_init_fuzz();
    }

    const uint8_t * cursor = data;
    size_t remaining = size;

    // Send random standalone signaling packets with arbitrary packet/message types.
    uint8_t standalone_packets = (consume_u8(&cursor, &remaining, 0) & 7) + 1;
    for (uint8_t i = 0; i < standalone_packets; i++){
        avdtp_packet_type_t packet_type = (avdtp_packet_type_t)(consume_u8(&cursor, &remaining, i) & 3);
        uint8_t transaction_label = consume_u8(&cursor, &remaining, (uint8_t)(i * 3)) & 0x0f;
        avdtp_message_type_t message_type = (avdtp_message_type_t)(consume_u8(&cursor, &remaining, i + 1) & 3);
        uint8_t signal_identifier = consume_u8(&cursor, &remaining, (uint8_t)(1 + i * 7)) & 0x3f;
        uint8_t num_packets = consume_u8(&cursor, &remaining, 0);
        send_synthesized_packet(&cursor, &remaining, packet_type, transaction_label, message_type, signal_identifier, num_packets);
    }

    // Stress fragmentation state with valid and intentionally invalid sequences.
    uint8_t sequence_count = (consume_u8(&cursor, &remaining, 0) & 3) + 1;
    for (uint8_t seq = 0; seq < sequence_count; seq++){
        uint8_t base_label = consume_u8(&cursor, &remaining, (uint8_t)seq) & 0x0f;
        avdtp_message_type_t base_message = (avdtp_message_type_t)(consume_u8(&cursor, &remaining, seq + 1) & 3);
        uint8_t base_signal_identifier = consume_u8(&cursor, &remaining, (uint8_t)(0x10 + seq)) & 0x3f;
        uint8_t frag_count = (consume_u8(&cursor, &remaining, 1) & 3) + 1;
        uint8_t start_num_packets = consume_u8(&cursor, &remaining, 0);
        uint8_t pattern = consume_u8(&cursor, &remaining, seq) % 5;

        for (uint8_t frag = 0; frag < frag_count; frag++){
            avdtp_packet_type_t packet_type;
            switch (pattern){
                case 0:
                    packet_type = (frag == 0) ? AVDTP_START_PACKET : ((frag + 1 == frag_count) ? AVDTP_END_PACKET : AVDTP_CONTINUE_PACKET);
                    break;
                case 1:
                    packet_type = (frag + 1 == frag_count) ? AVDTP_END_PACKET : AVDTP_CONTINUE_PACKET;
                    break;
                case 2:
                    packet_type = (frag == 0) ? AVDTP_END_PACKET : ((frag == 1) ? AVDTP_START_PACKET : AVDTP_CONTINUE_PACKET);
                    break;
                case 3:
                    packet_type = (avdtp_packet_type_t)(consume_u8(&cursor, &remaining, frag) & 3);
                    break;
                default:
                    packet_type = (frag == 1) ? AVDTP_SINGLE_PACKET : ((frag & 1) ? AVDTP_CONTINUE_PACKET : AVDTP_START_PACKET);
                    break;
            }

            uint8_t transaction_label = base_label;
            if (consume_u8(&cursor, &remaining, 0) & 1){
                transaction_label ^= consume_u8(&cursor, &remaining, frag) & 0x0f;
            }

            avdtp_message_type_t message_type = base_message;
            if (consume_u8(&cursor, &remaining, 0) & 1){
                message_type = (avdtp_message_type_t)(consume_u8(&cursor, &remaining, frag + 1) & 3);
            }

            uint8_t signal_identifier = base_signal_identifier;
            if (consume_u8(&cursor, &remaining, 0) & 1){
                signal_identifier = consume_u8(&cursor, &remaining, (uint8_t)(0x20 + frag)) & 0x3f;
            }

            uint8_t num_packets = start_num_packets;
            if (consume_u8(&cursor, &remaining, 0) & 1){
                num_packets = consume_u8(&cursor, &remaining, frag_count - frag);
            }

            send_synthesized_packet(&cursor, &remaining, packet_type, transaction_label, message_type, signal_identifier, num_packets);
        }
    }
    return 0;
}
