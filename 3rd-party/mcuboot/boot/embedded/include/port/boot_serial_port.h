
#ifndef _BOOT_SERIAL_PORT_H_
#define _BOOT_SERIAL_PORT_H_

#include "boot_serial.h"

#include <stdbool.h>

/* Initializes serial interface to mcumgr and gpio boot pin */
void boot_port_serial_init( void );

/* Return true if boot pin was activated within timeout */
bool boot_port_serial_detect_boot_pin( void );

/* Returns pointer to static structure with boot_uart_funcs defined */
const struct boot_uart_funcs * boot_port_serial_get_functions( void );

#endif