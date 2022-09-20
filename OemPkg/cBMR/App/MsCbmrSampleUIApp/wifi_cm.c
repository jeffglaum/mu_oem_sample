/*++

Copyright (c) 2021 Microsoft Corporation

Module Name:

  wifi_cm.c

Abstract:

  This module implements Wi-Fi connection manager UI

Author:

  Vineel Kovvuri (vineelko) 10-Mar-2021

Environment:

  UEFI mode only.

--*/

#include "cbmrapp.h"
#include "graphics_common.h"

EFI_EVENT WaitForNetworkOperation = NULL;

EFI_INPUT_KEY
EFIAPI
WifiCmGetCharNoEcho ()
{
  EFI_INPUT_KEY Key;
  UINTN Index = 0;

  gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &Index);
  gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);

  return Key;
}

VOID
EFIAPI
WifiCmNetworkOperationWaitCallback (
  IN EFI_EVENT Event,
  IN VOID* Context)
{
  //
  // Wait callbacks are triggered on every tick until the event is signaled.
  // So don't put anything here. Keep them empty!
  //
}

VOID
EFIAPI
WifiCmGetNetworksCallback (
  IN EFI_EVENT Event,
  IN VOID* Context)
{
  gBS->SignalEvent (WaitForNetworkOperation);
}

VOID
EFIAPI
WifiCmNetworkConnectCallback (
  IN EFI_EVENT Event,
  IN VOID* Context)
{
  gBS->SignalEvent (WaitForNetworkOperation);
}

INTN
EFIAPI
WifiCmNetworkDescriptionCompareFunc (
  IN CONST VOID* NetWorkDescription1,
  IN CONST VOID* NetWorkDescription2)
{
  return ((EFI_80211_NETWORK_DESCRIPTION*)NetWorkDescription2)->NetworkQuality -
         ((EFI_80211_NETWORK_DESCRIPTION*)NetWorkDescription1)->NetworkQuality;
}

VOID
EFIAPI
WifiCmFreeNetworkList (
  IN CHAR8** SsidList,
  IN UINTN SsidListLength)
{
  if (SsidList == NULL) {
    return;
  }

  for (UINTN i = 0; i < SsidListLength; i++) {
    FreePool (SsidList[i]);
  }

  FreePool (SsidList);
}

EFI_STATUS
EFIAPI
WifiCmGetNetworkList (
  OUT CHAR8*** SsidList,
  OUT UINTN* SsidListLength)
{
  EFI_STATUS Status = EFI_SUCCESS;
  EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL* ConMgr2Protocol = NULL;
  // WSSI
  //EFI_80211_GET_NETWORKS_TOKEN GetNetworksToken = {0};
  //EFI_80211_GET_NETWORKS_DATA GetData = {0};
  EFI_80211_GET_NETWORKS_TOKEN GetNetworksToken;
  EFI_80211_GET_NETWORKS_DATA GetData;
  EFI_80211_GET_NETWORKS_RESULT* NetworkList = NULL;
  UINTN Index = 0;
  CHAR8** RetSsidList = NULL;
  UINTN RetSsidListLength = 0;
  UINTN SsidLength = 0;

  // WSSI
  ZeroMem(&GetNetworksToken, sizeof(EFI_80211_GET_NETWORKS_TOKEN));
  ZeroMem(&GetData, sizeof(EFI_80211_GET_NETWORKS_DATA));

  //
  // Get hold of Wifi Connection 2 protocol
  //

  Status = gBS->LocateProtocol (&gEfiWiFi2ProtocolGuid,
                                NULL,
                                (VOID**)&ConMgr2Protocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "LocateProtocol() failed : (%r)\n", Status));
    goto Exit;
  }

  Status = gBS->CreateEvent (EVT_NOTIFY_WAIT,
                             TPL_CALLBACK,
                             WifiCmNetworkOperationWaitCallback,
                             NULL,
                             &WaitForNetworkOperation);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "CreateEvent() failed : (%r)\n", Status));
    goto Exit;
  }

  Status = gBS->CreateEvent (EVT_NOTIFY_SIGNAL,
                             TPL_CALLBACK,
                             WifiCmGetNetworksCallback,
                             &GetNetworksToken,
                             &GetNetworksToken.Event);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to create get network token's event. CreateEvent() failed : (%r)\n", Status));
    goto Exit;
  }

  GetNetworksToken.Data = &GetData;

  Status = ConMgr2Protocol->GetNetworks (ConMgr2Protocol, &GetNetworksToken);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to get network list. GetNetworks() failed : (%r)\n", Status));
    goto Exit;
  }

  //
  // Wait until get networks operations are done
  //

  Status = gBS->WaitForEvent (1, &WaitForNetworkOperation, &Index);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "WaitForEvent() failed : (%r)\n", Status));
    goto Exit;
  }

  NetworkList = GetNetworksToken.Result;
  if (NetworkList->NumOfNetworkDesc == 0) {
    DEBUG ((DEBUG_INFO, "No wireless networks found!\n"));
    goto Exit;
  }

  PerformQuickSort (NetworkList->NetworkDesc,
                    NetworkList->NumOfNetworkDesc,
                    sizeof (EFI_80211_NETWORK_DESCRIPTION),
                    WifiCmNetworkDescriptionCompareFunc);

  for (UINTN i = 0; i < NetworkList->NumOfNetworkDesc; i++) {
    EFI_80211_NETWORK_DESCRIPTION* NetworkDesc = &NetworkList->NetworkDesc[i];
    EFI_80211_NETWORK* Network = &NetworkDesc->Network;

    if (Network->SSId.SSIdLen == 0)
      continue;

    RetSsidListLength++;
  }

  //
  // Only pick top 10 networks(already sorted by signal strength above). As
  // the connection manager UX do not have scrolling viewport implemented yet
  //

  RetSsidListLength = MIN (RetSsidListLength, 10);

  RetSsidList = AllocateZeroPool (RetSsidListLength * sizeof (CHAR8*));
  if (RetSsidList == NULL) {
    DEBUG ((DEBUG_ERROR, "Unable to allocate memory for SsidList\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  for (UINTN i = 0, j = 0; i < NetworkList->NumOfNetworkDesc && j < RetSsidListLength; i++) {
    EFI_80211_NETWORK_DESCRIPTION* NetworkDesc = &NetworkList->NetworkDesc[i];
    EFI_80211_NETWORK* Network = &NetworkDesc->Network;

    if (Network->SSId.SSIdLen == 0) {
      continue;
    }

    // DEBUG ((DEBUG_INFO, "SSID: %-30s Quality:%3d BSS: %d\n",
    //         Network->SSId.SSId,
    //         NetworkDesc->NetworkQuality,
    //         Network->BSSType)); // Basic service sets

    SsidLength = AsciiStrnLenS ((CONST CHAR8*)Network->SSId.SSId, EFI_MAX_SSID_LEN + 1);
    if (SsidLength > EFI_MAX_SSID_LEN) {
      Status = EFI_INVALID_PARAMETER;
      DEBUG ((DEBUG_ERROR, "Invalid SSId length\n"));
      goto Exit;
    }

    RetSsidList[j] = AllocateZeroPool (SsidLength + 1);
    AsciiStrCpyS (RetSsidList[j], SsidLength + 1, (CONST CHAR8*)Network->SSId.SSId);
    j++;

    // // Dump Authentication and Key Management(AKM) suites
    // for (UINTN j = 0; j < Network->AKMSuite->AKMSuiteCount; j++) {
    //     EFI_80211_SUITE_SELECTOR* Selector = &Network->AKMSuite->AKMSuiteList[j];
    //     DEBUG ((DEBUG_INFO, "    [AKM] OUI: %02X-%02X-%02X Subtype: %02X\n",
    //             Selector->Oui[0],
    //             Selector->Oui[1],
    //             Selector->Oui[2],
    //             Selector->SuiteType));
    // }
    // // Dump Cipher suites
    // for (UINTN j = 0; j < Network->CipherSuite->CipherSuiteCount; j++) {
    //     EFI_80211_SUITE_SELECTOR* Selector = &Network->CipherSuite->CipherSuiteList[j];
    //     DEBUG ((DEBUG_INFO, "    [Cipher] OUI: %02X-%02X-%02X Subtype: %02X\n",
    //             Selector->Oui[0],
    //             Selector->Oui[1],
    //             Selector->Oui[2],
    //             Selector->SuiteType);
    // }
  }

  *SsidList = RetSsidList;
  *SsidListLength = RetSsidListLength;
  RetSsidList = NULL;

Exit:
  if (GetNetworksToken.Event != NULL) {
    gBS->CloseEvent (GetNetworksToken.Event);
  }

  if (WaitForNetworkOperation != NULL) {
    gBS->CloseEvent (WaitForNetworkOperation);
  }

  if (NetworkList != NULL) {
    FreePool (NetworkList);
  }

  if (RetSsidList != NULL) {
    for (UINTN i = 0; i < RetSsidListLength; i++) {
      FreePool (RetSsidList[i]);
    }

    FreePool (RetSsidList);
  }

  return Status;
}

EFI_STATUS
EFIAPI
WifiCmConnect (
  IN CHAR8* SsidName,
  IN CHAR8* Password)
{
  EFI_STATUS Status = EFI_SUCCESS;
  EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL* ConMgr2Protocol = NULL;
  // WSSI
  //EFI_80211_GET_NETWORKS_TOKEN GetNetworksToken = {0};
  //EFI_80211_CONNECT_NETWORK_TOKEN NetworkConnectToken = {0};
  //EFI_80211_GET_NETWORKS_DATA GetData = {0};
  //EFI_80211_CONNECT_NETWORK_DATA ConnectData = {0};
  //EFI_80211_NETWORK Network = {0};
  EFI_80211_GET_NETWORKS_TOKEN GetNetworksToken;
  EFI_80211_CONNECT_NETWORK_TOKEN NetworkConnectToken;
  EFI_80211_GET_NETWORKS_DATA GetData;
  EFI_80211_CONNECT_NETWORK_DATA ConnectData;
  EFI_80211_NETWORK Network;

  EFI_80211_GET_NETWORKS_RESULT* NetworkList = NULL;
  EFI_SUPPLICANT_PROTOCOL* Supplicant = NULL;
  // CHAR8 Ssid[EFI_MAX_SSID_LEN] = "<snipped>";
  // CHAR8 Password[] = "<snipped>";
  // WSSI
  //EFI_80211_SSID Ssid = {0};
  EFI_80211_SSID Ssid;

  BOOLEAN Found = FALSE;
  UINTN Index = 0;
  UINTN PasswordLength = 0;

  // WSSI
  ZeroMem(&GetNetworksToken, sizeof(EFI_80211_GET_NETWORKS_TOKEN));
  ZeroMem(&NetworkConnectToken, sizeof(EFI_80211_CONNECT_NETWORK_TOKEN));
  ZeroMem(&GetData, sizeof(EFI_80211_GET_NETWORKS_DATA));
  ZeroMem(&ConnectData, sizeof(EFI_80211_CONNECT_NETWORK_DATA));
  ZeroMem(&Network, sizeof(EFI_80211_NETWORK));
  ZeroMem(&Ssid, sizeof(EFI_80211_SSID));

  if (SsidName == NULL || Password == NULL) {
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  //
  // Get hold of Wifi Connection 2 protocol
  //

  Status = gBS->LocateProtocol (&gEfiWiFi2ProtocolGuid,
                                NULL,
                                (VOID**)&ConMgr2Protocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "LocateProtocol() failed : (%r)\n", Status));
    goto Exit;
  }

  //
  // Get hold of Wifi Supplicant protocol
  //

  Status = gBS->LocateProtocol (&gEfiSupplicantProtocolGuid, NULL, (VOID**)&Supplicant);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "LocateProtocol() failed : (%r)\n", Status));
    goto Exit;
  }

  Status = gBS->CreateEvent (EVT_NOTIFY_WAIT,
                             TPL_CALLBACK,
                             WifiCmNetworkOperationWaitCallback,
                             NULL,
                             &WaitForNetworkOperation);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "CreateEvent() failed : (%r)\n", Status));
    goto Exit;
  }

  Status = gBS->CreateEvent (EVT_NOTIFY_SIGNAL,
                             TPL_CALLBACK,
                             WifiCmGetNetworksCallback,
                             &GetNetworksToken,
                             &GetNetworksToken.Event);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to create get network token's event. CreateEvent() failed (%r)\n", Status));
    goto Exit;
  }

  GetNetworksToken.Data = &GetData;

  Status = ConMgr2Protocol->GetNetworks (ConMgr2Protocol, &GetNetworksToken);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to get network list. GetNetworks() failed : (%r)\n", Status));
    goto Exit;
  }

  //
  // Wait until get networks operations are done
  //

  Status = gBS->WaitForEvent (1, &WaitForNetworkOperation, &Index);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "WaitForEvent() failed : (%r)\n", Status));
    goto Exit;
  }

  NetworkList = GetNetworksToken.Result;
  if (NetworkList->NumOfNetworkDesc == 0) {
    Status = EFI_NOT_FOUND;
    DEBUG ((DEBUG_ERROR, "No wireless networks found!\n"));
    goto Exit;
  }

  PerformQuickSort (NetworkList->NetworkDesc,
                    NetworkList->NumOfNetworkDesc,
                    sizeof (EFI_80211_NETWORK_DESCRIPTION),
                    WifiCmNetworkDescriptionCompareFunc);

  //
  // Find the network with the SSidName
  //

  UINTN SsidNameLength = AsciiStrnLenS (SsidName, EFI_MAX_SSID_LEN + 1);
  if (SsidNameLength == EFI_MAX_SSID_LEN + 1) {
    DEBUG ((DEBUG_ERROR, "Invalid SSidName Length\n"));
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  for (UINTN i = 0; i < NetworkList->NumOfNetworkDesc; i++) {
    EFI_80211_NETWORK_DESCRIPTION* NetworkDesc = &NetworkList->NetworkDesc[i];

    if (NetworkDesc->Network.SSId.SSIdLen == 0)
      continue;

    if (NetworkDesc->Network.SSId.SSIdLen != SsidNameLength)
      continue;

    if (CompareMem (NetworkDesc->Network.SSId.SSId,
                    SsidName,
                    NetworkDesc->Network.SSId.SSIdLen) == 0) {
      CopyMem (&Network, &NetworkDesc->Network, sizeof (EFI_80211_NETWORK));
      Found = TRUE;
      break;
    }
  }

  //
  // Bailout if we could not find the network object with SSidName
  //
  if (Found == FALSE) {
    Status = EFI_NOT_FOUND;
    DEBUG ((DEBUG_ERROR, "Wireless network with SSID '%s' not found\n", SsidName));
    goto Exit;
  }

  //
  // Prepare the Supplicant with SSid and Password
  //

  Ssid.SSIdLen = (UINT8)AsciiStrnLenS (SsidName, EFI_MAX_SSID_LEN + 1);
  if (Ssid.SSIdLen > EFI_MAX_SSID_LEN) {
    DEBUG ((DEBUG_ERROR, "Invalid SsId length\n"));
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }
  CopyMem (Ssid.SSId, SsidName, Ssid.SSIdLen);
  Status = Supplicant->SetData (Supplicant,
                                EfiSupplicant80211TargetSSIDName,
                                &Ssid,
                                sizeof (EFI_80211_SSID));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Supplicant SetData for Ssid failed : (%r)\n", Status));
    goto Exit;
  }

  PasswordLength = AsciiStrnLenS (Password, MAX_80211_PWD_LEN + 1);
  if (PasswordLength == MAX_80211_PWD_LEN + 1) {
    DEBUG ((DEBUG_ERROR, "Invalid PasswordLength\n"));
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  Status = Supplicant->SetData (Supplicant,
                                EfiSupplicant80211PskPassword,
                                Password,
                                PasswordLength + 1);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Supplicant SetData for Password failed : (%r)\n", Status));
    goto Exit;
  }

  Status = gBS->CreateEvent (EVT_NOTIFY_SIGNAL,
                             TPL_CALLBACK,
                             WifiCmNetworkConnectCallback,
                             &NetworkConnectToken,
                             &NetworkConnectToken.Event);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Unable to create network connect token's event. CreateEvent() failed : (%r)\n", Status));
    goto Exit;
  }

  //
  // Dump Network object fields
  //

  DEBUG ((DEBUG_INFO, "SSID: %s BSS: %d\n", Network.SSId.SSId, Network.BSSType)); // Basic service sets

  // Dump Authentication and Key Management (AKM) suites
  for (UINTN j = 0; j < Network.AKMSuite->AKMSuiteCount; j++) {
    EFI_80211_SUITE_SELECTOR* Selector = &Network.AKMSuite->AKMSuiteList[j];
    DEBUG ((DEBUG_INFO,
            "    [AKM] OUI: %02X-%02X-%02X Subtype: %02X\n",
            Selector->Oui[0],
            Selector->Oui[1],
            Selector->Oui[2],
            Selector->SuiteType));
    if (((*(UINT32*)Selector->Oui) | Selector->SuiteType << 24) == IEEE_80211_AKM_SUITE_PSK) {
      DEBUG ((DEBUG_INFO, "        [AKM] IEEE_80211_AKM_SUITE_PSK\n"));
    }
  }

  // Dump Cipher suites
  for (UINTN j = 0; j < Network.CipherSuite->CipherSuiteCount; j++) {
    EFI_80211_SUITE_SELECTOR* Selector = &Network.CipherSuite->CipherSuiteList[j];
    DEBUG ((DEBUG_INFO,
            "    [Cipher] OUI: %02X-%02X-%02X Subtype: %02X\n",
            Selector->Oui[0],
            Selector->Oui[1],
            Selector->Oui[2],
            Selector->SuiteType));
    if (((*(UINT32*)Selector->Oui) | Selector->SuiteType << 24) ==
        IEEE_80211_PAIRWISE_CIPHER_SUITE_CCMP) {
      DEBUG ((DEBUG_INFO, "        [Cipher] IEEE_80211_PAIRWISE_CIPHER_SUITE_CCMP\n"));
    }
  }

  //
  // Assign Network object to Connect Token
  //
  ConnectData.Network = &Network;
  ConnectData.FailureTimeout = 20; // 20 Sec
  NetworkConnectToken.Data = &ConnectData;
  DEBUG ((DEBUG_INFO, "Network name: %s\n", Network.SSId.SSId));

  //
  // Connect to the Network
  //
  Status = ConMgr2Protocol->ConnectNetwork (ConMgr2Protocol, &NetworkConnectToken);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ConnectNetwork() failed : (%r)\n", Status));
    goto Exit;
  }

  //
  // Wait until ConnectNetwork operations are done
  //
  Status = gBS->WaitForEvent (1, &WaitForNetworkOperation, &Index);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "WaitForEvent() failed : (%r)\n", Status));
    goto Exit;
  }

  DEBUG ((DEBUG_INFO, "NetworkConnectToken.Status = 0x%08X\n", NetworkConnectToken.Status));
  DEBUG ((DEBUG_INFO, "NetworkConnectToken.ResultCode = 0x%02X", NetworkConnectToken.ResultCode));
  switch (NetworkConnectToken.ResultCode) {
    case ConnectSuccess:                  DEBUG ((DEBUG_INFO, " (ConnectSuccess)\n"));                  break;
    case ConnectRefused:                  DEBUG ((DEBUG_INFO, " (ConnectRefused)\n"));                  break;
    case ConnectFailed:                   DEBUG ((DEBUG_INFO, " (ConnectFailed)\n"));                   break;
    case ConnectFailureTimeout:           DEBUG ((DEBUG_INFO, " (ConnectFailureTimeout)\n"));           break;
    case ConnectFailedReasonUnspecified:  DEBUG ((DEBUG_INFO, " (ConnectFailedReasonUnspecified)\n"));  break;
    default:                              DEBUG ((DEBUG_INFO, " ( undefined )\n"));                     break;
  }

Exit:
  if (GetNetworksToken.Event != NULL) {
    gBS->CloseEvent (GetNetworksToken.Event);
  }

  if (NetworkConnectToken.Event != NULL) {
    gBS->CloseEvent (NetworkConnectToken.Event);
  }

  if (WaitForNetworkOperation != NULL) {
    gBS->CloseEvent (WaitForNetworkOperation);
  }

  if (NetworkList != NULL) {
    FreePool (NetworkList);
  }

  return Status;
}

EFI_STATUS
EFIAPI
WifiCmDrawNetworkListUIBorder (
  IN PGFX_FRAMEBUFFER FrameBuffer,
  IN PGFX_FONT_INFO FontInfo,
  IN PGFX_RECT Rect)
{
  EFI_STATUS Status = EFI_SUCCESS;
  GFX_RECT Destination = {0, 0, FrameBuffer->Width, FrameBuffer->Height};
  GFX_RECT ClipRect = GfxGetClipRectangle (Rect, &Destination);

  //
  // Draw top left corner
  //

  Status = GfxRasterCharacter (FrameBuffer,
                               FontInfo,
                               BOXDRAW_DOWN_RIGHT,
                               ClipRect.X,
                               ClipRect.Y,
                               RASTER_ATTRIBUTE_BG_BLUE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
    goto Exit;
  }

  //
  // Draw top border
  //

  for (UINTN j = ClipRect.X + GLYPH_WIDTH; j < ClipRect.X + ClipRect.Width; j += GLYPH_WIDTH) {
    Status = GfxRasterCharacter (FrameBuffer,
                                 FontInfo,
                                 BOXDRAW_HORIZONTAL,
                                 j,
                                 ClipRect.Y,
                                 RASTER_ATTRIBUTE_BG_BLUE);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
      goto Exit;
    }
  }

  //
  // Draw top right corner
  //

  Status = GfxRasterCharacter (FrameBuffer,
                               FontInfo,
                               BOXDRAW_DOWN_LEFT,
                               ClipRect.X + ClipRect.Width - GLYPH_WIDTH,
                               ClipRect.Y,
                               RASTER_ATTRIBUTE_BG_BLUE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
    goto Exit;
  }

  //
  // Draw left border
  //

  for (UINTN i = ClipRect.Y + GLYPH_HEIGHT; i < ClipRect.Y + ClipRect.Height; i += GLYPH_HEIGHT) {
    Status = GfxRasterCharacter (FrameBuffer,
                                 FontInfo,
                                 BOXDRAW_VERTICAL,
                                 ClipRect.X,
                                 i,
                                 RASTER_ATTRIBUTE_BG_BLUE);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
      goto Exit;
    }
  }

  //
  // Draw right border
  //

  for (UINTN i = ClipRect.Y + GLYPH_HEIGHT; i < ClipRect.Y + ClipRect.Height; i += GLYPH_HEIGHT) {
    Status = GfxRasterCharacter (FrameBuffer,
                                 FontInfo,
                                 BOXDRAW_VERTICAL,
                                 ClipRect.X + ClipRect.Width - GLYPH_WIDTH,
                                 i,
                                 RASTER_ATTRIBUTE_BG_BLUE);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
      goto Exit;
    }
  }

#if 0
  //
  // Draw bottom left corner
  //

  Status = GfxRasterCharacter (FrameBuffer,
                               FontInfo,
                               BOXDRAW_UP_RIGHT,
                               ClipRect.X,
                               ClipRect.Y + ClipRect.Height - GLYPH_HEIGHT,
                               RASTER_ATTRIBUTE_BG_BLUE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
    goto Exit;
  }

  //
  // Draw bottom border
  //

  for (UINTN j = ClipRect.X + GLYPH_WIDTH;
      j < ClipRect.X + ClipRect.Width;
      j += GLYPH_WIDTH) {

    Status = GfxRasterCharacter (FrameBuffer,
                                 FontInfo,
                                 BOXDRAW_HORIZONTAL,
                                 j,
                                 ClipRect.Y + ClipRect.Height - GLYPH_HEIGHT,
                                 RASTER_ATTRIBUTE_BG_BLUE);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
      goto Exit;
    }
  }

  //
  // Draw bottom right corner
  //

  Status = GfxRasterCharacter (FrameBuffer,
                               FontInfo,
                               BOXDRAW_UP_LEFT,
                               ClipRect.X + ClipRect.Width - GLYPH_WIDTH,
                               ClipRect.Y + ClipRect.Height - GLYPH_HEIGHT,
                               RASTER_ATTRIBUTE_BG_BLUE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
    goto Exit;
  }
#endif

  //
  // Draw 'Wi-Fi Networks' string on the top border
  //

  CHAR16 HeaderTitle[] = L"Wi-Fi Networks";
  UINTN HeaderLength = (ARRAY_SIZE (HeaderTitle)) - 1;
  UINTN StartOffset = (ClipRect.Width / 2) - (HeaderLength / 2) * GLYPH_WIDTH;
  for (UINTN j = ClipRect.X + StartOffset, i = 0; HeaderTitle[i]; i++, j += GLYPH_WIDTH) {
    Status = GfxRasterCharacter (FrameBuffer,
                                 FontInfo,
                                 HeaderTitle[i],
                                 j,
                                 ClipRect.Y,
                                 RASTER_ATTRIBUTE_BG_BLUE);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
      goto Exit;
    }
  }

Exit:
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
WifiCmDrawNetworkListUIItems (
  IN PGFX_FRAMEBUFFER FrameBuffer,
  IN PGFX_FONT_INFO FontInfo,
  IN PGFX_RECT Rect,
  IN PWIFI_CM_UI_STATE ConnMgrUI)
{
  EFI_STATUS Status = EFI_SUCCESS;
  GFX_RECT Destination = {0, 0, FrameBuffer->Width, FrameBuffer->Height};
  GFX_RECT ClipRect = GfxGetClipRectangle (Rect, &Destination);

  for (UINTN j = 0, dy = GLYPH_HEIGHT; j < ConnMgrUI->SsidListLength; j++, dy += GLYPH_HEIGHT) {
    CHAR8* String = ConnMgrUI->SsidList[j];
    if (j == ConnMgrUI->SelectedIndex) { // Draw selected entry with inverted colors
      UINTN i = 0, dx = 0;
      // Draw selected string in inverted colors
      for (i = 0, dx = GLYPH_WIDTH; String[i]; i++, dx += GLYPH_WIDTH) {
        CHAR16 Char = (CHAR16)String[i];
        Status = GfxRasterCharacter (FrameBuffer,
                                     FontInfo,
                                     Char,
                                     ClipRect.X + dx,
                                     ClipRect.Y + dy,
                                     RASTER_ATTRIBUTE_INVERT);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
          goto Exit;
        }
      }
      // Draw remaining part of the selected entry also in inverted colors
      for (; dx < ClipRect.Width - GLYPH_WIDTH; dx += GLYPH_WIDTH) {
        Status = GfxRasterCharacter (FrameBuffer,
                                     FontInfo,
                                     L' ',
                                     ClipRect.X + dx,
                                     ClipRect.Y + dy,
                                     RASTER_ATTRIBUTE_INVERT);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
          goto Exit;
        }
      }
    }
    else {
      for (UINTN i = 0, dx = GLYPH_WIDTH; String[i]; i++, dx += GLYPH_WIDTH) {
        Status = GfxRasterCharacter (FrameBuffer,
                                     FontInfo,
                                     String[i],
                                     ClipRect.X + dx,
                                     ClipRect.Y + dy,
                                     RASTER_ATTRIBUTE_BG_BLUE);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
          goto Exit;
        }
      }
    }
  }

Exit:
  return Status;
}

EFI_STATUS
EFIAPI
WifiCmDrawPasswordBoxUIBorder (
  IN PGFX_FRAMEBUFFER FrameBuffer,
  IN PGFX_FONT_INFO FontInfo,
  IN PGFX_RECT Rect)
{
  EFI_STATUS Status = EFI_SUCCESS;
  GFX_RECT Destination = {0, 0, FrameBuffer->Width, FrameBuffer->Height};
  GFX_RECT ClipRect = GfxGetClipRectangle (Rect, &Destination);

  //
  // Draw top left corner
  //

  Status = GfxRasterCharacter (FrameBuffer,
                               FontInfo,
                               BOXDRAW_VERTICAL_RIGHT,
                               ClipRect.X,
                               ClipRect.Y,
                               RASTER_ATTRIBUTE_BG_BLUE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
    goto Exit;
  }

  //
  // Draw top border
  //

  for (UINTN j = ClipRect.X + GLYPH_WIDTH; j < ClipRect.X + ClipRect.Width; j += GLYPH_WIDTH) {
    Status = GfxRasterCharacter (FrameBuffer,
                                 FontInfo,
                                 BOXDRAW_HORIZONTAL,
                                 j,
                                 ClipRect.Y,
                                 RASTER_ATTRIBUTE_BG_BLUE);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
      goto Exit;
    }
  }

  //
  // Draw top right corner
  //

  Status = GfxRasterCharacter (FrameBuffer,
                               FontInfo,
                               BOXDRAW_VERTICAL_LEFT,
                               ClipRect.X + ClipRect.Width - GLYPH_WIDTH,
                               ClipRect.Y,
                               RASTER_ATTRIBUTE_BG_BLUE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
    goto Exit;
  }

  //
  // Draw left border
  //

  for (UINTN i = ClipRect.Y + GLYPH_HEIGHT; i < ClipRect.Y + ClipRect.Height - GLYPH_HEIGHT;
      i += GLYPH_HEIGHT) {
    Status = GfxRasterCharacter (FrameBuffer,
                                 FontInfo,
                                 BOXDRAW_VERTICAL,
                                 ClipRect.X,
                                 i,
                                 RASTER_ATTRIBUTE_BG_BLUE);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
      goto Exit;
    }
  }

  //
  // Draw right border
  //

  for (UINTN i = ClipRect.Y + GLYPH_HEIGHT; i < ClipRect.Y + ClipRect.Height - GLYPH_HEIGHT;
      i += GLYPH_HEIGHT) {
    Status = GfxRasterCharacter (FrameBuffer,
                                 FontInfo,
                                 BOXDRAW_VERTICAL,
                                 ClipRect.X + ClipRect.Width - GLYPH_WIDTH,
                                 i,
                                 RASTER_ATTRIBUTE_BG_BLUE);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
      goto Exit;
    }
  }

  //
  // Draw bottom left corner
  //

  Status = GfxRasterCharacter (FrameBuffer,
                               FontInfo,
                               BOXDRAW_UP_RIGHT,
                               ClipRect.X,
                               ClipRect.Y + ClipRect.Height - GLYPH_HEIGHT,
                               RASTER_ATTRIBUTE_BG_BLUE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
    goto Exit;
  }

  //
  // Draw bottom border
  //

  for (UINTN j = ClipRect.X + GLYPH_WIDTH; j < ClipRect.X + ClipRect.Width; j += GLYPH_WIDTH) {
    Status = GfxRasterCharacter (FrameBuffer,
                                 FontInfo,
                                 BOXDRAW_HORIZONTAL,
                                 j,
                                 ClipRect.Y + ClipRect.Height - GLYPH_HEIGHT,
                                 RASTER_ATTRIBUTE_BG_BLUE);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
      goto Exit;
    }
  }

  //
  // Draw bottom right corner
  //

  Status = GfxRasterCharacter (FrameBuffer,
                               FontInfo,
                               BOXDRAW_UP_LEFT,
                               ClipRect.X + ClipRect.Width - GLYPH_WIDTH,
                               ClipRect.Y + ClipRect.Height - GLYPH_HEIGHT,
                               RASTER_ATTRIBUTE_BG_BLUE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
    goto Exit;
  }

  //
  // Draw 'Enter Password' string on the top border
  //

  CHAR16 HeaderTitle[] = L"Enter password";
  UINTN HeaderLength = (ARRAY_SIZE(HeaderTitle)) - 1;
  UINTN StartOffset = (ClipRect.Width / 2) - (HeaderLength / 2) * GLYPH_WIDTH;
  for (UINTN j = ClipRect.X + StartOffset, i = 0; HeaderTitle[i]; i++, j += GLYPH_WIDTH) {
    Status = GfxRasterCharacter (FrameBuffer,
                                 FontInfo,
                                 HeaderTitle[i],
                                 j,
                                 ClipRect.Y,
                                 RASTER_ATTRIBUTE_BG_BLUE);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
      goto Exit;
    }
  }

Exit:
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
WifiCmDrawPasswordBox (
  IN PGFX_FRAMEBUFFER FrameBuffer,
  IN PGFX_FONT_INFO FontInfo,
  IN PGFX_RECT Rect,
  IN PWIFI_CM_UI_STATE ConnMgrUI)
{
  EFI_STATUS Status = EFI_SUCCESS;
  GFX_RECT Destination = {0, 0, FrameBuffer->Width, FrameBuffer->Height};
  GFX_RECT ClipRect = GfxGetClipRectangle (Rect, &Destination);

  // // Draw  in inverted colors
  // for (UINTN dx = GLYPH_WIDTH; dx < ClipRect.Width - GLYPH_WIDTH; dx += GLYPH_WIDTH) {
  //     Status = GfxRasterCharacter (FrameBuffer,
  //                                 FontInfo,
  //                                 L' ',
  //                                 ClipRect.X + dx,
  //                                 ClipRect.Y + GLYPH_HEIGHT,
  //                                 RASTER_ATTRIBUTE_BG_WHITE);
  //     if (EFI_ERROR (Status)) {
  //         DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
  //         goto Exit;
  //     }
  // }

  UINTN i = 0, dx = 0;
  // Draw Password as asterisks string in inverted colors
  for (i = 0, dx = GLYPH_WIDTH; i < ConnMgrUI->PasswordLength; i++, dx += GLYPH_WIDTH) {
    Status = GfxRasterCharacter (FrameBuffer,
                                 FontInfo,
                                 L'*',
                                 ClipRect.X + dx,
                                 ClipRect.Y + GLYPH_HEIGHT,
                                 RASTER_ATTRIBUTE_INVERT);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
      goto Exit;
    }
  }

  // Draw remaining part of the password entry also in inverted colors
  for (; dx < ClipRect.Width - GLYPH_WIDTH; dx += GLYPH_WIDTH) {
    Status = GfxRasterCharacter (FrameBuffer,
                                 FontInfo,
                                 L' ',
                                 ClipRect.X + dx,
                                 ClipRect.Y + GLYPH_HEIGHT,
                                 RASTER_ATTRIBUTE_INVERT);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
      goto Exit;
    }
  }

Exit:
  return Status;
}

EFI_STATUS
EFIAPI
WifiCmDrawMainUI (
  IN PGFX_FRAMEBUFFER FrameBuffer,
  IN PGFX_FONT_INFO FontInfo,
  IN PWIFI_CM_UI_STATE ConnMgrUI)
{
  EFI_STATUS Status = EFI_SUCCESS;
  GFX_RECT NetworkListRect = 
  {
    FrameBuffer->Width / 2 - 200,
    FrameBuffer->Height / 2 - 150,
    400,
    300
  };
  GFX_RECT PasswordRect =
  {
    NetworkListRect.X,
    NetworkListRect.Y + NetworkListRect.Height,
    400,
    GLYPH_HEIGHT * 3
  };
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL BlueBackground =
  {
    0xFF,
    0,
    0,
    0
  };

  //
  // Fill background
  //

  GfxFillColor (FrameBuffer, &NetworkListRect, BlueBackground);

  //
  // Draw network list dialog box border
  //

  Status = WifiCmDrawNetworkListUIBorder (FrameBuffer, FontInfo, &NetworkListRect);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "WifiCmDrawNetworkListUIBorder() failed: (%r)\n", Status));
    goto Exit;
  }

  //
  // Draw network list items
  //

  Status = WifiCmDrawNetworkListUIItems (FrameBuffer, FontInfo, &NetworkListRect, ConnMgrUI);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "WifiCmDrawNetworkListUIItems() failed: (%r)\n", Status));
    goto Exit;
  }

  //
  // Fill background
  //

  GfxFillColor (FrameBuffer, &PasswordRect, BlueBackground);

  //
  // Draw password dialog box border
  //

  Status = WifiCmDrawPasswordBoxUIBorder (FrameBuffer, FontInfo, &PasswordRect);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "WifiCmDrawPasswordBoxUIBorder() failed: (%r)\n", Status));
    goto Exit;
  }

  //
  // Draw password field
  //

  Status = WifiCmDrawPasswordBox (FrameBuffer, FontInfo, &PasswordRect, ConnMgrUI);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "WifiCmDrawPasswordBox() failed: (%r)\n", Status));
    goto Exit;
  }

  //
  // Blt the framebuffer to screen
  //

  FrameBuffer->GraphicsProtocol->Blt (FrameBuffer->GraphicsProtocol,
                                      (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)FrameBuffer->Bitmap,
                                      EfiBltBufferToVideo,
                                      0,
                                      0,
                                      0,
                                      0,
                                      FrameBuffer->Width,
                                      FrameBuffer->Height,
                                      0);

Exit:
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
WifiCmHandleInput (
  IN PWIFI_CM_UI_STATE ConnMgrUI,
  IN EFI_INPUT_KEY Key)
{
  EFI_STATUS Status = EFI_SUCCESS;

  // DEBUG ((DEBUG_INFO, "ScanCode:%d UnicodeChar: %d\n", Key.ScanCode, Key.UnicodeChar));

  //
  // Handle network list specific keys (aka Up and Down keys)
  //

  switch (Key.ScanCode) {

    case SCAN_UP: // Wrap around backward
      ConnMgrUI->SelectedIndex = (ConnMgrUI->SelectedIndex - 1 + ConnMgrUI->SsidListLength) %
                                 ConnMgrUI->SsidListLength;
      goto Exit;

    case SCAN_DOWN: // Wrap around forward
      ConnMgrUI->SelectedIndex = (ConnMgrUI->SelectedIndex + 1) % ConnMgrUI->SsidListLength;
      goto Exit;

    case SCAN_RIGHT:
    case SCAN_LEFT:
    case SCAN_HOME:
    case SCAN_END:
      goto Exit;
  }

  //
  // Handle password box specific keys (aka character, ESC, Backspace keys)
  //

  switch (Key.UnicodeChar) {

    case CHAR_BACKSPACE:
      if (ConnMgrUI->PasswordLength > 0) {
        ConnMgrUI->Password[ConnMgrUI->PasswordLength - 1] = 0;
        ConnMgrUI->PasswordLength--;
      }
      goto Exit;

    case 27:        // ESC
    case CHAR_NULL: // Not sure why Esc is being triggered as NULL
      Status = EFI_ABORTED;
      goto Exit;

    case CHAR_CARRIAGE_RETURN: { // TODO: handle enter
        UINTN SelectedIndex = ConnMgrUI->SelectedIndex;
        CHAR8* Ssid = ConnMgrUI->SsidList[SelectedIndex];
        CHAR8* Password = ConnMgrUI->Password;
        Status = WifiCmConnect (Ssid, Password);
        if (!EFI_ERROR (Status)) {
          // break on the wifi connection is succeeded.
          // TODO: create new ufp error code
          Status = EFI_ABORTED;
        }
        goto Exit;
      }

    default:
      if (ConnMgrUI->PasswordLength < ARRAY_SIZE (ConnMgrUI->Password) - 1) {
        ConnMgrUI->Password[ConnMgrUI->PasswordLength] = (CHAR8)Key.UnicodeChar;
        ConnMgrUI->PasswordLength++;
      }
      goto Exit;
  }

Exit:
  return Status;
}

EFI_STATUS
EFIAPI
WifiCmRestoreFrameBuffer (
  IN PGFX_FRAMEBUFFER FrameBuffer)
{
  EFI_STATUS Status = EFI_SUCCESS;

  //
  // Restore original screen contents from back buffer
  //

  Status = FrameBuffer->GraphicsProtocol->Blt (FrameBuffer->GraphicsProtocol,
                                               (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)
                                               FrameBuffer->BackBuffer,
                                               EfiBltBufferToVideo,
                                               0,
                                               0,
                                               0,
                                               0,
                                               FrameBuffer->Width,
                                               FrameBuffer->Height,
                                               0);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Blt() failed : (%r)\n", Status));
    goto Exit;
  }

Exit:
  return Status;
}

EFI_STATUS
EFIAPI
WifiCmRestoreGraphicsResolution (
  IN PGFX_FRAMEBUFFER FrameBuffer,
  IN UINT32 Mode)
{
  EFI_STATUS Status = EFI_SUCCESS;

  Status = FrameBuffer->GraphicsProtocol->SetMode (FrameBuffer->GraphicsProtocol, Mode);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SetMode() failed : (%r)\n", Status));
    goto Exit;
  }

Exit:
  return Status;
}

EFI_STATUS
EFIAPI
WifiCmUIMain (
  IN OUT PEFI_MS_CBMR_WIFI_NETWORK_PROFILE Profile)
{
  EFI_STATUS Status = EFI_SUCCESS;
  // WSSI
  //GFX_FONT_INFO FontInfo = {0};
  //GFX_FRAMEBUFFER FrameBuffer = {0};
  GFX_FONT_INFO FontInfo;
  GFX_FRAMEBUFFER FrameBuffer;

  // WSSI
  ZeroMem(&FontInfo, sizeof(GFX_FONT_INFO));
  ZeroMem(&FrameBuffer, sizeof(GFX_FRAMEBUFFER));

  //WIFI_CM_UI_STATE ConnMgrUI = {NULL, 4, 1};
  WIFI_CM_UI_STATE ConnMgrUI;
  ConnMgrUI.SsidList = NULL;
  ConnMgrUI.SsidListLength = 4;
  ConnMgrUI.SelectedIndex = 1;

  CHAR8** SsidList = NULL;
  UINTN SsidListLength = 0;

  DEBUG ((DEBUG_INFO, "Starting Wi-Fi connection manager UI\n"));

  Status = GfxGetSystemFont (&FontInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GfxGetSystemFont() failed: (%r)\n", Status));
    goto Exit;
  }

  Status = GfxAllocateFrameBuffer (&FrameBuffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GfxAllocateFrameBuffer() failed: (%r)\n", Status));
    goto Exit;
  }

  //
  // Refresh the Wi-Fi network list
  //

  DEBUG ((DEBUG_INFO, "Getting Wi-Fi network list\n"));
  Status = WifiCmGetNetworkList (&SsidList, &SsidListLength);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "WifiCmGetNetworkList() failed: (%r)\n", Status));
    goto Exit;
  }
  DEBUG ((DEBUG_INFO, "Getting Wi-Fi network list done\n"));

  ConnMgrUI.SsidList = SsidList;
  ConnMgrUI.SsidListLength = SsidListLength;

  while (TRUE) {
    //
    // Draw the UI
    //

    Status = WifiCmDrawMainUI (&FrameBuffer, &FontInfo, &ConnMgrUI);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "WifiCmDrawMainUI() failed: (%r)\n", Status));
      goto Exit;
    }

    //
    // Read for keyboard input
    //

    EFI_INPUT_KEY Key = WifiCmGetCharNoEcho ();
    Status = WifiCmHandleInput (&ConnMgrUI, Key);
    if (Status == EFI_ABORTED) {
      Status = EFI_SUCCESS;
      break;
    }
  }

  //
  // Copy the Wi-Fi profile information from CM UX
  //

  DEBUG ((DEBUG_INFO, "Copying Wi-Fi credentials in to network profile\n"));
  Profile->SSIdLength = AsciiStrnLenS (ConnMgrUI.SsidList[ConnMgrUI.SelectedIndex],
                                       EFI_MAX_SSID_LEN + 1);
  if (Profile->SSIdLength == EFI_MAX_SSID_LEN + 1) {
    DEBUG ((DEBUG_ERROR, "Invalid SsId length\n"));
    Status = EFI_INVALID_PARAMETER;
    goto Exit;
  }

  AsciiStrCpyS (Profile->SSId,
                ARRAY_SIZE (Profile->SSId),
                ConnMgrUI.SsidList[ConnMgrUI.SelectedIndex]);
  Profile->PasswordLength = ConnMgrUI.PasswordLength;
  AsciiStrCpyS (Profile->Password, ARRAY_SIZE (Profile->Password), ConnMgrUI.Password);

Exit:
  WifiCmFreeNetworkList (SsidList, SsidListLength);

  // WifiCmRestoreGraphicsResolution (&FrameBuffer, PreviousMode);

  //
  // Restore the original screen contents from back buffer
  //

  WifiCmRestoreFrameBuffer (&FrameBuffer);

  ZeroMem (ConnMgrUI.Password, sizeof (ConnMgrUI.Password));

  FreePool (FrameBuffer.Bitmap);
  FreePool (FrameBuffer.BackBuffer);
  FreePool (FontInfo.Font);

  DEBUG ((DEBUG_INFO, "Exiting Wi-Fi connection manager UI\n"));

  return Status;
}

