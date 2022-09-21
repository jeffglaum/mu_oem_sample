/*++

Copyright (c) 2021 Microsoft Corporation

Module Name:

    cbmr.c

Abstract:

    This module implements uefi driver entry/exit portion of CBMR

Author:

    Vineel Kovvuri (vineelko) 24-May-2021

Environment:

    UEFI mode only.

--*/

//
// Global includes
//

#include "cbmrincludes.h"
#ifndef UEFI_BUILD_SYSTEM
#include "strsafe.h"
#endif

//
// Local includes
//

#include "cbmr_core.h"
#include "cbmr_protocols.h"

//
// Prototypes
//

#ifndef UEFI_BUILD_SYSTEM
EFI_SYSTEM_TABLE* gST;
EFI_HANDLE* gImageHandle = NULL;
#endif

EFI_GUID gEfiMsCbmrProtocolGuid = EFI_MS_CBMR_PROTOCOL_GUID;

EFI_STATUS
CbmrDriverExit(_In_ EFI_HANDLE ImageHandle);

EFI_STATUS
EFIAPI
CbmrDriverBindingSupported(_In_ EFI_DRIVER_BINDING_PROTOCOL* This,
                           _In_ EFI_HANDLE ControllerHandle,
                           _In_ EFI_DEVICE_PATH_PROTOCOL* RemainingDevicePath);
EFI_STATUS
EFIAPI
CbmrDriverBindingStart(_In_ EFI_DRIVER_BINDING_PROTOCOL* This,
                       _In_ EFI_HANDLE ControllerHandle,
                       _In_ EFI_DEVICE_PATH_PROTOCOL* RemainingDevicePath);
EFI_STATUS
EFIAPI
CbmrDriverBindingStop(_In_ EFI_DRIVER_BINDING_PROTOCOL* This,
                      _In_ EFI_HANDLE ControllerHandle,
                      _In_ UINTN NumberOfChildren,
                      _In_ EFI_HANDLE* ChildHandleBuffer);

static EFI_STATUS EFIAPI IsDriverServiced(_Inout_ CBMR_SERVICING_INFO* ServicingInfo);
static EFI_STATUS EFIAPI ClearServicingInfoVariable();
static EFI_STATUS EFIAPI PerformServicingOperations(_In_ CBMR_SERVICING_INFO* ServicingInfo);

static EFI_DRIVER_BINDING_PROTOCOL CbmrDriverBinding = {
                                                         CbmrDriverBindingSupported,
                                                         CbmrDriverBindingStart,
                                                         CbmrDriverBindingStop,
                                                         1,
                                                         NULL,
                                                         NULL};

static EFI_MS_CBMR_PROTOCOL_INTERNAL CbmrProtocol = {
    .Revision = (UINT32)EFI_MS_CBMR_PROTOCOL_REVISION,
    .Configure = (EFI_MS_CBMR_CONFIGURE)CbmrConfigure,
    .GetData = (EFI_MS_CBMR_GET_DATA)CbmrGetData,
    .Start = (EFI_MS_CBMR_START)CbmrStart,
    .Close = (EFI_MS_CBMR_CLOSE)CbmrClose,
};

EFI_STATUS
EFIAPI
CbmrDriverBindingSupported(_In_ EFI_DRIVER_BINDING_PROTOCOL* This,
                           _In_ EFI_HANDLE ControllerHandle,
                           _In_ EFI_DEVICE_PATH_PROTOCOL* RemainingDevicePath)
/*++

Description:

    Test to see if this driver supports ControllerHandle

Arguments:

    This                - A pointer to the EFI_DRIVER_BINDING instance
    ControllerHandle    - Handle of device to test
    RemainingDevicePath - Optional parameter use to pick a specific child device to start

Return Value:

    EFI_SUCCESS         This driver supports this device
    EFI_ALREADY_STARTED This driver is already running on this device
    Other               This driver does not support this device

--*/
{
    UNREFERENCED_PARAMETER(This);
    UNREFERENCED_PARAMETER(ControllerHandle);
    UNREFERENCED_PARAMETER(RemainingDevicePath);

    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CbmrDriverBindingStart(_In_ EFI_DRIVER_BINDING_PROTOCOL* This,
                       _In_ EFI_HANDLE ControllerHandle,
                       _In_ EFI_DEVICE_PATH_PROTOCOL* RemainingDevicePath)
/*++

Description:

    Start this driver on ControllerHandle

Arguments:

    This                - A pointer to the EFI_DRIVER_BINDING instance
    ControllerHandle    - Handle of device to bind driver to
    RemainingDevicePath - Optional parameter use to pick a specific child device to start

Return Value:

    EFI_SUCCESS         This driver is added to ControllerHandle
    EFI_ALREADY_STARTED This driver is already running on ControllerHandle
    Other               This driver does not support this device

--*/
{
    UNREFERENCED_PARAMETER(This);
    UNREFERENCED_PARAMETER(ControllerHandle);
    UNREFERENCED_PARAMETER(RemainingDevicePath);

    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CbmrDriverBindingStop(_In_ EFI_DRIVER_BINDING_PROTOCOL* This,
                      _In_ EFI_HANDLE ControllerHandle,
                      _In_ UINTN NumberOfChildren,
                      _In_ EFI_HANDLE* ChildHandleBuffer)
/*++

Description:

    Stop this driver on ControllerHandle

Arguments:

    This                - A pointer to the EFI_DRIVER_BINDING instance
    ControllerHandle    - Handle of device to stop driver on
    NumberOfChildren    - Number of Handles in ChildHandleBuffer. If number of children is zero stop
the entire bus driver ChildHandleBuffer   - List of Child Handles to Stop

Return Value:

    EFI_SUCCESS         This driver is removed ControllerHandle
    Other               This driver was not removed from this device

--*/
{
    UNREFERENCED_PARAMETER(This);
    UNREFERENCED_PARAMETER(ControllerHandle);
    UNREFERENCED_PARAMETER(NumberOfChildren);
    UNREFERENCED_PARAMETER(ChildHandleBuffer);

    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
CbmrDriverInit(_In_ EFI_HANDLE ImageHandle, _In_ EFI_SYSTEM_TABLE* SystemTable)
{
    EFI_STATUS Status = EFI_SUCCESS;

#ifdef UEFI_BUILD_SYSTEM
    gImageHandle = ImageHandle;
    gST = SystemTable;
#endif

    CbmrReadConfig(CBMR_CONFIG_DEBUG_SECTION);

    //
    // Init the debug support with updated options
    //

    DebugInit(t("CBMRDRIVER"));

#ifdef DEBUGMODE
    if (gCbmrConfig.EarlyBreak) {
        __debugbreak();
    }
#endif

    return Status;
}

EFI_STATUS
EFIAPI
MsCbmrDriverEntry(_In_ EFI_HANDLE ImageHandle, _In_ EFI_SYSTEM_TABLE* SystemTable)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_LOADED_IMAGE* LoadedImage = NULL;
    CBMR_SERVICING_INFO ServicingInfo = {0};

    // FIXME: Does driver need logging/debugging?
    Status = CbmrDriverInit(ImageHandle, SystemTable);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CbmrDriverInit() failed 0x%zx", Status);
        goto Exit;
    }

    DBG_INFO("CbmrDriverInit() done", NULL);

    Status = CbmrProtocolProbeAll();
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CbmrProtocolProbeAll() failed 0x%zx", Status);
        goto Exit;
    }

    DBG_INFO("CBMR driver version %d.%d", CBMR_MAJOR_VERSION, CBMR_MINOR_VERSION);

    //
    // Check if we are running from a serviced driver. If so, give control over to
    // handler and allow it to perform servicing operations.
    //

    Status = IsDriverServiced(&ServicingInfo);
    if (!EFI_ERROR(Status)) {
        return PerformServicingOperations(&ServicingInfo);
    } else {
        if (Status == EFI_NOT_FOUND) {
            DBG_INFO("Servicing variable not set. Continue with driver initialization.", NULL);
            Status = EFI_SUCCESS;
        } else {
            //
            // Immediately bail out if an untrusted variable was found or if some other
            // failure occured.
            //
            DBG_ERROR("IsDriverServiced() failed 0x%zx", Status);
            goto Exit;
        }
    }

    Status = gBS->OpenProtocol(gImageHandle,
                               &gEfiLoadedImageProtocolGuid,
                               (VOID**)&LoadedImage,
                               gImageHandle,
                               NULL,
                               EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("OpenProtocol() failed 0x%zx", Status);
        goto Exit;
    }

    LoadedImage->Unload = (EFI_IMAGE_UNLOAD)CbmrDriverExit;
    CbmrDriverBinding.ImageHandle = gImageHandle;
    CbmrDriverBinding.DriverBindingHandle = gImageHandle;

    Status = gBS->InstallMultipleProtocolInterfaces(&CbmrDriverBinding.DriverBindingHandle,
                                                    &gEfiDriverBindingProtocolGuid,
                                                    &CbmrDriverBinding,
                                                    &gEfiMsCbmrProtocolGuid,
                                                    &CbmrProtocol,
                                                    NULL);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("InstallMultipleProtocolInterfaces() failed 0x%zx", Status);
        goto Exit;
    }

    DBG_INFO("Installing Protocols done", NULL);

Exit:
    return Status;
}

EFI_STATUS
CbmrDriverExit(_In_ EFI_HANDLE ImageHandle)
{
    EFI_STATUS Status = EFI_SUCCESS;
    PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal = (PEFI_MS_CBMR_PROTOCOL_INTERNAL)&CbmrProtocol;

    Status = gBS->UninstallMultipleProtocolInterfaces(ImageHandle,
                                                      &gEfiDriverBindingProtocolGuid,
                                                      &CbmrDriverBinding,
                                                      &gEfiMsCbmrProtocolGuid,
                                                      &CbmrProtocol,
                                                      NULL);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("UninstallMultipleProtocolInterfaces() failed 0x%zx", Status);
        goto Exit;
    }

    //
    // If the application have not closed the driver via Close(). Do it now!
    //

    if (Internal->IsDriverConfigured == TRUE) {
        Status = Internal->Close((PEFI_MS_CBMR_PROTOCOL)Internal);
    }

    DBG_INFO("CbmrDriverExit() done", NULL);

Exit:
    return Status;
}

static EFI_STATUS EFIAPI IsDriverServiced(_Inout_ CBMR_SERVICING_INFO* ServicingInfo)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_GUID MsCbmrVariablesInternalGuid = EFI_MS_CBMR_VARIABLES_INTERNAL_GUID;
    UINT32 Attributes = 0;
    UINTN DataSize = sizeof(CBMR_SERVICING_INFO);

    Status = gRT->GetVariable(EFI_MS_CBMR_SERVICING_INFO_VARIABLE,
                              &MsCbmrVariablesInternalGuid,
                              &Attributes,
                              &DataSize,
                              ServicingInfo);
    if (EFI_ERROR(Status)) {
        if (Status == EFI_NOT_FOUND) {
            //
            // Ok, this is a first-run driver instance (not serviced)
            //
            DBG_INFO("ServicingInfo variable not found", NULL);
        }

        return Status;
    }

    //
    // Quick sanity check(s).
    //

    if (Attributes != EFI_VARIABLE_BOOTSERVICE_ACCESS) {
        //
        // Something fishy is going on here. This variable should
        // only be set with EFI_VARIABLE_BOOTSERVICE_ACCESS. Don't
        // trust anything else.
        //

        return EFI_ACCESS_DENIED;
    }

    DBG_INFO("Inside serviced driver", NULL);

    return Status;
}

static EFI_STATUS EFIAPI ClearServicingInfoVariable()
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_GUID MsCbmrVariablesInternalGuid = EFI_MS_CBMR_VARIABLES_INTERNAL_GUID;

    Status = gRT->SetVariable(EFI_MS_CBMR_SERVICING_INFO_VARIABLE,
                              &MsCbmrVariablesInternalGuid,
                              0,
                              0,
                              NULL);

    return Status;
}

static EFI_STATUS EFIAPI PerformServicingOperations(_In_ CBMR_SERVICING_INFO* ServicingInfo)
{
    EFI_STATUS Status = EFI_SUCCESS;

    ClearServicingInfoVariable();

    //
    // TODO: Potentially uninstall old inactive protocol and install new one.
    //

    EFI_MS_CBMR_PROGRESS_CALLBACK ProgressCallback = ServicingInfo->Internal->ProgressCallBack;
    PEFI_MS_CBMR_PROGRESS Progress = &ServicingInfo->Internal->Progress;

    //
    // Servicing operations phase
    //

    Progress->CurrentPhase = MsCbmrPhaseServicingOperations;

    //
    // Invoke the application/caller
    //

    if (ProgressCallback != NULL) {
        Status = ProgressCallback((PEFI_MS_CBMR_PROTOCOL)ServicingInfo->Internal, Progress);
        if (EFI_ERROR(Status)) {
            //
            // Terminate the download process if the caller asked us not
            // to proceed any further
            //
            Status = EFI_SUCCESS;
            goto Exit;
        }
    }

    //
    // NOTE: For the time being, there are no servicing operations. Update this function
    // as necessary to allow running new logic for devices out in the field.
    //

Exit:

    return Status;
}
