/*
 * Copyright (C) 2011 by Matthias Ringwald
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

// adapted from Ryan Petrich's Activator preferences bundle
// https://github.com/rpetrich/libactivator/blob/master/Preferences.m

#import <UIKit/UIKit.h>
#include <dlfcn.h>

#import "BluetoothTableViewAdapter.h"
#import "BluetoothController.h"

// instead of #import <Preferences/Preferences.h>, minimal class definition
@interface PSViewController : NSObject
- (id)initForContentSize:(CGSize)size;
- (id)navigationItem;
- (void)viewWillBecomeVisible:(void *)source;
- (void)viewWillAppear:(BOOL)animated;
- (void)viewDidDisappear:(BOOL)animated;
@end

@interface PSViewController (OS32)
@property (nonatomic, retain) UIView *view;
- (void)viewDidLoad;
@end

@interface UIDevice (OS32)
- (BOOL)isWildcat;
@end

@interface BluetoothPSViewController : PSViewController {
    BluetoothTableViewAdapter *tableViewAdapter;
    BluetoothController *bluetoothController;
    UIView *_wrapperView;   // for < 3.2
    UITableView *tableView;
    BOOL initialized;
}
@end

@implementation BluetoothPSViewController

- (id)initForContentSize:(CGSize)size
{
    // NSLog(@"initForContentSize");
    initialized = NO;
	if ([PSViewController instancesRespondToSelector:@selector(initForContentSize:)]) {
		if ((self = [super initForContentSize:size])) {
			CGRect frame;
			frame.origin.x = 0.0f;
			frame.origin.y = 0.0f;
			frame.size = size;
			_wrapperView = [[UIView alloc] initWithFrame:frame];
		}
		return self;
	}

	return [super init];
}

- (void)dealloc
{
    // NSLog(@"dealloc");
    
    [[BluetoothController sharedInstance] close];
    [[BluetoothController sharedInstance] setListener:nil];

	[tableViewAdapter release];
    tableView.dataSource = nil;
    tableView.delegate   = nil;
    [tableView release];
    
	[_wrapperView release];

    initialized = NO;

	[super dealloc];
}
 
- (UIView *)view
{
	return [super view] ? [super view] : _wrapperView;
}

-(void)myInit{
    
    if (initialized) return;
    
    initialized = YES;
    
    // NSLog(@"myInit");

    ((UINavigationItem*)[super navigationItem]).title = @"BTstack";
    
    UIView *view = [self view];

    tableView = [[UITableView alloc] initWithFrame:view.bounds style:UITableViewStyleGrouped];
    tableView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    
    tableViewAdapter = [[BluetoothTableViewAdapter alloc] initWithTableView:tableView];
    tableView.dataSource = tableViewAdapter;
    tableView.delegate   = tableViewAdapter;
    
    [view addSubview:tableView];
    
    [[BluetoothController sharedInstance] setListener:tableViewAdapter];
    [[BluetoothController sharedInstance] open];

    // register for backgrounding events on iOS 4+
    NSString ** pUIApplicationDidEnterBackgroundNotification  = nil;
    NSString ** pUIApplicationWillEnterForegroundNotification = nil;

    pUIApplicationDidEnterBackgroundNotification  = dlsym(RTLD_DEFAULT, "UIApplicationDidEnterBackgroundNotification");
    pUIApplicationWillEnterForegroundNotification = dlsym(RTLD_DEFAULT, "UIApplicationWillEnterForegroundNotification"); 

    if (!pUIApplicationDidEnterBackgroundNotification)  return;
    if (!pUIApplicationWillEnterForegroundNotification) return;

    [[NSNotificationCenter defaultCenter] 
         addObserver:self 
         selector:@selector(didEnterBackground:) 
         name:*pUIApplicationDidEnterBackgroundNotification
         object:nil];

    [[NSNotificationCenter defaultCenter] 
         addObserver:self 
         selector:@selector(willEnterForeground:) 
         name:*pUIApplicationWillEnterForegroundNotification
         object:nil];
}

-(void)didEnterBackground:(id)object{
    // NSLog(@"didEnterBackground");
    // close connection to BTdaemon
    [[BluetoothController sharedInstance] close];
}

-(void)willEnterForeground:(id)object{
    // NSLog(@"willEnterForeground");
    // open connection to BTdaemon
    [self myInit];
    [[BluetoothController sharedInstance] open];
}


- (void)viewWillAppear:(BOOL)animated
{
    // NSLog(@"viewWillAppear");
    [super viewWillAppear:animated];
    [self myInit];
    [[BluetoothController sharedInstance] open];
}

- (void)viewWillBecomeVisible:(void *)source
{
    // NSLog(@"viewWillBecomeVisible %@", source);
    [super viewWillBecomeVisible:source];
    [self myInit];
}

-(void)viewDidDisappear:(BOOL)animated {
    // NSLog(@"viewDidDisappear");
    // close connection to BTdaemon
    [[BluetoothController sharedInstance] close];
    [super viewDidDisappear:animated];
}

-(void)viewDidLoad
{
    // NSLog(@"viewDidLoad");
	[super viewDidLoad];
    [self myInit];
}
@end
