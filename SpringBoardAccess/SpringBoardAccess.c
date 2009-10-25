/*
 *  SpringBoardAcess.c
 *
 *  Created by Matthias Ringwald on 9/20/09.
 *
 */

#include "SpringBoardAccess.h"

#import <CoreFoundation/CoreFoundation.h>

static CFMessagePortRef springBoardAccessMessagePort = 0;

static int SBA_sendMessage(UInt8 cmd, UInt16 dataLen, UInt8 *data){
	// check for port
	if (!springBoardAccessMessagePort) {
		springBoardAccessMessagePort = CFMessagePortCreateRemote(NULL, CFSTR(SBA_MessagePortName));
	}
	if (!springBoardAccessMessagePort) {
		return kCFMessagePortIsInvalid;
	}
	// create and send message
	CFDataRef cfData = CFDataCreate(NULL, data, dataLen);
	int result = CFMessagePortSendRequest(springBoardAccessMessagePort, cmd, cfData, 1, 1, NULL, NULL);
	CFRelease(cfData);
	return result;
}

int SBA_addStatusBarImage(char *name){
	return SBA_sendMessage(SBAC_addStatusBarImage, strlen(name), (UInt8*) name);
}

int SBA_removeStatusBarImage(char *name){
	return SBA_sendMessage(SBAC_removeStatusBarImage, strlen(name), (UInt8*) name);
}

