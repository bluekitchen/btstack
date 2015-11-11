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
 *  sdp_query_util.c
 */
#include "sdp_parser.h"
#include "sdp_client.h"
#include "sdp_query_util.h"
 
static uint8_t des_attributeIDList[]    = { 0x35, 0x05, 0x0A, 0x00, 0x01, 0xff, 0xff};  // Attribute: 0x0001 - 0x0100
static uint8_t des_serviceSearchPattern[] = {0x35, 0x03, 0x19, 0x00, 0x00};
static uint8_t des_serviceSearchPatternUUID128[] = {
	0x35, 0x10, 0x19, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static uint8_t* create_service_search_pattern_for_uuid128(uint8_t* uuid){
	memcpy(&des_serviceSearchPatternUUID128[3], uuid, 16);
	return (uint8_t*)des_serviceSearchPatternUUID128;
}

uint8_t* create_service_search_pattern_for_uuid(uint16_t uuid){
	net_store_16(des_serviceSearchPattern, 3, uuid);
	return (uint8_t*)des_serviceSearchPattern;
}

void sdp_general_query_for_uuid(bd_addr_t remote, uint16_t uuid){
    create_service_search_pattern_for_uuid(uuid);
    sdp_parser_init();
    sdp_client_query(remote, (uint8_t*)&des_serviceSearchPattern[0], (uint8_t*)&des_attributeIDList[0]);
}

void sdp_general_query_for_uuid128(bd_addr_t remote, uint8_t* uuid){
    create_service_search_pattern_for_uuid128(uuid);
    sdp_parser_init();
    sdp_client_query(remote, (uint8_t*)&des_serviceSearchPatternUUID128[0], (uint8_t*)&des_attributeIDList[0]);
}



