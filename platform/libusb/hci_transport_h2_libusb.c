/*
 * Copyright (C) 2014 BlueKitchen GmbH
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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define BTSTACK_FILE__ "hci_transport_h2_libusb.c"

/*
 *  hci_transport_usb.c
 *
 *  HCI Transport API implementation for USB
 *
 *  Created by Matthias Ringwald on 7/5/09.
 */

// Interface Number - Alternate Setting - suggested Endpoint Address - Endpoint Type - Suggested Max Packet Size 
// HCI Commands 0 0 0x00 Control 8/16/32/64 
// HCI Events   0 0 0x81 Interrupt (IN) 16 
// ACL Data     0 0 0x82 Bulk (IN) 32/64 
// ACL Data     0 0 0x02 Bulk (OUT) 32/64 
// SCO Data     0 0 0x83 Isochronous (IN)
// SCO Data     0 0 0x03 Isochronous (Out)

#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>   /* UNIX standard function definitions */
#include <sys/types.h>

#include <libusb.h>

#include "btstack_config.h"

#include "btstack_debug.h"
#include "hci.h"
#include "hci_transport.h"
#include "hci_transport_usb.h"

// deal with changes in libusb API:
#ifdef LIBUSB_API_VERSION
#if LIBUSB_API_VERSION >= 0x01000106
// since 1.0.22, libusb_set_option replaces libusb_set_debug
#define libusb_set_debug(context,level) libusb_set_option(context, LIBUSB_OPTION_LOG_LEVEL, level)
#endif
#endif

#if (USB_VENDOR_ID != 0) && (USB_PRODUCT_ID != 0)
#define HAVE_USB_VENDOR_ID_AND_PRODUCT_ID
#endif

#define ACL_IN_BUFFER_COUNT    3
#define EVENT_IN_BUFFER_COUNT  3
#define SCO_IN_BUFFER_COUNT   10

#define ASYNC_POLLING_INTERVAL_MS 1

//
// Bluetooth USB Transport Alternate Settings:
//
// 0: No active voice channels (for USB compliance)
// 1: One 8 kHz voice channel with 8-bit encoding
// 2: Two 8 kHz voice channels with 8-bit encoding or one 8 kHz voice channel with 16-bit encoding
// 3: Three 8 kHz voice channels with 8-bit encoding
// 4: Two 8 kHz voice channels with 16-bit encoding or one 16 kHz voice channel with 16-bit encoding
// 5: Three 8 kHz voice channels with 16-bit encoding or one 8 kHz voice channel with 16-bit encoding and one 16 kHz voice channel with 16-bit encoding
// --> support only a single SCO connection
// #define ALT_SETTING (1)

#ifdef ENABLE_SCO_OVER_HCI
// alt setting for 1-3 connections and 8/16 bit
static const int alt_setting_8_bit[]  = {1,2,3};      
static const int alt_setting_16_bit[] = {2,4,5};

// for ALT_SETTING >= 1 and 8-bit channel, we need the following isochronous packets
// One complete SCO packet with 24 frames every 3 frames (== 3 ms)
#define NUM_ISO_PACKETS (3)

static const uint16_t iso_packet_size_for_alt_setting[] = {
    0,
    9,
    17,
    25,
    33,
    49,
    63,
};
#endif

// 49 bytes is the max usb packet size for alternate setting 5 (Three 8 kHz 16-bit channels or one 8 kHz 16-bit channel and one 16 kHz 16-bit channel)
// note: alt setting 6 has max packet size of 63 every 7.5 ms = 472.5 bytes / HCI packet, while max SCO packet has 255 byte payload
#define SCO_PACKET_SIZE  (49 * NUM_ISO_PACKETS)

// Outgoing SCO packet queue
// simplified ring buffer implementation
#define SCO_OUT_BUFFER_COUNT  (8)
#define SCO_OUT_BUFFER_SIZE (SCO_OUT_BUFFER_COUNT * SCO_PACKET_SIZE)

// seems to be the max depth for USB 3
#define USB_MAX_PATH_LEN 7

// prototypes
static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size); 
static int usb_close(void);    

typedef enum {
    LIB_USB_CLOSED = 0,
    LIB_USB_OPENED,
    LIB_USB_DEVICE_OPENDED,
    LIB_USB_INTERFACE_CLAIMED,
    LIB_USB_TRANSFERS_ALLOCATED
} libusb_state_t;

// SCO packet state machine
typedef enum {
    H2_W4_SCO_HEADER = 1,
    H2_W4_PAYLOAD,
} H2_SCO_STATE;

static libusb_state_t libusb_state = LIB_USB_CLOSED;

// single instance
static hci_transport_t * hci_transport_usb = NULL;

static void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = dummy_handler;

// libusb
#ifndef HAVE_USB_VENDOR_ID_AND_PRODUCT_ID
static struct libusb_device_descriptor desc;
#endif
static libusb_device_handle * handle;

static struct libusb_transfer *command_out_transfer;
static struct libusb_transfer *acl_out_transfer;
static struct libusb_transfer *event_in_transfer[EVENT_IN_BUFFER_COUNT];
static struct libusb_transfer *acl_in_transfer[ACL_IN_BUFFER_COUNT];

#ifdef ENABLE_SCO_OVER_HCI

#ifdef _WIN32
#error "SCO not working on Win32 (Windows 8, libusb 1.0.19, Zadic WinUSB), please uncomment ENABLE_SCO_OVER_HCI in btstack-config.h for now"
#endif

// incoming SCO
static H2_SCO_STATE sco_state;
static uint8_t  sco_buffer[255+3 + SCO_PACKET_SIZE];
static uint16_t sco_read_pos;
static uint16_t sco_bytes_to_read;
static struct  libusb_transfer *sco_in_transfer[SCO_IN_BUFFER_COUNT];
static uint8_t hci_sco_in_buffer[SCO_IN_BUFFER_COUNT][SCO_PACKET_SIZE]; 

// outgoing SCO
static uint8_t  sco_out_ring_buffer[SCO_OUT_BUFFER_SIZE];
static int      sco_ring_write;  // packet idx
static int      sco_out_transfers_active;
static struct libusb_transfer *sco_out_transfers[SCO_OUT_BUFFER_COUNT];
static int      sco_out_transfers_in_flight[SCO_OUT_BUFFER_COUNT];

// pause/resume
static uint16_t sco_voice_setting;
static int      sco_num_connections;
static int      sco_shutdown;

// dynamic SCO configuration
static uint16_t iso_packet_size;
static int      sco_enabled;

#endif

// outgoing buffer for HCI Command packets
static uint8_t hci_cmd_buffer[3 + 256 + LIBUSB_CONTROL_SETUP_SIZE];

// incoming buffer for HCI Events and ACL Packets
static uint8_t hci_event_in_buffer[EVENT_IN_BUFFER_COUNT][HCI_ACL_BUFFER_SIZE]; // bigger than largest packet
static uint8_t hci_acl_in_buffer[ACL_IN_BUFFER_COUNT][HCI_INCOMING_PRE_BUFFER_SIZE + HCI_ACL_BUFFER_SIZE]; 

// For (ab)use as a linked list of received packets
static struct libusb_transfer *handle_packet;

static int doing_pollfds;
static int num_pollfds;
static btstack_data_source_t * pollfd_data_sources;
static btstack_timer_source_t usb_timer;
static int usb_timer_active;

static int usb_acl_out_active = 0;
static int usb_command_active = 0;

// endpoint addresses
static int event_in_addr;
static int acl_in_addr;
static int acl_out_addr;
static int sco_in_addr;
static int sco_out_addr;

// device path
static int usb_path_len;
static uint8_t usb_path[USB_MAX_PATH_LEN];

// transport interface state
static int usb_transport_open;


#ifdef ENABLE_SCO_OVER_HCI
static void sco_ring_init(void){
    sco_ring_write = 0;
    sco_out_transfers_active = 0;
}
static int sco_ring_have_space(void){
    return sco_out_transfers_active < SCO_OUT_BUFFER_COUNT;
}
#endif

void hci_transport_usb_set_path(int len, uint8_t * port_numbers){
    if (len > USB_MAX_PATH_LEN || !port_numbers){
        log_error("hci_transport_usb_set_path: len or port numbers invalid");
        return;
    } 
    usb_path_len = len;
    memcpy(usb_path, port_numbers, len);
}

//
static void queue_transfer(struct libusb_transfer *transfer){

    // log_info("queue_transfer %p, endpoint %x size %u", transfer, transfer->endpoint, transfer->actual_length);

    transfer->user_data = NULL;

    // insert first element
    if (handle_packet == NULL) {
        handle_packet = transfer;
        return;
    }

    // Walk to end of list and add current packet there
    struct libusb_transfer *temp = handle_packet;
    while (temp->user_data) {
        temp = (struct libusb_transfer*)temp->user_data;
    }
    temp->user_data = transfer;
}

LIBUSB_CALL static void async_callback(struct libusb_transfer *transfer){

    int c;

    if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED){
        log_info("shutdown, transfer %p", transfer);
    }


    // identify and free transfers as part of shutdown
#ifdef ENABLE_SCO_OVER_HCI
    if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED || sco_shutdown) {
        for (c=0;c<SCO_IN_BUFFER_COUNT;c++){
            if (transfer == sco_in_transfer[c]){
                libusb_free_transfer(transfer);
                sco_in_transfer[c] = NULL;
                return;
            }
        }

        for (c=0;c<SCO_OUT_BUFFER_COUNT;c++){
            if (transfer == sco_out_transfers[c]){
                sco_out_transfers_in_flight[c] = 0;
                libusb_free_transfer(transfer);
                sco_out_transfers[c] = NULL;
                return;
            }
        }
    }
#endif

    if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED) {
        for (c=0;c<EVENT_IN_BUFFER_COUNT;c++){
            if (transfer == event_in_transfer[c]){
                libusb_free_transfer(transfer);
                event_in_transfer[c] = 0;
                return;
            }
        }
        for (c=0;c<ACL_IN_BUFFER_COUNT;c++){
            if (transfer == acl_in_transfer[c]){
                libusb_free_transfer(transfer);
                acl_in_transfer[c] = 0;
                return;
            }
        }
        return;
    }

#ifdef ENABLE_SCO_OVER_HCI
    // mark SCO OUT transfer as done
    for (c=0;c<SCO_OUT_BUFFER_COUNT;c++){
        if (transfer == sco_out_transfers[c]){
            sco_out_transfers_in_flight[c] = 0;
        }
    }
#endif

    int r;
    // log_info("begin async_callback endpoint %x, status %x, actual length %u", transfer->endpoint, transfer->status, transfer->actual_length );

    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        queue_transfer(transfer);
    } else if (transfer->status == LIBUSB_TRANSFER_STALL){
        log_info("-> Transfer stalled, trying again");
        r = libusb_clear_halt(handle, transfer->endpoint);
        if (r) {
            log_error("Error rclearing halt %d", r);
        }
        r = libusb_submit_transfer(transfer);
        if (r) {
            log_error("Error re-submitting transfer %d", r);
        }
    } else {
        log_info("async_callback. not data -> resubmit transfer, endpoint %x, status %x, length %u", transfer->endpoint, transfer->status, transfer->actual_length);
        // No usable data, just resubmit packet
        r = libusb_submit_transfer(transfer);
        if (r) {
            log_error("Error re-submitting transfer %d", r);
        }
    }
    // log_info("end async_callback");
}


#ifdef ENABLE_SCO_OVER_HCI
static int usb_send_sco_packet(uint8_t *packet, int size){
    int r;

    if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED) return -1;

    // log_info("usb_send_acl_packet enter, size %u", size);

    // store packet in free slot
    int tranfer_index = sco_ring_write;
    uint8_t * data = &sco_out_ring_buffer[tranfer_index * SCO_PACKET_SIZE];
    memcpy(data, packet, size);

    // setup transfer
    // log_info("usb_send_sco_packet: size %u, max size %u, iso packet size %u", size, NUM_ISO_PACKETS * iso_packet_size, iso_packet_size);
    struct libusb_transfer * sco_transfer = sco_out_transfers[tranfer_index];
    libusb_fill_iso_transfer(sco_transfer, handle, sco_out_addr, data, NUM_ISO_PACKETS * iso_packet_size, NUM_ISO_PACKETS, async_callback, NULL, 0);
    libusb_set_iso_packet_lengths(sco_transfer, iso_packet_size);
    r = libusb_submit_transfer(sco_transfer);
    if (r < 0) {
        log_error("Error submitting sco transfer, %d", r);
        return -1;
    }

    // mark slot as full
    sco_ring_write++;
    if (sco_ring_write == SCO_OUT_BUFFER_COUNT){
        sco_ring_write = 0;
    }
    sco_out_transfers_active++;
    sco_out_transfers_in_flight[tranfer_index] = 1;

    // log_info("H2: queued packet at index %u, num active %u", tranfer_index, sco_out_transfers_active);

    // notify upper stack that provided buffer can be used again
    uint8_t event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};
    packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));

    // and if we have more space for SCO packets
    if (sco_ring_have_space()) {
        uint8_t event_sco[] = { HCI_EVENT_SCO_CAN_SEND_NOW, 0};
        packet_handler(HCI_EVENT_PACKET, &event_sco[0], sizeof(event_sco));
    }
    return 0;
}

static void sco_state_machine_init(void){
    sco_state = H2_W4_SCO_HEADER;
    sco_read_pos = 0;
    sco_bytes_to_read = 3;
}

static void handle_isochronous_data(uint8_t * buffer, uint16_t size){
    while (size){
        if (size < sco_bytes_to_read){
            // just store incomplete data
            memcpy(&sco_buffer[sco_read_pos], buffer, size);
            sco_read_pos      += size;
            sco_bytes_to_read -= size;
            return;
        }
        // copy requested data
        memcpy(&sco_buffer[sco_read_pos], buffer, sco_bytes_to_read);
        sco_read_pos += sco_bytes_to_read;
        buffer       += sco_bytes_to_read;
        size         -= sco_bytes_to_read;

        // chunk read successfully, next action
        switch (sco_state){
            case H2_W4_SCO_HEADER:
                sco_state = H2_W4_PAYLOAD;
                sco_bytes_to_read = sco_buffer[2];
                break;
            case H2_W4_PAYLOAD:
                // packet complete
                packet_handler(HCI_SCO_DATA_PACKET, sco_buffer, sco_read_pos);
                sco_state_machine_init();
                break;
			default:
				btstack_assert(false);
				break;
        }
    }
}
#endif

static void handle_completed_transfer(struct libusb_transfer *transfer){

    int resubmit = 0;
    int signal_done = 0;

    if (transfer->endpoint == event_in_addr) {
        packet_handler(HCI_EVENT_PACKET, transfer->buffer, transfer->actual_length);
        resubmit = 1;
    } else if (transfer->endpoint == acl_in_addr) {
        // log_info("-> acl");
        packet_handler(HCI_ACL_DATA_PACKET, transfer->buffer, transfer->actual_length);
        resubmit = 1;
    } else if (transfer->endpoint == 0){
        // log_info("command done, size %u", transfer->actual_length);
        usb_command_active = 0;
        signal_done = 1;
    } else if (transfer->endpoint == acl_out_addr){
        // log_info("acl out done, size %u", transfer->actual_length);
        usb_acl_out_active = 0;
        signal_done = 1;
#ifdef ENABLE_SCO_OVER_HCI
    } else if (transfer->endpoint == sco_in_addr) {
        // log_info("handle_completed_transfer for SCO IN! num packets %u", transfer->NUM_ISO_PACKETS);
        int i;
        for (i = 0; i < transfer->num_iso_packets; i++) {
            struct libusb_iso_packet_descriptor *pack = &transfer->iso_packet_desc[i];
            if (pack->status != LIBUSB_TRANSFER_COMPLETED) {
                log_error("Error: pack %u status %d\n", i, pack->status);
                continue;
            }
            if (!pack->actual_length) continue;
            uint8_t * data = libusb_get_iso_packet_buffer_simple(transfer, i);
            // printf_hexdump(data, pack->actual_length);
            // log_info("handle_isochronous_data,size %u/%u", pack->length, pack->actual_length);
            handle_isochronous_data(data, pack->actual_length);
        }
        resubmit = 1;
    } else if (transfer->endpoint == sco_out_addr){
        int i;
        for (i = 0; i < transfer->num_iso_packets; i++) {
            struct libusb_iso_packet_descriptor *pack = &transfer->iso_packet_desc[i];
            if (pack->status != LIBUSB_TRANSFER_COMPLETED) {
                log_error("Error: pack %u status %d\n", i, pack->status);
            }
        }
        // log_info("sco out done, {{ %u/%u (%x)}, { %u/%u (%x)}, { %u/%u (%x)}}", 
        //     transfer->iso_packet_desc[0].actual_length, transfer->iso_packet_desc[0].length, transfer->iso_packet_desc[0].status,
        //     transfer->iso_packet_desc[1].actual_length, transfer->iso_packet_desc[1].length, transfer->iso_packet_desc[1].status,
        //     transfer->iso_packet_desc[2].actual_length, transfer->iso_packet_desc[2].length, transfer->iso_packet_desc[2].status);
        // notify upper layer if there's space for new SCO packets

        if (sco_ring_have_space()) {
            uint8_t event[] = { HCI_EVENT_SCO_CAN_SEND_NOW, 0};
            packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));
        }
        // decrease tab
        sco_out_transfers_active--;
        // log_info("H2: sco out complete, num active num active %u", sco_out_transfers_active);
#endif
    } else {
        log_info("usb_process_ds endpoint unknown %x", transfer->endpoint);
    }

    if (signal_done){
        // notify upper stack that provided buffer can be used again
        uint8_t event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};
        packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));
    }

    if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED) return;

    if (resubmit){
        // Re-submit transfer 
        transfer->user_data = NULL;
        int r = libusb_submit_transfer(transfer);
        if (r) {
            log_error("Error re-submitting transfer %d", r);
        }
    }   
}

static void usb_process_ds(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {

    UNUSED(ds);
    UNUSED(callback_type);

    if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED) return;

    // log_info("begin usb_process_ds");
    // always handling an event as we're called when data is ready
    struct timeval tv;
    memset(&tv, 0, sizeof(struct timeval));
    libusb_handle_events_timeout(NULL, &tv);

    // Handle any packet in the order that they were received
    while (handle_packet) {
        // log_info("handle packet %p, endpoint %x, status %x", handle_packet, handle_packet->endpoint, handle_packet->status);

        // pop next transfer
        struct libusb_transfer * transfer = handle_packet;
        handle_packet = (struct libusb_transfer*) handle_packet->user_data;

        // handle transfer
        handle_completed_transfer(transfer);

        // handle case where libusb_close might be called by hci packet handler        
        if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED) return;
    }
    // log_info("end usb_process_ds");
}

static void usb_process_ts(btstack_timer_source_t *timer) {

    UNUSED(timer);

    // log_info("in usb_process_ts");

    // timer is deactive, when timer callback gets called
    usb_timer_active = 0;

    if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED) return;

    // actually handled the packet in the pollfds function
    usb_process_ds((struct btstack_data_source *) NULL, DATA_SOURCE_CALLBACK_READ);

    // Get the amount of time until next event is due
    long msec = ASYNC_POLLING_INTERVAL_MS;

    // Activate timer
    btstack_run_loop_set_timer(&usb_timer, msec);
    btstack_run_loop_add_timer(&usb_timer);
    usb_timer_active = 1;

    return;
}

#ifndef HAVE_USB_VENDOR_ID_AND_PRODUCT_ID

// list of known devices, using VendorID/ProductID tuples
static const uint16_t known_bluetooth_devices[] = {
    // BCM20702A0 - DeLOCK Bluetooth 4.0
    0x0a5c, 0x21e8,
    // BCM20702A0 - Asus BT400
    0x0b05, 0x17cb,
    // BCM20702B0 - Generic USB Detuned Class 1 @ 20 MHz
    0x0a5c, 0x22be,
    // nRF5x Zephyr USB HCI, e.g nRF52840-PCA10056
    0x2fe3, 0x0100,
    0x2fe3, 0x000b,
};

static int num_known_devices = sizeof(known_bluetooth_devices) / sizeof(uint16_t) / 2;

static int is_known_bt_device(uint16_t vendor_id, uint16_t product_id){
    int i;
    for (i=0; i<num_known_devices; i++){
        if (known_bluetooth_devices[i*2] == vendor_id && known_bluetooth_devices[i*2+1] == product_id){
            return 1;
        }
    }
    return 0;
}

static int scan_for_bt_endpoints(libusb_device *dev) {
    int r;

    event_in_addr = 0;
    acl_in_addr = 0;
    acl_out_addr = 0;
    sco_out_addr = 0;
    sco_in_addr = 0;

    // get endpoints from interface descriptor
    struct libusb_config_descriptor *config_descriptor;
    r = libusb_get_active_config_descriptor(dev, &config_descriptor);
    if (r < 0) return r;

    int num_interfaces = config_descriptor->bNumInterfaces;
    log_info("active configuration has %u interfaces", num_interfaces);

    int i;
    for (i = 0; i < num_interfaces ; i++){
        const struct libusb_interface *interface = &config_descriptor->interface[i];
        const struct libusb_interface_descriptor * interface_descriptor = interface->altsetting;
        log_info("interface %u: %u endpoints", i, interface_descriptor->bNumEndpoints);

        const struct libusb_endpoint_descriptor *endpoint = interface_descriptor->endpoint;

        for (r=0;r<interface_descriptor->bNumEndpoints;r++,endpoint++){
            log_info("- endpoint %x, attributes %x", endpoint->bEndpointAddress, endpoint->bmAttributes);

            switch (endpoint->bmAttributes & 0x3){
                case LIBUSB_TRANSFER_TYPE_INTERRUPT:
                    if (event_in_addr) continue;
                    event_in_addr = endpoint->bEndpointAddress;
                    log_info("-> using 0x%2.2X for HCI Events", event_in_addr);
                    break;
                case LIBUSB_TRANSFER_TYPE_BULK:
                    if (endpoint->bEndpointAddress & 0x80) {
                        if (acl_in_addr) continue;
                        acl_in_addr = endpoint->bEndpointAddress;
                        log_info("-> using 0x%2.2X for ACL Data In", acl_in_addr);
                    } else {
                        if (acl_out_addr) continue;
                        acl_out_addr = endpoint->bEndpointAddress;
                        log_info("-> using 0x%2.2X for ACL Data Out", acl_out_addr);
                    }
                    break;
                case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
                    if (endpoint->bEndpointAddress & 0x80) {
                        if (sco_in_addr) continue;
                        sco_in_addr = endpoint->bEndpointAddress;
                        log_info("-> using 0x%2.2X for SCO Data In", sco_in_addr);
                    } else {
                        if (sco_out_addr) continue;
                        sco_out_addr = endpoint->bEndpointAddress;
                        log_info("-> using 0x%2.2X for SCO Data Out", sco_out_addr);
                    }
                    break;
                default:
                    break;
            }
        }
    }
    libusb_free_config_descriptor(config_descriptor);
    return 0;
}

// returns index of found device or -1
static int scan_for_bt_device(libusb_device **devs, int start_index) {
    int i;
    for (i = start_index; devs[i] ; i++){
        libusb_device * dev = devs[i];
        int r = libusb_get_device_descriptor(dev, &desc);
        if (r < 0) {
            log_error("failed to get device descriptor");
            return 0;
        }
        
        log_info("%04x:%04x (bus %d, device %d) - class %x subclass %x protocol %x ",
               desc.idVendor, desc.idProduct,
               libusb_get_bus_number(dev), libusb_get_device_address(dev),
               desc.bDeviceClass, desc.bDeviceSubClass, desc.bDeviceProtocol);
        
        // Detect USB Dongle based Class, Subclass, and Protocol
        // The class code (bDeviceClass) is 0xE0 – Wireless Controller. 
        // The SubClass code (bDeviceSubClass) is 0x01 – RF Controller. 
        // The Protocol code (bDeviceProtocol) is 0x01 – Bluetooth programming.
        if (desc.bDeviceClass == 0xE0 && desc.bDeviceSubClass == 0x01 && desc.bDeviceProtocol == 0x01) {
            return i;
        }

        // Detect USB Dongle based on whitelist
        if (is_known_bt_device(desc.idVendor, desc.idProduct)) {
            return i;
        }
    }
    return -1;
}
#endif

static int prepare_device(libusb_device_handle * aHandle){

    // print device path
    uint8_t port_numbers[USB_MAX_PATH_LEN];
    libusb_device * device = libusb_get_device(aHandle);
    int path_len = libusb_get_port_numbers(device, port_numbers, USB_MAX_PATH_LEN);
    printf("USB Path: ");
    int i;
    for (i=0;i<path_len;i++){
        if (i) printf("-");
        printf("%02x", port_numbers[i]);
    }
    printf("\n");

    int r;
    int kernel_driver_detached = 0;

    // Detach OS driver (not possible for OS X, FreeBSD, and WIN32)
#if !defined(__APPLE__) && !defined(_WIN32) && !defined(__FreeBSD__)
    r = libusb_kernel_driver_active(aHandle, 0);
    if (r < 0) {
        log_error("libusb_kernel_driver_active error %d", r);
        libusb_close(aHandle);
        return r;
    }

    if (r == 1) {
        r = libusb_detach_kernel_driver(aHandle, 0);
        if (r < 0) {
            log_error("libusb_detach_kernel_driver error %d", r);
            libusb_close(aHandle);
            return r;
        }
        kernel_driver_detached = 1;
    }
    log_info("libusb_detach_kernel_driver");
#endif

    const int configuration = 1;
    log_info("setting configuration %d...", configuration);
    r = libusb_set_configuration(aHandle, configuration);
    if (r < 0) {
        log_error("Error libusb_set_configuration: %d", r);
        if (kernel_driver_detached){
            libusb_attach_kernel_driver(aHandle, 0);
        }
        libusb_close(aHandle);
        return r;
    }

    // reserve access to device
    log_info("claiming interface 0...");
    r = libusb_claim_interface(aHandle, 0);
    if (r < 0) {
        log_error("Error %d claiming interface 0", r);
        if (kernel_driver_detached){
            libusb_attach_kernel_driver(aHandle, 0);
        }
        libusb_close(aHandle);
        return r;
    }

#ifdef ENABLE_SCO_OVER_HCI
    // get endpoints from interface descriptor
    struct libusb_config_descriptor *config_descriptor;
    r = libusb_get_active_config_descriptor(device, &config_descriptor);
    if (r >= 0){
        int num_interfaces = config_descriptor->bNumInterfaces;
        if (num_interfaces > 1) {
            r = libusb_claim_interface(aHandle, 1);
            if (r < 0) {
                log_error("Error %d claiming interface 1: - disabling SCO over HCI", r);
            } else {
                sco_enabled = 1;
            }
        } else {
            log_info("Device has only on interface, disabling SCO over HCI");
        }
    }
#endif

    return 0;
}

static libusb_device_handle * try_open_device(libusb_device * device){
    int r;

    libusb_device_handle * dev_handle;
    r = libusb_open(device, &dev_handle);

    if (r < 0) {
        log_error("libusb_open failed!");
        dev_handle = NULL;
        return NULL;
    }

    log_info("libusb open %d, handle %p", r, dev_handle);

    // reset device (Not currently possible under FreeBSD 11.x/12.x due to usb framework)
#if !defined(__FreeBSD__)
    r = libusb_reset_device(dev_handle);
    if (r < 0) {
        log_error("libusb_reset_device failed!");
        libusb_close(dev_handle);
        return NULL;
    }
#endif
    return dev_handle;
}

#ifdef ENABLE_SCO_OVER_HCI

static int usb_sco_start(void){

    printf("usb_sco_start\n");
    log_info("usb_sco_start");

    sco_state_machine_init();
    sco_ring_init();

    int alt_setting;
    if (sco_voice_setting & 0x0020){
        // 16-bit PCM  
        alt_setting = alt_setting_16_bit[sco_num_connections-1];
    } else {
        // 8-bit PCM or mSBC
        alt_setting = alt_setting_8_bit[sco_num_connections-1];
    }
    // derive iso packet size from alt setting
    iso_packet_size = iso_packet_size_for_alt_setting[alt_setting];

    log_info("Switching to setting %u on interface 1..", alt_setting);
    int r = libusb_set_interface_alt_setting(handle, 1, alt_setting); 
    if (r < 0) {
        log_error("Error setting alternative setting %u for interface 1: %s\n", alt_setting, libusb_error_name(r));
        return r;
    }

    // incoming
    int c;
    for (c = 0 ; c < SCO_IN_BUFFER_COUNT ; c++) {
        sco_in_transfer[c] = libusb_alloc_transfer(NUM_ISO_PACKETS); // isochronous transfers SCO in
        if (!sco_in_transfer[c]) {
            usb_close();
            return LIBUSB_ERROR_NO_MEM;
        }
        // configure sco_in handlers
        libusb_fill_iso_transfer(sco_in_transfer[c], handle, sco_in_addr, 
            hci_sco_in_buffer[c], NUM_ISO_PACKETS * iso_packet_size, NUM_ISO_PACKETS, async_callback, NULL, 0);
        libusb_set_iso_packet_lengths(sco_in_transfer[c], iso_packet_size);
        r = libusb_submit_transfer(sco_in_transfer[c]);
        if (r) {
            log_error("Error submitting isochronous in transfer %d", r);
            usb_close();
            return r;
        }
    }

    // outgoing
    for (c=0; c < SCO_OUT_BUFFER_COUNT ; c++){
        sco_out_transfers[c] = libusb_alloc_transfer(NUM_ISO_PACKETS); // 1 isochronous transfers SCO out - up to 3 parts
        sco_out_transfers_in_flight[c] = 0;
    }
    return 0;
}

static void usb_sco_stop(void){

    printf("usb_sco_stop\n");

    log_info("usb_sco_stop");
    sco_shutdown = 1;

    // Free SCO transfers already in queue
    struct libusb_transfer* transfer = handle_packet;
    struct libusb_transfer* prev_transfer = NULL;
    while (transfer != NULL) {
        uint16_t c;
        bool drop_transfer = false;
        for (c=0;c<SCO_IN_BUFFER_COUNT;c++){
            if (transfer == sco_in_transfer[c]){
                sco_in_transfer[c] = NULL;
                drop_transfer = true;
                break;
            }
        }
        for (c=0;c<SCO_OUT_BUFFER_COUNT;c++){
            if (transfer == sco_out_transfers[c]){
                sco_out_transfers_in_flight[c] = 0;
                sco_out_transfers[c] = NULL;
                drop_transfer = true;
                break;
            }
        }
        struct libusb_transfer * next_transfer = (struct libusb_transfer *) transfer->user_data;
        if (drop_transfer) {
            printf("Drop completed SCO transfer %p\n", transfer);
            if (prev_transfer == NULL) {
                // first item
                handle_packet = (struct libusb_transfer *) next_transfer;
            } else {
                // other item
                prev_transfer->user_data = (struct libusb_transfer *) next_transfer;
            }
            libusb_free_transfer(transfer);
        } else {
            prev_transfer = transfer;
        }
        transfer = next_transfer;
    }

    libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_ERROR);

    int c;
    for (c = 0 ; c < SCO_IN_BUFFER_COUNT ; c++) {
        if (sco_in_transfer[c] != NULL) {
            libusb_cancel_transfer(sco_in_transfer[c]);
        }
    }

    for (c = 0; c < SCO_OUT_BUFFER_COUNT ; c++){
        if (sco_out_transfers_in_flight[c]) {
            libusb_cancel_transfer(sco_out_transfers[c]);
        } else {
            if (sco_out_transfers[c] != NULL) {
                libusb_free_transfer(sco_out_transfers[c]);
                sco_out_transfers[c] = 0;
            }
        }
    }

    // wait until all transfers are completed
    int completed = 0;
    while (!completed){
        struct timeval tv;
        memset(&tv, 0, sizeof(struct timeval));
        libusb_handle_events_timeout(NULL, &tv);
        // check if all done
        completed = 1;

        // Cancel all synchronous transfer
        for (c = 0 ; c < SCO_IN_BUFFER_COUNT ; c++) {
            if (sco_in_transfer[c] != NULL){
                completed = 0;
                break;
            }
        }

        if (!completed) continue;

        for (c=0; c < SCO_OUT_BUFFER_COUNT ; c++){
            if (sco_out_transfers[c] != NULL){
                completed = 0;
                break;
            }
        }
    }
    sco_shutdown = 0;
    libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_WARNING);

    log_info("Switching to setting %u on interface 1..", 0);
    int r = libusb_set_interface_alt_setting(handle, 1, 0); 
    if (r < 0) {
        log_error("Error setting alternative setting %u for interface 1: %s", 0, libusb_error_name(r));
        return;
    }

    printf("usb_sco_stop done\n");
}



#endif

static int usb_open(void){
    int r;

    if (usb_transport_open) return 0;

    handle_packet = NULL;

    // default endpoint addresses
    event_in_addr = 0x81; // EP1, IN interrupt
    acl_in_addr =   0x82; // EP2, IN bulk
    acl_out_addr =  0x02; // EP2, OUT bulk
    sco_in_addr  =  0x83; // EP3, IN isochronous
    sco_out_addr =  0x03; // EP3, OUT isochronous    

    // USB init
    r = libusb_init(NULL);
    if (r < 0) return -1;

    libusb_state = LIB_USB_OPENED;

    // configure debug level
    libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_WARNING);
    
    libusb_device * dev = NULL;

#ifdef HAVE_USB_VENDOR_ID_AND_PRODUCT_ID

    // Use a specified device
    log_info("Want vend: %04x, prod: %04x", USB_VENDOR_ID, USB_PRODUCT_ID);
    handle = libusb_open_device_with_vid_pid(NULL, USB_VENDOR_ID, USB_PRODUCT_ID);

    if (!handle){
        log_error("libusb_open_device_with_vid_pid failed!");
        usb_close();
        return -1;
    }
    log_info("libusb open %d, handle %p", r, handle);

    r = prepare_device(handle);
    if (r < 0){
        usb_close();
        return -1;
    }

    dev = libusb_get_device(aHandle);
    r = scan_for_bt_endpoints(dev);
    if (r < 0){
        usb_close();
        return -1;
    }

#else
    // Scan system for an appropriate devices
    libusb_device **devs;
    ssize_t num_devices;

    log_info("Scanning for USB Bluetooth device");
    num_devices = libusb_get_device_list(NULL, &devs);
    if (num_devices < 0) {
        usb_close();
        return -1;
    }

    if (usb_path_len){
        int i;
        for (i=0;i<num_devices;i++){
            uint8_t port_numbers[USB_MAX_PATH_LEN];
            int len = libusb_get_port_numbers(devs[i], port_numbers, USB_MAX_PATH_LEN);
            if (len != usb_path_len) continue;
            if (memcmp(usb_path, port_numbers, len) == 0){
                log_info("USB device found at specified path");
                handle = try_open_device(devs[i]);
                if (!handle) continue;

                r = prepare_device(handle);
                if (r < 0) {
                    handle = NULL;
                    continue;
                }

                dev = devs[i];
                r = scan_for_bt_endpoints(dev);
                if (r < 0) {
                    handle = NULL;
                    continue;
                }

                libusb_state = LIB_USB_INTERFACE_CLAIMED;
                break;
            };
        }
        if (!handle){
            log_error("USB device with given path not found");
            printf("USB device with given path not found\n");
            return -1;
        }
    } else {

        int deviceIndex = -1;
        while (true){
            // look for next Bluetooth dongle
            deviceIndex = scan_for_bt_device(devs, deviceIndex+1);
            if (deviceIndex < 0) break;

            log_info("USB Bluetooth device found, index %u", deviceIndex);

            handle = try_open_device(devs[deviceIndex]);
            if (!handle) continue;

            r = prepare_device(handle);
            if (r < 0) {
                handle = NULL;
                continue;
            }

            dev = devs[deviceIndex];
            r = scan_for_bt_endpoints(dev);
            if (r < 0) {
                handle = NULL;
                continue;
            }

            libusb_state = LIB_USB_INTERFACE_CLAIMED;
            break;
        }
    }

    libusb_free_device_list(devs, 1);

    if (handle == 0){
        log_error("No USB Bluetooth device found");
        return -1;
    }

#endif
    
    // allocate transfer handlers
    int c;
    for (c = 0 ; c < EVENT_IN_BUFFER_COUNT ; c++) {
        event_in_transfer[c] = libusb_alloc_transfer(0); // 0 isochronous transfers Events
        if (!event_in_transfer[c]) {
            usb_close();
            return LIBUSB_ERROR_NO_MEM;
        }
    }
    for (c = 0 ; c < ACL_IN_BUFFER_COUNT ; c++) {
        acl_in_transfer[c]  =  libusb_alloc_transfer(0); // 0 isochronous transfers ACL in
        if (!acl_in_transfer[c]) {
            usb_close();
            return LIBUSB_ERROR_NO_MEM;
        }
    }

    command_out_transfer = libusb_alloc_transfer(0);
    acl_out_transfer     = libusb_alloc_transfer(0);

    // TODO check for error

    libusb_state = LIB_USB_TRANSFERS_ALLOCATED;

    for (c = 0 ; c < EVENT_IN_BUFFER_COUNT ; c++) {
        // configure event_in handlers
        libusb_fill_interrupt_transfer(event_in_transfer[c], handle, event_in_addr, 
                hci_event_in_buffer[c], HCI_ACL_BUFFER_SIZE, async_callback, NULL, 0) ;
        r = libusb_submit_transfer(event_in_transfer[c]);
        if (r) {
            log_error("Error submitting interrupt transfer %d", r);
            usb_close();
            return r;
        }
    }

    for (c = 0 ; c < ACL_IN_BUFFER_COUNT ; c++) {
        // configure acl_in handlers
        libusb_fill_bulk_transfer(acl_in_transfer[c], handle, acl_in_addr, 
                hci_acl_in_buffer[c] + HCI_INCOMING_PRE_BUFFER_SIZE, HCI_ACL_BUFFER_SIZE, async_callback, NULL, 0) ;
        r = libusb_submit_transfer(acl_in_transfer[c]);
        if (r) {
            log_error("Error submitting bulk in transfer %d", r);
            usb_close();
            return r;
        }
 
     }

 #if 0
    // Check for pollfds functionality
    doing_pollfds = libusb_pollfds_handle_timeouts(NULL);
#else
    // NOTE: using pollfds doesn't work on Linux, so it is disable until further investigation
    doing_pollfds = 0;
#endif

    if (doing_pollfds) {
        log_info("Async using pollfds:");

        const struct libusb_pollfd ** pollfd = libusb_get_pollfds(NULL);
        for (num_pollfds = 0 ; pollfd[num_pollfds] ; num_pollfds++);
        pollfd_data_sources = (btstack_data_source_t *)malloc(sizeof(btstack_data_source_t) * num_pollfds);
        if (!pollfd_data_sources){
            log_error("Cannot allocate data sources for pollfds");
            usb_close();
            return 1;            
        }
        memset(pollfd_data_sources, 0, sizeof(btstack_data_source_t) * num_pollfds);
        for (r = 0 ; r < num_pollfds ; r++) {
            btstack_data_source_t *ds = &pollfd_data_sources[r];
            btstack_run_loop_set_data_source_fd(ds, pollfd[r]->fd);
            btstack_run_loop_set_data_source_handler(ds, &usb_process_ds);
            btstack_run_loop_enable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_READ);
            btstack_run_loop_add_data_source(ds);
            log_info("%u: %p fd: %u, events %x", r, pollfd[r], pollfd[r]->fd, pollfd[r]->events);
        }
        free(pollfd);
    } else {
        log_info("Async using timers:");

        usb_timer.process = usb_process_ts;
        btstack_run_loop_set_timer(&usb_timer, ASYNC_POLLING_INTERVAL_MS);
        btstack_run_loop_add_timer(&usb_timer);
        usb_timer_active = 1;
    }

    usb_transport_open = 1;

    return 0;
}

static int usb_close(void){
    int c;
    int completed = 0;

    if (!usb_transport_open) return 0;

    log_info("usb_close");

    switch (libusb_state){
        case LIB_USB_CLOSED:
            break;

        case LIB_USB_TRANSFERS_ALLOCATED:
            libusb_state = LIB_USB_INTERFACE_CLAIMED;

            if(usb_timer_active) {
                btstack_run_loop_remove_timer(&usb_timer);
                usb_timer_active = 0;
            }

            if (doing_pollfds){
                int r;
                for (r = 0 ; r < num_pollfds ; r++) {
                    btstack_data_source_t *ds = &pollfd_data_sources[r];
                    btstack_run_loop_remove_data_source(ds);
                }
                free(pollfd_data_sources);
                pollfd_data_sources = NULL;
                num_pollfds = 0;
                doing_pollfds = 0;
            }

            /* fall through */

        case LIB_USB_INTERFACE_CLAIMED:
            // Cancel all transfers, ignore warnings for this
            libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_ERROR);
            for (c = 0 ; c < EVENT_IN_BUFFER_COUNT ; c++) {
                if (event_in_transfer[c]){
                    log_info("cancel event_in_transfer[%u] = %p", c, event_in_transfer[c]);
                    libusb_cancel_transfer(event_in_transfer[c]);
                }
            }
            for (c = 0 ; c < ACL_IN_BUFFER_COUNT ; c++) {
                if (acl_in_transfer[c]){
                    log_info("cancel acl_in_transfer[%u] = %p", c, acl_in_transfer[c]);
                    libusb_cancel_transfer(acl_in_transfer[c]);
                }
            }
#ifdef ENABLE_SCO_OVER_HCI
            for (c = 0 ; c < SCO_IN_BUFFER_COUNT ; c++) {
                if (sco_in_transfer[c]){
                    log_info("cancel sco_in_transfer[%u] = %p", c, sco_in_transfer[c]);
                    libusb_cancel_transfer(sco_in_transfer[c]);
                }
            }
            for (c = 0; c < SCO_OUT_BUFFER_COUNT ; c++){
                if (sco_out_transfers_in_flight[c]) {
                    log_info("cancel sco_out_transfers[%u] = %p", c, sco_out_transfers[c]);
                    libusb_cancel_transfer(sco_out_transfers[c]);
                } else {
                    if (sco_out_transfers[c] != NULL){
                        libusb_free_transfer(sco_out_transfers[c]);
                        sco_out_transfers[c] = 0;
                    }
                }
            }
#endif
            libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_WARNING);

            // wait until all transfers are completed - or 20 iterations
            int countdown = 20;
            while (!completed){

                if (--countdown == 0){
                    log_info("Not all transfers cancelled, leaking a bit.");
                    break;
                }

                struct timeval tv;
                memset(&tv, 0, sizeof(struct timeval));
                libusb_handle_events_timeout(NULL, &tv);
                // check if all done
                completed = 1;
                for (c=0;c<EVENT_IN_BUFFER_COUNT;c++){
                    if (event_in_transfer[c]) {
                        log_info("event_in_transfer[%u] still active (%p)", c, event_in_transfer[c]);
                        completed = 0;
                        break;
                    }
                }

                if (!completed) continue;

                for (c=0;c<ACL_IN_BUFFER_COUNT;c++){
                    if (acl_in_transfer[c]) {
                        log_info("acl_in_transfer[%u] still active (%p)", c, acl_in_transfer[c]);
                        completed = 0;
                        break;
                    }
                }

#ifdef ENABLE_SCO_OVER_HCI
                if (!completed) continue;

                // Cancel all synchronous transfer
                for (c = 0 ; c < SCO_IN_BUFFER_COUNT ; c++) {
                    if (sco_in_transfer[c]){
                        log_info("sco_in_transfer[%u] still active (%p)", c, sco_in_transfer[c]);
                        completed = 0;
                        break;
                    }
                }

                if (!completed) continue;

                for (c=0; c < SCO_OUT_BUFFER_COUNT ; c++){
                    if (sco_out_transfers[c] != NULL){
                        log_info("sco_out_transfers[%u] still active (%p)", c, sco_out_transfers[c]);
                        completed = 0;
                        break;
                    }
                }
                sco_enabled = 0;
#endif
            }

            // finally release interface
            libusb_release_interface(handle, 0);
#ifdef ENABLE_SCO_OVER_HCI
            libusb_release_interface(handle, 1);
#endif
            log_info("Libusb shutdown complete");

			/* fall through */

        case LIB_USB_DEVICE_OPENDED:
            libusb_close(handle);

			/* fall through */

        case LIB_USB_OPENED:
            libusb_exit(NULL);
            break;

		default:
			btstack_assert(false);
			break;
    }

    libusb_state = LIB_USB_CLOSED;
    handle = NULL;
    usb_transport_open = 0;

    return 0;
}

static int usb_send_cmd_packet(uint8_t *packet, int size){
    int r;

    if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED) return -1;

    // async
    libusb_fill_control_setup(hci_cmd_buffer, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE, 0, 0, 0, size);
    memcpy(hci_cmd_buffer + LIBUSB_CONTROL_SETUP_SIZE, packet, size);

    // prepare transfer
    int completed = 0;
    libusb_fill_control_transfer(command_out_transfer, handle, hci_cmd_buffer, async_callback, &completed, 0);
    command_out_transfer->flags = LIBUSB_TRANSFER_FREE_BUFFER;

    // update stata before submitting transfer
    usb_command_active = 1;

    // submit transfer
    r = libusb_submit_transfer(command_out_transfer);

    if (r < 0) {
        usb_command_active = 0;
        log_error("Error submitting cmd transfer %d", r);
        return -1;
    }

    return 0;
}

static int usb_send_acl_packet(uint8_t *packet, int size){
    int r;

    if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED) return -1;

    // log_info("usb_send_acl_packet enter, size %u", size);

    // prepare transfer
    int completed = 0;
    libusb_fill_bulk_transfer(acl_out_transfer, handle, acl_out_addr, packet, size,
        async_callback, &completed, 0);
    acl_out_transfer->type = LIBUSB_TRANSFER_TYPE_BULK;

    // update stata before submitting transfer
    usb_acl_out_active = 1;

    r = libusb_submit_transfer(acl_out_transfer);
    if (r < 0) {
        usb_acl_out_active = 0;
        log_error("Error submitting acl transfer, %d", r);
        return -1;
    }

    return 0;
}

static int usb_can_send_packet_now(uint8_t packet_type){
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
            return !usb_command_active;
        case HCI_ACL_DATA_PACKET:
            return !usb_acl_out_active;
#ifdef ENABLE_SCO_OVER_HCI
        case HCI_SCO_DATA_PACKET:
            if (!sco_enabled) return 0;
            return sco_ring_have_space();
#endif
        default:
            return 0;
    }
}

static int usb_send_packet(uint8_t packet_type, uint8_t * packet, int size){
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
            return usb_send_cmd_packet(packet, size);
        case HCI_ACL_DATA_PACKET:
            return usb_send_acl_packet(packet, size);
#ifdef ENABLE_SCO_OVER_HCI
        case HCI_SCO_DATA_PACKET:
            if (!sco_enabled) return -1;
            return usb_send_sco_packet(packet, size);
#endif
        default:
            return -1;
    }
}

#ifdef ENABLE_SCO_OVER_HCI
static void usb_set_sco_config(uint16_t voice_setting, int num_connections){
    if (!sco_enabled) return;

    log_info("usb_set_sco_config: voice settings 0x%04x, num connections %u", voice_setting, num_connections);

    if (num_connections != sco_num_connections){
        sco_voice_setting = voice_setting;
        if (sco_num_connections){
            usb_sco_stop();
        }
        sco_num_connections = num_connections;
        if (num_connections){
            usb_sco_start();
        }
    }
}
#endif

static void usb_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    log_info("registering packet handler");
    packet_handler = handler;
}

static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(packet);
    UNUSED(size);
}

// get usb singleton
const hci_transport_t * hci_transport_usb_instance(void) {
    if (!hci_transport_usb) {
        hci_transport_usb = (hci_transport_t*) malloc( sizeof(hci_transport_t));
        memset(hci_transport_usb, 0, sizeof(hci_transport_t));
        hci_transport_usb->name                          = "H2_LIBUSB";
        hci_transport_usb->open                          = usb_open;
        hci_transport_usb->close                         = usb_close;
        hci_transport_usb->register_packet_handler       = usb_register_packet_handler;
        hci_transport_usb->can_send_packet_now           = usb_can_send_packet_now;
        hci_transport_usb->send_packet                   = usb_send_packet;
#ifdef ENABLE_SCO_OVER_HCI
        hci_transport_usb->set_sco_config                = usb_set_sco_config;
#endif
    }
    return hci_transport_usb;
}
