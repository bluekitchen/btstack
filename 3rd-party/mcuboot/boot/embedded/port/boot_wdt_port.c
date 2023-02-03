
#include "port/boot_wdt_port.h"

//#include "hal/wdt_hal.h"

void boot_port_wdt_feed( void )
{
//    wdt_hal_context_t rtc_wdt_ctx = {.inst = WDT_RWDT, .rwdt_dev = &RTCCNTL};
//    wdt_hal_write_protect_disable(&rtc_wdt_ctx);
//    wdt_hal_feed(&rtc_wdt_ctx);
//    wdt_hal_write_protect_enable(&rtc_wdt_ctx);
}

/* Either called before serial recovery starts, which never returns, or upon app startup */
void boot_port_wdt_disable( void )
{
//    wdt_hal_context_t rtc_wdt_ctx = {.inst = WDT_RWDT, .rwdt_dev = &RTCCNTL };
//
//    wdt_hal_write_protect_disable(&rtc_wdt_ctx);
//    wdt_hal_disable(&rtc_wdt_ctx);
//    wdt_hal_write_protect_enable(&rtc_wdt_ctx);
}