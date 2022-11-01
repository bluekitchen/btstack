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

#define BTSTACK_FILE__ "mesh_lower_transport.c"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_memory.h"
#include "btstack_util.h"
#include "btstack_bool.h"
#include "btstack_debug.h"

#include "mesh/beacon.h"
#include "mesh/mesh_iv_index_seq_number.h"
#include "mesh/mesh_lower_transport.h"
#include "mesh/mesh_node.h"
#include "mesh/mesh_peer.h"

#define LOG_LOWER_TRANSPORT

// prototypes
static void mesh_lower_transport_run(void);
static void mesh_lower_transport_outgoing_complete(mesh_segmented_pdu_t * segmented_pdu, mesh_transport_status_t status);
static void mesh_lower_transport_outgoing_segment_transmission_timeout(btstack_timer_source_t * ts);


// lower transport outgoing state

// queued mesh_segmented_pdu_t or mesh_network_pdu_t
static btstack_linked_list_t lower_transport_outgoing_ready;

// mesh_segmented_pdu_t to unicast address, segment transmission timer is active
static btstack_linked_list_t lower_transport_outgoing_waiting;

// active outgoing segmented message
static mesh_segmented_pdu_t * lower_transport_outgoing_message;
// index of outgoing segment
static uint16_t               lower_transport_outgoing_seg_o;
// network pdu with outgoing segment
static mesh_network_pdu_t   * lower_transport_outgoing_segment;
// segment currently queued at network layer (only valid for lower_transport_outgoing_message)
static bool                    lower_transport_outgoing_segment_at_network_layer;
// transmission timeout occurred (while outgoing segment queued at network layer)
static bool                    lower_transport_outgoing_transmission_timeout;
// transmission completed fully ack'ed (while outgoing segment queued at network layer)
static bool                    lower_transport_outgoing_transmission_complete;
// transmission aborted by remote (while outgoing segment queued at network layer)
static bool                    lower_transport_outgoing_transmission_aborted;

// active outgoing unsegmented message
static mesh_network_pdu_t *   lower_transport_outgoing_network_pdu;

// deliver to higher layer
static void (*higher_layer_handler)( mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu);
static mesh_pdu_t * mesh_lower_transport_higher_layer_pdu;
static btstack_linked_list_t mesh_lower_transport_queued_for_higher_layer;

static void mesh_print_hex(const char * name, const uint8_t * data, uint16_t len){
    printf("%-20s ", name);
    printf_hexdump(data, len);
}

// utility

mesh_segmented_pdu_t * mesh_segmented_pdu_get(void){
    mesh_segmented_pdu_t * message_pdu = btstack_memory_mesh_segmented_pdu_get();
    if (message_pdu){
        message_pdu->pdu_header.pdu_type = MESH_PDU_TYPE_SEGMENTED;
    }
    return message_pdu;
}

void mesh_segmented_pdu_free(mesh_segmented_pdu_t * message_pdu){
    while (message_pdu->segments){
        mesh_network_pdu_t * segment = (mesh_network_pdu_t *) btstack_linked_list_pop(&message_pdu->segments);
        mesh_network_pdu_free(segment);
    }
    btstack_memory_mesh_segmented_pdu_free(message_pdu);
}

// INCOMING //

static void mesh_lower_transport_incoming_deliver_to_higher_layer(void){
    if (mesh_lower_transport_higher_layer_pdu == NULL && !btstack_linked_list_empty(&mesh_lower_transport_queued_for_higher_layer)){
        mesh_pdu_t * pdu = (mesh_pdu_t *) btstack_linked_list_pop(&mesh_lower_transport_queued_for_higher_layer);

        switch (pdu->pdu_type){
            case MESH_PDU_TYPE_NETWORK:
                // unsegmented pdu
                mesh_lower_transport_higher_layer_pdu = (mesh_pdu_t *) pdu;
                pdu->pdu_type = MESH_PDU_TYPE_UNSEGMENTED;
                break;
            case MESH_PDU_TYPE_SEGMENTED:
                // segmented control or access pdu
                mesh_lower_transport_higher_layer_pdu = pdu;
                break;
            default:
                btstack_assert(false);
                break;
        }
        higher_layer_handler(MESH_TRANSPORT_PDU_RECEIVED, MESH_TRANSPORT_STATUS_SUCCESS, mesh_lower_transport_higher_layer_pdu);
    }
}

static void mesh_lower_transport_incoming_queue_for_higher_layer(mesh_pdu_t * pdu){
    btstack_linked_list_add_tail(&mesh_lower_transport_queued_for_higher_layer, (btstack_linked_item_t *) pdu);
    mesh_lower_transport_incoming_deliver_to_higher_layer();
}

static void mesh_lower_transport_incoming_setup_acknowledge_message(uint8_t * data, uint8_t obo, uint16_t seq_zero, uint32_t block_ack){
    // printf("ACK Upper Transport, seq_zero %x\n", seq_zero);
    data[0] = 0;    // SEG = 0, Opcode = 0
    big_endian_store_16( data, 1, (obo << 15) | (seq_zero << 2) | 0);    // OBO, SeqZero, RFU
    big_endian_store_32( data, 3, block_ack);
#ifdef LOG_LOWER_TRANSPORT
    mesh_print_hex("ACK Upper Transport", data, 7);
#endif
}

static void mesh_lower_transport_incoming_send_ack(uint16_t netkey_index, uint8_t ttl, uint16_t dest, uint16_t seq_zero, uint32_t block_ack){
    // setup ack message
    uint8_t  ack_msg[7];
    mesh_lower_transport_incoming_setup_acknowledge_message(ack_msg, 0, seq_zero, block_ack);
    //
    // "3.4.5.2: The output filter of the interface connected to advertising or GATT bearers shall drop all messages with TTL value set to 1."
    // if (ttl <= 1) return 0;

    // TODO: check transport_pdu_len depending on ctl

    // lookup network by netkey_index
    const mesh_network_key_t * network_key = mesh_network_key_list_get(netkey_index);
    if (!network_key) return;

    // allocate network_pdu
    mesh_network_pdu_t * network_pdu = mesh_network_pdu_get();
    if (!network_pdu) return;

    // setup network_pdu
    network_pdu->pdu_header.pdu_type = MESH_PDU_TYPE_SEGMENT_ACKNOWLEDGMENT;
    mesh_network_setup_pdu(network_pdu, netkey_index, network_key->nid, 1, ttl, mesh_sequence_number_next(), mesh_node_get_primary_element_address(), dest, ack_msg, sizeof(ack_msg));

    // send network_pdu
    mesh_network_send_pdu(network_pdu);
}

static void mesh_lower_transport_incoming_send_ack_for_segmented_pdu(mesh_segmented_pdu_t * segmented_pdu){
    uint16_t seq_zero = segmented_pdu->seq & 0x1fff;
    uint8_t  ttl = segmented_pdu->ctl_ttl & 0x7f;
    uint16_t dest = segmented_pdu->src;
    uint16_t netkey_index = segmented_pdu->netkey_index;
#ifdef LOG_LOWER_TRANSPORT
    printf("mesh_transport_send_ack_for_transport_pdu %p with netkey_index %x, TTL = %u, SeqZero = %x, SRC = %x, DST = %x\n",
           segmented_pdu, netkey_index, ttl, seq_zero, mesh_node_get_primary_element_address(), dest);
#endif
    mesh_lower_transport_incoming_send_ack(netkey_index, ttl, dest, seq_zero, segmented_pdu->block_ack);
}

static void mesh_lower_transport_incoming_send_ack_for_network_pdu(mesh_network_pdu_t *network_pdu, uint16_t seq_zero, uint32_t block_ack) {
    uint8_t ttl = mesh_network_ttl(network_pdu);
    uint16_t dest = mesh_network_src(network_pdu);
    uint16_t netkey_index = network_pdu->netkey_index;
#ifdef LOG_LOWER_TRANSPORT
    printf("mesh_transport_send_ack_for_network_pdu %p with netkey_index %x, TTL = %u, SeqZero = %x, SRC = %x, DST = %x\n",
           network_pdu, netkey_index, ttl, seq_zero, mesh_node_get_primary_element_address(), dest);
#endif
    mesh_lower_transport_incoming_send_ack(netkey_index, ttl, dest, seq_zero, block_ack);
}

static void mesh_lower_transport_incoming_stop_acknowledgment_timer(mesh_segmented_pdu_t *segmented_pdu){
    if ((segmented_pdu->flags & MESH_TRANSPORT_FLAG_ACK_TIMER) == 0) return;
    segmented_pdu->flags &= ~MESH_TRANSPORT_FLAG_ACK_TIMER;
    btstack_run_loop_remove_timer(&segmented_pdu->acknowledgement_timer);
}

static void mesh_lower_transport_incoming_stop_incomplete_timer(mesh_segmented_pdu_t *segmented_pdu){
    if ((segmented_pdu->flags & MESH_TRANSPORT_FLAG_INCOMPLETE_TIMER) == 0) return;
    segmented_pdu->flags &= ~MESH_TRANSPORT_FLAG_INCOMPLETE_TIMER;
    btstack_run_loop_remove_timer(&segmented_pdu->incomplete_timer);
}

static void mesh_lower_transport_incoming_segmented_message_complete(mesh_segmented_pdu_t * segmented_pdu){
    // stop timers
    mesh_lower_transport_incoming_stop_acknowledgment_timer(segmented_pdu);
    mesh_lower_transport_incoming_stop_incomplete_timer(segmented_pdu);
    // stop reassembly
    mesh_peer_t * peer = mesh_peer_for_addr(segmented_pdu->src);
    if (peer){
        peer->message_pdu = NULL;
    }
}

static void mesh_lower_transport_incoming_ack_timeout(btstack_timer_source_t *ts){
    mesh_segmented_pdu_t * segmented_pdu = (mesh_segmented_pdu_t *) btstack_run_loop_get_timer_context(ts);
#ifdef LOG_LOWER_TRANSPORT
    printf("ACK: acknowledgement timer fired for %p, send ACK\n", segmented_pdu);
#endif
    segmented_pdu->flags &= ~MESH_TRANSPORT_FLAG_ACK_TIMER;
    mesh_lower_transport_incoming_send_ack_for_segmented_pdu(segmented_pdu);
}

static void mesh_lower_transport_incoming_incomplete_timeout(btstack_timer_source_t *ts){
    mesh_segmented_pdu_t * segmented_pdu = (mesh_segmented_pdu_t *) btstack_run_loop_get_timer_context(ts);
#ifdef LOG_LOWER_TRANSPORT
    printf("mesh_lower_transport_incoming_incomplete_timeout for %p - give up\n", segmented_pdu);
#endif
    mesh_lower_transport_incoming_segmented_message_complete(segmented_pdu);
    // free message
    mesh_segmented_pdu_free(segmented_pdu);
}

static void mesh_lower_transport_incoming_start_acknowledgment_timer(mesh_segmented_pdu_t * segmented_pdu, uint32_t timeout){
#ifdef LOG_LOWER_TRANSPORT
    printf("ACK: start rx ack timer for %p, timeout %u ms\n", segmented_pdu, (int) timeout);
#endif
    btstack_run_loop_set_timer(&segmented_pdu->acknowledgement_timer, timeout);
    btstack_run_loop_set_timer_handler(&segmented_pdu->acknowledgement_timer, &mesh_lower_transport_incoming_ack_timeout);
    btstack_run_loop_set_timer_context(&segmented_pdu->acknowledgement_timer, segmented_pdu);
    btstack_run_loop_add_timer(&segmented_pdu->acknowledgement_timer);
    segmented_pdu->flags |= MESH_TRANSPORT_FLAG_ACK_TIMER;
}

static void mesh_lower_transport_incoming_restart_incomplete_timer(mesh_segmented_pdu_t * segmented_pdu, uint32_t timeout,
                                                                   void (*callback)(btstack_timer_source_t *ts)){
#ifdef LOG_LOWER_TRANSPORT
    printf("RX-(re)start incomplete timer for %p, timeout %u ms\n", segmented_pdu, (int) timeout);
#endif
    if ((segmented_pdu->flags & MESH_TRANSPORT_FLAG_INCOMPLETE_TIMER) != 0){
        btstack_run_loop_remove_timer(&segmented_pdu->incomplete_timer);
    }
    btstack_run_loop_set_timer(&segmented_pdu->incomplete_timer, timeout);
    btstack_run_loop_set_timer_handler(&segmented_pdu->incomplete_timer, callback);
    btstack_run_loop_set_timer_context(&segmented_pdu->incomplete_timer, segmented_pdu);
    btstack_run_loop_add_timer(&segmented_pdu->incomplete_timer);
    segmented_pdu->flags |= MESH_TRANSPORT_FLAG_INCOMPLETE_TIMER;
}

static mesh_segmented_pdu_t * mesh_lower_transport_incoming_pdu_for_segmented_message(mesh_network_pdu_t *network_pdu){
    uint16_t src = mesh_network_src(network_pdu);
    uint16_t seq_zero = ( big_endian_read_16(mesh_network_pdu_data(network_pdu), 1) >> 2) & 0x1fff;
#ifdef LOG_LOWER_TRANSPORT
    printf("mesh_transport_pdu_for_segmented_message: seq_zero %x\n", seq_zero);
#endif
    mesh_peer_t * peer = mesh_peer_for_addr(src);
    if (!peer) {
        return NULL;
    }
#ifdef LOG_LOWER_TRANSPORT
    printf("mesh_seq_zero_validate(%x, %x) -- last (%x, %x)\n", src, seq_zero, peer->address, peer->seq_zero);
#endif

    // reception of transport message ongoing
    if (peer->message_pdu){
        // check if segment for same seq zero
        uint16_t active_seq_zero = peer->message_pdu->seq & 0x1fff;
        if (active_seq_zero == seq_zero) {
#ifdef LOG_LOWER_TRANSPORT
            printf("mesh_transport_pdu_for_segmented_message: segment for current transport pdu with SeqZero %x\n", active_seq_zero);
#endif
            return peer->message_pdu;
        } else {
            // seq zero differs from current transport pdu, but current pdu is not complete
#ifdef LOG_LOWER_TRANSPORT
            printf("mesh_transport_pdu_for_segmented_message: drop segment. current transport pdu SeqZero %x, now %x\n", active_seq_zero, seq_zero);
#endif
            return NULL;
        }
    }

    // send ACK if segment for previously completed transport pdu (no ongoing reception, block ack is cleared)
    if ((seq_zero == peer->seq_zero) && (peer->block_ack != 0)){
#ifdef LOG_LOWER_TRANSPORT
        printf("mesh_transport_pdu_for_segmented_message: segment for last completed message. send ack\n");
#endif
        mesh_lower_transport_incoming_send_ack_for_network_pdu(network_pdu, seq_zero, peer->block_ack);
        return NULL;
    }

    // reconstruct lowest 24 bit of SeqAuth
    uint32_t seq = mesh_network_seq(network_pdu);
    uint32_t seq_auth = (seq & 0xffe000) | seq_zero;
    if (seq_auth > seq){
        seq_auth -= 0x2000;
    }

    // no transport pdu active, check new message: seq auth is greater OR seq auth is same but no segments
    if (seq_auth > peer->seq_auth || (seq_auth == peer->seq_auth && peer->block_ack == 0)){
        mesh_segmented_pdu_t * pdu = mesh_segmented_pdu_get();
        if (!pdu) return NULL;

        // cache network pdu header
        pdu->ivi_nid = network_pdu->data[0];
        pdu->ctl_ttl = network_pdu->data[1];
        pdu->src = big_endian_read_16(network_pdu->data, 5);
        pdu->dst = big_endian_read_16(network_pdu->data, 7);
        // store lower 24 bit of SeqAuth for App / Device Nonce
        pdu->seq = seq_auth;

        // get akf_aid & transmic
        pdu->akf_aid_control = network_pdu->data[9] & 0x7f;
        if ((network_pdu->data[10] & 0x80) != 0){
            pdu->flags |= MESH_TRANSPORT_FLAG_TRANSMIC_64;
        }

        // store meta data in new pdu
        pdu->netkey_index = network_pdu->netkey_index;
        pdu->block_ack = 0;
        pdu->flags &= ~MESH_TRANSPORT_FLAG_ACK_TIMER;

        // update peer info
        peer->message_pdu   = pdu;
        peer->seq_zero      = seq_zero;
        peer->seq_auth      = seq_auth;
        peer->block_ack     = 0;

#ifdef LOG_LOWER_TRANSPORT
        printf("mesh_transport_pdu_for_segmented_message: setup transport pdu %p for src %x, seq %06" PRIx32 ", seq_zero %x\n", pdu, src,
               pdu->seq, seq_zero);
#endif
        return peer->message_pdu;
    }  else {
        // seq zero differs from current transport pdu
#ifdef LOG_LOWER_TRANSPORT
        printf("mesh_transport_pdu_for_segmented_message: drop segment for old seq %x\n", seq_zero);
#endif
        return NULL;
    }
}

static void mesh_lower_transport_incoming_process_segment(mesh_segmented_pdu_t * message_pdu, mesh_network_pdu_t * network_pdu){

    uint8_t * lower_transport_pdu     = mesh_network_pdu_data(network_pdu);
    uint8_t   lower_transport_pdu_len = mesh_network_pdu_len(network_pdu);

    // get seq_zero
    uint16_t seq_zero =  ( big_endian_read_16(lower_transport_pdu, 1) >> 2) & 0x1fff;

    // get seg fields
    uint8_t  seg_o    =  ( big_endian_read_16(lower_transport_pdu, 2) >> 5) & 0x001f;
    uint8_t  seg_n    =  lower_transport_pdu[3] & 0x1f;
    uint8_t   segment_len  =  lower_transport_pdu_len - 4;
    uint8_t * segment_data = &lower_transport_pdu[4];

#ifdef LOG_LOWER_TRANSPORT
    uint8_t transmic_len = ((message_pdu->flags & MESH_TRANSPORT_FLAG_TRANSMIC_64) != 0) ? 64 : 32;
    printf("mesh_lower_transport_incoming_process_segment: seq zero %04x, seg_o %02x, seg_n %02x, transmic len: %u bit\n", seq_zero, seg_o, seg_n, transmic_len);
    mesh_print_hex("Segment", segment_data, segment_len);
#endif

    // drop if already stored
    if ((message_pdu->block_ack & (1<<seg_o)) != 0){
        mesh_network_message_processed_by_higher_layer(network_pdu);
        return;
    }

    // mark as received
    message_pdu->block_ack |= (1<<seg_o);

    // store segment
    uint8_t max_segment_len = mesh_network_control(network_pdu) ? 8 : 12;
    mesh_network_pdu_t * latest_segment = (mesh_network_pdu_t *) btstack_linked_list_get_first_item(&message_pdu->segments);
    if ((latest_segment != NULL) && ((MESH_NETWORK_PAYLOAD_MAX - latest_segment->len) > (max_segment_len  + 1))){
        // store in last added segment if there is enough space available
        latest_segment->data[latest_segment->len++] = seg_o;
        (void) memcpy(&latest_segment->data[latest_segment->len], &lower_transport_pdu[4], segment_len);
        latest_segment->len += segment_len;
        // free buffer
        mesh_network_message_processed_by_higher_layer(network_pdu);
    } else {
        // move to beginning
        network_pdu->data[0] = seg_o;
        uint8_t i;
        for (i=0;i<segment_len;i++){
            network_pdu->data[1+i] = network_pdu->data[13+i];
        }
        network_pdu->len = 1 + segment_len;
        // add this buffer
        btstack_linked_list_add(&message_pdu->segments, (btstack_linked_item_t *) network_pdu);
    }

    // last segment -> store len
    if (seg_o == seg_n){
        message_pdu->len = (seg_n * max_segment_len) + segment_len;
#ifdef LOG_LOWER_TRANSPORT
        printf("Assembled payload len %u\n", message_pdu->len);
#endif
    }

    // check for complete
    int i;
    for (i=0;i<=seg_n;i++){
        if ( (message_pdu->block_ack & (1<<i)) == 0) return;
    }

    // store block ack in peer info
    mesh_peer_t * peer = mesh_peer_for_addr(message_pdu->src);
    // TODO: check if NULL check can be removed
    if (peer){
        peer->block_ack = message_pdu->block_ack;
    }

    // send ack
    mesh_lower_transport_incoming_send_ack_for_segmented_pdu(message_pdu);

    // forward to upper transport
    mesh_lower_transport_incoming_queue_for_higher_layer((mesh_pdu_t *) message_pdu);

    // mark as done
    mesh_lower_transport_incoming_segmented_message_complete(message_pdu);
}

void mesh_lower_transport_message_processed_by_higher_layer(mesh_pdu_t * pdu){
    btstack_assert(pdu == mesh_lower_transport_higher_layer_pdu);
    mesh_lower_transport_higher_layer_pdu = NULL;
    mesh_network_pdu_t * network_pdu;
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_SEGMENTED:
            // free segments
            mesh_segmented_pdu_free((mesh_segmented_pdu_t *) pdu);
            break;
        case MESH_PDU_TYPE_UNSEGMENTED:
            network_pdu = (mesh_network_pdu_t *) pdu;
            mesh_network_message_processed_by_higher_layer(network_pdu);
            break;
        default:
            btstack_assert(0);
            break;
    }
    mesh_lower_transport_incoming_deliver_to_higher_layer();
}

// OUTGOING //

static void mesh_lower_transport_outgoing_setup_block_ack(mesh_segmented_pdu_t *message_pdu){
    // setup block ack - set bit for segment to send, will be cleared on ack
    int      ctl = message_pdu->ctl_ttl >> 7;
    uint16_t max_segment_len = ctl ? 8 : 12;    // control 8 bytes (64 bit NetMic), access 12 bytes (32 bit NetMIC)
    uint8_t  seg_n = (message_pdu->len - 1) / max_segment_len;
    if (seg_n < 31){
        message_pdu->block_ack = (1 << (seg_n+1)) - 1;
    } else {
        message_pdu->block_ack = 0xffffffff;
    }
}

static mesh_segmented_pdu_t * mesh_lower_transport_outgoing_message_for_dst(uint16_t dst){
    if (lower_transport_outgoing_message != NULL && lower_transport_outgoing_message->dst == dst){
        return lower_transport_outgoing_message;
    }
    return NULL;
}

static void mesh_lower_transport_outgoing_process_segment_acknowledgement_message(mesh_network_pdu_t *network_pdu){
    mesh_segmented_pdu_t * segmented_pdu = mesh_lower_transport_outgoing_message_for_dst( mesh_network_src(network_pdu));
    if (segmented_pdu == NULL) return;

    uint8_t * lower_transport_pdu     = mesh_network_pdu_data(network_pdu);
    uint16_t seq_zero_pdu = big_endian_read_16(lower_transport_pdu, 1) >> 2;
    uint16_t seq_zero_out = lower_transport_outgoing_message->seq & 0x1fff;
    uint32_t block_ack = big_endian_read_32(lower_transport_pdu, 3);

#ifdef LOG_LOWER_TRANSPORT
    printf("[+] Segment Acknowledgment message with seq_zero %06x, block_ack %08" PRIx32 " - outgoing seq %06x, block_ack %08" PRIx32 "\n",
           seq_zero_pdu, block_ack, seq_zero_out, segmented_pdu->block_ack);
#endif

    if (block_ack == 0){
        // If a Segment Acknowledgment message with the BlockAck field set to 0x00000000 is received,
        // then the Upper Transport PDU shall be immediately cancelled and the higher layers shall be notified that
        // the Upper Transport PDU has been cancelled.
#ifdef LOG_LOWER_TRANSPORT
        printf("[+] Block Ack == 0 => Abort\n");
#endif
        // current?
        if ((lower_transport_outgoing_message == segmented_pdu) && lower_transport_outgoing_segment_at_network_layer){
            lower_transport_outgoing_transmission_aborted = true;
        } else {
            mesh_lower_transport_outgoing_complete(segmented_pdu, MESH_TRANSPORT_STATUS_SEND_ABORT_BY_REMOTE);
        }
        return;
    }
    if (seq_zero_pdu != seq_zero_out){

#ifdef LOG_LOWER_TRANSPORT
        printf("[!] Seq Zero doesn't match\n");
#endif
        return;
    }

    segmented_pdu->block_ack &= ~block_ack;
#ifdef LOG_LOWER_TRANSPORT
    printf("[+] Updated block_ack %08" PRIx32 "\n", segmented_pdu->block_ack);
#endif

    if (segmented_pdu->block_ack == 0){
#ifdef LOG_LOWER_TRANSPORT
        printf("[+] Sent complete\n");
#endif

        if ((lower_transport_outgoing_message == segmented_pdu) && lower_transport_outgoing_segment_at_network_layer){
            lower_transport_outgoing_transmission_complete = true;
        } else {
            mesh_lower_transport_outgoing_complete(segmented_pdu, MESH_TRANSPORT_STATUS_SUCCESS);
        }
    }
}

static void mesh_lower_transport_outgoing_stop_acknowledgment_timer(mesh_segmented_pdu_t *segmented_pdu){
    if ((segmented_pdu->flags & MESH_TRANSPORT_FLAG_ACK_TIMER) == 0) return;
    segmented_pdu->flags &= ~MESH_TRANSPORT_FLAG_ACK_TIMER;
    btstack_run_loop_remove_timer(&segmented_pdu->acknowledgement_timer);
}

static void mesh_lower_transport_outgoing_restart_segment_transmission_timer(mesh_segmented_pdu_t *segmented_pdu){
    // restart segment transmission timer for unicast dst
    // - "This timer shall be set to a minimum of 200 + 50 * TTL milliseconds."
    uint32_t timeout = 200 + 50 * (segmented_pdu->ctl_ttl & 0x7f);
    if ((segmented_pdu->flags & MESH_TRANSPORT_FLAG_ACK_TIMER) != 0){
        btstack_run_loop_remove_timer(&lower_transport_outgoing_message->acknowledgement_timer);
    }

#ifdef LOG_LOWER_TRANSPORT
    printf("[+] Lower transport, segmented pdu %p, seq %06" PRIx32 ": setup transmission timeout %u ms\n", segmented_pdu,
           segmented_pdu->seq, (int) timeout);
#endif

    btstack_run_loop_set_timer(&segmented_pdu->acknowledgement_timer, timeout);
    btstack_run_loop_set_timer_handler(&segmented_pdu->acknowledgement_timer, &mesh_lower_transport_outgoing_segment_transmission_timeout);
    btstack_run_loop_set_timer_context(&segmented_pdu->acknowledgement_timer, lower_transport_outgoing_message);
    btstack_run_loop_add_timer(&segmented_pdu->acknowledgement_timer);
    segmented_pdu->flags |= MESH_TRANSPORT_FLAG_ACK_TIMER;
}

static void mesh_lower_transport_outgoing_complete(mesh_segmented_pdu_t * segmented_pdu, mesh_transport_status_t status){
    btstack_assert(segmented_pdu != NULL);
#ifdef LOG_LOWER_TRANSPORT
    printf("[+] outgoing_complete %p, ack timer active %u, incomplete active %u\n", segmented_pdu,
           ((segmented_pdu->flags & MESH_TRANSPORT_FLAG_ACK_TIMER) != 0), ((segmented_pdu->flags & MESH_TRANSPORT_FLAG_INCOMPLETE_TIMER) != 0));
#endif
    // stop timers
    mesh_lower_transport_outgoing_stop_acknowledgment_timer(segmented_pdu);

    // remove from lists
    if (lower_transport_outgoing_message == segmented_pdu){
        lower_transport_outgoing_message = NULL;
    } else {
        btstack_linked_list_remove(&lower_transport_outgoing_waiting, (btstack_linked_item_t *) segmented_pdu);
        btstack_linked_list_remove(&lower_transport_outgoing_ready, (btstack_linked_item_t *) segmented_pdu);
    }

    // notify upper transport
    higher_layer_handler(MESH_TRANSPORT_PDU_SENT, status, (mesh_pdu_t *) segmented_pdu);
}

static void mesh_lower_transport_outgoing_setup_segment(mesh_segmented_pdu_t *message_pdu, uint8_t seg_o, mesh_network_pdu_t *network_pdu){

    int ctl = message_pdu->ctl_ttl >> 7;
    uint16_t max_segment_len = ctl ? 8 : 12;    // control 8 bytes (64 bit NetMic), access 12 bytes (32 bit NetMIC)

    // use seq number from transport pdu once if MESH_TRANSPORT_FLAG_SEQ_RESERVED (to allow reserving seq number in upper transport while using all seq numbers)
    uint32_t seq;
    if ((message_pdu->flags & MESH_TRANSPORT_FLAG_SEQ_RESERVED) != 0){
        message_pdu->flags &= ~(MESH_TRANSPORT_FLAG_SEQ_RESERVED);
        seq = message_pdu->seq;
    } else {
        seq = mesh_sequence_number_next();
    }
    uint16_t seq_zero = message_pdu->seq & 0x01fff;
    uint8_t  seg_n    = (message_pdu->len - 1) / max_segment_len;
    uint8_t  szmic    = ((message_pdu->flags & MESH_TRANSPORT_FLAG_TRANSMIC_64) != 0) ? 1 : 0;
    uint8_t  nid      = message_pdu->ivi_nid & 0x7f;
    uint8_t  ttl      = message_pdu->ctl_ttl & 0x7f;
    uint16_t src      = message_pdu->src;
    uint16_t dest     = message_pdu->dst;

    // only 1 for access messages with 64 bit TransMIC
    btstack_assert((szmic == 0) || !ctl);

    // current segment.
    uint16_t seg_offset = seg_o * max_segment_len;

    uint8_t lower_transport_pdu_data[16];
    lower_transport_pdu_data[0] = 0x80 | message_pdu->akf_aid_control;
    big_endian_store_24(lower_transport_pdu_data, 1, (szmic << 23) | (seq_zero << 10) | (seg_o << 5) | seg_n);
    uint16_t segment_len = btstack_min(message_pdu->len - seg_offset, max_segment_len);

    uint16_t lower_transport_pdu_len = 4 + segment_len;

    // find network-pdu with chunk for seg_offset
    mesh_network_pdu_t * chunk = (mesh_network_pdu_t *) lower_transport_outgoing_message->segments;
    uint16_t chunk_start = 0;
    while ((chunk_start + MESH_NETWORK_PAYLOAD_MAX) <= seg_offset){
        chunk = (mesh_network_pdu_t *) chunk->pdu_header.item.next;
        chunk_start += MESH_NETWORK_PAYLOAD_MAX;
    }
    // first part
    uint16_t chunk_offset = seg_offset - chunk_start;
    uint16_t bytes_to_copy = btstack_min(MESH_NETWORK_PAYLOAD_MAX - chunk_offset, segment_len);
    (void)memcpy(&lower_transport_pdu_data[4],
                 &chunk->data[chunk_offset], bytes_to_copy);
    segment_len -= bytes_to_copy;
    // second part
    if (segment_len > 0){
        chunk = (mesh_network_pdu_t *) chunk->pdu_header.item.next;
        (void)memcpy(&lower_transport_pdu_data[4+bytes_to_copy],
                     &chunk->data[0], segment_len);
    }

    mesh_network_setup_pdu(network_pdu, message_pdu->netkey_index, nid, 0, ttl, seq, src, dest, lower_transport_pdu_data, lower_transport_pdu_len);
}

static void mesh_lower_transport_outgoing_send_next_segment(void){
    btstack_assert(lower_transport_outgoing_message != NULL);

#ifdef LOG_LOWER_TRANSPORT
    printf("[+] Lower Transport, segmented pdu %p, seq %06" PRIx32 ": send next segment\n", lower_transport_outgoing_message,
           lower_transport_outgoing_message->seq);
#endif

    int ctl = lower_transport_outgoing_message->ctl_ttl >> 7;
    uint16_t max_segment_len = ctl ? 8 : 12;    // control 8 bytes (64 bit NetMic), access 12 bytes (32 bit NetMIC)
    uint8_t  seg_n = (lower_transport_outgoing_message->len - 1) / max_segment_len;

    // find next unacknowledged segment
    while ((lower_transport_outgoing_seg_o <= seg_n) && ((lower_transport_outgoing_message->block_ack & (1 << lower_transport_outgoing_seg_o)) == 0)){
        lower_transport_outgoing_seg_o++;
    }

    if (lower_transport_outgoing_seg_o > seg_n){
#ifdef LOG_LOWER_TRANSPORT
        printf("[+] Lower Transport, segmented pdu %p, seq %06" PRIx32 ": send complete (dst %x)\n", lower_transport_outgoing_message,
               lower_transport_outgoing_message->seq,
               lower_transport_outgoing_message->dst);
#endif
        lower_transport_outgoing_seg_o   = 0;

        // done for unicast, ack timer already set, too
        if (mesh_network_address_unicast(lower_transport_outgoing_message->dst)) {
            btstack_linked_list_add(&lower_transport_outgoing_waiting, (btstack_linked_item_t *) lower_transport_outgoing_message);
            lower_transport_outgoing_message = NULL;
            return;
        }

        // done for group/virtual, no more retries?
        if (lower_transport_outgoing_message->retry_count == 0){
#ifdef LOG_LOWER_TRANSPORT
            printf("[+] Lower Transport, message unacknowledged -> free\n");
#endif
            // notify upper transport
            mesh_lower_transport_outgoing_complete(lower_transport_outgoing_message, MESH_TRANSPORT_STATUS_SUCCESS);
            return;
        }

        // re-queue mssage
#ifdef LOG_LOWER_TRANSPORT
        printf("[+] Lower Transport, message unacknowledged retry count %u\n", lower_transport_outgoing_message->retry_count);
#endif
        lower_transport_outgoing_message->retry_count--;
        btstack_linked_list_add(&lower_transport_outgoing_ready, (btstack_linked_item_t *) lower_transport_outgoing_message);
        lower_transport_outgoing_message = NULL;
        mesh_lower_transport_run();
        return;
    }

    // restart segment transmission timer for unicast dst
    if (mesh_network_address_unicast(lower_transport_outgoing_message->dst)){
        mesh_lower_transport_outgoing_restart_segment_transmission_timer(lower_transport_outgoing_message);
    }

    mesh_lower_transport_outgoing_setup_segment(lower_transport_outgoing_message, lower_transport_outgoing_seg_o,
                                                lower_transport_outgoing_segment);

#ifdef LOG_LOWER_TRANSPORT
    printf("[+] Lower Transport, segmented pdu %p, seq %06" PRIx32 ": send seg_o %x, seg_n %x\n", lower_transport_outgoing_message,
           lower_transport_outgoing_message->seq, lower_transport_outgoing_seg_o, seg_n);
    mesh_print_hex("LowerTransportPDU", &lower_transport_outgoing_segment->data[9], lower_transport_outgoing_segment->len-9);
#endif

    // next segment
    lower_transport_outgoing_seg_o++;

    // send network pdu
    lower_transport_outgoing_segment_at_network_layer = true;
    mesh_network_send_pdu(lower_transport_outgoing_segment);
}

static void mesh_lower_transport_outgoing_setup_sending_segmented_pdus(mesh_segmented_pdu_t *segmented_pdu) {
    printf("[+] Lower Transport, segmented pdu %p, seq %06" PRIx32 ": send retry count %u\n", segmented_pdu, segmented_pdu->seq, segmented_pdu->retry_count);

    segmented_pdu->retry_count--;
    lower_transport_outgoing_seg_o   = 0;
    lower_transport_outgoing_transmission_timeout  = false;
    lower_transport_outgoing_transmission_complete = false;
    lower_transport_outgoing_transmission_aborted  = false;

    lower_transport_outgoing_message = segmented_pdu;
}

static void mesh_lower_transport_outgoing_segment_transmission_fired(mesh_segmented_pdu_t *segmented_pdu) {
    // once more?
    if (segmented_pdu->retry_count == 0){
        printf("[!] Lower transport, segmented pdu %p, seq %06" PRIx32 ": send failed, retries exhausted\n", segmented_pdu,
               segmented_pdu->seq);
        mesh_lower_transport_outgoing_complete(segmented_pdu, MESH_TRANSPORT_STATUS_SEND_FAILED);
        return;
    }

#ifdef LOG_LOWER_TRANSPORT
    printf("[+] Lower transport, segmented pdu %p, seq %06" PRIx32": transmission fired\n", segmented_pdu, segmented_pdu->seq);
#endif

    // re-queue message for sending remaining segments
    if (lower_transport_outgoing_message == segmented_pdu){
        lower_transport_outgoing_message = NULL;
    } else {
        btstack_linked_list_remove(&lower_transport_outgoing_waiting, (btstack_linked_item_t *) segmented_pdu);
    }
    btstack_linked_list_add_tail(&lower_transport_outgoing_ready, (btstack_linked_item_t *) segmented_pdu);

    // continue
    mesh_lower_transport_run();
}

static void mesh_lower_transport_outgoing_segment_transmission_timeout(btstack_timer_source_t * ts){
    mesh_segmented_pdu_t * segmented_pdu = (mesh_segmented_pdu_t *) btstack_run_loop_get_timer_context(ts);
#ifdef LOG_LOWER_TRANSPORT
    printf("[+] Lower transport, segmented pdu %p, seq %06" PRIx32 ": transmission timer fired\n", segmented_pdu,
           segmented_pdu->seq);
#endif
    segmented_pdu->flags &= ~MESH_TRANSPORT_FLAG_ACK_TIMER;

    if ((segmented_pdu == lower_transport_outgoing_message) && lower_transport_outgoing_segment_at_network_layer){
        lower_transport_outgoing_transmission_timeout = true;
    } else {
        mesh_lower_transport_outgoing_segment_transmission_fired(segmented_pdu);
    }
}

// GENERAL //

static void mesh_lower_transport_network_pdu_sent(mesh_network_pdu_t *network_pdu){
    // figure out what pdu was sent

    // Segment Acknowledgment message sent by us?
    if (network_pdu->pdu_header.pdu_type == MESH_PDU_TYPE_SEGMENT_ACKNOWLEDGMENT){
        btstack_memory_mesh_network_pdu_free(network_pdu);
        return;
    }

    // single segment?
    if (lower_transport_outgoing_segment == network_pdu){
        btstack_assert(lower_transport_outgoing_message != NULL);

        // of segmented message
#ifdef LOG_LOWER_TRANSPORT
        printf("[+] Lower transport, segmented pdu %p, seq %06" PRIx32 ": network pdu %p sent\n", lower_transport_outgoing_message,
               lower_transport_outgoing_message->seq, network_pdu);
#endif

        lower_transport_outgoing_segment_at_network_layer = false;
        if (lower_transport_outgoing_transmission_complete){
            // handle success
            lower_transport_outgoing_transmission_complete = false;
            lower_transport_outgoing_transmission_timeout  = false;
            mesh_lower_transport_outgoing_complete(lower_transport_outgoing_message, MESH_TRANSPORT_STATUS_SUCCESS);
            return;
        }
        if (lower_transport_outgoing_transmission_aborted){
            // handle abort
            lower_transport_outgoing_transmission_complete = false;
            lower_transport_outgoing_transmission_timeout  = false;
            mesh_lower_transport_outgoing_complete(lower_transport_outgoing_message, MESH_TRANSPORT_STATUS_SEND_ABORT_BY_REMOTE);
            return;
        }
        if (lower_transport_outgoing_transmission_timeout){
            // handle timeout
            lower_transport_outgoing_transmission_timeout = false;
            mesh_lower_transport_outgoing_segment_transmission_fired(lower_transport_outgoing_message);
            return;
        }

        // send next segment
        mesh_lower_transport_outgoing_send_next_segment();
        return;
    }

    // other
    higher_layer_handler(MESH_TRANSPORT_PDU_SENT, MESH_TRANSPORT_STATUS_SUCCESS, (mesh_pdu_t *) network_pdu);
}

static void mesh_lower_transport_process_unsegmented_control_message(mesh_network_pdu_t *network_pdu){
    uint8_t * lower_transport_pdu     = mesh_network_pdu_data(network_pdu);
    uint8_t  opcode = lower_transport_pdu[0];

#ifdef LOG_LOWER_TRANSPORT
    printf("Unsegmented Control message, outgoing message %p, opcode %x\n", lower_transport_outgoing_message, opcode);
#endif

    switch (opcode){
        case 0:
            mesh_lower_transport_outgoing_process_segment_acknowledgement_message(network_pdu);
            mesh_network_message_processed_by_higher_layer(network_pdu);
            break;
        default:
            mesh_lower_transport_incoming_queue_for_higher_layer((mesh_pdu_t *) network_pdu);
            break;
    }
}

static void mesh_lower_transport_process_network_pdu(mesh_network_pdu_t *network_pdu) {// segmented?
    if (mesh_network_segmented(network_pdu)){
        mesh_segmented_pdu_t * message_pdu = mesh_lower_transport_incoming_pdu_for_segmented_message(network_pdu);
        if (message_pdu) {
            // start acknowledgment timer if inactive
            if ((message_pdu->flags & MESH_TRANSPORT_FLAG_ACK_TIMER) == 0){
                // - "The acknowledgment timer shall be set to a minimum of 150 + 50 * TTL milliseconds"
                uint32_t timeout = 150 + 50 * mesh_network_ttl(network_pdu);
                mesh_lower_transport_incoming_start_acknowledgment_timer(message_pdu, timeout);
            }
            // restart incomplete timer
            mesh_lower_transport_incoming_restart_incomplete_timer(message_pdu, 10000,
                                                                   &mesh_lower_transport_incoming_incomplete_timeout);
            mesh_lower_transport_incoming_process_segment(message_pdu, network_pdu);
        } else {
            mesh_network_message_processed_by_higher_layer(network_pdu);
        }
    } else {
        // control?
        if (mesh_network_control(network_pdu)){
            // unsegmented control message (not encrypted)
            mesh_lower_transport_process_unsegmented_control_message(network_pdu);
        } else {
            // unsegmented access message (encrypted)
            mesh_lower_transport_incoming_queue_for_higher_layer((mesh_pdu_t *) network_pdu);
        }
    }
}

void mesh_lower_transport_received_message(mesh_network_callback_type_t callback_type, mesh_network_pdu_t *network_pdu){
    mesh_peer_t * peer;
    uint16_t src;
    uint16_t seq;
    switch (callback_type){
        case MESH_NETWORK_PDU_RECEIVED:
            src = mesh_network_src(network_pdu);
            seq = mesh_network_seq(network_pdu);
            peer = mesh_peer_for_addr(src);
#ifdef LOG_LOWER_TRANSPORT
            printf("Transport: received message. SRC %x, SEQ %x\n", src, seq);
#endif
            // validate seq
            if (peer && seq > peer->seq){
                // track seq
                peer->seq = seq;
                // process
                mesh_lower_transport_process_network_pdu(network_pdu);
                mesh_lower_transport_run();
            } else {
                // drop packet
#ifdef LOG_LOWER_TRANSPORT
                printf("Transport: drop packet - src/seq auth failed\n");
#endif
                mesh_network_message_processed_by_higher_layer(network_pdu);
            }
            break;
        case MESH_NETWORK_PDU_SENT:
            mesh_lower_transport_network_pdu_sent(network_pdu);
            break;
        default:
            break;
    }
}

static void mesh_lower_transport_run(void){

    // check if outgoing segmented pdu is active
    if (lower_transport_outgoing_message) return;

    while(!btstack_linked_list_empty(&lower_transport_outgoing_ready)) {
        // get next message
        mesh_segmented_pdu_t   * message_pdu;
        mesh_pdu_t * pdu = (mesh_pdu_t *) btstack_linked_list_pop(&lower_transport_outgoing_ready);
        switch (pdu->pdu_type) {
            case MESH_PDU_TYPE_UPPER_UNSEGMENTED_ACCESS:
            case MESH_PDU_TYPE_UPPER_UNSEGMENTED_CONTROL:
                printf("[+] Lower transport, unsegmented pdu, sending now %p\n", pdu);
                lower_transport_outgoing_network_pdu = (mesh_network_pdu_t *) pdu;
                mesh_network_send_pdu(lower_transport_outgoing_network_pdu);
                break;
            case MESH_PDU_TYPE_SEGMENTED:
                message_pdu = (mesh_segmented_pdu_t *) pdu;
                //
                printf("[+] Lower transport, segmented pdu %p, seq %06" PRIx32 ": run start sending now\n", message_pdu,
                       message_pdu->seq);
                // start sending segmented pdu
                mesh_lower_transport_outgoing_setup_sending_segmented_pdus(message_pdu);
                mesh_lower_transport_outgoing_send_next_segment();
                break;
            default:
                btstack_assert(false);
                break;
        }
    }
}

void mesh_lower_transport_send_pdu(mesh_pdu_t *pdu){
    mesh_segmented_pdu_t * segmented_pdu;
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_UPPER_UNSEGMENTED_ACCESS:
        case MESH_PDU_TYPE_UPPER_UNSEGMENTED_CONTROL:
            btstack_assert(((mesh_network_pdu_t *) pdu)->len >= 9);
            break;
        case MESH_PDU_TYPE_SEGMENTED:
            // set num retries, set of segments to send
            segmented_pdu = (mesh_segmented_pdu_t *) pdu;
            segmented_pdu->retry_count = 3;
            mesh_lower_transport_outgoing_setup_block_ack(segmented_pdu);
            break;
        default:
            btstack_assert(false);
            break;
    }
    btstack_linked_list_add_tail(&lower_transport_outgoing_ready, (btstack_linked_item_t*) pdu);
    mesh_lower_transport_run();
}

void mesh_lower_transport_dump_network_pdus(const char *name, btstack_linked_list_t *list){
    printf("List: %s:\n", name);
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, list);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_network_pdu_t * network_pdu = (mesh_network_pdu_t*) btstack_linked_list_iterator_next(&it);
        printf("- %p: ", network_pdu); printf_hexdump(network_pdu->data, network_pdu->len);
    }
}

void mesh_lower_transport_reset_network_pdus(btstack_linked_list_t *list){
    while (!btstack_linked_list_empty(list)){
        mesh_network_pdu_t * pdu = (mesh_network_pdu_t *) btstack_linked_list_pop(list);
        btstack_memory_mesh_network_pdu_free(pdu);
    }
}

bool mesh_lower_transport_can_send_to_dest(uint16_t dest){
    UNUSED(dest);
    // check current
    uint16_t num_messages = 0;
    if (lower_transport_outgoing_message != NULL) {
        if (lower_transport_outgoing_message->dst == dest) {
            return false;
        }
        num_messages++;
    }
    // check waiting
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &lower_transport_outgoing_waiting);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_segmented_pdu_t * segmented_pdu = (mesh_segmented_pdu_t *) btstack_linked_list_iterator_next(&it);
        num_messages++;
        if (segmented_pdu->dst == dest){
            return false;
        }
    }
#ifdef MAX_NR_MESH_OUTGOING_SEGMENTED_MESSAGES
    // limit number of parallel outgoing messages if configured
    if (num_messages >= MAX_NR_MESH_OUTGOING_SEGMENTED_MESSAGES) return false;
#endif
    return true;
}

void mesh_lower_transport_reserve_slot(void){
}

void mesh_lower_transport_reset(void){
    if (lower_transport_outgoing_message){
        while (!btstack_linked_list_empty(&lower_transport_outgoing_message->segments)){
            mesh_network_pdu_t * network_pdu = (mesh_network_pdu_t *) btstack_linked_list_pop(&lower_transport_outgoing_message->segments);
            mesh_network_pdu_free(network_pdu);
        }
        lower_transport_outgoing_message = NULL;
    }
    while (!btstack_linked_list_empty(&lower_transport_outgoing_waiting)){
        mesh_segmented_pdu_t * segmented_pdu = (mesh_segmented_pdu_t *) btstack_linked_list_pop(&lower_transport_outgoing_waiting);
        btstack_memory_mesh_segmented_pdu_free(segmented_pdu);
    }
    mesh_network_pdu_free(lower_transport_outgoing_segment);
    lower_transport_outgoing_segment_at_network_layer = false;
    lower_transport_outgoing_segment = NULL;
}

void mesh_lower_transport_init(){
    // register with network layer
    mesh_network_set_higher_layer_handler(&mesh_lower_transport_received_message);
    // allocate network_pdu for segmentation
    lower_transport_outgoing_segment_at_network_layer = false;
    lower_transport_outgoing_segment = mesh_network_pdu_get();
}

void mesh_lower_transport_set_higher_layer_handler(void (*pdu_handler)( mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu)){
    higher_layer_handler = pdu_handler;
}
