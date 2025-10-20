/*
 * Copyright (C) 2025 BlueKitchen GmbH
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

// Zephyr
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/buf.h>
#include <zephyr/bluetooth/hci_raw.h>

// BTstack
#include "btstack_debug.h"
#include "hci.h"
#include "hci_transport.h"

#include "btstack_run_loop_zephyr.h"

static K_FIFO_DEFINE(rx_queue);

static void (*transport_packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

static btstack_data_source_t hci_transport_zephyr_receive;
static fat_variable_t rx_queue_handle = { .variable = &rx_queue, .id = K_POLL_TYPE_FIFO_DATA_AVAILABLE };

static void hci_transport_zephyr_handler(btstack_data_source_t * ds, btstack_data_source_callback_type_t callback_type) {
    UNUSED(ds);
    UNUSED(callback_type);
    struct net_buf *buf = k_fifo_get( &rx_queue, K_FOREVER );

    uint16_t    size = buf->len-1;
    uint8_t * packet = buf->data+1;
    uint8_t     type = buf->data[0];
    switch (type) {
        case BT_HCI_H4_ISO:
            transport_packet_handler(HCI_ISO_DATA_PACKET, packet, size);
            break;
        case BT_HCI_H4_ACL:
            transport_packet_handler(HCI_ACL_DATA_PACKET, packet, size);
            break;
        case BT_HCI_H4_EVT:
            transport_packet_handler(HCI_EVENT_PACKET, packet, size);
            break;
        default:
            log_error("Unknown type %u\n", type);
            break;
    }
    net_buf_unref(buf);
}

#if DT_HAS_COMPAT_STATUS_OKAY(infineon_cyw43xxx_bt_hci)
static const struct device *const h4_dev = DEVICE_DT_GET(DT_PARENT(DT_CHOSEN(zephyr_bt_hci)));
extern int bt_h4_vnd_setup(const struct device *dev);
#endif

/**
 * init transport
 * @param transport_config
 */
static void transport_init(const void *transport_config){

    btstack_data_source_t *ds = &hci_transport_zephyr_receive;
    btstack_run_loop_set_data_source_handle(ds, &rx_queue_handle);
    btstack_run_loop_set_data_source_handler(ds, &hci_transport_zephyr_handler);
    btstack_run_loop_add_data_source(ds);
    btstack_run_loop_enable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_READ);

    int ret;
    /* startup Controller */
    ret = bt_enable_raw(&rx_queue);
    btstack_assert(ret == 0);
#if DT_HAS_COMPAT_STATUS_OKAY(infineon_cyw43xxx_bt_hci)
    ret = bt_h4_vnd_setup(h4_dev);
    btstack_assert(ret == 0)
#endif
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

static void send_hardware_error(uint8_t error_code){
    // hci_outgoing_event[0] = HCI_EVENT_HARDWARE_ERROR;
    // hci_outgoing_event[1] = 1;
    // hci_outgoing_event[2] = error_code;
    // hci_outgoing_event_ready = 1;
}

static int transport_send_packet(uint8_t packet_type, uint8_t *packet, int size){
    struct net_buf *buf;
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
            buf = bt_buf_get_tx(BT_BUF_CMD, K_NO_WAIT, packet, size);
            if (!buf) {
                log_error("No available command buffers!\n");
                break;
            }

            bt_send(buf);
            break;
        case HCI_ACL_DATA_PACKET:
            buf = bt_buf_get_tx(BT_BUF_ACL_OUT, K_NO_WAIT, packet, size);
            if (!buf) {
                log_error("No available ACL buffers!\n");
                break;
            }

            bt_send(buf);
            break;
        case HCI_ISO_DATA_PACKET:
            buf = bt_buf_get_tx(BT_BUF_ISO_OUT, K_NO_WAIT, packet, size);
            if (!buf) {
                log_error("No available ISO buffers!\n");
                break;
            }

            bt_send(buf);
            break;
       default:
            send_hardware_error(0x01);  // invalid HCI packet
            break;
    }

    return 0;
}

// provide HCI functions used by vendor setup, normally provided by hci_core.c

#include <zephyr/sys/byteorder.h>

/** Allocate an HCI command buffer.
 *
 * This function allocates a new buffer for an HCI command. Upon successful
 * return the buffer is ready to have the command parameters encoded into it.
 * Sufficient headroom gets automatically reserved in the buffer, allowing
 * the actual command and H:4 headers to be encoded later, as part of
 * calling bt_hci_cmd_send() or bt_hci_cmd_send_sync().
 *
 * @param timeout Timeout for the allocation.
 *
 * @return Newly allocated buffer or NULL if allocation failed.
 */
struct net_buf *bt_hci_cmd_alloc(k_timeout_t timeout) {
    struct net_buf *buf;
    buf = bt_buf_get_tx(BT_BUF_CMD, timeout, NULL, 0);
    if (!buf) {
        log_error("No available command buffers!\n");
        btstack_assert(false);
        return NULL;
    }
    return buf;
}

/** Send a HCI command synchronously.
  *
  * This function is used for sending a HCI command synchronously. It can
  * either be called for a buffer created using bt_hci_cmd_create(), or
  * if the command has no parameters a NULL can be passed instead.
  *
  * The function will block until a Command Status or a Command Complete
  * event is returned. If either of these have a non-zero status the function
  * will return a negative error code and the response reference will not
  * be set. If the command completed successfully and a non-NULL rsp parameter
  * was given, this parameter will be set to point to a buffer containing
  * the response parameters.
  *
  * @param opcode Command OpCode.
  * @param buf    Command buffer or NULL (if no parameters).
  * @param rsp    Place to store a reference to the command response. May
  *               be NULL if the caller is not interested in the response
  *               parameters. If non-NULL is passed the caller is responsible
  *               for calling net_buf_unref() on the buffer when done parsing
  *               it.
  *
  * @return 0 on success or negative error value on failure.
  */
#define xSYNC_SEND_PACKETLOG
int bt_hci_cmd_send_sync(uint16_t opcode, struct net_buf *buf, struct net_buf **rsp){
    // create packet if needed from opcode
    if (!buf) {
        buf = bt_hci_cmd_alloc( K_NO_WAIT );
        if (!buf) {
            return -ENOBUFS;
        }
    }

    struct bt_hci_cmd_hdr *hdr;

    // thanks to bt_buf_get_tx buf is now sizeof(*hdr) + 1 byte big, with the following layout
    // [BT_HCI_H4_CMD][payload]
    uint8_t *bytes = net_buf_push(buf, sizeof(*hdr));
    // [HDR][BT_HCI_H4_CMD][payload]
    bytes[0] = BT_HCI_H4_CMD;
    hdr = (struct bt_hci_cmd_hdr*)&bytes[1];
    hdr->opcode = sys_cpu_to_le16(opcode);
    hdr->param_len = buf->len - sizeof(*hdr) - 1;

    // hci dump packet
#ifdef SYNC_SEND_PACKETLOG
    uint8_t packet_type = buf->data[0];
    uint16_t  command_size = buf->len-1;
    uint8_t * command_data = buf->data+1;
    hci_dump_packet(packet_type, 0, command_data, command_size);
#endif
    // send packet
    bt_send(buf);

    // wait for HCI Event, timeout is considered an error
    struct net_buf * event_buffer = k_fifo_get(&rx_queue, K_MSEC(1000));
    int err;
    if (event_buffer){
#ifdef SYNC_SEND_PACKETLOG
        uint8_t packet_type = event_buffer->data[0];
        uint16_t  event_size = event_buffer->len-1;
        uint8_t * event_data = event_buffer->data+1;
        hci_dump_packet(packet_type, 1, event_data, event_size);
#endif
        net_buf_unref(event_buffer);
        err = 0;
    } else {
        log_info("No response for HCI CMD, abort");
        err = -EIO;

    }
    return err;
}

static const hci_transport_t transport = {
    /* const char * name; */                                        "zephyr",
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


