#ifndef _SYSTEM_DEFINITIONS_H
#define _SYSTEM_DEFINITIONS_H

#include <stddef.h> // Defines NULL
#include "system/common/sys_module.h"
#include "system/clk/sys_clk.h"
#include "system/devcon/sys_devcon.h"
#include "driver/tmr/drv_tmr.h"

/* System Objects */
typedef struct
{
    /* Device control system service object handle. */
    SYS_MODULE_OBJ sysDevcon;

    /* Timer driver object handle */
    SYS_MODULE_OBJ drvTmr;

} SYSTEM_OBJECTS;

extern SYSTEM_OBJECTS sysObj;

#endif /* _SYSTEM_DEFINITIONS_H */
