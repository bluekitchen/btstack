/*
 * Copyright (C) 2016 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

#define __BTSTACK_FILE__ "main.c"
 /**
  * BTstack port for Zephyr Bluetooth Controller
  *
  * Data Sources aside from the HCI Controller are not supported yet 
  * Timers are supported by waiting on the HCI Controller RX queue until the next timeout is due
  */

// libc
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// zephyr
#include <arch/cpu.h>
#include <device.h>
#include <init.h>
#include <misc/byteorder.h>
#include <misc/kernel_event_logger.h>
#include <misc/sys_log.h>
#include <misc/util.h>
#include <net/buf.h>
#include <sys_clock.h>
#include <uart.h>
#include <zephyr.h>

// Bluetooth Controller
#include "ll.h"
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/buf.h>
#include <bluetooth/hci_raw.h>

// Nordic NFK
#include "nrf.h"

// BTstack
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "hci.h"
#include "hci_dump.h"
#include "hci_transport.h"

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void (*transport_packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

//
// hci_transport_zephyr.c
//

/* HCI command buffers */
#define CMD_BUF_SIZE (CONFIG_BLUETOOTH_HCI_SEND_RESERVE + \
		      sizeof(struct bt_hci_cmd_hdr) + \
		      CONFIG_BLUETOOTH_MAX_CMD_LEN)

static struct nano_fifo avail_cmd_tx;
static NET_BUF_POOL(cmd_tx_pool, CONFIG_BLUETOOTH_HCI_CMD_COUNT, CMD_BUF_SIZE,
		    &avail_cmd_tx, NULL, BT_BUF_USER_DATA_MIN);

#define BT_L2CAP_MTU 64
/** Data size needed for ACL buffers */
#define BT_BUF_ACL_SIZE (CONFIG_BLUETOOTH_HCI_RECV_RESERVE + \
			 sizeof(struct bt_hci_acl_hdr) + \
			 4 /* L2CAP header size */ + \
			 BT_L2CAP_MTU)

static struct nano_fifo avail_acl_tx;
static NET_BUF_POOL(acl_tx_pool, CONFIG_BLUETOOTH_CONTROLLER_TX_BUFFERS,
		    BT_BUF_ACL_SIZE, &avail_acl_tx, NULL, BT_BUF_USER_DATA_MIN);

static struct nano_fifo rx_queue;
static struct nano_fifo tx_queue;

/**
 * init transport
 * @param transport_config
 */
static void transport_init(const void *transport_config){
	/* Initialize the buffer pools */
	net_buf_pool_init(cmd_tx_pool);
	net_buf_pool_init(acl_tx_pool);

	/* Initialize the FIFOs */
	nano_fifo_init(&tx_queue);
	nano_fifo_init(&rx_queue);

	/* startup Controller */
	bt_enable_raw(&rx_queue);
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
			buf = net_buf_get(&avail_cmd_tx, 0);
			if (buf) {
				bt_buf_set_type(buf, BT_BUF_CMD);
				memcpy(net_buf_add(buf, size), packet, size);
				bt_send(buf);
			} else {
				log_error("No available command buffers!\n");
			}
            break;
        case HCI_ACL_DATA_PACKET:
			buf = net_buf_get(&avail_acl_tx, 0);
			if (buf) {
				bt_buf_set_type(buf, BT_BUF_ACL_OUT);
				memcpy(net_buf_add(buf, size), packet, size);
				bt_send(buf);
			} else {
				log_error("No available ACL buffers!\n");
			}
            break;
        default:
            send_hardware_error(0x01);  // invalid HCI packet
            break;
    }

    return 0;   
}

static const hci_transport_t transport = {
    /* const char * name; */                                        "nRF5-Zephyr",
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
			case BT_BUF_ACL_IN:
				transport_packet_handler(HCI_ACL_DATA_PACKET, packet, size);
				break;
			case BT_BUF_EVT:
				transport_packet_handler(HCI_EVENT_PACKET, packet, size);
				break;
			default:
				log_error("Unknown type %u\n", bt_buf_get_type(buf));
				net_buf_unref(buf);
				break;
		}
		net_buf_unref(buf);
}

// btstack_run_loop_zephry.c

// the run loop
static btstack_linked_list_t timers;

static int sys_clock_ms_per_tick;	// set in btstack_run_loop_zephyr_init()

static uint32_t btstack_run_loop_zephyr_get_time_ms(void){
	return sys_tick_get_32() * sys_clock_ms_per_tick;
}

static uint32_t btstack_run_loop_zephyr_ticks_for_ms(uint32_t time_in_ms){
    return time_in_ms / sys_clock_ms_per_tick;
}

static void btstack_run_loop_zephyr_set_timer(btstack_timer_source_t *ts, uint32_t timeout_in_ms){
    uint32_t ticks = btstack_run_loop_zephyr_ticks_for_ms(timeout_in_ms);
    if (ticks == 0) ticks++;
    // time until next tick is < hal_tick_get_tick_period_in_ms() and we don't know, so we add one
    ts->timeout = sys_tick_get_32() + 1 + ticks; 
}

/**
 * Add timer to run_loop (keep list sorted)
 */
static void btstack_run_loop_zephyr_add_timer(btstack_timer_source_t *ts){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) &timers; it->next ; it = it->next){
        // don't add timer that's already in there
        if ((btstack_timer_source_t *) it->next == ts){
            log_error( "btstack_run_loop_timer_add error: timer to add already in list!");
            return;
        }
        if (ts->timeout < ((btstack_timer_source_t *) it->next)->timeout) {
            break;
        }
    }
    ts->item.next = it->next;
    it->next = (btstack_linked_item_t *) ts;
}

/**
 * Remove timer from run loop
 */
static int btstack_run_loop_zephyr_remove_timer(btstack_timer_source_t *ts){
    return btstack_linked_list_remove(&timers, (btstack_linked_item_t *) ts);
}

static void btstack_run_loop_zephyr_dump_timer(void){
#ifdef ENABLE_LOG_INFO 
    btstack_linked_item_t *it;
    int i = 0;
    for (it = (btstack_linked_item_t *) timers; it ; it = it->next){
        btstack_timer_source_t *ts = (btstack_timer_source_t*) it;
        log_info("timer %u, timeout %u\n", i, (unsigned int) ts->timeout);
    }
#endif
}

/**
 * Execute run_loop
 */
static void btstack_run_loop_zephyr_execute(void) {
    while (1) {
        // get next timeout
        int32_t timeout_ticks = TICKS_UNLIMITED;
        if (timers) {
            btstack_timer_source_t * ts = (btstack_timer_source_t *) timers;
            uint32_t now = sys_tick_get_32();
            if (ts->timeout < now){
                // remove timer before processing it to allow handler to re-register with run loop
                btstack_run_loop_remove_timer(ts);
                // printf("RL: timer %p\n", ts->process);
                ts->process(ts);
                continue;
            }
            timeout_ticks = ts->timeout - now;
        }
                
 	   	// process RX fifo only
    	struct net_buf *buf = net_buf_get_timeout(&rx_queue, 0, timeout_ticks);
		if (buf){
			transport_deliver_controller_packet(buf);
		}
	}
}

static void btstack_run_loop_zephyr_btstack_run_loop_init(void){
    timers = NULL;
    sys_clock_ms_per_tick  = sys_clock_us_per_tick / 1000;
    log_info("btstack_run_loop_init: ms_per_tick %u", sys_clock_ms_per_tick);
}

static const btstack_run_loop_t btstack_run_loop_wiced = {
    &btstack_run_loop_zephyr_btstack_run_loop_init,
    NULL,
    NULL,
    NULL,
    NULL,
    &btstack_run_loop_zephyr_set_timer,
    &btstack_run_loop_zephyr_add_timer,
    &btstack_run_loop_zephyr_remove_timer,
    &btstack_run_loop_zephyr_execute,
    &btstack_run_loop_zephyr_dump_timer,
    &btstack_run_loop_zephyr_get_time_ms,
};
/**
 * @brief Provide btstack_run_loop_posix instance for use with btstack_run_loop_init
 */
const btstack_run_loop_t * btstack_run_loop_zephyr_get_instance(void){
    return &btstack_run_loop_wiced;
}

// main.c

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != BTSTACK_EVENT_STATE) return;
    if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
    printf("BTstack up and running.\n");
}

int btstack_main(void);

void main(void)
{
	printf("BTstack booting up..\n");

	// start with BTstack init - especially configure HCI Transport
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_zephyr_get_instance());

    // enable full log output while porting
    hci_dump_open(NULL, HCI_DUMP_STDOUT);

    // init HCI
    hci_init(transport_get_instance(), NULL);

    // nRF5 chipsets don't have an official public address
    // Instead, they use a Static Random Address set in the factory
    bd_addr_t addr;
#if 0
    // set random static address
    big_endian_store_16(addr, 0, NRF_FICR->DEVICEADDR[1] | 0xc000);
    big_endian_store_32(addr, 2, NRF_FICR->DEVICEADDR[0]);
    gap_random_address_set(addr);
    printf("Random Static Address: %s\n", bd_addr_to_str(addr));
#else
    // make Random Static Address available via HCI Read BD ADDR as fake public address
    little_endian_store_32(addr, 0, NRF_FICR->DEVICEADDR[0]);
    little_endian_store_16(addr, 4, NRF_FICR->DEVICEADDR[1] | 0xc000);
    ll_address_set(0, addr);
#endif

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    
    // hand over to btstack embedded code 
    btstack_main();

    // go
    btstack_run_loop_execute();

    while (1){};
}
