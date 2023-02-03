
#ifndef _CRC16_PORT_H_
#define _CRC16_PORT_H_

#include <stdint.h>

uint16_t crc16_port_ccitt( uint16_t crc, char * data, uint32_t len);


#endif