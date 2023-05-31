/*
 * Copyright (C) 2023 BlueKitchen GmbH
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

/**
 * @title AVRCP Cover Art Client
 *
 */

#ifndef AVRCP_COVER_ART_CLIENT_H
#define AVRCP_COVER_ART_CLIENT_H

#include <stdint.h>

#include "bluetooth.h"
#include "btstack_config.h"
#include "btstack_defines.h"
#include "classic/goep_client.h"
#include "btstack_linked_list.h"
#include "classic/obex_parser.h"
#include "l2cap.h"

#if defined __cplusplus
extern "C" {
#endif

#ifdef ENABLE_AVRCP_COVER_ART
typedef enum {
    AVRCP_COVER_ART_INIT = 0,
    AVRCP_COVER_ART_W4_SDP_QUERY_COMPLETE,
    AVRCP_COVER_ART_W2_GOEP_CONNECT,
    AVRCP_COVER_ART_W4_GOEP_CONNECTION,
    AVRCP_COVER_ART_W2_SEND_CONNECT_REQUEST,
    AVRCP_COVER_ART_W4_CONNECT_RESPONSE,
    AVRCP_COVER_ART_W4_USER_AUTHENTICATION,
    AVRCP_COVER_ART_W2_SEND_AUTHENTICATED_CONNECT,
    AVRCP_COVER_ART_CONNECTED,
    // generic get object
    AVRCP_COVER_ART_W2_SEND_GET_OBJECT,
    AVRCP_COVER_ART_W4_OBJECT,
    // disconnect
    AVRCP_COVER_ART_W2_SEND_DISCONNECT_REQUEST,
    AVRCP_COVER_ART_W4_DISCONNECT_RESPONSE,
    // abort operation
    AVRCP_COVER_ART_W4_ABORT_COMPLETE,
} avrcp_cover_art_state_t;

typedef struct {
    uint8_t srm_value;
    uint8_t srmp_value;
} avrcp_cover_art_obex_srm_t;

typedef enum {
    SRM_DISABLED,
    SRM_W4_CONFIRM,
    SRM_ENABLED_BUT_WAITING,
    SRM_ENABLED
} avrcp_cover_art_srm_state_t;

typedef  struct {
    btstack_linked_item_t item;

    uint16_t cover_art_cid;
    uint16_t avrcp_cid;
    bd_addr_t addr;

    btstack_packet_handler_t packet_handler;

    uint8_t *ertm_buffer;
    uint32_t ertm_buffer_size;
    const l2cap_ertm_config_t * ertm_config;

    uint16_t goep_cid;
    goep_client_t goep_client;

    avrcp_cover_art_state_t state;

    //
    bool flow_control_enabled;
    bool flow_next_triggered;
    bool flow_wait_for_user;

    // obex parser
    bool obex_parser_waiting_for_response;
    obex_parser_t obex_parser;
    uint8_t obex_header_buffer[4];

    // obex srm
    avrcp_cover_art_obex_srm_t obex_srm;
    avrcp_cover_art_srm_state_t srm_state;

    // request
    const char * object_type;
    const char * image_handle;
    const char * image_descriptor;
    bool first_request;

} avrcp_cover_art_client_t;

/* API_START */

/**
 * @brief Set up AVRCP Cover Art client
 */
void avrcp_cover_art_client_init(void);

/**
 * @brief   Connect to AVRCP Cover Art service on a remote device, emits AVRCP_SUBEVENT_COVER_ART_CONNECTION_ESTABLISHED with status
 * @param   packet_handler
 * @param   remote_addr
 * @param   ertm_buffer
 * @param   ertm_buffer_size
 * @param   ertm_config
 * @param   avrcp_cover_art_cid  outgoing parameter, valid if status == ERROR_CODE_SUCCESS
 * @return status     
 */
uint8_t
avrcp_cover_art_client_connect(avrcp_cover_art_client_t *cover_art_client, btstack_packet_handler_t packet_handler,
                               bd_addr_t remote_addr, uint8_t *ertm_buffer, uint32_t ertm_buffer_size,
                               const l2cap_ertm_config_t *ertm_config, uint16_t *avrcp_cover_art_cid);

/**
 * @brief Request cover art thumbnail for cover with a given image handle retrieved via
 *        - avrcp_controller_get_now_playing_info or
 *        - avrcp_controller_get_element_attributes(... AVRCP_MEDIA_ATTR_DEFAULT_COVER_ART ...)
 * @param avrcp_cover_art_cid
 * @param image_handle
 * @return status
 */
uint8_t avrcp_cover_art_client_get_linked_thumbnail(uint16_t avrcp_cover_art_cid, const char * image_handle);

/**
 * @brief Request cover art image for given image handle retrieved via
 *        - avrcp_controller_get_now_playing_info or
 *        - avrcp_controller_get_element_attributes(... AVRCP_MEDIA_ATTR_DEFAULT_COVER_ART ...)
 *        and given image descriptor
 * @param avrcp_cover_art_cid
 * @param image_handle
 * @param image_descriptor
 * @return status
 */
uint8_t avrcp_cover_art_client_get_image(uint16_t avrcp_cover_art_cid, const char * image_handle, const char * image_descriptor);

/**
 * @brief Request image properties for given image handle retrieved via
 *        - avrcp_controller_get_now_playing_info or
 *        - avrcp_controller_get_element_attributes(... AVRCP_MEDIA_ATTR_DEFAULT_COVER_ART ...)
 * @param avrcp_cover_art_cid
 * @param image_handle
 * @return status
 */
uint8_t avrcp_cover_art_client_get_image_properties(uint16_t avrcp_cover_art_cid, const char * image_handle);

/**
 * @brief   Disconnect from AVRCP Cover Art service
 * @param   avrcp_cover_art_cid
 * @return status
 */
uint8_t avrcp_cover_art_client_disconnect(uint16_t avrcp_cover_art_cid);

/**
 * @brief De-Init AVRCP Cover Art Client
 */
void avrcp_cover_art_client_deinit(void);

/* API_END */

#endif

#if defined __cplusplus
}
#endif

#endif // AVRCP_COVER_ART_CLIENT_H
