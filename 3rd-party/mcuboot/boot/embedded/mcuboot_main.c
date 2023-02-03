
#include "bootutil/bootutil.h"
#include "bootutil/bootutil_log.h"
#include "bootutil/image.h"
#include "boot_serial.h"
#include "mcuboot_config/mcuboot_logging.h"

#include "port/boot_log_port.h"
#include "port/boot_serial_port.h"
#include "port/boot_startup_port.h"
#include "port/boot_heap_port.h"


int main_boot( void )
{
	struct boot_rsp rsp;
	fih_int fih_rc = FIH_FAILURE;

	BOOT_LOG_INF("Starting bootloader...");

	/* Init required bootloader hardware, such as watchdog */
	boot_port_init();
	BOOT_LOG_INF("Bootloader port initialized.");
	boot_port_log_init();
	BOOT_LOG_INF("Bootloader logging initialized.");

#if MCUBOOT_SERIAL > 0
	boot_port_serial_init();
	BOOT_LOG_INF("Bootloader serial IF initialized.");
	BOOT_LOG_INF("Checking serial boot activation pin/button.");
	if ( boot_port_serial_detect_boot_pin() )
	{
		BOOT_LOG_INF("Entering serial boot mode...");
		boot_port_wdt_disable();
		const struct boot_uart_funcs * boot_funcs = boot_port_serial_get_functions();
		boot_serial_start( boot_funcs );
		BOOT_LOG_ERR("Returned from serial boot mode.");
		FIH_PANIC;
	}
	else
	{
		BOOT_LOG_INF("Skipping serial boot mode...");
	}
#endif

	BOOT_LOG_INF("Starting boot...");
	boot_port_heap_init();
	FIH_CALL(boot_go, fih_rc, &rsp);
	if (fih_not_eq(fih_rc, FIH_SUCCESS))
	{
		BOOT_LOG_ERR("Unable to find bootable image.");
		FIH_PANIC;
	}
	boot_port_startup( &rsp );

	BOOT_LOG_ERR("Returned after image startup. Should never should get here.");
	while (1);
}
