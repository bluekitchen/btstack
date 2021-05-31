#include <stdint.h>
#include <stddef.h>

#include "classic/avrcp.h"
#include "classic/avrcp_controller.h"
#include "classic/avrcp_target.h"
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

static void avrcp_client_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    const hci_con_handle_t ble_handle = 0x0005;

    static bool avrcp_initialized = false;
    if (!avrcp_initialized){
        avrcp_initialized = true;
        btstack_memory_init();
        btstack_run_loop_init(btstack_run_loop_posix_get_instance());
        hci_init(&hci_transport_fuzz, NULL);
        avrcp_init();
        avrcp_controller_init();
        avrcp_target_init();
        avrcp_init_fuzz();
        avrcp_controller_register_packet_handler(&avrcp_client_packet_handler);
        avrcp_target_register_packet_handler(&avrcp_client_packet_handler);
    }
    avrcp_packet_handler_fuzz((uint8_t*)data, size);
    return 0;
}
