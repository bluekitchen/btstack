#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "btstack_config.h"

#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_windows.h"
#include "hci.h"
#include "hci_dump.h"
#include "hal_led.h"
#include "btstack_link_key_db_fs.h"

#include "stdin_support.h"

#include "btstack_chipset_bcm.h"
#include "btstack_chipset_csr.h"
#include "btstack_chipset_cc256x.h"
#include "btstack_chipset_em9301.h"
#include "btstack_chipset_stlc2500d.h"
#include "btstack_chipset_tc3566x.h"

int btstack_main(int argc, const char * argv[]);
static void local_version_information_handler(uint8_t * packet);

static hci_transport_config_uart_t config = {
    HCI_TRANSPORT_CONFIG_UART,
    115200,
    0,  // main baudrate
    1,  // flow control
    NULL,
};

int is_bcm;

static int led_state = 0;

void hal_led_toggle(void){
    led_state = 1 - led_state;
    printf("LED State %u\n", led_state);
}

static void sigint_handler(int param){
    UNUSED(param);

    printf("CTRL-C = SIGINT received, shutting down..\n");   
    log_info("sigint_handler: shutting down");

    // reset anyway
    btstack_stdin_reset();

    // power down
    hci_power_control(HCI_POWER_OFF);
    hci_close();
    log_info("Good bye, see you.\n");    
    exit(0);
}

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    switch (hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) break;
            printf("BTstack up and running.\n");
            break;
        case HCI_EVENT_COMMAND_COMPLETE:
            if (HCI_EVENT_IS_COMMAND_COMPLETE(packet, hci_read_local_name)){
                // terminate, name 248 chars
                packet[6+248] = 0;
                printf("Local name: %s\n", &packet[6]);
                if (is_bcm){
                    btstack_chipset_bcm_set_device_name((const char *)&packet[6]);
                }
            }
            if (HCI_EVENT_IS_COMMAND_COMPLETE(packet, hci_read_local_version_information)){
                local_version_information_handler(packet);
            }
            break;

        default:
            break;
    }
}

static void use_fast_uart(void){
    printf("Using 921600 baud.\n");
    config.baudrate_main = 921600;
}

static void local_version_information_handler(uint8_t * packet){
    printf("Local version information:\n");
    uint16_t hci_version    = little_endian_read_16(packet, 4);
    uint16_t hci_revision   = little_endian_read_16(packet, 6);
    uint16_t lmp_version    = little_endian_read_16(packet, 8);
    uint16_t manufacturer   = little_endian_read_16(packet, 10);
    uint16_t lmp_subversion = little_endian_read_16(packet, 12);
    printf("- HCI Version  0x%04x\n", hci_version);
    printf("- HCI Revision 0x%04x\n", hci_revision);
    printf("- LMP Version  0x%04x\n", lmp_version);
    printf("- LMP Revision 0x%04x\n", lmp_subversion);
    printf("- Manufacturer 0x%04x\n", manufacturer);
    switch (manufacturer){
        case COMPANY_ID_CAMBRIDGE_SILICON_RADIO:
            printf("Cambridge Silicon Radio - CSR chipset.\n");
            use_fast_uart();
            hci_set_chipset(btstack_chipset_csr_instance());
            break;
        case COMPANY_ID_TEXAS_INSTRUMENTS_INC: 
            printf("Texas Instruments - CC256x compatible chipset.\n");
            if (lmp_subversion != btstack_chipset_cc256x_lmp_subversion()){
                printf("Error: LMP Subversion does not match initscript!");
                printf("Your initscripts is for %s chipset\n", btstack_chipset_cc256x_lmp_subversion() < lmp_subversion ? "an older" : "a newer");
                printf("Please update Makefile to include the appropriate bluetooth_init_cc256???.c file\n");
                exit(10);
            }
            use_fast_uart();
            hci_set_chipset(btstack_chipset_cc256x_instance());
#ifdef ENABLE_EHCILL
            printf("eHCILL enabled.\n");
#else
            printf("eHCILL disable.\n");
#endif
            break;
        case COMPANY_ID_BROADCOM_CORPORATION:   
            printf("Broadcom - using BCM driver.\n");
            hci_set_chipset(btstack_chipset_bcm_instance());
            use_fast_uart();
            is_bcm = 1;
            break;
        case COMPANY_ID_ST_MICROELECTRONICS:   
            printf("ST Microelectronics - using STLC2500d driver.\n");
            use_fast_uart();
            hci_set_chipset(btstack_chipset_stlc2500d_instance());
            break;
        case COMPANY_ID_EM_MICROELECTRONICS_MARIN:
            printf("EM Microelectronics - using EM9301 driver.\n");
            hci_set_chipset(btstack_chipset_em9301_instance());
            break;
        case COMPANY_ID_NORDIC_SEMICONDUCTOR_ASA:
            printf("Nordic Semiconductor nRF5 chipset.\n");
            break;        
        default:
            printf("Unknown manufacturer / manufacturer not supported yet.\n");
            break;
    }
}

int main(int argc, const char * argv[]){
	printf("BTstack on windows booting up\n");

	/// GET STARTED with BTstack ///
	btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_windows_get_instance());

    hci_dump_open("hci_dump.pklg", HCI_DUMP_PACKETLOGGER);

    // pick serial port
    config.device_name = "\\\\.\\COM7";

    // init HCI
    const btstack_uart_block_t * uart_driver = btstack_uart_block_windows_instance();
	const hci_transport_t * transport = hci_transport_h4_instance(uart_driver);
    const btstack_link_key_db_t * link_key_db = btstack_link_key_db_fs_instance();
	hci_init(transport, (void*) &config);
    hci_set_link_key_db(link_key_db);

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // handle CTRL-c
    signal(SIGINT, sigint_handler);

    // setup app
    btstack_main(argc, argv);

    // go
    btstack_run_loop_execute();    

	return 0;
}
