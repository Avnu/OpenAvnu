//
//  AppDelegate.h
//  AVB Viewer
//
//  Created by John Gildred on 10/29/13.
//  Copyright (c) 2013 AVB.io. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface AppDelegate : NSObject <NSApplicationDelegate>

@property (assign) IBOutlet NSWindow *window;
@property (weak) IBOutlet NSSplitView *splitview;

@end
