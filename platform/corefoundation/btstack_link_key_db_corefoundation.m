/*
 * Copyright (C) 2009-2012 by Matthias Ringwald
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
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
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
 * Please inquire about commercial licensing options at contact@bluekitchen-gmbh.com
 *
 */

#define __BTSTACK_FILE__ "btstack_link_key_db_corefoundation.c"

#include "btstack_link_key_db_corefoundation.h"
#include "btstack_debug.h"

#import <Foundation/Foundation.h>

#define BTdaemonID         "ch.ringwald.btdaemon"
#define BTDaemonPrefsPath  "Library/Preferences/ch.ringwald.btdaemon.plist"

#define DEVICES_KEY        "devices"
#define PREFS_LINK_KEY     @"LinkKey"

static NSMutableDictionary *remote_devices  = nil;

// Device info
static void db_open(void){
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    
    // NSUserDefaults didn't work
    // 
	// NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	// NSDictionary * dict = [defaults persistentDomainForName:BTdaemonID];
	
    // NSDictionary * dict = (NSDictionary*) CFPreferencesCopyAppValue(CFSTR(DEVICES_KEY), CFSTR(BTdaemonID));
    NSDictionary * dict;
    dict = (NSDictionary*) CFPreferencesCopyAppValue(CFSTR(DEVICES_KEY), CFSTR(BTdaemonID));
    remote_devices = [[NSMutableDictionary alloc] initWithCapacity:([dict count]+5)];
    
	// copy entries
	for (id key in dict) {
		NSDictionary *value = [dict objectForKey:key];
		NSMutableDictionary *deviceEntry = [NSMutableDictionary dictionaryWithCapacity:[value count]];
		[deviceEntry addEntriesFromDictionary:value];
		[remote_devices setObject:deviceEntry forKey:key];
	}
        
    log_info("read prefs for %u devices\n", (unsigned int) [dict count]);
    
    [pool release];
}

static void db_set_local_bd_addr(bd_addr_t bd_addr){
}

static void db_synchronize(void){
    log_info("stored prefs for %u devices\n", (unsigned int) [remote_devices count]);
    
    // 3 different ways
    
    // Core Foundation
    CFPreferencesSetValue(CFSTR(DEVICES_KEY), (CFPropertyListRef) remote_devices, CFSTR(BTdaemonID), kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
    CFPreferencesSynchronize(CFSTR(BTdaemonID), kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
    
    // NSUserDefaults didn't work
    // 
	// NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    // [defaults setPersistentDomain:remote_devices forName:BTdaemonID];
    // [defaults synchronize];
}

static void db_close(void){ 
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    // don't call db_synchronize();
    // a) we're calling db_synchronize() after each change already
    // b) db_close is called during the SIGINT handler which causes a corrupt prefs file
    
    [remote_devices release];
    remote_devices = nil;
    [pool release];
}

static NSString * stringForAddress(bd_addr_t addr) {
	return [NSString stringWithFormat:@"%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2],
			addr[3], addr[4], addr[5]];
}
                             
static void set_value(bd_addr_t bd_addr, NSString *key, id value){
	NSString *devAddress = stringForAddress(bd_addr);
	NSMutableDictionary * deviceDict = [remote_devices objectForKey:devAddress];
	if (!deviceDict){
		deviceDict = [NSMutableDictionary dictionaryWithCapacity:3];
		[remote_devices setObject:deviceDict forKey:devAddress];
	}
    [deviceDict setObject:value forKey:key];
    db_synchronize();
}

static void delete_value(bd_addr_t bd_addr, NSString *key){
	NSString *devAddress = stringForAddress(bd_addr);
	NSMutableDictionary * deviceDict = [remote_devices objectForKey:devAddress];
	[deviceDict removeObjectForKey:key];
    db_synchronize();

}

static id get_value(bd_addr_t bd_addr, NSString *key){
	NSString *devAddress = stringForAddress(bd_addr);
	NSMutableDictionary * deviceDict = [remote_devices objectForKey:devAddress];
    if (!deviceDict) return nil;
    return [deviceDict objectForKey:key];
}

static int get_link_key(bd_addr_t bd_addr, link_key_t link_key, link_key_type_t * link_key_type) {
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    NSData *linkKey = get_value(bd_addr, PREFS_LINK_KEY);
    if ([linkKey length] == LINK_KEY_LEN){
        memcpy(link_key, [linkKey bytes], LINK_KEY_LEN);
        if (link_key_type){
            *link_key_type = COMBINATION_KEY;
        }
    }
    [pool release];
    return (linkKey != nil);
}

static void put_link_key(bd_addr_t bd_addr, link_key_t link_key, link_key_type_t link_key_type){
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
	NSData *linkKey = [NSData dataWithBytes:link_key length:16];
    set_value(bd_addr, PREFS_LINK_KEY, linkKey);
    [pool release];
}

static void delete_link_key(bd_addr_t bd_addr){
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    delete_value(bd_addr, PREFS_LINK_KEY);
    [pool release];
}

const btstack_link_key_db_t btstack_link_key_db_cocoa = {
    db_open,
    db_set_local_bd_addr,
    db_close,
    get_link_key,
    put_link_key,
    delete_link_key,
};

const btstack_link_key_db_t * btstack_link_key_db_corefoundation_instance(void){
    return &btstack_link_key_db_cocoa;
}
