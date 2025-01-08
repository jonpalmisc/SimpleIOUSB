#include "Common.h"

#include <stdlib.h>

char const *GetStringDescriptorAlloc(SIClient *client, uint8_t index)
{
    SIStringDescriptorBuffer desc;
    SITransferResult result = SIGetDescriptor(client, kSIDescriptorTypeString, index, &desc, sizeof(desc));
    if (result.error != kIOReturnSuccess)
        return NULL;

    char *ascii = malloc(0x100);
    if (!ascii)
        return NULL;

    if (!SIDecodeStringDescriptor((SIStringDescriptor *)desc, ascii, 0x100)) {
        free(ascii);
        return NULL;
    }

    return ascii;
}

int DumpInfo(SIClient *client)
{
    SIDeviceDescriptor device = { 0 };
    SITransferResult result = SIGetDescriptor(client, kSIDescriptorTypeDevice, 0,
        &device, sizeof(device));
    if (result.error != kIOReturnSuccess || result.length != sizeof(device)) {
        fprintf(stderr, "Failed to get device descriptor. (%#x)\n", result.error);
        return 0;
    }

    char const *manufacturer = GetStringDescriptorAlloc(client, device.iManufacturer);
    char const *product = GetStringDescriptorAlloc(client, device.iProduct);

    puts("Device:");
    printf("  bcdUSB:             %#x\n", device.bcdUSB);
    printf("  bDeviceClass:       %#x\n", device.bDeviceClass);
    printf("  bDeviceSubClass:    %#x\n", device.bDeviceSubClass);
    printf("  bDeviceProtocol:    %#x\n", device.bDeviceProtocol);
    printf("  bMaxPacketSize:     %#x\n", device.bMaxPacketSize);
    printf("  idVendor:           %#x\n", device.idVendor);
    printf("  idProduct:          %#x\n", device.idProduct);
    printf("  bcdDevice:          %#x\n", device.bcdDevice);
    printf("  iManufacturer:      %#x  # %s\n", device.iManufacturer, manufacturer);
    printf("  iProduct:           %#x  # %s\n", device.iProduct, product);
    printf("  iSerialNumber:      %#x\n", device.iSerialNumber);
    printf("  bNumConfigurations: %#x\n", device.bNumConfigurations);

    size_t numPipes = 0;
    SIPipeProps pipes[kSIPipesMax];
    IOReturn ret = SIGetAllPipes(client, pipes, &numPipes);
    if (ret != kIOReturnSuccess) {
        fprintf(stderr, "Failed to get pipes. (%#x)\n", ret);
        return 0;
    }

    for (size_t i = 0; i < numPipes; ++i) {
        SIPipeProps pipe = pipes[i];
        printf("Pipe_%zu:\n", i);
        printf("  direction: %#x\n", pipe.direction);
        printf("  endpoint:  %#x\n", pipe.endpoint);
        printf("  type:      %#x\n", pipe.type);
        printf("  max:       %#x\n", pipe.max);
        printf("  interval:  %#x\n", pipe.interval);
    }

    for (int i = 0; i < device.bNumConfigurations; ++i) {
        union {
            uint8_t pad[0x200];
            SIConfigDescriptor config;
        } buf;

        result = SIGetDescriptor(client, kSIDescriptorTypeConfig, i, &buf, sizeof(buf));
        if (result.error != kIOReturnSuccess) {
            fprintf(stderr, "Failed to get configuration %d descriptor. (%#x)\n", i, result.error);
            return 0;
        }

        printf("Config_%d:\n", i);
        printf("  wTotalLength: %#x\n", buf.config.wTotalLength);
        printf("  bNumInterfaces: %#x\n", buf.config.bNumInterfaces);
        printf("  bConfigurationValue: %#x\n", buf.config.bConfigurationValue);
        printf("  iConfiguration: %#x\n", buf.config.iConfiguration);
        printf("  bmAttributes: %#x\n", buf.config.bmAttributes);
        printf("  bMaxPower: %#x\n", buf.config.bMaxPower);

        for (int j = 0; j < buf.config.bNumInterfaces; ++j) {
            SIInterfaceDescriptor *interface = (SIInterfaceDescriptor *)(&buf.config + 1) + j;

            printf("  Interface_%d:\n", j);
            printf("    bInterfaceNumber: %#x\n", interface->bInterfaceNumber);
            printf("    bAlternateSetting: %#x\n", interface->bAlternateSetting);
            printf("    bNumEndpoints: %#x\n", interface->bNumEndpoints);
            printf("    bInterfaceClass: %#x\n", interface->bInterfaceClass);
            printf("    bInterfaceSubClass: %#x\n", interface->bInterfaceSubClass);
            printf("    bInterfaceProtocol: %#x\n", interface->bInterfaceProtocol);
            printf("    iInterface: %#x\n", interface->iInterface);

            for (int k = 0; k < interface->bNumEndpoints; ++k) {
                SIEndpointDescriptor *endpoint = (SIEndpointDescriptor *)(interface + 1) + k;

                printf("  Endpoint_%d:\n", k);
                printf("      bEndpointAddress: %#x\n", endpoint->bEndpointAddress);
                printf("      bmAttributes: %#x\n", endpoint->bmAttributes);
                printf("      wMaxPacketSize: %#x\n", endpoint->wMaxPacketSize);
                printf("      bInterval: %#x\n", endpoint->bInterval);
            }
        }
    }

    return 1;
}

int main(int argc, char const **argv)
{
    (void)argc;
    (void)argv;

    SIClient *client = SIClientCreate();
    IOReturn ret = SIConnect(client, kUSBVendorIDApple, kUSBProductIDAppleRecovery);
    if (ret != kIOReturnSuccess) {
        fprintf(stderr, "Failed to connect. (%#x)\n", ret);
        return EXIT_FAILURE;
    }

    if (!DumpInfo(client))
        return EXIT_FAILURE;

    SIClientDestroy(client);
    return EXIT_SUCCESS;
}
