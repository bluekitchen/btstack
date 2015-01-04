/*
 * Copyright (C) 2009-2012 by Matthias Ringwald
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
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
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
 * Please inquire about commercial licensing options at btstack@ringwald.ch
 *
 */

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

#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>   /* UNIX standard function definitions */
#include <sys/types.h>

#include <libusb.h>

#include "btstack-config.h"

#include "debug.h"
#include "hci.h"
#include "hci_transport.h"

#if (USB_VENDOR_ID != 0) && (USB_PRODUCT_ID != 0)
#define HAVE_USB_VENDOR_ID_AND_PRODUCT_ID
#endif

// prototypes
static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size); 
static int usb_close(void *transport_config);
    
typedef enum {
    LIB_USB_CLOSED = 0,
    LIB_USB_OPENED,
    LIB_USB_DEVICE_OPENDED,
    LIB_USB_KERNEL_DETACHED,
    LIB_USB_INTERFACE_CLAIMED,
    LIB_USB_TRANSFERS_ALLOCATED
} libusb_state_t;

static libusb_state_t libusb_state = LIB_USB_CLOSED;

// single instance
static hci_transport_t * hci_transport_usb = NULL;

static void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = dummy_handler;

// libusb
#ifndef HAVE_USB_VENDOR_ID_AND_PRODUCT_ID
static struct libusb_device_descriptor desc;
static libusb_device        * dev;
#endif
static libusb_device_handle * handle;

#define ASYNC_BUFFERS 20
#define AYSNC_POLLING_INTERVAL_MS 1

static struct libusb_transfer *event_in_transfer[ASYNC_BUFFERS];
static struct libusb_transfer *bulk_in_transfer[ASYNC_BUFFERS];
static struct libusb_transfer *bulk_out_transfer;
static struct libusb_transfer *command_out_transfer;


static uint8_t hci_event_in_buffer[ASYNC_BUFFERS][HCI_ACL_BUFFER_SIZE]; // bigger than largest packet
static uint8_t hci_bulk_in_buffer[ASYNC_BUFFERS][HCI_INCOMING_PRE_BUFFER_SIZE + HCI_ACL_BUFFER_SIZE]; 
static uint8_t hci_control_buffer[3 + 256 + LIBUSB_CONTROL_SETUP_SIZE];

// For (ab)use as a linked list of received packets
static struct libusb_transfer *handle_packet;

static int doing_pollfds;
static int num_pollfds;
static data_source_t * pollfd_data_sources;
static timer_source_t usb_timer;
static int usb_timer_active;

static int usb_acl_out_active = 0;
static int usb_command_active = 0;

// endpoint addresses
static int event_in_addr;
static int acl_in_addr;
static int acl_out_addr;

#ifndef HAVE_USB_VENDOR_ID_AND_PRODUCT_ID

// list of known devices, using VendorID/ProductID tuples
static uint16_t known_bt_devices[] = {
    // DeLOCK Bluetooth 4.0
    0x0a5c, 0x21e8,
    // Asus BT400
    0x0b05, 0x17cb,
};
static int num_known_devices = sizeof(known_bt_devices) / sizeof(uint16_t) / 2;

static int is_known_bt_device(uint16_t vendor_id, uint16_t product_id){
    int i;
    for (i=0; i<num_known_devices; i++){
        if (known_bt_devices[i*2] == vendor_id && known_bt_devices[i*2+1] == product_id){
            return 1;
        }
    }
    return 0;
}

static void scan_for_bt_endpoints(void) {
    int r;

    // get endpoints from interface descriptor
    struct libusb_config_descriptor *config_descriptor;
    r = libusb_get_active_config_descriptor(dev, &config_descriptor);
    log_info("configuration: %u interfaces", config_descriptor->bNumInterfaces);

    const struct libusb_interface *interface = config_descriptor->interface;
    const struct libusb_interface_descriptor * interface0descriptor = interface->altsetting;
    log_info("interface 0: %u endpoints", interface0descriptor->bNumEndpoints);

    const struct libusb_endpoint_descriptor *endpoint = interface0descriptor->endpoint;

    for (r=0;r<interface0descriptor->bNumEndpoints;r++,endpoint++){
        log_info("endpoint %x, attributes %x", endpoint->bEndpointAddress, endpoint->bmAttributes);

        if ((endpoint->bmAttributes & 0x3) == LIBUSB_TRANSFER_TYPE_INTERRUPT){
            event_in_addr = endpoint->bEndpointAddress;
            log_info("Using 0x%2.2X for HCI Events", event_in_addr);
        }
        if ((endpoint->bmAttributes & 0x3) == LIBUSB_TRANSFER_TYPE_BULK){
            if (endpoint->bEndpointAddress & 0x80) {
                acl_in_addr = endpoint->bEndpointAddress;
                log_info("Using 0x%2.2X for ACL Data In", acl_in_addr);
            } else {
                acl_out_addr = endpoint->bEndpointAddress;
                log_info("Using 0x%2.2X for ACL Data Out", acl_out_addr);
            }
        }
    }
    libusb_free_config_descriptor(config_descriptor);
}

static libusb_device * scan_for_bt_device(libusb_device **devs) {
    int i = 0;
    while ((dev = devs[i++]) != NULL) {
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
        // if (desc.bDeviceClass == 0xe0 && desc.bDeviceSubClass == 0x01 && desc.bDeviceProtocol == 0x01){
        if (desc.bDeviceClass == 0xE0 && desc.bDeviceSubClass == 0x01 && desc.bDeviceProtocol == 0x01) {
            return dev;
        }

        // Detect USB Dongle based on whitelist
        if (is_known_bt_device(desc.idVendor, desc.idProduct)) return dev;
    }
    return NULL;
}
#endif

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

static void async_callback(struct libusb_transfer *transfer)
{
    if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED)  return;
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
        log_info("async_callback resubmit transfer, endpoint %x, status %x, length %u", transfer->endpoint, transfer->status, transfer->actual_length);
        // No usable data, just resubmit packet
        r = libusb_submit_transfer(transfer);
        if (r) {
            log_error("Error re-submitting transfer %d", r);
        }
    }
    // log_info("end async_callback");
}

static void handle_completed_transfer(struct libusb_transfer *transfer){

    int r;

    int resubmit = 0;

    if (transfer->endpoint == event_in_addr) {
        packet_handler(HCI_EVENT_PACKET, transfer-> buffer, transfer->actual_length);
        resubmit = 1;
    }

    else if (transfer->endpoint == acl_in_addr) {
        // log_info("-> acl");
        packet_handler(HCI_ACL_DATA_PACKET, transfer-> buffer, transfer->actual_length);

        resubmit = 1;
    } else if (transfer->endpoint == acl_out_addr){
        // log_info("acl out done, size %u", transfer->actual_length);
        usb_acl_out_active = 0;

        // notify upper stack that iit might be possible to send again
        uint8_t event[] = { DAEMON_EVENT_HCI_PACKET_SENT, 0};
        packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));

        resubmit = 0;
    } else if (transfer->endpoint == 0){
        // log_info("command done, size %u", transfer->actual_length);
        usb_command_active = 0;

        // notify upper stack that it might be possible to send again
        uint8_t event[] = { DAEMON_EVENT_HCI_PACKET_SENT, 0};
        packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));
        
        resubmit = 0;
    } else {

        log_info("usb_process_ds endpoint unknown %x", transfer->endpoint);
    }

    if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED) return;

    if (resubmit){
        // Re-submit transfer 
        transfer->user_data = NULL;
        r = libusb_submit_transfer(transfer);

        if (r) {
            log_error("Error re-submitting transfer %d", r);
        }
    }   
}

static int usb_process_ds(struct data_source *ds) {

    if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED) return -1;

    // log_info("begin usb_process_ds");

    // always handling an event as we're called when data is ready
    struct timeval tv;
    memset(&tv, 0, sizeof(struct timeval));
    libusb_handle_events_timeout(NULL, &tv);

    // Handle any packet in the order that they were received
    while (handle_packet) {

        // log_info("handle packet %p, endpoint %x, status %x", handle_packet, handle_packet->endpoint, handle_packet->status);

        void * next = handle_packet->user_data;

        handle_completed_transfer(handle_packet);

        // handle case where libusb_close might be called by hci packet handler        
        if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED) return -1;

        // Move to next in the list of packets to handle
        if (next) {
            handle_packet = (struct libusb_transfer*)next;
        } else {
            handle_packet = NULL;
        }
    }

    // log_info("end usb_process_ds");

    return 0;
}

void usb_process_ts(timer_source_t *timer) {
    // log_info("in usb_process_ts");

    // timer is deactive, when timer callback gets called
    usb_timer_active = 0;

    if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED) return;

    // actually handled the packet in the pollfds function
    usb_process_ds((struct data_source *) NULL);

    // Get the amount of time until next event is due
    long msec = AYSNC_POLLING_INTERVAL_MS;

    // Activate timer
    run_loop_set_timer(&usb_timer, msec);
    run_loop_add_timer(&usb_timer);
    usb_timer_active = 1;

    return;
}

static int usb_open(void *transport_config){
    int r,c;
#ifndef HAVE_USB_VENDOR_ID_AND_PRODUCT_ID
    libusb_device * aDev;
    libusb_device **devs;
    ssize_t cnt;
#endif

    handle_packet = NULL;

    // default endpoint addresses
    event_in_addr = 0x81; // EP1, IN interrupt
    acl_in_addr =   0x82; // EP2, IN bulk
    acl_out_addr =  0x02; // EP2, OUT bulk

    // USB init
    r = libusb_init(NULL);
    if (r < 0) return -1;

    libusb_state = LIB_USB_OPENED;

    // configure debug level
    libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_WARNING);
    
#ifdef HAVE_USB_VENDOR_ID_AND_PRODUCT_ID
    // Use a specified device
    log_info("Want vend: %04x, prod: %04x", USB_VENDOR_ID, USB_PRODUCT_ID);
    handle = libusb_open_device_with_vid_pid(NULL, USB_VENDOR_ID, USB_PRODUCT_ID);

    if (!handle){
        log_error("libusb_open_device_with_vid_pid failed!");
        usb_close(handle);
        return -1;
    }
#else
    // Scan system for an appropriate device
    log_info("Scanning for USB Bluetooth device");
    cnt = libusb_get_device_list(NULL, &devs);
    if (cnt < 0) {
        usb_close(handle);
        return -1;
    }
    // Find BT modul
    aDev  = scan_for_bt_device(devs);
    if (!aDev){
        log_error("No USB Bluetooth device found");
        libusb_free_device_list(devs, 1);
        usb_close(handle);
        return -1;
    }
    log_info("USB Bluetooth device found");
    
    dev = aDev;
    r = libusb_open(dev, &handle);

    // reset device
    libusb_reset_device(handle);

    libusb_free_device_list(devs, 1);

    if (r < 0) {
        usb_close(handle);
        return r;
    }
#endif

    log_info("libusb open %d, handle %p", r, handle);

    // Detach OS driver (not possible for OS X and WIN32)
#if !defined(__APPLE__) && !defined(_WIN32)
    r = libusb_kernel_driver_active(handle, 0);
    if (r < 0) {
        log_error("libusb_kernel_driver_active error %d", r);
        usb_close(handle);
        return r;
    }

    if (r == 1) {
        r = libusb_detach_kernel_driver(handle, 0);
        if (r < 0) {
            log_error("libusb_detach_kernel_driver error %d", r);
            usb_close(handle);
            return r;
        }
    }
    log_info("libusb_detach_kernel_driver");
#endif
    libusb_state = LIB_USB_KERNEL_DETACHED;

    const int configuration = 1;
    log_info("setting configuration %d...", configuration);
    r = libusb_set_configuration(handle, configuration);
    if (r < 0) {
        log_error("Error libusb_set_configuration: %d\n", r);
        usb_close(handle);
        return r;
    }

    // reserve access to device
    log_info("claiming interface 0...");
    r = libusb_claim_interface(handle, 0);
    if (r < 0) {
        log_error("Error claiming interface %d", r);
        usb_close(handle);
        return r;
    }

    libusb_state = LIB_USB_INTERFACE_CLAIMED;
    
#ifndef HAVE_USB_VENDOR_ID_AND_PRODUCT_ID
    scan_for_bt_endpoints();
#endif

    // allocate transfer handlers
    for (c = 0 ; c < ASYNC_BUFFERS ; c++) {
        event_in_transfer[c] = libusb_alloc_transfer(0); // 0 isochronous transfers Events
        bulk_in_transfer[c]  = libusb_alloc_transfer(0); // 0 isochronous transfers ACL in
 
        if ( !event_in_transfer[c] || !bulk_in_transfer[c] ) {
            usb_close(handle);
            return LIBUSB_ERROR_NO_MEM;
        }
    }

    bulk_out_transfer = libusb_alloc_transfer(0);
    command_out_transfer = libusb_alloc_transfer(0);

    // TODO check for error

    libusb_state = LIB_USB_TRANSFERS_ALLOCATED;

    for (c = 0 ; c < ASYNC_BUFFERS ; c++) {
        // configure event_in handlers
        libusb_fill_interrupt_transfer(event_in_transfer[c], handle, event_in_addr, 
                hci_event_in_buffer[c], HCI_ACL_BUFFER_SIZE, async_callback, NULL, 0) ;
 
        r = libusb_submit_transfer(event_in_transfer[c]);
        if (r) {
            log_error("Error submitting interrupt transfer %d", r);
            usb_close(handle);
            return r;
        }
 
        // configure bulk_in handlers
        libusb_fill_bulk_transfer(bulk_in_transfer[c], handle, acl_in_addr, 
                hci_bulk_in_buffer[c] + HCI_INCOMING_PRE_BUFFER_SIZE, HCI_ACL_BUFFER_SIZE, async_callback, NULL, 0) ;
 
        r = libusb_submit_transfer(bulk_in_transfer[c]);
        if (r) {
            log_error("Error submitting bulk in transfer %d", r);
            usb_close(handle);
            return r;
        }
    }

    // Check for pollfds functionality
    doing_pollfds = libusb_pollfds_handle_timeouts(NULL);
    
    // NOTE: using pollfds doesn't work on Linux, so it is disable until further investigation here
    doing_pollfds = 0;

    if (doing_pollfds) {
        log_info("Async using pollfds:");

        const struct libusb_pollfd ** pollfd = libusb_get_pollfds(NULL);
        for (num_pollfds = 0 ; pollfd[num_pollfds] ; num_pollfds++);
        pollfd_data_sources = malloc(sizeof(data_source_t) * num_pollfds);
        if (!pollfd_data_sources){
            log_error("Cannot allocate data sources for pollfds");
            usb_close(handle);
            return 1;            
        }
        for (r = 0 ; r < num_pollfds ; r++) {
            data_source_t *ds = &pollfd_data_sources[r];
            ds->fd = pollfd[r]->fd;
            ds->process = usb_process_ds;
            run_loop_add_data_source(ds);
            log_info("%u: %p fd: %u, events %x", r, pollfd[r], pollfd[r]->fd, pollfd[r]->events);
        }
        free(pollfd);
    } else {
        log_info("Async using timers:");

        usb_timer.process = usb_process_ts;
        run_loop_set_timer(&usb_timer, AYSNC_POLLING_INTERVAL_MS);
        run_loop_add_timer(&usb_timer);
        usb_timer_active = 1;
    }

    return 0;
}
static int usb_close(void *transport_config){
    int c;
    // @TODO: remove all run loops!

    switch (libusb_state){
        case LIB_USB_CLOSED:
            break;

        case LIB_USB_TRANSFERS_ALLOCATED:
            libusb_state = LIB_USB_INTERFACE_CLAIMED;

            if(usb_timer_active) {
                run_loop_remove_timer(&usb_timer);
                usb_timer_active = 0;
            }

            // Cancel any asynchronous transfers
            for (c = 0 ; c < ASYNC_BUFFERS ; c++) {
                libusb_cancel_transfer(event_in_transfer[c]);
                libusb_cancel_transfer(bulk_in_transfer[c]);
            }

            /* TODO - find a better way to ensure that all transfers have completed */
            struct timeval tv;
            memset(&tv, 0, sizeof(struct timeval));
            libusb_handle_events_timeout(NULL, &tv);

            if (doing_pollfds){
                int r;
                for (r = 0 ; r < num_pollfds ; r++) {
                    data_source_t *ds = &pollfd_data_sources[r];
                    run_loop_remove_data_source(ds);
                }
                free(pollfd_data_sources);
                pollfd_data_sources = NULL;
                num_pollfds = 0;
                doing_pollfds = 0;
            }

        case LIB_USB_INTERFACE_CLAIMED:
            for (c = 0 ; c < ASYNC_BUFFERS ; c++) {
                if (event_in_transfer[c]) libusb_free_transfer(event_in_transfer[c]);
                if (bulk_in_transfer[c])  libusb_free_transfer(bulk_in_transfer[c]);
            }

            // TODO free control and acl out transfers

            libusb_release_interface(handle, 0);

        case LIB_USB_KERNEL_DETACHED:
#if !defined(__APPLE__) && !defined(_WIN32)
            libusb_attach_kernel_driver (handle, 0);
#endif
        case LIB_USB_DEVICE_OPENDED:
            libusb_close(handle);

        case LIB_USB_OPENED:
            libusb_exit(NULL);
    }

    libusb_state = LIB_USB_CLOSED;
    handle = NULL;

    return 0;
}

static int usb_send_cmd_packet(uint8_t *packet, int size){
    int r;

    if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED) return -1;

    // async
    libusb_fill_control_setup(hci_control_buffer, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE, 0, 0, 0, size);
    memcpy(hci_control_buffer + LIBUSB_CONTROL_SETUP_SIZE, packet, size);

    // prepare transfer
    int completed = 0;
    libusb_fill_control_transfer(command_out_transfer, handle, hci_control_buffer, async_callback, &completed, 0);
    command_out_transfer->flags = LIBUSB_TRANSFER_FREE_BUFFER;

    // update stata before submitting transfer
    usb_command_active = 1;

    // submit transfer
    r = libusb_submit_transfer(command_out_transfer);
    
    // // Use synchronous call to sent out command
    // r = libusb_control_transfer(handle, 
    //     LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
    //     0, 0, 0, packet, size, 0);

    if (r < 0) {
        usb_command_active = 0;
        log_error("Error submitting control transfer %d", r);
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
    libusb_fill_bulk_transfer(bulk_out_transfer, handle, acl_out_addr, packet, size,
        async_callback, &completed, 0);
    bulk_out_transfer->type = LIBUSB_TRANSFER_TYPE_BULK;

    // update stata before submitting transfer
    usb_acl_out_active = 1;

    r = libusb_submit_transfer(bulk_out_transfer);
    if (r < 0) {
        usb_acl_out_active = 0;
        log_error("Error submitting data transfer, %d", r);
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
        default:
            return -1;
    }
}

static void usb_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    log_info("registering packet handler");
    packet_handler = handler;
}

static const char * usb_get_transport_name(void){
    return "USB";
}

static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
}

// get usb singleton
hci_transport_t * hci_transport_usb_instance() {
    if (!hci_transport_usb) {
        hci_transport_usb = (hci_transport_t*) malloc( sizeof(hci_transport_t));
        hci_transport_usb->open                          = usb_open;
        hci_transport_usb->close                         = usb_close;
        hci_transport_usb->send_packet                   = usb_send_packet;
        hci_transport_usb->register_packet_handler       = usb_register_packet_handler;
        hci_transport_usb->get_transport_name            = usb_get_transport_name;
        hci_transport_usb->set_baudrate                  = NULL;
        hci_transport_usb->can_send_packet_now           = usb_can_send_packet_now;
    }
    return hci_transport_usb;
}
