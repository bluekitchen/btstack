
#ifndef _SYSTEM_PORT_H_
#define _SYSTEM_PORT_H_

#include <stdint.h>

uint16_t system_port_ntohs( uint16_t x );
uint16_t system_port_htons( uint16_t x );

void system_port_usleep( uint32_t usec );
void system_port_reset( void );
void system_port_deinit( void );
void system_port_remap_vector_table(uint32_t vector_table);
uint8_t system_port_is_msp_valid(uint32_t msp);
#endif