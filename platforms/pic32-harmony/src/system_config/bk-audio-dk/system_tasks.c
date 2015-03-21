#include "system_config.h"
#include "system_definitions.h"

void SYS_Tasks ( void )
{
    /* Maintain the state machines of all polled modules in the system. */
    SYS_DEVCON_Tasks(sysObj.sysDevcon);
    DRV_TMR_Tasks(sysObj.drvTmr);

    APP_Tasks();
}
