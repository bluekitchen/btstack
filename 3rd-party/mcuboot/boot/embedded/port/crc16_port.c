
#include "port/crc16_port.h"

uint16_t crc16_port_ccitt( uint16_t crc, char * data, uint32_t len)
{
    uint16_t inter = 0;
    for ( int i=0; i<len; i++ )
    {
        inter = (uint16_t)( data[i] << 8 );
        crc = crc ^ inter;

        for ( int j=0; j<8; j++ )
        {
            crc = (crc & 0x8000) ? (crc << 1 ^ 0x1021) : (crc << 1);
        }
    }

    return crc;
}