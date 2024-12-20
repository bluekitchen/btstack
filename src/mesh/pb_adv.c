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

#define BTSTACK_FILE__ "pb_adv.c"

#include "pb_adv.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_util.h"

#include "mesh/adv_bearer.h"
#include "mesh/beacon.h"
#include "mesh/mesh_node.h"
#include "mesh/provisioning.h"

#define PB_ADV_LINK_OPEN_RETRANSMIT_MS 1000
#define PB_ADV_LINK_OPEN_TIMEOUT_MS   60000
#define PB_ADV_LINK_OPEN_RETRIES (PB_ADV_LINK_OPEN_TIMEOUT_MS / PB_ADV_LINK_OPEN_RETRANSMIT_MS)
static void pb_adv_run(void);

/* taps: 32 31 29 1; characteristic polynomial: x^32 + x^31 + x^29 + x + 1 */
#define LFSR(a) ((a >> 1) ^ (uint32_t)((0 - (a & 1u)) & 0xd0000001u))

// PB-ADV - Provisioning Bearer using Advertisement Bearer

#define MESH_GENERIC_PROVISIONING_LINK_OPEN              0x00
#define MESH_GENERIC_PROVISIONING_LINK_ACK               0x01
#define MESH_GENERIC_PROVISIONING_LINK_CLOSE             0x02

#define MESH_GENERIC_PROVISIONING_TRANSACTION_TIMEOUT_MS 30000

// TODO: how large are prov messages? => how many segments are required?
#define MESH_PB_ADV_MAX_SEGMENTS    8
#define MESH_PB_ADV_START_PAYLOAD  20
#define MESH_PB_ADV_CONT_PAYLOAD   23
#define MESH_PB_ADV_MAX_PDU_SIZE  (MESH_PB_ADV_START_PAYLOAD + (MESH_PB_ADV_MAX_SEGMENTS-2) * MESH_PB_ADV_CONT_PAYLOAD)


typedef enum mesh_gpcf_format {
    MESH_GPCF_TRANSACTION_START = 0,
    MESH_GPCF_TRANSACTION_ACK,
    MESH_GPCF_TRANSACTION_CONT,
    MESH_GPCF_PROV_BEARER_CONTROL,
} mesh_gpcf_format_t;

typedef enum {
    LINK_STATE_W4_OPEN,
    LINK_STATE_W2_SEND_ACK,
    LINK_STATE_W4_ACK,
    LINK_STATE_OPEN,
    LINK_STATE_CLOSING,
} link_state_t;
static link_state_t link_state;

#ifdef ENABLE_MESH_PROVISIONER
static const uint8_t * pb_adv_peer_device_uuid;
static uint8_t pb_adv_provisioner_open_countdown;
#endif

static uint8_t  pb_adv_msg_in_buffer[MESH_PB_ADV_MAX_PDU_SIZE];

// single adv link, roles: provisioner = 1, device = 0
static uint16_t pb_adv_cid = 1;
static uint8_t  pb_adv_provisioner_role;

// link state
static uint32_t pb_adv_link_id;
static uint8_t  pb_adv_link_close_reason;
static uint8_t  pb_adv_link_close_countdown;
static bool     pb_adv_link_establish_timer_active;

// random delay for outgoing packets
static uint32_t pb_adv_lfsr;
static uint8_t  pb_adv_random_delay_active;

// adv link timer used for
// establishment:
// - device: 60s timeout after receiving link open and sending link ack until first provisioning PDU
// - provisioner: 1s timer to send link open messages
// open: random delay
static btstack_timer_source_t pb_adv_link_timer;

// incoming message
static uint8_t  pb_adv_msg_in_transaction_nr_prev;
static uint16_t pb_adv_msg_in_len;   // 
static uint8_t  pb_adv_msg_in_fcs;
static uint8_t  pb_adv_msg_in_last_segment; 
static uint8_t  pb_adv_msg_in_segments_missing; // bitfield for segmentes 1-n
static uint8_t  pb_adv_msg_in_transaction_nr;
static uint8_t  pb_adv_msg_in_send_ack;

// outgoing message
static uint8_t         pb_adv_msg_out_active;
static uint8_t         pb_adv_msg_out_transaction_nr;
static uint8_t         pb_adv_msg_out_completed_transaction_nr;
static uint16_t        pb_adv_msg_out_len;
static uint16_t        pb_adv_msg_out_pos;
static uint8_t         pb_adv_msg_out_seg;
static uint32_t        pb_adv_msg_out_start;
static const uint8_t * pb_adv_msg_out_buffer;

static btstack_packet_handler_t pb_adv_device_packet_handler;
static btstack_packet_handler_t pb_adv_provisioner_packet_handler;

// poor man's random number generator
static uint32_t pb_adv_random(void){
    pb_adv_lfsr = LFSR(pb_adv_lfsr);
    return pb_adv_lfsr;
}

static void pb_adv_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size){
    if (pb_adv_provisioner_role == 0){
        (*pb_adv_device_packet_handler)(packet_type, channel, packet, size);
    } else {
        (*pb_adv_provisioner_packet_handler)(packet_type, channel, packet, size);
    }
}

static void pb_adv_emit_pdu_sent(uint8_t status){
    uint8_t event[] = { HCI_EVENT_MESH_META, 2, MESH_SUBEVENT_PB_TRANSPORT_PDU_SENT, status};
    pb_adv_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void pb_adv_emit_link_open(uint8_t status, uint16_t pb_transport_cid){
    uint8_t event[7] = { HCI_EVENT_MESH_META, 5, MESH_SUBEVENT_PB_TRANSPORT_LINK_OPEN, status};
    little_endian_store_16(event, 4, pb_transport_cid);
    event[6] = MESH_PB_TYPE_ADV;
    pb_adv_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void pb_adv_emit_link_close(uint16_t pb_transport_cid, uint8_t reason){
    uint8_t event[6] = { HCI_EVENT_MESH_META, 3, MESH_SUBEVENT_PB_TRANSPORT_LINK_CLOSED};
    little_endian_store_16(event, 3, pb_transport_cid);
    event[5] = reason;
    pb_adv_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void pb_adv_device_link_timeout(btstack_timer_source_t * ts){
    UNUSED(ts);
    // timeout occured
    link_state = LINK_STATE_W4_OPEN;
    log_info("link timeout, %08" PRIx32, pb_adv_link_id);
    printf("PB-ADV: Link timeout %08" PRIx32 "\n", pb_adv_link_id);
    pb_adv_emit_link_close(pb_adv_cid, ERROR_CODE_PAGE_TIMEOUT);
}

static void pb_adv_handle_bearer_control(uint32_t link_id, uint8_t transaction_nr, const uint8_t * pdu, uint16_t size){
    UNUSED(transaction_nr);
    UNUSED(size);

    uint8_t bearer_opcode = pdu[0] >> 2;
    uint8_t reason;
    const uint8_t * own_device_uuid;
    switch (bearer_opcode){
        case MESH_GENERIC_PROVISIONING_LINK_OPEN: // Open a session on a bearer with a device
            // does it match our device_uuid?
            own_device_uuid = mesh_node_get_device_uuid();
            if (!own_device_uuid) break;
            if (memcmp(&pdu[1], own_device_uuid, 16) != 0) break;
            btstack_run_loop_remove_timer(&pb_adv_link_timer);
            btstack_run_loop_set_timer(&pb_adv_link_timer, PB_ADV_LINK_OPEN_TIMEOUT_MS);
            btstack_run_loop_set_timer_handler(&pb_adv_link_timer, &pb_adv_device_link_timeout);
            btstack_run_loop_add_timer(&pb_adv_link_timer);
            pb_adv_link_establish_timer_active = true;
            switch(link_state){
                case LINK_STATE_W4_OPEN:
                    pb_adv_link_id = link_id;
                    pb_adv_provisioner_role = 0;
                    pb_adv_msg_in_transaction_nr = 0xff;  // first transaction nr will be 0x00 
                    pb_adv_msg_in_transaction_nr_prev = 0xff;
                    log_info("link open, id %08" PRIx32, pb_adv_link_id);
                    printf("PB-ADV: Link Open %08" PRIx32 "\n", pb_adv_link_id);
                    link_state = LINK_STATE_W2_SEND_ACK;
                    adv_bearer_request_can_send_now_for_provisioning_pdu();
                    pb_adv_emit_link_open(ERROR_CODE_SUCCESS, pb_adv_cid);
                    break;
                case LINK_STATE_OPEN:
                    if (pb_adv_link_id != link_id) break;
                    log_info("link open, resend ACK");
                    link_state = LINK_STATE_W2_SEND_ACK;
                    adv_bearer_request_can_send_now_for_provisioning_pdu();
                    break;
                default:
                    break;
            }
            break;
#ifdef ENABLE_MESH_PROVISIONER
        case MESH_GENERIC_PROVISIONING_LINK_ACK:   // Acknowledge a session on a bearer
            if (link_state != LINK_STATE_W4_ACK) break;
            link_state = LINK_STATE_OPEN;
            pb_adv_msg_out_transaction_nr = 0;
            pb_adv_msg_in_transaction_nr = 0x7f;    // first transaction nr will be 0x80
            pb_adv_msg_in_transaction_nr_prev = 0x7f;
            btstack_run_loop_remove_timer(&pb_adv_link_timer);
            log_info("link open, id %08" PRIx32, pb_adv_link_id);
            printf("PB-ADV: Link Open %08" PRIx32 "\n", pb_adv_link_id);
            pb_adv_emit_link_open(ERROR_CODE_SUCCESS, pb_adv_cid);
            break;
#endif
        case MESH_GENERIC_PROVISIONING_LINK_CLOSE: // Close a session on a bearer
            // does it match link id
            if (link_id != pb_adv_link_id) break;
            if (link_state == LINK_STATE_W4_OPEN) break;
            btstack_run_loop_remove_timer(&pb_adv_link_timer);
            reason = pdu[1];
            link_state = LINK_STATE_W4_OPEN;
            log_info("link close, reason %x", reason);
            pb_adv_emit_link_close(pb_adv_cid, reason);
            break;
        default:
            log_info("BearerOpcode %x reserved for future use\n", bearer_opcode);
            break;
    }
}

static void pb_adv_pdu_complete(void){

    // Verify FCS
    uint8_t pdu_crc = btstack_crc8_calc((uint8_t*)pb_adv_msg_in_buffer, pb_adv_msg_in_len);
    if (pdu_crc != pb_adv_msg_in_fcs){
        printf("Incoming PDU: fcs %02x, calculated %02x -> drop packet\n", pb_adv_msg_in_fcs, btstack_crc8_calc(pb_adv_msg_in_buffer, pb_adv_msg_in_len));
        return;
    }

    printf("PB-ADV: %02x complete\n", pb_adv_msg_in_transaction_nr);

    // transaction complete
    pb_adv_msg_in_transaction_nr_prev = pb_adv_msg_in_transaction_nr;
    if (pb_adv_provisioner_role){
        pb_adv_msg_in_transaction_nr = 0x7f;    // invalid
    } else {
        pb_adv_msg_in_transaction_nr = 0xff;    // invalid
    }

    // Ack Transaction
    pb_adv_msg_in_send_ack = 1;
    pb_adv_run();

    // Forward to Provisioning
    pb_adv_packet_handler(PROVISIONING_DATA_PACKET, 0, pb_adv_msg_in_buffer, pb_adv_msg_in_len);
}

static void pb_adv_handle_transaction_start(uint8_t transaction_nr, const uint8_t * pdu, uint16_t size){

    // resend ack if packet from previous transaction received
    if (transaction_nr != 0xff && transaction_nr == pb_adv_msg_in_transaction_nr_prev){
        printf("PB_ADV: %02x transaction complete, resending ack \n", transaction_nr);
        pb_adv_msg_in_send_ack = 1;
        return;
    }

    // new transaction?
    if (transaction_nr != pb_adv_msg_in_transaction_nr){

        // check len
        if (size < 4) return;

        // check len
        uint16_t msg_len = big_endian_read_16(pdu, 1);
        if (msg_len > MESH_PB_ADV_MAX_PDU_SIZE){
            // abort transaction
            return;
        }

        // check num segments
        uint8_t last_segment = pdu[0] >> 2;
        if (last_segment >= MESH_PB_ADV_MAX_SEGMENTS){
            // abort transaction
            return;
        }

        printf("PB-ADV: %02x started\n", transaction_nr);

        pb_adv_msg_in_transaction_nr = transaction_nr;
        pb_adv_msg_in_len            = msg_len;
        pb_adv_msg_in_fcs            = pdu[3];
        pb_adv_msg_in_last_segment   = last_segment;

        // set bits for  segments 1..n (segment 0 already received in this message)
        pb_adv_msg_in_segments_missing = (1 << last_segment) - 1;

        // store payload
        uint16_t payload_len = size - 4;
        (void)memcpy(pb_adv_msg_in_buffer, &pdu[4], payload_len);

        // complete?
        if (pb_adv_msg_in_segments_missing == 0){
            pb_adv_pdu_complete();
        }
    }
}

static void pb_adv_handle_transaction_cont(uint8_t transaction_nr, const uint8_t * pdu, uint16_t size){

    // check transaction nr
    if (transaction_nr != 0xff && transaction_nr == pb_adv_msg_in_transaction_nr_prev){
        printf("PB_ADV: %02x transaction complete, resending resending ack\n", transaction_nr);
        pb_adv_msg_in_send_ack = 1;
        return;
    }

    if (transaction_nr != pb_adv_msg_in_transaction_nr){
        printf("PB-ADV: %02x received msg for transaction nr %x\n", pb_adv_msg_in_transaction_nr, transaction_nr);
        return;
    }

    // validate seg nr
    uint8_t seg = pdu[0] >> 2;
    if (seg >= MESH_PB_ADV_MAX_SEGMENTS || seg == 0){
        return;
    }

    // check if segment already received
    uint8_t seg_mask = 1 << (seg-1);
    if ((pb_adv_msg_in_segments_missing & seg_mask) == 0){
        printf("PB-ADV: %02x, segment %u already received\n", transaction_nr, seg);
        return;
    }
    printf("PB-ADV: %02x, segment %u stored\n", transaction_nr, seg);

    // calculate offset and fragment size
    uint16_t msg_pos = MESH_PB_ADV_START_PAYLOAD + (seg-1) * MESH_PB_ADV_CONT_PAYLOAD;
    uint16_t fragment_size = size - 1;

    // check size if last segment
    if (seg == pb_adv_msg_in_last_segment && (msg_pos + fragment_size) != pb_adv_msg_in_len){
        // last segment has invalid size
        return;
    }

    // store segment and mark as received
    (void)memcpy(&pb_adv_msg_in_buffer[msg_pos], &pdu[1], fragment_size);
    pb_adv_msg_in_segments_missing &= ~seg_mask;

     // last segment
     if (pb_adv_msg_in_segments_missing == 0){
        pb_adv_pdu_complete();
    }
}

static void pb_adv_outgoing_transaction_complete(uint8_t status){
    // stop sending
    pb_adv_msg_out_active = 0;
    // emit done
    pb_adv_emit_pdu_sent(status);
    // keep track of ack'ed transactions
    pb_adv_msg_out_completed_transaction_nr = pb_adv_msg_out_transaction_nr;
    // increment outgoing transaction nr
    pb_adv_msg_out_transaction_nr++;
    if (pb_adv_msg_out_transaction_nr == 0x00){
        // Device role
        pb_adv_msg_out_transaction_nr = 0x80;
    }
    if (pb_adv_msg_out_transaction_nr == 0x80){
        // Provisioner role
        pb_adv_msg_out_transaction_nr = 0x00;
    }
}

static void pb_adv_handle_transaction_ack(uint8_t transaction_nr, const uint8_t * pdu, uint16_t size){
    UNUSED(pdu);    
    UNUSED(size);
    if (transaction_nr == pb_adv_msg_out_transaction_nr){
        printf("PB-ADV: %02x ACK received\n", transaction_nr);
        pb_adv_outgoing_transaction_complete(ERROR_CODE_SUCCESS);
    } else if (transaction_nr == pb_adv_msg_out_completed_transaction_nr){
        // Transaction ack received again
    } else {
        printf("PB-ADV: %02x unexpected Transaction ACK %x recevied\n", pb_adv_msg_out_transaction_nr, transaction_nr);
    }
}

static int pb_adv_packet_to_send(void){
    return pb_adv_msg_in_send_ack || pb_adv_msg_out_active || (link_state == LINK_STATE_W4_ACK);
}

static void pb_adv_timer_handler(btstack_timer_source_t * ts){
    UNUSED(ts);
    pb_adv_random_delay_active = 0;
    if (!pb_adv_packet_to_send()) return;
    adv_bearer_request_can_send_now_for_provisioning_pdu();  
}

static void pb_adv_run(void){
    if (!pb_adv_packet_to_send()) return;
    if (pb_adv_random_delay_active) return;

    // spec recommends 20-50 ms, we use 20-51 ms
    pb_adv_random_delay_active = 1;
    uint16_t random_delay_ms = 20 + (pb_adv_random() & 0x1f);
    log_info("random delay %u ms", random_delay_ms);
    btstack_run_loop_set_timer_handler(&pb_adv_link_timer, &pb_adv_timer_handler);
    btstack_run_loop_set_timer(&pb_adv_link_timer, random_delay_ms);
    btstack_run_loop_add_timer(&pb_adv_link_timer);
}

static void pb_adv_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (size < 3) return;

    const uint8_t * data;
    uint8_t  length;
    uint32_t link_id;
    uint8_t  transaction_nr;
    uint8_t  generic_provisioning_control;
    switch(packet[0]){
        case GAP_EVENT_ADVERTISING_REPORT:
            // check minimal size
            if (size < (12 + 8)) return;

            // data starts at offset 12
            data = &packet[12];
            // PDB ADV PDU
            length = data[0];

            // validate length field
            if ((12 + length) > size) return;

            link_id = big_endian_read_32(data, 2);
            transaction_nr = data[6];
            // generic provision PDU
            generic_provisioning_control = data[7];
            mesh_gpcf_format_t generic_provisioning_control_format = (mesh_gpcf_format_t) generic_provisioning_control & 3;

            // unless, we're waiting for LINK_OPEN, check link_id
            if (link_state != LINK_STATE_W4_OPEN){
                if (link_id != pb_adv_link_id) break;
            }

            if (generic_provisioning_control_format == MESH_GPCF_PROV_BEARER_CONTROL){
                pb_adv_handle_bearer_control(link_id, transaction_nr, &data[7], length-6);
                break;
            }

            // verify link id and link state
            if (link_state != LINK_STATE_OPEN) break;

            // stop link establishment timer
            if (pb_adv_link_establish_timer_active) {
                pb_adv_link_establish_timer_active = false;
                btstack_run_loop_remove_timer(&pb_adv_link_timer);
            }

            switch (generic_provisioning_control_format){
                case MESH_GPCF_TRANSACTION_START:
                    pb_adv_handle_transaction_start(transaction_nr, &data[7], length-6);
                    break;
                case MESH_GPCF_TRANSACTION_CONT:
                    pb_adv_handle_transaction_cont(transaction_nr, &data[7], length-6);
                    break;
                case MESH_GPCF_TRANSACTION_ACK:
                    pb_adv_handle_transaction_ack(transaction_nr, &data[7], length-6);
                    break;
                default:
                    break;
            }
            pb_adv_run();
            break;
        case HCI_EVENT_MESH_META:
            switch(packet[2]){
                case MESH_SUBEVENT_CAN_SEND_NOW:
#ifdef ENABLE_MESH_PROVISIONER
                    if (link_state == LINK_STATE_W4_ACK){
                        pb_adv_provisioner_open_countdown--;
                        if (pb_adv_provisioner_open_countdown == 0){
                            pb_adv_emit_link_open(ERROR_CODE_PAGE_TIMEOUT, pb_adv_cid);
                            break;
                        }
                        // build packet
                        uint8_t buffer[22];
                        big_endian_store_32(buffer, 0, pb_adv_link_id);
                        buffer[4] = 0;            // Transaction ID = 0
                        buffer[5] = (0 << 2) | 3; // Link Open | Provisioning Bearer Control
                        (void)memcpy(&buffer[6], pb_adv_peer_device_uuid, 16);
                        adv_bearer_send_provisioning_pdu(buffer, sizeof(buffer));
                        log_info("link open %08" PRIx32, pb_adv_link_id);
                        printf("PB-ADV: Sending Link Open for device uuid: ");
                        printf_hexdump(pb_adv_peer_device_uuid, 16);
                        btstack_run_loop_set_timer_handler(&pb_adv_link_timer, &pb_adv_timer_handler);
                        btstack_run_loop_set_timer(&pb_adv_link_timer, PB_ADV_LINK_OPEN_RETRANSMIT_MS);
                        btstack_run_loop_add_timer(&pb_adv_link_timer);
                        break;
                    }
#endif
                    if (link_state == LINK_STATE_CLOSING){
                        log_info("link close %08" PRIx32, pb_adv_link_id);
                        printf("PB-ADV: Sending Link Close %08" PRIx32 "\n", pb_adv_link_id);
                        // build packet
                        uint8_t buffer[7];
                        big_endian_store_32(buffer, 0, pb_adv_link_id);
                        buffer[4] = 0;            // Transaction ID = 0
                        buffer[5] = (2 << 2) | 3; // Link Close | Provisioning Bearer Control
                        buffer[6] = pb_adv_link_close_reason;
                        adv_bearer_send_provisioning_pdu(buffer, sizeof(buffer));
                        pb_adv_link_close_countdown--;
                        if (pb_adv_link_close_countdown) {
                            adv_bearer_request_can_send_now_for_provisioning_pdu();  
                        } else {
                            link_state = LINK_STATE_W4_OPEN;
                        }
                        break;                        
                    }
                    if (link_state == LINK_STATE_W2_SEND_ACK){
                        link_state = LINK_STATE_OPEN;
                        pb_adv_msg_out_transaction_nr = 0x80;
                        // build packet
                        uint8_t buffer[6];
                        big_endian_store_32(buffer, 0, pb_adv_link_id);
                        buffer[4] = 0;
                        buffer[5] = (1 << 2) | 3; // Link Ack | Provisioning Bearer Control
                        adv_bearer_send_provisioning_pdu(buffer, sizeof(buffer));
                        log_info("link ack %08" PRIx32, pb_adv_link_id);
                        printf("PB-ADV: Sending Link Open Ack %08" PRIx32 "\n", pb_adv_link_id);
                        break;
                    }
                    if (pb_adv_msg_in_send_ack){
                        pb_adv_msg_in_send_ack = 0;
                        uint8_t buffer[6];
                        big_endian_store_32(buffer, 0, pb_adv_link_id);
                        buffer[4] = pb_adv_msg_in_transaction_nr_prev;
                        buffer[5] = MESH_GPCF_TRANSACTION_ACK;
                        adv_bearer_send_provisioning_pdu(buffer, sizeof(buffer));
                        log_info("transaction ack %08" PRIx32, pb_adv_link_id);
                        printf("PB-ADV: %02x sending ACK\n", pb_adv_msg_in_transaction_nr_prev);
                        pb_adv_run();
                        break;
                    }
                    if (pb_adv_msg_out_active){

                        // check timeout for outgoing message
                        // since uint32_t is used and time now must be greater than pb_adv_msg_out_start,
                        // this claculation is correct even when the run loop time overruns
                        uint32_t transaction_time_ms = btstack_run_loop_get_time_ms() - pb_adv_msg_out_start;
                        if (transaction_time_ms >= MESH_GENERIC_PROVISIONING_TRANSACTION_TIMEOUT_MS){
                            pb_adv_outgoing_transaction_complete(ERROR_CODE_CONNECTION_TIMEOUT);
                            return;
                        }

                        uint8_t buffer[29]; // ADV MTU
                        big_endian_store_32(buffer, 0, pb_adv_link_id);
                        buffer[4] = pb_adv_msg_out_transaction_nr;
                        uint16_t bytes_left;
                        uint16_t pos;
                        if (pb_adv_msg_out_pos == 0){
                            // Transaction start
                            int seg_n = pb_adv_msg_out_len / 24;
                            pb_adv_msg_out_seg = 0;
                            buffer[5] = seg_n << 2 | MESH_GPCF_TRANSACTION_START;
                            big_endian_store_16(buffer, 6, pb_adv_msg_out_len);
                            buffer[8] = btstack_crc8_calc((uint8_t*)pb_adv_msg_out_buffer, pb_adv_msg_out_len);
                            pos = 9;
                            bytes_left = 24 - 4;
                            printf("PB-ADV: %02x Sending Start: ", pb_adv_msg_out_transaction_nr);
                        } else {
                            // Transaction continue
                            buffer[5] = pb_adv_msg_out_seg << 2 | MESH_GPCF_TRANSACTION_CONT;
                            pos = 6;
                            bytes_left = 24 - 1;
                            printf("PB-ADV: %02x Sending Cont:  ", pb_adv_msg_out_transaction_nr);
                        }
                        pb_adv_msg_out_seg++;
                        uint16_t bytes_to_copy = btstack_min(bytes_left, pb_adv_msg_out_len - pb_adv_msg_out_pos);
                        (void)memcpy(&buffer[pos],
                                     &pb_adv_msg_out_buffer[pb_adv_msg_out_pos],
                                     bytes_to_copy);
                        pos += bytes_to_copy;
                        printf("bytes %02u, pos %02u, len %02u: ", bytes_to_copy, pb_adv_msg_out_pos, pb_adv_msg_out_len);
                        printf_hexdump(buffer, pos);
                        pb_adv_msg_out_pos += bytes_to_copy;

                        if (pb_adv_msg_out_pos == pb_adv_msg_out_len){
                            // done
                            pb_adv_msg_out_pos = 0;
                        }
                        adv_bearer_send_provisioning_pdu(buffer, pos);
                        pb_adv_run();
                        break;
                    }
                    break;
                default:
                    break;
            }
        default:
            break;
    }
}

void pb_adv_init(void){
    adv_bearer_register_for_provisioning_pdu(&pb_adv_handler);
    pb_adv_lfsr = 0x12345678;
    pb_adv_random();
}

void pb_adv_register_device_packet_handler(btstack_packet_handler_t packet_handler){
    pb_adv_device_packet_handler = packet_handler;
}

void pb_adv_register_provisioner_packet_handler(btstack_packet_handler_t packet_handler){
    pb_adv_provisioner_packet_handler = packet_handler;
}

void pb_adv_send_pdu(uint16_t pb_transport_cid, const uint8_t * pdu, uint16_t size){
    UNUSED(pb_transport_cid);
    printf("PB-ADV: Send packet ");
    printf_hexdump(pdu, size);
    pb_adv_msg_out_buffer = pdu;
    pb_adv_msg_out_len    = size;
    pb_adv_msg_out_pos = 0;
    pb_adv_msg_out_start = btstack_run_loop_get_time_ms();
    pb_adv_msg_out_active = 1;
    pb_adv_run();
}

/**
 * Close Link
 * @param pb_transport_cid
 */
void pb_adv_close_link(uint16_t pb_transport_cid, uint8_t reason){
    switch (link_state){
        case LINK_STATE_W4_ACK:
        case LINK_STATE_OPEN:
        case LINK_STATE_W2_SEND_ACK:
            pb_adv_emit_link_close(pb_transport_cid, 0);
            link_state = LINK_STATE_CLOSING;
            pb_adv_link_close_countdown = 3;
            pb_adv_link_close_reason = reason;
            adv_bearer_request_can_send_now_for_provisioning_pdu();
            break;
        case LINK_STATE_W4_OPEN:
        case LINK_STATE_CLOSING:
            // nothing to do
            break;
        default:
            btstack_assert(false);
            break;
    }
}

#ifdef ENABLE_MESH_PROVISIONER
uint16_t pb_adv_create_link(const uint8_t * device_uuid){
    if (link_state != LINK_STATE_W4_OPEN) return 0;
    
    pb_adv_peer_device_uuid = device_uuid;
    pb_adv_provisioner_role = 1;
    pb_adv_provisioner_open_countdown = PB_ADV_LINK_OPEN_RETRIES;

    // create new 32-bit link id
    pb_adv_link_id = pb_adv_random();

    // after sending OPEN, we wait for an ACK
    link_state = LINK_STATE_W4_ACK;

    // request outgoing
    adv_bearer_request_can_send_now_for_provisioning_pdu();

    // dummy pb_adv_cid
    return pb_adv_cid;
}
#endif

