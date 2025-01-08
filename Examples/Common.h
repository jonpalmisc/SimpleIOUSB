#pragma once

#include "SimpleIOUSB.h"

#include <ctype.h>
#include <stdio.h>

#define kUSBVendorIDApple 0x5ac
#define kUSBProductIDAppleRecovery 0x1281

void HexDump(void const *buf, size_t len)
{
    uint8_t const *data = buf;

    printf("\x1b[32m");
    for (size_t i = 0; i < len; i += 16) {
        printf("  %08zx: ", i);

        for (size_t j = 0; j < 16; ++j) {
            if (i + j < len) {
                printf("%02x ", data[i + j]);
            } else {
                printf("   ");
            }
        }
        printf("  ");

        for (size_t j = 0; j < 16; ++j) {
            if (i + j < len) {
                int c = data[i + j];
                printf("%c", (isprint(c) && c != ' ') ? c : '.');
            }
        }
        printf("\n");
    }

    printf("\x1b[0m");
}

void PrintSerial(SIClient *client)
{
    SIDeviceDescriptor deviceDesc = { 0 };
    SITransferResult result = SIGetDescriptor(client, kSIDescriptorTypeDevice, 0,
        &deviceDesc, sizeof(deviceDesc));
    if (result.error != kIOReturnSuccess || result.length != sizeof(deviceDesc)) {
        fprintf(stderr, "Failed to get device descriptor! (%#x)\n", result.error);
        return;
    }

    SIStringDescriptorBuffer serialDesc;
    result = SIGetDescriptor(client, kSIDescriptorTypeString, deviceDesc.iSerialNumber,
        &serialDesc, sizeof(serialDesc));
    if (result.error != kIOReturnSuccess) {
        fprintf(stderr, "Failed to get serial string descriptor! (%#x)\n", result.error);
        return;
    }

    HexDump(serialDesc, result.length);

    char serialAscii[0x100];
    if (!SIDecodeStringDescriptor((SIStringDescriptor *)serialDesc, serialAscii, sizeof(serialAscii))) {
        fputs("Failed to decode descriptor!", stderr);
        return;
    }

    puts(serialAscii);
}
