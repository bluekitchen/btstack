/**
 */

#include <stdint.h>
#include <btstack/utils.h>


static char new_bd_addr_to_str_buffer[6*3];  // 12:45:78:01:34:67\0
static char * new_bd_addr_to_str(bd_addr_t addr){
    sprintf(new_bd_addr_to_str_buffer, "%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
        return (char *) new_bd_addr_to_str_buffer;
}

#define bd_addr_to_str(x) new_bd_addr_to_str(x)
