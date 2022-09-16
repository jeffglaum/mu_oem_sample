/** @file MsCbmrProcessSampleLib.c

  cBMR Process Sample Library

  Copyright (c) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  The library is intended to be a sample of how to initiate the cBMR (Cloud Bare Metal Recovery) process and this file
  specifically contains the primary entry function to initiate the entire process.
**/

#include "MsCbmrProcessSampleLib.h"


/**
  Using the cBMR collateral and current progress, the function calculates the % complete value and returns a decimal
  between 0 and 100.

  @param[in]  Collaterals      Pointer to the collateral buffer collected from the cBMR driver
  @param[in]  CollateralCount  Number of structures in the Collaterals buffer
  @param[in]  Progress         Pointer to the current progress status structure

  @retval     Percentage complete
**/
UINTN
EFIAPI
CalculatePercentComplete(
  IN EFI_MS_CBMR_COLLATERAL                     *Collaterals,
  IN UINTN                                      CollateralCount,
  IN EFI_MS_CBMR_COLLATERALS_DOWNLOAD_PROGRESS  *Progress)
{
  UINTN CurrentAmt = 0;
  UINTN TotalAmt = 0;
  UINTN x;

  for (x = 0; x < CollateralCount; x++) {
    if (x == Progress->CollateralIndex) {
      CurrentAmt = TotalAmt + Progress->CollateralDownloadedSize;
    }
    TotalAmt += Collaterals[x].CollateralSize;
  }

  return (CurrentAmt * 100) / TotalAmt;
}

/**
  Callback function initiated by the cBMR driver to provide status on each HTTP packet received

  @param[in]  This      Pointer to the protocol produced by the driver initiating the callback
  @param[in]  Progress  Progress structure providing status

  @retval     EFI_STATUS
**/
EFI_STATUS
EFIAPI
CbmrExampleLibProgressCallback (
  IN EFI_MS_CBMR_PROTOCOL  *This,
  IN EFI_MS_CBMR_PROGRESS  *Progress)
{
  static EFI_MS_CBMR_COLLATERAL *Collaterals = NULL;
  static UINTN CollateralCount = 0;
  EFI_STATUS Status;

  // Input check
  if (This == NULL || Progress == NULL) {
    ASSERT (This);
    ASSERT (Progress);
    return EFI_INVALID_PARAMETER;
  }

  // Main switch to handle the phase indicator
  switch (Progress->CurrentPhase) {

    // Configuration phase start
    case MsCbmrPhaseConfiguring:
      DEBUG ((DEBUG_INFO, "[cBMR Callback]  MsCbmrPhaseConfiguring\n"));
      break;

    // Configuration phase finished
    case MsCbmrPhaseConfigured:
      DEBUG ((DEBUG_INFO, "[cBMR Callback]  MsCbmrPhaseConfigured\n"));
      break;

    // Periodic callback when downloading data
    case MsCbmrPhaseCollateralsDownloading:
      DEBUG ((DEBUG_INFO, "[cBMR Callback]  MsCbmrPhaseCollateralsDownloading\n"));

      ASSERT (Collaterals != NULL);
      ASSERT (CollateralCount != 0);
      DEBUG ((DEBUG_INFO,
              "                 CollateralIndex          = %d\n",
              Progress->ProgressData.DownloadProgress.CollateralIndex));
      DEBUG ((DEBUG_INFO,
              "                 CollateralDownloadedSize = %d\n",
              Progress->ProgressData.DownloadProgress.CollateralDownloadedSize));
      DEBUG ((DEBUG_INFO,
              "                 Percent Complete         = %d%%\n",
              CalculatePercentComplete(Collaterals, CollateralCount, &(Progress->ProgressData.DownloadProgress))));
      break;

    // Collateral data has been collected from the network and is available
    case MsCbmrPhaseCollateralsDownloaded:
      DEBUG ((DEBUG_INFO, "[cBMR Callback]  MsCbmrPhaseCollateralsDownloaded\n"));

      Status = CbmrDownloadCollaterals(This, &Collaterals, &CollateralCount);
      if (EFI_ERROR(Status)) {
        return Status;
      }
      break;

    // Network servicing periodic callback
    case MsCbmrPhaseServicingOperations:
      DEBUG ((DEBUG_INFO, "[cBMR Callback]  MsCbmrPhaseServicingOperations\n"));
      break;

    // Final callback prior to jumping to Stub-OS
    case MsCbmrPhaseStubOsRamboot:
      DEBUG ((DEBUG_INFO, "[cBMR Callback]  MsCbmrPhaseStubOsRamboot\n"));

      DEBUG ((DEBUG_INFO, "                 Final callback prior to Stub-OS Handoff"));
      break;
  }

  return EFI_SUCCESS;
}

/**
  Primary entry point to the library to initiate the entire cBMR process

  @param[in]  UseWiFi           TRUE if the process should attempt to attach to a WiFi access point, FALSE for wired
  @param[in]  SSIdName          SSID string used to attach to the WiFi access point. May be NULL if UseWifi is FALSE.
  @param[in]  SSIdPwd           Password  string used to attach to the WiFi access point. May be NULL if UseWifi is FALSE.
  @param[in]  ProgressCallback  Callback function to receive progress information.  May be NULL to use this library's default handler.

  @retval     EFI_STATUS
**/
EFI_STATUS
EFIAPI
ExecuteCbmrProcess (
  IN BOOLEAN                        UseWiFi,
  IN CHAR8                          *SSIdName,         OPTIONAL
  IN CHAR8                          *SSIdPwd,          OPTIONAL
  IN EFI_MS_CBMR_PROGRESS_CALLBACK  ProgressCallback)  OPTIONAL
{
  EFI_STATUS Status;

  //
  // Input check
  //

  DEBUG ((DEBUG_INFO, "[cBMR] Cloud Bare Metal Recovery process sample library\n"));
  DEBUG ((DEBUG_INFO, "       Copyright (c) Microsoft Corporation. All rights reserved.\n"));
  DEBUG ((DEBUG_INFO, "       SPDX-License-Identifier: BSD-2-Clause-Patent\n"));

  if (UseWiFi && (SSIdName == NULL || SSIdPwd == NULL)) {
    ASSERT(SSIdName);
    ASSERT(SSIdPwd);
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "[cBMR] Inputs:\n"));
  DEBUG ((DEBUG_INFO, "       Use WiFi:   %a\n", ((UseWiFi) ? "TRUE" : "FALSE")));
  DEBUG ((DEBUG_INFO, "       SSID Name:  %a\n", ((SSIdName == NULL) ? "<none>" : SSIdName)));
  DEBUG ((DEBUG_INFO, "       Password:   %a\n", ((SSIdPwd == NULL) ? "<none>" : SSIdPwd)));
  DEBUG ((DEBUG_INFO, "       Callback:   %a\n", ((ProgressCallback == NULL) ? "Using sample callback" : "Using caller provided callback")));

  if (ProgressCallback == NULL) {
    ProgressCallback = CbmrExampleLibProgressCallback;
  }

  //
  // Connect to WiFi access point if requested
  //

  if (UseWiFi) {
    Status = ConnectToWiFiAccessPoint (SSIdName, SSIdPwd);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  //
  // Request a network connection
  //

  Status = ConnectToNetwork();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Initiate the cBMR recovery process
  //

  Status = InitiateRecoveryProcess (UseWiFi,
                                    SSIdName,
                                    SSIdPwd,
                                    ProgressCallback);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return Status;
}

