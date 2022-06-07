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

#if defined __cplusplus
}
#endif
