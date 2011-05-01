/*
 *  RFCOMM.h
 *
 *  Created by Matthias Ringwald on 10/19/09.
 */

#include <btstack/btstack.h>
#include <btstack/utils.h>

#include <stdint.h>

void rfcomm_init();

// register packet handler
void rfcomm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
void rfcomm_register_packet_handler(void (*handler)(void * connection, uint8_t packet_type,
													uint16_t channel, uint8_t *packet, uint16_t size));

// BTstack Internal RFCOMM API
void rfcomm_create_channel_internal(void * connectio, bd_addr_t *addr, uint8_t channel);
void rfcomm_disconnect_internal(uint16_t rfcomm_cid);
void rfcomm_register_service_internal(void * connection, uint16_t registration_id, uint16_t max_frame_size);
void rfcomm_unregister_service_internal(uint8_t service_channel);
void rfcomm_accept_connection_internal(uint16_t rfcomm_cid);
void rfcomm_decline_connection_internal(uint16_t rfcomm_cid);
int  rfcomm_send_internal(uint8_t rfcomm_cid, uint8_t *data, uint16_t len);



