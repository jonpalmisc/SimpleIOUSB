#include "Common.h"

#include <CoreFoundation/CFRunLoop.h>

#include <stdio.h>
#include <stdlib.h>

void DeviceConnected(SIClient *client)
{
    PrintSerial(client);
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
        fprintf(stderr, "Failed to connect. (%#x)\n", ret);
        return EXIT_FAILURE;
    }

    CFRunLoopRun();

    return EXIT_SUCCESS;
}
