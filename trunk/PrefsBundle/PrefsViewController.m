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

#import <Preferences/Preferences.h>
#import <UIKit/UIKit.h>

#import "BluetoothTableViewAdapter.h"
#import "BluetoothController.h"

@interface PSViewController (OS32)
@property (nonatomic, retain) UIView *view;
- (void)viewDidLoad;
@end

@interface UIDevice (OS32)
- (BOOL)isWildcat;
@end

@interface BluetoothPSViewController : PSViewController
    BluetoothTableViewAdapter *tableViewAdapter;
    BluetoothController *bluetoothController;
    UIView *_wrapperView;   // for < 3.2
    UITableView *tableView;
    BOOL initialized;
@end

@implementation BluetoothPSViewController

- (id)initForContentSize:(CGSize)size
{
    NSLog(@"initForContentSize");
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
    NSLog(@"dealloc");
    
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
    
    NSLog(@"myInit");

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
}

-(void)viewDidLoad
{
    NSLog(@"viewDidLoad");
	[super viewDidLoad];
    [self myInit];
}

- (void)viewWillBecomeVisible:(void *)source
{
    NSLog(@"viewWillBecomeVisible %@", source);
    [self myInit];
	[super viewWillBecomeVisible:source];
}

- (void)viewWillAppear:(BOOL)animated
{
    NSLog(@"viewWillAppear");
    [self myInit];
}
@end
