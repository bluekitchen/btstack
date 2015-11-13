#include "system_config.h"
#include "system_definitions.h"

// ****************************************************************************
// ****************************************************************************
// Section: Configuration Bits
// ****************************************************************************
// ****************************************************************************

/*** DEVCFG0 ***/

#pragma config DEBUG =      ON
#pragma config JTAGEN =     OFF
#pragma config ICESEL =     ICS_PGx2
#pragma config PWP =        0xff
#pragma config BWP =        OFF
#pragma config CP =         OFF

/*** DEVCFG1 ***/

#pragma config FNOSC =      PRIPLL
#pragma config FSOSCEN =    OFF
#pragma config IESO =       OFF
#pragma config POSCMOD =    HS
#pragma config OSCIOFNC =   OFF
#pragma config FPBDIV =     DIV_2
#pragma config FCKSM =      CSDCMD
#pragma config WDTPS =      PS1048576
#pragma config FWDTEN =     OFF
#pragma config WINDIS =     OFF
#pragma config FWDTWINSZ =  WINSZ_25

/*** DEVCFG2 ***/

#pragma config FPLLIDIV =   DIV_3
#pragma config FPLLMUL =    MUL_24
#pragma config FPLLODIV =   DIV_1
#pragma config UPLLIDIV =   DIV_3
#pragma config UPLLEN =     ON

/*** DEVCFG3 ***/

#pragma config USERID =     0xffff
#pragma config FSRSSEL =    PRIORITY_7
#pragma config PMDL1WAY =   OFF
#pragma config IOL1WAY =    OFF
#pragma config FUSBIDIO =   OFF
#pragma config FVBUSONIO =  OFF


/* TMR Driver Initialization Data */
const DRV_TMR_INIT drvTmrInitData =
{
    {APP_TMR_DRV_POWER_MODE},
    APP_TMR_DRV_HW_ID,
    APP_TMR_DRV_CLOCK_SOURCE,
    APP_TMR_DRV_PRESCALE,
    APP_TMR_DRV_INT_SOURCE,
    APP_TMR_DRV_OPERATION_MODE
};

/* Structure to hold the system objects. */
SYSTEM_OBJECTS sysObj;

/* System Initialization Function */
void SYS_Initialize ( void* data )
{
    /* Initialize services, drivers & libraries */
    SYS_CLK_Initialize(NULL);

    // optimize system tuning
    sysObj.sysDevcon = SYS_DEVCON_Initialize(SYS_DEVCON_INDEX_0, NULL);
    SYS_DEVCON_PerformanceConfig(SYS_CLOCK_FREQENCY);

    sysObj.drvTmr = DRV_TMR_Initialize(APP_TMR_DRV_INDEX, (SYS_MODULE_INIT *)&drvTmrInitData);

    /* Initialize the application. */
    APP_Initialize();
}