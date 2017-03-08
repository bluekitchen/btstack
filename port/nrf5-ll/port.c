#include <stdint.h>

#include "cpu.h"
#include "ctrl.h"
#include "debug.h"
#include "pdu.h"
#include "soc.h"
#include "ticker.h"
#include "util.h"

#include "bluetooth.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "hci_transport.h"
#include "zephyr_diet.h"

int btstack_hci_acl_handle(uint8_t * packet_buffer, uint16_t packet_len);
int btstack_hci_cmd_handle(btstack_buf_t *cmd, btstack_buf_t *evt);

static void (*transport_packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

// largest Classic HCI Command: read locale name
// LE Advertisment Report with single item: 43 bytes
// Read Local Supported Commands -> Command Complete with 68 bytes payload
// LE Read Local P-256 Public Key: 66 bytes payload

static uint16_t hci_rx_pos;
static uint8_t  hci_rx_buffer[HCI_EVENT_HEADER_SIZE + 68];
static uint8_t  hci_rx_type;

static uint16_t   hci_tx_size;
static uint8_t  * hci_tx_data;
static uint8_t    hci_tx_type;

static volatile uint32_t btstack_run_loop_rtc0_overflow_counter;
static btstack_linked_list_t timers;
static volatile int trigger_event_received = 0;

/**
 * init transport
 * @param transport_config
 */
static void transport_init(const void *transport_config){
    UNUSED(transport_config);
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

int hci_driver_handle_cmd(btstack_buf_t * buf, uint8_t * event_buffer, uint16_t * event_size)
{
	btstack_buf_t evt;
	evt.data = event_buffer;
	evt.len = 0;
	int err = btstack_hci_cmd_handle(buf, &evt);
	*event_size = evt.len;
	return err;
}

static int transport_can_send_packet_now(uint8_t packet_type){
    UNUSED(packet_type);
    return hci_tx_type == 0;
}

static int transport_send_packet(uint8_t packet_type, uint8_t *packet, int size){
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
        case HCI_ACL_DATA_PACKET:
            hci_tx_type = packet_type;
            hci_tx_data = packet;
            hci_tx_size = size;
            return 0;
        default:
             log_error("transport_send_packet: invalid packet type: %02x", packet_type);
            break;
   }
   return 0;
}

static void transport_process_packet(void){
    btstack_buf_t buf;
    switch (hci_tx_type){
        case HCI_COMMAND_DATA_PACKET:
            hci_rx_pos  = 0;
            hci_rx_type = 0;
            buf.data = hci_tx_data,
            buf.len  = hci_tx_size;
            hci_driver_handle_cmd(&buf, hci_rx_buffer, &hci_rx_pos);
            hci_rx_type = HCI_EVENT_PACKET;
            break;
        case HCI_ACL_DATA_PACKET:
            btstack_hci_acl_handle(hci_tx_data, hci_tx_size);
            break;
        default:
            break;
    }
    hci_tx_type = 0;
}

static const hci_transport_t transport = {
    /* const char * name; */                                        "nRF5-Phoenix",
    /* void   (*init) (const void *transport_config); */            &transport_init,
    /* int    (*open)(void); */                                     &transport_open,
    /* int    (*close)(void); */                                    &transport_close,
    /* void   (*register_packet_handler)(void (*handler)(...); */   &transport_register_packet_handler,
    /* int    (*can_send_packet_now)(uint8_t packet_type); */       &transport_can_send_packet_now,
    /* int    (*send_packet)(...); */                               &transport_send_packet,
    /* int    (*set_baudrate)(uint32_t baudrate); */                NULL,
    /* void   (*reset_link)(void); */                               NULL,
};

const hci_transport_t * hci_transport_phoenix_get_instance(void){
	return &transport;
}

void btstack_hci_evt_encode(struct radio_pdu_node_rx *node_rx, btstack_buf_t *buf);
void hci_acl_encode_btstack(struct radio_pdu_node_rx *node_rx, uint8_t * packet_buffer, uint16_t * packet_size);
void btstack_hci_num_cmplt_encode(btstack_buf_t *buf, uint16_t handle, uint8_t num);

static void hci_driver_process_radio_data(struct radio_pdu_node_rx *node_rx, uint8_t * packet_type, uint8_t * packet_buffer, uint16_t * packet_size){

    struct pdu_data * pdu_data;

    pdu_data = (void *)node_rx->pdu_data;
    /* Check if we need to generate an HCI event or ACL
     * data
     */
    if (node_rx->hdr.type != NODE_RX_TYPE_DC_PDU ||
        pdu_data->ll_id == PDU_DATA_LLID_CTRL) {

        /* generate a (non-priority) HCI event */
        btstack_buf_t buf;
        buf.data = packet_buffer;
        buf.len = 0;
        btstack_hci_evt_encode(node_rx, &buf);
        *packet_size = buf.len;
        *packet_type = HCI_EVENT_PACKET;

    } else {
        /* generate ACL data */
        hci_acl_encode_btstack(node_rx, packet_buffer, packet_size);
        *packet_type = HCI_ACL_DATA_PACKET;
    }

    radio_rx_fc_set(node_rx->hdr.handle, 0);
    node_rx->hdr.onion.next = 0;
    radio_rx_mem_release(&node_rx);
}

// returns 1 if done
int hci_driver_task_step(uint8_t * packet_type, uint8_t * packet_buffer, uint16_t * packet_size){
    uint16_t handle;
    struct radio_pdu_node_rx *node_rx;

    // if num_completed != 0 => node_rx == null
    uint8_t num_completed = radio_rx_get(&node_rx, &handle);

    if (num_completed){
        // emit num completed event
        btstack_buf_t buf;
        buf.data = packet_buffer;
        buf.len = 0;
        btstack_hci_num_cmplt_encode(&buf, handle, num_completed);
        *packet_size = buf.len;
        *packet_type = HCI_EVENT_PACKET;
        return 0;
    }

    if (node_rx) {
        radio_rx_dequeue();
        hci_driver_process_radio_data(node_rx, packet_type, packet_buffer, packet_size);
        return 0;
    }

    return 1;
}

static void transport_deliver_packet(void){
    uint16_t size = hci_rx_pos;
    uint8_t  type = hci_rx_type;
    hci_rx_pos  = 0;
    hci_rx_type = 0;
    if (!transport_packet_handler) return;
    transport_packet_handler(type, hci_rx_buffer, size);
}

static void hal_cpu_disable_irqs(void){
    __disable_irq();
}

static void hal_cpu_enable_irqs(void){
    __enable_irq();
}

static void hal_cpu_enable_irqs_and_sleep(void){
    __enable_irq();
    DEBUG_CPU_SLEEP(1);
    cpu_sleep();
    DEBUG_CPU_SLEEP(0);
}

void btstack_run_loop_rtc0_overflow(void){
    btstack_run_loop_rtc0_overflow_counter++;
}

// drop resolution to 32 ticks ~ 1 ms
#define PRESCALER_TICKS 5

static uint64_t btstack_run_loop_phoenix_get_ticks(void){
    uint32_t high_ticks_before, high_ticks_after, low_ticks;
    while (1){
        high_ticks_before = btstack_run_loop_rtc0_overflow_counter;
        low_ticks = NRF_RTC0->COUNTER;
        high_ticks_after  = btstack_run_loop_rtc0_overflow_counter;
        if (high_ticks_after == high_ticks_before) break;
    }
    return (high_ticks_after << 24) | low_ticks;
}

static uint64_t btstack_run_loop_phoenix_get_ticks_prescaled(void){
    return btstack_run_loop_phoenix_get_ticks() >> PRESCALER_TICKS;
}

static uint32_t btstack_run_loop_phoenix_get_time_ms(void){
    return btstack_run_loop_phoenix_get_ticks() * 125 / 4096;  // == * 1000 / 32768
}

static uint32_t btstack_run_loop_phoenix_ticks_for_ms(uint32_t time_in_ms){
    return time_in_ms * 4096 / 125; // == * 32768 / 1000
}

static void btstack_run_loop_phoenix_set_timer(btstack_timer_source_t *ts, uint32_t timeout_in_ms){
    uint32_t ticks = btstack_run_loop_phoenix_ticks_for_ms(timeout_in_ms);
    uint64_t timeout = btstack_run_loop_phoenix_get_ticks() + ticks;
    // drop resolution to 32 ticks ~ 1 ms
    ts->timeout = timeout >> PRESCALER_TICKS;
}

static uint64_t btstack_run_loop_reconstruct_full_ticks(uint64_t reference_full, uint32_t ticks_low){
    
    uint32_t reference_low  = reference_full & 0x00000000ffffffff;
    uint32_t reference_high = reference_full & 0xffffffff00000000L;

    int32_t delta = ticks_low - reference_low;

    if (delta >= 0){
        if (ticks_low >= reference_low) {
            return reference_high | ticks_low;
        } else {
            return reference_high + 0x100000000L + ticks_low;
        }
    } else {
        if (ticks_low < reference_low) {
            return reference_high | ticks_low;
        } else {
            return reference_high - 0x100000000L + ticks_low;
        }            
    }
}

/**
 * Add timer to run_loop (keep list sorted)
 */
static void btstack_run_loop_phoenix_add_timer(btstack_timer_source_t *ts){

    uint64_t now = btstack_run_loop_phoenix_get_ticks_prescaled();
    uint64_t new_timeout = btstack_run_loop_reconstruct_full_ticks(now, ts->timeout);

    // log_info("btstack_run_loop_phoenix_add_timer: timeout %u", (int) ts->timeout);

    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) &timers; it->next ; it = it->next){
        // don't add timer that's already in there
        if ((btstack_timer_source_t *) it->next == ts){
            log_error( "btstack_run_loop_timer_add error: timer to add already in list!");
            return;
        }
        uint32_t next_timeout_low = ((btstack_timer_source_t *) it->next)->timeout;
        uint64_t next_timeout = btstack_run_loop_reconstruct_full_ticks(now, next_timeout_low);

        if (new_timeout < next_timeout) {
            break;
        }
    }
    ts->item.next = it->next;
    it->next = (btstack_linked_item_t *) ts;
}

/**
 * Remove timer from run loop
 */
static int btstack_run_loop_phoenix_remove_timer(btstack_timer_source_t *ts){
    // log_info("btstack_run_loop_phoenix_remove_timer: timeout %u", (int) ts->timeout);
    return btstack_linked_list_remove(&timers, (btstack_linked_item_t *) ts);
}

static void btstack_run_loop_phoenix_dump_timer(void){
#ifdef ENABLE_LOG_INFO 
    btstack_linked_item_t *it;
    int i = 0;
    for (it = (btstack_linked_item_t *) timers; it ; it = it->next){
        btstack_timer_source_t *ts = (btstack_timer_source_t*) it;
        log_info("timer %u, timeout %u\n", i, (unsigned int) ts->timeout);
    }
#endif
}

static uint64_t btstack_run_loop_phoenix_singleshot_timeout = 0;

static void btstack_run_loop_phoenix_singleshot_timeout_handler(uint32_t ticks_at_expire, uint32_t remainder, uint16_t lazy, void *context){
    (void)ticks_at_expire;
    (void)remainder;
    (void)lazy;
    (void)context;

    // just notify run loop
    trigger_event_received = 1;

    // single shot timer is not active anymore
    btstack_run_loop_phoenix_singleshot_timeout = 0;
}

static void btstack_run_loop_phoenix_start_singleshot_timer(uint32_t timeout_ticks){

    // limit ticks to ticker resolution - creatively use tick diff function to not specifiy resolution here
    uint32_t ticker_ticks = ticker_ticks_diff_get(timeout_ticks, 0);

    // log_info("btstack_run_loop_phoenix_start_singleshot_timer: %u, current %u", (int) timeout_ticks, (int) cntr_cnt_get());
    ticker_start(0 /* instance */
        , BTSTACK_USER_ID /* user */
        , BTSTACK_TICKER_ID /* ticker id */
        , ticker_ticks /* anchor point */
        , 0 /* first interval */
        , 0 /* periodic interval */
        , 0 /* remainder */
        , 0 /* lazy */
        , 0 /* slot */
        , btstack_run_loop_phoenix_singleshot_timeout_handler /* timeout callback function */
        , 0 /* context */
        , 0 /* op func */
        , 0 /* op context */
        );
    btstack_run_loop_phoenix_singleshot_timeout = timeout_ticks;
}

static void btstack_run_loop_phoenix_stop_singleshot_timer(void){
    // log_info("btstack_run_loop_phoenix_stop_singleshot_timer");
    ticker_stop(0 /* instance */
        , BTSTACK_USER_ID /* user */
        , BTSTACK_TICKER_ID /* ticker id */
        , 0 /* op func */
        , 0 /* op context */
        );
    btstack_run_loop_phoenix_singleshot_timeout = 0;
}

static void btstack_run_loop_phoenix_execute_once(void) {

    // printf("Time %u\n", (int) btstack_run_loop_phoenix_get_ticks() >> 5);

    // process queued radio packets
    while (1){
        // get next event from ll
        int done = hci_driver_task_step(&hci_rx_type, hci_rx_buffer, &hci_rx_pos);
        if (hci_rx_pos){
            transport_deliver_packet();
        }
        if (done) break;
    }

    // process queued HCI packets
    while (hci_tx_type){
        transport_process_packet();
        // deliver result to HCI Command
        if (hci_rx_pos){
            transport_deliver_packet();
        }
        // notify upper stack that provided buffer can be used again
        uint8_t event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};
        transport_packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));
    }

    // process timers
    uint64_t timeout = 0;
    while (timers) {
        uint64_t now = btstack_run_loop_phoenix_get_ticks_prescaled();
        btstack_timer_source_t *ts = (btstack_timer_source_t *) timers;
        timeout = btstack_run_loop_reconstruct_full_ticks(now, ts->timeout);
        // log_info("btstack_run_loop_phoenix_run: now %u, timeout %u - counter %u", (int) now, (int) ts->timeout, NRF_RTC0->COUNTER >> PRESCALER_TICKS);

        if (timeout > now) break;
        timeout = 0;

        btstack_run_loop_remove_timer(ts);
        ts->process(ts);
    }

    // use ticker to wake up if timer is set
    uint32_t timeout_ticks = (timeout << PRESCALER_TICKS);
    if (timeout_ticks != btstack_run_loop_phoenix_singleshot_timeout){
        if (btstack_run_loop_phoenix_singleshot_timeout){
            btstack_run_loop_phoenix_stop_singleshot_timer();
        }  
        if (timeout_ticks){
            btstack_run_loop_phoenix_start_singleshot_timer(timeout_ticks);
        }
    }

    // disable IRQs and check if run loop iteration has been requested. if not, go to sleep
    hal_cpu_disable_irqs();
    
    if (trigger_event_received){
        trigger_event_received = 0;
        hal_cpu_enable_irqs();
    } else {
        hal_cpu_enable_irqs_and_sleep();
    }
}

static void btstack_run_loop_phoenix_execute(void) {
    while (1) {
        btstack_run_loop_phoenix_execute_once();
    }
}

static void btstack_run_loop_phoenix_btstack_run_loop_init(void){
    timers = NULL;
}

static const btstack_run_loop_t btstack_run_loop_phoenix = {
    &btstack_run_loop_phoenix_btstack_run_loop_init,
    NULL,
    NULL,
    NULL,
    NULL,
    &btstack_run_loop_phoenix_set_timer,
    &btstack_run_loop_phoenix_add_timer,
    &btstack_run_loop_phoenix_remove_timer,
    &btstack_run_loop_phoenix_execute,
    &btstack_run_loop_phoenix_dump_timer,
    &btstack_run_loop_phoenix_get_time_ms,
};

/**
 * @brief Provide btstack_run_loop_posix instance for use with btstack_run_loop_init
 */
const btstack_run_loop_t * btstack_run_loop_phoenix_get_instance(void){
    return &btstack_run_loop_phoenix;
}

// copy from util/util.c, but returns number characters printed and uses <stdarg.h> to access arguments

#include "util.h"
#include <stdarg.h>

/* Format a string and print it on the screen, just like the libc
 *         function printf.
 */
int sprintf(char *str, const char *format, ...)
{
    int len = 0;

    va_list arg;
    va_start(arg, format);

    int c;
    char buf[20];

    while ((c = *format++)) {
        if (c != '%') {
            *str++ = c;
            len++;
        }
        else {
            char *p;

            c = *format++;
            switch (c) {
            case 'd':
            case 'u':
            case 'x':
                util_itoa(buf, c, va_arg(arg, int));
                p = buf;
                goto string;
            case 's':
                p = va_arg(arg, char *);
                if (!p)
                    p = "(null)";
string:
                while (*p){
                    *str++ = *p++;
                    len++;
                }
                break;
            default:
                // looks like %c to me
                *str++ = va_arg(arg, int);
                len++;
                break;
            }
        }
    }

    *str = 0;

    va_end(arg);
    return len;
}
