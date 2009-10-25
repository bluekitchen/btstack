//
//  platform_iphone.c
//
//  Created by Matthias Ringwald on 8/15/09.
//

#include "platform_iphone.h"

#include "../config.h"
#include "../SpringBoardAccess/SpringBoardAccess.h"

#include <stdio.h>

#ifdef USE_SPRINGBOARD

// update SpringBoard icons
void platform_iphone_status_handler(BLUETOOTH_STATE state){
    switch (state) {
        case BLUETOOTH_OFF:
            SBA_removeStatusBarImage("BTstack");
            SBA_removeStatusBarImage("BTstackActive");
            printf("Bluetooth status: OFF\n");
            break;
        case BLUETOOTH_ON:
            SBA_removeStatusBarImage("BTstackActive");
            SBA_addStatusBarImage("BTstack");
            printf("Bluetooth status: ON\n");
            break;
        case BLUETOOTH_ACTIVE:
            SBA_removeStatusBarImage("BTstack");
            SBA_addStatusBarImage("BTstackActive");
            printf("Bluetooth status: ACTIVE\n");
            break;
        default:
            break;
    }
}

#endif