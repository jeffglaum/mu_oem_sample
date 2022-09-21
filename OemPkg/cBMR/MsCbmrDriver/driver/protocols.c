/*++

Copyright (c) 2020  Microsoft Corporation

Module Name:

    protocolhelpers.c

Abstract:

    This module implements protocol handling routines

Author:

    Vineel Kovvuri (vineelko) 19-May-2020

Environment:

    UEFI and BOOT mode only.

--*/

#include "cbmrincludes.h"
#include "protocols.h"
#include "network_common.h"
#include "error.h"

EFI_STATUS
ProtocolOpenServiceBinding(_In_ EFI_HANDLE DeviceHandle,
                           _In_ EFI_GUID* ServiceBindingProtocolGuid,
                           _Outptr_ EFI_SERVICE_BINDING_PROTOCOL** ServiceBindingProtocol)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_SERVICE_BINDING_PROTOCOL* LocalSericeBindingProtocol = NULL;

    //
    // Get the protocol reference for the Protocolhandle.
    //

    Status = gBS->OpenProtocol(DeviceHandle,
                               ServiceBindingProtocolGuid,
                               (PVOID*)&LocalSericeBindingProtocol,
                               gImageHandle,
                               NULL,
                               EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("OpenProtocol() failed : 0x%zx", Status);
        goto Exit;
    }

    *ServiceBindingProtocol = LocalSericeBindingProtocol;

Exit:
    return Status;
}

EFI_STATUS
ProtocolOpenServiceBindingChildProtocol(_In_ EFI_SERVICE_BINDING_PROTOCOL* ServiceBindingProtocol,
                                        _In_ EFI_GUID* ProtocolGuid,
                                        _Outptr_ PVOID* Protocol,
                                        _Outptr_ EFI_HANDLE* ProtocolHandle)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_HANDLE ChildHandle = NULL;
    PVOID RetProtocol = NULL;

    if (ServiceBindingProtocol == NULL) {
        DBG_ERROR("ServiceBindingProtocol is null", NULL);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    //
    //  Get protocol child protocol handle.
    //

    Status = ServiceBindingProtocol->CreateChild(ServiceBindingProtocol, &ChildHandle);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CreateChild() failed : 0x%zx", Status);
        goto Exit;
    }

    //
    // Get the protocol reference to the child protocol handle.
    //

    Status = gBS->OpenProtocol(ChildHandle,
                               ProtocolGuid,
                               (PVOID*)&RetProtocol,
                               gImageHandle,
                               NULL,
                               EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("OpenProtocol() failed : 0x%zx", Status);
        goto Exit;
    }

    *Protocol = RetProtocol;
    *ProtocolHandle = ChildHandle;
    return Status;

Exit:
    if (ChildHandle != NULL) {
        ServiceBindingProtocol->DestroyChild(ServiceBindingProtocol, &ChildHandle);
    }

    return Status;
}

EFI_STATUS EFIAPI ProtocolServiceBindingClose(_In_ PPROTOCOL_INFO ProtocolInfo)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_GUID* ProtocolGuid = NULL;
    EFI_GUID* ServiceBindingProtocolGuid = NULL;
    EFI_HANDLE DeviceHandle = NULL;
    EFI_HANDLE ProtocolHandle = NULL;
    EFI_SERVICE_BINDING_PROTOCOL* ServiceBindingProtocol = NULL;
    VOID* Protocol = NULL;

    if (ProtocolInfo->ServiceBindingProtocolName == NULL)
        return Status;

    DeviceHandle = ProtocolInfo->DeviceHandle;
    ServiceBindingProtocolGuid = ProtocolInfo->ServiceBindingProtocolGuid;
    ServiceBindingProtocol = ProtocolInfo->ServiceBindingProtocol;
    ProtocolGuid = ProtocolInfo->ProtocolGuid;
    Protocol = ProtocolInfo->Protocol;
    ProtocolHandle = ProtocolInfo->ChildHandle;

    //
    // Close the child protocol first
    //

    if (ProtocolHandle != NULL && Protocol != NULL) {
        Status = gBS->CloseProtocol(ProtocolHandle, ProtocolGuid, gImageHandle, NULL);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("CloseProtocol() failed : 0x%zx", Status);
            goto Exit;
        }
    }

    //
    // Next, Close the child protocol handle
    //

    if (ServiceBindingProtocol != NULL && ProtocolHandle != NULL) {
        Status = ServiceBindingProtocol->DestroyChild(ServiceBindingProtocol, ProtocolHandle);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("DestroyChild() failed : 0x%zx", Status);
            goto Exit;
        }
    }

    //
    // Next, Close service binding protocol on device handle
    //

    if (DeviceHandle != NULL && ServiceBindingProtocol != NULL) {
        Status = gBS->CloseProtocol(DeviceHandle, ServiceBindingProtocolGuid, gImageHandle, NULL);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("CloseProtocol() failed : 0x%zx", Status);
            goto Exit;
        }
    }

Exit:
    return Status;
}

EFI_STATUS EFIAPI ProtocolGetInfo(_Inout_ PPROTOCOL_INFO ProtocolInfo)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_HANDLE* DeviceHandles = NULL;
    UINTN DeviceHandleCount = 0;

    if (ProtocolInfo->ProtocolGuid == NULL) {
        goto Exit;
    }

    if (ProtocolInfo->ServiceBindingProtocolName == NULL) {
        Status = gBS->LocateProtocol(ProtocolInfo->ProtocolGuid,
                                     NULL,
                                     (PVOID*)&ProtocolInfo->Protocol);
        ProtocolInfo->ProtocolStatus = Status;
    } else {
        Status = gBS->LocateHandleBuffer(ByProtocol,
                                         ProtocolInfo->ServiceBindingProtocolGuid,
                                         NULL,
                                         &DeviceHandleCount,
                                         &DeviceHandles);

        for (UINTN Index = 0; Index < DeviceHandleCount; Index++) {
            ProtocolInfo->DeviceHandle = DeviceHandles[Index];
            ProtocolInfo->ServiceBindingProtocolStatus = ProtocolOpenServiceBinding(
                ProtocolInfo->DeviceHandle,
                ProtocolInfo->ServiceBindingProtocolGuid,
                &ProtocolInfo->ServiceBindingProtocol);

            if (!EFI_ERROR(ProtocolInfo->ServiceBindingProtocolStatus)) {
                ProtocolInfo->ProtocolStatus = ProtocolOpenServiceBindingChildProtocol(
                    ProtocolInfo->ServiceBindingProtocol,
                    ProtocolInfo->ProtocolGuid,
                    &ProtocolInfo->Protocol,
                    &ProtocolInfo->ChildHandle);
            }

#if 0
            (VOID) ProtocolServiceBindingClose(DeviceHandles[Index],
                                                 ProtocolInfo->ServiceBindingProtocolGuid,
                                                 ProtocolInfo->ServiceBindingProtocol,
                                                 ProtocolInfo->ProtocolGuid,
                                                 ProtocolInfo->Protocol,
                                                 ChildHandle);
#endif
            if (!EFI_ERROR(ProtocolInfo->ServiceBindingProtocolStatus) &&
                !EFI_ERROR(ProtocolInfo->ProtocolStatus))
                break;
        }

        if (DeviceHandles) {
            gBS->FreePool(DeviceHandles);
        }
    }

Exit:
    return Status;
}
