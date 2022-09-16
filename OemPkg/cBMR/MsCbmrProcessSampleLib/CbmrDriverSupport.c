/** @file CbmrDriverSupport.c

  cBMR Process Sample Library

  Copyright (c) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  The library is intended to be a sample of how to initiate the cBMR (Cloud Bare Metal Recovery) process and this file
  specifically contains the primary function to communicate to the cBMR driver.
**/

#include "CbmrProcessCommon.h"


/**
  Collects the collateral list from the cBMR protocol and returns the data in a buffer the caller is reponsible for
  freeing.

  @param[in]  CbmrProtocol       Pointer to the cBMR protocol to use
  @param[out] CollateralDataPtr  Returns a pointer to the allocated buffer containing the collateral list
  @param[out] CollateralCount    Number of collateral structures returned

  @retval     EFI_STATUS
**/
EFI_STATUS
EFIAPI
CbmrDownloadCollaterals(
  IN  EFI_MS_CBMR_PROTOCOL    *CbmrProtocol,
  OUT EFI_MS_CBMR_COLLATERAL  **CollateralDataPtr,
  OUT UINTN                   *CollateralCount)
{
  EFI_MS_CBMR_COLLATERAL *CollateralData = NULL;
  EFI_STATUS Status;
  UINTN Size;

  DEBUG ((DEBUG_INFO, "[cBMR] %a()\n", __FUNCTION__));

  // Call GetData with a buffer size of 0 to retrieve the required size
  Size = 0;
  Status = CbmrProtocol->GetData (CbmrProtocol,
                                  EfiMsCbmrCollaterals,
                                  NULL,
                                  &Size);
  if (Status == EFI_SUCCESS) {
    Status = EFI_PROTOCOL_ERROR;
  }
  if (Status == EFI_BUFFER_TOO_SMALL) {

    // Allocate the required size
    CollateralData = (EFI_MS_CBMR_COLLATERAL*) AllocateZeroPool (Size);
    if (CollateralData == NULL) {
      ASSERT (CollateralData);
      return EFI_OUT_OF_RESOURCES;
    }

    // Call GetData a second time with the proper buffer
    Status = CbmrProtocol->GetData (CbmrProtocol,
                                    EfiMsCbmrCollaterals,
                                    CollateralData,
                                    &Size);
  }
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[cBMR] ERROR: EFI_MS_CBMR_PROTOCOL::GetData( EfiMsCbmrCollaterals ) - Status %r\n", Status));
    if (CollateralData != NULL) {
      FreePool(CollateralData);
    }
    return Status;
  }

  // Provide caller's data
  (*CollateralDataPtr) = CollateralData;
  *CollateralCount = Size / sizeof (EFI_MS_CBMR_COLLATERAL);

  // Debug print the collaterals collected
  for (UINTN x = 0; x < (*CollateralCount); x++) {
    DEBUG ((DEBUG_INFO, "    Collateral Data Block #%d:\n", x + 1));
    DEBUG ((DEBUG_INFO, "        URL:       %s\n", CollateralData[x].RootUrl));
    DEBUG ((DEBUG_INFO, "        File Path: %s\n", CollateralData[x].FilePath));
    DEBUG ((DEBUG_INFO, "        Size:      %d bytes\n", CollateralData[x].CollateralSize));
  }

  return EFI_SUCCESS;
}

/**
  Primary function to initiate the bare metal recovery process

  @param[in]  UseWiFi           TRUE if the process should attempt to attach to a WiFi access point, FALSE for wired
  @param[in]  SSIdName          SSID string used to attach to the WiFi access point. May be NULL if UseWifi is FALSE.
  @param[in]  SSIdPassword      Password  string used to attach to the WiFi access point. May be NULL if UseWifi is FALSE.
  @param[in]  ProgressCallback  Callback function to receive progress information

  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
InitiateRecoveryProcess (
  IN BOOLEAN                        UseWiFi,
  IN CHAR8                          *SSIdName,
  IN CHAR8                          *SSIdPassword,
  IN EFI_MS_CBMR_PROGRESS_CALLBACK  ProgressCallback)
{
  EFI_MS_CBMR_PROTOCOL *CbmrProtocol;
  EFI_MS_CBMR_CONFIG_DATA CbmrConfigData;
  EFI_MS_CBMR_ERROR_DATA ErrorData;
  EFI_STATUS Status;
  UINTN DataSize;

  //
  // Input checks
  //

  DEBUG ((DEBUG_INFO, "[cBMR] %a()\n", __FUNCTION__));
  if (UseWiFi && (SSIdName == NULL || SSIdPassword == NULL)) {
    ASSERT(SSIdName);
    ASSERT(SSIdPassword);
    return EFI_INVALID_PARAMETER;
  }
  if (ProgressCallback == NULL) {
    ASSERT(ProgressCallback);
    return EFI_INVALID_PARAMETER;
  }

  //
  // Locate the cBMR protocol and verify the published version
  //

  Status = gBS->LocateProtocol (&gEfiMsCbmrProtocolGuid, NULL, (VOID**)&CbmrProtocol);
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR(Status);
    return Status;
  }

  DEBUG ((DEBUG_INFO, "       EFI_MS_CBMR_PROTOCOL revision 0x%016lX\n", CbmrProtocol->Revision));

  if (EFI_MS_CBMR_PROTOCOL_REVISION != CbmrProtocol->Revision) {
    DEBUG ((DEBUG_ERROR, "[cBMR] ERROR: Expected EFI_MS_CBMR_PROTOCOL revision 0x%016lX\n", (UINTN)EFI_MS_CBMR_PROTOCOL_REVISION));
    return EFI_PROTOCOL_ERROR;
  }

  //
  // Setup the cBMR configuration input structure. For a wired connection, the structure is zeroed, for WiFi, the SSID
  // and password need to be set.
  //

  ZeroMem (&CbmrConfigData, sizeof(CbmrConfigData));
  if (UseWiFi) {

    Status = AsciiStrCpyS (CbmrConfigData.WifiProfile.SSId,
                           sizeof(CbmrConfigData.WifiProfile.SSId),
                           SSIdName);
    if (EFI_ERROR(Status)) {
      DEBUG ((DEBUG_ERROR, "[cBMR] ERROR: SSIdName length overrun of allowed EFI_MS_CBMR_WIFI_NETWORK_PROFILE size\n"));
      return Status;
    }
    CbmrConfigData.WifiProfile.SSIdLength = AsciiStrLen(SSIdName);

    Status = AsciiStrCpyS (CbmrConfigData.WifiProfile.Password,
                           sizeof(CbmrConfigData.WifiProfile.Password),
                           SSIdPassword);
    if (EFI_ERROR(Status)) {
      DEBUG ((DEBUG_ERROR, "[cBMR] ERROR: SSIdPassword length overrun of allowed EFI_MS_CBMR_WIFI_NETWORK_PROFILE size\n"));
      return Status;
    }
    CbmrConfigData.WifiProfile.PasswordLength = AsciiStrLen(SSIdPassword);
  }

  //
  // Call cBMR protocol configuration function
  //

  Status = CbmrProtocol->Configure (CbmrProtocol,
                                    &CbmrConfigData,
                                    ProgressCallback);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[cBMR] ERROR: EFI_MS_CBMR_PROTOCOL::Configure( %a ) - Status %r\n", ((UseWiFi) ? "WiFi" : "Wired"), Status));
    return Status;
  }

  //
  // The process is ready, initiate the OS image download
  //

  Status = CbmrProtocol->Start (CbmrProtocol);

  // 
  // NOTE:  Code should never get to this point.
  //        The start should jump to the cBMR driver which will provide status through the callback then on success
  //        will initiate the boot of the Stub-OS and never return.  The code after this point is for error handling.
  //

  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_ERROR, "[cBMR] ERROR: EFI_MS_CBMR_PROTOCOL::Start() - Status %r\n", Status));
  }

  else {
    DEBUG ((DEBUG_ERROR, "[cBMR] ERROR: The Cloud Bare Metal Recovery process exited unexpectedly\n"));

    DataSize = sizeof(ErrorData);
    Status = CbmrProtocol->GetData (CbmrProtocol, EfiMsCbmrExtendedErrorData, &ErrorData, &DataSize);

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "[cBMR] ERROR: EFI_MS_CBMR_PROTOCOL::GetData ( EfiMsCbmrExtendedErrorData ) - Status %r\n", Status));
    }

    else {
      DEBUG ((DEBUG_ERROR, "[cBMR] ERROR: EFI_MS_CBMR_ERROR_DATA::Status:   %r\n", ErrorData.Status));
      DEBUG ((DEBUG_ERROR, "[cBMR] ERROR: EFI_MS_CBMR_ERROR_DATA::StopCode: 0x%08x\n", ErrorData.StopCode));
      DEBUG ((DEBUG_ERROR, "[cBMR]        CBMR defined stop codes with extended error info at https://aka.ms/systemrecoveryerror\n"));
      Status = ErrorData.Status;
    }
  }

  CbmrProtocol->Close (CbmrProtocol);

  return Status;
}

