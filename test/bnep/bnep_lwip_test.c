#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../../platform/lwip/bnep_lwip.c"

void hci_dump_log(int level, const char * format, ...){ (void) level; (void) format; }
void bnep_request_can_send_now_event(uint16_t cid){ (void) cid; }
int bnep_send(uint16_t cid, uint8_t * packet, uint16_t len){ (void) cid; (void) packet; (void) len; return 0; }
void btstack_run_loop_add_timer(btstack_timer_source_t * timer){ (void) timer; }
int btstack_run_loop_remove_timer(btstack_timer_source_t * timer){ (void) timer; return 1; }
void btstack_run_loop_set_timer(btstack_timer_source_t * timer, uint32_t ms){ (void) timer; (void) ms; }
void btstack_run_loop_set_timer_handler(btstack_timer_source_t * timer, void (*handler)(btstack_timer_source_t *)){ (void) timer; (void) handler; }
void gap_local_bd_addr(bd_addr_t addr){ memset(addr, 0, sizeof(bd_addr_t)); }
void netif_set_up(struct netif * netif){ netif->flags |= NETIF_FLAG_UP; }
void netif_set_down(struct netif * netif){ netif->flags &= (u8_t) ~NETIF_FLAG_UP; }
struct pbuf * pbuf_alloc(pbuf_layer layer, u16_t len, pbuf_type type){ (void) layer; (void) len; (void) type; return NULL; }
u8_t pbuf_free(struct pbuf * p){ (void) p; return 1; }
u16_t pbuf_copy_partial(const struct pbuf * p, void * dst, u16_t len, u16_t offset){ (void) p; (void) dst; (void) offset; return len; }
void sys_check_timeouts(void){}

static unsigned accepted, declined, forwarded;
static uint16_t accepted_cid, declined_cid;
int bnep_connect(btstack_packet_handler_t handler, bd_addr_t addr, uint16_t psm, uint16_t src, uint16_t dest){
    (void) handler; (void) addr; (void) psm; (void) src; (void) dest; return 0;
}
uint8_t bnep_accept_connection(uint16_t cid){ accepted++; accepted_cid = cid; return 0; }
uint8_t bnep_decline_connection(uint16_t cid){ declined++; declined_cid = cid; return 0; }
static void client(uint8_t type, uint16_t cid, uint8_t * packet, uint16_t size){
    (void) type; (void) cid; (void) packet; (void) size; forwarded++;
}
static void incoming(uint16_t cid){
    uint8_t event[16] = { BNEP_EVENT_INCOMING_CONNECTION, 14 };
    little_endian_store_16(event, 2, cid);
    packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}
static void closed(uint16_t cid){
    uint8_t event[14] = { BNEP_EVENT_CHANNEL_CLOSED, 12 };
    little_endian_store_16(event, 2, cid);
    packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}
static void opened(uint16_t cid, uint8_t status, const bd_addr_t address){
    uint8_t event[20] = { BNEP_EVENT_CHANNEL_OPENED, 18 };
    event[2] = status;
    little_endian_store_16(event, 3, cid);
    little_endian_store_16(event, 9, 1500);
    reverse_bd_addr(address, &event[11]);
    packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}
static void timeout(uint16_t cid, const bd_addr_t address){
    uint8_t event[15] = { BNEP_EVENT_CHANNEL_TIMEOUT, 13 };
    little_endian_store_16(event, 2, cid);
    reverse_bd_addr(address, &event[8]);
    packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}
int main(void){
    bnep_lwip_outgoing_init_queue();
    client_handler = client;
    incoming(0x40);
    assert(accepted == 1 && accepted_cid == 0x40 && bnep_cid == 0x40);
    incoming(0x41);
    assert(declined == 1 && declined_cid == 0x41 && bnep_cid == 0x40);
    assert(forwarded == 1);
    closed(0x41);
    assert(bnep_cid == 0x40 && forwarded == 1);
    closed(0x40);
    assert(bnep_cid == 0 && forwarded == 2);
    incoming(0x41);
    assert(accepted == 2 && accepted_cid == 0x41);
    bd_addr_t address = { 1, 2, 3, 4, 5, 6 };
    opened(0x41, 0, address);
    assert(bnep_cid == 0x41 && (btstack_netif.flags & NETIF_FLAG_LINK_UP));
    incoming(0x42);
    assert(declined == 2 && declined_cid == 0x42);
    closed(0x42);
    assert(bnep_cid == 0x41 && (btstack_netif.flags & NETIF_FLAG_LINK_UP));
    closed(0x41);
    assert(bnep_cid == 0 && !(btstack_netif.flags & NETIF_FLAG_LINK_UP));
    incoming(0x43);
    timeout(0x43, address);
    assert(bnep_cid == 0);
    assert(bnep_lwip_connect(address, 15, 0, 0) == 0);
    incoming(0x44);
    assert(declined == 3 && declined_cid == 0x44);
    assert(bnep_lwip_connect(address, 15, 0, 0) == ERROR_CODE_COMMAND_DISALLOWED);
    timeout(0x45, address);
    assert(!bnep_lwip_outgoing_connection_pending);
    incoming(0x44);
    assert(accepted == 4 && bnep_cid == 0x44);
    puts("BNEP lwIP admission tests passed");
    return 0;
}
