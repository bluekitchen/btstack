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
 *  SpringBoardAccess.h
 *
 *  Allows to add and remove images to/from status bar
 *
 *  The images must be installed in:
 *  /System/Library/CoreServices/SpringBoard.app/
 * 
 *  Only the base name is required, e.g., Bluetooth instead of Default_Bluetooth.png
 *  Created by Matthias Ringwald on 9/20/09.
 *
 */
#ifndef __SPRINGBOARDACCESS_H
#define __SPRINGBOARDACCESS_H

#define SBA_MessagePortName "SpringBoardAccess"

#define SBAC_nop
#define SBAC_addStatusBarImage 1
#define SBAC_removeStatusBarImage 2
#define SBAC_getBluetoothEnabled 3
#define SBAC_setBluetoothEnabled 4

/**
 * Enables named status bar icon in Springboard
 * @returns CFMessagePortSendRequest error: 0 = ok
 */
int SBA_addStatusBarImage(char *name);

/**
 * Disables named status bar icon in Springboard
 * @returns CFMessagePortSendRequest error: 0 = ok
 */
int SBA_removeStatusBarImage(char *name);

/**
 * Get Bluetoot enabled property
 * @returns < 0 error: 0 = OFF, 1 = ON
 */
int SBA_getBluetoothEnabled();

/**
 * Set Bluetooth enable property: 0 for OFF, otherwise on
 * @returns CFMessagePortSendRequest error: 0 = ok
 */
int SBA_setBluetoothEnabled(int on);

/**
 * Tests if SpringBoardAccess server is available
 */
int SBA_available();

#endif // __SPRINGBOARDACCESS_H
