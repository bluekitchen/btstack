#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include <btstack_util.h>
#include "hci_transport.h"
#include "hci_transport_h4.h"

static hci_transport_config_uart_t config = {
        HCI_TRANSPORT_CONFIG_UART,
        115200,
        0,  // main baudrate
        1,  // flow control
        NULL,
};

static uint8_t * read_request_buffer;
static uint32_t  read_request_len;

static void (*block_received)(void);

static int btstack_uart_fuzz_init(const btstack_uart_config_t * config){
    return 0;
}

static int btstack_uart_fuzz_open(void){
    return 0;
}

static int btstack_uart_fuzz_close(void){
    return 0;
}

static void btstack_uart_fuzz_set_block_received( void (*block_handler)(void)){
    block_received = block_handler;
}

static void btstack_uart_fuzz_set_block_sent( void (*block_handler)(void)){
}

static void btstack_uart_fuzz_set_wakeup_handler( void (*the_wakeup_handler)(void)){
}

static int btstack_uart_fuzz_set_parity(int parity){
    return 0;
}

static void btstack_uart_fuzz_send_block(const uint8_t *data, uint16_t size){
}

static void btstack_uart_fuzz_receive_block(uint8_t *buffer, uint16_t len){
    read_request_buffer = buffer;
    read_request_len = len;
}

static int btstack_uart_fuzz_set_baudrate(uint32_t baudrate){
    return 0;
}

static int btstack_uart_fuzz_get_supported_sleep_modes(void){
    return BTSTACK_UART_SLEEP_MASK_RTS_HIGH_WAKE_ON_CTS_PULSE;
}

static void btstack_uart_fuzz_set_sleep(btstack_uart_sleep_mode_t sleep_mode){
}

btstack_uart_block_t uart_driver = {
        /* int  (*init)(hci_transport_config_uart_t * config); */         &btstack_uart_fuzz_init,
        /* int  (*open)(void); */                                         &btstack_uart_fuzz_open,
        /* int  (*close)(void); */                                        &btstack_uart_fuzz_close,
        /* void (*set_block_received)(void (*handler)(void)); */          &btstack_uart_fuzz_set_block_received,
        /* void (*set_block_sent)(void (*handler)(void)); */              &btstack_uart_fuzz_set_block_sent,
        /* int  (*set_baudrate)(uint32_t baudrate); */                    &btstack_uart_fuzz_set_baudrate,
        /* int  (*set_parity)(int parity); */                             &btstack_uart_fuzz_set_parity,
        /* int  (*set_flowcontrol)(int flowcontrol); */                   NULL,
        /* void (*receive_block)(uint8_t *buffer, uint16_t len); */       &btstack_uart_fuzz_receive_block,
        /* void (*send_block)(const uint8_t *buffer, uint16_t length); */ &btstack_uart_fuzz_send_block,
        /* int (*get_supported_sleep_modes); */                           &btstack_uart_fuzz_get_supported_sleep_modes,
        /* void (*set_sleep)(btstack_uart_sleep_mode_t sleep_mode); */    &btstack_uart_fuzz_set_sleep,
        /* void (*set_wakeup_handler)(void (*handler)(void)); */          &btstack_uart_fuzz_set_wakeup_handler,
};

static void packet_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            if (size < 2) __builtin_trap();
            if ((2 + packet[1]) != size)__builtin_trap();
            break;
        case HCI_SCO_DATA_PACKET:
            if (size < 3) __builtin_trap();
            if ((3 + packet[2]) != size)__builtin_trap();
            break;
        case HCI_ACL_DATA_PACKET:
            if (size < 3) __builtin_trap();
            if ((4 + little_endian_read_16( packet, 2)) != size)__builtin_trap();
            break;
        default:
            __builtin_trap();
            break;
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    const hci_transport_t * transport = hci_transport_h4_instance(&uart_driver);
    read_request_len = 0;
    transport->init(&config);
    transport->register_packet_handler(&packet_handler);
    transport->open();
    while (size > 0){
        if (read_request_len == 0) __builtin_trap();

        uint16_t bytes_to_feed = btstack_min(read_request_len, size);
        memcpy(read_request_buffer, data, bytes_to_feed);
        size -= bytes_to_feed;
        data += bytes_to_feed;
        (*block_received)();
    }
    return 0;
}
