#include <stdint.h>
#include "btstack_defines.h"

#if defined __cplusplus
extern "C" {
#endif

void sdp_parser_init(btstack_packet_handler_t callback);
void sdp_parser_handle_chunk(uint8_t * data, uint16_t size);
void sdp_parser_init_service_attribute_search(void);
void sdp_parser_init_service_search(void);
void sdp_parser_handle_service_search(uint8_t * data, uint16_t total_count, uint16_t record_handle_count);

void sdp_client_query_rfcomm_init(void);

void sdp_client_reset(void);

void     mock_l2cap_reset(void);
uint16_t mock_l2cap_get_disconnect_count(void);
uint16_t mock_l2cap_get_send_prepared_count(void);
uint16_t mock_l2cap_get_last_transaction_id(void);
void     mock_l2cap_emit_channel_opened(uint16_t local_cid);
void     mock_l2cap_emit_data(uint16_t local_cid, uint8_t * packet, uint16_t size);

#if defined __cplusplus
}
#endif
