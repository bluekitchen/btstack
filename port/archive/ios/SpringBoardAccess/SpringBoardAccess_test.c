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

//
//  SpringBoardAccess-test.c
//
//  Created by Matthias Ringwald on 9/15/09.
//
#include "SpringBoardAccess.h"

#include <string.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
	int usage = 1;
	
	if (argc > 1) {
		if (argc == 3 && strcmp("add", argv[1]) == 0) {
			SBA_addStatusBarImage(argv[2]);
			usage = 0;
		} else if (argc == 3 && strcmp("remove", argv[1]) == 0) {
			SBA_removeStatusBarImage(argv[2]);
			usage = 0;
		} else if (argc == 2 && strcmp("btstate", argv[1]) == 0) {
			int enabled = SBA_getBluetoothEnabled();
			printf("Bluetoot enabled: %u\n", enabled);
			usage = 0;
		} else if (argc == 2 && strcmp("bton", argv[1]) == 0) {
			SBA_setBluetoothEnabled(1);
			printf("Set Bluetooth enabled: YES\n");
			usage = 0;
		} else if (argc == 2 && strcmp("btoff", argv[1]) == 0) {
			SBA_setBluetoothEnabled(0);
			printf("Set Bluetooth enabled: NO\n");
			usage = 0;
		}
	}
	
	if (usage) {
		printf("Usage: %s [ btstate | bton | btoff | add StatuBarImageName | remove StatuBarImageName]\n", argv[0]);
		return -1;
	} 
	return 0;
}

