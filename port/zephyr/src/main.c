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

#include <zephyr/storage/flash_map.h>

#if DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart)
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#endif

// Nordic SDK
#if defined(CONFIG_SOC_SERIES_NRF53X)
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

#include "hci_transport_zephyr.h"
#include "btstack_run_loop_zephyr.h"

#include "hal_flash_bank_zephyr.h"
#include "btstack_tlv_flash_bank.h"

// Uncomment to wait during startup for USB CDC console to be ready
// #define ENABLE_USB_CDC_WAIT

static btstack_packet_callback_registration_t hci_event_callback_registration;

static bd_addr_t local_addr = { 0 };
#ifdef ENABLE_BLE
static bd_addr_t local_le_random_addr = { 0 };
static bool local_le_random_addr_set;
#endif

static void print_local_addr(void){
#ifdef ENABLE_BLE
    if (local_le_random_addr_set){
        printf("BTstack up and running on %s (random).\n", bd_addr_to_str(local_le_random_addr));
        return;
    }
#endif
    gap_local_bd_addr(local_addr);
    printf("BTstack up and running on %s.\n", bd_addr_to_str(local_addr));
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
        default:
            printf("Unknown manufacturer.\n");
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
                    print_local_addr();
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
                case HCI_OPCODE_HCI_ZEPHYR_READ_STATIC_ADDRESS:
#ifdef ENABLE_BLE
                    params = hci_event_command_complete_get_return_parameters(packet);
                    if(params[0] != 0)
                        break;
                    if(params[1] == 0)
                        break;
                    if(size < 13)
                        break;
                    printf("Use static random address reported by Zephyr Controller.\n");
                    reverse_48(&params[2], local_le_random_addr);
                    local_le_random_addr_set = true;
                    gap_random_address_set(local_le_random_addr);
#endif
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

static hal_flash_bank_zephyr_t  hal_flash_bank_context;
static btstack_tlv_flash_bank_t btstack_tlv_flash_bank_context;

#if defined(ENABLE_USB_CDC_WAIT) && DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart)
static void wait_for_usb_cdc_console(void)
{
    const struct device * console = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    uint32_t dtr = 0;

    if (!device_is_ready(console)) {
        return;
    }

    while (!dtr) {
        uart_line_ctrl_get(console, UART_LINE_CTRL_DTR, &dtr);
        k_sleep(K_MSEC(100));
    }
}
#else
static void wait_for_usb_cdc_console(void)
{
}
#endif

int main(void)
{
#if defined(CONFIG_SOC_SERIES_NRF53X)
    // switch the nrf5340_cpuapp to 128MHz
    nrfx_clock_divider_set(NRF_CLOCK_DOMAIN_HFCLK, NRF_CLOCK_HFCLK_DIV_1);
#endif

    wait_for_usb_cdc_console();

    printf("BTstack booting up...\n");

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

#if defined(CONFIG_FLASH) && \
    DT_HAS_CHOSEN(zephyr_code_partition) && \
    FIXED_PARTITION_EXISTS(storage_partition)

    // setup TLV
    uint32_t bank_size = FIXED_PARTITION_SIZE(storage_partition)/2;
    uint32_t bank_0_addr = FIXED_PARTITION_OFFSET(storage_partition);
    uint32_t bank_1_addr = bank_0_addr+bank_size;
    printf("configure persistent TLV storage:\n");
    printf("    bank_size:   %8d\n", bank_size);
    printf("    bank_0_addr: %08x\n", bank_0_addr);
    printf("    bank_1_addr: %08x\n", bank_1_addr);
    const hal_flash_bank_t * hal_flash_bank_impl = hal_flash_bank_zephyr_init_instance(
            &hal_flash_bank_context,
            bank_size,
            16,
            bank_0_addr,
            bank_1_addr);
    const btstack_tlv_t * btstack_tlv_impl = btstack_tlv_flash_bank_init_instance(
            &btstack_tlv_flash_bank_context,
            hal_flash_bank_impl,
            &hal_flash_bank_context);
#else
    const btstack_tlv_t * btstack_tlv_impl = btstack_tlv_none_init_instance();
#endif
    btstack_tlv_set_instance(btstack_tlv_impl, &btstack_tlv_flash_bank_context);

#ifdef ENABLE_CLASSIC
    // setup Link Key DB using TLV
    const btstack_link_key_db_t * btstack_link_key_db = btstack_link_key_db_tlv_get_instance(btstack_tlv_impl, &btstack_tlv_flash_bank_context);
    hci_set_link_key_db(btstack_link_key_db);
#endif
#ifdef ENABLE_BLE
    // setup LE Device DB using TLV
    le_device_db_tlv_configure(btstack_tlv_impl, &btstack_tlv_flash_bank_context);
#endif

    // init HCI
    hci_init(hci_transport_zephyr_get_instance(), NULL);

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
