
#include "boot_serial.h"
#include "bootutil/bootutil_log.h"

#include "port/boot_serial_port.h"

#include <stdint.h>

//#include "soc/rtc.h"
//#include "soc/gpio_periph.h"
//#include "soc/dport_reg.h"
//#include "soc/uart_struct.h"
//#include "soc/gpio_struct.h"
//#include "esp32/rom/uart.h"
//#include "esp32/rom/gpio.h"

//#include "esp_rom_sys.h"
//#include "esp_rom_uart.h"
//#include "esp_rom_gpio.h"
//#include "gpio_hal.h"
//#include "gpio_ll.h"

//#include "uart_ll.h"


#include <string.h>

/***************************************************************************/
static int boot_port_serial_read( char * str, int cnt, int *newline );
static void boot_port_serial_write( const char *prt, int cnt );

static const struct boot_uart_funcs xUartFunctions = {
    .read = boot_port_serial_read,
    .write = boot_port_serial_write
};

/***************************************************************************/
void boot_port_serial_init( void )
{
//    configure_mcumgr_uart();
//    configure_boot_serial_button();
}

bool boot_port_serial_detect_boot_pin( void )
{
//    uint8_t aggregate = 0;
//    const int n_samples = 5;
//    for( int i=0; i<n_samples; i++ )
//    {
//        aggregate += gpio_ll_get_level( &GPIO, BOOT_SERIAL_TRIGGER_BUTTON );
//        esp_rom_delay_us( 1000 );
//    }
//
//    return ( aggregate / n_samples ) == 1;
	return 0;
}

const struct boot_uart_funcs * boot_port_serial_get_functions( void )
{
    return &xUartFunctions;
}

static int boot_port_serial_read( char * str, int cnt, int *newline )
{
    volatile uint32_t n_read = 0;
//
//    n_read = readline( (uint8_t *)str, cnt );
//    *newline = (str[n_read-1] == '\n') ? 1 : 0;

    return n_read;
}

static void boot_port_serial_write( const char *prt, int cnt )
{
//    uint32_t space = uart_ll_get_txfifo_len( &UART1 );
//    if( cnt > space )
//    {
//        BOOT_LOG_ERR("Unable to send full message. TX fifo would overflow");
//    }
//    /* TODO: Should wait for available space, or continuously chunk */
//    uart_ll_write_txfifo( &UART1, (const uint8_t *)prt, cnt );
//
//    BOOT_LOG_DBG("Response[%d]: %s", cnt, prt);
}
