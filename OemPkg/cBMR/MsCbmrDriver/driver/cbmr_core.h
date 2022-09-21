#ifndef _CBMR_CORE_H_
#define _CBMR_CORE_H_

//
// Global includes
//

#include "cbmrincludes.h"

#include "cbmr.h"
#include "cbmr_config.h"
#include "ramdisk.h"
#include "http.h"

//
// Note: Please update versioning information anytime any change is made to
// CBMR driver. Rev minor version first, then major version.
//

#define CBMR_MAJOR_VERSION 0x01
#define CBMR_MINOR_VERSION 0x00

typedef struct _CBMR_DRIVER_VERSION {
    UINT8 Major;
    UINT8 Minor;
} CBMR_DRIVER_VERSION, *PCBMR_DRIVER_VERSION;

#define EFI_MS_CBMR_VARIABLES_INTERNAL_GUID                                        \
    {                                                                              \
        0xCA787F2E, 0x4D68, 0x4883, 0xB9, 0x9E, 0x7F, 0xB1, 0x2E, 0xB3, 0x49, 0xCD \
    }

//
// TODO: Add protections for this variable(s) so it doesn't become
// an attack vector for manipulating which version of CBMR driver
// to run. One mechanism to add this protection as spec requirement
// for OEMs/IBVs (e.g. requiring that variables can only be
// modified from BOOT_SERVICE environment, etc).
//

#define EFI_MS_CBMR_SERVICING_INFO_VARIABLE L"MsCbmrServicingInfo"

typedef enum _SOFTWARE_INVENTORY_TYPE {
    SOFTWARE_INVENTORY_PRIMARY,
    SOFTWARE_INVENTORY_SECONDARY
} SOFTWARE_INVENTORY_TYPE, *PSOFTWARE_INVENTORY_TYPE;

typedef struct _SOFTWARE_INVENTORY_INFO {
    SOFTWARE_INVENTORY_TYPE InventoryType;
    CHAR16* UefiVariableName;
    CHAR16* RamdiskFilePath;
    CHAR8* RequestJson;
    BOOLEAN Valid;
} SOFTWARE_INVENTORY_INFO, *PSOFTWARE_INVENTORY_INFO;

//
// Below structure define the public and private portions of UEFI CBMR protocol
//

typedef struct _EFI_MS_CBMR_PROTOCOL_INTERNAL {
    EFI_MS_CBMR_PROTOCOL;

    //
    // CBMR application progress call back to report more detailed info of the
    // driver operations
    //

    EFI_MS_CBMR_PROGRESS_CALLBACK ProgressCallBack;

    //
    //
    //

    EFI_MS_CBMR_PROGRESS Progress;

    //
    // Error object
    //

    EFI_MS_CBMR_ERROR_DATA ErrorData;

    //
    // Total number of collaterals used for ram booting to stub os
    //

    UINTN NumberOfCollaterals;

    //
    // Array of collaterals used for ram booting to stub os
    //

    EFI_MS_CBMR_COLLATERAL* Collaterals;

    BOOLEAN IsDriverConfigured;

    //
    // Ram disk parameters
    //

    UINTN RamdiskSize;
    RAMDISK_CONTEXT* RamdiskContext;

    //
    // Http Parameters
    //

    HTTP_CONTEXT* HttpContext;

    //
    // Downloaded CBMR driver used for servicing
    //

    VOID* CbmrDriver;
    UINTN CbmrDriverSize;

    //
    // Software Inventories
    //

    SOFTWARE_INVENTORY_INFO SoftwareInventories[2];

} EFI_MS_CBMR_PROTOCOL_INTERNAL, *PEFI_MS_CBMR_PROTOCOL_INTERNAL;

typedef struct _CBMR_SERVICING_INFO {
    BOOLEAN ServicingInitiated;
    CBMR_DRIVER_VERSION PriorVersion;
    PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal;
} CBMR_SERVICING_INFO, *PCBMR_SERVICING_INFO;

EFI_STATUS
EFIAPI
CbmrConfigure(_In_ PEFI_MS_CBMR_PROTOCOL This,
              _In_ PEFI_MS_CBMR_CONFIG_DATA CbmrConfigData,
              _In_opt_ EFI_MS_CBMR_PROGRESS_CALLBACK ProgressCallback);

EFI_STATUS
EFIAPI
CbmrGetData(_In_ PEFI_MS_CBMR_PROTOCOL This,
            _In_ EFI_MS_CBMR_DATA_TYPE DataType,
            _Inout_ VOID* Data,
            _In_ OUT UINTN* DataSize);
EFI_STATUS
EFIAPI
CbmrStart(_In_ PEFI_MS_CBMR_PROTOCOL This);

EFI_STATUS
EFIAPI
CbmrClose(_In_ PEFI_MS_CBMR_PROTOCOL This);

#endif // _CBMR_CORE_H_
