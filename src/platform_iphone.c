//
//  platform_iphone.c
//
//  Created by Matthias Ringwald on 8/15/09.
//

#include "../config.h"

#include "platform_iphone.h"

#include "../SpringBoardAccess/SpringBoardAccess.h"

#ifdef USE_SPRINGBOARD

// update SpringBoard icons
void platform_iphone_status_handler(BLUETOOTH_STATE state){
    switch (state) {
        case BLUETOOTH_OFF:
            SBA_removeStatusBarImage("BTstack");
            SBA_removeStatusBarImage("BTstackActive");
            break;
        case BLUETOOTH_ON:
            SBA_removeStatusBarImage("BTstackActive");
            SBA_addStatusBarImage("BTstack");
            break;
        case BLUETOOTH_ACTIVE:
            SBA_removeStatusBarImage("BTstack");
            SBA_addStatusBarImage("BTstackActive");
            break;
        default:
            break;
    }
}

#endif