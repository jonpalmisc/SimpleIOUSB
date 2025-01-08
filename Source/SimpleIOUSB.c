//
//  SimpleIOUSB.c
//
//  Copyright (c) 2022-2025 Jon Palmisciano. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//  2. Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
//  3. Neither the name of the copyright holder nor the names of its
//     contributors may be used to endorse or promote products derived from
//     this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
//  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//  CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE)
//  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//  POSSIBILITY OF SUCH DAMAGE.
//

#include "SimpleIOUSB.h"

#include <CoreFoundation/CFNumber.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

// Set to 1 below (or override in compile flags) for additional debug output.
#ifndef SI_CONFIG_DEBUG
#define SI_CONFIG_DEBUG 1
#endif

#if SI_CONFIG_DEBUG
#include <stdio.h>

#define SIDebug(...)                                                            \
    do {                                                                        \
        SIDebugWithContext(__FILE_NAME__, __LINE__, __FUNCTION__, __VA_ARGS__); \
    } while (0)

static void SIDebugWithContext(char const *file, int line, char const *func, char const *fmt, ...) __printflike(4, 5)
{
    fprintf(stderr, "\x1b[34m%s(%s:%d): ", func, file, line);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\x1b[0m\n");
}
#else
#define SIDebug(...) \
    do {             \
    } while (0)
#endif

SIClient *SIClientCreate(void)
{
    SIClient *client = malloc(sizeof(SIClient));
    return client;
}

void SIClientDestroy(SIClient *client)
{
    SIDebug("Destroying client %p...", (void *)client);

    if (client->interface) {
        SIDebug("Closing USB interface...");

        IOReturn ret = (*client->interface)->USBInterfaceClose(client->interface);
        if (ret != kIOReturnSuccess)
            SIDebug("Failed to close interface. (%#x)", ret);

        (*client->interface)->Release(client->interface);
    }

    if (client->device) {
        SIDebug("Closing USB device...");

        IOReturn ret = (*client->device)->USBDeviceClose(client->device);
        if (ret != kIOReturnSuccess)
            SIDebug("Failed to close device. (%#x)", ret);

        (*client->device)->Release(client->device);
    }

    free(client);
}

static CFMutableDictionaryRef SIServiceMatchingUSBDevice(uint16_t vendorID, uint16_t productID)
{
    CFMutableDictionaryRef query = IOServiceMatching(kIOUSBDeviceClassName);

    CFNumberRef cfVendorID = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &vendorID);
    CFDictionarySetValue(query, CFSTR(kUSBVendorID), cfVendorID);
    CFNumberRef cfProductID = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &productID);
    CFDictionarySetValue(query, CFSTR(kUSBProductID), cfProductID);

    // Adding these to the dictionary increased their reference counts, drop
    // our local reference so their lifetime is tied to the dictionary.
    CFRelease(cfVendorID);
    CFRelease(cfProductID);

    return query;
}

static IOReturn SIQueryInterface(io_service_t service, CFUUIDRef pluginType,
    CFUUIDRef interfaceType, LPVOID *interface)
{
    SInt32 score = 0;
    IOCFPlugInInterface **plugin = NULL;
    IOReturn ret = IOCreatePlugInInterfaceForService(service, pluginType, kIOCFPlugInInterfaceID, &plugin, &score);
    IOObjectRelease(service);
    if (ret != kIOReturnSuccess || !plugin) {
        SIDebug("Failed to create plugin instance. (%#x)", ret);
        return ret;
    }

    HRESULT ok = (*plugin)->QueryInterface(plugin, CFUUIDGetUUIDBytes(interfaceType), interface);
    (*plugin)->Release(plugin);
    if (ok != S_OK || !interface) {
        SIDebug("Failed to query plugin interface. (%#x)", ok);
        return kIOReturnError;
    }

    return kIOReturnSuccess;
}

static IOReturn SIGetDeviceHandle(io_service_t service, SIDeviceHandle *deviceOut)
{
    *deviceOut = NULL;
    SIDebug("Trying to acquire device handle...");

    SIDeviceInterface **device = NULL;
    IOReturn ret = SIQueryInterface(service, kIOUSBDeviceUserClientTypeID, kIOUSBDeviceInterfaceID245, (LPVOID *)&device);
    if (ret != kIOReturnSuccess || !device) {
        SIDebug("Failed to query device interface. (%#x)", ret);
        return ret;
    }

    if ((ret = (*device)->USBDeviceOpenSeize(device)) != kIOReturnSuccess) {
        SIDebug("Failed to open device. (%#x)", ret);
        goto L_failed;
    }
    if ((ret = (*device)->SetConfiguration(device, 1)) != kIOReturnSuccess) {
        SIDebug("Failed to set configuration. (%#x)", ret);
        goto L_failed;
    }

    *deviceOut = device;
    return ret;

L_failed:
    if (device) {
        (*device)->USBDeviceClose(device);
        (*device)->Release(device);
    }

    return ret;
}

static IOReturn SIGetInterfaceHandle(SIDeviceHandle device, unsigned index, SIInterfaceHandle *interfaceOut)
{
    (void)index;

    // XXX: Right now, this is a lie, which is why we null-cast the parameter
    // above so the compiler doesn't call us out for this...
    SIDebug("Looking for interface with index %u...", index);

    static IOUSBFindInterfaceRequest sInterfaceReq = {
        .bInterfaceProtocol = kIOUSBFindInterfaceDontCare,
        .bInterfaceSubClass = kIOUSBFindInterfaceDontCare,
        .bAlternateSetting = kIOUSBFindInterfaceDontCare,
        .bInterfaceClass = kIOUSBFindInterfaceDontCare,
    };

    io_iterator_t ifaceIter = IO_OBJECT_NULL;
    IOReturn ret = (*device)->CreateInterfaceIterator(device, &sInterfaceReq, &ifaceIter);
    if (ret != kIOReturnSuccess) {
        SIDebug("Failed to create interface iterator. (%#x)", ret);
        return ret;
    }

    io_service_t service = IO_OBJECT_NULL;
    while ((service = IOIteratorNext(ifaceIter))) {
        // TODO: Index?

        SIInterfaceHandle interface = NULL;
        ret = SIQueryInterface(service, kIOUSBInterfaceUserClientTypeID,
            kIOUSBInterfaceInterfaceID245, (LPVOID *)&interface);
        if (ret != kIOReturnSuccess || !interface) {
            SIDebug("Failed to query interface interface. (%#x)", ret);
            return ret;
        }

        ret = (*interface)->USBInterfaceOpenSeize(interface);
        if (ret != kIOReturnSuccess) {
            SIDebug("Failed to open device. (%#x)", ret);
            return ret;
        }

        *interfaceOut = interface;
        break;
    }

    IOObjectRelease(ifaceIter);
    return ret;
}

static IOReturn SIClientInitWithService(SIClient *client, io_service_t service)
{
    uint64_t regID = -1;
    IOReturn ret = IORegistryEntryGetRegistryEntryID(service, &regID);
    if (ret != kIOReturnSuccess) {
        SIDebug("Failed to get registry entry ID for service %#x. (%#x)", service, ret);
        return ret;
    }

    SIDebug("Trying to initialize with service %#x/%#llx...", service, regID);

    SIDeviceHandle device = NULL;
    ret = SIGetDeviceHandle(service, &device);
    if (ret != kIOReturnSuccess || !device) {
        SIDebug("Failed to get device handle for service %#x. (%#x)", service, ret);
        return ret;
    }

    SIDebug("Acquired device handle successfully.");

    SIInterfaceHandle interface = NULL;
    ret = SIGetInterfaceHandle(device, 0, &interface);
    if (ret != kIOReturnSuccess || !interface) {
        SIDebug("Failed to get interface handle for service %#x. (%#x)", service, ret);

        (*device)->USBDeviceClose(device);
        (*device)->Release(device);

        return ret;
    }

    SIDebug("Acquired interface handle successfully.");
    SIDebug("Initializing client...");

    client->device = device;
    client->interface = interface;
    client->regID = regID;
    return kIOReturnSuccess;
}

IOReturn SIConnect(SIClient *client, uint16_t vendorID, uint16_t productID)
{
    SIDebug("Attempting to connect to device %#x:%#x...", vendorID, productID);

    CFMutableDictionaryRef query = SIServiceMatchingUSBDevice(vendorID, productID);
    if (!query) {
        SIDebug("Failed to allocate matching dictionary.");
        return kIOReturnError;
    }

    io_iterator_t serviceIter = IO_OBJECT_NULL;
    IOReturn ret = IOServiceGetMatchingServices(kIOMainPortDefault, query, &serviceIter);
    if (ret != kIOReturnSuccess) {
        SIDebug("Failed to get matching services. (%#x)", ret);
        return ret;
    }

    ret = kIOReturnNoDevice;
    io_service_t service = IO_OBJECT_NULL;
    while ((service = IOIteratorNext(serviceIter))) {
        ret = SIClientInitWithService(client, service);
        if (ret != kIOReturnSuccess) {
            SIDebug("Failed to create client with service %#x. (%#x)", service, ret);
            continue;
        }

        break;
    }

    IOObjectRelease(serviceIter);
    return ret;
}

static void SIHandleFirstMatch(void *context, io_iterator_t serviceIter)
{
    SIAsyncCallbacks const *callbacks = (SIAsyncCallbacks const *)context;

    io_service_t service = IO_OBJECT_NULL;
    while ((service = IOIteratorNext(serviceIter))) {
        SIClient *client = SIClientCreate();
        IOReturn ret = SIClientInitWithService(client, service);
        if (ret != kIOReturnSuccess) {
            SIDebug("Failed to create client with service %#x. (%#x)", service, ret);

            SIClientDestroy(client);
            continue;
        }

        callbacks->connect(client);
    }
}

static void SIHandleTerminated(void *context, io_iterator_t serviceIter)
{
    SIAsyncCallbacks const *callbacks = (SIAsyncCallbacks const *)context;

    io_service_t service = IO_OBJECT_NULL;
    while ((service = IOIteratorNext(serviceIter))) {
        uint64_t entryID;
        if (IORegistryEntryGetRegistryEntryID(service, &entryID) != kIOReturnSuccess)
            goto L_next;

        if (callbacks)
            callbacks->disconnect(entryID);

    L_next:
        // TODO: Check error code?
        IOObjectRelease(service);
    }
}

IOReturn SIConnectAsync(uint16_t vendorID, uint16_t productID, SIAsyncCallbacks *callbacks)
{
    IONotificationPortRef notifyPort = IONotificationPortCreate(kIOMainPortDefault);
    if (!notifyPort)
        return kIOReturnError;

    CFMutableDictionaryRef query = SIServiceMatchingUSBDevice(vendorID, productID);
    if (!query)
        return kIOReturnError;

    // Each of the calls to 'IOServiceAddMatchingNotification' will consume a
    // reference to this, so we increment the reference count of the matching
    // dictionary so that it lives until the second call.
    CFRetain(query);

    CFRunLoopSourceRef runLoopSource = IONotificationPortGetRunLoopSource(notifyPort);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopDefaultMode);

    io_iterator_t sFirstMatchIter = IO_OBJECT_NULL;
    IOReturn ret = IOServiceAddMatchingNotification(notifyPort, kIOFirstMatchNotification, query,
        &SIHandleFirstMatch, callbacks, &sFirstMatchIter);
    if (ret != kIOReturnSuccess)
        return ret;

    io_iterator_t sTerminatedIter = IO_OBJECT_NULL;
    ret = IOServiceAddMatchingNotification(notifyPort, kIOTerminatedNotification, query,
        &SIHandleTerminated, callbacks, &sTerminatedIter);
    if (ret)
        return ret;

    SIHandleFirstMatch(callbacks, sFirstMatchIter);
    SIHandleTerminated(callbacks, sTerminatedIter);

    return kIOReturnSuccess;
}

IOReturn SIGetPipe(SIClient *client, uint8_t index, SIPipeProps *pipe)
{
    return (*client->interface)->GetPipeProperties(client->interface, index, //
        &pipe->direction, &pipe->endpoint, &pipe->type, &pipe->max, &pipe->interval);
}

IOReturn SIGetAllPipes(SIClient *client, SIPipeProps *pipes, size_t *numPipes)
{
    // XXX: There is actually one more endpoint than is listed here, since this
    // count doesn't include the control endpoint (0).
    uint8_t numEndpoints = 0;
    IOReturn ret = (*client->interface)->GetNumEndpoints(client->interface, &numEndpoints);
    if (ret != kIOReturnSuccess) {
        SIDebug("Failed to get number of endpoints. (%#x)", ret);
        return ret;
    }

    uint8_t num = 0;
    for (uint8_t i = 0; i <= numEndpoints; ++i) {
        SIPipeProps pipe;
        ret = SIGetPipe(client, i, &pipe);
        if (ret != kIOReturnSuccess) {
            SIDebug("Failed to get number pipe %d properties. (%#x)", i, ret);
            return ret;
        }

        pipes[num++] = pipe;
    }

    *numPipes = num;
    return kIOReturnSuccess;
}

IOReturn SIReadPipe(SIClient *client, uint8_t pipe, void *buffer, uint32_t *bufSizeInOut)
{
    SIDebug("Reading %d byte(s) from pipe %d...", *bufSizeInOut, pipe);
    return (*client->interface)->ReadPipe(client->interface, pipe, buffer, bufSizeInOut);
}

IOReturn SIWritePipe(SIClient *client, uint8_t pipe, void const *buffer, uint32_t bufSize)
{
    SIDebug("Writing %d byte(s) to pipe %d...", bufSize, pipe);
    return (*client->interface)->WritePipe(client->interface, pipe, (void *)buffer, bufSize);
}

IOReturn SIAbortPipe(SIClient *client, uint8_t pipe)
{
    SIDebug("Aborting pipe %d...", pipe);
    return (*client->interface)->AbortPipe(client->interface, pipe);
}

#define kSIRequestTimeoutDefault 6

SITransferResult SIControlTransfer(SIClient *client, uint8_t requestType, uint8_t request,
    uint16_t value, uint16_t index, void *data, size_t length)
{
    // Use the global null buffer if no data pointer is passed in but a
    // non-zero length is specified.
    if (length > 0 && data == NULL) {
        static char sNullBuffer[0x1000] = { 0 };
        memset(sNullBuffer, 0, sizeof sNullBuffer);
        data = &sNullBuffer;
    }

    SIDebug("Performing control transfer: %#x, %#x, %#x, %#x, %p, %#zx", requestType, request,
        value, index, data, length);

    IOUSBDevRequestTO req;
    req.wLenDone = 0;
    req.pData = data;
    req.bRequest = request;
    req.bmRequestType = requestType;
    req.wLength = OSSwapLittleToHostInt16(length);
    req.wValue = OSSwapLittleToHostInt16(value);
    req.wIndex = OSSwapLittleToHostInt16(index);
    req.completionTimeout = kSIRequestTimeoutDefault;
    req.noDataTimeout = kSIRequestTimeoutDefault;

    IOReturn error = (*client->device)->DeviceRequestTO(client->device, &req);
    return (SITransferResult) { .error = error, .length = req.wLenDone };
}

SITransferResult SIGetDescriptor(SIClient *client, SIDescriptorType type, uint8_t index,
    void *descOut, size_t bufSize)
{
    return SIControlTransfer(client,
        kSIDirectionToHost | kSITypeStandard | kSIRecipientDevice,
        kSIRequestGetDescriptor, (type << 8) | index,
        type == kSIDescriptorTypeString ? /* US English */ 0x409 : 0,
        descOut, bufSize);
}

int SIDecodeStringDescriptor(SIStringDescriptor *desc, char *out, size_t outSize)
{
    if (outSize < desc->bLength)
        return 0;

    int i;
    for (i = 0; i < desc->bLength / 2; ++i)
        out[i] = (char)(desc->wData[i] & 0xff);
    out[i - 1] = 0;

    return i;
}
