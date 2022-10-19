/** @file CbmrProcessSampleLib.c

  cBMR Process Sample Library

  Copyright (c) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  The library is intended to be a sample of how to initiate the cBMR (Cloud Bare Metal Recovery) process and this file
  specifically contains the primary entry function to initiate the entire process.
**/

#include "CbmrProcessCommon.h"

// Globals used to hold the cBMR driver collateral list that can be used across callbacks
static EFI_MS_CBMR_COLLATERAL *gCollaterals = NULL;
static UINTN gCollateralCount = 0;


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

// #### ERROR TODO ####
// FOUND A NULL 'This' pointer on a callback after download finished
  if (This == NULL) {
    DEBUG ((DEBUG_ERROR, "#### ERROR ####  [%a]  'This' pointer = %p\n", __FUNCTION__, This));
    // Can continue, This is currently not used
  }
  if (Progress == NULL) {
    DEBUG ((DEBUG_ERROR, "#### ERROR ####  [%a]  'Progress' pointer = %p\n", __FUNCTION__, Progress));
    return EFI_SUCCESS;
  }
////  // Input check
////  if (This == NULL || Progress == NULL) {
////    ASSERT (This);
////    ASSERT (Progress);
////    return EFI_INVALID_PARAMETER;
////  }

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

    // Periodic callback when downloading collaterals
    case MsCbmrPhaseCollateralsDownloading:
      DEBUG ((DEBUG_INFO, "[cBMR Callback]  MsCbmrPhaseCollateralsDownloading\n"));

      ASSERT (gCollaterals != NULL);
      ASSERT (gCollateralCount != 0);

      DEBUG ((DEBUG_INFO, "    Collateral Data Block #%d\n", Progress->ProgressData.DownloadProgress.CollateralIndex + 1));
      DEBUG ((DEBUG_INFO, "        Current Amt  = 0x%012X Bytes\n", Progress->ProgressData.DownloadProgress.CollateralDownloadedSize));
      DEBUG ((DEBUG_INFO, "        Expected Amt = "));
      if (Progress->ProgressData.DownloadProgress.CollateralIndex >= gCollateralCount) {
        DEBUG ((DEBUG_INFO, "<unknown> Bytes\n"));
      } else {
        DEBUG ((DEBUG_INFO, "0x%012X Bytes\n", gCollaterals[Progress->ProgressData.DownloadProgress.CollateralIndex].CollateralSize));
      }
      break;

    // Collateral data has finished it's download process
    case MsCbmrPhaseCollateralsDownloaded:
      DEBUG ((DEBUG_INFO, "[cBMR Callback]  MsCbmrPhaseCollateralsDownloaded\n"));
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
  EFI_MS_CBMR_PROTOCOL *CbmrProtocol;
  EFI_STATUS Status;
  EFI_IP4_CONFIG2_INTERFACE_INFO *InterfaceInfo;

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
  // Try connecting to either a wired LAN or a wireless network.
  //
  Status = (UseWiFi ? ConnectToWiFiAccessPoint (SSIdName, SSIdPwd) : ConnectToNetwork(&InterfaceInfo));
  
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Locate the cBMR protocol interface
  //

  Status = LocateCbmrProtocol(&CbmrProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Initialize the cBMR driver
  //

  Status = InitCbmrDriver (CbmrProtocol,
                           UseWiFi,
                           SSIdName,
                           SSIdPwd,
                           ProgressCallback);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Collect collaterals for the up-coming download process
  //

  Status = DownloadCbmrCollaterals(CbmrProtocol,
                                   &gCollaterals,
                                   &gCollateralCount);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  //
  // The process is ready, initiate the OS image download
  //
  // NOTE:  Code should never return from this call.  The start will initiate the download process that executes the
  //        periodic callback for status then jumps to the Stub-OS boot process.  The code after this point is for
  //        error handling.
  //

  Status = LaunchStubOS (CbmrProtocol);
  FreePool(gCollaterals);
  CbmrProtocol->Close (CbmrProtocol);
  return Status;
}

