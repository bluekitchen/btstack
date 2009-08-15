//
//  platform_iphone.m
//
//  Created by Matthias Ringwald on 8/15/09.
//

#import "platform_iphone.h"

// update SpringBoard icon
void platform_iphone_status_handler(BLUETOOTH_STATE state){
    switch (state) {
        case BLUETOOTH_OFF:
            break;
        case BLUETOOTH_ON:
            break;
        case BLUETOOTH_ACTIVE:
            break;
        default:
            break;
    }
}