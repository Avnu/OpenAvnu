//
//  OutlineViewController.h
//  AVB Viewer
//
//  Created by John Gildred on 10/29/13.
//  Copyright (c) 2013 AVB.io. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <AudioVideoBridging/AudioVideoBridging.h>
#import "AvbDevice.h"

@interface OutlineViewController : NSObject <NSOutlineViewDelegate, NSOutlineViewDataSource, AVB17221EntityDiscoveryDelegate>

@property (weak) IBOutlet NSOutlineView *outlineView;
@property (copy) NSMutableArray *avbDevices;
@property (copy) AVBInterface *avbInterface;

@property (weak) IBOutlet NSImageView *detailsImage;
@property (weak) IBOutlet NSTextField *detailsName;
@property (weak) IBOutlet NSTextField *detailsDeviceId;
@property (weak) IBOutlet NSTextField *detailsModelId;
@property (weak) IBOutlet NSTextField *detailsVendorId;
@property (weak) IBOutlet NSTextField *detailsIsLocal;
@property (weak) IBOutlet NSTextField *detailsIsTalker;
@property (weak) IBOutlet NSTextField *detailsIsListener;
@property (weak) IBOutlet NSTextField *detailsIsController;
@property (weak) IBOutlet NSTextField *detailsTalkerStreams;
@property (weak) IBOutlet NSTextField *detailsListenerStreams;
@property (weak) IBOutlet NSTextField *detailsMacAddresses;
@property (weak) IBOutlet NSTextField *detailsTalkerType;
@property (weak) IBOutlet NSTextField *detailsListenerType;

- (void)updateDeviceList:(NSNotification *)notification;
- (IBAction)add:(id)sender;
- (IBAction)remove:(id)sender;
- (IBAction)refresh:(id)sender;
- (IBAction)changedSelection:(id)sender;

@end
