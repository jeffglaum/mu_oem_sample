/*++

Copyright (c) 2022  Microsoft Corporation

Module Name:

    error.c

Abstract:

    This module implements the extended error handling routines

Author:

    Vineel Kovvuri (vineelko) 14-Jan-2022

Environment:

    UEFI mode only.

--*/

#include "cbmrincludes.h"
#include "cbmr_core.h"
#include "error.h"

static PEFI_MS_CBMR_PROTOCOL_INTERNAL gInternal = NULL;

VOID CbmrInitializeErrorModule(_In_ PEFI_MS_CBMR_PROTOCOL This)
{
    gInternal = (PEFI_MS_CBMR_PROTOCOL_INTERNAL)This;
    CbmrClearExtendedErrorInfo();
}

EFI_STATUS CbmrGetExtendedErrorInfo(_Inout_ PEFI_MS_CBMR_ERROR_DATA Data, _Inout_ UINTN* DataSize)
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (gInternal == NULL) {
        DBG_ERROR("Cbmr driver is not configured", NULL);
        Status = EFI_NOT_READY;
        goto Exit;
    }

    if (DataSize == NULL) {
        DBG_ERROR("Invalid DataSize parameter", NULL);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (*DataSize < sizeof(EFI_MS_CBMR_ERROR_DATA)) {
        *DataSize = sizeof(EFI_MS_CBMR_ERROR_DATA);
        Status = EFI_BUFFER_TOO_SMALL;
        goto Exit;
    }

    Status = CopyMemS(Data, *DataSize, &gInternal->ErrorData, sizeof(EFI_MS_CBMR_ERROR_DATA));
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CopyMemS() failed 0x%zx", Status);
        goto Exit;
    }

    *DataSize = sizeof(EFI_MS_CBMR_ERROR_DATA);
    return Status;

Exit:
    return Status;
}

VOID CbmrSetExtendedErrorInfo(_In_ EFI_STATUS ErrorStatus, _In_ UINTN StopCode)
{
    if (gInternal == NULL) {
        return;
    }

    //
    // Do not override the last error codes
    //

    if (gInternal->ErrorData.Status == EFI_SUCCESS &&
        gInternal->ErrorData.StopCode == CBMR_ERROR_SUCCESS) {
        gInternal->ErrorData.Status = ErrorStatus;
        gInternal->ErrorData.StopCode = StopCode;
    }
}

VOID CbmrClearExtendedErrorInfo()
{
    if (gInternal == NULL) {
        return;
    }

    gInternal->ErrorData.Status = EFI_SUCCESS;
    gInternal->ErrorData.StopCode = CBMR_ERROR_SUCCESS;
}
