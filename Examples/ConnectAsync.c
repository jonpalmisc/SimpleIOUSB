#include "Common.h"

#include <CoreFoundation/CFRunLoop.h>

#include <mach/mach_error.h>

#include <stdio.h>
#include <stdlib.h>

void DeviceConnected(SIClient *client)
{
    SIDeviceDescriptor deviceDesc = { 0 };
    SITransferResult result = SIGetDescriptor(client, kSIDescriptorTypeDevice, 0, &deviceDesc, sizeof(deviceDesc));
    if (result.error != kIOReturnSuccess || result.length != sizeof(deviceDesc)) {
        puts("Failed to get device descriptor!");
        return;
    }

    SIStringDescriptorBuffer serialDesc;
    result = SIGetDescriptor(client, kSIDescriptorTypeString, deviceDesc.iSerialNumber, &serialDesc, sizeof(serialDesc));
    if (result.error != kIOReturnSuccess) {
        puts("Failed to get serial string descriptor!");
        puts(mach_error_string(result.error));
        return;
    }

    HexDump(serialDesc, result.length);

    char serialAscii[0x100];
    if (!SIDecodeStringDescriptor((SIStringDescriptor *)serialDesc, serialAscii, sizeof(serialAscii))) {
        puts("Failed to decode descriptor!");
        return;
    }

    puts(serialAscii);

    SIClientDestroy(client);
}

void DeviceTerminated(uint64_t id)
{
    printf("Lost device with ID %#llx.\n", id);
}

int main(int argc, char const **argv)
{
    (void)argc;
    (void)argv;

    static SIAsyncCallbacks sCallbacks = {
        .connect = DeviceConnected,
        .disconnect = DeviceTerminated,
    };

    puts("Waiting for recovery devices...");
    IOReturn ret = SIConnectAsync(kUSBVendorIDApple, kUSBProductIDAppleRecovery, &sCallbacks);
    if (ret != kIOReturnSuccess) {
        printf("Failed to connect: %s\n", mach_error_string(ret));
        return EXIT_FAILURE;
    }

    CFRunLoopRun();

    return EXIT_SUCCESS;
}
