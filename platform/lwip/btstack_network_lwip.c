/*
 * Copyright (C) 2017 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btstack_network_lwip.c"

/*
 * btstack_network_lwip_.c
 */

#include "lwip/netif.h"
#include "lwip/sys.h"
#include "lwip/arch.h"
#include "lwip/api.h"
#include "lwip/netifapi.h"
#include "lwip/tcpip.h"
#include "lwip/ip.h"
#include "lwip/dhcp.h"
#include "lwip/sockets.h"
#include "netif/etharp.h"
#include "lwip/prot/dhcp.h"

#include "btstack_network.h"

#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_util.h"

#if NO_SYS
#include "btstack_ring_buffer.h"
#include "btstack_run_loop.h"
#include "lwip/timeouts.h"
#else
#include "btstack_run_loop_freertos.h"
#endif

/* Define those to better describe your network interface. */
#define IFNAME0 'b'
#define IFNAME1 't'

#define LWIP_TIMER_INTERVAL_MS 25

static void (*btstack_network_send_packet_callback)(const uint8_t * packet, uint16_t size);

static struct netif btstack_netif;

// temp buffer to unchain buffer
static uint8_t btstack_network_outgoing_buffer[HCI_ACL_PAYLOAD_SIZE];

// outgoing queue
#if NO_SYS
static uint8_t btstack_network_outgoing_queue_storage[ (TCP_SND_QUEUELEN+1) * sizeof(struct pbuf *)];
static btstack_ring_buffer_t btstack_network_outgoing_queue;
#else
static QueueHandle_t btstack_network_outgoing_queue;
#endif

static btstack_timer_source_t lwip_timer;

// next packet only modified from btstack context
static struct pbuf * btstack_network_outgoing_next_packet;

static void btstack_network_timeout_handler(btstack_timer_source_t * ts){

    // process lwIP timers
    sys_check_timeouts();

    // check if link is still up
    if ((btstack_netif.flags & NETIF_FLAG_LINK_UP) == 0) return;

    // restart timer
    btstack_run_loop_set_timer(ts, LWIP_TIMER_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}

static void btstack_network_outgoing_process(void * arg){
    UNUSED(arg);

	// previous packet not sent yet
    if (btstack_network_outgoing_next_packet) return;

    // get new pbuf to send
#if NO_SYS
    uint32_t bytes_read = 0;
    void * pointer = NULL;
    btstack_ring_buffer_read(&btstack_network_outgoing_queue, (uint8_t *) &pointer, sizeof(struct pbuf *), &bytes_read);
    (void) bytes_read;
    btstack_network_outgoing_next_packet = pointer;
#else
    xQueueReceive(btstack_network_outgoing_queue, &btstack_network_outgoing_next_packet, portMAX_DELAY);
#endif

    log_info("btstack_network_outgoing_process send %p", btstack_network_outgoing_next_packet);

    // flatten into our buffer
    uint32_t len = btstack_min(sizeof(btstack_network_outgoing_buffer), btstack_network_outgoing_next_packet->tot_len);
    pbuf_copy_partial(btstack_network_outgoing_next_packet, btstack_network_outgoing_buffer, len, 0);

	(*btstack_network_send_packet_callback)(btstack_network_outgoing_buffer, len);
}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become availale since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */
static err_t low_level_output( struct netif *netif, struct pbuf *p ){
    UNUSED(netif);
    
    log_info("low_level_output: queue %p, len %u, total len %u", p, p->len, p->tot_len);

    // inc refcount
    pbuf_ref( p );

#if NO_SYS

    // queue up
    void * pointer = (void * ) p;
    btstack_ring_buffer_write(&btstack_network_outgoing_queue, (uint8_t *) &pointer, sizeof(struct pbuf *));

    // trigger (might be new packet)
    btstack_network_outgoing_process(NULL);

#else

    // queue up
    xQueueSendToBack(btstack_network_outgoing_queue, &p, portMAX_DELAY);

    // trigger (might be new packet)
    btstack_run_loop_freertos_execute_code_on_main_thread(&btstack_network_outgoing_process, NULL);

#endif

	return (err_t) ERR_OK;
}

/**
 * @brief Notify network interface that packet from send_packet_callback was sent and the next packet can be delivered.
 */
void btstack_network_packet_sent(void){

	log_debug("btstack_network_packet_sent: %p", btstack_network_outgoing_next_packet);

#if NO_SYS

    // release buffer / decrease refcount
    pbuf_free(btstack_network_outgoing_next_packet);

    // mark as done
    btstack_network_outgoing_next_packet = NULL;

    // more messages queued?
    if (btstack_ring_buffer_empty(&btstack_network_outgoing_queue)) return;

    // trigger (might be new packet)
    btstack_network_outgoing_process(NULL);
 
#else

    // release buffer / decrease refcount
    pbuf_free_callback(btstack_network_outgoing_next_packet);

	// mark as done
	btstack_network_outgoing_next_packet = NULL;

	// more messages queued?
	if (uxQueueMessagesWaiting(btstack_network_outgoing_queue) == 0) return;

    // trigger (might be new packet)
    btstack_run_loop_freertos_execute_code_on_main_thread(&btstack_network_outgoing_process, NULL);

#endif
}

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */

static err_t btstack_network_netif_init(struct netif *netif){

    // interface short name
    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;

    // mtu
    netif->mtu = 1600;

    /* device capabilities */
    netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

    /* We directly use etharp_output() here to save a function call.
     * You can instead declare your own function an call etharp_output()
     * from it if you have to do some checks before sending (e.g. if link
     * is available...)
     */
    netif->output = etharp_output;
#if LWIP_IPV6
    netif->output_ip6 = ethip6_output;
#endif
    netif->linkoutput = low_level_output;

    return ERR_OK;
}

/**
 * @brief Initialize network interface
 * @param send_packet_callback
 */
void btstack_network_init(void (*send_packet_callback)(const uint8_t * packet, uint16_t size)){

    log_info("btstack_network_init");

    // set up outgoing queue
#if NO_SYS
    btstack_ring_buffer_init(&btstack_network_outgoing_queue, btstack_network_outgoing_queue_storage, sizeof(btstack_network_outgoing_queue_storage));
#else
    btstack_network_outgoing_queue = xQueueCreate(TCP_SND_QUEUELEN, sizeof(struct pbuf *));
    if (btstack_network_outgoing_queue == NULL){
    	log_error("cannot allocate outgoing queue");
    	return;
    }
#endif

    btstack_network_send_packet_callback = send_packet_callback;

    ip4_addr_t fsl_netif0_ipaddr, fsl_netif0_netmask, fsl_netif0_gw;

#if 0
    // when using DHCP Client, no address
    IP4_ADDR(&fsl_netif0_ipaddr, 0U, 0U, 0U, 0U);
    IP4_ADDR(&fsl_netif0_netmask, 0U, 0U, 0U, 0U);
#else
    // when playing DHCP Server, set address
    IP4_ADDR(&fsl_netif0_ipaddr, 192U, 168U, 7U, 1U);
    IP4_ADDR(&fsl_netif0_netmask, 255U, 255U, 255U, 0U);
#endif
    IP4_ADDR(&fsl_netif0_gw, 0U, 0U, 0U, 0U);

    // input function differs for sys vs nosys
    netif_input_fn input_function;
#if NO_SYS
    input_function = ethernet_input;
#else
    input_function = tcpip_input;
#endif

    netif_add(&btstack_netif, &fsl_netif0_ipaddr, &fsl_netif0_netmask, &fsl_netif0_gw, NULL, btstack_network_netif_init, input_function);
    netif_set_default(&btstack_netif);
}

/**
 * @brief Get network name after network was activated
 * @note e.g. tapX on Linux, might not be useful on all platforms
 * @returns network name
 */
const char * btstack_network_get_name(void){
    return "bt";
}

int btstack_network_up(bd_addr_t network_address){
    log_info("btstack_network_up start addr %s", bd_addr_to_str(network_address));

    // set mac address
    btstack_netif.hwaddr_len = 6;
    memcpy(btstack_netif.hwaddr, network_address, 6);

    // link is up
    btstack_netif.flags |= NETIF_FLAG_LINK_UP;

    // if up
    netif_set_up(&btstack_netif);

#if NO_SYS
    // start timer
    btstack_run_loop_set_timer_handler(&lwip_timer, btstack_network_timeout_handler);
    btstack_run_loop_set_timer(&lwip_timer, LWIP_TIMER_INTERVAL_MS);
    btstack_run_loop_add_timer(&lwip_timer);
#endif

    return 0;
}

/**
 * @brief Forward packet to TCP/IP stack
 * @param packet
 * @param size
 */
void btstack_network_process_packet(const uint8_t * packet, uint16_t size){

	/* We allocate a pbuf chain of pbufs from the pool. */
	struct pbuf * p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
	log_debug("btstack_network_process_packet, pbuf_alloc = %p", p);

	if (!p) return;

    /* store packet in pbuf chain */
	struct pbuf * q = p;
	while (q != NULL && size){
    	memcpy(q->payload, packet, q->len);
    	packet += q->len;
    	size   -= q->len;
    	q = q->next;
    }

	if (size != 0){
		log_error("failed to copy data into pbuf");
		pbuf_free(p);
		return;
	}

    /* pass all packets to ethernet_input, which decides what packets it supports */
    if (btstack_netif.input(p, &btstack_netif) != ERR_OK){
        log_error("btstack_network_process_packet: IP input error\n");
        pbuf_free(p);
        p = NULL;
    }
}

/**
 * @brief Bring up network interfacd
 * @param network_address
 * @return 0 if ok
 */
int btstack_network_down(void){
    log_info("btstack_network_up down");

    // link is down
    btstack_netif.flags &= ~NETIF_FLAG_LINK_UP;

    netif_set_down(&btstack_netif);
    return 0;
}

