/*
 * Copyright (C) 2014 by Ole Reinhardt <ole.reinhardt@kernelconcepts.de>
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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
 *  bnep.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h> // memcpy
#include <stdint.h>

#include <btstack/btstack.h>
#include <btstack/hci_cmds.h>
#include <btstack/utils.h>

#include <btstack/utils.h>
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"
#include "debug.h"
#include "bnep.h"

#include "l2cap.h"

static linked_list_t bnep_services = NULL;
static linked_list_t bnep_channels = NULL;

static gap_security_level_t bnep_security_level;

static void (*app_packet_handler)(void * connection, uint8_t packet_type,
                                  uint16_t channel, uint8_t *packet, uint16_t size);


static void bnep_run(void);

/* Emit service registered event */
static void bnep_emit_service_registered(void *connection, uint8_t status, uint16_t service_uuid)
{
    log_info("BNEP_EVENT_SERVICE_REGISTERED status 0x%02x, uuid: 0x%04x", status, service_uuid);
    uint8_t event[5];
    event[0] = BNEP_EVENT_SERVICE_REGISTERED;
    event[1] = sizeof(event) - 2;
    event[2] = status;
    bt_store_16(event, 3, service_uuid);
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
	(*app_packet_handler)(connection, HCI_EVENT_PACKET, 0, (uint8_t *) event, sizeof(event));
}

static void bnep_emit_channel_opened(bnep_channel_t *channel, uint8_t status) 
{
    log_info("BNEP_EVENT_OPEN_CHANNEL_COMPLETE status 0x%02x bd_addr: %s", status, bd_addr_to_str(channel->remote_addr));
    uint8_t event[3 + sizeof(bd_addr_t) + 2 * sizeof(uint16_t)];
    event[0] = BNEP_EVENT_CHANNEL_OPENED;
    event[1] = sizeof(event) - 2;
    event[2] = status;
    bt_store_16(event, 3, channel->uuid_source);
    bt_store_16(event, 5, channel->uuid_dest);
    memcpy(&event[7], channel->remote_addr, sizeof(bd_addr_t));
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
	(*app_packet_handler)(channel->connection, HCI_EVENT_PACKET, 0, (uint8_t *) event, sizeof(event));
}

static void bnep_emit_incomming_connection(bnep_channel_t *channel) 
{
    log_info("BNEP_EVENT_INCOMMING_CONNECTION bd_addr: %s", bd_addr_to_str(channel->remote_addr));
    uint8_t event[2 + sizeof(bd_addr_t) + 2 * sizeof(uint16_t)];
    event[0] = BNEP_EVENT_INCOMMING_CONNECTION;
    event[1] = sizeof(event) - 2;
    bt_store_16(event, 3, channel->uuid_source);
    bt_store_16(event, 5, channel->uuid_dest);
    memcpy(&event[7], channel->remote_addr, sizeof(bd_addr_t));
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
	(*app_packet_handler)(channel->connection, HCI_EVENT_PACKET, 0, (uint8_t *) event, sizeof(event));
}

static void bnep_emit_channel_closed(bnep_channel_t *channel) 
{
    log_info("BNEP_EVENT_CHANNEL_CLOSED bd_addr: %s", bd_addr_to_str(channel->remote_addr));
    uint8_t event[2 + sizeof(bd_addr_t) + 2 * sizeof(uint16_t)];
    event[0] = BNEP_EVENT_CHANNEL_CLOSED;
    event[1] = sizeof(event) - 2;
    bt_store_16(event, 2, channel->uuid_source);
    bt_store_16(event, 4, channel->uuid_dest);
    memcpy(&event[6], channel->remote_addr, sizeof(bd_addr_t));
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
	(*app_packet_handler)(channel->connection, HCI_EVENT_PACKET, 0, (uint8_t *) event, sizeof(event));
}

/* Send BNEP connection request */
static int bnep_send_command_not_understood(bnep_channel_t *channel, uint8_t control_type)
{
    uint8_t *bnep_out_buffer = NULL;
    uint16_t pos = 0;
    int      err = 0; 
    
    if (channel->state == BNEP_CHANNEL_STATE_CLOSED) {
        return -1; // TODO
    }
       
    l2cap_reserve_packet_buffer();
    bnep_out_buffer = l2cap_get_outgoing_buffer();

    /* Setup control packet type */
	bnep_out_buffer[pos++] = BNEP_PKT_TYPE_CONTROL;
	bnep_out_buffer[pos++] = BNEP_CONTROL_TYPE_COMMAND_NOT_UNDERSTOOD;

    /* Add not understood control type */
    bnep_out_buffer[pos++] = control_type;    

    err = l2cap_send_prepared(channel->l2cap_cid, pos);
    
    if (err) {
        // TODO: Log error 
    }
    return err;
}


/* Send BNEP connection request */
static int bnep_send_connection_request(bnep_channel_t *channel, uint16_t uuid_source, uint16_t uuid_dest)
{
    uint8_t *bnep_out_buffer = NULL;
    uint16_t pos = 0;
    int      err = 0; 
    
    if (channel->state == BNEP_CHANNEL_STATE_CLOSED) {
        return -1; // TODO
    }
    
    l2cap_reserve_packet_buffer();
    bnep_out_buffer = l2cap_get_outgoing_buffer();

    /* Setup control packet type */
	bnep_out_buffer[pos++] = BNEP_PKT_TYPE_CONTROL;
	bnep_out_buffer[pos++] = BNEP_CONTROL_TYPE_SETUP_CONNECTION_REQUEST;

    /* Add UUID Size */
    bnep_out_buffer[pos++] = 2;

    /* Add dest and source UUID */
    bnep_out_buffer[pos++] = (uuid_dest >> 8) & 0xFF;    
    bnep_out_buffer[pos++] = uuid_dest & 0xFF;
    
    bnep_out_buffer[pos++] = (uuid_source >> 8) & 0xFF;    
    bnep_out_buffer[pos++] = uuid_source & 0xFF;

    err = l2cap_send_prepared(channel->l2cap_cid, pos);
    
    if (err) {
        // TODO: Log error 
    }
    return err;
}

/* Send BNEP connection response */
static int bnep_send_connection_response(bnep_channel_t *channel, uint16_t response_code)
{
    uint8_t *bnep_out_buffer = NULL;
    uint16_t pos = 0;
    int      err = 0; 
    
    if (channel->state == BNEP_CHANNEL_STATE_CLOSED) {
        return -1; // TODO
    }
    
    l2cap_reserve_packet_buffer();
    bnep_out_buffer = l2cap_get_outgoing_buffer();

    /* Setup control packet type */
	bnep_out_buffer[pos++] = BNEP_PKT_TYPE_CONTROL;
	bnep_out_buffer[pos++] = BNEP_CONTROL_TYPE_SETUP_CONNECTION_RESPONSE;

    /* Add response code */
    bnep_out_buffer[pos++] = (response_code >> 8) & 0xFF;    
    bnep_out_buffer[pos++] = response_code & 0xFF;

    err = l2cap_send_prepared(channel->l2cap_cid, pos);
    
    if (err) {
        // TODO: Log error 
    }
    return err;
}

/* Send BNEP filter net type set message */
static int bnep_send_filter_net_type_set(bnep_channel_t *channel, ...)
{  
    if (channel->state == BNEP_CHANNEL_STATE_CLOSED) {
        return -1; // TODO
    }

    /* TODO: Not yet implemented */
    
    return -1;
}

/* Send BNEP filter net type response message */
static int bnep_send_filter_net_type_response(bnep_channel_t *channel, uint16_t response_code)
{
    uint8_t *bnep_out_buffer = NULL;
    uint16_t pos = 0;
    int      err = 0; 
    
    if (channel->state == BNEP_CHANNEL_STATE_CLOSED) {
        return -1; // TODO
    }
    
    l2cap_reserve_packet_buffer();
    bnep_out_buffer = l2cap_get_outgoing_buffer();

    /* Setup control packet type */
	bnep_out_buffer[pos++] = BNEP_PKT_TYPE_CONTROL;
	bnep_out_buffer[pos++] = BNEP_CONTROL_TYPE_FILTER_NET_TYPE_RESPONSE;

    /* Add response code */
    bnep_out_buffer[pos++] = (response_code >> 8) & 0xFF;    
    bnep_out_buffer[pos++] = response_code & 0xFF;

    err = l2cap_send_prepared(channel->l2cap_cid, pos);
    
    if (err) {
        // TODO: Log error 
    }
    return err;
}

/* Send BNEP filter multicast address set message */
static int bnep_send_filter_multi_addr_set(bnep_channel_t *channel, ...)
{    
    if (channel->state == BNEP_CHANNEL_STATE_CLOSED) {
        return -1; // TODO
    }

    /* TODO: Not yet implemented */
    
    return -1;
}

/* Send BNEP filter multicast address response message */
static int bnep_send_filter_multi_addr_response(bnep_channel_t *channel, uint16_t response_code)
{
    uint8_t *bnep_out_buffer = NULL;
    uint16_t pos = 0;
    int      err = 0; 
    
    if (channel->state == BNEP_CHANNEL_STATE_CLOSED) {
        return -1; // TODO
    }
    
    l2cap_reserve_packet_buffer();
    bnep_out_buffer = l2cap_get_outgoing_buffer();

    /* Setup control packet type */
	bnep_out_buffer[pos++] = BNEP_PKT_TYPE_CONTROL;
	bnep_out_buffer[pos++] = BNEP_CONTROL_TYPE_FILTER_MULTI_ADDR_RESPONSE;

    /* Add response code */
    bnep_out_buffer[pos++] = (response_code >> 8) & 0xFF;    
    bnep_out_buffer[pos++] = response_code & 0xFF;

    err = l2cap_send_prepared(channel->l2cap_cid, pos);
    
    if (err) {
        // TODO: Log error 
    }
    return err;
}

/* Send BNEP ethernet packet */
// TODO: Make static later?
int bnep_send(bnep_channel_t *channel, uint8_t *src_addr, uint8_t *dest_addr, uint16_t protocol_type, uint8_t *payload, uint16_t len)
{
    uint8_t *bnep_out_buffer = NULL;
    uint16_t pos = 0;
    int      err = 0;
    int      has_src;
    int      has_dest;
    
    if (channel->state == BNEP_CHANNEL_STATE_CLOSED) {
        return -1; // TODO
    }
    
    /* Check for free ACL buffers */
    if (!l2cap_can_send_packet_now(channel->l2cap_cid)) {
        return BTSTACK_ACL_BUFFERS_FULL;
    }
    
    l2cap_reserve_packet_buffer();
    bnep_out_buffer = l2cap_get_outgoing_buffer();

    /* Check if source address is the same as our local address and if the 
       destination address is the same as the remote addr. Maybe we can use
       the compressed data format
     */ 
    has_src  = (memcmp(src_addr, channel->local_addr, ETHER_ADDR_LEN) != 0);
    has_dest = (memcmp(dest_addr, channel->remote_addr, ETHER_ADDR_LEN) != 0);

    /* Fill in the package type depending on the given source and destination address */
    if (has_src && has_dest) {
        bnep_out_buffer[pos++] = BNEP_PKT_TYPE_GENERAL_ETHERNET;
    } else 
    if (has_src && !has_dest) {
        bnep_out_buffer[pos++] = BNEP_PKT_TYPE_COMPRESSED_ETHERNET_SOURCE_ONLY;
    } else 
    if (!has_src && has_dest) {
        bnep_out_buffer[pos++] = BNEP_PKT_TYPE_COMPRESSED_ETHERNET_DEST_ONLY;
    } else {
        bnep_out_buffer[pos++] = BNEP_PKT_TYPE_COMPRESSED_ETHERNET;
    }

    /* Add the destination address if needed */
    if (has_dest) {
        memcpy(bnep_out_buffer + pos, dest_addr, ETHER_ADDR_LEN);
        pos += ETHER_ADDR_LEN;
    }

    /* Add the source address if needed */
    if (has_src) {
        memcpy(bnep_out_buffer + pos, src_addr, ETHER_ADDR_LEN);
        pos += ETHER_ADDR_LEN;
    }

    /* Add protocol type */
    bnep_out_buffer[pos++] = (protocol_type >> 8) & 0xFF;    
    bnep_out_buffer[pos++] = protocol_type & 0xFF;

    /* TODO: Add extension headers, if we may support them at a later stage */
    
    /* Check for MTU limits add the payload and then send out the package */
    if (pos + len <= channel->max_frame_size) {
        memcpy(bnep_out_buffer + pos, payload, len);
        pos += len;

        err = l2cap_send_prepared(channel->l2cap_cid, pos);
    } else {
        // TODO: Error, MTU exceeded
    }
    
    if (err) {
        // TODO: Log error 
    }
    return err;        
}

inline static void bnep_channel_state_add(bnep_channel_t *channel, BNEP_CHANNEL_STATE_VAR event){
    channel->state_var = (BNEP_CHANNEL_STATE_VAR) (channel->state_var | event);    
}
inline static void bnep_channel_state_remove(bnep_channel_t *channel, BNEP_CHANNEL_STATE_VAR event){
    channel->state_var = (BNEP_CHANNEL_STATE_VAR) (channel->state_var & ~event);    
}

static uint16_t bnep_max_frame_size_for_l2cap_mtu(uint16_t l2cap_mtu){

    /* Assume a standard BNEP header, containing BNEP Type (1 Byte), dest and 
       source address (6 bytes each) and networking protocol type (2 bytes)
     */
    uint16_t max_frame_size = l2cap_mtu - 15; // 15 bytes BNEP header
    
    // single byte can denote len up to 127
    if (max_frame_size > 127) {
        max_frame_size--;
    }

    log_info("bnep_max_frame_size_for_l2cap_mtu:  %u -> %u", l2cap_mtu, max_frame_size);
    return max_frame_size;
}

static bnep_channel_t * bnep_channel_create_for_addr(bd_addr_t *addr)
{
    /* Allocate new channel structure */
    bnep_channel_t *channel = btstack_memory_bnep_channel_get();
    if (!channel) {
        return NULL;
    }

    /* Initialize the channel struct */
    memset(channel, 0, sizeof(bnep_channel_t));

    channel->state = BNEP_CHANNEL_STATE_CLOSED;
    channel->max_frame_size = bnep_max_frame_size_for_l2cap_mtu(l2cap_max_mtu());
    BD_ADDR_COPY(&channel->remote_addr, addr);
    BD_ADDR_COPY(&channel->local_addr, hci_local_bd_addr());

    channel->net_filter_count = 0;
    channel->multicast_filter_count = 0;

    /* Finally add it to the channel list */
    linked_list_add(&bnep_channels, (linked_item_t *) channel);
    
    return channel;
}

static bnep_channel_t* bnep_channel_for_addr(bd_addr_t *addr)
{
    linked_item_t *it;
    for (it = (linked_item_t *) bnep_channels; it ; it = it->next){
        bnep_channel_t *channel = ((bnep_channel_t *) it);
        if (BD_ADDR_CMP(addr, channel->remote_addr) == 0) {
            return channel;
        }
    }
    return NULL;
}

static bnep_channel_t * bnep_channel_for_l2cap_cid(uint16_t l2cap_cid)
{
    linked_item_t *it;
    for (it = (linked_item_t *) bnep_channels; it ; it = it->next){
        bnep_channel_t *channel = ((bnep_channel_t *) it);
        if (channel->l2cap_cid == l2cap_cid) {
            return channel;
        }
    }
    return NULL;
}

static bnep_service_t * bnep_service_for_uuid(uint16_t uuid)
{
    linked_item_t *it;
    for (it = (linked_item_t *) bnep_services; it ; it = it->next){
        bnep_service_t * service = ((bnep_service_t *) it);
        if ( service->service_uuid == uuid){
            return service;
        }
    }
    return NULL;
}

static void bnep_channel_free(bnep_channel_t *channel)
{
    linked_list_remove( &bnep_channels, (linked_item_t *) channel);
    btstack_memory_bnep_channel_free(channel);
}

static void bnep_channel_finalize(bnep_channel_t *channel)
{    
    uint16_t l2cap_cid;
    
    /* Inform application about closed channel */
    if (channel->state == BNEP_CHANNEL_STATE_CONNECTED) {
        bnep_emit_channel_closed(channel);
    }
 
    l2cap_cid = channel->l2cap_cid;
    
    /* Free ressources and then close the l2cap channel */
    bnep_channel_free(channel);
    l2cap_disconnect_internal(l2cap_cid, 0x13);
}

static int bnep_handle_connection_request(bnep_channel_t *channel, uint8_t *packet, uint16_t size)
{
    uint16_t uuid_size;
    uint16_t uuid_offset;
    uuid_size = packet[2];
    uint16_t response_code = BNEP_RESP_SETUP_SUCCESS;
    bnep_service_t * service;

    /* Sanity check packet size */
    if (size < 1 + 1 + 1 + 2 * uuid_size) {
        return 0;
    }

    if ((channel->state != BNEP_CHANNEL_STATE_WAIT_FOR_CONNECTION_REQUEST) &&
        (channel->state != BNEP_CHANNEL_STATE_CONNECTED)) {
        /* Ignore a connection request if not waiting for or still connected */
        log_info("BNEP_CONNCTION_REQUEST: ignored in state %d, l2cap_cid: %d!", channel->state, channel->l2cap_cid);
        return 0;
    }

     /* Extract source and destination UUID and convert them to UUID16 format */
    switch (uuid_size) {
        case 2:  /* UUID16  */
            uuid_offset = 0;
            break;
        case 4:  /* UUID32  */
        case 16: /* UUID128 */
            uuid_offset = 2;
            break;
        default:
            log_info("BNEP_CONNCTION_REQUEST: Invalid UUID size %d, l2cap_cid: %d!", channel->state, channel->l2cap_cid);
            response_code = BNEP_RESP_SETUP_INVALID_SERVICE_UUID_SIZE;
            break;
    }

    /* Check source and destination UUIDs for valid combinations */
    if (response_code == BNEP_RESP_SETUP_SUCCESS) {
        channel->uuid_dest = READ_BT_16(packet, 3 + uuid_offset);
        channel->uuid_source = READ_BT_16(packet, 3 + uuid_offset + uuid_size);

        if ((channel->uuid_dest != BNEP_UUID_PANU) && 
            (channel->uuid_dest != BNEP_UUID_NAP) &&
            (channel->uuid_dest != BNEP_UUID_GN)) {
            log_info("BNEP_CONNCTION_REQUEST: Invalid destination service UUID: %04x", channel->uuid_dest);
            channel->uuid_dest = 0;
        }    
        if ((channel->uuid_source != BNEP_UUID_PANU) && 
            (channel->uuid_source != BNEP_UUID_NAP) &&
            (channel->uuid_source != BNEP_UUID_GN)) {
            log_info("BNEP_CONNCTION_REQUEST: Invalid source service UUID: %04x", channel->uuid_source);
            channel->uuid_source = 0;
        }

        /* Check if we have registered a service for the requested destination UUID */
        service = bnep_service_for_uuid(channel->uuid_dest);
        if (service == NULL) {
            response_code = BNEP_RESP_SETUP_INVALID_DEST_UUID;
        } else 
        if ((channel->uuid_source != BNEP_UUID_PANU) && (channel->uuid_dest != BNEP_UUID_PANU)) {
            response_code = BNEP_RESP_SETUP_INVALID_SOURCE_UUID;
        }
    }

    /* Set flag to send out the connection response on next statemachine cycle */
    bnep_channel_state_add(channel, BNEP_CHANNEL_STATE_VAR_SND_CONNECTION_RESPONSE);
    channel->response_code = response_code;
        
    /* Return the number of processed package bytes = BNEP Type, BNEP Control Type, UUID-Size + 2 * UUID */
    return 1 + 1 + 1 + 2 * uuid_size;
}

static int bnep_handle_connection_response(bnep_channel_t *channel, uint8_t *packet, uint16_t size)
{
    uint16_t response_code;

    /* Sanity check packet size */
    if (size < 1 + 1 + 2) {
        return 0;
    }

    if (channel->state != BNEP_CHANNEL_STATE_WAIT_FOR_CONNECTION_RESPONSE) {
        /* Ignore a connection response in any state but WAIT_FOR_CONNECTION_RESPONSE */
        log_info("BNEP_CONNCTION_RESPONSE: Ignored in channel state %d", channel->state);
        return 1 + 1 + 2;
    }

    response_code = READ_BT_16(packet, 2);

    if (response_code == BNEP_RESP_SETUP_SUCCESS) {
        log_info("BNEP_CONNCTION_RESPONSE: Channel established to %s", bd_addr_to_str(channel->remote_addr));
        channel->state = BNEP_CHANNEL_STATE_CONNECTED;
    } else {
        log_info("BNEP_CONNCTION_RESPONSE: Connection to %s failed. Err: %d", bd_addr_to_str(channel->remote_addr), response_code);
        bnep_channel_finalize(channel);
    }
    return 1 + 1 + 2;
}

static int bnep_handle_filter_net_type_set(bnep_channel_t *channel, uint8_t *packet, uint16_t size)
{
    uint16_t list_length;
    uint16_t response_code = BNEP_RESP_FILTER_SUCCESS;
    
    /* Sanity check packet size */
    if (size < 2) {
        return 0;
    }
    
    list_length = READ_BT_16(packet, 2);
    /* Sanity check packet size again with known package size */
    if (size < 2 + list_length) {
        return 0;
    }

    if (channel->state != BNEP_CHANNEL_STATE_CONNECTED) {
        /* Ignore filter net type set in any state but CONNECTED */
        log_info("BNEP_FILTER_NET_TYPE_SET: Ignored in channel state %d", channel->state);
        return 2 + list_length;
    }

    /* Check if we have enough space for more filters */
    if ((list_length / (2*2)) + channel->net_filter_count > MAX_BNEP_NETFILTER) {
        log_info("BNEP_FILTER_NET_TYPE_SET: Too many filter");         
        response_code = BNEP_RESP_FILTER_ERR_TOO_MANY_FILTERS;
    } else {
        int i;
        /* There is still enough space, copy the filters to our filter list */
        for (i = 0; i < list_length / (2*2); i ++) {
            channel->net_filter[channel->net_filter_count].range_start = READ_BT_16(packet, 4 + i * 4);
            channel->net_filter[channel->net_filter_count].range_end = READ_BT_16(packet, 4 + i * 4 + 2);
            if (channel->net_filter[channel->net_filter_count].range_start > channel->net_filter[channel->net_filter_count].range_end) {
                /* Invalid filter range, ignore this filter rule */
                log_info("BNEP_FILTER_NET_TYPE_SET: Invalid filter: start: %d, end: %d", 
                         channel->net_filter[channel->net_filter_count].range_start,
                         channel->net_filter[channel->net_filter_count].range_end);                
                response_code = BNEP_RESP_FILTER_ERR_INVALID_RANGE;
            } else {
                /* Valid filter, increase the filter count */
                log_info("BNEP_FILTER_NET_TYPE_SET: Add filter: start: %d, end: %d", 
                         channel->net_filter[channel->net_filter_count].range_start,
                         channel->net_filter[channel->net_filter_count].range_end);
                channel->net_filter_count ++;
            }
        }
    }

    /* Set flag to send out the set net filter response on next statemachine cycle */
    bnep_channel_state_add(channel, BNEP_CHANNEL_STATE_VAR_SND_FILTER_NET_TYPE_RESPONSE);
    channel->response_code = response_code;

    return 2 + list_length;
}

static int bnep_handle_filter_net_type_response(bnep_channel_t *channel, uint8_t *packet, uint16_t size)
{
	uint16_t response_code;

    // TODO: Currently we do not support setting a network filter.
    
    /* Sanity check packet size */
    if (size < 1 + 1 + 2) {
        return 0;
    }

    if (channel->state != BNEP_CHANNEL_STATE_CONNECTED) {
        /* Ignore a filter net type response in any state but CONNECTED */
        log_info("BNEP_FILTER_NET_TYPE_RESPONSE: Ignored in channel state %d", channel->state);
        return 1 + 1 + 2;
    }

    response_code = READ_BT_16(packet, 2);

    if (response_code == BNEP_RESP_FILTER_SUCCESS) {
        log_info("BNEP_FILTER_NET_TYPE_RESPONSE: Net filter set successfully for %s", bd_addr_to_str(channel->remote_addr));
    } else {
        log_info("BNEP_FILTER_NET_TYPE_RESPONSE: Net filter setting for %s failed. Err: %d", bd_addr_to_str(channel->remote_addr), response_code);
    }

    return 1 + 1 + 2;
}

static int bnep_handle_multi_addr_set(bnep_channel_t *channel, uint8_t *packet, uint16_t size)
{
    uint16_t list_length;
    uint16_t response_code = BNEP_RESP_FILTER_SUCCESS;

    /* Sanity check packet size */
    if (size < 2) {
        return 0;
    }
    
    list_length = READ_BT_16(packet, 2);
    /* Sanity check packet size again with known package size */
    if (size < 2 + list_length) {
        return 0;
    }

    if (channel->state != BNEP_CHANNEL_STATE_CONNECTED) {
        /* Ignore multicast filter address set in any state but CONNECTED */
        log_info("BNEP_MULTI_ADDR_SET: Ignored in channel state %d", channel->state);
        return 2 + list_length;
    }

    /* Check if we have enough space for more filters */
    if ((list_length / (2 * ETHER_ADDR_LEN)) + channel->multicast_filter_count > MAX_BNEP_MULTICAST_FILTER) {
        log_info("BNEP_MULTI_ADDR_SET: Too many filter");         
        response_code = BNEP_RESP_FILTER_ERR_TOO_MANY_FILTERS;
    } else {
        int i;
        /* There is still enough space, copy the filters to our filter list */
        for (i = 0; i < list_length / (2 * ETHER_ADDR_LEN); i ++) {
            memcpy(channel->multicast_filter[channel->multicast_filter_count].addr_start, packet + 4 + i * ETHER_ADDR_LEN * 2, ETHER_ADDR_LEN);
            memcpy(channel->multicast_filter[channel->multicast_filter_count].addr_end, packet + 4 + i * ETHER_ADDR_LEN * 2 + ETHER_ADDR_LEN, ETHER_ADDR_LEN);

            if (memcmp(channel->multicast_filter[channel->multicast_filter_count].addr_start, 
                       channel->multicast_filter[channel->multicast_filter_count].addr_end, ETHER_ADDR_LEN) > 0) {
                /* Invalid filter range, ignore this filter rule */
                log_info("BNEP_MULTI_ADDR_SET: Invalid filter: start: %s", 
                         bd_addr_to_str(channel->multicast_filter[channel->multicast_filter_count].addr_start));
                log_info("BNEP_MULTI_ADDR_SET: Invalid filter: end: %s",
                         bd_addr_to_str(channel->multicast_filter[channel->multicast_filter_count].addr_end));
                response_code = BNEP_RESP_FILTER_ERR_INVALID_RANGE;
            } else {
                /* Valid filter, increase the filter count */
                log_info("BNEP_MULTI_ADDR_SET: Add filter: start: %s", 
                         bd_addr_to_str(channel->multicast_filter[channel->multicast_filter_count].addr_start));
                log_info("BNEP_MULTI_ADDR_SET: Add filter: end: %s",
                         bd_addr_to_str(channel->multicast_filter[channel->multicast_filter_count].addr_end));
                channel->multicast_filter_count ++;
            }
        }
    }
    /* Set flag to send out the set multi addr response on next statemachine cycle */
    bnep_channel_state_add(channel, BNEP_CHANNEL_STATE_VAR_SND_FILTER_MULTI_ADDR_RESPONSE);
    channel->response_code = response_code;
    
    return 2 + list_length;
}

static int bnep_handle_multi_addr_response(bnep_channel_t *channel, uint8_t *packet, uint16_t size)
{
	uint16_t response_code;

    // TODO: Currently we do not support setting multicast address filter.
    
    /* Sanity check packet size */
    if (size < 1 + 1 + 2) {
        return 0;
    }

    if (channel->state != BNEP_CHANNEL_STATE_CONNECTED) {
        /* Ignore multicast filter set response in any state but CONNECTED */
        log_info("BNEP_MULTI_ADDR_RESPONSE: Ignored in channel state %d", channel->state);
        return 1 + 1 + 2;
    }

    response_code = READ_BT_16(packet, 2);

    if (response_code == BNEP_RESP_FILTER_SUCCESS) {
        log_info("BNEP_MULTI_ADDR_RESPONSE: Multicast address filter set successfully for %s", bd_addr_to_str(channel->remote_addr));
    } else {
        log_info("BNEP_MULTI_ADDR_RESPONSE: Multicast address filter setting for %s failed. Err: %d", bd_addr_to_str(channel->remote_addr), response_code);
    }

    return 1 + 1 + 2;
}

static int bnep_handle_control_packet(bnep_channel_t *channel, uint8_t *packet, uint16_t size, int is_extension)
{
    int      rc = 0;
    uint16_t len;
    uint8_t  bnep_control_type;

    bnep_control_type = packet[1];
    /* Save last control type. Needed by statemachin in case of unknown control code */
    
    channel->last_control_type = bnep_control_type;
    switch (bnep_control_type) {
        case BNEP_CONTROL_TYPE_COMMAND_NOT_UNDERSTOOD:
            /* The last command we send was not understood. We should close the connection */
            log_info("BNEP_CONTROL: Received COMMAND_NOT_UNDERSTOOD: l2cap_cid: %d, cmd: %d", channel->l2cap_cid, packet[3]);
            bnep_channel_finalize(channel);
            len = 3; // Length of command not understood packet
            rc  = 1;
            break;
        case BNEP_CONTROL_TYPE_SETUP_CONNECTION_REQUEST:
            if (is_extension) {
                /* Connection requests are not allowed to be send in an extension header */
                log_info("BNEP_CONTROL: Received SETUP_CONNECTION_REQUEST in extension header: l2cap_cid: %d", channel->l2cap_cid);
                return 0;
            }
            len = bnep_handle_connection_request(channel, packet, size);
            if (len == 0) {
                /* If the connection request could not be handled, send a 
                   COMMAND_NOT_UNDERSTOOD message. 
                   Set flag to process the request in the next statemachine loop 
                 */
                bnep_channel_state_add(channel, BNEP_CHANNEL_STATE_VAR_SND_NOT_UNDERSTOOD);
            }
            rc = 1;
            break;
        case BNEP_CONTROL_TYPE_SETUP_CONNECTION_RESPONSE:
            if (is_extension) {
                /* Connection requests are not allowed to be send in an 
                   extension header 
                 */
                log_info("BNEP_CONTROL: Received SETUP_CONNECTION_RESPONSE in extension header: l2cap_cid: %d", channel->l2cap_cid);
                return 0;
            }
            len = bnep_handle_connection_response(channel, packet, size);             
            if (len == 0) {
                /* If the connection response could not be handled, send a 
                   COMMAND_NOT_UNDERSTOOD message. 
                   Set flag to handle in the next statemachine loop 
                 */
                bnep_channel_state_add(channel, BNEP_CHANNEL_STATE_VAR_SND_NOT_UNDERSTOOD);
            }
            rc = 1;
            break;
        case BNEP_CONTROL_TYPE_FILTER_NET_TYPE_SET:
            len = bnep_handle_filter_net_type_set(channel, packet, size);
            return 0;
            break;
        case BNEP_CONTROL_TYPE_FILTER_NET_TYPE_RESPONSE:
            len = bnep_handle_filter_net_type_response(channel, packet, size);
            break;
        case BNEP_CONTROL_TYPE_FILTER_MULTI_ADDR_SET:
            len = bnep_handle_multi_addr_set(channel, packet, size);
            break;
        case BNEP_CONTROL_TYPE_FILTER_MULTI_ADDR_RESPONSE:
            len = bnep_handle_multi_addr_response(channel, packet, size);
            break;
        default:
            log_info("BNEP_CONTROL: Invalid bnep control type: l2cap_cid: %d, cmd: %d", channel->l2cap_cid, bnep_control_type);
            len = 0;
            rc  = 1;
            break;
    }

    if (len == 0) {
        /* In case the command could not be handled, send a 
           COMMAND_NOT_UNDERSTOOD message. 
           Set flag to process the request in the next statemachine loop 
         */
        bnep_channel_state_add(channel, BNEP_CHANNEL_STATE_VAR_SND_NOT_UNDERSTOOD);        
    }

    return rc;
}

/**
 * @return handled packet
 */
static int bnep_hci_event_handler(uint8_t *packet, uint16_t size)
{
    bd_addr_t event_addr;
    uint16_t  psm;
    uint16_t  l2cap_cid;
    hci_con_handle_t con_handle;
    bnep_channel_t  *channel = NULL;
    uint8_t   status;
    
    switch (packet[0]) {
            
        /* Accept an incomming L2CAP connection on PSM_BNEP */
        case L2CAP_EVENT_INCOMING_CONNECTION:
            /* L2CAP event data: event(8), len(8), address(48), handle (16),  psm (16), source cid(16) dest cid(16) */
            bt_flip_addr(event_addr, &packet[2]);
            con_handle = READ_BT_16(packet,  8);
            psm        = READ_BT_16(packet, 10); 
            l2cap_cid  = READ_BT_16(packet, 12); 

            if (psm != PSM_BNEP) break;

            channel = bnep_channel_for_addr(&event_addr);

            if (channel) {
                log_info("INCOMING_CONNECTION (l2cap_cid 0x%02x) for PSM_BNEP => decline - channel already exists", l2cap_cid);
                l2cap_decline_connection_internal(l2cap_cid,  0x04);    // no resources available
                return 1;
            }
            
            /* Create a new BNEP channel instance (incomming) */
            channel = bnep_channel_create_for_addr(&event_addr);

            if (!channel) {
                log_info("INCOMING_CONNECTION (l2cap_cid 0x%02x) for PSM_BNEP => decline - no memory left", l2cap_cid);
                l2cap_decline_connection_internal(l2cap_cid,  0x04);    // no resources available
                return 1;
            }

            /* Assign connection handle and l2cap cid */
            channel->con_handle = con_handle;
            channel->l2cap_cid = l2cap_cid;

            /* Set channel into accept state */
            channel->state = BNEP_CHANNEL_STATE_WAIT_FOR_CONNECTION_REQUEST;
            
            log_info("L2CAP_EVENT_INCOMING_CONNECTION (l2cap_cid 0x%02x) for PSM_BNEP => accept", l2cap_cid);
            l2cap_accept_connection_internal(l2cap_cid);
            return 1;
            break;
            
        /* Outgoing L2CAP connection has been opened -> store l2cap_cid, remote_addr */
        case L2CAP_EVENT_CHANNEL_OPENED: 
            /* Check if the l2cap channel has been opened for PSM_BNEP */ 
            if (READ_BT_16(packet, 11) != PSM_BNEP) {
                break;
            }
            
            status = packet[2];
            log_info("L2CAP_EVENT_CHANNEL_OPENED for PSM_BNEP, status %u", status);
            
            /* Get the bnep channel fpr remote address */
            con_handle = READ_BT_16(packet, 9);
            l2cap_cid  = READ_BT_16(packet, 13);
            bt_flip_addr(event_addr, &packet[3]);
            channel = bnep_channel_for_addr(&event_addr);
            if (!channel) {
                log_error("L2CAP_EVENT_CHANNEL_OPENED but no BNEP channel prepared");
                return 1;
            }
            
            /* On L2CAP open error discard everything */
            if (status) {
                /* Emit bnep_channel_opened with status and free channel */
                bnep_emit_channel_opened(channel, status);

                /* Free BNEP channel mempory */
                bnep_channel_free(channel);
                return 1;
            }
            
            if (channel->state == BNEP_CHANNEL_STATE_CLOSED) {
                log_info("L2CAP_EVENT_CHANNEL_OPENED: outgoing connection");

                /* Check for the correct remote address */
                if (BD_ADDR_CMP(event_addr, channel->remote_addr)) {
                    break;
                }

                /* Assign connection handle and l2cap cid */
                channel->l2cap_cid  = l2cap_cid;
                channel->con_handle = con_handle;

                // TODO: Should we now send the connect request ?
                channel->state = BNEP_CHANNEL_STATE_WAIT_FOR_CONNECTION_RESPONSE;
                bnep_channel_state_add(channel, BNEP_CHANNEL_STATE_VAR_SND_CONNECTION_REQUEST); 
                channel->max_frame_size = bnep_max_frame_size_for_l2cap_mtu(READ_BT_16(packet, 17));
                bnep_run();
            } else {              
                log_info("L2CAP_EVENT_CHANNEL_OPENED: Instalid state: %d", channel->state);
            }
            return 1;
            break;
                    
        case DAEMON_EVENT_HCI_PACKET_SENT:
            bnep_run();
            break;
            
        case L2CAP_EVENT_CHANNEL_CLOSED:
            // data: event (8), len(8), channel (16)
            l2cap_cid   = READ_BT_16(packet, 2);
            channel = bnep_channel_for_l2cap_cid(l2cap_cid);
            log_info("L2CAP_EVENT_CHANNEL_CLOSED cid 0x%0x, channel %p", l2cap_cid, channel);

            if (!channel) {
                break;
            }

            log_info("L2CAP_EVENT_CHANNEL_CLOSED state %u", channel->state);
            switch (channel->state) {
                case BNEP_CHANNEL_STATE_WAIT_FOR_CONNECTION_REQUEST:
                case BNEP_CHANNEL_STATE_WAIT_FOR_CONNECTION_RESPONSE:
                case BNEP_CHANNEL_STATE_CONNECTED:
                    bnep_channel_finalize(channel);
                    return 1;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return 0;
}

static int bnep_l2cap_packet_handler(uint16_t l2cap_cid, uint8_t *packet, uint16_t size)
{
    int      rc = 0;
    uint8_t  bnep_type;
    uint8_t  bnep_header_has_ext;
    uint16_t pos = 0;
    bnep_channel_t *channel = NULL;
    
    /* Get the bnep channel for this package */
    channel = bnep_channel_for_l2cap_cid(l2cap_cid);
    if (!channel) {
        return rc;
    }

    /* Sort out short packages */
    if (size < 2) {
        return rc;
    }
    
    bnep_type = BNEP_TYPE(packet[0]);
    bnep_header_has_ext = BNEP_HEADER_HAS_EXT(packet[0]); 

    switch(bnep_type) {
        case BNEP_PKT_TYPE_GENERAL_ETHERNET:
            break;
        case BNEP_PKT_TYPE_COMPRESSED_ETHERNET:
            break;
        case BNEP_PKT_TYPE_COMPRESSED_ETHERNET_SOURCE_ONLY:
            break;
        case BNEP_PKT_TYPE_COMPRESSED_ETHERNET_DEST_ONLY:
            break;
        case BNEP_PKT_TYPE_CONTROL:
            rc = bnep_handle_control_packet(channel, packet, size, 0 /* TODO: Is extension bit ? */);
            break;
        default:
            break;
    }

    return rc;

}

void bnep_packet_handler(uint8_t packet_type, uint16_t l2cap_cid, uint8_t *packet, uint16_t size)
{
    bnep_channel_t* channel = NULL;
    
    int handled = 0;
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            handled = bnep_hci_event_handler(packet, size);
            break;
        case L2CAP_DATA_PACKET:
            handled = bnep_l2cap_packet_handler(l2cap_cid, packet, size);
            break;
        default:
            break;
    }
    
    if (handled) {
        bnep_run();
        return;
    }

    /* Forward non l2cap packages to application handler */
    if (packet_type != L2CAP_DATA_PACKET) {
        (*app_packet_handler)(NULL, packet_type, l2cap_cid, packet, size);
        return;
    }

    bnep_run();
}

static void bnep_channel_state_machine(bnep_channel_t* channel, bnep_channel_event_t *event)
{
    log_info("bnep_state_machine: state %u, state var: %02x, event %u", channel->state, channel->state_var, event->type);
 
    if (event->type == BNEP_CH_EVT_READY_TO_SEND) {
        /* Send outstanding packets. */
        if (channel->state_var & BNEP_CHANNEL_STATE_VAR_SND_NOT_UNDERSTOOD) {
            bnep_channel_state_remove(channel, BNEP_CHANNEL_STATE_VAR_SND_NOT_UNDERSTOOD);
            bnep_send_command_not_understood(channel, channel->last_control_type);
            return;
        }        
        if (channel->state_var & BNEP_CHANNEL_STATE_VAR_SND_CONNECTION_REQUEST) {
            bnep_channel_state_remove(channel, BNEP_CHANNEL_STATE_VAR_SND_CONNECTION_REQUEST);
            channel->state = BNEP_CHANNEL_STATE_WAIT_FOR_CONNECTION_RESPONSE;
            bnep_send_connection_request(channel, channel->uuid_source, channel->uuid_dest);
        }
        if (channel->state_var & BNEP_CHANNEL_STATE_VAR_SND_CONNECTION_RESPONSE) {
            if ((channel->state == BNEP_CHANNEL_STATE_CLOSED) ||
                (channel->state == BNEP_CHANNEL_STATE_WAIT_FOR_CONNECTION_REQUEST)) {
                /* Set channel state to STATE_CONNECTED */
                channel->state = BNEP_CHANNEL_STATE_CONNECTED;
            }
            
            bnep_channel_state_remove(channel, BNEP_CHANNEL_STATE_VAR_SND_CONNECTION_RESPONSE);
            bnep_send_connection_response(channel, channel->response_code);
            return;
        }
        if (channel->state_var & BNEP_CHANNEL_STATE_VAR_SND_FILTER_NET_TYPE_RESPONSE) {
            bnep_channel_state_remove(channel, BNEP_CHANNEL_STATE_VAR_SND_FILTER_NET_TYPE_RESPONSE);
            bnep_send_filter_net_type_response(channel, channel->response_code);
            return;
        }
        if (channel->state_var & BNEP_CHANNEL_STATE_VAR_SND_FILTER_MULTI_ADDR_RESPONSE) {
            bnep_channel_state_remove(channel, BNEP_CHANNEL_STATE_VAR_SND_FILTER_MULTI_ADDR_RESPONSE);
            bnep_send_filter_multi_addr_response(channel, channel->response_code);
            return;
        }
    }    
}


/* Process oustanding signaling tasks */
static void bnep_run(void)
{
    linked_item_t *it;
    linked_item_t *next;
    
    for (it = (linked_item_t *) bnep_channels; it ; it = next){

        next = it->next;    // be prepared for removal of channel in state machine

        bnep_channel_t * channel = ((bnep_channel_t *) it);
        
        if (!l2cap_can_send_packet_now(channel->l2cap_cid)) {
            continue;
        }

        bnep_channel_event_t channel_event = { BNEP_CH_EVT_READY_TO_SEND };
        bnep_channel_state_machine(channel, &channel_event);
    }
}
    
/* BNEP BTStack API */
void bnep_init(void)
{
    bnep_security_level = LEVEL_0;
}

void bnep_set_required_security_level(gap_security_level_t security_level)
{
    bnep_security_level = security_level;
}

int bnep_connect(void * connection, bd_addr_t *addr, uint16_t uuid_dest)
{
    bnep_channel_t *channel;
    log_info("BNEP_CONNECT addr %s", bd_addr_to_str(*addr));

    channel = bnep_channel_create_for_addr(addr);
    if (channel == NULL) {
        return -1;
    }

    channel->uuid_source = BNEP_UUID_PANU;
    channel->uuid_dest   = uuid_dest;

    l2cap_create_channel_internal(connection, bnep_packet_handler, *addr, PSM_BNEP, l2cap_max_mtu());
    
    return 0;
}

void bnep_disconnect(bd_addr_t *addr)
{
    bnep_channel_t *channel;
    log_info("BNEP_DISCONNECT");

    channel = bnep_channel_for_addr(addr);
    
    bnep_channel_finalize(channel);

    bnep_run();
}


void bnep_register_service(void * connection, uint16_t service_uuid, uint16_t max_frame_size)
{
    log_info("BNEP_REGISTER_SERVICE mtu %d", max_frame_size);

    /* Check if we already registered a service */
    bnep_service_t * service = bnep_service_for_uuid(service_uuid);
    if (service) {
        bnep_emit_service_registered(connection, BNEP_SERVICE_ALREADY_REGISTERED, service_uuid);
        return;
    }

    /* Only alow one the three service types: PANU, NAP, GN */
    if ((service_uuid != BNEP_UUID_PANU) && 
        (service_uuid != BNEP_UUID_NAP) &&
        (service_uuid != BNEP_UUID_GN)) {
        log_info("BNEP_REGISTER_SERVICE: Invalid service UUID: %04x", service_uuid);
        return;
    }
    
    /* Allocate service memory */
    service = (bnep_service_t*) btstack_memory_bnep_service_get();
    if (!service) {
        bnep_emit_service_registered(connection, BTSTACK_MEMORY_ALLOC_FAILED, service_uuid);
        return;
    }
    
    /* register with l2cap if not registered before, max MTU */
    l2cap_register_service_internal(NULL, bnep_packet_handler, PSM_BNEP, 0xffff, bnep_security_level);
        
    /* Setup the service struct */
    service->connection     = connection;
    service->max_frame_size = max_frame_size;
    service->service_uuid    = service_uuid;

    /* Add to services list */
    linked_list_add(&bnep_services, (linked_item_t *) service);
    
    /* Inform the application layer */
    bnep_emit_service_registered(connection, 0, service_uuid);
}

void bnep_unregister_service(uint16_t service_uuid)
{
    log_info("BNEP_UNREGISTER_SERVICE #%04x", service_uuid);

    bnep_service_t *service = bnep_service_for_uuid(service_uuid);    
    if (!service) {
        return;
    }

    linked_list_remove(&bnep_services, (linked_item_t *) service);
    btstack_memory_bnep_service_free(service);
    service = NULL;
    
    l2cap_unregister_service_internal(NULL, PSM_BNEP);
}
