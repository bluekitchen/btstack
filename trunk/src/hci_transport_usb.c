/*
 *  hci_transport_usb.c
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
#include <sys/types.h>

#include <libusb-1.0/libusb.h>

struct libusb_device_descriptor desc;
libusb_device *dev;
libusb_device_handle *handle;

int find_bt(libusb_device **devs)
{
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
		
		// @TODO detect BT USB Dongle based on character and not by id
		// The class code (bDeviceClass) is 0xE0 – Wireless Controller. 
		// The SubClass code (bDeviceSubClass) is 0x01 – RF Controller. 
		// The Protocol code (bDeviceProtocol) is 0x01 – Bluetooth programming.
		if (desc.bDeviceClass == 0xe0 && desc.bDeviceSubClass == 0x01 && desc.bDeviceProtocol == 0x01){
		// if (desc.idVendor == 0x0a12 && desc.idProduct == 0x0001){
			printf("BT Dongle found.\n");
			return 1;
		}
	}
	return 0;
}

int state = 0;
struct libusb_transfer *interrupt_transfer;
static void control_callback(struct libusb_transfer *transfer){
	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		fprintf(stderr, "control_callback not completed!\n");
	}
	
	printf("async cb_mode_changed length=%d actual_length=%d\n",
		   transfer->length, transfer->actual_length);
	
	printf("control_callback\n");
	state = 1;
}

static void event_callback(struct libusb_transfer *transfer)
{
	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		fprintf(stderr, "interrupttransfer not completed!\n");
	} 
	printf("async cb_mode_changed length=%d actual_length=%d\n",
		   transfer->length, transfer->actual_length);

	printf("received data len %u\n", transfer->actual_length);
	int r;
	for (r=0;r<transfer->actual_length; r++) printf("0x%.x ", transfer->buffer[r]);
	printf("\n");
	
	r = libusb_submit_transfer(interrupt_transfer);
	if (r) {
		printf("Error submitting interrupt transfer %d\n", r);
	}
	
	if (state) 
		state++;
}

int usb_main(void)
{
	libusb_device **devs;
	int r;
	ssize_t cnt;
	
	r = libusb_init(NULL);
	if (r < 0)
		return r;
	
	cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0)
		return (int) cnt;
	
	r = find_bt(devs);

	if (r) {
		r = libusb_open(dev, &handle);
		printf("libusb open %d, handle %xu\n", r, (int) handle);
	}
	libusb_free_device_list(devs, 1);

	if (r < 0) {
		goto exit;
	}
	
	libusb_set_debug(0,3);

#ifndef __APPLE__
	r = libusb_detach_kernel_driver (handle, 0);
	if (r < 0) {
		fprintf(stderr, "libusb_detach_kernel_driver error %d\n", r);
		goto close;
	}
	printf("libusb_detach_kernel_driver\n");
#endif

	printf("claimed interface 0\n");
	libusb_claim_interface(handle, 0);
	if (r < 0) {
		fprintf(stderr, "usb_claim_interface error %d\n", r);
		goto reattach;
	}
	printf("claimed interface 0\n");
	
#if 0
	struct libusb_config_descriptor *config_descriptor;
	r = libusb_get_active_config_descriptor(dev, &config_descriptor);
	printf("configuration: %u interfacs\n", config_descriptor->bNumInterfaces);
	const struct libusb_interface *interface = config_descriptor->interface;
	const struct libusb_interface_descriptor * interface0descriptor = interface->altsetting;
	printf("interface 0: %u endpoints\n", interface0descriptor->bNumEndpoints);
	const struct libusb_endpoint_descriptor *endpoint = interface0descriptor->endpoint;
	for (r=0;r<interface0descriptor->bNumEndpoints;r++,endpoint++){
		printf("endpoint %x, attributes %x\n", endpoint->bEndpointAddress, endpoint->bmAttributes);
	}
#endif	
	// allocation
	struct libusb_transfer *control_transfer = libusb_alloc_transfer(0);   // 0 isochronous transfers CMDs
	interrupt_transfer = libusb_alloc_transfer(0); // 0 isochronous transfers Events
	
	// get answer
	uint8_t buffer[260];
	libusb_fill_interrupt_transfer(interrupt_transfer, handle, 0x81, buffer, 260, event_callback, NULL, 2000) ;	
	r = libusb_submit_transfer(interrupt_transfer);
	if (r) {
		printf("Error submitting interrupt transfer %d\n", r);
	}
	printf("interrupt started\n");
	
	
	// control
	uint8_t hci_reset[] = { 0x03, 0x0c, 0x00 };
	uint8_t cmd_buffer[20];
	
	// synchronous
	// r = libusb_control_transfer (handle, LIBUSB_REQUEST_TYPE_CLASS,
	//							 0, 0, 0, 
	//							 hci_reset, sizeof (hci_reset),
	//							 0);

	libusb_fill_control_setup(cmd_buffer, LIBUSB_REQUEST_TYPE_CLASS, 0, 0, 0, sizeof(hci_reset));
	for (r = 0 ; r < sizeof(hci_reset) ; r++)
		cmd_buffer[LIBUSB_CONTROL_SETUP_SIZE + r] = hci_reset[r];
	libusb_fill_control_transfer(control_transfer, handle, cmd_buffer, control_callback, NULL, 1000);
	control_transfer->flags = LIBUSB_TRANSFER_SHORT_NOT_OK | LIBUSB_TRANSFER_FREE_TRANSFER;
	r = libusb_submit_transfer(control_transfer);
	if (r) {
		printf("Error submitting control transfer %d\n", r);
	}
	while (state < 5) 
		libusb_handle_events(NULL);

	printf("interrupt done\n");

	// libusb_free_transfer(control_transfer);
	libusb_free_transfer(interrupt_transfer);
	
	// int length = -1;
	// r = libusb_interrupt_transfer(handle, 0x81, buffer, 100, &length, 0);

/*	r = libusb_interrupt_transfer(handle, 0x81, buffer, 100, &length, 0);
	printf("received data len %u, r= %d\n", length, r);
	for (r=0;r<length; r++) printf("0x%.x ", buffer[r]);
	printf("\n");
*/				
out:
	
release_interface:
	libusb_release_interface(handle, 0);	
reattach:
#ifndef __APPLE__
	libusb_attach_kernel_driver (handle, 0);
#endif
close: 	
	libusb_close(handle);
exit:
	libusb_exit(NULL);
	return 0;
}


