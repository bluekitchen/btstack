#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../example/pan_lwip_http_server.c"

static tcpip_callback_fn pending_core;
static btstack_context_callback_registration_t * pending_btstack;
static btstack_timer_source_t * pending_timer;
static int context, dhcp_count, http_count, pan_count, power_count;
static bool fail_post, fail_dhcp;

void hci_dump_log(int level, const char * format, ...){ (void) level; (void) format; }
void gap_set_local_name(const char * name){ (void) name; assert(context == 2); }
void gap_discoverable_control(uint8_t enable){ (void) enable; }
void gap_set_class_of_device(uint32_t cod){ (void) cod; }
int gap_pin_code_negative(bd_addr_t address){ (void) address; return 0; }
int gap_ssp_confirmation_response(const bd_addr_t address){ (void) address; return 0; }
void hci_add_event_handler(btstack_packet_callback_registration_t * cb){ (void) cb; }
int hci_power_control(HCI_POWER_MODE mode){ assert(context == 2 && mode == HCI_POWER_ON); power_count++; return 0; }
void l2cap_init(void){}
void sm_init(void){}
void bnep_init(void){ assert(context == 2); pan_count++; }
void sdp_init(void){}
uint32_t sdp_create_service_record_handle(void){ return 1; }
uint8_t sdp_register_service(const uint8_t * record){ (void) record; return 0; }
uint32_t de_get_len(const uint8_t * record){ (void) record; return 0; }
void pan_create_nap_sdp_record(uint8_t * record, uint32_t handle, uint16_t * types, const char * name, const char * description, security_description_t security, net_access_type_t access, uint32_t rate, const char * ipv4, const char * ipv6){
    (void) record; (void) handle; (void) types; (void) name; (void) description; (void) security; (void) access; (void) rate; (void) ipv4; (void) ipv6;
}
void bnep_lwip_init_with_callback(void (*callback)(uint8_t)){ (void) callback; }
void bnep_lwip_register_packet_handler(btstack_packet_handler_t cb){ (void) cb; }
uint8_t bnep_lwip_register_service(uint16_t uuid, uint16_t size){ (void) uuid; (void) size; return 0; }
err_t dhserv_init(dhcp_config_t * c){ (void) c; assert(context == 1); dhcp_count++; return fail_dhcp ? ERR_MEM : ERR_OK; }
void httpd_init(void){ assert(context == 1 && dhcp_count == 1); http_count++; }
void btstack_run_loop_execute_on_main_thread(btstack_context_callback_registration_t * r){ assert(context == 1); assert(!pending_btstack); pending_btstack = r; }
void tcpip_init(tcpip_init_done_fn fn, void * arg){ assert(context == 0 && arg == NULL); pending_core = fn; }
err_t tcpip_try_callback(tcpip_callback_fn fn, void * arg){ (void) arg; if (fail_post) return ERR_MEM; assert(!pending_core); pending_core = fn; return ERR_OK; }
void btstack_run_loop_set_timer_handler(btstack_timer_source_t * t, void (*fn)(btstack_timer_source_t *)){ t->process = fn; }
void btstack_run_loop_set_timer(btstack_timer_source_t * t, uint32_t ms){ (void) t; (void) ms; }
void btstack_run_loop_add_timer(btstack_timer_source_t * t){ assert(!pending_timer); pending_timer = t; }
static void run_core(void){ assert(pending_core); tcpip_callback_fn fn = pending_core; pending_core = NULL; context = 1; fn(NULL); context = 0; }
static void run_btstack(void){ assert(pending_btstack); btstack_context_callback_registration_t * reg = pending_btstack; pending_btstack = NULL; context = 2; reg->callback(reg->context); context = 0; }
int main(int argc, char ** argv){
    (void) argv; fail_dhcp = argc > 1;
#ifdef HAVE_LWIP
    fail_post = true;
#endif
    network_setup();
    assert(!dhcp_count && !http_count && !pan_count && !power_count);
#ifdef HAVE_LWIP
    assert(pending_timer && !pending_core);
    fail_post = false;
    btstack_timer_source_t * timer = pending_timer; pending_timer = NULL; timer->process(timer);
#endif
    run_core();
    assert(dhcp_count == 1 && http_count == !fail_dhcp && !pan_count && !power_count);
    run_btstack();
    assert(pan_count == !fail_dhcp && power_count == !fail_dhcp);
    puts("PAN lwIP startup test passed");
    return 0;
}
