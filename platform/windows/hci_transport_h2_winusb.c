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

#define BTSTACK_FILE__ "hci_transport_h2_winusb.c"

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
#include <string.h>
#include <sys/types.h>
#include <inttypes.h>   // to print long long int (aka 64 bit ints)

#include "btstack_config.h"

#include "btstack_debug.h"
#include "hci.h"
#include "hci_transport.h"
#include "hci_transport_usb.h"

#include <Windows.h>
#include <SetupAPI.h>
#include <Winusb.h>

#ifdef ENABLE_SCO_OVER_HCI

// Isochronous Add-On

// Function signatures frome https://abi-laboratory.pro/compatibility/Windows_7.0_to_Windows_8.1/x86_64/info/winusb.dll/symbols.html
// MSDN documentation has multiple errors (Jan 2017), annotated below

// As Isochochronous functions are provided by newer versions of ming64, we use a BTstack/BTSTACK prefix to prevent name collisions

typedef PVOID BTSTACK_WINUSB_ISOCH_BUFFER_HANDLE, *BTSTACK_PWINUSB_ISOCH_BUFFER_HANDLE;

typedef struct _BTSTACK_WINUSB_PIPE_INFORMATION_EX {
  USBD_PIPE_TYPE PipeType;
  UCHAR          PipeId;
  USHORT         MaximumPacketSize;
  UCHAR          Interval;
  ULONG          MaximumBytesPerInterval;
} BTSTACK_WINUSB_PIPE_INFORMATION_EX, *BTSTACK_PWINUSB_PIPE_INFORMATION_EX;

typedef BOOL (WINAPI * BTstack_WinUsb_QueryPipeEx_t) (
	WINUSB_INTERFACE_HANDLE 	InterfaceHandle,
	UCHAR						AlternateInterfaceNumber,
	UCHAR 						PipeIndex,
	BTSTACK_PWINUSB_PIPE_INFORMATION_EX PipeInformationEx
);
typedef BOOL (WINAPI * BTstack_WinUsb_RegisterIsochBuffer_t)(
	WINUSB_INTERFACE_HANDLE     InterfaceHandle,
	UCHAR                       PipeID,
	PVOID                       Buffer,
	ULONG                       BufferLength,
	BTSTACK_PWINUSB_ISOCH_BUFFER_HANDLE BufferHandle
);
typedef BOOL (WINAPI * BTstack_WinUsb_ReadIsochPipe_t)(
    BTSTACK_PWINUSB_ISOCH_BUFFER_HANDLE BufferHandle,
    ULONG                       Offset,
    ULONG                       Length,
    PULONG                      FrameNumber,
    ULONG                       NumberOfPackets,        // MSDN lists PULONG
    PUSBD_ISO_PACKET_DESCRIPTOR IsoPacketDescriptors,   // MSDN lists PULONG
    LPOVERLAPPED                Overlapped
);
typedef BOOL (WINAPI * BTstack_WinUsb_ReadIsochPipeAsap_t)(
    BTSTACK_PWINUSB_ISOCH_BUFFER_HANDLE BufferHandle,
    ULONG                       Offset,
    ULONG                       Length,
    BOOL                        ContinueStream,
    ULONG                      	NumberOfPackets,        // MSDN lists PULONG
    PUSBD_ISO_PACKET_DESCRIPTOR IsoPacketDescriptors,
 	LPOVERLAPPED                Overlapped
);
typedef BOOL (WINAPI * BTstack_WinUsb_WriteIsochPipe_t)(
    BTSTACK_PWINUSB_ISOCH_BUFFER_HANDLE BufferHandle,
    ULONG                       Offset,
    ULONG                       Length,
    PULONG                      FrameNumber,
	LPOVERLAPPED                Overlapped
);
typedef BOOL (WINAPI * BTstack_WinUsb_WriteIsochPipeAsap_t)(
    BTSTACK_PWINUSB_ISOCH_BUFFER_HANDLE BufferHandle,
    ULONG                       Offset,
    ULONG                       Length,
    BOOL                        ContinueStream,
	LPOVERLAPPED                Overlapped
);
typedef BOOL (WINAPI * BTstack_WinUsb_UnregisterIsochBuffer_t)(
	BTSTACK_PWINUSB_ISOCH_BUFFER_HANDLE BufferHandle
);
typedef BOOL (WINAPI * BTstack_WinUsb_GetCurrentFrameNumber_t)(
    WINUSB_INTERFACE_HANDLE     InterfaceHandle,        // MSDN lists 'Device handle returned from CreateFile'
    PULONG                      CurrentFrameNumber,
    LARGE_INTEGER               *TimeStamp
);

static BTstack_WinUsb_QueryPipeEx_t 			BTstack_WinUsb_QueryPipeEx;
static BTstack_WinUsb_RegisterIsochBuffer_t 	BTstack_WinUsb_RegisterIsochBuffer;
static BTstack_WinUsb_ReadIsochPipe_t 			BTstack_WinUsb_ReadIsochPipe;
static BTstack_WinUsb_ReadIsochPipeAsap_t 		BTstack_WinUsb_ReadIsochPipeAsap;
static BTstack_WinUsb_WriteIsochPipe_t 			BTstack_WinUsb_WriteIsochPipe;
static BTstack_WinUsb_WriteIsochPipeAsap_t 		BTstack_WinUsb_WriteIsochPipeAsap;
static BTstack_WinUsb_UnregisterIsochBuffer_t 	BTstack_WinUsb_UnregisterIsochBuffer;
static BTstack_WinUsb_GetCurrentFrameNumber_t   BTstack_WinUsb_GetCurrentFrameNumber;
#endif

// Doesn't work as expected
// #define SCHEDULE_SCO_IN_TRANSFERS_MANUALLY

// Not tested yet
// #define SCHEDULE_SCO_OUT_TRANSFERS_MANUALLY

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
#define ALT_SETTING (1)

// alt setting for 1-3 connections and 8/16 bit
const int alt_setting_8_bit[]  = {1,2,3};      
const int alt_setting_16_bit[] = {2,4,5};

// for ALT_SETTING >= 1 and 8-bit channel, we need the following isochronous packets
// One complete SCO packet with 24 frames every 3 frames (== 3 ms)
#define NUM_ISO_PACKETS (3)

const uint16_t iso_packet_size_for_alt_setting[] = {
    0,
    9,
    17,
    25,
    33,
    49,
    63,
};

// 49 bytes is the max usb packet size for alternate setting 5 (Three 8 kHz 16-bit channels or one 8 kHz 16-bit channel and one 16 kHz 16-bit channel)
// note: alt setting 6 has max packet size of 63 every 7.5 ms = 472.5 bytes / HCI packet, while max SCO packet has 255 byte payload
#define SCO_PACKET_SIZE  (49 * NUM_ISO_PACKETS)

#define ISOC_BUFFERS   8

// Outgoing SCO packet queue
// simplified ring buffer implementation
#define SCO_RING_BUFFER_COUNT  (20)
#define SCO_RING_BUFFER_SIZE (SCO_RING_BUFFER_COUNT * SCO_PACKET_SIZE)

/** Request type bits of the "bmRequestType" field in control transfers. */
enum usb_request_type {
	USB_REQUEST_TYPE_STANDARD = (0x00 << 5),
	USB_REQUEST_TYPE_CLASS = (0x01 << 5),
	USB_REQUEST_TYPE_VENDOR = (0x02 << 5),
};

/** Recipient bits of the "bmRequestType" field in control transfers. Values 4 through 31 are reserved. */
enum usb_request_recipient {
	USB_RECIPIENT_DEVICE = 0x00,
	USB_RECIPIENT_INTERFACE = 0x01,
	USB_RECIPIENT_ENDPOINT = 0x02,
	USB_RECIPIENT_OTHER = 0x03,
};

// This is the GUID for the USB device class
static GUID GUID_DEVINTERFACE_USB_DEVICE =
{ 0xA5DCBF10L, 0x6530, 0x11D2, { 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED } };

static void usb_dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size);

static void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = &usb_dummy_handler;

// endpoint addresses
static int event_in_addr;
static int acl_in_addr;
static int acl_out_addr;
static int sco_in_addr;
static int sco_out_addr;

// 
static HANDLE usb_device_handle;
static WINUSB_INTERFACE_HANDLE usb_interface_0_handle;
static WINUSB_INTERFACE_HANDLE usb_interface_1_handle;
static OVERLAPPED usb_overlapped_event_in;
static OVERLAPPED usb_overlapped_command_out;
static OVERLAPPED usb_overlapped_acl_in;
static OVERLAPPED usb_overlapped_acl_out;
static btstack_data_source_t usb_data_source_event_in;
static btstack_data_source_t usb_data_source_command_out;
static btstack_data_source_t usb_data_source_acl_in;
static btstack_data_source_t usb_data_source_acl_out;

//
static int usb_command_out_active;
static int usb_acl_out_active;

// buffer for HCI Events and ACL Packets
static uint8_t hci_event_in_buffer[2 + 255];
static uint8_t hci_acl_in_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + HCI_ACL_BUFFER_SIZE]; 

// transport interface state
static int usb_transport_open;

#ifdef ENABLE_SCO_OVER_HCI

typedef enum {
    H2_W4_SCO_HEADER = 1,
    H2_W4_PAYLOAD,
} H2_SCO_STATE;

// SCO Incoming Windows
static uint8_t hci_sco_in_buffer[ISOC_BUFFERS * SCO_PACKET_SIZE]; 
static BTSTACK_WINUSB_ISOCH_BUFFER_HANDLE hci_sco_in_buffer_handle;
static USBD_ISO_PACKET_DESCRIPTOR hci_sco_packet_descriptors[ISOC_BUFFERS * NUM_ISO_PACKETS];
static OVERLAPPED usb_overlapped_sco_in[ISOC_BUFFERS];
static int usb_sco_in_expected_transfer;

// SCO Incoming Run Loop
static btstack_data_source_t usb_data_source_sco_in[ISOC_BUFFERS];

// SCO Incoming HCI
static H2_SCO_STATE sco_state;
static uint8_t  sco_buffer[SCO_PACKET_SIZE];
static uint16_t sco_read_pos;
static uint16_t sco_bytes_to_read;

// SCO Outgoing Windows
static BTSTACK_WINUSB_ISOCH_BUFFER_HANDLE hci_sco_out_buffer_handle;
static OVERLAPPED usb_overlapped_sco_out[SCO_RING_BUFFER_COUNT];
static int        sco_ring_transfers_active;
static int        usb_sco_out_expected_transfer;

#ifdef SCHEDULE_SCO_IN_TRANSFERS_MANUALLY
// next tranfer
static ULONG sco_next_transfer_at_frame;
#endif

// SCO Outgoing Run Loop
static btstack_data_source_t usb_data_source_sco_out[SCO_RING_BUFFER_COUNT];

// SCO Outgoing HCI
static uint8_t  sco_ring_buffer[SCO_RING_BUFFER_SIZE];
static int      sco_ring_write;  // packet idx

// SCO Reconfiguration - pause/resume
static uint16_t sco_voice_setting;
static int      sco_num_connections;
static int      sco_shutdown;

static uint16_t iso_packet_size;
#endif

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

// known devices
typedef struct {
    btstack_linked_item_t next;
    uint16_t vendor_id;
    uint16_t product_id;
} usb_known_device_t;

static btstack_linked_list_t usb_knwon_devices;

void hci_transport_usb_add_device(uint16_t vendor_id, uint16_t product_id) {
    usb_known_device_t * device = malloc(sizeof(usb_known_device_t));
    if (device != NULL) {
        device->vendor_id = vendor_id;
        device->product_id = product_id;
        btstack_linked_list_add(&usb_knwon_devices, (btstack_linked_item_t *) device);
    }
}

static bool usb_is_vmware_bluetooth_adapter(const char * device_path){
    // VMware Vendor ID 0e0f
    const char * pos = strstr(device_path, "\\usb#vid_0e0f&pid");
    return (pos > 0);
}

static bool usb_device_path_match(const char * device_path, uint16_t vendor_id, uint16_t product_id){
    // construct pid/vid substring
    char substring[20];
    sprintf_s(substring, sizeof(substring), "vid_%04x&pid_%04x", vendor_id, product_id);
    const char * pos = strstr(device_path, substring);
    log_info("check %s in %s -> %p", substring, device_path, pos);
    return (pos > 0);
}

static bool usb_is_known_bluetooth_device(const char * device_path){
    int i;
    for (i=0; i<num_known_devices; i++){
        if (usb_device_path_match( device_path, known_bluetooth_devices[i*2], known_bluetooth_devices[i*2+1])){
            return true;
        }
    }
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &usb_knwon_devices);
    while (btstack_linked_list_iterator_has_next(&it)) {
        usb_known_device_t * device = (usb_known_device_t *) btstack_linked_list_iterator_next(&it);
        if (usb_device_path_match( device_path, device->vendor_id, device->product_id)){
            return true;
        }
    }
    return false;
}

#ifdef ENABLE_SCO_OVER_HCI
static void sco_ring_init(void){
    sco_ring_write = 0;
    sco_ring_transfers_active = 0;
}
static int sco_ring_have_space(void){
    return sco_ring_transfers_active < SCO_RING_BUFFER_COUNT;
}
static void usb_sco_register_buffers(void){
    BOOL result;
    result = BTstack_WinUsb_RegisterIsochBuffer(usb_interface_1_handle, sco_in_addr, hci_sco_in_buffer, sizeof(hci_sco_in_buffer), &hci_sco_in_buffer_handle);
    if (!result) {
        log_error("usb_sco_register_buffers: register in buffer failed, error %lu", GetLastError());
    }
    log_info("hci_sco_in_buffer_handle %p", hci_sco_in_buffer_handle);

    result = BTstack_WinUsb_RegisterIsochBuffer(usb_interface_1_handle, sco_out_addr, sco_ring_buffer, sizeof(sco_ring_buffer), &hci_sco_out_buffer_handle);
    if (!result) {
        log_error("usb_sco_unregister_buffers: register out buffer failed, error %lu", GetLastError());
    }
    log_info("hci_sco_out_buffer_handle %p", hci_sco_out_buffer_handle);
}
static void usb_sco_unregister_buffers(void){
    if (hci_sco_in_buffer_handle){
        BTstack_WinUsb_UnregisterIsochBuffer(hci_sco_in_buffer_handle);
        hci_sco_in_buffer_handle = NULL;
    }
    if (hci_sco_out_buffer_handle){
        BTstack_WinUsb_UnregisterIsochBuffer(hci_sco_out_buffer_handle);
        hci_sco_out_buffer_handle = NULL;
    }
}
#endif

static void usb_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    log_info("registering packet handler");
    packet_handler = handler;
}

static void usb_dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
}

static void usb_init(const void *transport_config){
}

static void usb_free_resources(void){
	if (usb_interface_1_handle){
		WinUsb_Free(usb_interface_1_handle);
		usb_interface_1_handle = NULL;
	}

	if (usb_interface_0_handle){
		WinUsb_Free(usb_interface_0_handle);
		usb_interface_0_handle = NULL;
	}

	if (usb_device_handle) {
		CloseHandle(usb_device_handle);	
		usb_device_handle = NULL;
	}

#ifdef ENABLE_SCO_OVER_HCI
    usb_sco_unregister_buffers();
#endif
}

static void usb_submit_event_in_transfer(void){
	// submit transfer
	BOOL result = WinUsb_ReadPipe(usb_interface_0_handle, event_in_addr, hci_event_in_buffer, sizeof(hci_event_in_buffer), NULL, &usb_overlapped_event_in);
	if (!result) {
		if (GetLastError() != ERROR_IO_PENDING) goto exit_on_error;
	}

    // IO_PENDING -> wait for completed
    btstack_run_loop_enable_data_source_callbacks(&usb_data_source_event_in, DATA_SOURCE_CALLBACK_READ);
    return;

exit_on_error:
	log_error("usb_submit_event_in_transfer: winusb last error %lu", GetLastError());
}

static void usb_submit_acl_in_transfer(void){
	// submit transfer
	BOOL result = WinUsb_ReadPipe(usb_interface_0_handle, acl_in_addr, hci_acl_in_buffer, sizeof(hci_acl_in_buffer), NULL, &usb_overlapped_acl_in);
	if (!result) {
		if (GetLastError() != ERROR_IO_PENDING) goto exit_on_error;
	}

    // IO_PENDING -> wait for completed
    btstack_run_loop_enable_data_source_callbacks(&usb_data_source_acl_in, DATA_SOURCE_CALLBACK_READ);
    return;

exit_on_error:
	log_error("usb_submit_acl_in_transfer: winusb last error %lu", GetLastError());
}

#ifdef ENABLE_SCO_OVER_HCI
#ifdef SCHEDULE_SCO_IN_TRANSFERS_MANUALLY

// frame number gets updated
static void usb_submit_sco_in_transfer_at_frame(int i, ULONG * frame_number){

    if (sco_shutdown){
        log_info("USB SCO Shutdown:: usb_submit_sco_in_transfer_at_frame called");
        return;
    }

    LARGE_INTEGER timestamp;
    ULONG current_frame_number;
    WinUsb_GetCurrentFrameNumber(usb_interface_0_handle, &current_frame_number, &timestamp);

    ULONG frame_before = *frame_number;

    BOOL result = BTstack_WinUsb_ReadIsochPipe(hci_sco_in_buffer_handle, i * SCO_PACKET_SIZE, iso_packet_size * NUM_ISO_PACKETS,  
        frame_number, NUM_ISO_PACKETS, &hci_sco_packet_descriptors[i * NUM_ISO_PACKETS], &usb_overlapped_sco_in[i]);

    // log_info("BTstack_WinUsb_ReadIsochPipe #%02u: current %lu, planned %lu - buffer %lu", i, current_frame_number, frame_before, frame_before - current_frame_number);

    if (!result) {
        if (GetLastError() == ERROR_IO_PENDING) {
        } else {
            goto exit_on_error;
        }
    }

    return;

exit_on_error:
    log_error("usb_submit_sco_in_transfer: winusb last error %lu", GetLastError());
}

#else

static void usb_submit_sco_in_transfer_asap(int i, int continue_stream){

    if (sco_shutdown){
        log_info("USB SCO Shutdown:: usb_submit_sco_in_transfer_at_frame called");
        return;
    }

    LARGE_INTEGER timestamp;
    ULONG current_frame_number;
    BTstack_WinUsb_GetCurrentFrameNumber(usb_interface_0_handle, &current_frame_number, &timestamp);

    // log_info("usb_submit_sco_in_transfer[%02u]: current frame %lu", i, current_frame_number);

    BOOL result = BTstack_WinUsb_ReadIsochPipeAsap(hci_sco_in_buffer_handle, i * SCO_PACKET_SIZE, iso_packet_size * NUM_ISO_PACKETS,  
        continue_stream, NUM_ISO_PACKETS, &hci_sco_packet_descriptors[i * NUM_ISO_PACKETS], &usb_overlapped_sco_in[i]);

    if (!result) {
        if (GetLastError() != ERROR_IO_PENDING) goto exit_on_error;
    }

    return;

exit_on_error:
    log_error("usb_submit_sco_in_transfer: winusb last error %lu", GetLastError());
}
#endif
#endif

static void usb_process_event_in(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {

    btstack_run_loop_disable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_READ);

    DWORD bytes_read;
    BOOL ok = WinUsb_GetOverlappedResult(usb_interface_0_handle, &usb_overlapped_event_in, &bytes_read, FALSE);
    if(!ok){
        DWORD err = GetLastError();
        if (err == ERROR_IO_INCOMPLETE){
            // IO_INCOMPLETE -> wait for completed
            btstack_run_loop_enable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_READ);
        } else {
            log_error("usb_process_event_in: error reading");
        }
        return;
    }

    // notify uppper
    packet_handler(HCI_EVENT_PACKET, hci_event_in_buffer, (uint16_t) bytes_read);

	// re-submit transfer
	usb_submit_event_in_transfer();
}

static void usb_process_acl_in(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {

    btstack_run_loop_disable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_READ);

    DWORD bytes_read;
    BOOL ok = WinUsb_GetOverlappedResult(usb_interface_0_handle, &usb_overlapped_acl_in, &bytes_read, FALSE);
    if(!ok){
        DWORD err = GetLastError();
        if (err == ERROR_IO_INCOMPLETE){
            // IO_INCOMPLETE -> wait for completed
            btstack_run_loop_enable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_READ);
        } else {
            log_error("usb_process_acl_in: error reading");

            // Reset Pipe
            err = WinUsb_ResetPipe(usb_interface_0_handle, acl_in_addr);
            log_info("WinUsb_ResetPipe: result %u", (int) err);
            if (err){
                log_info("WinUsb_ResetPipe error %u", (int) GetLastError());
            }
            
            // re-submit transfer
            usb_submit_acl_in_transfer();
        }
        return;
    }

    // notify uppper
    packet_handler(HCI_ACL_DATA_PACKET, hci_acl_in_buffer, (uint16_t) bytes_read);

	// re-submit transfer
	usb_submit_acl_in_transfer();
}

#ifdef ENABLE_SCO_OVER_HCI
static void sco_state_machine_init(void){
    sco_state = H2_W4_SCO_HEADER;
    sco_read_pos = 0;
    sco_bytes_to_read = 3;
}

static void sco_handle_data(uint8_t * buffer, uint16_t size){
	// printf("sco_handle_data: state %u, pos %u, to read %u, size %u", sco_state, sco_read_pos, sco_bytes_to_read, size);
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
                if (sco_bytes_to_read > (sizeof(sco_buffer)-3)){
                	log_error("sco_handle_data: sco packet len > packet size");
	                sco_state_machine_init();
                }
                break;
            case H2_W4_PAYLOAD:
                // packet complete
                packet_handler(HCI_SCO_DATA_PACKET, sco_buffer, sco_read_pos);
                sco_state_machine_init();
                break;
			default:
				btstack_unreachable();
				break;
        }
    }
}

static void usb_process_sco_out(btstack_data_source_t *ds,  btstack_data_source_callback_type_t callback_type){

    btstack_run_loop_disable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_WRITE);

    if (sco_shutdown){
        log_info("USB SCO Shutdown:: usb_process_sco_out called");
        return;
    }

    // get current frame number
    ULONG current_frame_number;
    LARGE_INTEGER timestamp;
    BTstack_WinUsb_GetCurrentFrameNumber(usb_interface_0_handle, &current_frame_number, &timestamp);

    // find index
    int transfer_index;
    for (transfer_index=0;transfer_index<SCO_RING_BUFFER_COUNT;transfer_index++){
        if (ds == &usb_data_source_sco_out[transfer_index]) break;
    }

    // log_info("usb_process_sco_out[%02u] -- current frame %lu", transfer_index, current_frame_number);

    DWORD bytes_transferred;
    BOOL ok = WinUsb_GetOverlappedResult(usb_interface_0_handle, &usb_overlapped_sco_out[transfer_index], &bytes_transferred, FALSE);
    // log_info("usb_process_sco_out_done: #%u result %u, bytes %u, state %u", transfer_index, ok, (int) bytes_transferred, sco_state);
    if(!ok){
        DWORD err = GetLastError();
        if (err == ERROR_IO_INCOMPLETE){
            // IO_INCOMPLETE -> wait for completed
            btstack_run_loop_enable_data_source_callbacks(&usb_data_source_sco_out[transfer_index], DATA_SOURCE_CALLBACK_WRITE);
            return;
        }
        log_error("usb_process_sco_out_done[%02u]: error writing %u, Internal %x", transfer_index, (int) err, (int) usb_overlapped_sco_out[transfer_index].Internal);
    }

    // decrease tab
    sco_ring_transfers_active--;

    // enable next data source callback
    if (sco_ring_transfers_active){
        // update expected and wait for completion
        usb_sco_out_expected_transfer = (transfer_index+ 1) % SCO_RING_BUFFER_COUNT;
        // log_info("usb_process_sco_out_done[%02u]: wait for transfer %02u", transfer_index, usb_sco_out_expected_transfer);
        btstack_run_loop_enable_data_source_callbacks(&usb_data_source_sco_out[usb_sco_out_expected_transfer], DATA_SOURCE_CALLBACK_WRITE);
    }

    // log_info("usb_process_sco_out_done: transfers active %u", sco_ring_transfers_active);

    // mark free
    if (sco_ring_have_space()) {
        uint8_t event[] = { HCI_EVENT_SCO_CAN_SEND_NOW, 0};
        packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));
    }
}

static void usb_process_sco_in(btstack_data_source_t *ds,  btstack_data_source_callback_type_t callback_type){

    btstack_run_loop_disable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_READ);

    if (sco_shutdown){
        log_info("USB SCO Shutdown: usb_process_sco_out called");
        return;
    }

    // find index
    int i;
    for (i=0;i<ISOC_BUFFERS;i++){
        if (ds == &usb_data_source_sco_in[i]) break;
    }
    int transfer_index = i;

    // ULONG current_frame_number;
    // LARGE_INTEGER timestamp;
    // BTstack_WinUsb_GetCurrentFrameNumber(usb_interface_0_handle, &current_frame_number, &timestamp);

    // log_info("usb_process_sco_in[%02u] -- current frame %lu", transfer_index, current_frame_number);

    DWORD bytes_transferred;
    BOOL ok = WinUsb_GetOverlappedResult(usb_interface_0_handle, &usb_overlapped_sco_in[transfer_index], &bytes_transferred, FALSE);

    if(!ok) {
        DWORD err = GetLastError();
        if (err == ERROR_IO_INCOMPLETE) {
            // IO_INCOMPLETE -> wait for completed
            btstack_run_loop_enable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_READ);
            return;
        }
        log_error("usb_process_sco_in[%02u]: error reading %u, Internal %x", transfer_index, (int) err, (int) usb_overlapped_sco_out[i].Internal);
    }

    if (ok){
        for (i=0;i<NUM_ISO_PACKETS;i++){
            USBD_ISO_PACKET_DESCRIPTOR * packet_descriptor = &hci_sco_packet_descriptors[transfer_index * NUM_ISO_PACKETS + i];
            if (packet_descriptor->Length){
                uint8_t * iso_data = &hci_sco_in_buffer[transfer_index * SCO_PACKET_SIZE + packet_descriptor->Offset];
                uint16_t  iso_len  = (uint16_t) packet_descriptor->Length;
                sco_handle_data(iso_data, iso_len);
            }
        }
    }

#ifdef SCHEDULE_SCO_IN_TRANSFERS_MANUALLY
    usb_submit_sco_in_transfer_at_frame(i, &sco_next_transfer_at_frame);
#else
    usb_submit_sco_in_transfer_asap(transfer_index, 1);
#endif
    // update expected and wait for completion
    usb_sco_in_expected_transfer = (transfer_index+ 1) % ISOC_BUFFERS;

    // log_info("usb_process_sco_in[%02u]: enable data source %02u", transfer_index, usb_sco_in_expected_transfer);
    btstack_run_loop_enable_data_source_callbacks(&usb_data_source_sco_in[usb_sco_in_expected_transfer], DATA_SOURCE_CALLBACK_READ);
}
#endif

static void usb_process_command_out(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type){

    btstack_run_loop_disable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_WRITE);

    // update stata before submitting transfer
    usb_command_out_active = 0;

    // notify upper stack that provided buffer can be used again
    uint8_t event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};
    packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));
} 

static void usb_process_acl_out(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type){

    btstack_run_loop_disable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_WRITE);

    // update stata before submitting transfer
    usb_acl_out_active = 0;

    // notify upper stack that provided buffer can be used again
    uint8_t event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};
    packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));
}

static BOOL usb_scan_for_bluetooth_endpoints(void) {
    int i;
    USB_INTERFACE_DESCRIPTOR usb_interface_descriptor;

    // reset
    event_in_addr = 0;
    acl_in_addr = 0;
    acl_out_addr = 0;

    log_info("Scanning USB Entpoints:");

    // look for Event and ACL pipes on Interface #0
    BOOL result = WinUsb_QueryInterfaceSettings(usb_interface_0_handle, 0, &usb_interface_descriptor);
    if (!result) goto exit_on_error;
    for (i=0;i<usb_interface_descriptor.bNumEndpoints;i++){
        WINUSB_PIPE_INFORMATION pipe;
        result = WinUsb_QueryPipe(
                     usb_interface_0_handle,
                     0,
                     (UCHAR) i,
                     &pipe);
        if (!result) goto exit_on_error;
        log_info("Interface #0, Alt #0, Pipe idx #%u: type %u, id 0x%02x, max packet size %u,",
            i, pipe.PipeType, pipe.PipeId, pipe.MaximumPacketSize);
        switch (pipe.PipeType){
            case USB_ENDPOINT_TYPE_INTERRUPT:
                if (event_in_addr) continue;
                event_in_addr = pipe.PipeId;
                log_info("-> using 0x%2.2X for HCI Events", event_in_addr);
                break;
            case USB_ENDPOINT_TYPE_BULK:
                if (pipe.PipeId & 0x80) {
                    if (acl_in_addr) continue;
                    acl_in_addr = pipe.PipeId;
                    log_info("-> using 0x%2.2X for ACL Data In", acl_in_addr);
                } else {
                    if (acl_out_addr) continue;
                    acl_out_addr = pipe.PipeId;
                    log_info("-> using 0x%2.2X for ACL Data Out", acl_out_addr);
                }
                break;
            default:
                break;
        }
    }

#ifdef ENABLE_SCO_OVER_HCI
    sco_out_addr = 0;
    sco_in_addr = 0;

    // look for SCO pipes on Interface #1, Alt Setting 1
    int alt_setting = 1;
    result = WinUsb_QueryInterfaceSettings(usb_interface_1_handle, alt_setting, &usb_interface_descriptor);
    if (!result) goto exit_on_error;
    for (i=0;i<usb_interface_descriptor.bNumEndpoints;i++){
        BTSTACK_WINUSB_PIPE_INFORMATION_EX pipe;
        result = BTstack_WinUsb_QueryPipeEx(
                     usb_interface_1_handle,
                     alt_setting,
                     (UCHAR) i,
                     &pipe);
        if (!result) goto exit_on_error;
        log_info("Interface #1, Alt #%u, Pipe idx #%u: type %u, id 0x%02x, max packet size %u, interval %u, max bytes per interval %u",
            alt_setting, i, pipe.PipeType, pipe.PipeId, pipe.MaximumPacketSize, pipe.Interval, (int) pipe.MaximumBytesPerInterval);
        switch (pipe.PipeType){
            case USB_ENDPOINT_TYPE_ISOCHRONOUS:
                if (pipe.PipeId & 0x80) {
                    if (sco_in_addr) continue;
                    sco_in_addr = pipe.PipeId;
                    log_info("-> using 0x%2.2X for SCO Data In", sco_in_addr);
                } else {
                    if (sco_out_addr) continue;
                    sco_out_addr = pipe.PipeId;
                    log_info("-> using 0x%2.2X for SCO Data Out", sco_out_addr);
                }
                break;
            default:
                break;
        }
    }
    if (!sco_in_addr){
        log_error("Couldn't find pipe for SCO IN!");
        return FALSE;
    }
    if (!sco_out_addr){
        log_error("Couldn't find pipe for SCO IN!");
        return FALSE;
    }
#endif

    // check if all found
    if (!event_in_addr){
        log_error("Couldn't find pipe for Event IN!");
        return FALSE;
    }
    if (!acl_in_addr){
        log_error("Couldn't find pipe for ACL IN!");
        return FALSE;
    }
    if (!acl_out_addr){
        log_error("Couldn't find pipe for ACL OUT!");
        return FALSE;
    }

    // all clear
    return TRUE;

exit_on_error:    
    log_error("usb_scan_for_bluetooth_endpoints: last error %lu", GetLastError());
    return FALSE;
}

#ifdef ENABLE_SCO_OVER_HCI

static int usb_sco_start(void){
    printf("usb_sco_start\n");
    log_info("usb_sco_start");

    sco_shutdown = 0;

    sco_state_machine_init();
    sco_ring_init();

    // calc alt setting
    int alt_setting;
    if (sco_voice_setting & 0x0020){
        // 16-bit PCM  
        alt_setting = alt_setting_16_bit[sco_num_connections-1];
    } else {
        // 8-bit PCM or mSBC
        alt_setting = alt_setting_8_bit[sco_num_connections-1];
    }

    log_info("Switching to setting %u on interface 1..", alt_setting);
    // WinUsb_SetCurrentAlternateSetting returns TRUE if the operation succeeds.
    BOOL result = WinUsb_SetCurrentAlternateSetting(usb_interface_1_handle, alt_setting);
    if (!result) goto exit_on_error;

    // derive iso packet size from alt setting
    iso_packet_size = iso_packet_size_for_alt_setting[alt_setting];

    // register isochronous buffer after setting alternate setting
    usb_sco_register_buffers();

#ifdef SCHEDULE_SCO_IN_TRANSFERS_MANUALLY
    // get current frame number
    ULONG current_frame_number;
    LARGE_INTEGER timestamp;
    BTstack_WinUsb_GetCurrentFrameNumber(usb_interface_0_handle, &current_frame_number, &timestamp);
    // plan for next tranfer
    sco_next_transfer_at_frame = current_frame_number + ISOC_BUFFERS * NUM_ISO_PACKETS;
#endif

    int i;
    for (i=0;i<ISOC_BUFFERS;i++){
#ifdef SCHEDULE_SCO_IN_TRANSFERS_MANUALLY
        usb_submit_sco_in_transfer_at_frame(i, &sco_next_transfer_at_frame);
#else       
        usb_submit_sco_in_transfer_asap(i, 0);
#endif    
    }

    usb_sco_in_expected_transfer = 0;

    // only await first transfer to return
    btstack_run_loop_enable_data_source_callbacks(&usb_data_source_sco_in[usb_sco_in_expected_transfer], DATA_SOURCE_CALLBACK_READ);
    return 1;

exit_on_error:
    log_error("usb_sco_start: last error %lu", GetLastError());
    usb_free_resources();
    return 0;    
}

static void usb_sco_stop(void){
    printf("usb_sco_stop\n");
    log_info("usb_sco_stop");

    sco_shutdown = 1;

    // abort SCO transfers
    WinUsb_AbortPipe(usb_interface_0_handle, sco_in_addr);
    WinUsb_AbortPipe(usb_interface_0_handle, sco_out_addr);

    // unlock/free SCO buffers
    usb_sco_unregister_buffers();

    int alt_setting = 0;
    log_info("Switching to setting %u on interface 1..", alt_setting);
    // WinUsb_SetCurrentAlternateSetting returns TRUE if the operation succeeds.
    WinUsb_SetCurrentAlternateSetting(usb_interface_1_handle, alt_setting);
}
#endif

// returns 0 if successful, -1 otherwise
static int usb_try_open_device(const char * device_path){

	// open file
	usb_device_handle = CreateFile(device_path,
		GENERIC_WRITE | GENERIC_READ,
        FILE_SHARE_WRITE | FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        NULL);
	log_info("Opening USB device: %p", usb_device_handle);
	if (!usb_device_handle) goto exit_on_error;

	// WinUsb_Initialize returns TRUE if the operation succeed
	BOOL result = WinUsb_Initialize(usb_device_handle, &usb_interface_0_handle);
	if (!result) goto exit_on_error;

    // Detect USB Dongle based Class, Subclass, and Protocol
    // The class code (bDeviceClass) is 0xE0 – Wireless Controller. 
    // The SubClass code (bDeviceSubClass) is 0x01 – RF Controller. 
    // The Protocol code (bDeviceProtocol) is 0x01 – Bluetooth programming.
    USB_INTERFACE_DESCRIPTOR usb_interface_descriptor;
    result = WinUsb_QueryInterfaceSettings(usb_interface_0_handle, 0, &usb_interface_descriptor);
    if (!result) goto exit_on_error;

    // ignore virtual Bluetooth adapter of VMware
    if (usb_is_vmware_bluetooth_adapter(device_path)) {
        log_info("Ignoring simulated VMware Bluetooth adapter");
        usb_free_resources();
        return -1;
    }

    // 
    if (usb_interface_descriptor.bInterfaceClass    != 0xe0 ||
        usb_interface_descriptor.bInterfaceSubClass != 0x01 || 
        usb_interface_descriptor.bInterfaceProtocol != 0x01){

        // check whitelist
        if (!usb_is_known_bluetooth_device(device_path)){
            log_info("Class, Subclass, Protocol does not match Bluetooth device");
            usb_free_resources();
            return 0;
        }
    }

#ifdef ENABLE_SCO_OVER_HCI
	log_info("Claiming interface 1...");
	// WinUsb_GetAssociatedInterface returns TRUE if the operation succeeds.
	// We use index 1 - assuming it refers to interface #1 with libusb
	// A value of 0 indicates the first associated interface, a value of 1 indicates the second associated interface, and so on.
	result = WinUsb_GetAssociatedInterface(usb_interface_0_handle, 0, &usb_interface_1_handle);
	if (!result) goto exit_on_error;
	log_info("Claiming interface 1: success");
#endif

    result = usb_scan_for_bluetooth_endpoints();
    if (!result) {
        log_error("Could not find all Bluetooth Endpoints!");
        usb_free_resources();
        return 0;
    }

#ifdef ENABLE_SCO_OVER_HCI
    int i;

	memset(hci_sco_packet_descriptors, 0, sizeof(hci_sco_packet_descriptors));
	log_info("Size of packet descriptors for SCO IN%u", (int) sizeof(hci_sco_packet_descriptors));

	// setup async io && btstack handler
	memset(&usb_overlapped_sco_in, 0, sizeof(usb_overlapped_sco_in));
	for (i=0;i<ISOC_BUFFERS;i++){
		usb_overlapped_sco_in[i].hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		// log_info_hexdump(&usb_overlapped_sco_in[i], sizeof(OVERLAPPED));
        // log_info("data source SCO in %u, handle %p", i, usb_overlapped_sco_in[i].hEvent);
		usb_data_source_sco_in[i].source.handle = usb_overlapped_sco_in[i].hEvent;
	    btstack_run_loop_set_data_source_handler(&usb_data_source_sco_in[i], &usb_process_sco_in);
        btstack_run_loop_add_data_source(&usb_data_source_sco_in[i]);
	}

    memset(&usb_overlapped_sco_out, 0, sizeof(usb_overlapped_sco_out));
    for (i=0;i<SCO_RING_BUFFER_COUNT;i++){
        usb_overlapped_sco_out[i].hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        // log_info("data source SCO out %u, handle %p", i, usb_overlapped_sco_out[i].hEvent);
        usb_data_source_sco_out[i].source.handle = usb_overlapped_sco_out[i].hEvent;
        btstack_run_loop_set_data_source_handler(&usb_data_source_sco_out[i], &usb_process_sco_out);
        btstack_run_loop_add_data_source(&usb_data_source_sco_out[i]);
    }
#endif

	// setup async io
    memset(&usb_overlapped_event_in,     0, sizeof(usb_overlapped_event_in));
    memset(&usb_overlapped_command_out,  0, sizeof(usb_overlapped_command_out));
    memset(&usb_overlapped_acl_out,      0, sizeof(usb_overlapped_acl_out));
    memset(&usb_overlapped_acl_in,       0, sizeof(usb_overlapped_acl_in));
    usb_overlapped_event_in.hEvent    = CreateEvent(NULL, TRUE, FALSE, NULL);
    usb_overlapped_command_out.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    usb_overlapped_acl_in.hEvent      = CreateEvent(NULL, TRUE, FALSE, NULL);
    usb_overlapped_acl_out.hEvent     = CreateEvent(NULL, TRUE, FALSE, NULL);

	// setup btstack data soures
    usb_data_source_event_in.source.handle = usb_overlapped_event_in.hEvent;
    btstack_run_loop_set_data_source_handler(&usb_data_source_event_in, &usb_process_event_in);
    btstack_run_loop_add_data_source(&usb_data_source_event_in);

    usb_data_source_command_out.source.handle = usb_overlapped_command_out.hEvent;
    btstack_run_loop_set_data_source_handler(&usb_data_source_command_out, &usb_process_command_out);
    btstack_run_loop_add_data_source(&usb_data_source_command_out);

    usb_data_source_acl_in.source.handle = usb_overlapped_acl_in.hEvent;
    btstack_run_loop_set_data_source_handler(&usb_data_source_acl_in, &usb_process_acl_in);
    btstack_run_loop_add_data_source(&usb_data_source_acl_in);

    usb_data_source_acl_out.source.handle = usb_overlapped_acl_out.hEvent;
    btstack_run_loop_set_data_source_handler(&usb_data_source_acl_out, &usb_process_acl_out);
    btstack_run_loop_add_data_source(&usb_data_source_acl_out);

    // submit all incoming transfers
    usb_submit_event_in_transfer();
    usb_submit_acl_in_transfer();    
	return 1;

exit_on_error:
	log_error("usb_try_open_device: last error %lu", GetLastError());
	usb_free_resources();
	return 0;
}

#ifdef ENABLE_SCO_OVER_HCI

#define WinUSB_Lookup(fn) do { BTstack_##fn = (BTstack_##fn##_t) GetProcAddress(h, #fn); log_info("%-30s %p", #fn, BTstack_##fn); if (!BTstack_##fn) return FALSE; } while(0)

static BOOL usb_lookup_symbols(void){
	// lookup runtime symbols missing in current mingw64 distribution
	HMODULE h = GetModuleHandleA("WinUSB");
	log_info("%-30s %p", "WinUSB", h);
	WinUSB_Lookup(WinUsb_QueryPipeEx);
	WinUSB_Lookup(WinUsb_RegisterIsochBuffer);
	WinUSB_Lookup(WinUsb_ReadIsochPipe);
	WinUSB_Lookup(WinUsb_ReadIsochPipeAsap);
	WinUSB_Lookup(WinUsb_WriteIsochPipe);
	WinUSB_Lookup(WinUsb_WriteIsochPipeAsap);
	WinUSB_Lookup(WinUsb_UnregisterIsochBuffer);
    WinUSB_Lookup(WinUsb_GetCurrentFrameNumber);
    return TRUE;
}
#endif

// returns 0 on success, -1 otherwise
static int usb_open(void){

    if (usb_transport_open) return 0;

    int r = -1;

#ifdef ENABLE_SCO_OVER_HCI
	BOOL ok = usb_lookup_symbols();
    if (!ok){
        log_error("usb_open: Failed to lookup WinSUB ISOCHRONOUS functions. Please disable ENABLE_SCO_OVER_HCI or use Windows 8.1 or higher");
        return r;
    }
    sco_state_machine_init();
    sco_ring_init();
#endif

	HDEVINFO                         hDevInfo;
	SP_DEVICE_INTERFACE_DATA         DevIntfData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA DevIntfDetailData;
	SP_DEVINFO_DATA                  DevData;

	DWORD dwSize;
	DWORD dwMemberIdx;

    // default endpoint addresses
    event_in_addr = 0x81; // EP1, IN interrupt
    acl_in_addr =   0x82; // EP2, IN bulk
    acl_out_addr =  0x02; // EP2, OUT bulk
    sco_in_addr  =  0x83; // EP3, IN isochronous
    sco_out_addr =  0x03; // EP3, OUT isochronous   

	// We will try to get device information set for all USB devices that have a
	// device interface and are currently present on the system (plugged in).
	hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_USB_DEVICE, NULL, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
	
	log_info("usb_open: SetupDiGetClassDevs -> %p", hDevInfo);
	if (hDevInfo == INVALID_HANDLE_VALUE) return -1;

	// Prepare to enumerate all device interfaces for the device information
	// set that we retrieved with SetupDiGetClassDevs(..)
	DevIntfData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	dwMemberIdx = 0;
	
	// Next, we will keep calling this SetupDiEnumDeviceInterfaces(..) until this
	// function causes GetLastError() to return  ERROR_NO_MORE_ITEMS. With each
	// call the dwMemberIdx value needs to be incremented to retrieve the next
	// device interface information.

	SetupDiEnumDeviceInterfaces(hDevInfo, NULL, (LPGUID) &GUID_DEVINTERFACE_USB_DEVICE,
		dwMemberIdx, &DevIntfData);

	while(GetLastError() != ERROR_NO_MORE_ITEMS){

		// As a last step we will need to get some more details for each
		// of device interface information we are able to retrieve. This
		// device interface detail gives us the information we need to identify
		// the device (VID/PID), and decide if it's useful to us. It will also
		// provide a DEVINFO_DATA structure which we can use to know the serial
		// port name for a virtual com port.

		DevData.cbSize = sizeof(DevData);
		
		// Get the required buffer size. Call SetupDiGetDeviceInterfaceDetail with
		// a NULL DevIntfDetailData pointer, a DevIntfDetailDataSize
		// of zero, and a valid RequiredSize variable. In response to such a call,
		// this function returns the required buffer size at dwSize.

		SetupDiGetDeviceInterfaceDetail(
			  hDevInfo, &DevIntfData, NULL, 0, &dwSize, NULL);

		// Allocate memory for the DeviceInterfaceDetail struct. Don't forget to
		// deallocate it later!
		DevIntfDetailData = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwSize);
		DevIntfDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		if (SetupDiGetDeviceInterfaceDetail(hDevInfo, &DevIntfData,
			DevIntfDetailData, dwSize, &dwSize, &DevData))
		{
			// Finally we can start checking if we've found a useable device,
			// by inspecting the DevIntfDetailData->DevicePath variable.
			// The DevicePath looks something like this:
			//
			// \\?\usb#vid_04d8&pid_0033#5&19f2438f&0&2#{a5dcbf10-6530-11d2-901f-00c04fb951ed}
			//

			log_info("usb_open: Device Path: %s", DevIntfDetailData->DevicePath);

#if 0
            // check for hard-coded vendor/product ids
			char vid_pid_match[30];
			uint16_t vid = 0x0a12;
			uint16_t pid = 0x0001;
			sprintf(vid_pid_match, "\\\\?\\usb#vid_%04x&pid_%04x", vid, pid);
			if (strncmp(DevIntfDetailData->DevicePath, &vid_pid_match[0], strlen(vid_pid_match)) == 0 ){
				log_info("Matched search string %s", vid_pid_match);					

				BOOL result = usb_try_open_device(DevIntfDetailData->DevicePath);
				if (result){
					log_info("usb_open: Device opened, stop scanning");
					r = 0;
				} else {
					log_error("usb_open: Device open failed");					
				}
			}
#endif

            // try all devices
            BOOL result = usb_try_open_device(DevIntfDetailData->DevicePath);
            if (result){
                log_info("usb_open: Device opened, stop scanning");
                r = 0;
            } else {
                log_error("usb_open: Device open failed");                  
            }
        }
		HeapFree(GetProcessHeap(), 0, DevIntfDetailData);

		if (r == 0) break;

		// Continue looping
		SetupDiEnumDeviceInterfaces(
			hDevInfo, NULL, &GUID_DEVINTERFACE_USB_DEVICE, ++dwMemberIdx, &DevIntfData);
	}

	SetupDiDestroyDeviceInfoList(hDevInfo);

	log_info("usb_open: done, r = %x", r);

    if (r == 0){
        // opened
        usb_transport_open = 1;
    }

    return r;    
}

static int usb_close(void){
    
    if (!usb_transport_open) return 0;

    // remove data sources
    btstack_run_loop_remove_data_source(&usb_data_source_command_out);
    btstack_run_loop_remove_data_source(&usb_data_source_event_in);
    btstack_run_loop_remove_data_source(&usb_data_source_acl_in);
    btstack_run_loop_remove_data_source(&usb_data_source_acl_out);

#ifdef ENABLE_SCO_OVER_HCI
    int i;
    for (i=0;i<ISOC_BUFFERS;i++){
        btstack_run_loop_remove_data_source(&usb_data_source_sco_in[i]);
    }
    for (i=0;i<SCO_RING_BUFFER_COUNT;i++){
        btstack_run_loop_remove_data_source(&usb_data_source_sco_out[i]);
    }
#endif

    log_info("usb_close abort event and acl pipes");

    // stop transfers
    WinUsb_AbortPipe(usb_interface_0_handle, event_in_addr);
    WinUsb_AbortPipe(usb_interface_0_handle, acl_in_addr);
    WinUsb_AbortPipe(usb_interface_0_handle, acl_out_addr);
#ifdef ENABLE_SCO_OVER_HCI
    usb_sco_stop();
#endif
    usb_acl_out_active = 0;

    // control transfer cannot be stopped, just wait for completion
    if (usb_command_out_active){
        log_info("usb_close command out active, wait for complete");
        DWORD bytes_transferred;
        WinUsb_GetOverlappedResult(usb_interface_0_handle, &usb_overlapped_command_out, &bytes_transferred, TRUE);
        usb_command_out_active = 0;
    }

    log_info("usb_close free resources");

    // free everything
    usb_free_resources();

    // transport closed
    usb_transport_open = 0;
    
    return 0;    
}

static int usb_can_send_packet_now(uint8_t packet_type){
    // return 0;
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
            return !usb_command_out_active;
        case HCI_ACL_DATA_PACKET:
            return !usb_acl_out_active;
#ifdef ENABLE_SCO_OVER_HCI
        case HCI_SCO_DATA_PACKET:
            // return 0;
            return sco_ring_have_space();
#endif
        default:
            return 0;
    }
}

static int usb_send_cmd_packet(uint8_t *packet, int size){

    // update stata before submitting transfer
    usb_command_out_active = 1;

	// Start trasnsfer
	WINUSB_SETUP_PACKET setup_packet;
	memset(&setup_packet, 0, sizeof(setup_packet));
	setup_packet.RequestType =  USB_REQUEST_TYPE_CLASS | USB_RECIPIENT_INTERFACE;
	setup_packet.Length = sizeof(size);
	BOOL result = WinUsb_ControlTransfer(usb_interface_0_handle, setup_packet, packet, size,  NULL, &usb_overlapped_command_out);
	if (!result) {
		if (GetLastError() != ERROR_IO_PENDING) goto exit_on_error;
	}

    // IO_PENDING -> wait for completed
    btstack_run_loop_enable_data_source_callbacks(&usb_data_source_command_out, DATA_SOURCE_CALLBACK_WRITE);

    return 0;

exit_on_error:
	log_error("winusb: last error %lu", GetLastError());
	return -1;
}

static int usb_send_acl_packet(uint8_t *packet, int size){

    // update stata before submitting transfer
    usb_acl_out_active = 1;

	// Start trasnsfer
	BOOL ok = WinUsb_WritePipe(usb_interface_0_handle, acl_out_addr, packet, size,  NULL, &usb_overlapped_acl_out);
	if (!ok) {
		if (GetLastError() != ERROR_IO_PENDING) goto exit_on_error;
	}

    // IO_PENDING -> wait for completed
    btstack_run_loop_enable_data_source_callbacks(&usb_data_source_acl_out, DATA_SOURCE_CALLBACK_WRITE);
    return 0;

exit_on_error:
	log_error("winusb: last error %lu", GetLastError());
	return -1;
}

#ifdef ENABLE_SCO_OVER_HCI
static int usb_send_sco_packet(uint8_t *packet, int size){

    if (size > SCO_PACKET_SIZE){
        log_error("usb_send_sco_packet: size %u > SCO_PACKET_SIZE %u", size, SCO_PACKET_SIZE);
        return -1;
    }

    // get current frame number
    ULONG current_frame_number;
    LARGE_INTEGER timestamp;
    BTstack_WinUsb_GetCurrentFrameNumber(usb_interface_0_handle, &current_frame_number, &timestamp);

    // store packet in free slot
    int transfer_index = sco_ring_write;
    uint8_t * data = &sco_ring_buffer[transfer_index * SCO_PACKET_SIZE];
    memcpy(data, packet, size);


    // setup transfer
    int continue_stream = sco_ring_transfers_active > 0;
    BOOL ok = BTstack_WinUsb_WriteIsochPipeAsap(hci_sco_out_buffer_handle, transfer_index * SCO_PACKET_SIZE, size, continue_stream, &usb_overlapped_sco_out[transfer_index]);
    // log_info("usb_send_sco_packet: using slot #%02u, current frame %lu, continue stream %u, ok %u", transfer_index, current_frame_number, continue_stream, ok);
    if (!ok) {
        if (GetLastError() != ERROR_IO_PENDING) goto exit_on_error;
    }

    // successful started transfer, enable data source callback if first active transfer
    if (sco_ring_transfers_active == 0){
        usb_sco_out_expected_transfer = transfer_index;
        btstack_run_loop_enable_data_source_callbacks(&usb_data_source_sco_out[transfer_index], DATA_SOURCE_CALLBACK_WRITE);
    }

    // mark slot as full
    sco_ring_write = (sco_ring_write + 1) % SCO_RING_BUFFER_COUNT;
    sco_ring_transfers_active++;

    // notify upper stack that provided buffer can be used again
    uint8_t event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};
    packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));

    // log_info("usb_send_sco_packet: transfers active %u", sco_ring_transfers_active);

    // and if we have more space for SCO packets
    if (sco_ring_have_space()) {
        uint8_t event_sco[] = { HCI_EVENT_SCO_CAN_SEND_NOW, 0};
        packet_handler(HCI_EVENT_PACKET, &event_sco[0], sizeof(event_sco));
    }
    return 0;

exit_on_error:
    log_error("usb_send_sco_packet: last error %lu", GetLastError());
    return -1;
}
#endif

static int usb_send_packet(uint8_t packet_type, uint8_t * packet, int size){
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
            return usb_send_cmd_packet(packet, size);
        case HCI_ACL_DATA_PACKET:
            return usb_send_acl_packet(packet, size);
#ifdef ENABLE_SCO_OVER_HCI
        case HCI_SCO_DATA_PACKET:
            return usb_send_sco_packet(packet, size);
#endif
        default:
            return -1;
    }
}

#ifdef ENABLE_SCO_OVER_HCI
static void usb_set_sco_config(uint16_t voice_setting, int num_connections){
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

// get usb singleton
static const hci_transport_t hci_transport_usb = {
    /* const char * name; */                                        "H2_WINUSB",
    /* void   (*init) (const void *transport_config); */            &usb_init,
    /* int    (*open)(void); */                                     &usb_open,
    /* int    (*close)(void); */                                    &usb_close,
    /* void   (*register_packet_handler)(void (*handler)(...); */   &usb_register_packet_handler,
    /* int    (*can_send_packet_now)(uint8_t packet_type); */       &usb_can_send_packet_now,
    /* int    (*send_packet)(...); */                               &usb_send_packet,
    /* int    (*set_baudrate)(uint32_t baudrate); */                NULL,
    /* void   (*reset_link)(void); */                               NULL,
#ifdef ENABLE_SCO_OVER_HCI
    /* void   (*set_sco_config)(uint16_t voice_setting, int num_connections); */ usb_set_sco_config, 
#else
    /* void   (*set_sco_config)(uint16_t voice_setting, int num_connections); */ NULL, 
#endif    
};

const hci_transport_t * hci_transport_usb_instance(void) {
    return &hci_transport_usb;
}
