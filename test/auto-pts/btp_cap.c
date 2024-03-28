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
    ASE_STATE_W2_CONFIGURE_CODEC,
    ASE_STATE_W4_CODEC_CONFIGURED,
    ASE_STATE_W2_CREATE_CIG,
    ASE_STATE_W4_CIG_CREATED,
    ASE_STATE_W2_CONFIGURE_QOS,
    ASE_STATE_W4_QOS_CONFIGURED,
    ASE_STATE_QOS_CONFIGURED,
    ASE_STATE_W2_CREATE_CIS,
    ASE_STATE_W4_CIS_CREATED,
    ASE_STATE_W4_ALL_CIS_CREATED,
    ASE_STATE_W2_ENABLE,
    ASE_STATE_W4_ENABLING,
    ASE_STATE_ENABLING,
    ASE_STATE_W2_RECEIVER_START_READY,
    ASE_STATE_W4_STREAMING,
    ASE_STATE_STREAMING,
} btp_cap_ase_state;

typedef struct {
    uint8_t cig_id;
    uint8_t cis_id;
    uint8_t server_index;
    uint8_t ase_id;
    hci_con_handle_t acl_con_handle;
    hci_con_handle_t cis_con_handle;
    btp_cap_ase_state state;

    ascs_client_codec_configuration_request_t codec_configuration_request;
    ascs_qos_configuration_t                  qos_configuration;
    le_audio_metadata_t                       metadata;
} btp_cap_ase_t;
static btp_cap_ase_t btp_cap_ases[MAX_NUM_ASES];
static uint8_t btp_bap_num_ases;

// Broadcast
#define MAX_NUM_BROADCAST_STREAMS 2
#define MAX_NUM_BROADCAST_SUBGROUPS 1
#define MAX_BROADCAST_CODEC_CONFIG_LEN 64
#define MAX_BROADCAST_METADATA_LEN 64

// streams
typedef struct {
    uint8_t source_id;
    uint8_t subgroup_id;
#if 0
    uint8_t coding_format;
    uint16_t vid;
    uint16_t pid;
#else
    uint8_t codec_id[5];
#endif
    uint8_t codec_config_len;
    uint8_t codec_config_data[MAX_BROADCAST_CODEC_CONFIG_LEN];
    uint8_t metadata_len;
    uint8_t metadata_data[MAX_BROADCAST_METADATA_LEN];
} btp_cap_broadcast_stream_t;
static btp_cap_broadcast_stream_t broadcast_streams[MAX_NUM_BROADCAST_STREAMS];
static uint8_t btp_cap_broadcast_stream_count;

// subgroups
typedef struct {
    uint8_t source_id;
#if 0
    uint8_t coding_format;
    uint16_t vid;
    uint16_t pid;
#else
    uint8_t codec_id[5];
#endif
    uint8_t codec_config_len;
    uint8_t codec_config_data[MAX_BROADCAST_CODEC_CONFIG_LEN];
    uint8_t metadata_len;
    uint8_t metadata_data[MAX_BROADCAST_METADATA_LEN];
} btp_cap_broadcast_subgroup_t ;
static btp_cap_broadcast_subgroup_t broadcast_subgroups[MAX_NUM_BROADCAST_SUBGROUPS];
static uint8_t btp_cap_broadcast_subgroup_count;

// sources
static bool btp_cap_broadcast_source_codec_config_at_subgroup;
static uint32_t btp_cap_broadcast_source_presenentation_delay_us;

static void btp_cap_send_response_if_pending(void){
    if ((response_service_id == BTP_SERVICE_ID_CAP) && (response_op != 0)){
        btp_send(BTP_SERVICE_ID_CAP, response_op, 0, 0, NULL);
        response_op = 0;
    }
}

static btp_cap_ase_t * btp_cap_ase_for_server_index_and_ase_id(uint8_t server_index, uint8_t ase_id){
    uint8_t i;
    for (i=0;i<btp_bap_num_ases;i++){
        btp_cap_ase_t * ase = &btp_cap_ases[i];
        if ((ase->server_index == server_index) && (ase->ase_id == ase_id)){
            return ase;
        }
    }
    MESSAGE("ASE ID %u for server %u not found", ase_id, server_index);
    btstack_assert(false);
    return NULL;
}

static bool btp_cap_ases_in_state(btp_cap_ase_state state){
    if (btp_bap_num_ases == 0){
        return false;
    }
    uint8_t i;
    for (i=0;i<btp_bap_num_ases;i++){
        if (btp_cap_ases[i].state != state) {
            return false;
        }
    }
    return true;
}

static void btp_cap_ases_set_state(btp_cap_ase_state state){
    uint8_t i;
    for (i=0;i<btp_bap_num_ases;i++) {
        MESSAGE("btp_cap_ases_set_state %u = state %u", i, state);
        btp_cap_ases[i].state = state;
    }
}

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

static void     btp_cap_ases_run(void){

    uint8_t status = ERROR_CODE_SUCCESS;
    uint8_t i;

    // All ASE operations

    // create cig when all ase configured
    if (btp_cap_ases_in_state(ASE_STATE_W2_CREATE_CIG)){
        btp_cap_ases_set_state(ASE_STATE_W4_CIG_CREATED);
        // ascs configured
        status = gap_cig_create(&btp_cap_cig, &btp_cap_cig_params);
        MESSAGE("gap_cig_create status 0x%02x", status);
    }

    // create cis when?
    if (btp_cap_ases_in_state(ASE_STATE_W2_CREATE_CIS)){
        btp_cap_ases_set_state(ASE_STATE_W4_CIS_CREATED);
        hci_con_handle_t acl_con_handles[MAX_NUM_SERVERS];
        hci_con_handle_t cis_con_handles[MAX_NUM_SERVERS];
        for (i=0;i<btp_cap_cig_params.num_cis;i++){
            acl_con_handles[i] = btp_cap_ases[i].acl_con_handle;
            cis_con_handles[i] = btp_cap_ases[i].cis_con_handle;
        }
        status = gap_cis_create(btp_cap_cig_params.cig_id, cis_con_handles, acl_con_handles);
        MESSAGE("gap_cis_create status 0x%02x", status);
    }

    // enable when all cis are created
    if (btp_cap_ases_in_state(ASE_STATE_W4_ALL_CIS_CREATED)) {
        MESSAGE("BTP CAP All CIS Created -> ASE_STATE_W2_ENABLE");
        btp_cap_ases_set_state(ASE_STATE_W2_ENABLE);
    }

    // trigger receiver start ready for remote remote sources when all are in enabling
    if (btp_cap_ases_in_state(ASE_STATE_ENABLING)){
        MESSAGE("BTP CAP All ASE in Enabling, send Receiver Start Ready");
        for (i=0;i<btp_bap_num_ases;i++){
            btp_cap_ase_t * ase = &btp_cap_ases[i];
            server_t * server = btp_server_for_index(ase->server_index);
            if (audio_stream_control_service_client_get_ase_role(server->ascs_cid, ase->ase_id) == LE_AUDIO_ROLE_SOURCE){
                // if we are SINK, tell remote SOURCE to start streaming
                ase->state = ASE_STATE_W2_RECEIVER_START_READY;
            } else {
                ase->state = ASE_STATE_W4_STREAMING;
            }
        }
    }

    if (btp_cap_ases_in_state(ASE_STATE_STREAMING)) {
        if (response_op == BTP_CAP_UNICAST_AUDIO_START) {
            response_op = 0;
            MESSAGE("BTP CAP All ASE in state Streaming -> BTP_CAP_EV_UNICAST_START_COMPLETED");
            btp_bap_start_audio_completed(btp_cap_cig_params.cig_id, BTP_CAP_UNICAST_START_STATUS_SUCCESS);
        }
    }

    // Handle single ASE
    for (i=0;i<btp_bap_num_ases;i++){
        btp_cap_ase_t * ase = &btp_cap_ases[i];
        server_t * server = btp_server_for_index(ase->server_index);
        switch (ase->state){
            case ASE_STATE_W2_CONFIGURE_CODEC:
                ase->state = ASE_STATE_W4_CODEC_CONFIGURED;
                status = audio_stream_control_service_client_streamendpoint_configure_codec(server->ascs_cid, ase->ase_id, &ase->codec_configuration_request);
                MESSAGE("BTP ASCS %u: Configure Codec for ase id %u -> 0x%02x", ase->server_index,  ase->ase_id, status);
                break;
            case ASE_STATE_W2_CONFIGURE_QOS:
                ase->state = ASE_STATE_W4_QOS_CONFIGURED;
                status = audio_stream_control_service_client_streamendpoint_configure_qos(server->ascs_cid, ase->ase_id,&ase->qos_configuration);
                MESSAGE("BTP ASCS %u: Configure QoS for ase id %u -> 0x%02x", server->server_id, ase->ase_id, status);
                break;
            case ASE_STATE_W2_ENABLE:
                ase->state = ASE_STATE_W4_ENABLING;
                status = audio_stream_control_service_client_streamendpoint_enable(server->ascs_cid, ase->ase_id, &ase->metadata);
                MESSAGE("BTP ASCS %u: Enable for ase id %u -> 0x%02x", server->server_id, ase->ase_id, status);
                return;
            case ASE_STATE_W2_RECEIVER_START_READY:
                ase->state = ASE_STATE_W4_STREAMING;
                status = audio_stream_control_service_client_streamendpoint_receiver_start_ready(server->ascs_cid, ase->ase_id);
                MESSAGE("BTP ASCS %u: Receiver Start Ready for ase id %u -> 0x%02x", server->server_id, ase->ase_id, status);
                return;
            default:
                MESSAGE("BTP ASCS %u state %u - wait", i, ase->state);
                break;
        }
    }

    btstack_assert(status == ERROR_CODE_SUCCESS);
}

#define MAX_NUM_CIS
static void btp_cap_hci_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    if (packet_type != HCI_EVENT_PACKET) return;
    uint8_t i;
    uint8_t cis_id;

    switch (hci_event_packet_get_type(packet)){
        case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)){
                case GAP_SUBEVENT_CIG_CREATED:
                    // TODO: check status
                    // store CIS handles, and prepare create cis
                    for (i=0;i<btp_cap_cig_params.num_cis;i++){
                        // TODO: check CIG ID
                        btp_cap_ases[i].cis_con_handle = gap_subevent_cig_created_get_cis_con_handles(packet, i);
                        btp_cap_ases[i].state = ASE_STATE_W2_CONFIGURE_QOS;
                    }
                    btp_cap_ases_run();
                    break;
                case GAP_SUBEVENT_CIS_CREATED:
                    // TODO: check CIG ID
                    for (i=0;i<btp_cap_cig_params.num_cis;i++) {
                        cis_id = gap_subevent_cis_created_get_cis_id(packet);
                        if (cis_id == btp_cap_ases[i].cis_id){
                            MESSAGE("CIS %u for server %u, ASE ID %u ready", cis_id, btp_cap_ases[i].server_index, btp_cap_ases[i].ase_id);
                            btp_cap_ases[i].state = ASE_STATE_W2_ENABLE;
                        }
                    }
                    btp_cap_ases_run();
                    break;
                case GAP_SUBEVENT_BIG_CREATED:
                    btp_cap_send_response_if_pending();
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
    ascs_state_t ase_state;
    uint16_t ascs_cid;
    uint8_t ase_id;
    server_t * server;
    uint8_t status = ERROR_CODE_SUCCESS;
    uint16_t pos;
    bool emit_state = false;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_CODEC_CONFIGURATION:
            ascs_cid = gattservice_subevent_ascs_client_codec_configuration_get_ascs_cid(packet);
            ase_id = gattservice_subevent_ascs_client_codec_configuration_get_ase_id(packet);
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
            MESSAGE("BTP ASCS %u: Codec Configured for ase_id %d", server->server_id, ase_id);
            break;

        case GATTSERVICE_SUBEVENT_ASCS_CLIENT_QOS_CONFIGURATION:
            ascs_cid = gattservice_subevent_ascs_server_qos_configuration_get_con_handle(packet);
            ase_id  = gattservice_subevent_ascs_server_qos_configuration_get_ase_id(packet);
            server = btp_server_for_ascs_cid(ascs_cid);
            btstack_assert(server != NULL);

            MESSAGE("BTP ASCS %u: QoS Configured for ase_id %d", server->server_id, ase_id);

            if (response_op == BTP_CAP_UNICAST_SETUP_ASE){
                MESSAGE("BTP_CAP_UNICAST_SETUP_ASE %u, ase id %u complete", server->server_id, ase_id);
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

            btp_cap_ase_t * ase;
            switch (response_op){
                case BTP_CAP_UNICAST_AUDIO_START:
                    ase = btp_cap_ase_for_server_index_and_ase_id(server->server_id, ase_id);
                    switch (ase->state){
                        case ASE_STATE_W4_CODEC_CONFIGURED:
                            if (ase_state == ASCS_STATE_CODEC_CONFIGURED){
                                ase->state = ASE_STATE_W2_CREATE_CIG;
                                emit_state = true;
                            } else {
                                MESSAGE("ASE State %u unexpected, expected %u", ase_state, ASCS_STATE_CODEC_CONFIGURED);
                            }
                            break;
                        case ASE_STATE_W4_QOS_CONFIGURED:
                            if (ase_state == ASCS_STATE_QOS_CONFIGURED){
                                ase->state = ASE_STATE_W2_ENABLE;
                                emit_state = true;
                            } else {
                                MESSAGE("ASE State %u unexpected, expected %u", ase_state, ASCS_STATE_QOS_CONFIGURED);
                            }
                            break;
                        case ASE_STATE_W4_ENABLING:
                            if (ase_state == ASCS_STATE_ENABLING){
                                ase->state = ASE_STATE_ENABLING;
                                emit_state = true;
                            } else {
                                MESSAGE("ASE State %u unexpected, expected %u", ase_state, ASCS_STATE_ENABLING);
                            }
                            break;
                        case ASE_STATE_W4_STREAMING:
                            if (ase_state == ASCS_STATE_STREAMING){
                                ase->state = ASE_STATE_STREAMING;
                                emit_state = true;
                            } else {
                                MESSAGE("ASE State %u unexpected, expected %u", ase_state, ASCS_STATE_STREAMING);
                            }
                            break;
                        case ASE_STATE_STREAMING:
                            // hack: keep state, but emit updates
                            emit_state = true;
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }

            // report ASE State changes
            // TODO: move into btp_ascs.c
            if (emit_state){
                uint8_t ase_state_changed_ev[9];
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

    btp_cap_ases_run();

    if (status != ERROR_CODE_SUCCESS){
        MESSAGE("BTP CAP Operation failed, status 0x%02x", status);
        btstack_assert(false);
    }
}

void btp_cap_setup_periodic_advertising_data(void) {
    // setup BASE
    btstack_assert(btp_cap_broadcast_stream_count > 0);
    btp_cap_broadcast_stream_t * stream = &broadcast_streams[0];
    btstack_assert(btp_cap_broadcast_subgroup_count > 0);
    btp_cap_broadcast_subgroup_t * subgroup = &broadcast_subgroups[0];
    uint8_t subgroup_codec_ltv_len = 0;
    const uint8_t * subgroup_codec_ltv_bytes = NULL;
    uint8_t stream_codec_ltv_len = 0;
    const uint8_t * stream_codec_ltv_bytes = NULL;
    if (btp_cap_broadcast_source_codec_config_at_subgroup){
        subgroup_codec_ltv_len = subgroup->codec_config_len;
        subgroup_codec_ltv_bytes = subgroup->codec_config_data;
    } else {
        stream_codec_ltv_len = stream->codec_config_len;
        stream_codec_ltv_bytes = subgroup->codec_config_data;
    }
    btp_bap_setup_periodic_advertising_data(btp_cap_broadcast_stream_count,
                                            btp_cap_broadcast_source_presenentation_delay_us, subgroup->codec_id,
                                            subgroup_codec_ltv_len, subgroup_codec_ltv_bytes, subgroup->metadata_len,
                                            subgroup->metadata_data,
                                            stream_codec_ltv_len, stream_codec_ltv_bytes);
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

                // Reset state

                // Unicast CIG params + ASEs
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
                // Broadcast
                btp_cap_broadcast_stream_count = 0;
                btp_cap_broadcast_subgroup_count = 0;
            }
            break;
        case BTP_CAP_UNICAST_SETUP_ASE:
            if (controller_index == 0){

                btp_cap_ase_t * ase = &btp_cap_ases[btp_bap_num_ases++];

                // lookup server
                int offset = 0;
                bd_addr_type_t addr_type = data[offset++];
                bd_addr_t address;
                reverse_bd_addr(&data[1], address);
                server = btp_server_for_address(addr_type, address);
                offset += 6;

                // get ASE info
                uint8_t ase_id = data[offset++];
                uint8_t cis_id = data[offset++];
                uint8_t cig_id = data[offset++];

                // setup ASE
                ase->state = ASE_STATE_IDLE;
                ase->ase_id = ase_id;
                ase->cig_id = cig_id;
                ase->cis_id = cis_id;
                ase->ase_id = ase_id;
                ase->server_index = server->server_id;
                ase->acl_con_handle = server->acl_con_handle;
                ase->cis_con_handle = HCI_CON_HANDLE_INVALID;

                MESSAGE("BTP_CAP_UNICAST_SETUP_ASE %u, ase id %u", server->server_id, ase->ase_id );

                // store codec config
                ase->qos_configuration.cig_id = cig_id;
                ase->qos_configuration.cis_id = cis_id;
                ase->codec_configuration_request.coding_format = data[offset++];
                ase->codec_configuration_request.company_id = little_endian_read_16(data, offset);
                offset += 2;
                ase->codec_configuration_request.vendor_specific_codec_id = little_endian_read_16(data, offset);
                offset += 2;

                // store qos config
                uint32_t sdu_interval_us = little_endian_read_24(data, offset);
                ase->qos_configuration.sdu_interval = sdu_interval_us;
                offset += 3;
                ase->qos_configuration.framing = data[offset++];
                uint16_t max_sdu = little_endian_read_16(data, offset);
                ase->qos_configuration.max_sdu = max_sdu;
                offset += 2;
                uint8_t retransmission_number = data[offset++];
                ase->qos_configuration.retransmission_number = retransmission_number;
                uint16_t max_transport_latency_ms = little_endian_read_16(data, offset);
                ase->qos_configuration.max_transport_latency_ms = max_transport_latency_ms;
                offset += 2;
                ase->qos_configuration.presentation_delay_us =  little_endian_read_24(data, offset);
                offset += 3;

                // parse codec config and meta data
                uint8_t codec_config_ltvs_length = data[offset++];
                uint8_t metadata_ltvs_length = data[offset++];
                ascs_util_specific_codec_configuration_parse(&data[offset], codec_config_ltvs_length, &ase->codec_configuration_request.specific_codec_configuration);
                offset += codec_config_ltvs_length;

                // metadata parser expects lenght in first field
                uint8_t metadata[256];
                metadata[0] = metadata_ltvs_length;
                memcpy(&metadata[1], &data[offset], metadata_ltvs_length);
                uint16_t parsed_bytes = le_audio_util_metadata_parse(metadata, metadata_ltvs_length + 1, &ase->metadata);
                MESSAGE("Parsed %u of %u meta data bytes", parsed_bytes, metadata_ltvs_length);
                log_info_hexdump(&metadata[1], metadata_ltvs_length);

                // collect CIG params
                le_audio_role_t direction = audio_stream_control_service_client_get_ase_role(server->ascs_cid, ase_id);
                uint8_t cis_index = btp_cap_cig_params.num_cis;
                btstack_assert(cig_id == 0);
                btp_cap_cig_params.cig_id = cig_id;
                btp_cap_cig_params.cis_params[cis_index].cis_id = cis_id;
                btp_cap_cig_params.cis_params[cis_index].phy_c_to_p = LE_AUDIO_SERVER_PHY_MASK_2M;
                btp_cap_cig_params.framing = ase->qos_configuration.framing;
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

                // local setup completed
                btp_send(response_service_id, response_op, 0, 0, NULL);
            }
            break;
        case BTP_CAP_UNICAST_AUDIO_START:
            if (controller_index == 0){
                uint8_t cig_id = data[0];
                uint8_t set_type = data[1];
                btp_send(response_service_id, response_op, 0, 0, NULL);

                // TODO: support ordered access
                MESSAGE("Unicast audio start, num ASEs %u", btp_bap_num_ases);

                btp_cap_ases_set_state(ASE_STATE_W2_CONFIGURE_CODEC);

                // go
                btp_cap_ases_run();
            }
            break;
        case BTP_CAP_BROADCAST_SOURCE_SETUP_STREAM:
            if (controller_index == 0) {
                btp_cap_broadcast_stream_t * stream = &broadcast_streams[btp_cap_broadcast_stream_count++];
                uint16_t offset = 0;
                stream->source_id = data[offset++];
                stream->subgroup_id = data[offset++];
                memcpy(stream->codec_id, &data[offset], 5);
                offset += 5;
                stream->codec_config_len = data[offset++];
                stream->metadata_len = data[offset++];
                memcpy(stream->codec_config_data, &data[offset], stream->codec_config_len);
                offset += stream->codec_config_len;
                memcpy(stream->metadata_data, &data[offset], stream->metadata_len);
                MESSAGE("BTP_CAP_BROADCAST_SOURCE_SETUP_STREAM");
                MESSAGE("Codec Config");
                log_info_hexdump(stream->codec_config_data, stream->codec_config_len);
                MESSAGE("Metadata");
                log_info_hexdump(stream->metadata_data, stream->metadata_len);
                // local setup completed
                btp_send(response_service_id, response_op, 0, 0, NULL);
            }
            break;
        case BTP_CAP_BROADCAST_SOURCE_SETUP_SUBGROUP:
            if (controller_index == 0) {
                uint16_t offset = 0;
                uint8_t source_id = data[offset++];
                uint8_t subgroup_id = data[offset++];
                if (subgroup_id != btp_cap_broadcast_subgroup_count){
                    MESSAGE("BTP_CAP_BROADCAST_SOURCE_SETUP_SUBGROUP: expected subgroup id %u, but got %u -> abort", btp_cap_broadcast_subgroup_count, subgroup_id);
                    btstack_assert(false);
                }
                btp_cap_broadcast_subgroup_t * subgroup = &broadcast_subgroups[btp_cap_broadcast_subgroup_count++];
                subgroup->source_id = source_id;
                memcpy(subgroup->codec_id, &data[offset], 5);
                offset += 5;
                subgroup->codec_config_len = data[offset++];
                subgroup->metadata_len = data[offset++];
                memcpy(subgroup->codec_config_data, &data[offset], subgroup->codec_config_len);
                offset += subgroup->codec_config_len;
                memcpy(subgroup->metadata_data, &data[offset], subgroup->metadata_len);
                MESSAGE("BTP_CAP_BROADCAST_SOURCE_SETUP_SUBGROUP");
                MESSAGE("Codec Config");
                log_info_hexdump(subgroup->codec_config_data, subgroup->codec_config_len);
                MESSAGE("Metadata");
                log_info_hexdump(subgroup->metadata_data, subgroup->metadata_len);
                // local setup completed
                btp_send(response_service_id, response_op, 0, 0, NULL);
            }
            break;
        case BTP_CAP_BROADCAST_SOURCE_SETUP:
#if 0
            struct btp_cap_broadcast_source_setup_cmd {
                uint8_t source_id;
                uint8_t broadcast_id[3];
                uint8_t sdu_interval[3];
                uint8_t framing;
                uint16_t max_sdu;
                uint8_t retransmission_num;
                uint16_t max_transport_latency;
                uint8_t presentation_delay[3];
                uint8_t flags;
                uint8_t broadcast_code[16];
            };
            struct btp_cap_broadcast_source_setup_rp {
                uint8_t source_id;
                uint32_t gap_settings;
                uint8_t broadcast_id[16];
            };
#define BTP_CAP_BROADCAST_SOURCE_SETUP_FLAG_ENCRYPTION		BIT(0)
#define BTP_CAP_BROADCAST_SOURCE_SETUP_FLAG_SUBGROUP_CODEC	BIT(1)
#endif
            if (controller_index == 0) {
                MESSAGE("BTP_CAP_BROADCAST_SOURCE_SETUP, subgroups %u, streams %u",
                        btp_cap_broadcast_subgroup_count, btp_cap_broadcast_stream_count);
                uint16_t pos = 0;
                uint8_t  source_id = data[pos++];
                uint32_t broadcast_id = little_endian_read_24(data, pos);
                pos += 3;
                uint32_t sdu_interval = little_endian_read_24(data, pos);
                pos += 3;
                uint8_t framing = data[pos++];
                uint16_t max_sdu = little_endian_read_16(data, pos);
                pos += 2;
                uint8_t retransmission_num = data[pos++];
                uint16_t max_transport_latency = little_endian_read_16(data, pos);
                pos += 2;
                uint32_t presentation_delay = little_endian_read_24(data, pos);
                pos += 3;
                uint8_t flags = data[pos++];
                const uint8_t * broadcast_code = &data[pos];
                // - Flags is a bit map of settings:
                // bit 0: Use encryption and the given Broadcast Code
                // bit 1: Setup Codec Config at Subgroup level

                // setup BIG params
                btp_bap_big_params.num_bis = btp_cap_broadcast_stream_count;
                btp_bap_big_params.framing = framing;
                btp_bap_big_params.max_sdu = max_sdu;
                btp_bap_big_params.rtn = retransmission_num;
                btp_bap_big_params.max_transport_latency_ms = max_transport_latency;
                btp_bap_big_params.sdu_interval_us = sdu_interval;
                btp_bap_big_params.encryption = ((flags & BTP_CAP_BROADCAST_SOURCE_SETUP_FLAG_ENCRYPTION) != 0) ? 1 : 0;
                memcpy(btp_bap_big_params.broadcast_code, broadcast_code, 16);
                btp_cap_broadcast_source_codec_config_at_subgroup = (flags & BTP_CAP_BROADCAST_SOURCE_SETUP_FLAG_SUBGROUP_CODEC) != 0;
                btp_cap_broadcast_source_presenentation_delay_us = presentation_delay;

                // setup data for periodic advertising
                btp_cap_setup_periodic_advertising_data();

                // parse LTV and configure codec
                btp_cap_broadcast_subgroup_t * subgroup = &broadcast_subgroups[0];
                uint32_t sampling_frequency_hz = 0;
                pos = 0;
                uint8_t ltv_len = subgroup->codec_config_len;
                const uint8_t * ltv_data = subgroup->codec_config_data;
                while (pos < ltv_len){
                    uint8_t ltv_entry_len = ltv_data[pos++];
                    uint8_t codec_config_type = ltv_data[pos];
                    if (codec_config_type == LE_AUDIO_CODEC_CONFIGURATION_TYPE_SAMPLING_FREQUENCY){
                        sampling_frequency_hz = le_audio_get_sampling_frequency_hz(ltv_data[pos]);
                    }
                    pos += ltv_entry_len;
                }
                btstack_lc3_frame_duration_t frame_duration =
                        btp_bap_big_params.sdu_interval_us == 7500 ? BTSTACK_LC3_FRAME_DURATION_7500US : BTSTACK_LC3_FRAME_DURATION_10000US;
                le_audio_demo_util_source_configure(btp_bap_big_params.num_bis, 1, sampling_frequency_hz, frame_duration, btp_bap_big_params.max_sdu);
                MESSAGE("LC3 Config: streams %u, channels per stream %u, sampling frequency %u, frame duration %u, max sdu %u", btp_bap_big_params.num_bis, 1, sampling_frequency_hz, btp_bap_big_params.sdu_interval_us, btp_bap_big_params.max_sdu);
                btstack_assert(sampling_frequency_hz != 0);

                // response
                pos = 0;
                uint8_t response[21];
                response[pos++] = source_id;
                little_endian_store_32(response, pos, btp_gap_current_settings());
                pos += 4;
                memcpy(&response[pos], broadcast_code, 16);
                pos += 16;
                // local setup completed
                btp_send(response_service_id, response_op, 0, pos, response);
            }
            break;
        case BTP_CAP_BROADCAST_SOURCE_RELEASE:
#if 0
            struct btp_cap_broadcast_source_release_cmd {
                uint8_t source_id;
            };
#endif
            if (controller_index == 0) {
                MESSAGE("BTP_CAP_BROADCAST_SOURCE_SETUP_STREAM");
            }
            break;

        case BTP_CAP_BROADCAST_ADV_START:
            if (controller_index == 0) {
                MESSAGE("BTP_CAP_BROADCAST_ADV_START");
                uint8_t source_id = data[0];
                btstack_assert(source_id == 0);
                btp_bap_start_advertising(0);
                btp_send(response_service_id, opcode, controller_index, 0, NULL);
            }
            break;

        case BTP_CAP_BROADCAST_ADV_STOP:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_BROADCAST_ADV_STOP");
                uint8_t source_id = data[0];
                btstack_assert(source_id == 0);
                btp_bap_stop_advertising();
                btp_send(response_service_id, opcode, controller_index, 0, NULL);
            }
            break;

        case BTP_CAP_BROADCAST_SOURCE_START:
#if 0
            struct btp_cap_broadcast_source_start_cmd {
                uint8_t source_id;
            };
#endif
            if (controller_index == 0) {
                MESSAGE("BTP_CAP_BROADCAST_SOURCE_START");
                uint8_t source_id = data[0];
                btstack_assert(source_id == 0);
                btp_bap_setup_big();
            }
            break;

        case BTP_CAP_BROADCAST_SOURCE_STOP:
            if (controller_index == 0) {
                MESSAGE("BTP_CAP_BROADCAST_SOURCE_STOP");
                uint8_t source_id = data[0];
                btstack_assert(source_id == 0);
                btp_bap_release_big();
                btp_send(response_service_id, opcode, controller_index, 0, NULL);
            }
            break;

        case BTP_CAP_BROADCAST_SOURCE_UPDATE:
            if (controller_index == 0) {
                MESSAGE("BTP_CAP_BROADCAST_SOURCE_UPDATE");
                // get new metadata
                uint8_t source_id = data[0];
                btstack_assert(source_id == 0);
                btp_cap_broadcast_subgroup_t * subgroup = &broadcast_subgroups[0];
                subgroup->metadata_len = data[1];
                memcpy(subgroup->metadata_data, &data[2], subgroup->metadata_len);

                // setup data for periodic advertising
                btp_cap_setup_periodic_advertising_data();

                btp_send(response_service_id, opcode, controller_index, 0, NULL);
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
