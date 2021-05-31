#include "app.h"
#include "btstack_port.h"

/* Application State Machine */
void APP_Initialize ( void ) {
    APP_Debug_Initialize();
    BTSTACK_Initialize();
}

void APP_Tasks ( void ) {
    BTSTACK_Tasks();
}

