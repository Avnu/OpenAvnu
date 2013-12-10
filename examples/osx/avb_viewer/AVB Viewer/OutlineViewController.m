//
//  OutlineViewController.m
//  AVB Viewer
//
//  Created by John Gildred on 10/29/13.
//  Copyright (c) 2013 AVB.io. All rights reserved.
//

#import "OutlineViewController.h"

#define kIconImageSize          16.0

@implementation OutlineViewController

NSString *avbDevicesChanged = @"AVBCHANGED";
NSString *interfaceName;

- (id)init {
    self = [super init];
    if (self) {
        _avbDevices = [[NSMutableArray alloc] init];
        
        // Test device insertion
        //AvbDevice *newDevice = [[AvbDevice alloc] initWithId: @"Test Switch"];
        //[newDevice addChild:[[AvbDevice alloc] initWithId: @"Test Talker"]];
        //[self.avbDevices addObject:newDevice];
        
        [self getLocalInterface];
        //[self createLocalEntity];  Not used yet
        [self resetDiscovery];
    }
    return self;
}

- (void)updateDetailView:(AvbDevice *)avbDevice {
    NSImage *image = avbDevice.image;
    [image setSize:NSMakeSize(64, 64)];
    [_detailsImage setImage:                 image];
    [_detailsName setStringValue:            avbDevice.name ? avbDevice.name : @"-"];
    [_detailsDeviceId setStringValue:        avbDevice.deviceId ? avbDevice.deviceId : @"-"];
    [_detailsModelId setStringValue:         avbDevice.modelId ? avbDevice.modelId : @"-"];
    [_detailsVendorId setStringValue:        avbDevice.vendorId ? avbDevice.vendorId : @"-"];
    [_detailsIsTalker setStringValue:        avbDevice.isTalker ? @"YES" : @"NO"];
    [_detailsIsListener setStringValue:      avbDevice.isListener ? @"YES" : @"NO"];
    [_detailsIsController setStringValue:    avbDevice.isController ? @"YES" : @"NO"];
    [_detailsTalkerStreams setStringValue:   avbDevice.talkerSources ? [NSString stringWithFormat:@"%u", avbDevice.talkerSources] : @"-"];
    [_detailsListenerStreams setStringValue: avbDevice.listenerSinks ? [NSString stringWithFormat:@"%u", avbDevice.listenerSinks] : @"-"];
    [_detailsTalkerType setStringValue:      avbDevice.talkerType ? avbDevice.talkerType : @"-"];
    [_detailsListenerType setStringValue:    avbDevice.listenerType ? avbDevice.listenerType : @"-"];
    AVBMACAddress *address = (avbDevice.macAddresses.count > 0) ? [avbDevice.macAddresses objectAtIndex:0] : [[AVBMACAddress alloc] init];
    [_detailsMacAddresses setStringValue:    [address.stringRepresentation substringFromIndex:(address.stringRepresentation.length - 17)]];
}

- (void)updateDeviceList:(NSNotification *)notification {

}

- (IBAction)add:(id)sender {
    NSLog ( @"Adding...");
    AvbDevice *device = [self.outlineView itemAtRow:[self.outlineView selectedRow]];
    if (device) {
        [device addChild:[[AvbDevice alloc] init]];
    }
    else {
        [self.avbDevices addObject:[[AvbDevice alloc] init]];
    }
    [self.outlineView reloadData];
}

- (IBAction)remove:(id)sender {
    NSLog ( @"Removing...");
    AvbDevice *device = [self.outlineView itemAtRow:[self.outlineView selectedRow]];
    AvbDevice *parent = [self.outlineView parentForItem:device];
    if (parent) {
        [parent removeChild:device];
    }
    else if (device) {
        [self.avbDevices removeObject:device];
    }
    [self.outlineView reloadData];
}

- (id)getLocalInterface {
    NSString *interfaceName;
    NSArray *interfaces = [AVBEthernetInterface supportedInterfaces];
    NSString *result = interfaces ? [interfaces componentsJoinedByString: @", "] : @"NONE";
    NSLog ( @"AVB interfaces: %@", result );
    
    if (interfaces && ([interfaces count] > 0)) {
        interfaceName = interfaces[0];
        NSString *capable = [AVBInterface isAVBEnabledOnInterfaceNamed: interfaceName] ? @"YES" : @"NO";
        NSLog ( @"Support AVB? %@", capable );
        NSString *enabled = [AVBInterface isAVBEnabledOnInterfaceNamed: interfaceName] ? @"YES" : @"NO";
        NSLog ( @"Enabled for AVB? %@", enabled );
    }
    return interfaces[0];
}

- (id)createLocalEntity {
    AVB17221EntityDiscovery *disco = [[AVB17221EntityDiscovery alloc] init];
    NSError *__autoreleasing err = [[NSError alloc] init];
    AVB17221Entity *entity = [[AVB17221Entity alloc] init];
    [disco addLocalEntity:entity error:&err];
    [disco setInterfaceName:@"en3"];
    [entity setTalkerStreamSources:1];
    [entity setEntityID:666666];
    [entity setLocalEntity:YES];
    AVB17221ACMPInterface *acmp = [[AVB17221ACMPInterface alloc] init];
    return acmp;
}

- (void)resetDiscovery {
    // Check interfaces and log result to the console
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(updateDeviceList:) name:avbDevicesChanged object:nil];
    _avbInterface = [[AVBInterface alloc] initWithInterfaceName: interfaceName];
    [self.avbInterface.entityDiscovery setDiscoveryDelegate:self];
    [self.avbInterface.entityDiscovery primeIterators];
    [self.avbInterface.entityDiscovery discoverEntities];
}

- (IBAction)refresh:(id)sender {
    [self.avbInterface.entityDiscovery setDiscoveryDelegate:self];
    [self.avbInterface.entityDiscovery primeIterators];
    [self.avbInterface.entityDiscovery discoverEntities];
}

- (IBAction)changedSelection:(id)sender {
    AvbDevice *device = [self.outlineView itemAtRow:[self.outlineView selectedRow]];
    [self updateDetailView:device];
}

#pragma mark NSOutlineView Data Source Methods

- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item {
    return !item ? [self.avbDevices count] : [[item children] count];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item {
    return !item ? NO : [[item children] count] != 0;
}

- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item {
    return !item ? [self.avbDevices objectAtIndex:index] : [[item children] objectAtIndex:index];
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item {
    if ([[tableColumn identifier] isEqualToString: @"name"]) {
        return [item name];
    }
    else {
        return @"";
    }
}

- (id)outlineView:(NSOutlineView *)outlineView viewForTableColumn:(NSTableColumn *)tableColumn item:(id)item {
    if ([[tableColumn identifier] isEqualToString: @"name"]) {
        NSTableCellView *result = [outlineView makeViewWithIdentifier:tableColumn.identifier owner:self];
        [result.textField setStringValue:[item name]];
        NSImage *image = [item image];
		[image setSize:NSMakeSize(64, 64)];
        [result.imageView setImage:image];
        return result;
    }
    else {
        return nil;
    }
}

- (void)outlineView:(NSOutlineView *)outlineView setObjectValue:(id)object forTableColumn:(NSTableColumn *)tableColumn byItem:(id)item {
    [(AvbDevice *)item setName:object];
}

// AVB Discovery Stuff

- (AvbDevice *)deviceExistsWithEntity:(AVB17221Entity *)entity {
    AvbDevice *matchedDevice;
    NSString *deviceId = [NSString stringWithFormat:@"0x00%llx", [entity entityID]];
    for (AvbDevice *device in self.avbDevices) {
        if ([device.deviceId isEqualToString: deviceId]) {
            matchedDevice = device;
        }
    }
    return matchedDevice;
};

- (void)updateDevicesWithEntity:(AVB17221Entity *)entity {
    AvbDevice *device = [self deviceExistsWithEntity:entity];
    if (device) {
        [device updateFromEntity:entity];
    }
    else {
        AvbDevice *newDevice = [[AvbDevice alloc] initWithEntity:entity];
        [self.avbDevices addObject: newDevice];
    }
}

- (void)didAddRemoteEntity:(AVB17221Entity *)newEntity on17221EntityDiscovery:(AVB17221EntityDiscovery *)entityDiscovery {
    NSLog ( @"Found something remote: %@", newEntity );
    if (![self deviceExistsWithEntity:newEntity]) {
        AvbDevice *newDevice = [[AvbDevice alloc] initWithEntity:newEntity];
        [self.avbDevices addObject: newDevice];
        [[NSNotificationCenter defaultCenter] postNotificationName:avbDevicesChanged object:nil];
    }
};

- (void)didRemoveRemoteEntity:(AVB17221Entity *)oldEntity on17221EntityDiscovery:(AVB17221EntityDiscovery *)entityDiscovery {
    NSLog ( @"Lost something remote: %@", oldEntity );
    AvbDevice *device = [self deviceExistsWithEntity:oldEntity];
    if (device) {
        [self.avbDevices removeObject: device];
        [[NSNotificationCenter defaultCenter] postNotificationName:avbDevicesChanged object:nil];
    }
};

- (void)didRediscoverRemoteEntity:(AVB17221Entity *)entity on17221EntityDiscovery:(AVB17221EntityDiscovery *)entityDiscovery {
    NSLog ( @"Rediscovered something remote: %@", entity );
    [self updateDevicesWithEntity:entity];
    [[NSNotificationCenter defaultCenter] postNotificationName:avbDevicesChanged object:nil];
};

- (void)didUpdateRemoteEntity:(AVB17221Entity *)entity changedProperties:(AVB17221EntityPropertyChanged)changedProperties on17221EntityDiscovery:(AVB17221EntityDiscovery *)entityDiscovery {
    NSLog ( @"Updated something remote: %@", entity );
    [self updateDevicesWithEntity:entity];
    [[NSNotificationCenter defaultCenter] postNotificationName:avbDevicesChanged object:nil];
};

- (void)didAddLocalEntity:(AVB17221Entity *)newEntity on17221EntityDiscovery:(AVB17221EntityDiscovery *)entityDiscovery {
    NSLog ( @"Found something local: %@", newEntity );
    if (![self deviceExistsWithEntity:newEntity]) {
        AvbDevice *newDevice = [[AvbDevice alloc] initWithEntity:newEntity];
        [self.avbDevices addObject: newDevice];
        [[NSNotificationCenter defaultCenter] postNotificationName:avbDevicesChanged object:nil];
    }
};

- (void)didRemoveLocalEntity:(AVB17221Entity *)oldEntity on17221EntityDiscovery:(AVB17221EntityDiscovery *)entityDiscovery {
    NSLog ( @"Lost something local: %@", oldEntity );
    AvbDevice *device = [self deviceExistsWithEntity:oldEntity];
    if (device) {
        [self.avbDevices removeObject: device];
        [[NSNotificationCenter defaultCenter] postNotificationName:avbDevicesChanged object:nil];
    }
};

- (void)didRediscoverLocalEntity:(AVB17221Entity *)entity on17221EntityDiscovery:(AVB17221EntityDiscovery *)entityDiscovery {
    NSLog ( @"Rediscovered something local: %@", entity );
    [self updateDevicesWithEntity:entity];
    [[NSNotificationCenter defaultCenter] postNotificationName:avbDevicesChanged object:nil];
};

- (void)didUpdateLocalEntity:(AVB17221Entity *)entity changedProperties:(AVB17221EntityPropertyChanged)changedProperties on17221EntityDiscovery:(AVB17221EntityDiscovery *)entityDiscovery {
    NSLog ( @"Updated something local: %@", entity );
    [self updateDevicesWithEntity:entity];
    [[NSNotificationCenter defaultCenter] postNotificationName:avbDevicesChanged object:nil];
};

@end
