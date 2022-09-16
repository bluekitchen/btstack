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

#define BTSTACK_FILE__ "pan.c"

/*
 *  pan.h
 *
 *  Created by Milanka Ringwald on 10/16/14.
 */

#include "pan.h"

#include <string.h>

#include "bluetooth_psm.h"
#include "bluetooth_sdp.h"
#include "btstack_config.h"
#include "classic/core.h"
#include "classic/sdp_util.h"
#include "l2cap.h"

static const char pan_default_panu_service_name[] = "Personal Ad-hoc User Service";
static const char pan_default_panu_service_desc[] = "Personal Ad-hoc User Service";

static const char pan_default_nap_service_name[] = "Network Access Point Service";
static const char pan_default_nap_service_desc[] = "Personal Ad-hoc Network Service which provides access to a network";

static const char pan_default_gn_service_name[] = "Group Ad-hoc Network Service";
static const char pan_default_gn_service_desc[] = "Personal Group Ad-hoc Network Service";

static void pan_create_service(uint8_t *service, uint32_t service_record_handle,
	 uint32_t service_uuid, uint16_t * network_packet_types, const char *name, const char *descriptor,
	 security_description_t security_desc, net_access_type_t net_access_type, uint32_t max_net_access_rate,
	 const char *IPv4Subnet, const char *IPv6Subnet){

	uint8_t* attribute;
	de_create_sequence(service);

	// 0x0000 "Service Record Handle"
	de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_RECORD_HANDLE);
	de_add_number(service, DE_UINT, DE_SIZE_32, service_record_handle);

	// 0x0001 "Service Class ID List"
	de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST);
	attribute = de_push_sequence(service);
	{
		//  "UUID for PAN Service"
		de_add_number(attribute, DE_UUID, DE_SIZE_32, service_uuid);
	}
	de_pop_sequence(service, attribute);

	// 0x0004 "Protocol Descriptor List"
	de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST);
	attribute = de_push_sequence(service);
	{
		uint8_t* l2cpProtocol = de_push_sequence(attribute);
		{
			de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_L2CAP);
			de_add_number(l2cpProtocol,  DE_UINT, DE_SIZE_16, BLUETOOTH_PSM_BNEP);  // l2cap psm
		}
		de_pop_sequence(attribute, l2cpProtocol);
		
		uint8_t* bnep = de_push_sequence(attribute);
		{
			de_add_number(bnep,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_BNEP);
			de_add_number(bnep,  DE_UINT, DE_SIZE_16, 0x0100);  // version

			uint8_t * net_packet_type_list = de_push_sequence(bnep);
			{		
				if (network_packet_types){
					while (*network_packet_types){
						de_add_number(net_packet_type_list,  DE_UINT, DE_SIZE_16, *network_packet_types++);
					}
				}
			}
			de_pop_sequence(bnep, net_packet_type_list);
		}
		de_pop_sequence(attribute, bnep);
	}
	de_pop_sequence(service, attribute);

	// 0x0005 "Public Browse Group"
	de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_BROWSE_GROUP_LIST); // public browse group
	attribute = de_push_sequence(service);
	{
		de_add_number(attribute,  DE_UUID, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PUBLIC_BROWSE_ROOT);
	}
	de_pop_sequence(service, attribute);

	// 0x0006 "LanguageBaseAttributeIDList"
	de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_LANGUAGE_BASE_ATTRIBUTE_ID_LIST);
	attribute = de_push_sequence(service);
	{
		de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x656e);
		de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x006a);
		de_add_number(attribute, DE_UINT, DE_SIZE_16, 0x0100);	// Base for Service Name and Service Description
	}
	de_pop_sequence(service, attribute);

	// 0x0008 "Service Availability", optional
	// de_add_number(service, DE_UINT, DE_SIZE_16, 0x0008);
	// de_add_number(service, DE_UINT, DE_SIZE_8, service_availability);

	// 0x0009 "Bluetooth Profile Descriptor List"
	de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_BLUETOOTH_PROFILE_DESCRIPTOR_LIST);
	attribute = de_push_sequence(service);
	{
		uint8_t *sppProfile = de_push_sequence(attribute);
		{
			de_add_number(sppProfile,  DE_UUID, DE_SIZE_16, service_uuid); 
			de_add_number(sppProfile,  DE_UINT, DE_SIZE_16, 0x0100); // Verision 1.0
		}
		de_pop_sequence(attribute, sppProfile);
	}
	de_pop_sequence(service, attribute);

	// 0x0100 "Service Name"
	de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0100);
	if (name){
		de_add_data(service,  DE_STRING, (uint16_t) strlen(name), (uint8_t *) name);
	} else {
		switch (service_uuid){
			case BLUETOOTH_SERVICE_CLASS_PANU:
				de_add_data(service, DE_STRING, (uint16_t) strlen(pan_default_panu_service_name), (uint8_t *) pan_default_panu_service_name);
				break;
			case BLUETOOTH_SERVICE_CLASS_NAP:
				de_add_data(service, DE_STRING, (uint16_t) strlen(pan_default_nap_service_name), (uint8_t *) pan_default_nap_service_name);
				break;
			case BLUETOOTH_SERVICE_CLASS_GN:
				de_add_data(service, DE_STRING, (uint16_t) strlen(pan_default_gn_service_name), (uint8_t *) pan_default_gn_service_name);
				break;
			default:
				break;
		}
	}

	// 0x0101 "Service Description"
	de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0101);
	if (descriptor){
		de_add_data(service,  DE_STRING, (uint16_t) strlen(descriptor), (uint8_t *) descriptor);
	} else {
		switch (service_uuid){
			case BLUETOOTH_SERVICE_CLASS_PANU:
				de_add_data(service, DE_STRING, (uint16_t) strlen(pan_default_panu_service_desc), (uint8_t *) pan_default_panu_service_desc);
				break;
			case BLUETOOTH_SERVICE_CLASS_NAP:
				de_add_data(service, DE_STRING, (uint16_t) strlen(pan_default_nap_service_desc), (uint8_t *) pan_default_nap_service_desc);
				break;
			case BLUETOOTH_SERVICE_CLASS_GN:
				de_add_data(service, DE_STRING, (uint16_t) strlen(pan_default_gn_service_desc), (uint8_t *) pan_default_gn_service_desc);
				break;
			default:
				break;
		}
	}

	// 0x030A "Security Description"
	de_add_number(service, DE_UINT, DE_SIZE_16, 0x030A);
	de_add_number(service, DE_UINT, DE_SIZE_16, security_desc);

	if (service_uuid == BLUETOOTH_SERVICE_CLASS_PANU) return;

	if (IPv4Subnet){
		// 0x030D "IPv4Subnet", optional
		de_add_number(service,  DE_UINT, DE_SIZE_16, 0x030D);
		de_add_data(service,  DE_STRING, (uint16_t) strlen(IPv4Subnet), (uint8_t *) IPv4Subnet);
	}

	if (IPv6Subnet){
		// 0x030E "IPv6Subnet", optional
		de_add_number(service,  DE_UINT, DE_SIZE_16, 0x030E);
		de_add_data(service,  DE_STRING, (uint16_t) strlen(IPv6Subnet), (uint8_t *) IPv6Subnet);
	}

	if (service_uuid == BLUETOOTH_SERVICE_CLASS_GN) return;

	// 0x030B "NetAccessType"
	de_add_number(service, DE_UINT, DE_SIZE_16, 0x030B);
	de_add_number(service, DE_UINT, DE_SIZE_16, net_access_type);

	// 0x030C "MaxNetAccessRate"
	de_add_number(service, DE_UINT, DE_SIZE_16, 0x030C);
	de_add_number(service, DE_UINT, DE_SIZE_32, max_net_access_rate);

}


void pan_create_nap_sdp_record(uint8_t *service, uint32_t service_record_handle, uint16_t * network_packet_types, const char *name, const char *description, security_description_t security_desc,
	net_access_type_t net_access_type, uint32_t max_net_access_rate, const char *IPv4Subnet, const char *IPv6Subnet){

	pan_create_service(service, service_record_handle, BLUETOOTH_SERVICE_CLASS_NAP, network_packet_types, name, description, security_desc, net_access_type, max_net_access_rate, IPv4Subnet, IPv6Subnet);
}

void pan_create_gn_sdp_record(uint8_t *service, uint32_t service_record_handle, uint16_t * network_packet_types, const char *name, const char *description, security_description_t security_desc,
	const char *IPv4Subnet, const char *IPv6Subnet){
	pan_create_service(service, service_record_handle, BLUETOOTH_SERVICE_CLASS_GN, network_packet_types, name, description, security_desc, PAN_NET_ACCESS_TYPE_NONE, 0, IPv4Subnet, IPv6Subnet);
}

void pan_create_panu_sdp_record(uint8_t *service, uint32_t service_record_handle, uint16_t * network_packet_types, const char *name, const char *description, security_description_t security_desc){
	pan_create_service(service, service_record_handle, BLUETOOTH_SERVICE_CLASS_PANU, network_packet_types, name, description, security_desc, PAN_NET_ACCESS_TYPE_NONE, 0, NULL, NULL);
}
