/*
 * Copyright (C) 2024 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btp_cap.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "btstack_debug.h"
#include "btpclient.h"
#include "btp.h"
#include "btp_bap.h"
#include "btp_cap.h"
#include "btp_csip.h"
#include "btp_server.h"
#include "btstack.h"
#include "le-audio/le_audio_base_builder.h"
#include "le_audio_demo_util_source.h"
#include "le-audio/le_audio_base_parser.h"

#define MAX_NUM_ASES 4

static btstack_packet_callback_registration_t hci_event_callback_registration;

// CAP
typedef enum {
    CAP_DISCOVERY_IDLE,
    CAP_DISCOVERY_CSIS_WAIT,
    CAP_DISCOVERY_DONE,
} btp_cap_state_t;

// CIG
static le_audio_cig_t        btp_cap_cig;
static le_audio_cig_params_t btp_cap_cig_params;

// CIS <-> ASE
typedef enum {
    ASE_STATE_IDLE,
    ASE_STATE_W2_ENABLE,
    ASE_STATE_W4_EANBLING,
    ASE_STATE_ENABLING,
} btp_cap_ase_state;

static struct {
    uint8_t cig_id;
    uint8_t cis_id;
    uint8_t server_index;
    uint8_t ase_id;
    hci_con_handle_t acl_con_handle;
    hci_con_handle_t cis_con_handle;
    btp_cap_ase_state state;
} btp_cap_ases[MAX_NUM_ASES];
static uint8_t btp_bap_num_ases;

// CIS


static void btp_cap_send_discovery_complete(server_t * server) {
    struct btp_cap_discovery_completed_ev discovery_completed_ev;
    discovery_completed_ev.addr_type = server->address_type;
    reverse_bd_addr(server->address, discovery_completed_ev.address);
    MESSAGE("BTP_CAP_EV_DISCOVERY_COMPLETED %s", bd_addr_to_str(server->address));
    btp_send(BTP_SERVICE_ID_CAP, BTP_CAP_EV_DISCOVERY_COMPLETED, 0, sizeof(discovery_completed_ev), (const uint8_t *) &discovery_completed_ev);
}

static void btp_cap_discovery_next(server_t * server) {
    uint8_t ascs_index;
    uint16_t ascs_cid;
    switch((btp_cap_state_t) server->cap_state){
        case CAP_DISCOVERY_IDLE:
            server->cap_state = (uint8_t) CAP_DISCOVERY_CSIS_WAIT;
            btp_csip_connect_to_server(server);
            break;
        case CAP_DISCOVERY_CSIS_WAIT:
            server->cap_state = (uint8_t) CAP_DISCOVERY_DONE;
            btp_cap_send_discovery_complete(server);
            break;
        default:
            btstack_unreachable();
            break;
    }
}

static void btp_bap_start_audio_completed(uint8_t cig_id, uint8_t status){
    response_op = 0;
    uint8_t buffer[2];
    buffer[0] = cig_id;
    buffer[1] = status;
    btp_send(BTP_SERVICE_ID_CAP, BTP_CAP_EV_UNICAST_START_COMPLETED, 0, sizeof(buffer), buffer);
    response_op = 0;
}
static bool btp_cap_aess_in_state(btp_cap_ase_state state){
    uint8_t i;
    for (i=0;i<btp_bap_num_ases;i++){
        if (btp_cap_ases[i].state != state) {
            return false;
        }
    }
    return true;
}

static void btp_cap_ases_run(void){
    uint8_t i;
    for (i=0;i<btp_bap_num_ases;i++){
        switch (btp_cap_ases[i].state){
            case ASE_STATE_W2_ENABLE:
                btp_cap_ases[i].state = ASE_STATE_W4_EANBLING;
                // get server
                server_t * server = btp_server_for_index(btp_cap_ases[i].server_index);
                audio_stream_control_service_client_streamendpoint_enable(server->ascs_cid, btp_cap_ases[i].ase_id, NULL);
                return;
            default:
                break;
        }
    }
    // all enable?
    if (response_op == BTP_CAP_UNICAST_AUDIO_START){
        if (btp_cap_aess_in_state(ASE_STATE_ENABLING)){
            MESSAGE("BTP CAP All ASE in state Enabling");
            btp_bap_start_audio_completed(btp_cap_cig_params.cig_id, BTP_CAP_UNICAST_START_STATUS_SUCCESS);
        }
    }
}

#define MAX_NUM_CIS
static void btp_cap_hci_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    if (packet_type != HCI_EVENT_PACKET) return;
    uint8_t i;
    uint8_t status;

    hci_con_handle_t acl_con_handles[MAX_NUM_SERVERS];
    hci_con_handle_t cis_con_handles[MAX_NUM_SERVERS];

    switch (hci_event_packet_get_type(packet)){
        case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)){
                case GAP_SUBEVENT_CIG_CREATED:
                    // TODO: check status
                    // store CIS handles, and prepare create cis
                    for (i=0;i<btp_cap_cig_params.num_cis;i++){
                        btp_cap_ases[i].cis_con_handle = gap_subevent_cig_created_get_cis_con_handles(packet, i);
                        acl_con_handles[i] = btp_cap_ases[i].acl_con_handle;
                        cis_con_handles[i] = btp_cap_ases[i].cis_con_handle;
                    }
                    // create CIS
                    MESSAGE("CIG Created");
                    status = gap_cis_create(btp_cap_cig_params.cig_id, cis_con_handles, acl_con_handles);
                    MESSAGE("gap_cis_create status 0x%02x", status);
                    btstack_assert(status == ERROR_CODE_SUCCESS);
                    break;
                case GAP_SUBEVENT_CIS_CREATED:
                    MESSAGE("CIS Created");
                    for (i=0;i<btp_cap_cig_params.num_cis;i++) {
                        btp_cap_ases[i].state = ASE_STATE_W2_ENABLE;
                    }
                    btp_cap_ases_run();
                    break;
            }
        default:
            break;
    }

}

static void btp_cap_csip_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_CSIS_CLIENT_RANK:
            if ((response_service_id == BTP_SERVICE_ID_CAP) && (response_op == BTP_CAP_DISCOVER)){
                server_t * server = btp_server_for_csis_cid(gattservice_subevent_csis_client_sirk_get_csis_cid(packet));
                btstack_assert(server != NULL);
                MESSAGE("BTP_CAP_DISCOVER complete");
                btp_cap_discovery_next(server);
            }
            break;
        default:
            break;
    }
}


static void btp_cap_bap_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    ascs_codec_configuration_t codec_configuration;
    hci_con_handle_t con_handle;
    ascs_state_t ase_state;
    uint8_t response_code;
    uint8_t reason;
    uint16_t ascs_cid;
    uint16_t pacs_cid;
    uint8_t ase_id;
    uint8_t num_ases;
    uint8_t i;
    uint8_t opcode;
    hci_connection_t * hci_connection;
    server_t * server;
    uint8_t status = ERROR_CODE_SUCCESS;
    uint8_t ase_state_changed_ev[9];
    uint16_t pos;
    bool emit_state = false;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_CODEC_CONFIGURATION:
            ascs_cid = gattservice_subevent_ascs_client_codec_configuration_get_ascs_cid(packet);
            server = btp_server_for_ascs_cid(ascs_cid);
            btstack_assert(server != NULL);

            // codec id:
            codec_configuration.coding_format =  gattservice_subevent_ascs_client_codec_configuration_get_coding_format(packet);;
            codec_configuration.company_id = gattservice_subevent_ascs_client_codec_configuration_get_company_id(packet);
            codec_configuration.vendor_specific_codec_id = gattservice_subevent_ascs_client_codec_configuration_get_vendor_specific_codec_id(packet);

            codec_configuration.specific_codec_configuration.codec_configuration_mask = gattservice_subevent_ascs_client_codec_configuration_get_specific_codec_configuration_mask(packet);
            codec_configuration.specific_codec_configuration.sampling_frequency_index = gattservice_subevent_ascs_client_codec_configuration_get_sampling_frequency_index(packet);
            codec_configuration.specific_codec_configuration.frame_duration_index = gattservice_subevent_ascs_client_codec_configuration_get_frame_duration_index(packet);
            codec_configuration.specific_codec_configuration.audio_channel_allocation_mask = gattservice_subevent_ascs_client_codec_configuration_get_audio_channel_allocation_mask(packet);
            codec_configuration.specific_codec_configuration.octets_per_codec_frame = gattservice_subevent_ascs_client_codec_configuration_get_octets_per_frame(packet);
            codec_configuration.specific_codec_configuration.codec_frame_blocks_per_sdu = gattservice_subevent_ascs_client_codec_configuration_get_frame_blocks_per_sdu(packet);
            MESSAGE("BTP ASCS %u: Codec Configured for ase_id %d", server->server_id, server->ascs_ase);

            if (response_op == BTP_CAP_UNICAST_SETUP_ASE){
                MESSAGE("BTP ASCS %u: Configure QoS for ase id %u", server->server_id, server->ascs_ase);
                status = audio_stream_control_service_client_streamendpoint_configure_qos(server->ascs_cid, server->ascs_ase,&server->ascs_qos_configuration);
            }
            break;

        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_QOS_CONFIGURATION:
            ase_id  = gattservice_subevent_ascs_server_qos_configuration_get_ase_id(packet);
            ascs_cid = gattservice_subevent_ascs_server_qos_configuration_get_con_handle(packet);
            server = btp_server_for_ascs_cid(ascs_cid);
            btstack_assert(server != NULL);

            MESSAGE("BTP ASCS %u: QoS Configured for ase_id %d", server->server_id, server->ascs_ase);

            if (response_op == BTP_CAP_UNICAST_SETUP_ASE){
                MESSAGE("BTP_CAP_UNICAST_SETUP_ASE %u, ase id %u complete", server->server_id, server->ascs_ase);
                btp_send(BTP_SERVICE_ID_CAP, BTP_CAP_UNICAST_SETUP_ASE, 0, 0, NULL);
                response_op = 0;
            }
            break;

        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_STREAMENDPOINT_STATE:
            log_info("GATTSERVICE_SUBEVENT_ASCS_STREAMENDPOINT_STATE");
            ase_id     = gattservice_subevent_ascs_client_streamendpoint_state_get_ase_id(packet);
            ascs_cid   = gattservice_subevent_ascs_client_streamendpoint_state_get_ascs_cid(packet);
            server = btp_server_for_ascs_cid(ascs_cid);
            btstack_assert(server != NULL);

            ase_state  = gattservice_subevent_ascs_client_streamendpoint_state_get_state(packet);
            log_info("ASCS Client: ASE STATE (%s) - ase_id %d, ascs_cid 0x%02x, role %s", ascs_util_ase_state2str(ase_state), ase_id, ascs_cid,
                     (audio_stream_control_service_client_get_ase_role(ascs_cid, ase_id) == LE_AUDIO_ROLE_SOURCE) ? "SOURCE" : "SINK" );

            switch (response_op){
                case BTP_CAP_UNICAST_SETUP_ASE:
                    if (ase_state == ASCS_STATE_QOS_CONFIGURED){
                        btp_send(BTP_SERVICE_ID_ASCS, BTP_CAP_UNICAST_SETUP_ASE, 0, 0, NULL);
                    }
                    break;
                case BTP_CAP_UNICAST_AUDIO_START:
                    emit_state = true;
                    break;
                default:
                    break;
            }

            // report ASE State changes
            // TODO: move ot btp_ascs.c
            if (emit_state){
                pos = 0;
                ase_state_changed_ev[pos++] = server->address_type;
                reverse_bd_addr(server->address, &ase_state_changed_ev[pos]);
                pos += 6;
                ase_state_changed_ev[pos++] = ase_id;
                ase_state_changed_ev[pos++] = ase_state;
                btp_send(BTP_SERVICE_ID_ASCS, BTP_BAP_ASE_STATE_CHANGED, 0, pos, ase_state_changed_ev);
            }

#if 0
            // stop streaming if remote is a sink
            if ((ase_state == ASCS_STATE_RELEASING) && audio_stream_control_service_client_get_ase_role(ascs_cid, ase_id) == LE_AUDIO_ROLE_SINK){
                log_info("Releasing on Sink Stream Endpoint -> stop sending on all CIS");
                for (i=0;i<MAX_CHANNELS;i++){
                    cis_con_handles[i] = HCI_CON_HANDLE_INVALID;
                }
            }

            // send done
            if (response_service_id == BTP_SERVICE_ID_LE_AUDIO){
                switch (response_op){
                    case BTP_LE_AUDIO_OP_ASCS_ENABLE:
                        switch (ase_state){
                            case ASCS_STATE_ENABLING:
                                btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                                response_op = 0;
                                break;
                            default:
                                break;
                        }
                        break;
                    case BTP_LE_AUDIO_OP_ASCS_RECEIVER_START_READY:
                        // moved to control operation complete
                        break;
                    case BTP_LE_AUDIO_OP_ASCS_RECEIVER_STOP_READY:
                        switch (ase_state){
                            case ASCS_STATE_IDLE:
                            case ASCS_STATE_QOS_CONFIGURED:
                                btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                                response_op = 0;
                                break;
                            default:
                                break;
                        }
                        break;
                    case BTP_LE_AUDIO_OP_ASCS_DISABLE:
                        switch (ase_state){
                            case ASCS_STATE_DISABLING:
                            case ASCS_STATE_QOS_CONFIGURED:
                                btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                                response_op = 0;
                                break;
                            default:
                                break;
                        }
                        break;
                    case BTP_LE_AUDIO_OP_ASCS_RELEASE:
                        btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                        response_op = 0;
                        break;
                    case BTP_LE_AUDIO_OP_ASCS_UPDATE_METADATA:
                        btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                        response_op = 0;
                        break;
                    default:
                        break;
                }
            }
#endif
            break;
#if 0
        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_METADATA:
            ase_id  = gattservice_subevent_ascs_client_metadata_get_ase_id(packet);
            ascs_cid = gattservice_subevent_ascs_client_metadata_get_ascs_cid(packet);

            printf("ASCS Client: METADATA UPDATE - ase_id %d, ascs_cid 0x%02x\n", ase_id, ascs_cid);
            break;

        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_CONTROL_POINT_OPERATION_RESPONSE:
            ase_id        = gattservice_subevent_ascs_client_control_point_operation_response_get_ase_id(packet);
            ascs_cid      = gattservice_subevent_ascs_client_control_point_operation_response_get_ascs_cid(packet);
            response_code = gattservice_subevent_ascs_client_control_point_operation_response_get_response_code(packet);
            reason        = gattservice_subevent_ascs_client_control_point_operation_response_get_reason(packet);
            opcode        = gattservice_subevent_ascs_client_control_point_operation_response_get_opcode(packet);
            printf("ASCS Client: Operation complete - ase_id %d, opcode %u, response [0x%02x, 0x%02x], ascs_cid 0x%02x\n", ase_id, opcode, response_code, reason, ascs_cid);
            if ((opcode == ASCS_OPCODE_RECEIVER_START_READY) && (response_op != 0)){
                MESSAGE("Receiver Start Ready completed");
                btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                response_op = 0;
            }
            break;


#endif
        default:
            break;
    }

    if (status != ERROR_CODE_SUCCESS){
        MESSAGE("BTP CAP Operation failed, status 0x%02x", status);
        btstack_assert(false);
    }
}

void btp_cap_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data) {
    // provide op info for response
    response_len = 0;
    response_service_id = BTP_SERVICE_ID_CAP;
    response_op = opcode;
    server_t * server;
    uint8_t status = ERROR_CODE_SUCCESS;
    switch (opcode) {
        case BTP_CAP_READ_SUPPORTED_COMMANDS:
            MESSAGE("BTP_CAP_READ_SUPPORTED_COMMANDS");
            if (controller_index == BTP_INDEX_NON_CONTROLLER) {
                uint8_t commands = 0;
                btp_send(response_service_id, opcode, controller_index, 1, &commands);
            }
            break;
        case BTP_CAP_DISCOVER:
            if (controller_index == 0) {
                bd_addr_type_t addr_type = (bd_addr_type_t) data[0];
                bd_addr_t address;
                reverse_bd_addr(&data[1], address);
                server = btp_server_for_address(addr_type, address);
                MESSAGE("BTP_CAP_DISCOVER %u, addr %s, addr type %u", server->server_id, bd_addr_to_str(server->address), server->address_type);
                server->cap_state = (uint8_t) CAP_DISCOVERY_IDLE;
                btp_cap_discovery_next(server);
                btp_send(response_service_id, opcode, controller_index, 0, NULL);
                // Reset CIG params + ASEs
                memset (&btp_cap_cig_params, 0, sizeof(le_audio_cig_params_t));
                btp_cap_cig_params.max_transport_latency_c_to_p = 40;
                btp_cap_cig_params.max_transport_latency_p_to_c = 40;
                btp_bap_num_ases = 0;
                // set defaults
                uint8_t i;
                for (i=0;i<MAX_NR_CIS;i++){
                    btp_cap_cig_params.cis_params[i].rtn_c_to_p = 2;
                    btp_cap_cig_params.cis_params[i].rtn_p_to_c = 2;
                    btp_cap_cig_params.cis_params[i].phy_c_to_p = LE_AUDIO_SERVER_PHY_MASK_2M;
                    btp_cap_cig_params.cis_params[i].phy_p_to_c = LE_AUDIO_SERVER_PHY_MASK_2M;
                }
            }
            break;
        case BTP_CAP_UNICAST_SETUP_ASE:
            if (controller_index == 0){
                int offset = 0;
                bd_addr_type_t addr_type = data[offset++];
                bd_addr_t address;
                reverse_bd_addr(&data[1], address);
                server = btp_server_for_address(addr_type, address);
                offset += 6;
                uint8_t ase_id = data[offset++];
                server->ascs_ase = ase_id;
                uint8_t cis_id = data[offset++];
                server->ascs_qos_configuration.cis_id = cis_id;
                uint8_t cig_id = data[offset++];
                server->ascs_qos_configuration.cig_id = cig_id;
                server->ascs_codec_configuration_request.coding_format = data[offset++];
                server->ascs_codec_configuration_request.company_id = little_endian_read_16(data, offset);
                offset += 2;
                server->ascs_codec_configuration_request.vendor_specific_codec_id = little_endian_read_16(data, offset);
                offset += 2;
                uint32_t sdu_interval_us = little_endian_read_24(data, offset);
                server->ascs_qos_configuration.sdu_interval = sdu_interval_us;
                offset += 3;
                server->ascs_qos_configuration.framing = data[offset++];
                uint16_t max_sdu = little_endian_read_16(data, offset);
                server->ascs_qos_configuration.max_sdu = max_sdu;
                offset += 2;
                uint8_t retransmission_number = data[offset++];
                server->ascs_qos_configuration.retransmission_number = retransmission_number;
                uint16_t max_transport_latency_ms = little_endian_read_16(data, offset);
                server->ascs_qos_configuration.max_transport_latency_ms = max_transport_latency_ms;
                offset += 2;
                server->ascs_qos_configuration.presentation_delay_us =  little_endian_read_24(data, offset);
                offset += 3;
                uint8_t codec_config_ltvs_length = data[offset++];
                uint8_t metadata_ltvs_length = data[offset++];
                ascs_util_specific_codec_configuration_parse(&data[offset], codec_config_ltvs_length, &server->ascs_codec_configuration_request.specific_codec_configuration);
                MESSAGE("Parse Meta Data");
                MESSAGE("BTP_CAP_UNICAST_SETUP_ASE %u, ase id %u", server->server_id, server->ascs_ase);

                le_audio_role_t direction = audio_stream_control_service_client_get_ase_role(server->ascs_cid, ase_id);

                // collect ASEs
                btp_cap_ases[btp_bap_num_ases].cig_id = cig_id;
                btp_cap_ases[btp_bap_num_ases].cis_id = cis_id;
                btp_cap_ases[btp_bap_num_ases].server_index = server->server_id;
                btp_cap_ases[btp_bap_num_ases].ase_id = ase_id;
                btp_cap_ases[btp_bap_num_ases].acl_con_handle = server->acl_con_handle;
                btp_cap_ases[btp_bap_num_ases].cis_con_handle = HCI_CON_HANDLE_INVALID;
                btp_cap_ases[btp_bap_num_ases].state = ASE_STATE_IDLE;
                btp_bap_num_ases++;
                MESSAGE("BTP_CAP CIG %u, CIS %u, server %u, ASE ID %u", cig_id, cis_id, server->server_id, ase_id);

                // collect CIG params
                uint8_t cis_index = btp_cap_cig_params.num_cis;
                btstack_assert(cig_id == 0);
                btp_cap_cig_params.cig_id = cig_id;
                btp_cap_cig_params.cis_params[cis_index].cis_id = cis_id;
                btp_cap_cig_params.cis_params[cis_index].phy_c_to_p = LE_AUDIO_SERVER_PHY_MASK_2M;
                btp_cap_cig_params.framing = server->ascs_qos_configuration.framing;
                btp_cap_cig_params.sdu_interval_c_to_p = sdu_interval_us;
                btp_cap_cig_params.sdu_interval_p_to_c = sdu_interval_us;
                switch (direction){
                    case LE_AUDIO_ROLE_SINK:
                        btp_cap_cig_params.max_transport_latency_c_to_p = max_transport_latency_ms;
                        btp_cap_cig_params.cis_params[cis_index].max_sdu_c_to_p = max_sdu;
                        btp_cap_cig_params.cis_params[cis_index].rtn_c_to_p = retransmission_number;
                        break;
                    case LE_AUDIO_ROLE_SOURCE:
                        btp_cap_cig_params.max_transport_latency_p_to_c = max_transport_latency_ms;
                        btp_cap_cig_params.cis_params[cis_index].max_sdu_p_to_c = max_sdu;
                        btp_cap_cig_params.cis_params[cis_index].rtn_p_to_c = retransmission_number;
                        break;
                    default:
                        btstack_unreachable();
                        break;
                }
                btp_cap_cig_params.num_cis++;

                MESSAGE("BTP ASCS %u: Configure Codec for ase id %u", server->server_id, server->ascs_ase);
                status = audio_stream_control_service_client_streamendpoint_configure_codec(server->ascs_cid, server->ascs_ase, &server->ascs_codec_configuration_request);
            }
            break;
        case BTP_CAP_UNICAST_AUDIO_START:
            if (controller_index == 0){
                uint8_t cig_id = data[0];
                uint8_t set_type = data[1];
                btp_send(BTP_SERVICE_ID_CAP, response_op, 0, 0, NULL);

                // TODO: support ordered access
                MESSAGE("Unicaset audio start");
                // ascs configured
                uint8_t status = gap_cig_create(&btp_cap_cig, &btp_cap_cig_params);
                btstack_assert(status == ERROR_CODE_SUCCESS);
            }
            break;
        default:
            break;
    }

    if (status != ERROR_CODE_SUCCESS){
        MESSAGE("BTP CAP Operation failed, status 0x%02x", status);
        btstack_assert(false);
    }
}

void btp_cap_init(void) {
    // register for HCI events
    hci_event_callback_registration.callback = &btp_cap_hci_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // Clients
    btp_csip_register_higher_layer(&btp_cap_csip_handler);
    btp_bap_register_higher_layer(&btp_cap_bap_handler);
}
