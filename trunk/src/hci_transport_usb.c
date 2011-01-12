/*
 * Copyright (C) 2009 by Matthias Ringwald
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
 */

/*
 *  hci_transport_usb.c
 *
 *  HCI Transport API implementation for USB
 *
 *  Created by Matthias Ringwald on 7/5/09.
 */

// this is not even alpha... :)

// delock bt class 2 - csr
// 0a12:0001 (bus 27, device 2)

// Interface Number - Alternate Setting - suggested Endpoint Address - Endpoint Type - Suggested Max Packet Size 
// HCI Commands 0 0 0x00 Control 8/16/32/64 
// HCI Events   0 0 0x81 Interrupt (IN) 16 
// ACL Data  0 0 0x82 Bulk (IN) 32/64 
// ACL Data  0 0 0x02 Bulk (OUT) 32/64 

#include <stdio.h>
#include <strings.h>
#include <unistd.h>   /* UNIX standard function definitions */
#include <sys/types.h>

#include <libusb-1.0/libusb.h>

#include "hci.h"
#include "hci_transport.h"
#include "hci_dump.h"

// prototypes
static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size); 
static int usb_close();
    
enum {
    LIB_USB_CLOSED,
    LIB_USB_OPENED,
    LIB_USB_DEVICE_OPENDED,
    LIB_USB_KERNEL_DETACHED,
    LIB_USB_INTERFACE_CLAIMED,
    LIB_USB_TRANSFERS_ALLOCATED
} libusb_state = LIB_USB_CLOSED;

// single instance
static hci_transport_t * hci_transport_usb = NULL;

static  void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = dummy_handler;

static uint8_t hci_cmd_out[HCI_ACL_3DH5_SIZE];      // bigger than largest packet
static uint8_t hci_event_buffer[HCI_ACL_3DH5_SIZE]; // bigger than largest packet
static uint8_t hci_acl_in[HCI_ACL_3DH5_SIZE];       // bigger than largest packet

// libusb
static struct libusb_device_descriptor desc;
static libusb_device        * dev;
static libusb_device_handle * handle;
static struct libusb_transfer *interrupt_transfer;
static struct libusb_transfer *control_transfer;
static struct libusb_transfer *bulk_in_transfer;
static struct libusb_transfer *bulk_out_transfer;


static libusb_device * find_bt(libusb_device **devs) {
	int i = 0;
	while ((dev = devs[i++]) != NULL) {
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0) {
			fprintf(stderr, "failed to get device descriptor");
			return 0;
		}
		
		printf("%04x:%04x (bus %d, device %d) - class %x subclass %x protocol %x \n",
			   desc.idVendor, desc.idProduct,
			   libusb_get_bus_number(dev), libusb_get_device_address(dev),
			   desc.bDeviceClass, desc.bDeviceSubClass, desc.bDeviceProtocol);
		
		// @TODO: detect BT USB Dongle based on character and not by id
		// The class code (bDeviceClass) is 0xE0 – Wireless Controller. 
		// The SubClass code (bDeviceSubClass) is 0x01 – RF Controller. 
		// The Protocol code (bDeviceProtocol) is 0x01 – Bluetooth programming.
		if (desc.bDeviceClass == 0xe0 && desc.bDeviceSubClass == 0x01 && desc.bDeviceProtocol == 0x01){
		// if (desc.idVendor == 0x0a12 && desc.idProduct == 0x0001){
			printf("BT Dongle found.\n");
			return dev;
		}
	}
	return NULL;
}

static void control_callback(struct libusb_transfer *transfer){
	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		fprintf(stderr, "control_callback not completed!\n");
        return;
	}
	
	printf("control_callback length=%d actual_length=%d\n",
		   transfer->length, transfer->actual_length);
}

static void bulk_out_callback(struct libusb_transfer *transfer){
	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		fprintf(stderr, "bulk_out_callback not completed!\n");
        return;
	}
	
	printf("bulk_out_callback length=%d actual_length=%d\n",
		   transfer->length, transfer->actual_length);
}


static void event_callback(struct libusb_transfer *transfer)
{
    printf("event_callback length=%d actual_length=%d: ",
           transfer->length, transfer->actual_length);
	if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        int i;
        for (i=0;i<transfer->actual_length; i++) printf("0x%02x ", transfer->buffer[i]);
        printf("\n");

        hci_dump_packet( HCI_EVENT_PACKET, 1, transfer->buffer, transfer->actual_length);
        packet_handler(HCI_EVENT_PACKET, transfer->buffer, transfer->actual_length);
    }
	int r = libusb_submit_transfer(transfer);
	if (r) {
		printf("Error submitting interrupt transfer %d\n", r);
	}
}

static void bulk_in_callback(struct libusb_transfer *transfer)
{
    printf("bulk_in_callback length=%d actual_length=%d: \n",
           transfer->length, transfer->actual_length);
	if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        int i;
        for (i=0;i<transfer->actual_length; i++) printf("0x%02x ", transfer->buffer[i]);
        printf("\n");
        
        hci_dump_packet( HCI_EVENT_PACKET, 1, transfer->buffer, transfer->actual_length);
        packet_handler(HCI_EVENT_PACKET, transfer->buffer, transfer->actual_length);
    }
	int r = libusb_submit_transfer(transfer);
	if (r) {
		printf("Error submitting bulk in transfer %d\n", r);
	}
}

static int usb_process(struct data_source *ds) {
    if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED) return -1;
    struct timeval tv;
    bzero(&tv, sizeof(struct timeval));
    libusb_handle_events_timeout(NULL, &tv);
    return 0;
}

static int usb_open(void *transport_config){
   
    libusb_device **devs;
	int r;
	ssize_t cnt;
	
	// USB init
    r = libusb_init(NULL);
	if (r < 0) return -1;
    libusb_state = LIB_USB_OPENED;
    
	// Get Devices 
    cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0) {
		usb_close();
        return -1;
    }	
	// Find BT modul
    libusb_device * aDev  = find_bt(devs);
    if (!aDev){
        libusb_free_device_list(devs, 1);
        usb_close();
        return -1;
    }
    dev = aDev;
    r = libusb_open(dev, &handle);
    printf("libusb open %d, handle %xu\n", r, (int) handle);
    libusb_state = LIB_USB_OPENED;
	libusb_free_device_list(devs, 1);
	if (r < 0) {
        usb_close();
        return r;
	}
	
	libusb_set_debug(0,3);
    
    // Detach OS driver (not possible for OS X)
#ifndef __APPLE__
	r = libusb_detach_kernel_driver (handle, 0);
	if (r < 0) {
		fprintf(stderr, "libusb_detach_kernel_driver error %d\n", r);
        usb_close();
        return r;
	}
	printf("libusb_detach_kernel_driver\n");
#endif
    libusb_state = LIB_USB_KERNEL_DETACHED;

	// libusb_set_debug(0,3);
    
    // reserve access to device
	printf("claiming interface 0...\n");
	r = libusb_claim_interface(handle, 0);
	if (r < 0) {
		fprintf(stderr, "usb_claim_interface error %d\n", r);
        usb_close();
        return r;
	}
    libusb_state = LIB_USB_INTERFACE_CLAIMED;
	printf("claimed interface 0\n");
	
/*
    // get endpoints - broken on OS X until libusb 1.0.3
	struct libusb_config_descriptor *config_descriptor;
	r = libusb_get_active_config_descriptor(dev, &config_descriptor);
	printf("configuration: %u interfaces\n", config_descriptor->bNumInterfaces);
	const struct libusb_interface *interface = config_descriptor->interface;
	const struct libusb_interface_descriptor * interface0descriptor = interface->altsetting;
	printf("interface 0: %u endpoints\n", interface0descriptor->bNumEndpoints);
	const struct libusb_endpoint_descriptor *endpoint = interface0descriptor->endpoint;
	for (r=0;r<interface0descriptor->bNumEndpoints;r++,endpoint++){
		printf("endpoint %x, attributes %x\n", endpoint->bEndpointAddress, endpoint->bmAttributes);
	}
*/    
	// allocation
	control_transfer   = libusb_alloc_transfer(0); // 0 isochronous transfers CMDs
	interrupt_transfer = libusb_alloc_transfer(0); // 0 isochronous transfers Events
	bulk_in_transfer   = libusb_alloc_transfer(0); // 0 isochronous transfers ACL in
	bulk_out_transfer  = libusb_alloc_transfer(0); // 0 isochronous transfers ACL in
	if (!control_transfer || !interrupt_transfer || !bulk_in_transfer || !bulk_out_transfer){
        usb_close();
        return LIBUSB_ERROR_NO_MEM;
    }
    libusb_state = LIB_USB_TRANSFERS_ALLOCATED;
    
    // interrupt (= HCI event) handler
	libusb_fill_interrupt_transfer(interrupt_transfer, handle, 0x81, hci_event_buffer, 260, event_callback, NULL, 3000) ;	
	// interrupt_transfer->flags = LIBUSB_TRANSFER_SHORT_NOT_OK;
	r = libusb_submit_transfer(interrupt_transfer);
	if (r) {
		printf("Error submitting interrupt transfer %d\n", r);
	}
	printf("interrupt started\n");
	

    // bulk in (= ACL packets) handler
	libusb_fill_bulk_transfer(bulk_in_transfer, handle, 0x82, hci_acl_in, HCI_ACL_3DH5_SIZE, bulk_in_callback, NULL, 3000) ;	
	// bulk_in_transfer->flags = LIBUSB_TRANSFER_SHORT_NOT_OK;
	r = libusb_submit_transfer(bulk_in_transfer);
	if (r) {
		printf("Error submitting bulk in transfer %d\n", r);
	}
	printf("bulk in started\n");

    // set up data_sources
    const struct libusb_pollfd ** pollfd = libusb_get_pollfds(NULL);
    for (r = 0 ; pollfd[r] ; r++) {
        data_source_t *ds = malloc(sizeof(data_source_t));
        ds->fd = pollfd[r]->fd;
        ds->process = usb_process;
        run_loop_add_data_source(ds);
        printf("%u: %x fd: %u, events %x\n", r, (unsigned int) pollfd[r], pollfd[r]->fd, pollfd[r]->events);
    }

    // init state machine
    // bytes_to_read = 1;
    // usb_state = USB_W4_PACKET_TYPE;
    // read_pos = 0;
    
    return 0;
}
static int usb_close(){
    // @TODO: remove all run loops!

    switch (libusb_state){
        case LIB_USB_TRANSFERS_ALLOCATED:
            libusb_free_transfer(bulk_in_transfer);
            libusb_free_transfer(bulk_out_transfer);
            libusb_free_transfer(control_transfer);
            libusb_free_transfer(interrupt_transfer);
        case LIB_USB_INTERFACE_CLAIMED:
            libusb_release_interface(handle, 0);	
        case LIB_USB_KERNEL_DETACHED:
#ifndef __APPLE__
            libusb_attach_kernel_driver (handle, 0);
#endif
        case LIB_USB_DEVICE_OPENDED:
            libusb_close(handle);
        case LIB_USB_OPENED:
            libusb_exit(NULL);
    }
    return 0;
}

static int    usb_send_cmd_packet(uint8_t *packet, int size){

    if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED) return -1;
    
    hci_dump_packet( HCI_COMMAND_DATA_PACKET, 0, packet, size);
    printf("send HCI packet, len %u\n", size);
    
    // send packet over USB
	libusb_fill_control_setup(hci_cmd_out, LIBUSB_REQUEST_TYPE_CLASS, 0, 0, 0, size);
    memcpy(&hci_cmd_out[LIBUSB_CONTROL_SETUP_SIZE], packet, size);
	libusb_fill_control_transfer(control_transfer, handle, hci_cmd_out, control_callback, NULL, 1000);
	control_transfer->flags = LIBUSB_TRANSFER_SHORT_NOT_OK;
	int r = libusb_submit_transfer(control_transfer);
	if (r) {
		printf("Error submitting control transfer %d\n", r);
        return r;
	}
    
    return 0;
}

static int usb_send_acl_packet(uint8_t *packet, int size){
    
    if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED) return -1;
	
    hci_dump_packet( HCI_ACL_DATA_PACKET, 0, packet, size);
    
#if 1  // send packet over USB
	libusb_fill_bulk_transfer(bulk_out_transfer, handle, 0x02, packet, size, bulk_out_callback, NULL, 1000);
	bulk_out_transfer->flags = LIBUSB_TRANSFER_SHORT_NOT_OK;
	int r = libusb_submit_transfer(bulk_out_transfer);
	if (r) {
		printf("Error submitting control transfer %d\n", r);
        return r;
	}
#else
    int transferred;
    int ret = libusb_bulk_transfer (handle, 0x02, packet, size, &transferred, 5000);
    if(ret>=0){
        printf("acl data transfer succeeded");
    }else{
        printf("acl data transfer failed");
    }
#endif
    return 0;
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
    packet_handler = handler;
}

static const char * usb_get_transport_name(){
    return "USB";
}

static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
}

// get usb singleton
hci_transport_t * hci_transport_usb_instance() {
    if (!hci_transport_usb) {
        hci_transport_usb = malloc( sizeof(hci_transport_t));
        hci_transport_usb->open                          = usb_open;
        hci_transport_usb->close                         = usb_close;
        hci_transport_usb->send_packet                   = usb_send_packet;
        hci_transport_usb->register_packet_handler       = usb_register_packet_handler;
        hci_transport_usb->get_transport_name            = usb_get_transport_name;
    }
    return hci_transport_usb;
}
