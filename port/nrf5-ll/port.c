#include <stdint.h>

#include "debug.h"
#include "soc.h"
#include "cpu.h"

#include "misc.h"
#include "util.h"
#include "rand.h"
#include "ccm.h"
#include "radio.h"
#include "pdu.h"
#include "ctrl.h"

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

static uint16_t hci_rx_pos;
static uint8_t  hci_rx_buffer[256];
static uint8_t  hci_rx_type;

static volatile uint32_t btstack_run_loop_rtc0_overflow_counter;
static btstack_linked_list_t timers;

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

static int transport_send_packet(uint8_t packet_type, uint8_t *packet, int size){
    btstack_buf_t buf;
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
            buf.data = packet,
            buf.len  = size;
            if (hci_rx_pos){
                log_error("transport_send_packet, but previous event not delivered size %u, type %u", hci_rx_pos, hci_rx_type);
                log_info_hexdump(hci_rx_buffer, hci_rx_pos);
            }
            hci_rx_pos = 0;
            hci_rx_type = 0;
            hci_driver_handle_cmd(&buf, hci_rx_buffer, &hci_rx_pos);
            hci_rx_type = HCI_EVENT_PACKET;
            break;
        case HCI_ACL_DATA_PACKET:
            btstack_hci_acl_handle(packet, size);
            break;
        default:
            log_error("transport_send_packet: invalid packet type: %02x", packet_type);
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

const hci_transport_t * hci_transport_zephyr_get_instance(void){
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

uint64_t btstack_run_loop_zephyr_get_ticks(void){
    uint32_t high_ticks_before, high_ticks_after, low_ticks;
    while (1){
        high_ticks_before = btstack_run_loop_rtc0_overflow_counter;
        low_ticks = NRF_RTC0->COUNTER;
        high_ticks_after  = btstack_run_loop_rtc0_overflow_counter;
        if (high_ticks_after == high_ticks_before) break;
    }
    return (high_ticks_after << 24) | low_ticks;
}

static uint32_t btstack_run_loop_zephyr_get_time_ms(void){
    return btstack_run_loop_zephyr_get_ticks() * 125 / 4096;  // == * 1000 / 32768
}

static uint32_t btstack_run_loop_zephyr_ticks_for_ms(uint32_t time_in_ms){
    return time_in_ms * 4096 / 125; // == * 32768 / 1000
}

static void btstack_run_loop_zephyr_set_timer(btstack_timer_source_t *ts, uint32_t timeout_in_ms){
    uint32_t ticks = btstack_run_loop_zephyr_ticks_for_ms(timeout_in_ms);
    uint64_t timeout = btstack_run_loop_zephyr_get_ticks() + ticks;
    // drop resolution to 32 ticks
    ts->timeout = timeout >> 5;
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

static void btstack_run_loop_zephyr_execute_once(void) {

    // deliver packet if ready
    if (hci_rx_pos){
        transport_deliver_packet();
    }

    // printf("Time %u, %lu, %u\n", btstack_run_loop_zephyr_get_ticks());
    // printf("Time %08u ms\n", btstack_run_loop_get_time_ms());

    // process ready
    while (1){
        // get next event from ll
        int done = hci_driver_task_step(&hci_rx_type, hci_rx_buffer, &hci_rx_pos);
        if (hci_rx_pos){
            transport_deliver_packet();
            continue;
        }
        if (done) break;
    }

#if 0
    uint64_t now = btstack_run_loop_zephyr_get_ticks() >> 5;

    // process timers
    while (timers) {
        btstack_timer_source_t *ts = (btstack_timer_source_t *) timers;
        if (ts->timeout > now) break;
        btstack_run_loop_remove_timer(ts);
        ts->process(ts);
    }

    // deliver packet if ready
    if (hci_rx_pos) return;
#endif

#if 0
    // disable IRQs and check if run loop iteration has been requested. if not, go to sleep
    hal_cpu_disable_irqs();
    if (trigger_event_received){
        trigger_event_received = 0;
        hal_cpu_enable_irqs();
    } else {
        hal_cpu_enable_irqs_and_sleep();
    }
#endif
}

static void btstack_run_loop_zephyr_execute(void) {
    while (1) {
        btstack_run_loop_zephyr_execute_once();

        /* goto sleep */
        DEBUG_CPU_SLEEP(1);
        cpu_sleep();
        DEBUG_CPU_SLEEP(0);
    }
}

static void btstack_run_loop_zephyr_btstack_run_loop_init(void){
    timers = NULL;
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
