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
