#include <stdint.h>
#include <stddef.h>

#include "classic/avrcp.h"
#include "classic/avrcp_controller.h"
#include "classic/avrcp_target.h"
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

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    const hci_con_handle_t ble_handle = 0x0005;
    const uint16_t l2cap_cid = 0x41;

    static bool avrcp_initialized = false;
    if (!avrcp_initialized){
        avrcp_initialized = true;
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
        avdtp_init_fuzz();
    }
    avdtp_packet_handler_fuzz((uint8_t*)data, size);
    return 0;
}
