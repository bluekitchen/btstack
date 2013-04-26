
//*****************************************************************************
//
// minimal setup for SDP client over USB or UART
//
//*****************************************************************************

#include <btstack/utils.h>

void sdp_client_query(bd_addr_t remote, uint8_t * des_serviceSearchPattern, uint8_t * des_attributeIDList);
