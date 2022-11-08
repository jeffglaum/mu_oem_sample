/** @file CbmrDriverInterface.c

  cBMR (Cloud Bare Metal Recovery) driver interface routines.

  Copyright (c) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  cBMR driver interface routines that can be used by the cBMR application to configure and control driver behavior.
**/

#include <Library/CbmrSupportLib.h>

static EFI_MS_CBMR_PROTOCOL  *CbmrProtocolPtr;

static
EFI_STATUS
EFIAPI
CbmrDriverConnect (
  VOID
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  Status = gBS->LocateProtocol (
                  &gEfiMsCbmrProtocolGuid,
                  NULL,
                  (VOID **)&CbmrProtocolPtr
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to locate cBMR (driver) protocol (%r).\r\n", Status));
    goto Exit;
  }

Exit:
  return Status;
}

EFI_STATUS
EFIAPI
CbmrDriverConfigure (
  IN EFI_MS_CBMR_CONFIG_DATA        *CbmrConfigData,
  IN EFI_MS_CBMR_PROGRESS_CALLBACK  ProgressCallback
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  // Locate the cBMR driver protocol if we haven't already.
  //
  if (CbmrProtocolPtr == NULL) {
    Status = CbmrDriverConnect ();
    if (EFI_ERROR (Status)) {
      goto Exit;
    }
  }

  Status = CbmrProtocolPtr->Configure (
                              CbmrProtocolPtr,
                              CbmrConfigData,
                              (EFI_MS_CBMR_PROGRESS_CALLBACK)ProgressCallback
                              );

Exit:
  return Status;
}

EFI_STATUS
EFIAPI
CbmrDriverFetchCollateral (
  OUT EFI_MS_CBMR_COLLATERAL  **Collateral,
  OUT UINTN                   *CollateralSize
  )
{
  EFI_STATUS               Status          = EFI_SUCCESS;
  UINTN                    DataSize        = 0;
  PEFI_MS_CBMR_COLLATERAL  LocalCollateral = NULL;

  // Locate the cBMR driver protocol if we haven't already.
  //
  if (CbmrProtocolPtr == NULL) {
    Status = CbmrDriverConnect ();
    if (EFI_ERROR (Status)) {
      goto Exit;
    }
  }

  Status = CbmrProtocolPtr->GetData (CbmrProtocolPtr, EfiMsCbmrCollaterals, NULL, &DataSize);
  if (EFI_ERROR (Status) && (Status != EFI_BUFFER_TOO_SMALL)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to get cBMR collateral size (%r).\r\n", Status));
    goto Exit;
  }

  LocalCollateral = AllocateZeroPool (DataSize);
  if (LocalCollateral == NULL) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to memory buffer for cBMR collateral  (%r).\r\n", Status));

    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Status = CbmrProtocolPtr->GetData (CbmrProtocolPtr, EfiMsCbmrCollaterals, LocalCollateral, &DataSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to fetch cBMR collateral (%r).\r\n", Status));
    goto Exit;
  }

  *Collateral     = LocalCollateral;
  *CollateralSize = DataSize;

Exit:
  return Status;
}

EFI_STATUS
EFIAPI
CbmrDriverStartDownload (
  VOID
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  // Locate the cBMR driver protocol if we haven't already.
  //
  if (CbmrProtocolPtr == NULL) {
    Status = CbmrDriverConnect ();
    if (EFI_ERROR (Status)) {
      goto Exit;
    }
  }

  Status = CbmrProtocolPtr->Start (CbmrProtocolPtr);

  if (EFI_ERROR (Status)) {
    goto Exit;
  }

Exit:
  return Status;
}
