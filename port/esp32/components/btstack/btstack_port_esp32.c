/*
 * Copyright (C) 2023 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "main.c"

#include "sdkconfig.h"

#if CONFIG_BT_ENABLED

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_config.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_freertos.h"
#include "btstack_ring_buffer.h"
#include "btstack_tlv.h"
#include "btstack_tlv_esp32.h"
#include "ble/le_device_db_tlv.h"
#include "classic/btstack_link_key_db.h"
#include "classic/btstack_link_key_db_tlv.h"
#include "hci.h"
#include "hci_dump.h"
#include "esp_bt.h"
#include "btstack_debug.h"
#include "btstack_audio.h"
#include "btstack_port_esp32.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

uint32_t esp_log_timestamp();

uint32_t hal_time_ms(void) {
    // super hacky way to get ms
    return esp_log_timestamp();
}

#ifdef CONFIG_BT_ENABLED

// ESP32 and next generation ESP32-C3 + ESP32-S3 provide an asynchronous VHCI interface with
// a callback when the packet was accepted by the Controller. 
// Newer Controller only rely on the regular Host to Controller Flow Control and can be 
// considered synchronous

#if defined(CONFIG_IDF_TARGET_ESP32) || defined (CONFIG_IDF_TARGET_ESP32C3) || defined (CONFIG_IDF_TARGET_ESP32S3)
#define ENABLE_ESP32_VHCI_ASYNCHRONOUS
#endif

// assert pre-buffer for packet type is available
#if !defined(HCI_OUTGOING_PRE_BUFFER_SIZE) || (HCI_OUTGOING_PRE_BUFFER_SIZE < 1)
#error HCI_OUTGOING_PRE_BUFFER_SIZE not defined or smaller than 1. Please update hci.h
#endif

static void (*transport_packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

// ring buffer for incoming HCI packets. Each packet has 2 byte len tag + H4 packet type + packet itself
#define MAX_NR_HOST_EVENT_PACKETS 4
static uint8_t hci_ringbuffer_storage[HCI_HOST_ACL_PACKET_NUM   * (2 + 1 + HCI_ACL_HEADER_SIZE + HCI_HOST_ACL_PACKET_LEN) +
                                      HCI_HOST_SCO_PACKET_NUM   * (2 + 1 + HCI_SCO_HEADER_SIZE + HCI_HOST_SCO_PACKET_LEN) +
                                      MAX_NR_HOST_EVENT_PACKETS * (2 + 1 + HCI_EVENT_BUFFER_SIZE)];

static btstack_ring_buffer_t hci_ringbuffer;

// incoming packet buffer
static uint8_t hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + HCI_INCOMING_PACKET_BUFFER_SIZE]; // packet type + max(acl header + acl payload, event header + event data)
static uint8_t * hci_receive_buffer = &hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE];

static SemaphoreHandle_t ring_buffer_mutex;

static void transport_notify_packet_send(void *context);
static btstack_context_callback_registration_t packet_send_callback_context = {
        .callback = transport_notify_packet_send,
        .context = NULL,
};

static void transport_deliver_packets(void *context);
static btstack_context_callback_registration_t packet_receive_callback_context = {
        .callback = transport_deliver_packets,
        .context = NULL,
};

// never supposed to happen, just panic if it does happen anyway
static void panic_recv_called_from_isr(void) {
    btstack_assert(0);
    printf("host_recv_pkt_cb called from ISR!\n");
}

static void panic_sent_called_from_isr(void) {
    btstack_assert(0);
    printf("host_send_pkt_available_cb called from ISR!\n");
}

// VHCI callbacks, run from VHCI Task "BT Controller"

static void host_send_pkt_available_cb(void){

    if (xPortInIsrContext()) {
        panic_sent_called_from_isr();
        return;
    }

    btstack_run_loop_execute_on_main_thread(&packet_send_callback_context);
}

static int host_recv_pkt_cb(uint8_t *data, uint16_t len){

    if (xPortInIsrContext()){
        panic_recv_called_from_isr();
        return 0;
    }

    xSemaphoreTake(ring_buffer_mutex, portMAX_DELAY);

    // check space
    uint16_t space = btstack_ring_buffer_bytes_free(&hci_ringbuffer);
    if (space < len){
        xSemaphoreGive(ring_buffer_mutex);
        log_error("transport_recv_pkt_cb packet %u, space %u -> dropping packet", len, space);
        return 0;
    }

    // store size in ringbuffer
    uint8_t len_tag[2];
    little_endian_store_16(len_tag, 0, len);
    btstack_ring_buffer_write(&hci_ringbuffer, len_tag, sizeof(len_tag));

    // store in ringbuffer
    btstack_ring_buffer_write(&hci_ringbuffer, data, len);

    xSemaphoreGive(ring_buffer_mutex);

    btstack_run_loop_execute_on_main_thread(&packet_receive_callback_context);
    return 0;
}

static const esp_vhci_host_callback_t vhci_host_cb = {
    .notify_host_send_available = host_send_pkt_available_cb,
    .notify_host_recv = host_recv_pkt_cb,
};

// run from main thread

static void transport_notify_packet_send(void *context){
    UNUSED(context);
    // notify upper stack that it might be possible to send again
    uint8_t event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};
    transport_packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));
}

static void transport_deliver_packets(void *context){
    UNUSED(context);
    xSemaphoreTake(ring_buffer_mutex, portMAX_DELAY);
    while (btstack_ring_buffer_bytes_available(&hci_ringbuffer)){
        uint32_t number_read;
        uint8_t len_tag[2];
        btstack_ring_buffer_read(&hci_ringbuffer, len_tag, 2, &number_read);
        uint32_t len = little_endian_read_16(len_tag, 0);
        btstack_ring_buffer_read(&hci_ringbuffer, hci_receive_buffer, len, &number_read);
        xSemaphoreGive(ring_buffer_mutex);
        transport_packet_handler(hci_receive_buffer[0], &hci_receive_buffer[1], len-1);
        xSemaphoreTake(ring_buffer_mutex, portMAX_DELAY);
    }
    xSemaphoreGive(ring_buffer_mutex);
}


/**
 * init transport
 * @param transport_config
 */
static void transport_init(const void *transport_config){
    log_info("transport_init");
    ring_buffer_mutex = xSemaphoreCreateMutex();
}

/**
 * open transport connection
 */
static int bt_controller_initialized;
static int transport_open(void){
    esp_err_t ret;

#ifdef ENABLE_ESP32_VHCI_ASYNCHRONOUS
    log_info("transport_open: using asynchronous VHCI");
#else
    log_info("transport_open: using synchronous VHCI");
#endif

    btstack_ring_buffer_init(&hci_ringbuffer, hci_ringbuffer_storage, sizeof(hci_ringbuffer_storage));

    // http://esp-idf.readthedocs.io/en/latest/api-reference/bluetooth/controller_vhci.html (2017104)
    // - "esp_bt_controller_init: ... This function should be called only once, before any other BT functions are called."
    // - "esp_bt_controller_deinit" .. This function should be called only once, after any other BT functions are called. 
    //    This function is not whole completed, esp_bt_controller_init cannot called after this function."
    // -> esp_bt_controller_init can only be called once after boot
    if (!bt_controller_initialized){
        bt_controller_initialized = 1;


#if CONFIG_IDF_TARGET_ESP32
#ifndef ENABLE_CLASSIC
        //  LE-only on ESP32 - release memory used for classic mode
        ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
        if (ret) {
            log_error("Bluetooth controller release classic bt memory failed: %s", esp_err_to_name(ret));
            return -1;
        }
#endif
#endif

        esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        ret = esp_bt_controller_init(&bt_cfg);
        if (ret) {
            log_error("transport: esp_bt_controller_init failed");
            return -1;
        }

    }

    // Enable LE mode by default
    esp_bt_mode_t bt_mode = ESP_BT_MODE_BLE;
#if CONFIG_IDF_TARGET_ESP32
#if CONFIG_BTDM_CTRL_MODE_BTDM
    bt_mode = ESP_BT_MODE_BTDM;
#elif CONFIG_BTDM_CTRL_MODE_BR_EDR_ONLY
    bt_mode = ESP_BT_MODE_CLASSIC_BT;
#endif
#endif

    ret = esp_bt_controller_enable(bt_mode);
    if (ret) {
        log_error("transport: esp_bt_controller_enable failed");
        return -1;
    }

    esp_vhci_host_register_callback(&vhci_host_cb);

    return 0;
}

/**
 * close transport connection
 */
static int transport_close(void){
    log_info("transport_close");

    // disable controller
    esp_bt_controller_disable();
    return 0;
}

/**
 * register packet handler for HCI packets: ACL, SCO, and Events
 */
static void transport_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    log_info("transport_register_packet_handler");
    transport_packet_handler = handler;
}

#ifdef ENABLE_ESP32_VHCI_ASYNCHRONOUS
static int transport_can_send_packet_now(uint8_t packet_type) {
    return esp_vhci_host_check_send_available();
}
#endif

static int transport_send_packet(uint8_t packet_type, uint8_t *packet, int size){
    // store packet type before actual data and increase size
    size++;
    packet--;
    *packet = packet_type;

    // send packet
    esp_vhci_host_send_packet(packet, size);
    return 0;
}

static const hci_transport_t transport = {
    .name  = "esp32-vhci",
    .init  = &transport_init,
    .open  = &transport_open,
    .close = &transport_close,
    .register_packet_handler = &transport_register_packet_handler,
#ifdef ENABLE_ESP32_VHCI_ASYNCHRONOUS
    // asynchronous
    .can_send_packet_now = &transport_can_send_packet_now,
#else
    // synchronous <=> can_send_packet_now == NULL
#endif
    .send_packet = &transport_send_packet,
};

#else

// this port requires the ESP32 Bluetooth to be enabled in the sdkconfig
// try to tell the user

#include "esp_log.h"
static void transport_init(const void *transport_config){
    while (1){
        ESP_LOGE("BTstack", "ESP32 Transport Init called, but Bluetooth support not enabled in sdkconfig.");
        ESP_LOGE("BTstack", "Please enable CONFIG_BT_ENABLED with 'make menuconfig and compile again.");
        ESP_LOGE("BTstack", "");
    }
}

static const hci_transport_t transport = {
    .name = "none",
    .init = &transport_init,
};
#endif


static const hci_transport_t * transport_get_instance(void){
    return &transport;
}

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    switch(hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
#ifdef ENABLE_SCO_OVER_HCI
                esp_err_t ret = esp_bredr_sco_datapath_set(ESP_SCO_DATA_PATH_HCI);
                log_info("transport: configure SCO over HCI, result 0x%04x", ret);
#endif
                bd_addr_t addr;
                gap_local_bd_addr(addr);
                printf("BTstack up and running at %s\n",  bd_addr_to_str(addr));
            }
            break;
        case HCI_EVENT_COMMAND_COMPLETE:
            if (hci_event_command_complete_get_command_opcode(packet) == HCI_OPCODE_HCI_READ_LOCAL_VERSION_INFORMATION){
                // @TODO
            }
            break;
        default:
            break;
    }
}

uint8_t btstack_init(void){
    // Setup memory pools and run loop
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_freertos_get_instance());

    // init HCI
    hci_init(transport_get_instance(), NULL);

    // setup TLV ESP32 implementation and register with system
    const btstack_tlv_t * btstack_tlv_impl = btstack_tlv_esp32_get_instance();
    btstack_tlv_set_instance(btstack_tlv_impl, NULL);

#ifdef ENABLE_CLASSIC
    // setup Link Key DB using TLV
    const btstack_link_key_db_t * btstack_link_key_db = btstack_link_key_db_tlv_get_instance(btstack_tlv_impl, NULL);
    hci_set_link_key_db(btstack_link_key_db);
#endif

#ifdef ENABLE_BLE
    // setup LE Device DB using TLV
    le_device_db_tlv_configure(btstack_tlv_impl, NULL);
#endif

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

#if CONFIG_BTSTACK_AUDIO
    // setup i2s audio for sink and source
    btstack_audio_sink_set_instance(btstack_audio_esp32_sink_get_instance());
    btstack_audio_source_set_instance(btstack_audio_esp32_source_get_instance());
#endif

    return ERROR_CODE_SUCCESS;
}

#endif
