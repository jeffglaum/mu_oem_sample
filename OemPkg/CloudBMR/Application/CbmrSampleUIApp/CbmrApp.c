/** @file CbmrApp.c

  cBMR (Cloud Bare Metal Recovery) sample application with user interface

  Copyright (c) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  The application is a sample, demonstrating how one might present the cBMR process to a user.
**/

#include "CbmrApp.h"

// Global definitions.
//
CBMR_APP_CONTEXT  gAppContext;
EFI_HANDLE        gDialogHandle;

static PEFI_MS_CBMR_COLLATERAL  gCbmrCollaterals;
static UINTN                    gNumberOfCollaterals;
static UINTN                    gAllCollateralsSize;
static UINTN                    gAllCollateralsRunningSize;

// External definitions.
//
extern UINT32  PcdCbmrGraphicsMode;

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
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: [%a]  Progress callback pointer = %p.\n", __FUNCTION__, Progress));
    return EFI_SUCCESS;
  }

  switch (Progress->CurrentPhase) {
    // Configuration phase start
    case MsCbmrPhaseConfiguring:
      DEBUG ((DEBUG_INFO, "INFO [cBMR App]: Progress callback: MsCbmrPhaseConfiguring.\n"));
      CbmrUIUpdateLabelValue (cBMRState, L"Configuring...");
      break;

    // Configuration phase finished
    case MsCbmrPhaseConfigured:
      DEBUG ((DEBUG_INFO, "INFO [cBMR App]: Progress callback: MsCbmrPhaseConfigured.\n"));
      CbmrUIUpdateLabelValue (cBMRState, L"Configured.");
      break;

    // Periodic callback when downloading collaterals
    case MsCbmrPhaseCollateralsDownloading:
      DEBUG ((DEBUG_INFO, "INFO [cBMR App]: Progress callback: MsCbmrPhaseCollateralsDownloading.\n"));
      CbmrUIUpdateLabelValue (cBMRState, L"Downloading StubOS...");

 #if 0
      UINTN  PerCollateralRunningSize = Progress->ProgressData.DownloadProgress
                                          .CollateralDownloadedSize;
      UINTN  PerCollateralSize               = gCbmrCollaterals[CollateralIndex].CollateralSize;
      UINTN  PerCollateralProgressPercentage = (100 * PerCollateralRunningSize) /
                                               PerCollateralSize;
 #endif

      // UINTN  AllCollateralsRunningSize = gAllCollateralsRunningSize + PerCollateralRunningSize;
      // UINTN  AllCollateralsSize = gAllCollateralsSize;
      CbmrUIUpdateDownloadProgress (
        (UINT8)((100 * (gAllCollateralsRunningSize +
                        Progress->ProgressData.DownloadProgress.CollateralDownloadedSize)) /
                gAllCollateralsSize)
        );

      break;

    // Collateral data has finished it's download process
    case MsCbmrPhaseCollateralsDownloaded:
      DEBUG ((DEBUG_INFO, "INFO [cBMR App]: Progress callback: MsCbmrPhaseCollateralsDownloaded.\n"));
      CbmrUIUpdateLabelValue (cBMRState, L"Downloaded StubOS.");
      break;

    // Network servicing periodic callback
    case MsCbmrPhaseServicingOperations:
      DEBUG ((DEBUG_INFO, "INFO [cBMR App]: Progress callback: MsCbmrPhaseServicingOperations.\n"));
      CbmrUIUpdateLabelValue (cBMRState, L"Servicing operations...");
      break;

    // Final callback prior to jumping to Stub-OS
    case MsCbmrPhaseStubOsRamboot:
      DEBUG ((DEBUG_INFO, "INFO [cBMR App]: Progress callback: MsCbmrPhaseStubOsRamboot.\n"));
      CbmrUIUpdateLabelValue (cBMRState, L"Jumping to StubOS...");
      break;

    default:
      DEBUG ((DEBUG_WARN, "WARN [cBMR App]: Unknown progress phase (%d).\n", (UINT32)Progress->CurrentPhase));
      break;
  }

  return EFI_SUCCESS;
}

static
EFI_STATUS
EFIAPI
UpdateNetworkInterfaceUI (
  IN EFI_IP4_CONFIG2_INTERFACE_INFO  *InterfaceInfo
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  CHAR16      SSIDName[SSID_MAX_NAME_LENGTH];

  // Show connected status.
  //
  CbmrUIUpdateLabelValue (NetworkState, L"Connected");
  AsciiStrToUnicodeStrS (gAppContext.SSIDNameA, SSIDName, SSID_MAX_NAME_LENGTH);
  CbmrUIUpdateLabelValue (NetworkSSID, (gAppContext.bUseWiFiConnection ? SSIDName : L"N/A (Ethernet)"));

  // Show network policy type (DHCP vs. Static IP).
  //
  CbmrUIUpdateLabelValue (NetworkPolicy, (gAppContext.NetworkPolicy == Ip4Config2PolicyStatic ? L"Static" : L"DHCP"));

  // Show IP address assigned.
  //
  CHAR16  IpAddressString[25];

  UnicodeSPrint (
    IpAddressString,
    sizeof (IpAddressString),
    L"%u.%u.%u.%u",
    InterfaceInfo->StationAddress.Addr[0],
    InterfaceInfo->StationAddress.Addr[1],
    InterfaceInfo->StationAddress.Addr[2],
    InterfaceInfo->StationAddress.Addr[3]
    );
  DEBUG ((DEBUG_INFO, "INFO [cBMR App]: IP Address: %s.\r\n", IpAddressString));
  CbmrUIUpdateLabelValue (NetworkIPAddr, IpAddressString);

  // Show Gateway address.
  //
  CHAR16  GatewayAddressString[25];

  for (UINTN j = 0; j < InterfaceInfo->RouteTableSize; j++) {
    EFI_IP4_ROUTE_TABLE  *RoutingTable = &InterfaceInfo->RouteTable[j];
    if ((RoutingTable->GatewayAddress.Addr[0] == 0) &&
        (RoutingTable->GatewayAddress.Addr[1] == 0) &&
        (RoutingTable->GatewayAddress.Addr[2] == 0) &&
        (RoutingTable->GatewayAddress.Addr[3] == 0))
    {
      continue;
    }

    UnicodeSPrint (
      GatewayAddressString,
      sizeof (GatewayAddressString),
      L"%u.%u.%u.%u",
      RoutingTable->GatewayAddress.Addr[0],
      RoutingTable->GatewayAddress.Addr[1],
      RoutingTable->GatewayAddress.Addr[2],
      RoutingTable->GatewayAddress.Addr[3]
      );
    DEBUG ((DEBUG_INFO, "INFO [cBMR App]: Gateway Address: %s.\r\n", GatewayAddressString));
    CbmrUIUpdateLabelValue (NetworkGatewayAddr, GatewayAddressString);
    break;
  }

  // Show DNS Server address.
  //
  EFI_IPv4_ADDRESS  DNSIpAddress;

  Status = GetDNSServerIpAddress (&DNSIpAddress);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to find DNS Server address (%r).\n", Status));
    goto Exit;
  }

  CHAR16  DNSAddressString[25];

  UnicodeSPrint (
    DNSAddressString,
    sizeof (DNSAddressString),
    L"%u.%u.%u.%u",
    DNSIpAddress.Addr[0],
    DNSIpAddress.Addr[1],
    DNSIpAddress.Addr[2],
    DNSIpAddress.Addr[3]
    );
  DEBUG ((DEBUG_INFO, "INFO [cBMR App]: DNS Server Address: %s.\r\n", DNSAddressString));
  CbmrUIUpdateLabelValue (NetworkDNSAddr, DNSAddressString);
Exit:

  return Status;
}

/**
  cBMR UEFI application entry point.

  @param[in]  ImageHandle     Application image handle.
  @param[in]  SystemTable     Pointer to the system table.

  @retval     EFI_STATUS
**/
EFI_STATUS
EFIAPI
CbmrAppEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                      Status        = EFI_SUCCESS;
  UINT32                          PreviousMode  = 0;
  Canvas                          *WindowCanvas = NULL;
  EFI_GUID                        DialogGuid    = CBMR_APP_DIALOG_PROTOCOL_GUID;
  CHAR16                          GenericString[DATA_LABEL_MAX_LENGTH];
  EFI_IP4_CONFIG2_INTERFACE_INFO  *InterfaceInfo = NULL;

  // Initialize application context.
  //
  gAppContext.bUseWiFiConnection = FALSE;

  // Set the working graphics resolution.
  //
  Status = GfxSetGraphicsResolution (FixedPcdGet32 (PcdCbmrGraphicsMode), &PreviousMode);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to set desired graphics resolution (%r).\n", Status));
    goto Exit;
  }

  // Obtain a new handle for app pop-up dialogs.
  //
  Status = gBS->InstallProtocolInterface (&gDialogHandle, &DialogGuid, EFI_NATIVE_INTERFACE, NULL);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to create dialog window handle (%r).\r\n", Status));
    goto Exit;
  }

  // Initialize the Simple UI ToolKit for presentation.
  //
  Status = InitializeUIToolKit (ImageHandle);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to initialize the UI toolkit (%r).\r\n", Status));
    goto Exit;
  }

  // Create application main window.
  //
  Status = CbmrUICreateWindow (&WindowCanvas);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to initialize application window (%r).\r\n", Status));
    goto Exit;
  }

  // Ready.  Wait for user input to either proceed with cBMR or to cancel.
  //
  CbmrUIUpdateLabelValue (cBMRState, L"Ready");

  SWM_MB_RESULT  Result = CbmrUIWindowMessageHandler (
                            WindowCanvas
                            );

  // If the user decided to cancel, exit.
  //
  if (Result == SWM_MB_IDCANCEL) {
    Status = EFI_SUCCESS;
    goto Exit;
  }

  // Connect to the network (tries wired LAN first then falls back to Wi-Fi if that fails).
  //
  Status = FindAndConnectToNetwork (&InterfaceInfo);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to connect to the network (%r).\r\n", Status));
    goto Exit;
  }

  // Display network connection details.
  //
  UpdateNetworkInterfaceUI (InterfaceInfo);

  // Locate cBMR protocol.
  //
  CbmrUIUpdateLabelValue (cBMRState, L"Locating cBMR driver...");

  EFI_MS_CBMR_PROTOCOL  *CbmrProtocolPtr;

  Status = gBS->LocateProtocol (
                  &gEfiMsCbmrProtocolGuid,
                  NULL,
                  (VOID **)&CbmrProtocolPtr
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to locate cBMR (driver) protocol (%r).\r\n", Status));
    goto Exit;
  }

  // Configure cBMR (driver) protocol.
  //
  CbmrUIUpdateLabelValue (cBMRState, L"Configuring cBMR driver...");
  EFI_MS_CBMR_CONFIG_DATA  CbmrConfigData;

  SetMem (&CbmrConfigData, sizeof (EFI_MS_CBMR_CONFIG_DATA), 0);
  if (gAppContext.bUseWiFiConnection) {
    AsciiStrCpyS (
      CbmrConfigData.WifiProfile.SSId,
      sizeof (CbmrConfigData.WifiProfile.SSId),
      gAppContext.SSIDNameA
      );
    CbmrConfigData.WifiProfile.SSIdLength = AsciiStrLen (gAppContext.SSIDNameA);
    AsciiStrCpyS (
      CbmrConfigData.WifiProfile.Password,
      sizeof (CbmrConfigData.WifiProfile.Password),
      gAppContext.SSIDPasswordA
      );
    CbmrConfigData.WifiProfile.PasswordLength = AsciiStrLen (gAppContext.SSIDPasswordA);
  }

  Status = CbmrProtocolPtr->Configure (
                              CbmrProtocolPtr,
                              &CbmrConfigData,
                              (EFI_MS_CBMR_PROGRESS_CALLBACK)CbmrAppProgressCallback
                              );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to configure cBMR protocol (%r).\r\n", Status));
    goto Exit;
  }

  // Fetch cBMR download collateral information.
  //
  CbmrUIUpdateLabelValue (cBMRState, L"Fetching manifest...");

  UINTN  DataSize = 0;

  Status = CbmrProtocolPtr->GetData (CbmrProtocolPtr, EfiMsCbmrCollaterals, NULL, &DataSize);
  if (EFI_ERROR (Status) && (Status != EFI_BUFFER_TOO_SMALL)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to get cBMR collateral size (%r).\r\n", Status));
    goto Exit;
  }

  gCbmrCollaterals = AllocateZeroPool (DataSize);
  if (gCbmrCollaterals == NULL) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to memory buffer for cBMR collateral  (%r).\r\n", Status));

    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Status = CbmrProtocolPtr->GetData (CbmrProtocolPtr, EfiMsCbmrCollaterals, gCbmrCollaterals, &DataSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to fetch cBMR collateral (%r).\r\n", Status));
    goto Exit;
  }

  gNumberOfCollaterals = DataSize / sizeof (EFI_MS_CBMR_COLLATERAL);

  for (UINTN i = 0; i < gNumberOfCollaterals; i++) {
    gAllCollateralsSize += gCbmrCollaterals[i].CollateralSize;
  }

  UnicodeSPrint (GenericString, sizeof (GenericString), L"%d", gNumberOfCollaterals);
  CbmrUIUpdateLabelValue (DownloadFileCount, GenericString);
  UnicodeSPrint (GenericString, sizeof (GenericString), L"%d MB", (gAllCollateralsSize / (1024 * 1024)));
  CbmrUIUpdateLabelValue (DownloadTotalSize, GenericString);

  // Start cBMR download.
  //
  Status = CbmrProtocolPtr->Start (CbmrProtocolPtr);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to start cBMR download (%r).\r\n", Status));
    goto Exit;
  }

Exit:

  // Clean-up.
  //
  if (gDialogHandle != NULL) {
    gBS->UninstallMultipleProtocolInterfaces (
           gDialogHandle,
           &DialogGuid,
           NULL
           );
  }

  if (gCbmrCollaterals != NULL) {
    FreePool (gCbmrCollaterals);
  }

  if (InterfaceInfo != NULL) {
    FreePool (InterfaceInfo);
    InterfaceInfo = NULL;
  }

  return Status;
}
