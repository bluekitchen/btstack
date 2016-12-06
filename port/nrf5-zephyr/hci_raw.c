/* hci_userchan.c - HCI user channel Bluetooth handling */

/*
 * Copyright (c) 2015-2016 Intel Corporation
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
#include <atomic.h>

#include <bluetooth/hci_driver.h>
#include <bluetooth/log.h>

// temp
#include "zephyr_diet.h"

// #include "monitor.h"

static struct bt_dev {
	/* Registered HCI driver */
	struct bt_hci_driver	*drv;
} bt_dev;

int bt_hci_driver_register(struct bt_hci_driver *drv)
{
	if (bt_dev.drv) {
		return -EALREADY;
	}

	if (!drv->open || !drv->send) {
		return -EINVAL;
	}

	bt_dev.drv = drv;

	BT_DBG("Registered %s", drv->name ? drv->name : "");

	return 0;
}

void bt_hci_driver_unregister(struct bt_hci_driver *drv)
{
	bt_dev.drv = NULL;
}

#if 0
int bt_send(struct net_buf *buf)
{
	BT_DBG("buf %p len %u", buf, buf->len);
	// bt_monitor_send(bt_monitor_opcode(buf), buf->data, buf->len);
	return bt_dev.drv->send(buf);
}
#endif

int bt_enable_raw(struct nano_fifo *rx_queue)
{
	struct bt_hci_driver *drv = bt_dev.drv;
	int err;

	BT_DBG("");

	// net_buf_pool_init(hci_evt_pool);

	if (!bt_dev.drv) {
		BT_ERR("No HCI driver registered");
		return -ENODEV;
	}

	err = drv->open();
	if (err) {
		BT_ERR("HCI driver open failed (%d)", err);
		return err;
	}

	BT_INFO("Bluetooth enabled in RAW mode");

	return 0;
}
