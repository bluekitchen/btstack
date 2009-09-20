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
	// create message
	int messageLen = 1 + dataLen;
	UInt8 message[messageLen];
	message[0] = cmd;
	memcpy(&message[1], data, dataLen);
	CFDataRef cfData = CFDataCreate(NULL, message, messageLen);
	int result = CFMessagePortSendRequest(springBoardAccessMessagePort, 0, cfData, 1, 1, NULL, NULL);
	CFRelease(cfData);
	return result;
}

int SBA_addStatusBarImage(char *name){
	return SBA_sendMessage(SBAC_addStatusBarImage, strlen(name), (UInt8*) name);
}

int SBA_removeStatusBarImage(char *name){
	return SBA_sendMessage(SBAC_removeStatusBarImage, strlen(name), (UInt8*) name);
}

