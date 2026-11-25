#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../platform/lwip/bnep_lwip.c"

static void (*core_callback)(void *);
static btstack_context_callback_registration_t * main_callback;
static btstack_timer_source_t * retry;
static bool fail_post;
static bool locked;
static int core_context, netif_adds, up_count, down_count, completion_count;
static uint8_t completion_status;
static uint16_t last_mtu;
static bool change_during_up;
static bool fail_netif_add;
static bd_addr_t last_addr;

void bnep_request_can_send_now_event(uint16_t cid){ (void) cid; }
err_t etharp_output(struct netif * n, struct pbuf * p, const ip4_addr_t * addr){ (void) n; (void) p; (void) addr; return ERR_OK; }
u16_t pbuf_copy_partial(const struct pbuf * p, void * dst, u16_t len, u16_t offset){ (void) p; (void) dst; (void) offset; return len; }
void pbuf_ref(struct pbuf * p){ (void) p; }
u8_t pbuf_free(struct pbuf * p){ (void) p; return 1; }
err_t pbuf_free_callback(struct pbuf * p){ (void) p; return ERR_OK; }
void hci_dump_log(int level, const char * format, ...){ (void) level; (void) format; }
QueueHandle_t xQueueCreate(UBaseType_t n, UBaseType_t size){ (void) n; (void) size; return (void *) 1; }
BaseType_t xQueueSendToBack(QueueHandle_t q, const void * p, uint32_t t){ (void) q; (void) p; (void) t; return pdPASS; }
BaseType_t xQueueReceive(QueueHandle_t q, void * p, uint32_t t){ (void) q; (void) p; (void) t; return 0; }
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t q){ (void) q; return 0; }
void vQueueDelete(QueueHandle_t q){ (void) q; }
SemaphoreHandle_t xSemaphoreCreateMutex(void){ return (void *) 2; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, uint32_t t){ (void) s; (void) t; assert(!locked); locked = true; return pdPASS; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t s){ (void) s; assert(locked); locked = false; return pdPASS; }
err_t tcpip_try_callback(tcpip_callback_fn fn, void * ctx){
    assert(locked); assert(ctx == NULL);
    if (fail_post) return ERR_MEM;
    assert(core_callback == NULL); core_callback = fn; return ERR_OK;
}
void btstack_run_loop_execute_on_main_thread(btstack_context_callback_registration_t * reg){ assert(core_context); main_callback = reg; }
void btstack_run_loop_set_timer_handler(btstack_timer_source_t * timer, void (*handler)(btstack_timer_source_t *)){ timer->process = handler; }
void btstack_run_loop_set_timer(btstack_timer_source_t * timer, uint32_t ms){ (void) timer; (void) ms; }
void btstack_run_loop_add_timer(btstack_timer_source_t * timer){ assert(retry == NULL); retry = timer; }
int btstack_run_loop_remove_timer(btstack_timer_source_t * timer){ if (retry != timer) return 0; retry = NULL; return 1; }
struct netif * netif_add(struct netif * netif, const ip4_addr_t * ip, const ip4_addr_t * mask, const ip4_addr_t * gw, void * state, netif_init_fn init, netif_input_fn input){
    (void) ip; (void) mask; (void) gw; (void) state; (void) init; (void) input; assert(core_context); netif_adds++; return fail_netif_add ? NULL : netif;
}
void netif_set_default(struct netif * netif){ (void) netif; assert(core_context); }
void netif_set_link_up(struct netif * netif){ assert(core_context); netif->flags |= NETIF_FLAG_LINK_UP; }
void netif_set_link_down(struct netif * netif){ assert(core_context); netif->flags &= (u8_t) ~NETIF_FLAG_LINK_UP; }
void netif_set_up(struct netif * netif){
    assert(core_context); up_count++; last_mtu = netif->mtu; memcpy(last_addr, netif->hwaddr, 6);
    if (change_during_up){
        change_during_up = false;
        bnep_lwip_netif_update(false, NULL, 0);
    }
}
void netif_set_down(struct netif * netif){ (void) netif; assert(core_context); down_count++; }
err_t tcpip_input(struct pbuf * p, struct netif * n){ (void) p; (void) n; return ERR_OK; }
static void completed(uint8_t status){ assert(!core_context); completion_count++; completion_status = status; }
static void run_core(void){ assert(core_callback); void (*fn)(void *) = core_callback; core_callback = NULL; core_context = 1; fn(NULL); core_context = 0; }
static void run_main(void){ assert(main_callback); btstack_context_callback_registration_t * r = main_callback; main_callback = NULL; r->callback(r->context); }
int main(int argc, char ** argv){
    (void) argv;
    if (argc > 1){
        fail_netif_add = true;
        bnep_lwip_init_with_callback(completed);
        run_core();
        run_main();
        assert(completion_count == 1 && completion_status == BTSTACK_MEMORY_ALLOC_FAILED && !bnep_lwip_initialized);
        puts("BNEP lwIP RTOS initialization failure test passed");
        return 0;
    }
    bd_addr_t a = {1,2,3,4,5,6}, b = {6,5,4,3,2,1};
    fail_post = true;
    bnep_lwip_init_with_callback(completed);
    assert(retry && !completion_count && !bnep_lwip_initialized);
    fail_post = false;
    btstack_timer_source_t * timer = retry; retry = NULL; timer->process(timer);
    run_core();
    assert(netif_adds == 1 && !bnep_lwip_initialized);
    run_main();
    assert(completion_count == 1 && completion_status == 0 && bnep_lwip_initialized);
    bnep_lwip_netif_update(true, a, 1500);
    run_core();
    assert(up_count == 1 && down_count == 0 && last_mtu == 1500 && memcmp(last_addr, a, 6) == 0);
    bnep_lwip_netif_update(false, NULL, 0);
    bnep_lwip_netif_update(true, b, 1400);
    run_core();
    assert(down_count == 1 && up_count == 2 && last_mtu == 1400 && memcmp(last_addr, b, 6) == 0);
    fail_post = true;
    bnep_lwip_netif_update(false, NULL, 0);
    assert(retry && down_count == 1);
    bnep_lwip_netif_update(false, NULL, 0);
    assert(retry && down_count == 1);
    fail_post = false;
    timer = retry; retry = NULL; timer->process(timer);
    run_core();
    assert(down_count == 2 && !netif_callback_pending);
    change_during_up = true;
    bnep_lwip_netif_update(true, a, 1500);
    run_core();
    assert(up_count == 3 && down_count == 3 && !netif_callback_pending);
    puts("BNEP lwIP RTOS netif tests passed");
    return 0;
}
