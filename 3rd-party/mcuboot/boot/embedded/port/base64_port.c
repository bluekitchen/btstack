
#include "port/base64_port.h"
//#include "mbedtls/base64.h"

int base64_port_encode( char * dst, size_t dlen, size_t * olen, char * src, size_t slen )
{
    int rc = 0;/*mbedtls_base64_encode( (unsigned char *)dst,
                                    dlen,
                                    (size_t *)olen,
                                    (unsigned char *)src,
                                    slen );*/
    return rc == 0 ? 0 : -1;
}

int base64_port_decode( char * dst, size_t dlen, int * olen, char * src, size_t slen )
{
    int rc = 0;/*mbedtls_base64_decode( (unsigned char *)dst,
                                    dlen,
                                    (size_t *)olen,
                                    (unsigned char *)src,
                                    slen );*/
    return rc == 0 ? 0 : -1;
}