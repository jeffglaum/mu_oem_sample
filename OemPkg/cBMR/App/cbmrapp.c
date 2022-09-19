/*++

Copyright (c) 2021 Microsoft Corporation

Module Name:

  cbmrapp.c

Abstract:

  This module implements cbmr application

Author:

  Vineel Kovvuri (vineelko) 01-Jun-2021

Environment:

  UEFI mode only.

--*/

#include "CbmrApp.h"

#include <Protocol/SimpleWindowManager.h>

#include <UIToolKit/SimpleUIToolKit.h>

EFI_SHELL_PROTOCOL* gEfiShellProtocol = NULL;
PEFI_MS_CBMR_COLLATERAL gCbmrCollaterals = NULL;
UINTN gNumberOfCollaterals = 0;
CBMR_CONFIG gCbmrConfig;

EFI_STATUS
EFIAPI
CbmrAppInit ()
{
  EFI_STATUS Status;

  // Read the application configuration
  gCbmrConfig.ShowWiFiUX = PcdGetBool (PcdCbmrShowWiFiUX);
  gCbmrConfig.WifiSid = (CHAR8*) PcdGetPtr (PcdCbmrDefaultWifiSid);
  gCbmrConfig.WifiPwd = (CHAR8*) PcdGetPtr (PcdCbmrDefaultWifiPwd);
  ASSERT (AsciiStrLen(gCbmrConfig.WifiSid) <= EFI_MAX_SSID_LEN);
  ASSERT (AsciiStrLen(gCbmrConfig.WifiPwd) <= MAX_80211_PWD_LEN);
  if (AsciiStrLen(gCbmrConfig.WifiSid) > EFI_MAX_SSID_LEN || AsciiStrLen(gCbmrConfig.WifiPwd) > MAX_80211_PWD_LEN) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "cBMR App Configuration:\n"));
  DEBUG ((DEBUG_INFO, "  Show WiFi UX:  %a\n", ((gCbmrConfig.ShowWiFiUX) ? "TRUE" : "FALSE") ));
  DEBUG ((DEBUG_INFO, "  Default SID:   %a\n", ((gCbmrConfig.WifiSid[0] == 0) ? "<not set>" : gCbmrConfig.WifiSid) ));
  DEBUG ((DEBUG_INFO, "  Default PWD:   %a\n", ((gCbmrConfig.WifiPwd[0] == 0) ? "<not set>" : gCbmrConfig.WifiPwd) ));

  // Get hold of Shell protocol to respond to CTRL+C events
  Status = gBS->LocateProtocol(&gEfiShellProtocolGuid, NULL, (VOID**)&gEfiShellProtocol);
  if (EFI_ERROR(Status)) {
    // Not a fatal error, the app just won't respond to Ctrl+c
    gEfiShellProtocol = NULL;
    DEBUG ((DEBUG_WARN, "Warning:  Locating gEfiShellProtocolGuid returned status (%r), Key combination <Ctrl-C> can not be monitored\n", Status));
  }

  return EFI_SUCCESS;
}

BOOLEAN
EFIAPI
CbmrIsAppExecutionInterrupted ()
{
  if (gEfiShellProtocol == NULL) {
    return FALSE;
  }
  if (gBS->CheckEvent (gEfiShellProtocol->ExecutionBreak) != EFI_SUCCESS) {
    return FALSE;
  }
  return TRUE;
}

EFI_STATUS
EFIAPI
CbmrHandleExtendedErrorData (
  IN PEFI_MS_CBMR_PROTOCOL This)
{
  EFI_STATUS Status = EFI_SUCCESS;
  // WSSI
  //EFI_MS_CBMR_ERROR_DATA ErrorData = {0};
  EFI_MS_CBMR_ERROR_DATA ErrorData;

  CHAR16 ErrorStatus[512];
  UINTN DataSize = sizeof (ErrorData);

  // WSSI
  ZeroMem(&ErrorData, sizeof(EFI_MS_CBMR_ERROR_DATA));

  Status = This->GetData (This, EfiMsCbmrExtendedErrorData, &ErrorData, &DataSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GetData() failed for EfiMsCbmrExtendedErrorData (%r)\n", Status));
    goto Exit;
  }

  if ((!EFI_ERROR (ErrorData.Status)) && ErrorData.StopCode == 0) {
    UnicodeSPrint (ErrorStatus, ARRAY_SIZE (ErrorStatus), (CHAR16*)L" ");
  } else {
    UnicodeSPrint (ErrorStatus,
                   ARRAY_SIZE (ErrorStatus),
                   (CHAR16*)L"Stop code: 0x%08x (EFI Status: 0x%08x) for more info visit https://aka.ms/systemrecoveryerror",
                   ErrorData.StopCode,
                   ErrorData.Status);
  }

  CbmrUIUpdateApplicationStatus (ErrorStatus);
Exit:
  return Status;
}

EFI_STATUS
EFIAPI
CbmrAppProgressCallback (
  IN PEFI_MS_CBMR_PROTOCOL This,
  IN EFI_MS_CBMR_PROGRESS* Progress)
{
  EFI_STATUS Status = EFI_SUCCESS;

  switch (Progress->CurrentPhase) {

    case MsCbmrPhaseConfiguring:
      CbmrUIUpdateApplicationStatus (L"Configuring CBMR driver...");
      break;

    case MsCbmrPhaseConfigured:
      CbmrUIUpdateApplicationStatus (L"Configured CBMR driver...");
      break;

    case MsCbmrPhaseCollateralsDownloading:
      {
        CHAR16 DownloadStatusText[1024];
        UINTN CollateralIndex = Progress->ProgressData.DownloadProgress.CollateralIndex;
        UINTN CurrentDownloadSize = Progress->ProgressData.DownloadProgress.CollateralDownloadedSize;
        UINTN TotalCollateralSize = gCbmrCollaterals[CollateralIndex].CollateralSize;
  
        CbmrUIUpdateApplicationStatus (L"Downloading CBMR collaterals...");
        UnicodeSPrint (DownloadStatusText,
                       ARRAY_SIZE (DownloadStatusText),
                       (CHAR16*)L"%s to %s (%d/%d) bytes",
                       gCbmrCollaterals[CollateralIndex].RootUrl,
                       gCbmrCollaterals[CollateralIndex].FilePath,
                       CurrentDownloadSize,
                       TotalCollateralSize);
  
        Status = CbmrUIUpdateDownloadProgress ((CHAR16*)DownloadStatusText,
                                               (UINTN)((100 * CurrentDownloadSize) / TotalCollateralSize),
                                               (CollateralIndex * 100) / gNumberOfCollaterals);
        if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "CbmrUIUpdateDownloadProgress() failed (%r)\n", Status));
            goto Exit;
        }
      }
      break;

    case MsCbmrPhaseCollateralsDownloaded:
      Status = CbmrUIUpdateDownloadProgress (L"Collateral download finished", 100, 100);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "CbmrUIUpdateDownloadProgress() failed (%r)\n", Status));
        goto Exit;
      }
      break;

    case MsCbmrPhaseServicingOperations:
      Status = CbmrUIUpdateDownloadProgress (L"Performing servicing operations", 100, 100);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "CbmrUIUpdateDownloadProgress() failed (%r)\n", Status));
        goto Exit;
      }
      gBS->Stall (SEC_TO_uS (2));
      break;

    case MsCbmrPhaseStubOsRamboot:
      CbmrUIUpdateApplicationStatus (L"Rambooting to Stub OS");
      Status = CbmrUIUpdateDownloadProgress (L"     ", 100, 100);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "CbmrUIUpdateDownloadProgress() failed (%r)\n", Status));
        goto Exit;
      }
      CbmrUIUpdateApplicationStatus (L"Handoff to Stub OS ...");
      gBS->Stall (SEC_TO_uS (2));
      break;
  }

Exit:

  //
  // If the user hits Ctrl+C when the app is running terminate the application
  //
  if (CbmrIsAppExecutionInterrupted ()) {
    Status = EFI_ABORTED;
  }

  return Status;
}

EFI_STATUS
EFIAPI
CbmrInitializeNetworkAdapters ()
{
  EFI_STATUS Status = EFI_SUCCESS;
  EFI_HANDLE* Handles = NULL;
  UINTN HandleCount = 0;
  EFI_IP4_CONFIG2_PROTOCOL* Ip4Config2 = NULL;
  EFI_IP4_CONFIG2_POLICY Policy = Ip4Config2PolicyDhcp;

  Status = gBS->LocateHandleBuffer (ByProtocol,
                                    &gEfiIp4Config2ProtocolGuid,
                                    NULL,
                                    &HandleCount,
                                    &Handles);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "LocateHandleBuffer() failed (%r)\n", Status));
    goto Exit;
  }

  for (UINTN i = 0; i < HandleCount; i++) {
    Status = gBS->HandleProtocol (Handles[i], &gEfiIp4Config2ProtocolGuid, (VOID**)&Ip4Config2);
    if (EFI_ERROR (Status)) {
      Status = EFI_SUCCESS;
      continue;
    }

    //
    // This will set the adapter to get the IP from dhcp
    //
    Status = Ip4Config2->SetData (Ip4Config2,
                                  Ip4Config2DataTypePolicy,
                                  sizeof (EFI_IP4_CONFIG2_POLICY),
                                  &Policy);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "SetData() failed : (%r)\n", Status));
      goto Exit;
    }
  }

  //
  // Give it a couple of seconds to acquire the ip from dhcp source
  //
  gBS->Stall (SEC_TO_uS (5));

Exit:
  if (NULL != Handles) {
    FreePool (Handles);
  }
  return Status;
}

EFI_STATUS
EFIAPI
CbmrDumpNetworkInfo ()
{
  EFI_STATUS Status = EFI_SUCCESS;
  EFI_HANDLE* Handles = NULL;
  UINTN HandleCount = 0;
  EFI_IP4_CONFIG2_PROTOCOL* Ip4Config2 = NULL;
  EFI_IP4_CONFIG2_INTERFACE_INFO* InterfaceInfo = NULL;
  EFI_IP4_CONFIG2_POLICY Policy = Ip4Config2PolicyDhcp;
  UINTN Size = 0;

  Status = gBS->LocateHandleBuffer (ByProtocol,
                                    &gEfiIp4Config2ProtocolGuid,
                                    NULL,
                                    &HandleCount,
                                    &Handles);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "LocateHandleBuffer() failed : (%r)\n", Status));
    goto Exit;
  }

  for (UINTN i = 0; i < HandleCount; i++) {
    Status = gBS->HandleProtocol (Handles[i], &gEfiIp4Config2ProtocolGuid, (VOID**)&Ip4Config2);
    if (EFI_ERROR (Status)) {
      Status = EFI_SUCCESS;
      continue;
    }

    Size = 0;
    Status = Ip4Config2->GetData (Ip4Config2, Ip4Config2DataTypeInterfaceInfo, &Size, NULL);
    if (Status == EFI_BUFFER_TOO_SMALL) {
      InterfaceInfo = AllocateZeroPool (Size);
    }
    else if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "GetData() failed : (%r)\n", Status));
      goto Exit;
    }

    Status = Ip4Config2->GetData (Ip4Config2,
                                  Ip4Config2DataTypeInterfaceInfo,
                                  &Size,
                                  InterfaceInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "GetData() failed : (%r)\n", Status));
      goto Exit;
    }

    Size = sizeof (EFI_IP4_CONFIG2_POLICY);
    Status = Ip4Config2->GetData (Ip4Config2, Ip4Config2DataTypePolicy, &Size, &Policy);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "GetData() failed : (%r)\n", Status));
      goto Exit;
    }

    DEBUG ((DEBUG_INFO, "Interface Name: %s\n", InterfaceInfo->Name));
    DEBUG ((DEBUG_INFO, "Interface Type: %d (%a)\n", InterfaceInfo->IfType, InterfaceInfo->IfType == 1 ? "NET_IFTYPE_ETHERNET" : "UNKNOWN"));
    DEBUG ((DEBUG_INFO, "Policy: %a\n", Policy == Ip4Config2PolicyStatic ? "Static" : "Dhcp"));
    DEBUG ((DEBUG_INFO, "MAC Address: "));
    for (UINTN j = 0; j < InterfaceInfo->HwAddressSize; j++) {
      if (j > 0) {
        DEBUG ((DEBUG_INFO, "-"));
      }
      DEBUG ((DEBUG_INFO, "%02x", InterfaceInfo->HwAddress.Addr[j]));
    }
    DEBUG ((DEBUG_INFO, "\n"));

    DEBUG ((DEBUG_INFO,
            "IP Address: %d.%d.%d.%d\n",
            InterfaceInfo->StationAddress.Addr[0],
            InterfaceInfo->StationAddress.Addr[1],
            InterfaceInfo->StationAddress.Addr[2],
            InterfaceInfo->StationAddress.Addr[3]));

    DEBUG ((DEBUG_INFO,
            "Subnet Mask: %d.%d.%d.%d\n",
            InterfaceInfo->SubnetMask.Addr[0],
            InterfaceInfo->SubnetMask.Addr[1],
            InterfaceInfo->SubnetMask.Addr[2],
            InterfaceInfo->SubnetMask.Addr[3]));

    DEBUG ((DEBUG_INFO, "Routing Table:\n"));
    for (UINTN j = 0; j < InterfaceInfo->RouteTableSize; j++) {
      EFI_IP4_ROUTE_TABLE* RoutingTable = &InterfaceInfo->RouteTable[j];

      DEBUG ((DEBUG_INFO,
              "    Subnet Address: %d.%d.%d.%d\n",
              RoutingTable->SubnetAddress.Addr[0],
              RoutingTable->SubnetAddress.Addr[1],
              RoutingTable->SubnetAddress.Addr[2],
              RoutingTable->SubnetAddress.Addr[3]));

      DEBUG ((DEBUG_INFO,
              "    Subnet Mask: %d.%d.%d.%d\n",
              RoutingTable->SubnetMask.Addr[0],
              RoutingTable->SubnetMask.Addr[1],
              RoutingTable->SubnetMask.Addr[2],
              RoutingTable->SubnetMask.Addr[3]));

      DEBUG ((DEBUG_INFO,
              "    Gateway Address: %d.%d.%d.%d\n",
              RoutingTable->GatewayAddress.Addr[0],
              RoutingTable->GatewayAddress.Addr[1],
              RoutingTable->GatewayAddress.Addr[2],
              RoutingTable->GatewayAddress.Addr[3]));

      DEBUG ((DEBUG_INFO, "----------------------------------\n"));
    }
  }

Exit:
  FreePool (Handles);
  FreePool (InterfaceInfo);
  return Status;
}

EFI_STATUS
EFIAPI
CbmrInitializeWiFi (
  IN OUT PEFI_MS_CBMR_WIFI_NETWORK_PROFILE Profile)
{
  EFI_STATUS Status = EFI_SUCCESS;

  Status = CbmrInitializeNetworkAdapters ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "CbmrInitializeNetworkAdapters() failed (%r)\n", Status));
    goto Exit;
  }

  if (gCbmrConfig.ShowWiFiUX == FALSE && gCbmrConfig.WifiSid[0] == 0 && gCbmrConfig.WifiPwd[0] == 0) {
    DEBUG ((DEBUG_INFO, "Skipping Wi-Fi connectivity\n"));
    goto Exit;
  }

  DEBUG ((DEBUG_INFO, "Connecting to Wi-Fi\n"));

  if (gCbmrConfig.ShowWiFiUX == TRUE) {
    //
    // Launch Wi-Fi Connection UX
    //

    Status = WifiCmUIMain (Profile);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "WifiCmUIMain() failed (%r)\n", Status));
      goto Exit;
    }
  }
  else if (gCbmrConfig.WifiSid[0] != 0 && gCbmrConfig.WifiPwd[0] != 0) {
    //
    // By pass Wi-Fi Connection UX
    //

    Status = WifiCmConnect (gCbmrConfig.WifiSid, gCbmrConfig.WifiPwd);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "WifiCmConnect() failed (%r)\n", Status));
      goto Exit;
    }

    Profile->SSIdLength = AsciiStrnLenS (gCbmrConfig.WifiSid, EFI_MAX_SSID_LEN + 1);
    AsciiStrCpyS (Profile->SSId, ARRAY_SIZE (Profile->SSId), gCbmrConfig.WifiSid);
    Profile->PasswordLength = AsciiStrnLenS (gCbmrConfig.WifiPwd, MAX_80211_PWD_LEN + 1);
    AsciiStrCpyS (Profile->Password, ARRAY_SIZE (Profile->Password), gCbmrConfig.WifiPwd);
  }

  gBS->Stall (SEC_TO_uS (10));
  DEBUG ((DEBUG_INFO, "Connecting to Wi-Fi done\n"));

Exit:
  return Status;
}

EFI_STATUS
EFIAPI
CbmrAppEntry (
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE* SystemTable)
{
  EFI_STATUS Status = EFI_SUCCESS;
  PEFI_MS_CBMR_PROTOCOL CbmrProtocol = NULL;
  EFI_GUID CbmrProtocolGuid = EFI_MS_CBMR_PROTOCOL_GUID;
  // WSSI
  //EFI_MS_CBMR_CONFIG_DATA CbmrConfigData = {0};
  EFI_MS_CBMR_CONFIG_DATA CbmrConfigData;

  UINTN DataSize = 0;

  // WSSI
  ZeroMem(&CbmrConfigData, sizeof(EFI_MS_CBMR_CONFIG_DATA));

  Status = CbmrAppInit ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Initialize the Simple UI ToolKit.
  //
  Status = InitializeUIToolKit (ImageHandle);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [FP]: Failed to initialize the UI toolkit (%r).\r\n", Status));
    goto Exit;
  }

  DEBUG ((DEBUG_INFO, "Initializing Application UI\n"));
  Status = CbmrUIInitialize ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "CbmrUIInitialize () failed (%r)\n", Status));
    goto Exit;
  }
  DEBUG ((DEBUG_INFO, "Initializing Application UI done\n"));

  Status = CbmrInitializeWiFi (&CbmrConfigData.WifiProfile);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "CbmrInitializeWiFi () failed (%r)\n", Status));
    goto Exit;
  }

  Status = CbmrDumpNetworkInfo ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "CbmrDumpNetworkInfo () failed (%r)\n", Status));
    goto Exit;
  }

  //
  // Locate CBMR protocol
  //

  DEBUG ((DEBUG_INFO, "Locating CBMR protocol\n"));
  Status = gBS->LocateProtocol (&CbmrProtocolGuid, NULL, (VOID**)&CbmrProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "LocateProtocol () failed (%r)\n", Status));
    goto Exit;
  }

  DEBUG ((DEBUG_INFO, "Locating CBMR protocol done\n"));
  DEBUG ((DEBUG_INFO, "CBMR revision 0x%08llX\n", CbmrProtocol->Revision));

  //
  // Configure CBMR protocol
  //

  DEBUG ((DEBUG_INFO, "Configuring CBMR protocol instance\n"));
  Status = CbmrProtocol->Configure (CbmrProtocol,
                                    &CbmrConfigData,
                                    (EFI_MS_CBMR_PROGRESS_CALLBACK)CbmrAppProgressCallback);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Configure () failed (%r)\n", Status));
    CbmrHandleExtendedErrorData (CbmrProtocol);
    goto Exit;
  }
  DEBUG ((DEBUG_INFO, "Configuring CBMR protocol instance done\n"));

  //
  // Fetch all the collateral metadata
  //

  DEBUG ((DEBUG_INFO, "Getting collateral information\n"));
  Status = CbmrProtocol->GetData (CbmrProtocol, EfiMsCbmrCollaterals, NULL, &DataSize);
  if (EFI_ERROR (Status) && Status != EFI_BUFFER_TOO_SMALL) {
    DEBUG ((DEBUG_ERROR, "GetData () failed for EfiMsCbmrCollaterals (%r)\n", Status));
    goto Exit;
  }

  gCbmrCollaterals = AllocateZeroPool (DataSize);
  if (gCbmrCollaterals == NULL) {
    DEBUG ((DEBUG_ERROR, "Unable to allocate memory for get collaterals of size = %d", DataSize));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  Status = CbmrProtocol->GetData (CbmrProtocol, EfiMsCbmrCollaterals, gCbmrCollaterals, &DataSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GetData () failed for EfiMsCbmrCollaterals (%r)\n", Status));
    goto Exit;
  }

  gNumberOfCollaterals = DataSize / sizeof (EFI_MS_CBMR_COLLATERAL);

  DEBUG ((DEBUG_INFO, "Getting collateral information done\n"));

  for (UINTN i = 0; i < gNumberOfCollaterals; i++) {
    DEBUG ((DEBUG_INFO,
            "Url:%s  FilePath:%s  FileSize:%ull\n",
            gCbmrCollaterals[i].RootUrl,
            gCbmrCollaterals[i].FilePath,
            gCbmrCollaterals[i].CollateralSize));
  }

  //
  // Start CBMR process
  //

  DEBUG ((DEBUG_INFO, "Start CBMR process\n"));
  Status = CbmrProtocol->Start (CbmrProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Start () failed (%r)\n", Status));
    CbmrHandleExtendedErrorData (CbmrProtocol);
    goto Exit;
  }

Exit:

  //
  // Release CBMR protocol resources
  //
  // NOTE: Most likely this gets called only in error case. As in success case
  // the device will go for ramboot and the driver unload will take care of
  // cleaning up of CBMR protocol resources anyway
  //

  if (CbmrProtocol != NULL) {
    DEBUG ((DEBUG_INFO, "Closing CBMR protocol instance\n"));
    Status = CbmrProtocol->Close (CbmrProtocol);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Close () failed (%r)\n", Status));
    }
  }

  if (gCbmrCollaterals != NULL) {
    for (UINTN i = 0; i < gNumberOfCollaterals; i++) {
      FreePool (gCbmrCollaterals[i].RootUrl);
      FreePool (gCbmrCollaterals[i].FilePath);
    }

    FreePool (gCbmrCollaterals);
  }

  //
  // If execution is interrupted via Ctrl+C make sure the reset the console to
  // get back the shell prompt
  //

  if (CbmrIsAppExecutionInterrupted ()) {
    gST->ConOut->Reset (gST->ConOut, FALSE);
  }

  return Status;
}
