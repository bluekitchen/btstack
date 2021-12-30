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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define BTSTACK_FILE__ "bnep_lwip_lwip.c"

/*
 * bnep_lwip_lwip_.c
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
#include "lwip/prot/dhcp.h"
#include "netif/etharp.h"

#if LWIP_IPV6
#include "lwip/ethip6.h"
#endif

#include "bnep_lwip.h"

#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_util.h"
#include "btstack_event.h"
#include "classic/bnep.h"

#if NO_SYS
#include "btstack_ring_buffer.h"
#include "btstack_run_loop.h"
#include "lwip/timeouts.h"
#else
#include "btstack_run_loop_freertos.h"
#endif

/* Short name used for netif in lwIP */
#define IFNAME0 'b'
#define IFNAME1 't'

#define LWIP_TIMER_INTERVAL_MS 25

static void bnep_lwip_outgoing_process(void * arg);
static int bnep_lwip_outgoing_packets_empty(void);

// lwip data
static struct netif btstack_netif;

// outgoing queue
#if NO_SYS
static uint8_t bnep_lwip_outgoing_queue_storage[ (TCP_SND_QUEUELEN+1) * sizeof(struct pbuf *)];
static btstack_ring_buffer_t bnep_lwip_outgoing_queue;
#else
static QueueHandle_t bnep_lwip_outgoing_queue;
#endif

#if NO_SYS
static btstack_timer_source_t   bnep_lwip_timer;
#endif

// bnep data
static uint16_t  bnep_cid;
static btstack_packet_handler_t client_handler;

// next packet only modified from btstack context
static struct pbuf * bnep_lwip_outgoing_next_packet;

// temp buffer to unchain buffer
static uint8_t btstack_network_outgoing_buffer[HCI_ACL_PAYLOAD_SIZE];

// helper functions to hide NO_SYS vs. FreeRTOS implementations

static int bnep_lwip_outgoing_init_queue(void){
#if NO_SYS
    btstack_ring_buffer_init(&bnep_lwip_outgoing_queue, bnep_lwip_outgoing_queue_storage, sizeof(bnep_lwip_outgoing_queue_storage));
#else
    bnep_lwip_outgoing_queue = xQueueCreate(TCP_SND_QUEUELEN, sizeof(struct pbuf *));
    if (bnep_lwip_outgoing_queue == NULL){
        log_error("cannot allocate outgoing queue");
        return 1;
    }
#endif
    return 0;
}

static void bnep_lwip_outgoing_reset_queue(void){
#if NO_SYS
    btstack_ring_buffer_init(&bnep_lwip_outgoing_queue, bnep_lwip_outgoing_queue_storage, sizeof(bnep_lwip_outgoing_queue_storage));
#else
    xQueueReset(bnep_lwip_outgoing_queue);
#endif
}

static void bnep_lwip_outgoing_queue_packet(struct pbuf *p){
#if NO_SYS
    // queue up
    btstack_ring_buffer_write(&bnep_lwip_outgoing_queue, (uint8_t *) &p, sizeof(struct pbuf *));
#else
    // queue up
    xQueueSendToBack(bnep_lwip_outgoing_queue, &p, portMAX_DELAY);
#endif
}

static struct pbuf * bnep_lwip_outgoing_pop_packet(void){
    struct pbuf * p = NULL;
#if NO_SYS
    uint32_t bytes_read = 0;
    btstack_ring_buffer_read(&bnep_lwip_outgoing_queue, (uint8_t *) &p, sizeof(struct pbuf *), &bytes_read);
    (void) bytes_read;
#else
    xQueueReceive(bnep_lwip_outgoing_queue, &p, portMAX_DELAY);
#endif
    return p;
}

static int bnep_lwip_outgoing_packets_empty(void){
#if NO_SYS
    return btstack_ring_buffer_empty(&bnep_lwip_outgoing_queue);
 #else
    return uxQueueMessagesWaiting(bnep_lwip_outgoing_queue) == 0;
#endif
}

static void bnep_lwip_free_pbuf(struct pbuf * p){
#if NO_SYS
    // release buffer / decrease refcount
    pbuf_free(p);
 #else
    // release buffer / decrease refcount
    pbuf_free_callback(p);
#endif
}

static void bnep_lwip_outgoing_packet_processed(void){
    // free pbuf 
    bnep_lwip_free_pbuf(bnep_lwip_outgoing_next_packet);
    // mark as done
    bnep_lwip_outgoing_next_packet = NULL;
}

static void bnep_lwip_trigger_outgoing_process(void){
#if NO_SYS
    bnep_lwip_outgoing_process(NULL);
#else
    btstack_run_loop_freertos_execute_code_on_main_thread(&bnep_lwip_outgoing_process, NULL);
#endif
}

/// lwIP functions

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
    
    log_info("low_level_output: packet %p, len %u, total len %u ", p, p->len, p->tot_len);

    // bnep up?
    if (bnep_cid == 0) return ERR_OK;

    // inc refcount
    pbuf_ref( p );

    // queue empty now?
    int queue_empty = bnep_lwip_outgoing_packets_empty();

    // queue up
    bnep_lwip_outgoing_queue_packet(p);

    // trigger processing if queue was empty (might be new packet)
    if (queue_empty){
        bnep_lwip_trigger_outgoing_process();
    }

    return ERR_OK;
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

static err_t bnep_lwip_netif_init(struct netif *netif){

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

static int bnep_lwip_netif_up(bd_addr_t network_address){
    log_info("bnep_lwip_netif_up start addr %s", bd_addr_to_str(network_address));

    // set mac address
    btstack_netif.hwaddr_len = 6;
    memcpy(btstack_netif.hwaddr, network_address, 6);

    // link is up
    btstack_netif.flags |= NETIF_FLAG_LINK_UP;

    // if up
    netif_set_up(&btstack_netif);

    return 0;
}

/**
 * @brief Bring up network interfacd
 * @param network_address
 * @return 0 if ok
 */
static int bnep_lwip_netif_down(void){
    log_info("bnep_lwip_netif_down");

    // link is down
    btstack_netif.flags &= ~NETIF_FLAG_LINK_UP;

    netif_set_down(&btstack_netif);
    return 0;
}

/**
 * @brief Forward packet to TCP/IP stack
 * @param packet
 * @param size
 */
static void bnep_lwip_netif_process_packet(const uint8_t * packet, uint16_t size){

    /* We allocate a pbuf chain of pbufs from the pool. */
    struct pbuf * p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
    log_debug("bnep_lwip_netif_process_packet, pbuf_alloc = %p", p);

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
        bnep_lwip_free_pbuf(p);
        return;
    }

    /* pass all packets to ethernet_input, which decides what packets it supports */
    int res = btstack_netif.input(p, &btstack_netif);
    if (res != ERR_OK){
        log_error("bnep_lwip_netif_process_packet: IP input error\n");
        bnep_lwip_free_pbuf(p);
        p = NULL;
    }
}


// BNEP Functions & Handler

#if NO_SYS
static void bnep_lwip_timeout_handler(btstack_timer_source_t * ts){

    // process lwIP timers
    sys_check_timeouts();

    // check if link is still up
    if ((btstack_netif.flags & NETIF_FLAG_LINK_UP) == 0) return;

    // restart timer
    btstack_run_loop_set_timer(ts, LWIP_TIMER_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}
#endif

static void bnep_lwip_outgoing_process(void * arg){
    UNUSED(arg);

    // previous packet not sent yet
    if (bnep_lwip_outgoing_next_packet) return;

    bnep_lwip_outgoing_next_packet = bnep_lwip_outgoing_pop_packet();

    // request can send now
    bnep_request_can_send_now_event(bnep_cid);
}

static void bnep_lwip_send_packet(void){
    if (bnep_lwip_outgoing_next_packet == NULL){
        log_error("CAN SEND NOW, but now packet queued");
        return;
    }

    // flatten into our buffer
    uint32_t len = btstack_min(sizeof(btstack_network_outgoing_buffer), bnep_lwip_outgoing_next_packet->tot_len);
    pbuf_copy_partial(bnep_lwip_outgoing_next_packet, btstack_network_outgoing_buffer, len, 0);
    bnep_send(bnep_cid, (uint8_t*) btstack_network_outgoing_buffer, len);
}

static void bnep_lwip_packet_sent(void){
    log_debug("bnep_lwip_packet_sent: %p", bnep_lwip_outgoing_next_packet);

    // release current packet
    bnep_lwip_outgoing_packet_processed();

    // more ?
    if (bnep_lwip_outgoing_packets_empty()) return;
    bnep_lwip_trigger_outgoing_process();
}

static void bnep_lwip_discard_packets(void){
    // discard current packet
    if (bnep_lwip_outgoing_next_packet){
        bnep_lwip_outgoing_packet_processed();
    }

    // reset queue
    bnep_lwip_outgoing_reset_queue();
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
/* LISTING_PAUSE */
    UNUSED(channel);

    bd_addr_t local_addr;
  
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {            

                /* @text BNEP_EVENT_CHANNEL_OPENED is received after a BNEP connection was established or 
                 * or when the connection fails. The status field returns the error code.
                 * 
                 * The TAP network interface is then configured. A data source is set up and registered with the 
                 * run loop to receive Ethernet packets from the TAP interface.
                 *
                 * The event contains both the source and destination UUIDs, as well as the MTU for this connection and
                 * the BNEP Channel ID, which is used for sending Ethernet packets over BNEP.
                 */  
                case BNEP_EVENT_CHANNEL_OPENED:
                    if (bnep_event_channel_opened_get_status(packet) != 0) break;

                    bnep_cid = bnep_event_channel_opened_get_bnep_cid(packet);
                    
                    /* Setup network interface */
                    gap_local_bd_addr(local_addr);
                    bnep_lwip_netif_up(local_addr);

#if NO_SYS
                    // start timer
                    btstack_run_loop_set_timer_handler(&bnep_lwip_timer, bnep_lwip_timeout_handler);
                    btstack_run_loop_set_timer(&bnep_lwip_timer, LWIP_TIMER_INTERVAL_MS);
                    btstack_run_loop_add_timer(&bnep_lwip_timer);
#endif
                    break;
                
                /* @text BNEP_EVENT_CHANNEL_CLOSED is received when the connection gets closed.
                 */
                case BNEP_EVENT_CHANNEL_CLOSED:
                    bnep_cid = 0;
                    bnep_lwip_discard_packets();

                    // Mark Link as Down
                    bnep_lwip_netif_down();
                    break;

                /* @text BNEP_EVENT_CAN_SEND_NOW indicates that a new packet can be send. This triggers the send of a 
                 * stored network packet. The tap datas source can be enabled again
                 */
                case BNEP_EVENT_CAN_SEND_NOW:
                    bnep_lwip_send_packet();
                    bnep_lwip_packet_sent();
                    break;
                    
                default:
                    break;
            }
            break;

        /* @text Ethernet packets from the remote device are received in the packet handler with type BNEP_DATA_PACKET.
         * It is forwarded to the TAP interface.
         */
        case BNEP_DATA_PACKET:
            if (bnep_cid == 0) break;
            // Write out the ethernet frame to the network interface
            bnep_lwip_netif_process_packet(packet, size);
            break;            
            
        default:
            break;
    }

    // forward to app
    if (!client_handler) return;
    (*client_handler)(packet_type, channel, packet, size);
}

/// API

/**
 * @brief Initialize network interface
 * @param send_packet_callback
 */
void bnep_lwip_init(void){

    // set up outgoing queue
    int error = bnep_lwip_outgoing_init_queue();
    if (error) return;

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

    netif_add(&btstack_netif, &fsl_netif0_ipaddr, &fsl_netif0_netmask, &fsl_netif0_gw, NULL, bnep_lwip_netif_init, input_function);
    netif_set_default(&btstack_netif);
}

/**
 * @brief Register packet handler for BNEP events
 */
void bnep_lwip_register_packet_handler(btstack_packet_handler_t handler){
    client_handler = handler;
}

/**
 * @brief Register BNEP service
 * @brief Same as benp_register_service, but bnep lwip adapter handles all events
 * @param service_uuid 
 * @Param max_frame_size
 */
uint8_t bnep_lwip_register_service(uint16_t service_uuid, uint16_t max_frame_size){
    return bnep_register_service(packet_handler, service_uuid, max_frame_size);
}

/**
 * @brief Creates BNEP connection (channel) to a given server on a remote device with baseband address. A new baseband connection will be initiated if necessary.
 * @note: uses our packet handler to manage lwIP network interface
 * @param addr
 * @param l2cap_psm
 * @param uuid_src
 * @param uuid_dest
 * @return status
 */
uint8_t bnep_lwip_connect(bd_addr_t addr, uint16_t l2cap_psm, uint16_t uuid_src, uint16_t uuid_dest){
    int status = bnep_connect(packet_handler, addr, l2cap_psm, uuid_src, uuid_dest);
    if (status != 0){
        return ERROR_CODE_UNSPECIFIED_ERROR;
    } else {
        return ERROR_CODE_SUCCESS;
    }
}
