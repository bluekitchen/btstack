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

#include "btstack_config.h"

#include "btstack_debug.h"
#include "hci.h"
#include "hci_transport.h"

#include <Windows.h>
#include <SetupAPI.h>
#include <Winusb.h>

#if 1

// Isochronous Add-On

typedef struct _WINUSB_PIPE_INFORMATION_EX {
  USBD_PIPE_TYPE PipeType;
  UCHAR          PipeId;
  USHORT         MaximumPacketSize;
  UCHAR          Interval;
  ULONG          MaximumBytesPerInterval;
} WINUSB_PIPE_INFORMATION_EX, *PWINUSB_PIPE_INFORMATION_EX;

typedef PVOID WINUSB_ISOCH_BUFFER_HANDLE, *PWINUSB_ISOCH_BUFFER_HANDLE;

typedef WINBOOL (WINAPI * WinUsb_QueryPipeEx_t) (
	WINUSB_INTERFACE_HANDLE 	InterfaceHandle,
	UCHAR						AlternateInterfaceNumber,
	UCHAR 						PipeIndex,
	PWINUSB_PIPE_INFORMATION_EX PipeInformationEx
);
typedef WINBOOL (WINAPI * WinUsb_RegisterIsochBuffer_t)(
	WINUSB_INTERFACE_HANDLE     InterfaceHandle,
	UCHAR                       PipeID,
	PVOID                       Buffer,
	ULONG                       BufferLength,
	PWINUSB_ISOCH_BUFFER_HANDLE BufferHandle
);
typedef WINBOOL (WINAPI * WinUsb_ReadIsochPipe_t)(
    PWINUSB_ISOCH_BUFFER_HANDLE BufferHandle,
    ULONG                       Offset,
    ULONG                       Length,
    PULONG                      FrameNumber,
    PULONG                      NumberOfPackets,
    PULONG                      IsoPacketDescriptors,
    LPOVERLAPPED                Overlapped
);
typedef WINBOOL (WINAPI * WinUsb_ReadIsochPipeAsap_t)(
    PWINUSB_ISOCH_BUFFER_HANDLE BufferHandle,
    ULONG                       Offset,
    ULONG                       Length,
    BOOL                        ContinueStream,
    ULONG                      	NumberOfPackets,
    PUSBD_ISO_PACKET_DESCRIPTOR IsoPacketDescriptors,
 	LPOVERLAPPED                Overlapped
);
typedef WINBOOL (WINAPI * WinUsb_WriteIsochPipe_t)(
    PWINUSB_ISOCH_BUFFER_HANDLE BufferHandle,
    ULONG                       Offset,
    ULONG                       Length,
    PULONG                      FrameNumber,
	LPOVERLAPPED                Overlapped
);
typedef WINBOOL (WINAPI * WinUsb_WriteIsochPipeAsap_t)(
    PWINUSB_ISOCH_BUFFER_HANDLE BufferHandle,
    ULONG                       Offset,
    ULONG                       Length,
    BOOL                        ContinueStream,
	LPOVERLAPPED                Overlapped
);
typedef WINBOOL (WINAPI * WinUsb_UnregisterIsochBuffer_t)(
	PWINUSB_ISOCH_BUFFER_HANDLE BufferHandle
);

static WinUsb_QueryPipeEx_t 			WinUsb_QueryPipeEx;
static WinUsb_RegisterIsochBuffer_t 	WinUsb_RegisterIsochBuffer;
static WinUsb_ReadIsochPipe_t 			WinUsb_ReadIsochPipe;
static WinUsb_ReadIsochPipeAsap_t 		WinUsb_ReadIsochPipeAsap;
static WinUsb_WriteIsochPipe_t 			WinUsb_WriteIsochPipe;
static WinUsb_WriteIsochPipeAsap_t 		WinUsb_WriteIsochPipeAsap;
static WinUsb_UnregisterIsochBuffer_t 	WinUsb_UnregisterIsochBuffer;

#endif

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

// #define ASYNC_BUFFERS  10


// for ALT_SETTING >= 1 and 8-bit channel, we need the following isochronous packets
// One complete SCO packet with 24 frames every 3 frames (== 3 ms)
#define NUM_ISO_PACKETS (10)

// results in 9 bytes per frame
#define ISO_PACKET_SIZE (9)

// 49 bytes is the max usb packet size for alternate setting 5 (Three 8 kHz 16-bit channels or one 8 kHz 16-bit channel and one 16 kHz 16-bit channel)
// note: alt setting 6 has max packet size of 63 every 7.5 ms = 472.5 bytes / HCI packet, while max SCO packet has 255 byte payload
#define SCO_PACKET_SIZE (NUM_ISO_PACKETS * ISO_PACKET_SIZE)

#define ISOC_BUFFERS   20

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

// SCO Incoming Windows
static uint8_t hci_sco_in_buffer[ISOC_BUFFERS * SCO_PACKET_SIZE]; 
static WINUSB_ISOCH_BUFFER_HANDLE hci_sco_in_buffer_handle;
static USBD_ISO_PACKET_DESCRIPTOR hci_sco_packet_descriptors[ISOC_BUFFERS * NUM_ISO_PACKETS];
// static OVERLAPPED canary_overlapped;
static OVERLAPPED usb_overlapped_sco_in[ISOC_BUFFERS];

// SCO Incoming Run Loop
static btstack_data_source_t usb_data_source_sco_in[ISOC_BUFFERS];

// SCO Incoming HCI
typedef enum {
    H2_W4_SCO_HEADER = 1,
    H2_W4_PAYLOAD,
} H2_SCO_STATE;
static H2_SCO_STATE sco_state;
static uint8_t  sco_buffer[SCO_PACKET_SIZE];
static uint16_t sco_read_pos;
static uint16_t sco_bytes_to_read;

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
	log_error("usb_submit_event_in_transfer: winusb last error %lu\n", GetLastError());
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
	log_error("usb_submit_acl_in_transfer: winusb last error %lu\n", GetLastError());
}

static void usb_submit_sco_in_transfer(int i, int continue_stream){

	// log_info("WinUsb_ReadIsochPipeAsap #%u - handle, %u, %u, %u, %u, %p, %p", i, i * SCO_PACKET_SIZE, SCO_PACKET_SIZE, (i == 0) ? FALSE : TRUE, NUM_ISO_PACKETS, &hci_sco_packet_descriptors[i * NUM_ISO_PACKETS], &usb_overlapped_sco_in[i]);
	// log_info_hexdump(&usb_overlapped_sco_in[i], sizeof(OVERLAPPED));
	// log_info_hexdump(&canary_overlapped,        sizeof(OVERLAPPED));

	BOOL result = WinUsb_ReadIsochPipeAsap(hci_sco_in_buffer_handle, i * SCO_PACKET_SIZE, SCO_PACKET_SIZE,  
		continue_stream, NUM_ISO_PACKETS, &hci_sco_packet_descriptors[i * NUM_ISO_PACKETS], &usb_overlapped_sco_in[i]);

	if (!result) {
		if (GetLastError() != ERROR_IO_PENDING) goto exit_on_error;
	}

#if 0	// check result
	DWORD bytes_transferred;
	BOOL ok = WinUsb_GetOverlappedResult(usb_interface_0_handle, &usb_overlapped_sco_in[i], &bytes_transferred, FALSE);
	if (!ok){
		int err = (int) GetLastError();
		if (err == 87 && continue_stream == 1){
			// invalid parameters, try without continue
			log_info("usb_submit_sco_in_transfer[%u] error 87, reset pipe", i);
			result = WinUsb_ResetPipe(usb_interface_1_handle, sco_in_addr);
			log_info("usb_submit_sco_in_transfer[%u] result %u, error %u - retry without continue flag", i, result, (int) GetLastError());
			WinUsb_ReadIsochPipeAsap(hci_sco_in_buffer_handle, i * SCO_PACKET_SIZE, SCO_PACKET_SIZE,  
				0, NUM_ISO_PACKETS, &hci_sco_packet_descriptors[i * NUM_ISO_PACKETS], &usb_overlapped_sco_in[i]);
		} else {
			log_info("WinUsb_GetOverlappedResult:[%u] error %u", i, (int) GetLastError());
		}
	}
#endif

	// IO_PENDING -> wait for completed
    btstack_run_loop_enable_data_source_callbacks(&usb_data_source_sco_in[i], DATA_SOURCE_CALLBACK_READ);
	return;

exit_on_error:
	log_error("usb_submit_sco_in_transfer: winusb last error %lu\n", GetLastError());
}

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
    packet_handler(HCI_EVENT_PACKET, hci_event_in_buffer, bytes_read);

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
            log_error("usb_process_acl_in: error writing");
        }
        return;
    }

    // notify uppper
    packet_handler(HCI_ACL_DATA_PACKET, hci_acl_in_buffer, bytes_read);

	// re-submit transfer
	usb_submit_acl_in_transfer();
}

static void sco_state_machine_init(void){
    sco_state = H2_W4_SCO_HEADER;
    sco_read_pos = 0;
    sco_bytes_to_read = 3;
}

static void sco_handle_data(uint8_t * buffer, uint16_t size){
	// printf("sco_handle_data: state %u, pos %u, to read %u, size %u\n", sco_state, sco_read_pos, sco_bytes_to_read, size);
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
        }
    }
}

static void usb_process_sco_in(btstack_data_source_t *ds,  btstack_data_source_callback_type_t callback_type){

    btstack_run_loop_disable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_READ);

    // find index
    int i;
    for (i=0;i<ISOC_BUFFERS;i++){
    	if (ds == &usb_data_source_sco_in[i]) break;
    }
    // log_info("usb_process_sco_in[%u]", i);
    int transfer_index = i;

	DWORD bytes_transferred;
	BOOL ok = WinUsb_GetOverlappedResult(usb_interface_0_handle, &usb_overlapped_sco_in[transfer_index], &bytes_transferred, FALSE);
	// log_info("usb_process_sco_in_done: #%u result %u, bytes %u, state %u", transfer_index, result, (int) bytes_transferred, sco_state);
    if(!ok){
        DWORD err = GetLastError();
        if (err == ERROR_IO_INCOMPLETE){
            // IO_INCOMPLETE -> wait for completed
            btstack_run_loop_enable_data_source_callbacks(&usb_data_source_sco_in[transfer_index], DATA_SOURCE_CALLBACK_READ);
	        return;
        } else {
            log_error("usb_process_sco_in_done[%u]: error reading %u, Internal %x", transfer_index, (int) err, (int) usb_overlapped_sco_in[i].Internal);
			log_info_hexdump(&usb_overlapped_sco_in[transfer_index], sizeof(OVERLAPPED));
			// log_info_hexdump(&canary_overlapped, sizeof(OVERLAPPED));
			return;
        }
    }

    if (ok){
		for (i=0;i<NUM_ISO_PACKETS;i++){
			USBD_ISO_PACKET_DESCRIPTOR * packet_descriptor = &hci_sco_packet_descriptors[transfer_index * NUM_ISO_PACKETS + i];
			if (packet_descriptor->Length){
				uint8_t * iso_data = &hci_sco_in_buffer[transfer_index * SCO_PACKET_SIZE + packet_descriptor->Offset];
				uint16_t  iso_len  = packet_descriptor->Length; 
				// log_info_hexdump(iso_data, iso_len);
				sco_handle_data(iso_data, iso_len);
			}
		}
    }

	if (!ok) return;

    usb_submit_sco_in_transfer(transfer_index, 1);
}

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

#if 0
// Scan for Bluetooth Endpoints
// example: https://android.googlesource.com/platform/development/+/master/host/windows/usb/winusb/adb_winusb_interface.cpp
// example: https://msdn.microsoft.com/en-us/windows/hardware/dn376866(v=vs.85)
static void usb_scan_for_bluetooth_endpoints(void) {
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
	log_info("USB Dev: %p", usb_device_handle);
	if (!usb_device_handle) goto exit_on_error;

	log_info("Claiming interface 0...");

	// USB_INTERFACE_DESCRIPTOR iface_descriptor;
	// WinUsb_Initialize returns TRUE if the operation succeed
	BOOL result = WinUsb_Initialize(usb_device_handle, &usb_interface_0_handle);
	if (!result) goto exit_on_error;
	log_info("Claiming interface 0: success, handle %p", usb_interface_0_handle);

#ifdef ENABLE_SCO_OVER_HCI
	log_info("Claiming interface 1...");
	// WinUsb_GetAssociatedInterface returns TRUE if the operation succeeds.
	// We use index 1 - assuming it refers to interface #1 with libusb
	// A value of 0 indicates the first associated interface, a value of 1 indicates the second associated interface, and so on.
	result = WinUsb_GetAssociatedInterface(usb_interface_0_handle, 0, &usb_interface_1_handle);
	if (!result) goto exit_on_error;
	log_info("Claiming interface 1: success");

#if 1
	log_info("Switching to setting %u on interface 1..", ALT_SETTING);
	// WinUsb_SetCurrentAlternateSetting returns TRUE if the operation succeeds.
	result = WinUsb_SetCurrentAlternateSetting(usb_interface_1_handle, ALT_SETTING);
	if (!result) goto exit_on_error;
#endif

	// check ISO Pipes
	USB_INTERFACE_DESCRIPTOR usb_interface_descriptor;
	result = WinUsb_QueryInterfaceSettings(usb_interface_1_handle, ALT_SETTING, &usb_interface_descriptor);
	if (!result) goto exit_on_error;
	int i;
	for (i=0;i<usb_interface_descriptor.bNumEndpoints;i++){
		WINUSB_PIPE_INFORMATION_EX pipe;
		result = WinUsb_QueryPipeEx(
                 	 usb_interface_1_handle,
                     ALT_SETTING,
                     (UCHAR) i,
                     &pipe);
		if (!result) continue;
		log_info("Pipe idx #%u: type %u, id 0x%02x, max packet size %u, interval %u, max bytes per interval %u",
			i, pipe.PipeType, pipe.PipeId, pipe.MaximumPacketSize, pipe.Interval, (int) pipe.MaximumBytesPerInterval);

		// 
	}

	// AUTO_CLEAR_STALL, RAW_IO is not callable for ISO EP
	// uint8_t value_on = 1;
	// result = WinUsb_SetPipePolicy(usb_interface_1_handle, sco_in_addr, RAW_IO, sizeof(value_on), &value_on);
	// if (!result) goto exit_on_error;

	memset(hci_sco_packet_descriptors, 0, sizeof(hci_sco_packet_descriptors));
	log_info("Size of packet descriptors %u", (int) sizeof(hci_sco_packet_descriptors));
	result = WinUsb_RegisterIsochBuffer(usb_interface_1_handle, sco_in_addr, hci_sco_in_buffer, ISOC_BUFFERS * SCO_PACKET_SIZE, &hci_sco_in_buffer_handle);
	if (!result) goto exit_on_error;
	log_info("hci_sco_in_buffer_handle %p", hci_sco_in_buffer_handle);

	// setup overlapped && btstack handler
	memset(&usb_overlapped_sco_in, 0, sizeof(usb_overlapped_sco_in));
	for (i=0;i<ISOC_BUFFERS;i++){
		usb_overlapped_sco_in[i].hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		log_info_hexdump(&usb_overlapped_sco_in[i], sizeof(OVERLAPPED));
		usb_data_source_sco_in[i].handle = usb_overlapped_sco_in[i].hEvent;
	    btstack_run_loop_set_data_source_handler(&usb_data_source_sco_in[i], &usb_process_sco_in);
	    btstack_run_loop_add_data_source(&usb_data_source_sco_in[i]);
	}

	// test code to not miss a packet/frame

// while (1){
	for (i=0;i<ISOC_BUFFERS;i++){
		usb_submit_sco_in_transfer(i, 0);
#if 0
		HANDLE handles[1];
		handles[0] = usb_overlapped_sco_in[i].hEvent;
		int res = WaitForMultipleObjects(1, &handles[0], FALSE, INFINITE);
		log_info("WaitForMultipleObjects res %x", res);

		// usb_process_sco_in_done(0);
#else
	    // IO_PENDING -> wait for completed
	    // btstack_run_loop_enable_data_source_callbacks(&usb_data_source_sco_in[i], DATA_SOURCE_CALLBACK_READ);
#endif
	// }
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
    usb_data_source_event_in.handle = usb_overlapped_event_in.hEvent;
    btstack_run_loop_set_data_source_handler(&usb_data_source_event_in, &usb_process_event_in);
    btstack_run_loop_add_data_source(&usb_data_source_event_in);

    usb_data_source_command_out.handle = usb_overlapped_command_out.hEvent;
    btstack_run_loop_set_data_source_handler(&usb_data_source_command_out, &usb_process_command_out);
    btstack_run_loop_add_data_source(&usb_data_source_command_out);

    usb_data_source_acl_in.handle = usb_overlapped_acl_in.hEvent;
    btstack_run_loop_set_data_source_handler(&usb_data_source_acl_in, &usb_process_acl_in);
    btstack_run_loop_add_data_source(&usb_data_source_acl_in);

    usb_data_source_acl_out.handle = usb_overlapped_acl_out.hEvent;
    btstack_run_loop_set_data_source_handler(&usb_data_source_acl_out, &usb_process_acl_out);
    btstack_run_loop_add_data_source(&usb_data_source_acl_out);

	// re-submit transfer
	usb_submit_event_in_transfer();
	usb_submit_acl_in_transfer();
	return 1;

exit_on_error:
	log_error("winusb: last error %lu\n", GetLastError());
	usb_free_resources();
	return 0;
}

#define WinUSB_Lookup(fn) do { fn = (fn##_t) GetProcAddress(h, #fn); log_info("%30s %p", #fn, fn); if (!fn) return; } while(0)

static void usb_lookup_symbols(void){
	// lookup runtime symbols missing in current mingw64 distribution
	HMODULE h = GetModuleHandleA("WinUSB");
	log_info("%30s %p", "WinUSB", h);
	WinUSB_Lookup(WinUsb_QueryPipeEx);
	WinUSB_Lookup(WinUsb_RegisterIsochBuffer);
	WinUSB_Lookup(WinUsb_ReadIsochPipe);
	WinUSB_Lookup(WinUsb_ReadIsochPipeAsap);
	WinUSB_Lookup(WinUsb_WriteIsochPipe);
	WinUSB_Lookup(WinUsb_WriteIsochPipeAsap);
	WinUSB_Lookup(WinUsb_UnregisterIsochBuffer);
}

// returns 0 on success, -1 otherwise
static int usb_open(void){

	usb_lookup_symbols();

    int r = -1;

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

	sco_state_machine_init();

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
			// to access USB Class, Subclass, .. go through registry
			// https://msdn.microsoft.com/en-us/library/windows/hardware/jj649944(v=vs.85).aspx

			// DWORD dwType;
			// HKEY hKey;
			// BYTE lpData[1024];

			hKey = SetupDiOpenDevRegKey(
						hDevInfo,
						&DevData,
						DICS_FLAG_GLOBAL,
						0,
						DIREG_DEV,
						KEY_READ
					);

			dwType = REG_SZ;
			dwSize = sizeof(lpData);
			RegQueryValueEx(hKey, _T("PortName"), NULL, &dwType, lpData, &dwSize);
			RegCloseKey(hKey);

#endif				

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
		}

		HeapFree(GetProcessHeap(), 0, DevIntfDetailData);

		if (r == 0) break;

		// Continue looping
		SetupDiEnumDeviceInterfaces(
			hDevInfo, NULL, &GUID_DEVINTERFACE_USB_DEVICE, ++dwMemberIdx, &DevIntfData);
	}

	SetupDiDestroyDeviceInfoList(hDevInfo);

	log_info("usb_open: done\n");

    return r;    
}

static int usb_close(void){
    int r = -1;
    usb_free_resources();
    return r;    
}

static int usb_can_send_packet_now(uint8_t packet_type){
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
            return !usb_command_out_active;
        case HCI_ACL_DATA_PACKET:
            return !usb_acl_out_active;
#if 0
#ifdef ENABLE_SCO_OVER_HCI
        case HCI_SCO_DATA_PACKET:
            return sco_ring_have_space();
#endif
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
	log_error("winusb: last error %lu\n", GetLastError());
	return -1;
}

static int usb_send_acl_packet(uint8_t *packet, int size){

    // update stata before submitting transfer
    usb_acl_out_active = 1;

	// Start trasnsfer
	BOOL result = WinUsb_WritePipe(usb_interface_0_handle, acl_out_addr, packet, size,  NULL, &usb_overlapped_acl_out);
	if (!result) {
		if (GetLastError() != ERROR_IO_PENDING) goto exit_on_error;
	}

    // IO_PENDING -> wait for completed
    btstack_run_loop_enable_data_source_callbacks(&usb_data_source_acl_out, DATA_SOURCE_CALLBACK_WRITE);
    return 0;

exit_on_error:
	log_error("winusb: last error %lu\n", GetLastError());
	return -1;
}


static int usb_send_packet(uint8_t packet_type, uint8_t * packet, int size){
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
            return usb_send_cmd_packet(packet, size);
        case HCI_ACL_DATA_PACKET:
            return usb_send_acl_packet(packet, size);
#if 0
#ifdef ENABLE_SCO_OVER_HCI
        case HCI_SCO_DATA_PACKET:
            return usb_send_sco_packet(packet, size);
#endif
#endif
        default:
            return -1;
    }
}

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
};

const hci_transport_t * hci_transport_usb_instance(void) {
    return &hci_transport_usb;
}
