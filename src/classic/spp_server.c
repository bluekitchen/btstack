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

#define BTSTACK_FILE__ "spp_server.c"

/*
 *  spp_server.c
 *
 * Create SPP SDP Records
 */

#include "spp_server.h"

#include <string.h>

#include "bluetooth.h"
#include "bluetooth_sdp.h"
#include "btstack_config.h"
#include "classic/core.h"
#include "classic/sdp_util.h"

static const char * spp_server_default_sdp_service_name = "SPP";

static void spp_create_sdp_record_internal(uint8_t *service, uint32_t service_record_handle, const uint8_t * service_uuid128, int rfcomm_channel, const char *name){
	
	uint8_t* attribute;
	de_create_sequence(service);
    
    // 0x0000 "Service Record Handle"
	de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_RECORD_HANDLE);
	de_add_number(service, DE_UINT, DE_SIZE_32, service_record_handle);
    
	// 0x0001 "Service Class ID List"
	de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST);
	attribute = de_push_sequence(service);
	{
		if (service_uuid128 == NULL){
			de_add_number(attribute,  DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_SERIAL_PORT );
		} else {
			de_add_uuid128(attribute, (uint8_t *) service_uuid128);
		}
	}
	de_pop_sequence(service, attribute);
	
	// 0x0004 "Protocol Descriptor List"
	de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST);
	attribute = de_push_sequence(service);
	{
		uint8_t* l2cpProtocol = de_push_sequence(attribute);
		{
			de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_L2CAP);
		}
		de_pop_sequence(attribute, l2cpProtocol);
		
		uint8_t* rfcomm = de_push_sequence(attribute);
		{
			de_add_number(rfcomm,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_RFCOMM);  // rfcomm_service
			de_add_number(rfcomm,  DE_UINT, DE_SIZE_8,  rfcomm_channel);  // rfcomm channel
		}
		de_pop_sequence(attribute, rfcomm);
	}
	de_pop_sequence(service, attribute);
	
	// 0x0005 "Public Browse Group"
	de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_BROWSE_GROUP_LIST); // public browse group
	attribute = de_push_sequence(service);
	{
		de_add_number(attribute,  DE_UUID, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PUBLIC_BROWSE_ROOT );
	}
	de_pop_sequence(service, attribute);
	
	// 0x0006
	de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_LANGUAGE_BASE_ATTRIBUTE_ID_LIST);
	attribute = de_push_sequence(service);
	{
		de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x656e);
		de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x006a);
		de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x0100);
	}
	de_pop_sequence(service, attribute);
	
	// 0x0009 "Bluetooth Profile Descriptor List"
	de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_BLUETOOTH_PROFILE_DESCRIPTOR_LIST);
	attribute = de_push_sequence(service);
	{
		uint8_t *sppProfile = de_push_sequence(attribute);
		{
			de_add_number(sppProfile,  DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_SERIAL_PORT);
			de_add_number(sppProfile,  DE_UINT, DE_SIZE_16, 0x0102);
		}
		de_pop_sequence(attribute, sppProfile);
	}
	de_pop_sequence(service, attribute);
	
	// 0x0100 "ServiceName"
    if (name == NULL){
        name = spp_server_default_sdp_service_name;
    }
    if (strlen(name) > 0){
        de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0100);
        de_add_data(service,  DE_STRING, (uint16_t) strlen(name), (uint8_t *) name);
    }
}

void spp_create_sdp_record(uint8_t *service, uint32_t service_record_handle, int rfcomm_channel, const char *name){
	spp_create_sdp_record_internal(service, service_record_handle, NULL, rfcomm_channel, name);
}

void spp_create_custom_sdp_record(uint8_t *service, uint32_t service_record_handle, const uint8_t * service_uuid128, int rfcomm_channel, const char *name){
	spp_create_sdp_record_internal(service, service_record_handle, service_uuid128, rfcomm_channel, name);
}
