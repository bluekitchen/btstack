/*
 * Copyright (C) 2014 by BlueKitchen GmbH
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
 *  pan.h
 *
 *  Created by Milanka Ringwald on 10/16/14.
 */

#include "pan.h"

#include <stdio.h>
#include <string.h>

#include <btstack/sdp_util.h>

static const char default_panu_service_name[] = "Personal Ad-hoc User Service";
static const char default_panu_service_desc[] = "Personal Ad-hoc User Service";

static const char default_nap_service_name[] = "Network Access Point Service";
static const char default_nap_service_desc[] = "Personal Ad-hoc Network Service which provides access to a network";

static const char default_gn_service_name[] = "Group Ad-hoc Network Service";
static const char default_gn_service_desc[] = "Personal Group Ad-hoc Network Service";

void pan_create_service(bnep_service_uuid_t service_uuid, uint8_t *service, const char *name, const char *descriptor, 
	 security_description_t security_desc, net_access_type_t net_access_type, uint32_t max_net_access_rate,
	 const char *IPv4Subnet, const char *IPv6Subnet){

	uint8_t* attribute;
	de_create_sequence(service);

	// 0x0000 "Service Record Handle"
	de_add_number(service, DE_UINT, DE_SIZE_16, SDP_ServiceRecordHandle);
	de_add_number(service, DE_UINT, DE_SIZE_32, 0x10001);

	// 0x0001 "Service Class ID List"
	de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_ServiceClassIDList);
	attribute = de_push_sequence(service);
	{
		//  "UUID for PAN Service"
		de_add_number(attribute, DE_UUID, DE_SIZE_16, service_uuid);
	}
	de_pop_sequence(service, attribute);

	// 0x0004 "Protocol Descriptor List"
	de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_ProtocolDescriptorList);
	attribute = de_push_sequence(service);
	{
		uint8_t* l2cpProtocol = de_push_sequence(attribute);
		{
			de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, 0x0100);
		}
		de_pop_sequence(attribute, l2cpProtocol);
		
		uint8_t* bnep = de_push_sequence(attribute);
		{
			de_add_number(bnep,  DE_UUID, DE_SIZE_16, 0x000F);  // bnep
			de_add_number(bnep,  DE_UINT, DE_SIZE_16, 0x0100);  // version

			uint8_t * net_packet_type_list = de_push_sequence(bnep);
			{		
				// add Supported Network Packet Type List
				// de_add_number(net_packet_type_list,  DE_UINT, DE_SIZE_16, 0x0100);  // version
			}
			de_pop_sequence(bnep, net_packet_type_list);
		}
		de_pop_sequence(attribute, bnep);
	}
	de_pop_sequence(service, attribute);

	// 0x0005 "Public Browse Group"
	de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_BrowseGroupList); // public browse group
	attribute = de_push_sequence(service);
	{
		de_add_number(attribute,  DE_UUID, DE_SIZE_16, 0x1002 );
	}
	de_pop_sequence(service, attribute);

	// 0x0006 "LanguageBaseAttributeIDList"
	de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_LanguageBaseAttributeIDList);
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
	de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_BluetoothProfileDescriptorList);
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
		de_add_data(service,  DE_STRING, strlen(name), (uint8_t *) name);
	} else {
		switch (service_uuid){
			case PANU_UUID:
				de_add_data(service,  DE_STRING, strlen(default_panu_service_name), (uint8_t *) default_panu_service_name);
				break;
			case NAP_UUID:
				de_add_data(service,  DE_STRING, strlen(default_nap_service_name), (uint8_t *) default_nap_service_name);
				break;
			case GN_UUID:
				de_add_data(service,  DE_STRING, strlen(default_gn_service_name), (uint8_t *) default_gn_service_name);
				break;
			default:
				break;
		}
	}

	// 0x0101 "Service Description"
	de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0101);
	if (descriptor){
		de_add_data(service,  DE_STRING, strlen(descriptor), (uint8_t *) descriptor);
	} else {
		switch (service_uuid){
			case PANU_UUID:
				de_add_data(service,  DE_STRING, strlen(default_panu_service_desc), (uint8_t *) default_panu_service_desc);
				break;
			case NAP_UUID:
				de_add_data(service,  DE_STRING, strlen(default_nap_service_desc), (uint8_t *) default_nap_service_desc);
				break;
			case GN_UUID:
				de_add_data(service,  DE_STRING, strlen(default_gn_service_desc), (uint8_t *) default_gn_service_desc);
				break;
			default:
				break;
		}
	}

	// 0x030A "Security Description"
	de_add_number(service, DE_UINT, DE_SIZE_16, 0x030A);
	de_add_number(service, DE_UINT, DE_SIZE_16, security_desc);

	if (service_uuid == PANU_UUID) return;

	if (IPv4Subnet){
		// 0x030D "IPv4Subnet", optional
		de_add_number(service,  DE_UINT, DE_SIZE_16, 0x030D);
		de_add_data(service,  DE_STRING, strlen(IPv4Subnet), (uint8_t *) IPv4Subnet);
	}

	if (IPv6Subnet){
		// 0x030E "IPv6Subnet", optional
		de_add_number(service,  DE_UINT, DE_SIZE_16, 0x030E);
		de_add_data(service,  DE_STRING, strlen(IPv6Subnet), (uint8_t *) IPv6Subnet);
	}

	if (service_uuid == GN_UUID) return;

	// 0x030B "NetAccessType"
	de_add_number(service, DE_UINT, DE_SIZE_16, 0x030B);
	de_add_number(service, DE_UINT, DE_SIZE_16, net_access_type);

	// 0x030C "MaxNetAccessRate"
	de_add_number(service, DE_UINT, DE_SIZE_16, 0x030C);
	de_add_number(service, DE_UINT, DE_SIZE_32, max_net_access_rate);

}


void pan_create_nap_service(uint8_t *service, const char *name, const char *description, security_description_t security_desc, 
	net_access_type_t net_access_type, uint32_t max_net_access_rate, const char *IPv4Subnet, const char *IPv6Subnet){

	pan_create_service(NAP_UUID, service, name, description, security_desc, net_access_type, max_net_access_rate, IPv4Subnet, IPv6Subnet);
}

void pan_create_gn_service(uint8_t *service, const char *name, const char *description, security_description_t security_desc, 
	const char *IPv4Subnet, const char *IPv6Subnet){
	pan_create_service(GN_UUID, service, name, description, security_desc, PAN_NET_ACCESS_TYPE_NONE, 0, IPv4Subnet, IPv6Subnet);
}

void pan_create_panu_service(uint8_t *service, const char *name, const char *description, security_description_t security_desc){
	pan_create_service(PANU_UUID, service, name, description, security_desc, PAN_NET_ACCESS_TYPE_NONE, 0, NULL, NULL);
}
