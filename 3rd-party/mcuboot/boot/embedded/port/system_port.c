
#include "port/system_port.h"
#include "unistd.h"

uint16_t system_port_ntohs( uint16_t x )
{
//    return be16toh( x );
	return x;
}

uint16_t system_port_htons( uint16_t x )
{
//    return htobe16( x );
	return x;
}

void system_port_usleep( uint32_t usec )
{
//    esp_rom_delay_us( usec );
	//usleep(usec);
}

void system_port_reset( void )
{
//    software_reset();
}
