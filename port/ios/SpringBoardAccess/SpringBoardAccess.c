/*
 * Copyright (C) 2009 by Matthias Ringwald
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 *  SpringBoardAcess.c
 *
 *  Created by Matthias Ringwald on 9/20/09.
 *
 */

#include "SpringBoardAccess.h"

#import <CoreFoundation/CoreFoundation.h>

static CFMessagePortRef springBoardAccessMessagePort = 0;


static void SBA_refresh(void){
	// still valid
	if (springBoardAccessMessagePort && !CFMessagePortIsValid(springBoardAccessMessagePort)){
		CFRelease(springBoardAccessMessagePort);
		springBoardAccessMessagePort = NULL;
	}
	// create new one
	if (!springBoardAccessMessagePort) {
		springBoardAccessMessagePort = CFMessagePortCreateRemote(NULL, CFSTR(SBA_MessagePortName));
	}
}

int SBA_available(void){
	SBA_refresh();
	if (springBoardAccessMessagePort) return 1;
	return 0;
}

static int SBA_sendMessage(UInt8 cmd, UInt16 dataLen, UInt8 *data, CFDataRef *resultData){
		
	SBA_refresh();
	
	// well, won't work
	if (!springBoardAccessMessagePort) {
		return kCFMessagePortIsInvalid;
	}
	// create and send message
	CFDataRef cfData = CFDataCreate(NULL, data, dataLen);
	CFStringRef replyMode = NULL;
	if (resultData) {
		replyMode = kCFRunLoopDefaultMode;
	}
	int result = CFMessagePortSendRequest(springBoardAccessMessagePort, cmd, cfData, 1, 1, replyMode, resultData);
	CFRelease(cfData);
	return result;
}

int SBA_addStatusBarImage(char *name){
	return SBA_sendMessage(SBAC_addStatusBarImage, strlen(name), (UInt8*) name, NULL);
}

int SBA_removeStatusBarImage(char *name){
	return SBA_sendMessage(SBAC_removeStatusBarImage, strlen(name), (UInt8*) name, NULL);
}

int SBA_getBluetoothEnabled(void){
	CFDataRef cfData;
	int result = SBA_sendMessage(SBAC_getBluetoothEnabled, 0, NULL, &cfData);
	if (result == 0){
		const uint8_t *data = CFDataGetBytePtr(cfData);
		UInt16 dataLen = CFDataGetLength(cfData);
		if (!dataLen) return -10;
		if (data[0]) {
			result = 1;
		} else {
			result = 0;
		}
		CFRelease(cfData);
	} else {
		// error talking to SpringBoardAccess
		// well, assume it's off and hope for the best
		result = 0;
	}
	return result;
}

int SBA_setBluetoothEnabled(int on){
	uint8_t enabled = 0;
	if (on) enabled = 1;
	return SBA_sendMessage(SBAC_setBluetoothEnabled, 1, (UInt8*) &enabled, NULL);
}


