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

#ifndef __GOEP_CLIENT_H

#if defined __cplusplus
extern "C" {
#endif
 
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_defines.h"

//------------------------------------------------------------------------------------------------------------
// goep_client.h
//
// Communicate with remote OBEX server - General Object Exchange
//

/* API_START */

/**
 * Setup GOEP Client
 */
void    goep_client_init(void);

/*
 * @brief Create GOEP connection to a GEOP server with specified UUID on a remote deivce.
 * @param handler 
 * @param addr
 * @param uuid
 * @param out_cid to use for further commands
 * @result status
*/
uint8_t goep_client_create_connection(btstack_packet_handler_t handler, bd_addr_t addr, uint16_t uuid, uint16_t * out_cid);

/** 
 * @brief Disconnects GOEP connection with given identifier.
 * @param gope_cid
 * @return status
 */
uint8_t goep_client_disconnect(uint16_t goep_cid);

/** 
 * @brief Request emission of GOEP_SUBEVENT_CAN_SEND_NOW as soon as possible
 * @note GOEP_SUBEVENT_CAN_SEND_NOW might be emitted during call to this function
 *       so packet handler should be ready to handle it
 * @param goep_cid
 */
void    goep_client_request_can_send_now(uint16_t goep_cid);

/**
 * @brief Get Opcode from last created request, needed for parsing of OBEX response packet
 * @param gope_cid
 * @return opcode
 */
uint8_t goep_client_get_request_opcode(uint16_t goep_cid);

/**
 * @brief Set Connection ID used for newly created requests
 * @param gope_cid
 */
void    goep_client_set_connection_id(uint16_t goep_cid, uint32_t connection_id);

/**
 * @brief Start Connect request
 * @param gope_cid
 * @param obex_version_number
 * @param flags
 * @param maximum_obex_packet_length
 */
void    goep_client_create_connect_request(uint16_t goep_cid, uint8_t obex_version_number, uint8_t flags, uint16_t maximum_obex_packet_length);

/**
 * @brief Start Get request
 * @param gope_cid
 */
void    goep_client_create_get_request(uint16_t goep_cid);

/**
 * @brief Start Set Path request
 * @param gope_cid
 */
void    goep_client_create_set_path_request(uint16_t goep_cid, uint8_t flags);

// not implemented yet
// void  goep_client_create_put(uint16_t goep_cid);

/**
 * @brief Add name header to current request
 * @param goep_cid
 * @param name
 */
void    goep_client_add_header_name(uint16_t goep_cid, const char * name);

/**
 * @brief Add target header to current request
 * @param goep_cid
 * @param target
 */
void    goep_client_add_header_target(uint16_t goep_cid, uint16_t length, const uint8_t * target);

/**
 * @brief Add type header to current request
 * @param goep_cid
 * @param type
 */
void    goep_client_add_header_type(uint16_t goep_cid, const char * type);

/**
 * @brief Add count header to current request
 * @param goep_cid
 * @param count
 */
void    goep_client_add_header_count(uint16_t goep_cid, uint32_t count);

/**
 * @brief Add application parameters header to current request
 * @param goep_cid
 * @param lenght of application parameters
 * @param daa 
 */
void    goep_client_add_header_application_parameters(uint16_t goep_cid, uint16_t length, uint8_t * data);

// int  goep_client_add_body_static(uint16_t goep_cid,  uint32_t length, uint8_t * data);
// int  goep_client_add_body_dynamic(uint16_t goep_cid, uint32_t length, void (*data_callback)(uint32_t offset, uint8_t * buffer, uint32_t len));

/**
 * @brief Execute prepared request
 * @param goep_cid
 * @param daa 
 */
int goep_client_execute(uint16_t goep_cid);

/* API_END */

#if defined __cplusplus
}
#endif
#endif