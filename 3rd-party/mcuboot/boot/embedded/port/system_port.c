
#include "port/system_port.h"
#include "unistd.h"
#include "hal_cpu.h"

uint16_t system_port_ntohs( uint16_t x )
{
	return x;
}

uint16_t system_port_htons( uint16_t x )
{
	return x;
}

void system_port_usleep( uint32_t usec )
{
}

void system_port_reset( void )
{
    hal_cpu_reset();
}

void system_port_deinit( void )
{
    hal_cpu_deinit();
}

void system_port_remap_vector_table(uint32_t vector_table)
{
    hal_cpu_remap_vecotr_table(vector_table);
}

uint8_t system_port_is_msp_valid(uint32_t msp)
{
    return hal_cpu_is_msp_valid(msp);
}

