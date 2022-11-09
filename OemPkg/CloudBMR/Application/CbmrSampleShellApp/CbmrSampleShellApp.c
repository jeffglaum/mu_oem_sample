/** @file MsCbmrSampleShellApp.c

  Copyright (c) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  cBMR Process Initiation Sample Shell Application
**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/CbmrSupportLib.h>
#include <Library/PrintLib.h>
#include <Library/ShellLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/Shell.h>

#define MAX_CMD_LINE_ARG_SIZE  128

static UINTN  gAllCollateralSize;

/**
  Converts an Unicode command line argument to ASCII.  If the input Arg is longer than MAX_CMD_LINE_ARG_SIZE, the
  string is truncated and the output Arg buffer is still properly NULL terminated at the max length.

  @param[in]  UnicodeArgStr - Argument to convert/copy to the destination
  @param[in]  AsciiArgBuffer - Destination ASCII buffer that has at least MAX_CMD_LINE_ARG_SIZE bytes available
**/
VOID
EFIAPI
UnicodeArgToAsciiArgN (
  CONST CHAR16  *UnicodeArgStr,
  CHAR8         *AsciiArgBuffer
  )
{
  UINTN  DstMax = MAX_CMD_LINE_ARG_SIZE - 1;
  UINTN  x;

  for (x = 0; (x < DstMax) && (UnicodeArgStr[x] != 0x00); x++) {
    AsciiArgBuffer[x] = (CHAR8)UnicodeArgStr[x];
  }

  AsciiArgBuffer[x] = 0x00;
}

/**
  Callback that receives updates from the cBMR process sample library handling network negotiations and
  StubOS download as part of the cBMR process.

  @param[in]  This       Pointer to the cBMR protocol to use.
  @param[in]  Progress   Pointer to a structure containin progress stage and associated payload data.

  @retval     EFI_STATUS
**/
EFI_STATUS
EFIAPI
CbmrAppProgressCallback (
  IN PEFI_MS_CBMR_PROTOCOL  This,
  IN EFI_MS_CBMR_PROGRESS   *Progress
  )
{
  if (Progress == NULL) {
    DEBUG ((DEBUG_WARN, "WARN [cBMR App]: [%a]  Progress callback pointer = %p.\n", __FUNCTION__, Progress));
    return EFI_SUCCESS;
  }

  switch (Progress->CurrentPhase) {
    // Configuration phase start
    case MsCbmrPhaseConfiguring:
      DEBUG ((DEBUG_INFO, "INFO [cBMR App]: Progress callback: MsCbmrPhaseConfiguring.\n"));
      Print (L"INFO: Configuring cBMR driver...\n");
      break;

    // Configuration phase finished
    case MsCbmrPhaseConfigured:
      DEBUG ((DEBUG_INFO, "INFO [cBMR App]: Progress callback: MsCbmrPhaseConfigured.\n"));
      Print (L"INFO: cBMR driver configured.\n");
      break;

    // Periodic callback when downloading collaterals
    case MsCbmrPhaseCollateralsDownloading:
      DEBUG ((DEBUG_INFO, "INFO [cBMR App]: Progress callback: MsCbmrPhaseCollateralsDownloading.\n"));
      Print (L"INFO: Downloading cBMR collateral (%d%%)...\n", (UINT8)((100 * (Progress->ProgressData.DownloadProgress.CollateralDownloadedSize)) / gAllCollateralSize));
      break;

    // Collateral data has finished it's download process
    case MsCbmrPhaseCollateralsDownloaded:
      DEBUG ((DEBUG_INFO, "INFO [cBMR App]: Progress callback: MsCbmrPhaseCollateralsDownloaded.\n"));
      Print (L"INFO: cBMR collateral downloaded.\n");
      break;

    // Network servicing periodic callback
    case MsCbmrPhaseServicingOperations:
      DEBUG ((DEBUG_INFO, "INFO [cBMR App]: Progress callback: MsCbmrPhaseServicingOperations.\n"));
      Print (L"INFO: Performing network servicing...\n");
      break;

    // Final callback prior to jumping to Stub-OS
    case MsCbmrPhaseStubOsRamboot:
      DEBUG ((DEBUG_INFO, "INFO [cBMR App]: Progress callback: MsCbmrPhaseStubOsRamboot.\n"));
      Print (L"INFO: Jumping to StubOS...\n");
      break;

    default:
      DEBUG ((DEBUG_WARN, "WARN [cBMR App]: Unknown progress phase (%d).\n", (UINT32)Progress->CurrentPhase));
      break;
  }

  return EFI_SUCCESS;
}

/**
  Shell app entry point.
**/
EFI_STATUS
EFIAPI
CbmrSampleShellAppEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_SHELL_PARAMETERS_PROTOCOL   *ShellParams = NULL;
  EFI_STATUS                      Status       = EFI_SUCCESS;
  CHAR8                           AsciiArgV1[MAX_CMD_LINE_ARG_SIZE];
  EFI_IP4_CONFIG2_INTERFACE_INFO  *InterfaceInfo  = NULL;
  PEFI_MS_CBMR_COLLATERAL         CbmrCollaterals = NULL;

  // Init app
  //
  Print (L"Cloud Bare Metal Recovery - Sample Process Shell Application\n\n");
  Status = gBS->HandleProtocol (ImageHandle, &gEfiShellParametersProtocolGuid, (VOID **)&ShellParams);
  ASSERT_EFI_ERROR (Status);

  // Option 1) One argument of 'Wired' to indicate the app should use a wired connection
  //
  if (ShellParams->Argc == 2) {
    UnicodeArgToAsciiArgN (ShellParams->Argv[1], AsciiArgV1);

    if (0 == AsciiStriCmp ("Wired", AsciiArgV1)) {
      Print (L"INFO: Initating a wired connection download...\n");

      // Connect to wired (existing) LAN interface.
      //
      Status = ConnectToNetwork (&InterfaceInfo);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to connect to Wired LAN connection (%r).\r\n", Status));
        goto Exit;
      }

      goto StartCbmrProcess;
    }
  }

  // Option 2) Two arguments indicate the app should use a wireless connection where Arg1 is SSID and Arg2 is password
  //
  if (ShellParams->Argc == 3) {
    Print (L"INFO: Initating a WiFi connection download...\n");
    Print (L"INFO:     SSID:      %s\n", ShellParams->Argv[1]);
    Print (L"INFO:     Password:  %s\n", ShellParams->Argv[2]);

    CHAR8  SSIDNameA[SSID_MAX_NAME_LENGTH];
    CHAR8  SSIDPasswordA[SSID_MAX_PASSWORD_LENGTH];

    UnicodeArgToAsciiArgN (ShellParams->Argv[1], SSIDNameA);
    UnicodeArgToAsciiArgN (ShellParams->Argv[2], SSIDPasswordA);

    // Try to connect to specified Wi-Fi access point with password provided.
    //
    Status = ConnectToWiFiAccessPoint (SSIDNameA, SSIDPasswordA);

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to connect to specified Wi-Fi access point. (%r).\r\n", Status));
      goto Exit;
    }

    // Try to connect to the network (this time via the Wi-Fi connection).
    //
    Status = ConnectToNetwork (&InterfaceInfo);

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Unabled to connect to a (Wi-Fi) network (%r).\r\n", Status));
      goto Exit;
    }

    goto StartCbmrProcess;
  }

  // Fall through, the command line is invalid
  //
  Print (L"Invalid command line parameters, expecting one of two choices:\n");
  Print (L"    '%s Wired'              Attempt cBMR with a wired connection\n", ShellParams->Argv[0]);
  Print (L"    '%s <SSID> <Password>'  Attempt cBMR using WIFI SSID & PWD\n\n", ShellParams->Argv[0]);
  Status = EFI_INVALID_PARAMETER;
  goto Exit;

StartCbmrProcess:
  Print (L"INFO: Connected to network.\n");

  // Configured cBMR driver.
  //
  EFI_MS_CBMR_CONFIG_DATA  CbmrConfigData;

  SetMem (&CbmrConfigData, sizeof (EFI_MS_CBMR_CONFIG_DATA), 0);

  Status = CbmrDriverConfigure (&CbmrConfigData, CbmrAppProgressCallback);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to configure cBMR protocol (%r).\r\n", Status));
    goto Exit;
  }

  // Fetch cBMR download collateral information.
  //
  UINTN  CollateralDataSize  = 0;
  UINTN  NumberOfCollaterals = 0;

  Status = CbmrDriverFetchCollateral (&CbmrCollaterals, &CollateralDataSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to fetch cBMR collateral (%r).\r\n", Status));
    goto Exit;
  }

  NumberOfCollaterals = CollateralDataSize / sizeof (EFI_MS_CBMR_COLLATERAL);
  for (UINTN i = 0; i < NumberOfCollaterals; i++) {
    gAllCollateralSize += CbmrCollaterals[i].CollateralSize;
  }

  Print (L"INFO: cBMR collateral count=%d size=%d MB.\r\n", NumberOfCollaterals, (gAllCollateralSize / 1024 / 1024));

  // Start cBMR download.
  //
  Status = CbmrDriverStartDownload ();

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to start cBMR download (%r).\r\n", Status));
    goto Exit;
  }

Exit:

  if (CbmrCollaterals != NULL) {
    FreePool (CbmrCollaterals);
  }

  if (InterfaceInfo != NULL) {
    FreePool (InterfaceInfo);
    InterfaceInfo = NULL;
  }

  return Status;
}
