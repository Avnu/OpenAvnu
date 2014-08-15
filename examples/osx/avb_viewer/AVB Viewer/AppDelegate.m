//
//  AppDelegate.m
//  AVB Viewer
//
//  Created by John Gildred on 10/29/13.
//  Copyright (c) 2013 AVB.io. All rights reserved.
//

#import "AppDelegate.h"

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    [self.splitview setPosition:250 ofDividerAtIndex:0];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender
{
	return YES;
}

@end
