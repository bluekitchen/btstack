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
		// The class code (bDeviceClass) is 0xE0 a Wireless Controller. 
		// The SubClass code (bDeviceSubClass) is 0x01 a RF Controller. 
		// The Protocol code (bDeviceProtocol) is 0x01 a Bluetooth programming.
		if (desc.idVendor == 0x0a12 && desc.idProduct == 0x0001){
			printf("BT Dongle found.\n");
			return 1;
		}
	}
	return 0;
}

int main(void)
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
    
	r = libusb_detach_kernel_driver (handle, 0);
	if (r < 0) {
		fprintf(stderr, "libusb_detach_kernel_driver error %d\n", r);
		goto close;
	}
	printf("libusb_detach_kernel_driver\n");
	
	libusb_claim_interface(handle, 0);
	if (r < 0) {
		fprintf(stderr, "usb_claim_interface error %d\n", r);
		goto reattach;
	}
	printf("claimed interface 0\n");
	
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
	
	uint8_t hci_reset[] = { 0x03, 0x0c, 0x00 };
	r = libusb_control_transfer (handle, LIBUSB_REQUEST_TYPE_CLASS,
								 0, 0, 0, 
								 hci_reset, sizeof (hci_reset),
								 0);
	if (r < 0) {
		fprintf(stderr, "libusb_control_transfer error %d\n", r);
		goto out;
	}
	printf("send data (%u bytes)\n", r);
	
    
	// get answer
	uint8_t buffer[260];
	int length = -1;
	r = libusb_interrupt_transfer(handle, 0x81, buffer, 100, &length, 0);
	printf("received data len %u, r= %d\n", length, r);
	for (r=0;r<length; r++) printf("0x%.x ", buffer[r]);
	printf("\n");
    
	r = libusb_interrupt_transfer(handle, 0x81, buffer, 100, &length, 0);
	printf("received data len %u, r= %d\n", length, r);
	for (r=0;r<length; r++) printf("0x%.x ", buffer[r]);
	printf("\n");
    
out:
	
release_interface:
	libusb_release_interface(handle, 0);	
reattach:
	libusb_attach_kernel_driver (handle, 0);
close: 	
	libusb_close(handle);
exit:
	libusb_exit(NULL);
	return 0;
}


