
//*****************************************************************************
//
// minimal setup for SDP client over USB or UART
//
//*****************************************************************************

#include <btstack/utils.h>
#include "sdp_parser.h"


typedef struct sdp_query_rfcomm_event {
    uint8_t type;
} sdp_query_rfcomm_event_t;

typedef struct sdp_query_rfcomm_service_event {
    uint8_t type;
    uint8_t channel_nr;
    uint8_t * service_name;
} sdp_query_rfcomm_service_event_t;

typedef struct sdp_query_rfcomm_complete_event {
    uint8_t type;
    uint8_t status; // 0 == OK
} sdp_query_rfcomm_complete_event_t;


void sdp_query_rfcomm_init(void);
void sdp_query_rfcomm_channel_and_name_for_service_with_uuid(bd_addr_t remote, uint16_t uuid);
void sdp_query_rfcomm_channel_and_name_for_service_with_service_search_pattern(bd_addr_t remote, uint8_t * des_serviceSearchPattern);
void sdp_query_rfcomm_register_callback(void(*sdp_app_callback)(sdp_query_rfcomm_event_t * event, void * context), void * context);
