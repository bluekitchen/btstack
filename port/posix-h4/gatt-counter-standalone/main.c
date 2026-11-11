#include <stdio.h>

#include "btstack.h"
#include "btstack_run_loop_posix.h"
#include "btstack_uart.h"
#include "hci_transport_h4.h"

int btstack_main(int argc, const char * argv[]);

int main(int argc, const char * argv[]){
    if (argc != 2){
        fprintf(stderr, "Usage: %s <serial-device>\n", argv[0]);
        return 1;
    }
    hci_transport_config_uart_t config = {
        .type = HCI_TRANSPORT_CONFIG_UART,
        .baudrate_init = 115200,
        .baudrate_main = 0,
        .flowcontrol = BTSTACK_UART_FLOWCONTROL_ON,
        .device_name = argv[1],
        .parity = BTSTACK_UART_PARITY_OFF,
    };
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    hci_init(hci_transport_h4_instance_for_uart(btstack_uart_posix_instance()), &config);
    btstack_main(argc, argv);
    btstack_run_loop_execute();
    return 0;
}
