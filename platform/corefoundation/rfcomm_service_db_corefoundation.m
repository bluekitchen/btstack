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
#include "rfcomm_service_db.h"
#include "btstack_debug.h"

#import <Foundation/Foundation.h>

#define BTdaemonID         "ch.ringwald.btdaemon"
#define BTDaemonPrefsPath  "Library/Preferences/ch.ringwald.btdaemon.plist"

#define MAX_RFCOMM_CHANNEL_NR 30

#define RFCOMM_SERVICES_KEY "rfcommServices"
#define PREFS_CHANNEL      @"channel"
#define PREFS_LAST_USED    @"lastUsed"

static NSMutableDictionary *rfcomm_services = nil;

// Device info
static void db_open(void){

    if (rfcomm_services) return;

	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    
    NSDictionary * dict = (NSDictionary*) CFPreferencesCopyAppValue(CFSTR(RFCOMM_SERVICES_KEY), CFSTR(BTdaemonID));
    rfcomm_services = [[NSMutableDictionary alloc] initWithCapacity:([dict count]+5)];
    
	// copy entries
	for (id key in dict) {
		NSDictionary *value = [dict objectForKey:key];
		NSMutableDictionary *serviceEntry = [NSMutableDictionary dictionaryWithCapacity:[value count]];
		[serviceEntry addEntriesFromDictionary:value];
		[rfcomm_services setObject:serviceEntry forKey:key];
	}
    [pool release];
}

static void db_synchronize(void){
    // 3 different ways
    
    // Core Foundation
    CFPreferencesSetValue(CFSTR(RFCOMM_SERVICES_KEY), (CFPropertyListRef) rfcomm_services, CFSTR(BTdaemonID), kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
    CFPreferencesSynchronize(CFSTR(BTdaemonID), kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
    
    // NSUserDefaults didn't work
    // 
	// NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    // [defaults setPersistentDomain:remote_devices forName:BTdaemonID];
    // [defaults synchronize];
}
                      
// MARK: PERSISTENT RFCOMM CHANNEL ALLOCATION

static int firstFreeChannelNr(void){
    BOOL channelUsed[MAX_RFCOMM_CHANNEL_NR+1];
    int i;
    for (i=0; i<=MAX_RFCOMM_CHANNEL_NR ; i++) channelUsed[i] = NO;
    channelUsed[0] = YES;
    channelUsed[1] = YES; // preserve channel #1 for testing
    for (NSDictionary * serviceEntry in [rfcomm_services allValues]){
        int channel = [(NSNumber *) [serviceEntry objectForKey:PREFS_CHANNEL] intValue];
        channelUsed[channel] = YES;
    }
    for (i=0;i<=MAX_RFCOMM_CHANNEL_NR;i++) {
        if (channelUsed[i] == NO) return i;
    }
    return -1;
}

static void deleteLeastUsed(void){
    NSString * leastUsedName = nil;
    NSDate *   leastUsedDate = nil;
    for (NSString * serviceName in [rfcomm_services allKeys]){
        NSDictionary *service = [rfcomm_services objectForKey:serviceName];
        NSDate *serviceDate = [service objectForKey:PREFS_LAST_USED];
        if (leastUsedName == nil || [leastUsedDate compare:serviceDate] == NSOrderedDescending) {
            leastUsedName = serviceName;
            leastUsedDate = serviceDate;
            continue;
        }
    }
    if (leastUsedName){
        // NSLog(@"removing %@", leastUsedName);
        [rfcomm_services removeObjectForKey:leastUsedName];
    }
}

static void addService(NSString * serviceName, int channel){
    NSMutableDictionary * serviceEntry = [NSMutableDictionary dictionaryWithCapacity:2];
    [serviceEntry setObject:[NSNumber numberWithInt:channel] forKey:PREFS_CHANNEL];
    [serviceEntry setObject:[NSDate date] forKey:PREFS_LAST_USED];
    [rfcomm_services setObject:serviceEntry forKey:serviceName];
}

uint8_t rfcomm_service_db_channel_for_service(const char *serviceName){

    db_open();

    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    NSLog(@"persistent_rfcomm_channel for %s", serviceName);

    // find existing entry
    NSString *serviceString = [NSString stringWithUTF8String:serviceName];
    NSMutableDictionary *serviceEntry = [rfcomm_services objectForKey:serviceString];
    if (serviceEntry){
        // update timestamp
        [serviceEntry setObject:[NSDate date] forKey:PREFS_LAST_USED];
        
        db_synchronize();
        
        return [(NSNumber *) [serviceEntry objectForKey:PREFS_CHANNEL] intValue];
    }
    // free channel exist?
    int channel = firstFreeChannelNr();
    if (channel < 0){
        // free channel
        deleteLeastUsed();
        channel = firstFreeChannelNr();
    }
    addService(serviceString, channel);

    db_synchronize();

    [pool release];
    
    return channel;
}
