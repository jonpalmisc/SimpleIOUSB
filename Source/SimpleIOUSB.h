//
//  SimpleIOUSB.h
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

#pragma once

#include <IOKit/IOTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SI_PACKED __attribute__((packed))

struct IOUSBDeviceStruct245;
typedef struct IOUSBDeviceStruct245 SIDeviceInterface;
typedef SIDeviceInterface **SIDeviceHandle;

struct IOUSBInterfaceStruct245;
typedef struct IOUSBInterfaceStruct245 SIInterfaceInterface;
typedef SIInterfaceInterface **SIInterfaceHandle;

/// SimpleIOUSB client type.
///
/// You are discouraged from using this structure directly! This is C, so I
/// can't stop you, but these would be private members if this were C++.
typedef struct {
    SIDeviceHandle device;       ///< IOUSB device handle.
    SIInterfaceHandle interface; ///< IOUSB interface handle.
    uint64_t regID;              ///< Registry ID of the underlying device.
} SIClient;

/// Create a client.
SIClient *SIClientCreate(void);

/// Destroy a client, closing any connections if open.
void SIClientDestroy(SIClient *client);

/// Connect to a USB device by vendor & product ID.
IOReturn SIConnect(SIClient *client, uint16_t vendorID, uint16_t productID);

/// USB asynchronous connection callbacks.
typedef struct {
    void (*connect)(SIClient *client);
    void (*disconnect)(uint64_t id);
} SIAsyncCallbacks;

/// Connect asynchronously to a USB device by vendor & product ID.
IOReturn SIConnectAsync(uint16_t vendorID, uint16_t productID, SIAsyncCallbacks *callbacks);

/// Properties of a USB pipe.
typedef struct {
    uint8_t direction;
    uint8_t endpoint;
    uint16_t max;
    uint8_t type;
    uint8_t interval;
} SIPipeProps;

#define kSIPipesMax 8

/// Get pipe properties by pipe index.
IOReturn SIGetPipe(SIClient *client, uint8_t index, SIPipeProps *pipe);

/// Get properties for all available pipes.
IOReturn SIGetAllPipes(SIClient *client, SIPipeProps *pipes, size_t *numPipes);

/// Read from a pipe.
IOReturn SIReadPipe(SIClient *client, uint8_t pipe, void *buffer, uint32_t *bufSizeInOut);

/// Write to a pipe.
IOReturn SIWritePipe(SIClient *client, uint8_t pipe, void const *buffer, uint32_t bufSize);

/// Abort a pipe.
IOReturn SIAbortPipe(SIClient *client, uint8_t pipe);

/// USB request direction flags.
typedef enum {
    kSIDirectionToDevice = 0x00,
    kSIDirectionToHost = 0x80,
} SIRequestDirection;

/// USB request type flags.
typedef enum {
    kSITypeStandard = 0x00,
    kSITypeClass = 0x20,
    kSITypeVendor = 0x40,
} SIRequestType;

/// USB request recipient flags.
typedef enum {
    kSIRecipientDevice = 0x00,
    kSIRecipientInterface = 0x01,
    kSIRecipientEndpoint = 0x02,
    kSIRecipientOther = 0x03,
} SIRequestRecipient;

/// USB request codes.
typedef enum {
    kSIRequestGetStatus = 0x00,
    kSIRequestClearFeature = 0x01,
    kSIRequestSetFeature = 0x03,
    kSIRequestSetAddress = 0x05,
    kSIRequestGetDescriptor = 0x06,
    kSIRequestSetDescriptor = 0x07,
    kSIRequestGetConfiguration = 0x08,
    kSIRequestSetConfiguration = 0x09,
    kSIRequestGetInterface = 0x0a,
    kSIRequestSetInterface = 0x0b,
} SIRequest;

/// Tuple type returned from control transfers.
typedef struct {
    IOReturn error;  ///< IOKit error, if applicable.
    uint32_t length; ///< Length of the reply, or 'wLenDone'.
} SITransferResult;

/// Perform a USB control transfer.
SITransferResult SIControlTransfer(SIClient *client, uint8_t requestType, uint8_t request,
    uint16_t value, uint16_t index, void *data, size_t length);

/// USB descriptor types.
typedef enum {
    kSIDescriptorTypeDevice = 0x01,
    kSIDescriptorTypeConfig = 0x02,
    kSIDescriptorTypeString = 0x03,
    kSIDescriptorTypeInterface = 0x03,
    kSIDescriptorTypeEndpoint = 0x05,
} SIDescriptorType;

/// USB device descriptor.
typedef struct SI_PACKED {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} SIDeviceDescriptor;

/// USB configuration descriptor.
typedef struct SI_PACKED {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
} SIConfigDescriptor;

/// USB interface descriptor.
typedef struct SI_PACKED {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} SIInterfaceDescriptor;

/// USB endpoint descriptor.
typedef struct SI_PACKED {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} SIEndpointDescriptor;

/// USB string descriptor; variable-length.
typedef struct SI_PACKED {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wData[];
} SIStringDescriptor;

/// Buffer type alias appropriate for holding a string descriptor.
typedef uint8_t SIStringDescriptorBuffer[0x100];

/// Get a USB descriptor.
SITransferResult SIGetDescriptor(SIClient *client, SIDescriptorType type, uint8_t index,
    void *descOut, size_t bufSize);

/// Decode a string descriptor to ASCII.
///
/// \return Number of bytes decoded, or zero on failure.
int SIDecodeStringDescriptor(SIStringDescriptor *desc, char *out, size_t outSize);

#undef SI_PACKED

#ifdef __cplusplus
}
#endif
