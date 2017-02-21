/*
Copyright (c) 2016, Vinayak Kariappa Chettimada
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// orig includes
#include "soc.h"
#include "cpu.h"
#include "irq.h"
#include "uart.h"

#include "misc.h"
#include "util.h"
#include "mayfly.h"

#include "clock.h"
#include "cntr.h"
#include "ticker.h"

#include "rand.h"
#include "ccm.h"
#include "radio.h"
#include "pdu.h"
#include "ctrl.h"
#include "ll.h"

#include "config.h"

#include "debug.h"

// btstack includes
#include "bluetooth.h"
#include "btstack_config.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "hci_dump.h"
#include "hci_transport.h"
#include "btstack_debug.h"

#include <stdio.h>


int btstack_main(void);

const btstack_run_loop_t * btstack_run_loop_zephyr_get_instance(void);
const hci_transport_t * hci_transport_zephyr_get_instance();
void btstack_run_loop_rtc0_overflow();

uint8_t __noinit isr_stack[512];
uint8_t __noinit main_stack[1024];

void * const isr_stack_top = isr_stack + sizeof(isr_stack);
void * const main_stack_top = main_stack + sizeof(main_stack);

static uint8_t ALIGNED(4) ticker_nodes[RADIO_TICKER_NODES + BTSTACK_TICKER_NODES][TICKER_NODE_T_SIZE];
static uint8_t ALIGNED(4) ticker_users[MAYFLY_CALLER_COUNT][TICKER_USER_T_SIZE];
static uint8_t ALIGNED(4) ticker_user_ops[RADIO_TICKER_USER_OPS + BTSTACK_USER_OPS] [TICKER_USER_OP_T_SIZE];

static uint8_t ALIGNED(4) rng[3 + 4 + 1];

static uint8_t ALIGNED(4) radio[RADIO_MEM_MNG_SIZE];


void uart0_handler(void)
{
	isr_uart0(0);
}

void power_clock_handler(void)
{
	isr_power_clock(0);
}

void rtc0_handler(void)
{
	if (NRF_RTC0->EVENTS_COMPARE[0]) {
		NRF_RTC0->EVENTS_COMPARE[0] = 0;

		ticker_trigger(0);
	}

	if (NRF_RTC0->EVENTS_COMPARE[1]) {
		NRF_RTC0->EVENTS_COMPARE[1] = 0;

		ticker_trigger(1);
	}

	// Count overflows
	if (NRF_RTC0->EVENTS_OVRFLW){
		NRF_RTC0->EVENTS_OVRFLW = 0;
		btstack_run_loop_rtc0_overflow();
	}

	mayfly_run(MAYFLY_CALL_ID_0);
}

void swi4_handler(void)
{
	mayfly_run(MAYFLY_CALL_ID_1);
}

void rng_handler(void)
{
	isr_rand(0);
}

void radio_handler(void)
{
	isr_radio(0);
}

void mayfly_enable(uint8_t caller_id, uint8_t callee_id, uint8_t enable)
{
	(void)caller_id;

	ASSERT(callee_id == MAYFLY_CALL_ID_1);

	if (enable) {
		irq_enable(SWI4_IRQn);
	} else {
		irq_disable(SWI4_IRQn);
	}
}

uint32_t mayfly_is_enabled(uint8_t caller_id, uint8_t callee_id)
{
	(void)caller_id;

	if (callee_id == MAYFLY_CALL_ID_0) {
		return irq_is_enabled(RTC0_IRQn);
	} else if (callee_id == MAYFLY_CALL_ID_1) {
		return irq_is_enabled(SWI4_IRQn);
	} else {
		ASSERT(0);
	}

	return 0;
}

uint32_t mayfly_prio_is_equal(uint8_t caller_id, uint8_t callee_id)
{
	return (caller_id == callee_id) ||
	       ((caller_id == MAYFLY_CALL_ID_0) &&
		(callee_id == MAYFLY_CALL_ID_1)) ||
	       ((caller_id == MAYFLY_CALL_ID_1) &&
		(callee_id == MAYFLY_CALL_ID_0));
}

void mayfly_pend(uint8_t caller_id, uint8_t callee_id)
{
	(void)caller_id;

	switch (callee_id) {
	case MAYFLY_CALL_ID_0:
		irq_pending_set(RTC0_IRQn);
		break;

	case MAYFLY_CALL_ID_1:
		irq_pending_set(SWI4_IRQn);
		break;

	case MAYFLY_CALL_ID_PROGRAM:
	default:
		ASSERT(0);
		break;
	}
}

void radio_active_callback(uint8_t active)
{
	(void)active;
}

void radio_event_callback(void)
{
}

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	UNUSED(channel);
	UNUSED(size);
    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != BTSTACK_EVENT_STATE) return;
    if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
    printf("BTstack up and running.\n");
}

int main(void)
{
	uint32_t retval;

	DEBUG_INIT();

	/* Dongle RGB LED */
	NRF_GPIO->DIRSET = (1 << 21) | (1 << 22) | (1 << 23);
	NRF_GPIO->OUTSET = (1 << 21) | (1 << 22) | (1 << 23);

	/* Mayfly shall be initialized before any ISR executes */
	mayfly_init();

	#if UART
	uart_init(UART, 1);
	irq_priority_set(UART0_IRQn, 0xFF);
	irq_enable(UART0_IRQn);

	uart_tx_str("\n\n\nBLE LL.\n");

	{
		extern void assert_print(void);

		assert_print();
	}
	#endif

	/* turn on green LED */
	NRF_GPIO->OUTSET = (1 << 21) | (1 << 22) | (1 << 23);
	NRF_GPIO->OUTCLR = (1 << 22);

	clock_k32src_start(1);
	irq_priority_set(POWER_CLOCK_IRQn, 0xFF);
	irq_enable(POWER_CLOCK_IRQn);

	cntr_init();
	irq_priority_set(RTC0_IRQn, 0xFE);
	irq_enable(RTC0_IRQn);

	// also enable OVERLOW event

	// enable ticker overflow couner
	NRF_RTC0->EVTENSET = RTC_EVTENSET_OVRFLW_Msk;
	NRF_RTC0->INTENSET = RTC_EVTENSET_OVRFLW_Msk;

    // assert RTC doesn't stop counting when there are no tickers active
    // note: BTstack Run Loop works when RTC gets stopped, however, the system time pauses as well
    cntr_start(); 

	irq_priority_set(SWI4_IRQn, 0xFF);
	irq_enable(SWI4_IRQn);

	ticker_users[MAYFLY_CALL_ID_0][0] = RADIO_TICKER_USER_WORKER_OPS;
	ticker_users[MAYFLY_CALL_ID_1][0] = RADIO_TICKER_USER_JOB_OPS;
	ticker_users[MAYFLY_CALL_ID_2][0] = 0;
	ticker_users[MAYFLY_CALL_ID_PROGRAM][0] = RADIO_TICKER_USER_APP_OPS;

	ticker_init(RADIO_TICKER_INSTANCE_ID_RADIO, RADIO_TICKER_NODES,
		    &ticker_nodes[0], MAYFLY_CALLER_COUNT, &ticker_users[0],
		    RADIO_TICKER_USER_OPS, &ticker_user_ops[0]);

	rand_init(rng, sizeof(rng));
	irq_priority_set(RNG_IRQn, 0xFF);
	irq_enable(RNG_IRQn);

	irq_priority_set(ECB_IRQn, 0xFF);

	retval = radio_init(7, /* 20ppm = 7 ... 250ppm = 1, 500ppm = 0 */
			    RADIO_CONNECTION_CONTEXT_MAX,
			    RADIO_PACKET_COUNT_RX_MAX,
			    RADIO_PACKET_COUNT_TX_MAX,
			    RADIO_LL_LENGTH_OCTETS_RX_MAX,
			    RADIO_PACKET_TX_DATA_SIZE,
			    &radio[0], sizeof(radio));

	if (retval){
		printf("RADIO_MEM_MNG_SIZE must be %u (current size %u)\n", (int) retval, (int) sizeof(radio));
	}
	ASSERT(retval == 0);

	irq_priority_set(RADIO_IRQn, 0xFE);

	/* turn on blue LED */
	NRF_GPIO->OUTSET = (1 << 21) | (1 << 22) | (1 << 23);
	NRF_GPIO->OUTCLR = (1 << 23);

    // make Random Static Address from FICR available via HCI Read BD ADDR as fake public address
    uint8_t addr[6];
    little_endian_store_16(addr, 4, NRF_FICR->DEVICEADDR[1] | 0xc000);
    little_endian_store_32(addr, 0, NRF_FICR->DEVICEADDR[0]);
    ll_address_set(0, addr);

	printf("BTstack booting up..\n");

	// start with BTstack init - especially configure HCI Transport
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_zephyr_get_instance());

    // init HCI
    hci_init(hci_transport_zephyr_get_instance(), NULL);

    // enable full log output while porting
    hci_dump_open(NULL, HCI_DUMP_STDOUT);
    log_info("HCI Dump ready!");
    // inform about BTstack state

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    
    // hand over to btstack embedded code 
    btstack_main();
    btstack_run_loop_execute();

    while (1);
}
