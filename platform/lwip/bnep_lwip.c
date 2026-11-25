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

#include "btstack_bool.h"

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
#ifdef HAVE_FREERTOS_INCLUDE_PREFIX
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#else
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#endif
#endif

/* Short name used for netif in lwIP */
#define IFNAME0 'b'
#define IFNAME1 't'

#define LWIP_TIMER_INTERVAL_MS 25

static void bnep_lwip_outgoing_process(void * arg);
static bool bnep_lwip_outgoing_packets_empty(void);

// lwip data
static struct netif btstack_netif;
static bool bnep_lwip_initialized;
static void (*bnep_lwip_init_callback)(uint8_t status);

#if !NO_SYS
// Protected by netif_mutex. The BTstack thread publishes desired state, and
// the TCP/IP thread applies it. A generation prevents lost updates.
static SemaphoreHandle_t netif_mutex;
static bool netif_callback_pending;
static bool netif_desired_up;
static bd_addr_t netif_desired_address;
static uint16_t netif_desired_mtu;
static uint32_t netif_desired_epoch;
static uint32_t netif_generation;
static btstack_timer_source_t netif_retry_timer;
static btstack_context_callback_registration_t netif_init_complete_registration;
static uint8_t netif_init_status;
#endif

// outgoing queue
#if NO_SYS
static uint8_t bnep_lwip_outgoing_queue_storage[ (TCP_SND_QUEUELEN+1) * sizeof(struct pbuf *)];
static btstack_ring_buffer_t bnep_lwip_outgoing_queue;
#else
#ifndef BNEP_LWIP_OUTGOING_QUEUE_SIZE
#define BNEP_LWIP_OUTGOING_QUEUE_SIZE TCP_SND_QUEUELEN
#endif
#if BNEP_LWIP_OUTGOING_QUEUE_SIZE < 1
#error "BNEP_LWIP_OUTGOING_QUEUE_SIZE must be positive"
#endif
typedef struct {
    bool used;
    uint16_t len;
    uint8_t data[HCI_ACL_PAYLOAD_SIZE];
} bnep_lwip_frame_t;
static bnep_lwip_frame_t bnep_lwip_frames[BNEP_LWIP_OUTGOING_QUEUE_SIZE];
static QueueHandle_t bnep_lwip_outgoing_queue;
// Protected by netif_mutex, including the queue's connection boundary.
static bool bnep_lwip_tx_link_up;
static bool bnep_lwip_outgoing_callback_pending;
static struct pbuf * bnep_lwip_pending_rx_free;
static btstack_timer_source_t bnep_lwip_rx_free_timer;
static btstack_context_callback_registration_t bnep_lwip_outgoing_callback_registration;
#endif

#if NO_SYS
static btstack_timer_source_t   bnep_lwip_timer;
#endif

// bnep data
// A CID is reserved as soon as an incoming connection is accepted.
static uint16_t  bnep_cid;
static bool bnep_lwip_outgoing_connection_pending;
static bd_addr_t bnep_lwip_outgoing_address;
static btstack_packet_handler_t client_handler;

// next packet only modified from btstack context
#if NO_SYS
static struct pbuf * bnep_lwip_outgoing_next_packet;
#else
static bnep_lwip_frame_t * bnep_lwip_outgoing_next_packet;
#endif

// temp buffer to unchain buffer
#if NO_SYS
static uint8_t btstack_network_outgoing_buffer[HCI_ACL_PAYLOAD_SIZE];
#endif

// helper functions to hide NO_SYS vs. FreeRTOS implementations

static int bnep_lwip_outgoing_init_queue(void){
#if NO_SYS
    btstack_ring_buffer_init(&bnep_lwip_outgoing_queue, bnep_lwip_outgoing_queue_storage, sizeof(bnep_lwip_outgoing_queue_storage));
#else
    bnep_lwip_outgoing_queue = xQueueCreate(BNEP_LWIP_OUTGOING_QUEUE_SIZE, sizeof(bnep_lwip_frame_t *));
    if (bnep_lwip_outgoing_queue == NULL){
        log_error("cannot allocate outgoing queue");
        return 1;
    }
#endif
    return 0;
}

#if NO_SYS
static bool bnep_lwip_outgoing_queue_packet(struct pbuf *p){
    return btstack_ring_buffer_write(&bnep_lwip_outgoing_queue, (uint8_t *) &p, sizeof(p)) == 0;
}

static struct pbuf * bnep_lwip_outgoing_pop_packet(void){
    struct pbuf * p = NULL;
    uint32_t bytes_read = 0;
    btstack_ring_buffer_read(&bnep_lwip_outgoing_queue, (uint8_t *) &p, sizeof(p), &bytes_read);
    return p;
}
#else
static bnep_lwip_frame_t * bnep_lwip_outgoing_pop_packet(void){
    bnep_lwip_frame_t * frame = NULL;
    xQueueReceive(bnep_lwip_outgoing_queue, &frame, 0);
    return frame;
}

static void bnep_lwip_rx_free_retry(btstack_timer_source_t * timer){
    if (pbuf_free_callback(bnep_lwip_pending_rx_free) == ERR_OK){
        bnep_lwip_pending_rx_free = NULL;
        return;
    }
    btstack_run_loop_set_timer(timer, 10);
    btstack_run_loop_add_timer(timer);
}
#endif

static bool bnep_lwip_outgoing_packets_empty(void){
#if NO_SYS
    return btstack_ring_buffer_empty(&bnep_lwip_outgoing_queue) != 0;
#else
    return uxQueueMessagesWaiting(bnep_lwip_outgoing_queue) == 0;
#endif
}

// Incoming buffers are released from the BTstack context. If posting their
// release fails, retain one pbuf and drop new input until the retry succeeds.
static void bnep_lwip_free_pbuf(struct pbuf * p){
#if NO_SYS
    pbuf_free(p);
#else
    if (pbuf_free_callback(p) != ERR_OK){
        btstack_assert(bnep_lwip_pending_rx_free == NULL);
        bnep_lwip_pending_rx_free = p;
        btstack_run_loop_set_timer(&bnep_lwip_rx_free_timer, 10);
        btstack_run_loop_add_timer(&bnep_lwip_rx_free_timer);
    }
#endif
}

static void bnep_lwip_outgoing_packet_processed(void){
#if NO_SYS
    pbuf_free(bnep_lwip_outgoing_next_packet);
#else
    xSemaphoreTake(netif_mutex, portMAX_DELAY);
    bnep_lwip_outgoing_next_packet->used = false;
    xSemaphoreGive(netif_mutex);
#endif
    bnep_lwip_outgoing_next_packet = NULL;
}

#if !NO_SYS
static void bnep_lwip_outgoing_callback(void * arg){
    xSemaphoreTake(netif_mutex, portMAX_DELAY);
    bnep_lwip_outgoing_callback_pending = false;
    xSemaphoreGive(netif_mutex);
    bnep_lwip_outgoing_process(arg);
}
#endif

static void bnep_lwip_trigger_outgoing_process(void){
    // Called in the BTstack context after a packet has been sent.
    bnep_lwip_outgoing_process(NULL);
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
 * @note Returns ERR_MEM when the bounded transmit queue is full; lwIP does
 *       not retry every protocol's dropped packets.
 */

static err_t low_level_output( struct netif *netif, struct pbuf *p ){
    UNUSED(netif);
    
    log_info("low_level_output: packet %p, len %u, total len %u ", p, p->len, p->tot_len);

#if NO_SYS
    if (bnep_cid == 0) return ERR_IF;
    pbuf_ref(p);
    bool queue_empty = bnep_lwip_outgoing_packets_empty();
    if (!bnep_lwip_outgoing_queue_packet(p)){
        pbuf_free(p);
        return ERR_MEM;
    }
    if (queue_empty) bnep_lwip_trigger_outgoing_process();
#else
    if (p->tot_len > HCI_ACL_PAYLOAD_SIZE) return ERR_BUF;
    xSemaphoreTake(netif_mutex, portMAX_DELAY);
    if (!bnep_lwip_tx_link_up){
        xSemaphoreGive(netif_mutex);
        return ERR_IF;
    }
    bnep_lwip_frame_t * frame = NULL;
    for (unsigned int i = 0; i < BNEP_LWIP_OUTGOING_QUEUE_SIZE; i++){
        if (!bnep_lwip_frames[i].used){
            frame = &bnep_lwip_frames[i];
            break;
        }
    }
    if (frame == NULL){
        xSemaphoreGive(netif_mutex);
        return ERR_MEM;
    }
    frame->used = true;
    frame->len = p->tot_len;
    if ((pbuf_copy_partial(p, frame->data, frame->len, 0) != frame->len) ||
        (xQueueSendToBack(bnep_lwip_outgoing_queue, &frame, 0) != pdPASS)){
        frame->used = false;
        xSemaphoreGive(netif_mutex);
        return ERR_MEM;
    }
    bool schedule = !bnep_lwip_outgoing_callback_pending;
    if (schedule) bnep_lwip_outgoing_callback_pending = true;
    xSemaphoreGive(netif_mutex);
    if (schedule){
        btstack_run_loop_execute_on_main_thread(&bnep_lwip_outgoing_callback_registration);
    }
#endif
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

    // Set to the negotiated BNEP payload limit when the channel opens.
    netif->mtu = 0;

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

static void bnep_lwip_netif_up(const bd_addr_t network_address, uint16_t mtu){
    btstack_netif.hwaddr_len = 6;
    memcpy(btstack_netif.hwaddr, network_address, 6);
    btstack_netif.mtu = mtu;
    netif_set_link_up(&btstack_netif);
    netif_set_up(&btstack_netif);
}

static void bnep_lwip_netif_down(void){
    netif_set_link_down(&btstack_netif);
    netif_set_down(&btstack_netif);
}

static bool bnep_lwip_netif_add(void){
    ip4_addr_t ipaddr, netmask, gw;
    IP4_ADDR(&ipaddr, 192U, 168U, 7U, 1U);
    IP4_ADDR(&netmask, 255U, 255U, 255U, 0U);
    IP4_ADDR(&gw, 0U, 0U, 0U, 0U);
#if NO_SYS
    netif_input_fn input = ethernet_input;
#else
    netif_input_fn input = tcpip_input;
#endif
    if (netif_add(&btstack_netif, &ipaddr, &netmask, &gw, NULL, bnep_lwip_netif_init, input) == NULL){
        return false;
    }
    netif_set_default(&btstack_netif);
    return true;
}

#if !NO_SYS
static void bnep_lwip_netif_init_complete(void * arg){
    UNUSED(arg);
    bnep_lwip_initialized = netif_init_status == ERROR_CODE_SUCCESS;
    if (bnep_lwip_init_callback != NULL){
        bnep_lwip_init_callback(netif_init_status);
    }
}

static void bnep_lwip_netif_apply(void * arg){
    UNUSED(arg);
    static bool netif_added;
    static uint32_t applied_epoch;
    if (!netif_added){
        netif_added = bnep_lwip_netif_add();
        netif_init_status = netif_added ? ERROR_CODE_SUCCESS : BTSTACK_MEMORY_ALLOC_FAILED;
        netif_init_complete_registration.callback = &bnep_lwip_netif_init_complete;
        btstack_run_loop_execute_on_main_thread(&netif_init_complete_registration);
        if (!netif_added){
            xSemaphoreTake(netif_mutex, portMAX_DELAY);
            netif_callback_pending = false;
            xSemaphoreGive(netif_mutex);
            return;
        }
    }
    for (;;){
        bd_addr_t address;
        uint16_t mtu;
        uint32_t generation, epoch;
        bool up;
        xSemaphoreTake(netif_mutex, portMAX_DELAY);
        up = netif_desired_up;
        epoch = netif_desired_epoch;
        mtu = netif_desired_mtu;
        bd_addr_copy(address, netif_desired_address);
        generation = netif_generation;
        xSemaphoreGive(netif_mutex);

        if (applied_epoch && (!up || (applied_epoch != epoch))){
            bnep_lwip_netif_down();
            applied_epoch = 0;
        }
        if (up && !applied_epoch){
            // Only enable output for the connection whose netif state is
            // about to be applied. A close can invalidate this snapshot.
            xSemaphoreTake(netif_mutex, portMAX_DELAY);
            bool current = netif_desired_up && (generation == netif_generation);
            if (current) bnep_lwip_tx_link_up = true;
            xSemaphoreGive(netif_mutex);
            if (current){
                bnep_lwip_netif_up(address, mtu);
                applied_epoch = epoch;
            }
        }
        xSemaphoreTake(netif_mutex, portMAX_DELAY);
        if (generation == netif_generation){
            netif_callback_pending = false;
            xSemaphoreGive(netif_mutex);
            return;
        }
        xSemaphoreGive(netif_mutex);
    }
}

static void bnep_lwip_netif_schedule(void);
static void bnep_lwip_netif_retry(btstack_timer_source_t * timer){
    UNUSED(timer);
    bnep_lwip_netif_schedule();
}

// Called only in BTstack context. Never wait for the TCP/IP thread.
static void bnep_lwip_netif_schedule(void){
    xSemaphoreTake(netif_mutex, portMAX_DELAY);
    if (!netif_callback_pending){
        netif_callback_pending = true;
        if (tcpip_try_callback(&bnep_lwip_netif_apply, NULL) != ERR_OK){
            netif_callback_pending = false;
            xSemaphoreGive(netif_mutex);
            btstack_run_loop_remove_timer(&netif_retry_timer);
            btstack_run_loop_set_timer(&netif_retry_timer, 10);
            btstack_run_loop_add_timer(&netif_retry_timer);
            return;
        }
    }
    xSemaphoreGive(netif_mutex);
}

static void bnep_lwip_netif_update(bool up, const bd_addr_t address, uint16_t mtu){
    xSemaphoreTake(netif_mutex, portMAX_DELAY);
    netif_desired_up = up;
    if (up){
        netif_desired_epoch++;
        if (netif_desired_epoch == 0) netif_desired_epoch++;
    }
    netif_desired_mtu = mtu;
    if (address != NULL) bd_addr_copy(netif_desired_address, address);
    netif_generation++;
    xSemaphoreGive(netif_mutex);
    bnep_lwip_netif_schedule();
}
#endif

/**
 * @brief Forward packet to TCP/IP stack
 * @param packet
 * @param size
 */
static void bnep_lwip_netif_process_packet(const uint8_t * packet, uint16_t size){

#if !NO_SYS
    if (bnep_lwip_pending_rx_free != NULL) return;
#endif
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

    // get next packet from queue
    bnep_lwip_outgoing_next_packet = bnep_lwip_outgoing_pop_packet();

    // queue emptied or channel closed since this was scheduled
    if (bnep_lwip_outgoing_next_packet == NULL) return;
    if (bnep_cid == 0) {
        bnep_lwip_outgoing_packet_processed();
        return;
    }

    // request can send now
    bnep_request_can_send_now_event(bnep_cid);
}

static void bnep_lwip_send_packet(void){
    if (bnep_lwip_outgoing_next_packet == NULL){
        log_error("CAN SEND NOW, but now packet queued");
        return;
    }

#if NO_SYS
    uint32_t len = bnep_lwip_outgoing_next_packet->tot_len;
    if (len > sizeof(btstack_network_outgoing_buffer)){
        log_error("BNEP outgoing frame too large: %u", (unsigned int) len);
        return;
    }
    pbuf_copy_partial(bnep_lwip_outgoing_next_packet, btstack_network_outgoing_buffer, len, 0);
    int status = bnep_send(bnep_cid, btstack_network_outgoing_buffer, len);
#else
    int status = bnep_send(bnep_cid, bnep_lwip_outgoing_next_packet->data,
                           bnep_lwip_outgoing_next_packet->len);
#endif
    if (status != 0){
        log_error("BNEP send failed: %d", status);
    }
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

    // Release every queued reference before discarding the queue contents.
#if NO_SYS
    struct pbuf * p;
    while ((p = bnep_lwip_outgoing_pop_packet()) != NULL){
        pbuf_free(p);
    }
#else
    bnep_lwip_frame_t * frame;
    xSemaphoreTake(netif_mutex, portMAX_DELAY);
    while ((frame = bnep_lwip_outgoing_pop_packet()) != NULL){
        frame->used = false;
    }
    xSemaphoreGive(netif_mutex);
#endif
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
/* LISTING_PAUSE */
    bd_addr_t local_addr;
    bd_addr_t remote_addr;
    uint16_t event_cid;
  
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
                case BNEP_EVENT_INCOMING_CONNECTION:
                    event_cid = bnep_event_incoming_connection_get_bnep_cid(packet);
                    if (bnep_cid || bnep_lwip_outgoing_connection_pending){
                        log_info("Declining additional BNEP connection, cid 0x%04x", event_cid);
                        bnep_decline_connection(event_cid);
                        return;
                    }
                    bnep_cid = event_cid;
                    if (bnep_accept_connection(event_cid) != ERROR_CODE_SUCCESS){
                        bnep_cid = 0;
                    }
                    break;

                case BNEP_EVENT_CHANNEL_OPENED:
                    event_cid = bnep_event_channel_opened_get_bnep_cid(packet);
                    bnep_event_channel_opened_get_remote_address(packet, remote_addr);
                    if (bnep_lwip_outgoing_connection_pending &&
                        (memcmp(remote_addr, bnep_lwip_outgoing_address, sizeof(bd_addr_t)) == 0)){
                        bnep_lwip_outgoing_connection_pending = false;
                    } else if (event_cid != bnep_cid){
                        return;
                    }
                    if (bnep_event_channel_opened_get_status(packet) != 0){
                        if (event_cid == bnep_cid) bnep_cid = 0;
                        break;
                    }

                    bnep_cid = event_cid;
                    // The event MTU is the maximum Ethernet payload. Leave room
                    // for the Ethernet header in the outgoing staging buffer.
                    uint16_t mtu = btstack_min(bnep_event_channel_opened_get_mtu(packet),
                                               HCI_ACL_PAYLOAD_SIZE - 14);

                    /* Setup network interface */
                    gap_local_bd_addr(local_addr);
#if NO_SYS
                    bnep_lwip_netif_up(local_addr, mtu);
#else
                    bnep_lwip_netif_update(true, local_addr, mtu);
#endif

#if NO_SYS
                    // start timer
                    btstack_run_loop_set_timer_handler(&bnep_lwip_timer, bnep_lwip_timeout_handler);
                    btstack_run_loop_set_timer(&bnep_lwip_timer, LWIP_TIMER_INTERVAL_MS);
                    btstack_run_loop_add_timer(&bnep_lwip_timer);
#endif
                    break;
                
                /* @text BNEP_EVENT_CHANNEL_CLOSED is received when the connection gets closed.
                 */
                case BNEP_EVENT_CHANNEL_TIMEOUT:
                    event_cid = bnep_event_channel_timeout_get_bnep_cid(packet);
                    if (event_cid == bnep_cid){
                        bnep_cid = 0;
                    } else {
                        bnep_event_channel_timeout_get_remote_address(packet, remote_addr);
                        if (!bnep_lwip_outgoing_connection_pending ||
                            memcmp(remote_addr, bnep_lwip_outgoing_address, sizeof(bd_addr_t)) != 0) return;
                        bnep_lwip_outgoing_connection_pending = false;
                    }
                    break;

                case BNEP_EVENT_CHANNEL_CLOSED:
                    if (bnep_event_channel_closed_get_bnep_cid(packet) != bnep_cid) return;
                    bnep_cid = 0;
#if !NO_SYS
                    xSemaphoreTake(netif_mutex, portMAX_DELAY);
                    bnep_lwip_tx_link_up = false;
                    xSemaphoreGive(netif_mutex);
#endif
                    bnep_lwip_discard_packets();

                    // Mark Link as Down
#if NO_SYS
                    bnep_lwip_netif_down();
                    btstack_run_loop_remove_timer(&bnep_lwip_timer);
#else
                    bnep_lwip_netif_update(false, NULL, 0);
#endif
                    break;

                /* @text BNEP_EVENT_CAN_SEND_NOW indicates that a new packet can be send. This triggers the send of a 
                 * stored network packet. The tap datas source can be enabled again
                 */
                case BNEP_EVENT_CAN_SEND_NOW:
                    if (bnep_event_can_send_now_get_bnep_cid(packet) != bnep_cid) return;
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
            if ((bnep_cid == 0) || (channel != bnep_cid)) return;
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
void bnep_lwip_init_with_callback(void (*callback)(uint8_t status)){
    bnep_lwip_init_callback = callback;
    if (bnep_lwip_outgoing_init_queue() != 0){
        if (callback != NULL) callback(BTSTACK_MEMORY_ALLOC_FAILED);
        return;
    }
#if NO_SYS
    bnep_lwip_initialized = bnep_lwip_netif_add();
    if (callback != NULL){
        callback(bnep_lwip_initialized ? ERROR_CODE_SUCCESS : BTSTACK_MEMORY_ALLOC_FAILED);
    }
#else
    netif_mutex = xSemaphoreCreateMutex();
    if (netif_mutex == NULL){
        vQueueDelete(bnep_lwip_outgoing_queue);
        if (callback != NULL) callback(BTSTACK_MEMORY_ALLOC_FAILED);
        return;
    }
    btstack_run_loop_set_timer_handler(&netif_retry_timer, &bnep_lwip_netif_retry);
    btstack_run_loop_set_timer_handler(&bnep_lwip_rx_free_timer, &bnep_lwip_rx_free_retry);
    bnep_lwip_outgoing_callback_registration.callback = &bnep_lwip_outgoing_callback;
    bnep_lwip_netif_schedule();
#endif
}

void bnep_lwip_init(void){
    bnep_lwip_init_with_callback(NULL);
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
    if (!bnep_lwip_initialized) return ERROR_CODE_COMMAND_DISALLOWED;
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
    if (!bnep_lwip_initialized || bnep_cid || bnep_lwip_outgoing_connection_pending) return ERROR_CODE_COMMAND_DISALLOWED;
    bnep_lwip_outgoing_connection_pending = true;
    bd_addr_copy(bnep_lwip_outgoing_address, addr);
    int status = bnep_connect(packet_handler, addr, l2cap_psm, uuid_src, uuid_dest);
    if (status != 0){
        bnep_lwip_outgoing_connection_pending = false;
        return ERROR_CODE_UNSPECIFIED_ERROR;
    } else {
        return ERROR_CODE_SUCCESS;
    }
}

struct netif *bnep_lwip_get_interface(void){
    return &btstack_netif;
}
