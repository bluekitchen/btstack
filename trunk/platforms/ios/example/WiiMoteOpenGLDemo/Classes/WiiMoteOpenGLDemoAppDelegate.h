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
//  WiiMoteOpenGLDemoAppDelegate.h
//
//  Created by Matthias Ringwald on 8/2/09.
//

#import <UIKit/UIKit.h>
#import <BTstack/BTstackManager.h>
#import <BTstack/BTDiscoveryViewController.h>

@class EAGLView;
@class BTstackManager;
@class BTDiscoveryViewController;

@interface WiiMoteOpenGLDemoAppDelegate :
		   NSObject<UIApplicationDelegate, BTstackManagerDelegate, BTstackManagerListener,BTDiscoveryDelegate>{

    UIWindow *window;
	UIViewController *glViewControl;
	BTDiscoveryViewController* discoveryView;
	UINavigationController *navControl;
    EAGLView *glView;
	UILabel  *status;
}

- (void)startDemo;

@property (nonatomic, retain) UIWindow *window;
@property (nonatomic, retain) UINavigationController *navControl;
@property (nonatomic, retain) UIViewController *glViewControl;
@property (nonatomic, retain) EAGLView *glView;

@end

