/*
 * Copyright (c) 2016 Nordic Semiconductor ASA
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/l2cap.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/buf.h>
#include <zephyr/bluetooth/hci_raw.h>

// Nordic NDK
#if defined(CONFIG_HAS_NORDIC_DRIVERS)
#include "nrf.h"
#include <nrfx_clock.h>
#endif

// BTstack
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"
#ifdef ENABLE_HCI_DUMP
#ifdef ENABLE_SEGGER_RTT_BINARY
#include "hci_dump_segger_rtt_binary.h"
#else
#include "hci_dump_embedded_stdout.h"
#endif
#endif
#include "hci_transport.h"
#include "bluetooth_company_id.h"
#include "btstack_chipset_zephyr.h"

#include "btstack_tlv.h"
#include "btstack_tlv_none.h"
#include "ble/le_device_db_tlv.h"

static K_FIFO_DEFINE(tx_queue);
static K_FIFO_DEFINE(rx_queue);

//
// hci_transport_zephyr.c
//

static void (*transport_packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

/**
 * init transport
 * @param transport_config
 */
static void transport_init(const void *transport_config){
    /* startup Controller */
    bt_enable_raw(&rx_queue);
    bt_hci_raw_set_mode( BT_HCI_RAW_MODE_PASSTHROUGH );
}

/**
 * open transport connection
 */
static int transport_open(void){
    return 0;
}

/**
 * close transport connection
 */
static int transport_close(void){
    return 0;
}

/**
 * register packet handler for HCI packets: ACL, SCO, and Events
 */
static void transport_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    transport_packet_handler = handler;
}

static void send_hardware_error(uint8_t error_code){
    // hci_outgoing_event[0] = HCI_EVENT_HARDWARE_ERROR;
    // hci_outgoing_event[1] = 1;
    // hci_outgoing_event[2] = error_code;
    // hci_outgoing_event_ready = 1;
}

static int transport_send_packet(uint8_t packet_type, uint8_t *packet, int size){
    struct net_buf *buf;
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
            buf = bt_buf_get_tx(BT_BUF_CMD, K_NO_WAIT, packet, size);
            if (!buf) {
                log_error("No available command buffers!\n");
                break;
            }

            bt_send(buf);
            break;
        case HCI_ACL_DATA_PACKET:
            buf = bt_buf_get_tx(BT_BUF_ACL_OUT, K_NO_WAIT, packet, size);
            if (!buf) {
                log_error("No available ACL buffers!\n");
                break;
            }

            bt_send(buf);
            break;
        case HCI_ISO_DATA_PACKET:
            buf = bt_buf_get_tx(BT_BUF_ISO_OUT, K_NO_WAIT, packet, size);
            if (!buf) {
                log_error("No available ISO buffers!\n");
                break;
            }

            bt_send(buf);
            break;
       default:
            send_hardware_error(0x01);  // invalid HCI packet
            break;
    }

    return 0;
}

static const hci_transport_t transport = {
    /* const char * name; */                                        "zephyr",
    /* void   (*init) (const void *transport_config); */            &transport_init,
    /* int    (*open)(void); */                                     &transport_open,
    /* int    (*close)(void); */                                    &transport_close,
    /* void   (*register_packet_handler)(void (*handler)(...); */   &transport_register_packet_handler,
    /* int    (*can_send_packet_now)(uint8_t packet_type); */       NULL,
    /* int    (*send_packet)(...); */                               &transport_send_packet,
    /* int    (*set_baudrate)(uint32_t baudrate); */                NULL,
    /* void   (*reset_link)(void); */                               NULL,
};

static const hci_transport_t * transport_get_instance(void){
    return &transport;
}

static void transport_deliver_controller_packet(struct net_buf * buf){
        uint16_t    size = buf->len;
        uint8_t * packet = buf->data;
        switch (bt_buf_get_type(buf)) {
            case BT_BUF_ISO_IN:
                transport_packet_handler(HCI_ISO_DATA_PACKET, packet, size);
                break;
            case BT_BUF_ACL_IN:
                transport_packet_handler(HCI_ACL_DATA_PACKET, packet, size);
                break;
            case BT_BUF_EVT:
                transport_packet_handler(HCI_EVENT_PACKET, packet, size);
                break;
            default:
                log_error("Unknown type %u\n", bt_buf_get_type(buf));
                break;
        }
        net_buf_unref(buf);
}

// btstack_run_loop_zephry.c

// the run loop

// TODO: handle 32 bit ms time overrun
static uint32_t btstack_run_loop_zephyr_get_time_ms(void){
    return  k_uptime_get_32();
}

static void btstack_run_loop_zephyr_set_timer(btstack_timer_source_t *ts, uint32_t timeout_in_ms){
    ts->timeout = k_uptime_get_32() + 1 + timeout_in_ms;
}

/**
 * Execute run_loop
 */
static void btstack_run_loop_zephyr_execute(void) {
    while (1) {
        // process timers
        uint32_t now = k_uptime_get_32();
        btstack_run_loop_base_process_timers(now);

        // get time until next timer expires
        k_timeout_t timeout;
        timeout.ticks = btstack_run_loop_base_get_time_until_timeout(now);
        if (timeout.ticks < 0){
            timeout.ticks = K_TICKS_FOREVER;
        }

        // process RX fifo only
        struct net_buf *buf = net_buf_get(&rx_queue, timeout);
        if (buf){
            transport_deliver_controller_packet(buf);
        }
    }
}

static void btstack_run_loop_zephyr_btstack_run_loop_init(void){
    btstack_run_loop_base_init();
}

static const btstack_run_loop_t btstack_run_loop_zephyr = {
    &btstack_run_loop_zephyr_btstack_run_loop_init,
    NULL,
    NULL,
    NULL,
    NULL,
    &btstack_run_loop_zephyr_set_timer,
    &btstack_run_loop_base_add_timer,
    &btstack_run_loop_base_remove_timer,
    &btstack_run_loop_zephyr_execute,
    &btstack_run_loop_base_dump_timer,
    &btstack_run_loop_zephyr_get_time_ms,
};
/**
 * @brief Provide btstack_run_loop_posix instance for use with btstack_run_loop_init
 */
static const btstack_run_loop_t * btstack_run_loop_zephyr_get_instance(void){
    return &btstack_run_loop_zephyr;
}

static btstack_packet_callback_registration_t hci_event_callback_registration;

static bd_addr_t local_addr = { 0 };

void nrf_get_static_random_addr( bd_addr_t addr ) {
    // nRF5 chipsets don't have an official public address
    // Instead, a Static Random Address is assigned during manufacturing
    // let's use it as well
#if defined(CONFIG_SOC_SERIES_NRF51X) || defined(CONFIG_SOC_SERIES_NRF52X)
    big_endian_store_16(addr, 0, NRF_FICR->DEVICEADDR[1] | 0xc000);
    big_endian_store_32(addr, 2, NRF_FICR->DEVICEADDR[0]);
#elif defined(CONFIG_SOC_SERIES_NRF53X)
    big_endian_store_16(addr, 0, NRF_FICR->INFO.DEVICEID[1] | 0xc000 );
    big_endian_store_32(addr, 2, NRF_FICR->INFO.DEVICEID[0]);
#endif
}

static void local_version_information_handler(uint8_t * packet){
    printf("Local version information:\n");
    uint16_t hci_version    = packet[6];
    uint16_t hci_revision   = little_endian_read_16(packet, 7);
    uint16_t lmp_version    = packet[9];
    uint16_t manufacturer   = little_endian_read_16(packet, 10);
    uint16_t lmp_subversion = little_endian_read_16(packet, 12);
    printf("- HCI Version    %#04x\n", hci_version);
    printf("- HCI Revision   %#04x\n", hci_revision);
    printf("- LMP Version    %#04x\n", lmp_version);
    printf("- LMP Subversion %#04x\n", lmp_subversion);
    printf("- Manufacturer   %#04x\n", manufacturer);
    switch (manufacturer){
        case BLUETOOTH_COMPANY_ID_NORDIC_SEMICONDUCTOR_ASA:
        case BLUETOOTH_COMPANY_ID_THE_LINUX_FOUNDATION:
            printf("Nordic Semiconductor nRF5 chipset.\n");
            hci_set_chipset(btstack_chipset_zephyr_instance());
            break;
        case BLUETOOTH_COMPANY_ID_PACKETCRAFT_INC:
            printf("PacketCraft HCI Controller\n");
            nrf_get_static_random_addr( local_addr );
            gap_random_address_set( local_addr );
            break;
        default:
            printf("Unknown manufacturer.\n");
            nrf_get_static_random_addr( local_addr );
            gap_random_address_set( local_addr );
            break;
    }
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    const uint8_t *params;
    if (packet_type != HCI_EVENT_PACKET) return;
    switch (hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:
            switch(btstack_event_state_get_state(packet)){
                case HCI_STATE_WORKING:
                    printf("BTstack up and running on %s.\n", bd_addr_to_str(local_addr));
                    break;
                case HCI_STATE_OFF:
                    log_info("Good bye, see you.\n");
                    break;
                default:
                    break;
            }
            break;
        case HCI_EVENT_COMMAND_COMPLETE:
            switch (hci_event_command_complete_get_command_opcode(packet)){
                case HCI_OPCODE_HCI_READ_LOCAL_VERSION_INFORMATION:
                    local_version_information_handler(packet);
                    break;
                case HCI_OPCODE_HCI_READ_BD_ADDR:
                    params = hci_event_command_complete_get_return_parameters(packet);
                    if(params[0] != 0)
                        break;
                    if(size < 12)
                        break;
                    reverse_48(&params[1], local_addr);
                    break;
                case HCI_OPCODE_HCI_ZEPHYR_READ_STATIC_ADDRESS:
                    params = hci_event_command_complete_get_return_parameters(packet);
                    if(params[0] != 0)
                        break;
                    if(size < 13)
                        break;
                    reverse_48(&params[2], local_addr);
                    gap_random_address_set(local_addr);
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

int btstack_main(void);

#if defined(CONFIG_BT_CTLR_ASSERT_HANDLER)
void bt_ctlr_assert_handle(char *file, uint32_t line)
{
    printf("CONFIG_BT_CTLR_ASSERT_HANDLER: file %s, line %u\n", file, line);
    while (1) {
    }
}
#endif /* CONFIG_BT_CTLR_ASSERT_HANDLER */

int main(void)
{
#if defined(CONFIG_SOC_SERIES_NRF53X)
    // switch the nrf5340_cpuapp to 128MHz
    nrfx_clock_divider_set(NRF_CLOCK_DOMAIN_HFCLK, NRF_CLOCK_HFCLK_DIV_1);
#endif

    printf("BTstack booting up..\n");

    // start with BTstack init - especially configure HCI Transport
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_zephyr_get_instance());

#ifdef ENABLE_HCI_DUMP
    // enable full log output while porting
#ifdef ENABLE_SEGGER_RTT_BINARY
    hci_dump_segger_rtt_binary_open( HCI_DUMP_PACKETLOGGER );
    hci_dump_init(hci_dump_segger_rtt_binary_get_instance());
#else
     hci_dump_init(hci_dump_embedded_stdout_get_instance());
#endif
#endif

    const btstack_tlv_t * btstack_tlv_impl = btstack_tlv_none_init_instance();
    // setup global tlv
    btstack_tlv_set_instance(btstack_tlv_impl, NULL);

    // setup LE Device DB using TLV
    le_device_db_tlv_configure(btstack_tlv_impl, NULL);

    // init HCI
    hci_init(transport_get_instance(), NULL);

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // hand over to btstack embedded code 
    btstack_main();

    // go
    btstack_run_loop_execute();

    while (1){};
    return 0;
}
