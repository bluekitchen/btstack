
#include "mcuboot_config/mcuboot_logging.h"

#include <stdarg.h>
#include <stdint.h>

/* Workaround issue with wrapping ets_printf. vLog is LD provided as symbol with same address
 * as ets_printf, which is fine as they have the same signature */

/*-----------------------------------------------------------*/
/* Boot logging is initialized by MCUBoot port bootloader_init()*/
void boot_port_log_init( void )
{}