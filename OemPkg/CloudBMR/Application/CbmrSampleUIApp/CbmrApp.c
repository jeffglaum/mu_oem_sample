/** @file CbmrApp.c

  cBMR Sample Application with User Interface

  Copyright (c) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  The application is intended to be a sample of how to present cBMR (Cloud Bare Metal Recovery) process to the end user.
**/

#include "CbmrApp.h"

#define SSID_MAX_NAME_LENGTH      64
#define SSID_MAX_PASSWORD_LENGTH  64

// Dialog Protocol Guid
#define CBMR_APP_DIALOG_PROTOCOL_GUID  /* 567d4f03-6ff1-45cd-8fc5-9f192bc1450b */     \
{                                                                                  \
    0x567d4f03, 0x6ff1, 0x45cd, { 0x8f, 0xc5, 0x9f, 0x19, 0x2b, 0xc1, 0x45, 0x0b } \
}

PEFI_MS_CBMR_COLLATERAL  gCbmrCollaterals           = NULL;
UINTN                    gNumberOfCollaterals       = 0;
UINTN                    gAllCollateralsSize        = 0; // This is the sum of all collaterals fully downloaded size
UINTN                    gAllCollateralsRunningSize = 0; // This is the sum of all currently downloaded/downloading
                                                         // collaterals size
EFI_HANDLE       gDialogHandle = NULL;
static EFI_GUID  gDialogGuid   = CBMR_APP_DIALOG_PROTOCOL_GUID;

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
  // CHAR16  GenericString[64];

  if (This == NULL) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: [%a] 'This' pointer = %p.\n", __FUNCTION__, This));
    // Can continue, This is currently not used
  }

  if (Progress == NULL) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: [%a]  'Progress' pointer = %p.\n", __FUNCTION__, Progress));
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
      // UnicodeSPrint (GenericString, sizeof (GenericString), L"%d", Progress->ProgressData.DownloadProgress.CollateralIndex);
      // CbmrUIUpdateLabelValue (DownloadFileCount, GenericString);
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
  EFI_STATUS                      Status             = EFI_SUCCESS;
  UINT32                          PreviousMode       = 0;
  Canvas                          *WindowCanvas      = NULL;
  BOOLEAN                         bUseWiFiConnection = FALSE;
  CHAR8                           SSIDNameA[SSID_MAX_NAME_LENGTH];
  CHAR8                           SSIDPasswordA[SSID_MAX_PASSWORD_LENGTH];
  CHAR16                          SSIDName[SSID_MAX_NAME_LENGTH];
  CHAR16                          SSIDPassword[SSID_MAX_PASSWORD_LENGTH];
  CHAR16                          GenericString[64];
  EFI_IP4_CONFIG2_INTERFACE_INFO  *InterfaceInfo = NULL;

  ZeroMem (SSIDNameA, sizeof (SSIDNameA));
  ZeroMem (SSIDPasswordA, sizeof (SSIDPasswordA));
  ZeroMem (SSIDName, sizeof (SSIDName));
  ZeroMem (SSIDPassword, sizeof (SSIDPassword));

  //
  // Set the working graphics resolution.
  //

  Status = GfxSetGraphicsResolution (&PreviousMode);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to set desired graphics resolution (%r).\n", Status));
    goto Exit;
  }

  Status = gBS->InstallProtocolInterface (&gDialogHandle, &gDialogGuid, EFI_NATIVE_INTERFACE, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to create dialog window handle (%r).\r\n", Status));
    goto Exit;
  }

  //
  // Initialize the Simple UI ToolKit for presentation.
  //

  Status = InitializeUIToolKit (ImageHandle);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to initialize the UI toolkit (%r).\r\n", Status));
    goto Exit;
  }

  //
  // Initialize application user interface.
  //

  Status = CbmrUICreateWindow (&WindowCanvas);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to initialize application interface (%r).\r\n", Status));
    goto Exit;
  }

  //
  // Ready.  Wait for user input to either proceed or cancel.
  //

  CbmrUIUpdateLabelValue (cBMRState, L"Ready");

  SWM_MB_RESULT  Result = CbmrUIWindowMessageHandler (
                            WindowCanvas
                            );

  // User decided to cancel.
  if (Result == SWM_MB_IDCANCEL) {
    Status = EFI_SUCCESS;
    goto Exit;
  }

  //
  // Configure the network connection.
  //
  CbmrUIUpdateLabelValue (cBMRState, L"Connecting to network...");
  Status = ConnectToNetwork (&InterfaceInfo);

  if (EFI_ERROR (Status)) {
    // Present WiFi SSID list and try to connect.
    DEBUG ((DEBUG_INFO, "INFO [cBMR App]: Unable to connect to a wired LAN network, looking for a Wi-Fi access point.\r\n"));

    // Prompt the user for an SSID and password.
    Status = CbmrUIGetSSIDAndPassword (&SSIDName[0], SSID_MAX_NAME_LENGTH, &SSIDPassword[0], SSID_MAX_PASSWORD_LENGTH);

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "INFO [cBMR App]: Failed to retrieve Wi-Fi SSID and password from user. (%r).\r\n", Status));
      goto Exit;
    }

    DEBUG ((DEBUG_INFO, "INFO [cBMR App]: SSIDname=%s, SSIDpassword=%s (Status = %r).\r\n", SSIDName, SSIDPassword, Status));

    // Try to connect to specified Wi-Fi access point with password provided.
    UnicodeStrToAsciiStrS (SSIDName, SSIDNameA, SSID_MAX_NAME_LENGTH);
    UnicodeStrToAsciiStrS (SSIDPassword, SSIDPasswordA, SSID_MAX_PASSWORD_LENGTH);

    Status = ConnectToWiFiAccessPoint (SSIDNameA, SSIDPasswordA);

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "INFO [cBMR App]: Failed to connect to specified Wi-Fi access point. (%r).\r\n", Status));
      goto Exit;
    }

    bUseWiFiConnection = TRUE;
  }

  CbmrUIUpdateLabelValue (NetworkState, L"Connected");
  CbmrUIUpdateLabelValue (NetworkSSID, (bUseWiFiConnection ? SSIDName : L"N/A (Ethernet)"));

  // TODO
  // CbmrUIUpdateLabelValue( NetworkPolicy, Policy == Ip4Config2PolicyStatic ? t("Static") : t("DHCP"));

  // TODO - how to get IPv4 information from Wi-Fi connection?

  if (bUseWiFiConnection == FALSE) {
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

    // TODO - multiple routing table entries possible.
    EFI_IP4_ROUTE_TABLE  *RoutingTable = &InterfaceInfo->RouteTable[0];

    CHAR16  GatewayAddressString[25];

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

    // TODO - DNS server.

    FreePool (InterfaceInfo);
    InterfaceInfo = NULL;
  }

  //
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
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to locate cBMR protocol (%r).\r\n", Status));
    goto Exit;
  }

  //
  // Configure cBMR protocol.
  //
  CbmrUIUpdateLabelValue (cBMRState, L"Configuring cBMR driver...");
  EFI_MS_CBMR_CONFIG_DATA  CbmrConfigData;

  SetMem (&CbmrConfigData, sizeof (EFI_MS_CBMR_CONFIG_DATA), 0);
  if (bUseWiFiConnection) {
    AsciiStrCpyS (
      CbmrConfigData.WifiProfile.SSId,
      sizeof (CbmrConfigData.WifiProfile.SSId),
      SSIDNameA
      );
    CbmrConfigData.WifiProfile.SSIdLength = AsciiStrLen (SSIDNameA);
    AsciiStrCpyS (
      CbmrConfigData.WifiProfile.Password,
      sizeof (CbmrConfigData.WifiProfile.Password),
      SSIDPasswordA
      );
    CbmrConfigData.WifiProfile.PasswordLength = AsciiStrLen (SSIDPasswordA);
  }

  Status = CbmrProtocolPtr->Configure (
                              CbmrProtocolPtr,
                              &CbmrConfigData,
                              (EFI_MS_CBMR_PROGRESS_CALLBACK)CbmrAppProgressCallback
                              );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to configure cBMR protocol (%r).\r\n", Status));
    // CbmrShowDriverErrorInfo (CbmrProtocolPtr);
    goto Exit;
  }

  //
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

  //
  // Start cBMR download.
  //

  Status = CbmrProtocolPtr->Start (CbmrProtocolPtr);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to start cBMR download (%r).\r\n", Status));
    // CbmrShowDriverErrorInfo (CbmrProtocolPtr);
    goto Exit;
  }

Exit:

  if (gDialogHandle != NULL) {
    gBS->UninstallMultipleProtocolInterfaces (
           gDialogHandle,
           &gDialogGuid,
           NULL
           );
  }

  return Status;
}
