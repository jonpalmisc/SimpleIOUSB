#include "Common.h"

#include <stdlib.h>

int main(int argc, char const **argv)
{
    (void)argc;
    (void)argv;

    puts("Connecting...");
    SIClient *client = SIClientCreate();
    IOReturn ret = SIConnect(client, kUSBVendorIDApple, kUSBProductIDAppleRecovery);
    if (ret != kIOReturnSuccess) {
        fprintf(stderr, "Failed to connect. (%#x)\n", ret);
        return EXIT_FAILURE;
    }

    puts("Getting serial number...");
    PrintSerial(client);

    SIClientDestroy(client);
    return EXIT_SUCCESS;
}
