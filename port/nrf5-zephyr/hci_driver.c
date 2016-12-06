/*
 * Copyright (c) 2016 Nordic Semiconductor ASA
 * Copyright (c) 2016 Vinayak Kariappa Chettimada
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <stddef.h>
#include <string.h>

#include <zephyr.h>
#include <soc.h>
#include <init.h>
#include <device.h>
#include <clock_control.h>

#include <misc/util.h>
#include <misc/stack.h>
#include <misc/byteorder.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/log.h>
#include <bluetooth/hci.h>
#include <drivers/bluetooth/hci_driver.h>

#ifdef CONFIG_CLOCK_CONTROL_NRF5
#include <drivers/clock_control/nrf5_clock_control.h>
#endif

#include "util/defines.h"
#include "util/work.h"
#include "hal/rand.h"
#include "hal/ccm.h"
#include "hal/radio.h"
#include "ll/ticker.h"
#include "ll/ctrl_internal.h"
#include "hci_internal.h"

#include "hal/debug.h"

#include "bluetooth.h"

// temp
#include "zephyr_diet.h"

// updated hci.c
int  btstack_hci_cmd_handle(btstack_buf_t *cmd, btstack_buf_t *evt);
void btstack_hci_evt_encode(struct radio_pdu_node_rx *node_rx, btstack_buf_t *buf);
void btstack_hci_num_cmplt_encode(btstack_buf_t *buf, uint16_t handle, uint8_t num);

#if !defined(CONFIG_BLUETOOTH_DEBUG_HCI_DRIVER)
#undef BT_DBG
#define BT_DBG(fmt, ...)
#endif

#define HCI_CMD		0x01
#define HCI_ACL		0x02
#define HCI_SCO		0x03
#define HCI_EVT		0x04

static uint8_t ALIGNED(4) _rand_context[3 + 4 + 1];
static uint8_t ALIGNED(4) _ticker_nodes[RADIO_TICKER_NODES][TICKER_NODE_T_SIZE];
static uint8_t ALIGNED(4) _ticker_users[RADIO_TICKER_USERS][TICKER_USER_T_SIZE];
static uint8_t ALIGNED(4) _ticker_user_ops[RADIO_TICKER_USER_OPS]
						[TICKER_USER_OP_T_SIZE];
static uint8_t ALIGNED(4) _radio[LL_MEM_TOTAL];

// BTstack: make public
void btstack_run_loop_embedded_trigger(void);

void radio_active_callback(uint8_t active)
{
}

void radio_event_callback(void)
{
	btstack_run_loop_embedded_trigger();
}

static void radio_nrf5_isr(void *arg)
{
	radio_isr();
}

static void rtc0_nrf5_isr(void *arg)
{
	uint32_t compare0, compare1;

	/* store interested events */
	compare0 = NRF_RTC0->EVENTS_COMPARE[0];
	compare1 = NRF_RTC0->EVENTS_COMPARE[1];

	/* On compare0 run ticker worker instance0 */
	if (compare0) {
		NRF_RTC0->EVENTS_COMPARE[0] = 0;

		ticker_trigger(0);
	}

	/* On compare1 run ticker worker instance1 */
	if (compare1) {
		NRF_RTC0->EVENTS_COMPARE[1] = 0;

		ticker_trigger(1);
	}

	work_run(RTC0_IRQn);
}

static void rng_nrf5_isr(void *arg)
{
	rng_isr();
}

static void swi4_nrf5_isr(void *arg)
{
	work_run(NRF5_IRQ_SWI4_IRQn);
}

static void swi5_nrf5_isr(void *arg)
{
	work_run(NRF5_IRQ_SWI5_IRQn);
}

// from subsys/bluetooth/controller/hci/hci.c
void hci_acl_encode_btstack(struct radio_pdu_node_rx *node_rx, uint8_t * packet_buffer, uint16_t * packet_size)
{
	struct bt_hci_acl_hdr *acl;
	struct pdu_data *pdu_data;
	uint16_t handle_flags;
	uint16_t handle;
	uint8_t *data;

	uint16_t offset = 0;

	pdu_data = (struct pdu_data *)node_rx->pdu_data;
	handle = node_rx->hdr.handle;

	switch (pdu_data->ll_id) {
	case PDU_DATA_LLID_DATA_CONTINUE:
	case PDU_DATA_LLID_DATA_START:
		if (pdu_data->ll_id == PDU_DATA_LLID_DATA_START) {
			handle_flags = bt_acl_handle_pack(handle, BT_ACL_START);
		} else {
			handle_flags = bt_acl_handle_pack(handle, BT_ACL_CONT);
		}
		acl = (struct bt_hci_acl_hdr*) packet_buffer;
		acl->handle = sys_cpu_to_le16(handle_flags);
		acl->len = sys_cpu_to_le16(pdu_data->len);	
		offset += sizeof(*acl);

		data = &packet_buffer[offset];
		memcpy(data, &pdu_data->payload.lldata[0], pdu_data->len);
		offset += pdu_data->len;
		break;

	default:
		LL_ASSERT(0);
		break;
	}
	*packet_size = offset;
}


static void recv_task_a(struct radio_pdu_node_rx *node_rx, uint8_t * packet_type, uint8_t * packet_buffer, uint16_t * packet_size){

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

	radio_rx_dequeue();
	radio_rx_fc_set(node_rx->hdr.handle, 0);
	node_rx->hdr.onion.next = 0;
	radio_rx_mem_release(&node_rx);
}

// BTstack: make public
static void hci_driver_emit_num_completed(uint16_t handle, uint16_t num_completed, uint8_t * packet_buffer, uint16_t * packet_size){
	btstack_buf_t buf;
	buf.data = packet_buffer;
	buf.len = 0;
	btstack_hci_num_cmplt_encode(&buf, handle, num_completed);
	*packet_size = buf.len;
}

// returns 1 if done
int hci_driver_task_step(uint8_t * packet_type, uint8_t * packet_buffer, uint16_t * packet_size){
	uint16_t handle;
	struct radio_pdu_node_rx *node_rx;

	// if num_completed != 0 => node_rx == null
	uint8_t num_completed = radio_rx_get(&node_rx, &handle);

	if (num_completed){
		hci_driver_emit_num_completed(handle,num_completed, packet_buffer, packet_size);
		*packet_type = HCI_EVENT_PACKET;
		return 0;
	}

	if (node_rx) {
		recv_task_a(node_rx, packet_type, packet_buffer, packet_size);
		return 0;
	}

	return 1;
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

static int hci_driver_send(struct net_buf *buf){
	printf("hci_driver_send called, but not active anymore\n");
	return -EINVAL;
}

static int hci_driver_open(void)
{
	struct device *clk_k32;
	struct device *clk_m16;
	uint32_t err;

	DEBUG_INIT();

	clk_k32 = device_get_binding(CONFIG_CLOCK_CONTROL_NRF5_K32SRC_DRV_NAME);
	if (!clk_k32) {
		return -ENODEV;
	}

	clock_control_on(clk_k32, (void *)CLOCK_CONTROL_NRF5_K32SRC);

	_ticker_users[RADIO_TICKER_USER_ID_WORKER][0] =
	    RADIO_TICKER_USER_WORKER_OPS;
	_ticker_users[RADIO_TICKER_USER_ID_JOB][0] =
		RADIO_TICKER_USER_JOB_OPS;
	_ticker_users[RADIO_TICKER_USER_ID_APP][0] =
		RADIO_TICKER_USER_APP_OPS;

	ticker_init(RADIO_TICKER_INSTANCE_ID_RADIO, RADIO_TICKER_NODES,
		   &_ticker_nodes[0]
		   , RADIO_TICKER_USERS, &_ticker_users[0]
		   , RADIO_TICKER_USER_OPS, &_ticker_user_ops[0]
	    );

	rand_init(_rand_context, sizeof(_rand_context));

	clk_m16 = device_get_binding(CONFIG_CLOCK_CONTROL_NRF5_M16SRC_DRV_NAME);
	if (!clk_m16) {
		return -ENODEV;
	}

	err = radio_init(clk_m16,
			 CLOCK_CONTROL_NRF5_K32SRC_ACCURACY,
			 RADIO_CONNECTION_CONTEXT_MAX,
			 RADIO_PACKET_COUNT_RX_MAX,
			 RADIO_PACKET_COUNT_TX_MAX,
			 RADIO_LL_LENGTH_OCTETS_RX_MAX, &_radio[0],
			 sizeof(_radio));
	if (err) {
		BT_ERR("Required RAM size: %d, supplied: %u.", err,
		       sizeof(_radio));
		return -ENOMEM;
	}

	IRQ_CONNECT(NRF5_IRQ_RADIO_IRQn, 0, radio_nrf5_isr, 0, 0);
	IRQ_CONNECT(NRF5_IRQ_RTC0_IRQn, 0, rtc0_nrf5_isr, 0, 0);
	IRQ_CONNECT(NRF5_IRQ_RNG_IRQn, 1, rng_nrf5_isr, 0, 0);
	IRQ_CONNECT(NRF5_IRQ_SWI4_IRQn, 0, swi4_nrf5_isr, 0, 0);
	IRQ_CONNECT(NRF5_IRQ_SWI5_IRQn, 1, swi5_nrf5_isr, 0, 0);
	irq_enable(NRF5_IRQ_RADIO_IRQn);
	irq_enable(NRF5_IRQ_RTC0_IRQn);
	irq_enable(NRF5_IRQ_RNG_IRQn);
	irq_enable(NRF5_IRQ_SWI4_IRQn);
	irq_enable(NRF5_IRQ_SWI5_IRQn);

	BT_DBG("Success.");

	return 0;
}

static struct bt_hci_driver drv = {
	.name	= "Controller",
	.bus	= BT_HCI_DRIVER_BUS_VIRTUAL,
	.open	= hci_driver_open,
	.send	= hci_driver_send,
};

static int _hci_driver_init(struct device *unused)
{
	ARG_UNUSED(unused);

	bt_hci_driver_register(&drv);

	return 0;
}

SYS_INIT(_hci_driver_init, NANOKERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
