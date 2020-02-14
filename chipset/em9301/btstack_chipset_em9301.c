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

#define BTSTACK_FILE__ "btstack_chipset_em9301.c"

/*
 *  btstack_chipset_em9301.c
 *
 *  Adapter to use em9301-based chipsets with BTstack
 *  
 *  Allows to set public BD ADDR
 */

#include "btstack_config.h"
#include "btstack_chipset_em9301.h"
#include "btstack_debug.h"

#include <stddef.h>   /* NULL */
#include <string.h>   /* memcpy */
#include "hci.h"

// general vendor opcodes
#define HCI_OPCODE_EM_SET_PUBLIC_ADDRESS                0x02
#define HCI_OPCODE_EM_SET_UART_BAUD_RATE                0x07
#define HCI_OPCODE_EM_TRANSMITTER_TEST                  0x11
#define HCI_OPCODE_EM_TRANSMITTER_TEST_END              0x12

// EM9301 vendor opcodes
#define HCI_OPCODE_EM_WRITE_DATA                        0x00
#define HCI_OPCODE_EM_READ_DATA                         0x01
#define HCI_OPCODE_EM_SET_OPERATING_STATE               0x03
#define HCI_OPCODE_EM_WRITE_PROGRAM                     0x80  // only in IPS mode available
#define HCI_OPCODE_EM_CALC_CRC_CODE                     0x81  // only in IPS mode available

// EM9304 vendor opcodes
#define HCI_OPCODE_EM_WRITE_PATCH_START                 0x27
#define HCI_OPCODE_EM_WRITE_PATCH_CONTINUE              0x28
#define HCI_OPCODE_EM_WRITE_PATCH_ABORT                 0x29
#define HCI_OPCODE_EM_SET_MEMORY_MODE                   0x2B
#define HCI_OPCODE_EM_SET_SLEEP_OPTIONS                 0x2D
#define HCI_OPCODE_EM_CPU_RESET                         0x32
#define HCI_OPCODE_EM_PATCH_QUERY                       0x34

/**
 * @param bd_addr
 */
const hci_cmd_t hci_vendor_em_set_public_address = {
	HCI_CMD_OPCODE(OGF_VENDOR, HCI_OPCODE_EM_SET_PUBLIC_ADDRESS), "B"
};

/**
 * @param baud_rate_index
 */
const hci_cmd_t hci_vendor_em_set_uart_baudrate = {
	HCI_CMD_OPCODE(OGF_VENDOR, HCI_OPCODE_EM_SET_UART_BAUD_RATE), "1"
};

/**
 * @param transmitter_test_mode
 * @param channel_number
 * @param packet_length
 * @param packet_payload_type
 */
const hci_cmd_t hci_vendor_em_transmitter_test = {
	HCI_CMD_OPCODE(OGF_VENDOR, HCI_OPCODE_EM_TRANSMITTER_TEST), "1111"
};

/**
 */
const hci_cmd_t hci_vendor_em_transmitter_test_end = {
	HCI_CMD_OPCODE(OGF_VENDOR, HCI_OPCODE_EM_TRANSMITTER_TEST_END), ""
};

/**
 * @param patch_index
 */
const hci_cmd_t hci_vendor_em_patch_query = {
	HCI_CMD_OPCODE(OGF_VENDOR, HCI_OPCODE_EM_PATCH_QUERY), "2"
};

/**
 * Change the state of the selected memory.
 * @param memory_attribute
 */
const hci_cmd_t hci_vendor_em_set_memory_mode = {
	HCI_CMD_OPCODE(OGF_VENDOR, HCI_OPCODE_EM_SET_MEMORY_MODE), "1"
};

/**
 * @param sleep_option_settings
 */
const hci_cmd_t hci_vendor_em_set_sleep_options = {
	HCI_CMD_OPCODE(OGF_VENDOR, HCI_OPCODE_EM_SET_SLEEP_OPTIONS), "1"
};

/**
 * @param operating_state_settings
 */
const hci_cmd_t hci_vendor_em_set_operating_state = {
	HCI_CMD_OPCODE(OGF_VENDOR, HCI_OPCODE_EM_SET_OPERATING_STATE), "1"
};

// baudrate to index for hci_vendor_em_set_uart_baudrate
static const uint32_t baudrates[] = {
	      0, 
	      0,
	      0, 
	   9600,
	  14400,
	  19200,
	  28800,
	  38400,
	  57600,
	  76800,
	 115200,
	 230400,
	 460800,
	 921600,
	1843200,
};

#ifdef HAVE_EM9304_PATCH_CONTAINER

extern const uint8_t   container_blob_data[];
extern const uint32_t  container_blob_size;

static uint32_t container_blob_offset  = 0;
static uint32_t container_end;	// current container
static uint16_t patch_sequence_number;
static int      em_cpu_reset_sent;

static enum {
	UPLOAD_IDLE,
	UPLOAD_ACTIVE,
} upload_state;


// CRC32 implementation using 4-bit lookup table created by pycrc v0.9.1, https://pycrc.org
// ./pycrc.py --model crc-32 --algorithm table-driven --table-idx-width=4 --generate c
static const uint32_t crc32_table[16] = { 
	0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
	0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

static uint32_t btstack_crc32(const uint8_t *buf, uint16_t size){
	uint16_t pos;
	uint32_t crc = 0xffffffff;
	for (pos=0 ; pos<size ; pos++){
        int tbl_idx = crc ^ buf[pos];
        crc = crc32_table[tbl_idx & 0x0f] ^ (crc >> 4);
        tbl_idx = crc ^ (buf[pos] >> 4);
        crc = crc32_table[tbl_idx & 0x0f] ^ (crc >> 4);
    }
	return ~crc;
}

#endif

static void chipset_set_bd_addr_command(bd_addr_t addr, uint8_t *hci_cmd_buffer){
    little_endian_store_16(hci_cmd_buffer, 0, hci_vendor_em_set_public_address.opcode);
    hci_cmd_buffer[2] = 0x06;
    reverse_bd_addr(addr, &hci_cmd_buffer[3]);
}

static void chipset_set_baudrate_command(uint32_t baudrate, uint8_t *hci_cmd_buffer){
	// lookup baudrates
	int i;
	int found = 0;
	for (i=0 ; i < sizeof(baudrates)/sizeof(uint32_t) ; i++){
		if (baudrates[i] == baudrate){
			found = i;
			break;		
		}
	}
	if (!found){
		log_error("Baudrate %u not found in table", baudrate);
		return;
	}
    little_endian_store_16(hci_cmd_buffer, 0, hci_vendor_em_set_uart_baudrate.opcode);
    hci_cmd_buffer[2] = 0x01;
    hci_cmd_buffer[3] = i;
}

#ifdef HAVE_EM9304_PATCH_CONTAINER
static void chipset_em9304_init(const void * config){
	UNUSED(config);
	container_blob_offset = 0;
	em_cpu_reset_sent = 0;
	upload_state = UPLOAD_IDLE;
}

static btstack_chipset_result_t chipset_em9304_next_command(uint8_t * hci_cmd_buffer){
	log_info("pos %u, container end %u, blob size %u", container_blob_offset, container_end, container_blob_size);

    if (container_blob_offset >= container_blob_size) {
    	if (0 == em_cpu_reset_sent){
    		// send EM CPU Reset
		    little_endian_store_16(hci_cmd_buffer, 0, HCI_CMD_OPCODE(OGF_VENDOR, HCI_OPCODE_EM_CPU_RESET));
		    hci_cmd_buffer[2] = 0;
		    em_cpu_reset_sent = 1;
		    return BTSTACK_CHIPSET_VALID_COMMAND;
    	} else {
	        return BTSTACK_CHIPSET_DONE;
    	}
    }

    uint32_t tag;
    uint16_t bytes_to_upload;
    uint32_t crc;
    uint32_t container_size;

	switch (upload_state){
		case UPLOAD_IDLE:
			// check for 'em93' tag
			tag = little_endian_read_32(container_blob_data, container_blob_offset);
			if (0x656d3933 != tag) {
				log_error("Expected 0x656d3933 ('em934') but got %08x", (int) tag);
				return BTSTACK_CHIPSET_DONE;
			}
			// fetch info for current container
			container_size = little_endian_read_32(container_blob_data, container_blob_offset + 4);
			container_end = container_blob_offset + container_size;
			// start uploading (<= 59 bytes)
			patch_sequence_number = 1;
			bytes_to_upload = btstack_min(59, container_end - container_blob_offset);
			crc = btstack_crc32(&container_blob_data[container_blob_offset], bytes_to_upload); 
			log_info("Container type 0x%02x, id %u, build nr %u, user build nr %u, size %u", 
				(int) container_blob_data[container_blob_offset+9], 
				(int) container_blob_data[container_blob_offset+10], 
				(int) little_endian_read_16(container_blob_data, container_blob_offset+12), 
				(int) little_endian_read_16(container_blob_data, container_blob_offset+14), 
				(int) container_size);
			// build command
		    little_endian_store_16(hci_cmd_buffer, 0, HCI_CMD_OPCODE(OGF_VENDOR, HCI_OPCODE_EM_WRITE_PATCH_START));
		    hci_cmd_buffer[2] = 5 + bytes_to_upload;
		    hci_cmd_buffer[3] = 0;	// upload to iRAM1
		    little_endian_store_32(hci_cmd_buffer, 4, crc);
		    memcpy(&hci_cmd_buffer[8], &container_blob_data[container_blob_offset], bytes_to_upload);
		    container_blob_offset += bytes_to_upload;
		    if (container_blob_offset < container_end){
		    	upload_state = UPLOAD_ACTIVE;
		    }
		    return BTSTACK_CHIPSET_VALID_COMMAND;
		case UPLOAD_ACTIVE:
			// Upload next segement
			bytes_to_upload = btstack_min(58, container_end - container_blob_offset);
			crc = btstack_crc32(&container_blob_data[container_blob_offset], bytes_to_upload); 
			// build command
			little_endian_store_16(hci_cmd_buffer, 0, HCI_CMD_OPCODE(OGF_VENDOR, HCI_OPCODE_EM_WRITE_PATCH_CONTINUE));
		    hci_cmd_buffer[2] = 6 + bytes_to_upload;
		    little_endian_store_16(hci_cmd_buffer, 3, patch_sequence_number++);
		    little_endian_store_32(hci_cmd_buffer, 5, crc);
		    memcpy(&hci_cmd_buffer[9], &container_blob_data[container_blob_offset], bytes_to_upload);
		    container_blob_offset += bytes_to_upload;
		    if (container_blob_offset >= container_end){
		    	log_info("container done maybe another one");
		    	upload_state = UPLOAD_IDLE;
		    }
		    return BTSTACK_CHIPSET_VALID_COMMAND;
	}
	return BTSTACK_CHIPSET_DONE;
}
#endif

#ifdef HAVE_EM9301_PATCH_CONTAINER

/*  Number of ROW in patch file [0-63] */
#define NUMBER_OF_ROWS			64

/* Number of Bytes Per ROW in patch file [0-63] */
#define ROW_SIZE				48

/* Number of Sectors [0-17] in patch file [0-63] */
#define NUMBER_OF_SECTORS		2

extern const uint8_t   EMPatchArray[];
extern const uint32_t  EMPatchArray_size;

signed char currentRow;
signed char currentSector;

uint8_t patchLoadingState;

#define PATCH_LOAD_STATE_NOTSTARTED             0x00u
#define PATCH_LOAD_STATE_ENTERINGISPMODE        0x01u
#define PATCH_LOAD_STATE_LOADING                0x02u
#define PATCH_LOAD_STATE_EXITINGISPMODE         0x04u
#define PATCH_LOAD_STATE_DONE                   0x08u


void em9301_hardware_error(uint8_t error){
	//TODO: Stack is freezing, wait how to continue
	if (patchLoadingState == PATCH_LOAD_STATE_LOADING && error == 0x80) {
		return; // everything ok
	}
	//TODO: something is wrong
}

static void chipset_em9301_init(const void * config){
	UNUSED(config);

	currentRow = NUMBER_OF_ROWS - 1;
	currentSector = NUMBER_OF_SECTORS - 1;

	patchLoadingState = PATCH_LOAD_STATE_NOTSTARTED;

	hci_set_hardware_error_callback(&em9301_hardware_error);
}

static btstack_chipset_result_t chipset_em9301_next_command(uint8_t * hci_cmd_buffer){
	log_info("current row %d, current sector %d, blob size %u", currentRow, currentSector, EMPatchArray_size);

	switch (patchLoadingState)
	{
	case PATCH_LOAD_STATE_NOTSTARTED:
		/* we have not yet loading the patch, start the
		 loading procedure now
		 try to enter in ISP mode by sending  HCI command
		 HCI_EM_Write_Data with parameters (address=0x1FFE, data=0x0000).*/
		patchLoadingState = PATCH_LOAD_STATE_ENTERINGISPMODE;

	    little_endian_store_16(hci_cmd_buffer, 0, HCI_CMD_OPCODE(OGF_VENDOR, HCI_OPCODE_EM_WRITE_DATA));
	    hci_cmd_buffer[2] = 4;
	    little_endian_store_16(hci_cmd_buffer, 3, 0x1FFE);
	    little_endian_store_16(hci_cmd_buffer, 5, 0x0000);
	    log_debug("PATCH_LOAD_STATE_ENTERINGISPMODE");

	    return BTSTACK_CHIPSET_VALID_COMMAND;

	case PATCH_LOAD_STATE_ENTERINGISPMODE:
		patchLoadingState = PATCH_LOAD_STATE_LOADING;
		// send EM CPU Reset
	    little_endian_store_16(hci_cmd_buffer, 0, hci_reset.opcode);
	    hci_cmd_buffer[2] = 0;
	    log_debug("PATCH_LOAD_STATE_LOADING");

	    return BTSTACK_CHIPSET_VALID_COMMAND;

	case PATCH_LOAD_STATE_LOADING:

		if (currentSector < 0) {
			/* We have now finish to load the patch it is now time to exiting ISP mode
			 try to exit in ISP mode by sending HCI command HCI_EM_Write_Data with parameters (address=0x1FFE, data=0x55AA).*/
			patchLoadingState = PATCH_LOAD_STATE_EXITINGISPMODE;

		    little_endian_store_16(hci_cmd_buffer, 0, HCI_CMD_OPCODE(OGF_VENDOR, HCI_OPCODE_EM_WRITE_DATA));
		    hci_cmd_buffer[2] = 4;
		    little_endian_store_16(hci_cmd_buffer, 3, 0x1FFE);
		    little_endian_store_16(hci_cmd_buffer, 5, 0x55AA);

		    return BTSTACK_CHIPSET_VALID_COMMAND;
		}

	    little_endian_store_16(hci_cmd_buffer, 0, HCI_CMD_OPCODE(OGF_VENDOR, HCI_OPCODE_EM_WRITE_PROGRAM));
	    hci_cmd_buffer[2] = ROW_SIZE + 2;
	    hci_cmd_buffer[3] = currentRow;
	    hci_cmd_buffer[4] = currentSector;
	    memcpy(&hci_cmd_buffer[5], &EMPatchArray[(ROW_SIZE * currentRow) + (NUMBER_OF_ROWS * ROW_SIZE * currentSector)], ROW_SIZE);

	    if (currentRow == 0) {
			currentRow = NUMBER_OF_ROWS - 1;
			currentSector--;
		} else {
			currentRow--;
		}

	    return BTSTACK_CHIPSET_VALID_COMMAND;

	case PATCH_LOAD_STATE_EXITINGISPMODE:
		patchLoadingState = PATCH_LOAD_STATE_DONE;
		// send EM CPU Reset
	    little_endian_store_16(hci_cmd_buffer, 0, hci_reset.opcode);
	    hci_cmd_buffer[2] = 0;

	    return BTSTACK_CHIPSET_VALID_COMMAND;

	case PATCH_LOAD_STATE_DONE:
		return BTSTACK_CHIPSET_DONE;
	}
	return BTSTACK_CHIPSET_DONE;
}

#endif

static const btstack_chipset_t btstack_chipset_em9301 = {
    "EM9301",
#ifdef HAVE_EM9304_PATCH_CONTAINER
    chipset_em9304_init,
    chipset_em9304_next_command,
#else
#ifdef HAVE_EM9301_PATCH_CONTAINER
    chipset_em9301_init,
    chipset_em9301_next_command,
#else
    NULL,
    NULL,
#endif
#endif
    chipset_set_baudrate_command,
    chipset_set_bd_addr_command,
};

// MARK: public API
const btstack_chipset_t * btstack_chipset_em9301_instance(void){
    return &btstack_chipset_em9301;
}
