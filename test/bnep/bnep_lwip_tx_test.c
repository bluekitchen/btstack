#include <assert.h>
#include <stdio.h>
#include <string.h>
#define BNEP_LWIP_OUTGOING_QUEUE_SIZE 3
#include "../../platform/lwip/bnep_lwip.c"

static bnep_lwip_frame_t * frames[3];
static unsigned head, count, posts, sends, requests, refs, frees, rx_allocs;
static bool locked, fail_free;
static btstack_context_callback_registration_t * callback;
static btstack_timer_source_t * retry_timer;
static struct pbuf rx_pbuf;
static uint8_t rx_data[8];
static uint8_t sent[HCI_ACL_PAYLOAD_SIZE];
static uint16_t sent_len;
void hci_dump_log(int level, const char * format, ...){ (void) level; (void) format; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, uint32_t timeout){ (void) sem; (void) timeout; assert(!locked); locked = true; return pdPASS; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t sem){ (void) sem; assert(locked); locked = false; return pdPASS; }
BaseType_t xQueueSendToBack(QueueHandle_t q, const void * item, uint32_t timeout){
    (void) q; assert(locked && timeout == 0); if (count == 3) return 0;
    frames[(head + count) % 3] = *(bnep_lwip_frame_t * const *)item; count++; return pdPASS;
}
BaseType_t xQueueReceive(QueueHandle_t q, void * item, uint32_t timeout){
    (void) q; assert(timeout == 0); if (!count) return 0;
    *(bnep_lwip_frame_t **)item = frames[head]; head = (head + 1) % 3; count--; return pdPASS;
}
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t q){ (void) q; return count; }
void btstack_run_loop_execute_on_main_thread(btstack_context_callback_registration_t * r){ assert(!locked && !callback); callback = r; posts++; }
void bnep_request_can_send_now_event(uint16_t cid){ assert(cid == bnep_cid); requests++; }
int bnep_send(uint16_t cid, uint8_t * data, uint16_t len){ assert(cid == bnep_cid); memcpy(sent, data, len); sent_len = len; sends++; return 0; }
u16_t pbuf_copy_partial(const struct pbuf * p, void * dest, u16_t len, u16_t offset){ assert(offset == 0 && len == p->tot_len); memcpy(dest, p->payload, len); return len; }
void pbuf_ref(struct pbuf * p){ (void) p; refs++; }
u8_t pbuf_free(struct pbuf * p){ (void) p; frees++; return 1; }
err_t pbuf_free_callback(struct pbuf * p){ assert(p == &rx_pbuf); if (fail_free) return ERR_MEM; frees++; return ERR_OK; }
struct pbuf * pbuf_alloc(pbuf_layer layer, u16_t len, pbuf_type type){
    (void) layer; (void) type; assert(len <= sizeof(rx_data)); rx_allocs++; memset(&rx_pbuf, 0, sizeof(rx_pbuf)); rx_pbuf.len = rx_pbuf.tot_len = len; rx_pbuf.payload = rx_data; return &rx_pbuf;
}
void btstack_run_loop_set_timer(btstack_timer_source_t * t, uint32_t ms){ (void) t; (void) ms; }
void btstack_run_loop_add_timer(btstack_timer_source_t * t){ assert(!retry_timer); retry_timer = t; }
static err_t reject_input(struct pbuf * p, struct netif * netif){ (void) p; (void) netif; return ERR_MEM; }
static void run_callback(void){ assert(callback); btstack_context_callback_registration_t * r = callback; callback = NULL; r->callback(r->context); }
static void retry_free(void){ assert(retry_timer); btstack_timer_source_t * t = retry_timer; retry_timer = NULL; bnep_lwip_rx_free_retry(t); }
static void transmit_done(void){ bnep_lwip_send_packet(); bnep_lwip_packet_sent(); }
int main(void){
    uint8_t data[] = {1,2,3,4};
    struct pbuf p = {0}; p.payload = data; p.len = p.tot_len = sizeof(data);
    netif_mutex = (void *)1; bnep_lwip_outgoing_queue = (void *)2;
    bnep_lwip_outgoing_callback_registration.callback = bnep_lwip_outgoing_callback;
    assert(low_level_output(&btstack_netif, &p) == ERR_IF);
    bnep_cid = 0x40; bnep_lwip_tx_link_up = true;
    assert(low_level_output(&btstack_netif, &p) == ERR_OK);
    data[0] = 9;
    assert(low_level_output(&btstack_netif, &p) == ERR_OK);
    assert(low_level_output(&btstack_netif, &p) == ERR_OK);
    assert(low_level_output(&btstack_netif, &p) == ERR_MEM);
    assert(posts == 1 && refs == 0 && frees == 0);
    run_callback(); assert(requests == 1 && count == 2);
    transmit_done(); assert(sends == 1 && sent_len == sizeof(data) && sent[0] == 1 && requests == 2);
    // A wakeup can remain queued while direct processing drains the queue.
    assert(low_level_output(&btstack_netif, &p) == ERR_OK && callback);
    transmit_done(); assert(requests == 3);
    transmit_done(); assert(requests == 4);
    transmit_done(); assert(count == 0);
    assert(low_level_output(&btstack_netif, &p) == ERR_OK && posts == 2);
    run_callback(); assert(requests == 5);
    // Disconnect releases the in-flight frame and all queued frames.
    assert(low_level_output(&btstack_netif, &p) == ERR_OK);
    bnep_cid = 0; bnep_lwip_tx_link_up = false;
    bnep_lwip_discard_packets();
    assert(count == 0 && bnep_lwip_outgoing_next_packet == NULL);
    assert(low_level_output(&btstack_netif, &p) == ERR_IF);
    bnep_cid = 0x41; bnep_lwip_tx_link_up = true;
    assert(low_level_output(&btstack_netif, &p) == ERR_OK);
    run_callback(); assert(requests == 6);
    transmit_done(); assert(sends == 5 && count == 0);
    // Failed receive pbuf release is retained and retried without allocating another.
    btstack_netif.input = reject_input; fail_free = true;
    bnep_lwip_netif_process_packet(data, sizeof(data));
    assert(rx_allocs == 1 && bnep_lwip_pending_rx_free == &rx_pbuf && retry_timer);
    bnep_lwip_netif_process_packet(data, sizeof(data)); assert(rx_allocs == 1);
    retry_free(); assert(retry_timer && bnep_lwip_pending_rx_free == &rx_pbuf);
    fail_free = false; retry_free(); assert(!retry_timer && !bnep_lwip_pending_rx_free && frees == 1);
    puts("BNEP lwIP RTOS transmit and release tests passed");
    return 0;
}
