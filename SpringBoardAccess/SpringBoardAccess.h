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
#pragma once

#define SBA_MessagePortName "SpringBoardAccess"

#define SBAC_nop
#define SBAC_addStatusBarImage 1
#define SBAC_removeStatusBarImage 2

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
