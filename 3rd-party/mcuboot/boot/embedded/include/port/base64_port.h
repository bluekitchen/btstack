
#ifndef _BASE64_PORT_H_
#define _BASE64_PORT_H_

#include <stddef.h>

int base64_port_encode( char * dst, size_t dlen, size_t * olen, char * src, size_t slen );
int base64_port_decode( char * dst, size_t dlen, int * olen, char * src, size_t slen );

#endif