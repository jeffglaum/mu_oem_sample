/** @file MsCloudBareMetalRecovery.h

This module implements CBMR UEFI protocol

Copyright (C) Microsoft Corporation. All rights reserved.
SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#ifndef _MS_CLOUD_BARE_METAL_RECOVERY__H_
#define _MS_CLOUD_BARE_METAL_RECOVERY__H_


#define EFI_MS_CBMR_PROTOCOL_REVISION 0x0000000000010000
typedef struct _EFI_MS_CBMR_PROTOCOL EFI_MS_CBMR_PROTOCOL, *PEFI_MS_CBMR_PROTOCOL;


//
// Error Data
//

typedef struct _EFI_MS_CBMR_ERROR_DATA {
    //
    // UEFI specific operation error code
    //

    EFI_STATUS Status;

    //
    // CBMR defined stop codes with extended error info at https://aka.ms/systemrecoveryerror
    //

    UINTN StopCode;

} EFI_MS_CBMR_ERROR_DATA, *PEFI_MS_CBMR_ERROR_DATA;

//
// Configuration options to be used by the driver
//

typedef struct _EFI_MS_CBMR_WIFI_NETWORK_PROFILE {
    CHAR8 SSId[64];
    UINTN SSIdLength;
    CHAR8 Password[64]; // Max allowed WPA2-PSK is 63 Ascii characters. 64 allows for 63 chars + NUL
                        // character
    UINTN PasswordLength;
} EFI_MS_CBMR_WIFI_NETWORK_PROFILE, *PEFI_MS_CBMR_WIFI_NETWORK_PROFILE;

typedef struct _EFI_MS_CBMR_CONFIG_DATA {
    //
    // Below Wi-Fi Profile information will be passed to StubOS
    //

    EFI_MS_CBMR_WIFI_NETWORK_PROFILE WifiProfile;
} EFI_MS_CBMR_CONFIG_DATA, *PEFI_MS_CBMR_CONFIG_DATA;

//
// Below object captures the current collateral download progress
//

typedef struct _EFI_MS_CBMR_COLLATERALS_DOWNLOAD_PROGRESS {
    //
    // Index in to the Collaterals array returned by EFI_MS_CBMR_GET_COLLATERALS
    // function. This will be the currently downloading collateral
    //

    UINTN CollateralIndex;

    //
    // HTTP downloads the collateral in chunks. Below field captures the total
    // size of the current collateral downloaded so far
    //

    UINTN CollateralDownloadedSize;

    // //
    // // HTTP downloads the collateral in chunks. Below field captures the current
    // // chunk size
    // //

    // UINTN CollateralCurrentChunkSize;

    //
    // HTTP downloads the collateral in chunks. Below field captures the current
    // chunk data
    //

    // UINT8* CollateralCurrentChunkData;

} EFI_MS_CBMR_COLLATERALS_DOWNLOAD_PROGRESS, *PEFI_MS_CBMR_COLLATERALS_DOWNLOAD_PROGRESS;

typedef enum _EFI_MS_CBMR_PHASE {
    MsCbmrPhaseConfiguring,
    MsCbmrPhaseConfigured,
    MsCbmrPhaseCollateralsDownloading,
    MsCbmrPhaseCollateralsDownloaded,
    MsCbmrPhaseServicingOperations,
    MsCbmrPhaseStubOsRamboot,
} EFI_MS_CBMR_PHASE;

//
// Below object captures the overall CBMR progress
//

typedef struct _EFI_MS_CBMR_PROGRESS {
    //
    // Current phase of CBMR
    //

    EFI_MS_CBMR_PHASE CurrentPhase;

    //
    // Current phase's progress data
    //

    union {
        EFI_MS_CBMR_COLLATERALS_DOWNLOAD_PROGRESS DownloadProgress;
        // Data related to each phase of CBMR will go in its own struct here
    } ProgressData;
} EFI_MS_CBMR_PROGRESS, *PEFI_MS_CBMR_PROGRESS;

//
// This function is an application provided callback used by the CBMR driver to
// let the application know about the current collateral download progress.
// Which will then be used by the application to render the UI.
//
// NOTE: Any EFI error returned in the callback will be treated as fatal and
// terminate the CBMR process
//

typedef EFI_STATUS(EFIAPI* EFI_MS_CBMR_PROGRESS_CALLBACK)(IN PEFI_MS_CBMR_PROTOCOL This,
                                                          IN EFI_MS_CBMR_PROGRESS* Progress);

//
// This is the first function to be called by the application to configure
// the driver. Rest of the functions defined on the protocol only work once the
// configuration is successfull.
//

typedef EFI_STATUS(EFIAPI* EFI_MS_CBMR_CONFIGURE)(IN PEFI_MS_CBMR_PROTOCOL This,
                                                  IN PEFI_MS_CBMR_CONFIG_DATA CbmrConfigData,
                                                  IN EFI_MS_CBMR_PROGRESS_CALLBACK
                                                      ProgressCallback);

//
// Downloading of CBMR Collateral
// NOTE: Be very careful when updating this structure, as it requires
// updating the public cbmr.h header. This means that all consumers
// of this structure must also be updated, otherwise there will be a
// mismatch between cbmr_app and cbmr_driver (e.g. GetCollaterals() as
// currently implemented will return an array that cbmr_app will not
// interpret correctly, resulting in undefined behavior).
//

//
// TODO: Refactor EFI_MS_CBMR_COLLATERAL and its consumers (cbmr_app):
// 1) Add Size field to beginning of struct to provide backcompat and other
// benefits.
// 2) Update cbmr_app to use new Size field when it calls GetCollaterals(),
// so that it can accurately iterate through the collaterals. Once this is
// done the above NOTE can be deleted.
// 3) Make FilePath and MemoryLocation into a union.
//

typedef struct _EFI_MS_CBMR_COLLATERAL {
    //
    // Size of struct
    //

    UINTN Size;

    //
    // HTTP Url of the collateral.
    //

    CHAR16* RootUrl;

    //
    // Length of root URL
    //

    UINTN RootUrlLength;

    //
    // Relative file path on server (relative to root).
    //

    CHAR16* RelativeUrl;

    //
    // Length of relative URL
    //

    UINTN RelativeUrlLength;

    //
    // Local location where the collateral is saved. In our case, it will be the
    // path inside the Ramboot fat32 volume
    //

    CHAR16* FilePath;

    //
    // The full size of the collateral aka ContentLength
    //

    UINTN CollateralSize;

    //
    // Determines if collateral should be kept in memory rather than written to
    // a file. If TRUE, FilePath is ignored.
    //

    BOOLEAN StoreInMemory;

    //
    // Memory location of collaterals. Only applicable if StoreInMemory is TRUE;
    //

    UINT8* MemoryLocation;

    //
    // SHA-256 Digest of collateral. Typically used for verifying DCAT payloads.
    //

    UINT8 Digest[32];
} EFI_MS_CBMR_COLLATERAL, *PEFI_MS_CBMR_COLLATERAL;

typedef enum _EFI_MS_CBMR_DATA_TYPE {

    //
    // Get the version info CBMR driver. For now, this is same the Revision
    // field on EFI_MS_CBMR_PROTOCOL
    //

    EfiMsCbmrVersion,

    //
    // Get the list of collaterals that will be downloaded by CBMR. This will be
    // an array of type EFI_MS_CBMR_COLLATERAL.
    //

    EfiMsCbmrCollaterals,

    //
    // Get the most recent extended error info. An object of type EFI_MS_CBMR_ERROR_DATA.
    //

    EfiMsCbmrExtendedErrorData,
} EFI_MS_CBMR_DATA_TYPE;

typedef EFI_STATUS(EFIAPI* EFI_MS_CBMR_GET_DATA)(IN PEFI_MS_CBMR_PROTOCOL This,
                                                 IN EFI_MS_CBMR_DATA_TYPE DataType,
                                                 IN OUT VOID* Data,
                                                 IN OUT UINTN* DataSize);

//
// This function will ask the driver to start downloading the collaterals
// download. The provided callback will be called up on each HTTP chunk receive.
// The downloaded collaterals will be written to a FAT32 formatted Ramdisk
// volume
//
// NOTE: As of now, this is a blocking call.
//

typedef EFI_STATUS(EFIAPI* EFI_MS_CBMR_START)(IN PEFI_MS_CBMR_PROTOCOL This);

//
// This function is expected to be called when the application is done using the
// driver. But in CBMR case, this will most likely gets triggered when unloading
// the driver as Rambooting a device may not give the UEFI application a chance
// to free the driver!
//

typedef EFI_STATUS(EFIAPI* EFI_MS_CBMR_CLOSE)(IN PEFI_MS_CBMR_PROTOCOL This);

//
// Below structure define the publicly exposed portion of UEFI CBMR protocol
//

typedef struct _EFI_MS_CBMR_PROTOCOL {
    UINT64 Revision;

    EFI_MS_CBMR_CONFIGURE Configure;
    EFI_MS_CBMR_GET_DATA GetData;
    EFI_MS_CBMR_START Start;
    EFI_MS_CBMR_CLOSE Close;

} EFI_MS_CBMR_PROTOCOL, *PEFI_MS_CBMR_PROTOCOL;

extern EFI_GUID gEfiMsCbmrProtocolGuid;

#endif // _MS_CLOUD_BARE_METAL_RECOVERY__H_
