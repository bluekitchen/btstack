#define BTSTACK_FILE__ "gatt_whitelist.c"

#include <stdint.h>
#include <stdio.h>
#include "btstack.h"

#define HEARTBEAT_PERIOD_MS 2000

static btstack_timer_source_t heartbeat;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static bd_addr_t addr;

static void heartbeat_handler(struct btstack_timer_source *ts){
    addr[5]++;
    addr[4]++;
    gap_whitelist_add(BD_ADDR_TYPE_LE_PUBLIC, addr);
    printf("Add %s to Whitelist\n", bd_addr_to_str(addr));

    btstack_run_loop_set_timer(ts, HEARTBEAT_PERIOD_MS);
    btstack_run_loop_add_timer(ts);
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    if (packet_type != HCI_EVENT_PACKET) return;
    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
            // started
            printf("GAP Connect with Whitelist\n");
            gap_connect_with_whitelist();
            // set one-shot timer
            heartbeat.process = &heartbeat_handler;
            btstack_run_loop_set_timer(&heartbeat, HEARTBEAT_PERIOD_MS);
            btstack_run_loop_add_timer(&heartbeat);
            break;
        default:
            break;
    }
}

int btstack_main(void);
int btstack_main(void)
{
    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    hci_power_control(HCI_POWER_ON);
    return 0;
}
/* EXAMPLE_END */
