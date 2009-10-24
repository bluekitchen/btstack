//
//  EAGLViewController.m
//
//  Created by Matthias Ringwald on 10/23/09.
//

#import "EAGLViewController.h"
#import "EAGLView.h"

@implementation EAGLViewController
// The simplest UIViewController subclass just overrides -loadView.
- (void)loadView
{
	UIView *view = [[EAGLView alloc] initWithFrame:[[UIScreen mainScreen] applicationFrame]];
    self.view = view;
    [view release];
	
	self.title = @"BTstack WiiMote Demo";

	// disable back button
	self.navigationItem.hidesBackButton = TRUE; 
}
// Override to allow orientations other than the default portrait orientation.
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
	return YES;
}

@end
