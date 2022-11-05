/** @file CbmrAppWiFiSupport.c

  cBMR (Cloud Bare Metal Recovery) sample application Wi-Fi helper functions

  Copyright (c) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  The application is a sample, demonstrating how one might present the cBMR process to a user.
**/

#include "CbmrApp.h"

// Event used with the WiFi protocol
EFI_EVENT  gWiFiEvent = NULL;

extern UINTN  PcdCbmrGetWifiNetworksTimeout;
extern UINTN  PcdCbmrWifiNetworkConnectTimeout;

/**
  WiFi event callback will close the event then clear the global event variable as a flag to the primary process flow
  to continue execution.

  @param[in]  Event     Handle to event that caused the callback
  @param[in]  Context   Pointer to a boolean used to hold up the process until the callback happens
**/
VOID
EFIAPI
WiFiEventCallback (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  // Close the initating event
  gBS->CloseEvent (Event);

  // Confirm event matches our event, then NULL the global variable
  if (Event == gWiFiEvent) {
    gWiFiEvent = NULL;
  }
}

/**
  Spins until a timeout or the gWiFiEvent changes to NULL.  Used to halt the primary code flow until the WiFi driver
  indicates it's data is ready.

  @param[in]  TimeoutInSeconds  Time to wait in seconds
**/
EFI_STATUS
EFIAPI
WaitForWiFiEvent (
  IN UINT32  TimeoutInSeconds
  )
{
  volatile EFI_EVENT  *pEvent;
  UINTN               Timeout_uS;

  // Init function vars
  pEvent     = &gWiFiEvent;
  Timeout_uS = (UINTN)TimeoutInSeconds * 1000 * 1000;

  // Loop while the event has not triggered
  while (*pEvent != NULL) {
    // If a timeout, force the event closed and return
    if (Timeout_uS == 0) {
      return EFI_TIMEOUT;
    }

    // 10 mS stall before looping
    gBS->Stall (10 * 1000);
    Timeout_uS -= (10 * 1000);
  }

  return EFI_SUCCESS;
}

/**
  Function copies the byte chars from the SSID structure to a NULL terminated ASCII string.  If the input SSID
  structure length value is larger than the max defined EFI_MAX_SSID_LEN, the value is forced to EFI_MAX_SSID_LEN
  and the function proceeds to copy the data.

  @param[in]  SSIdStruct   Pointer to the SSID structure with the data length and byte array
  @param[out] SSIdNameStr  Buffer to receive the NULL terminated string which is expected to be at least
                           (EFI_MAX_SSID_LEN + 1) bytes in size
**/
VOID
EFIAPI
SSIdNameToStr (
  IN  EFI_80211_SSID  *SSIdStruct,
  OUT CHAR8           *SSIdNameStr
  )
{
  BOOLEAN  Invalid = FALSE;

  if (SSIdStruct->SSIdLen > EFI_MAX_SSID_LEN) {
    Invalid             = TRUE;
    SSIdStruct->SSIdLen = EFI_MAX_SSID_LEN;
  }

  CopyMem (SSIdNameStr, SSIdStruct->SSId, SSIdStruct->SSIdLen);
  SSIdNameStr[SSIdStruct->SSIdLen] = 0;

  if (Invalid) {
    DEBUG ((DEBUG_WARN, "WARN [cBMR App]: Invalid SSID name string length provided by WiFi access point\n"));
    DEBUG ((DEBUG_WARN, "                 '%s' has been truncated to the max length of %d chars\n", SSIdNameStr, SSIdStruct->SSIdLen));
  }
}

/**
  Uses the connection manager protocol to retrieve a list of wireless networks

  @param[in]  WiFi2Protocol    Pointer to the WiFi protocol to be used
  @param[out] NetworkInfoPtr   Receives a pointer to a buffer containing the EFI_80211_GET_NETWORKS_RESULT structure.
                               The caller is responsible for freeing this buffer.

  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
GetWiFiNetworkList (
  IN  EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL  *WiFi2Protocol,
  OUT EFI_80211_GET_NETWORKS_RESULT            **NetworkInfoPtr
  )
{
  EFI_80211_GET_NETWORKS_TOKEN  GetNetworksToken;
  EFI_80211_GET_NETWORKS_DATA   GetNetworksData;
  CHAR8                         SSIdNameStr[EFI_MAX_SSID_LEN + 1];
  EFI_STATUS                    Status;

  DEBUG ((DEBUG_INFO, "INFO [cBMR App]: Entered function %a()\n", __FUNCTION__));

  // Create an event to be used with the WiFi2Protocol->GetNetworks().  Per spec the event must be EVT_NOTIFY_SIGNAL.
  gWiFiEvent = NULL;
  Status     = gBS->CreateEvent (
                      EVT_NOTIFY_SIGNAL,
                      TPL_CALLBACK,
                      WiFiEventCallback,
                      NULL,
                      &gWiFiEvent
                      );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Setup the GetNetworks input structures
  GetNetworksToken.Event  = gWiFiEvent;
  GetNetworksToken.Status = EFI_PROTOCOL_ERROR;
  GetNetworksToken.Data   = &GetNetworksData;
  GetNetworksToken.Result = NULL;

  // The GetNetworksData structure is used to provide a list of hidden networks to look for
  ZeroMem (&GetNetworksData, sizeof (GetNetworksData));
  GetNetworksData.NumOfSSID = 0;

  // Call the connection manager to retrieve the network list
  Status = WiFi2Protocol->GetNetworks (WiFi2Protocol, &GetNetworksToken);

  // On success, wait for the event indicating data is ready
  if (!EFI_ERROR (Status)) {
    Status = WaitForWiFiEvent (FixedPcdGet32 (PcdCbmrGetWifiNetworksTimeout));
  }

  // If error in call or wait, close the event and return
  if (EFI_ERROR (Status)) {
    gBS->CloseEvent (gWiFiEvent);
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL::GetNetworks() - Status %r\n", Status));
    return Status;
  }

  // The GetNetworks call was successful, so use the GetNetworksToken.Status as this function's return status.
  Status = GetNetworksToken.Status;
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: EFI_80211_GET_NETWORKS_TOKEN::Status %r\n", Status));
    return Status;
  }

  *NetworkInfoPtr = GetNetworksToken.Result;

  // Report the data found and return
  DEBUG ((DEBUG_INFO, "INFO [cBMR App]: Available Wi-Fi networks:\n"));
  DEBUG ((DEBUG_INFO, "                 Strength | SSID\n"));
  DEBUG ((DEBUG_INFO, "                 -------- | ----------\n"));
  for (UINTN i = 0; i < (*NetworkInfoPtr)->NumOfNetworkDesc; i++) {
    SSIdNameToStr (&((*NetworkInfoPtr)->NetworkDesc[i].Network.SSId), SSIdNameStr);
    DEBUG ((DEBUG_INFO, "                  %3d%%   | %a\n", (*NetworkInfoPtr)->NetworkDesc[i].NetworkQuality, SSIdNameStr));
  }

  return EFI_SUCCESS;
}

/**
  Performs the steps to communicate to the wireless access point and connect

  @param[in]  WiFi2Protocol  Protocol to use to attempt connection
  @param[in]  Network        Description structure of network to connect to

  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
AttemptWiFiConnection (
  IN EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL  *WiFi2Protocol,
  IN EFI_80211_NETWORK                        *Network
  )
{
  EFI_80211_CONNECT_NETWORK_TOKEN  NetworkConnectToken;
  EFI_80211_CONNECT_NETWORK_DATA   NetworkConnectData;
  EFI_STATUS                       Status;

  // Create an event to be used with the WiFi2Protocol->ConnectNetwork().  Per spec the event must be EVT_NOTIFY_SIGNAL.
  gWiFiEvent = NULL;
  Status     = gBS->CreateEvent (
                      EVT_NOTIFY_SIGNAL,
                      TPL_CALLBACK,
                      WiFiEventCallback,
                      NULL,
                      &gWiFiEvent
                      );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: CreateEvent( WiFiEvent ) - Status %r\n", Status));
    return Status;
  }

  // Setup the input parameters for the ConnectNetwork call
  NetworkConnectToken.Event         = gWiFiEvent;                                       // Event triggered when connection is finished
  NetworkConnectToken.Status        = EFI_TIMEOUT;                                      // Init return structure status code
  NetworkConnectToken.Data          = &NetworkConnectData;                              // Connect token data structure
  NetworkConnectToken.ResultCode    = (EFI_80211_CONNECT_NETWORK_RESULT_CODE)-1;        // Init result to an undefined value to prove call changed the data
  NetworkConnectData.Network        = Network;                                          // Info structure of network to connected to
  NetworkConnectData.FailureTimeout = FixedPcdGet32 (PcdCbmrWifiNetworkConnectTimeout); // Set timeout value

  // Initiate the WiFi network connect
  Status = WiFi2Protocol->ConnectNetwork (WiFi2Protocol, &NetworkConnectToken);

  // On success, wait for the event indicating data is ready.  Use 1 second more than the timeout provided in the
  // NetworkConnectData structure to catch the error where the ConnectNetwork function may not timeout properly.
  if (!EFI_ERROR (Status)) {
    Status = WaitForWiFiEvent (FixedPcdGet32 (PcdCbmrWifiNetworkConnectTimeout) + 1);
  }

  // If error in call or wait, close the event and return
  if (EFI_ERROR (Status)) {
    gBS->CloseEvent (gWiFiEvent);
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL::ConnectNetwork() - Status %r\n", Status));
    return Status;
  }

  // Convert the result code to an EFI_STATUS and return
  switch (NetworkConnectToken.ResultCode) {
    case ConnectSuccess:
      Status = EFI_SUCCESS;
      break;

    case ConnectRefused:
      DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Connection Refused\n"));
      DEBUG ((DEBUG_ERROR, "                  The connection was refused by the Network - Status EFI_ACCESS_DENIED\n"));
      Status = EFI_ACCESS_DENIED;
      break;

    case ConnectFailed:
      DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Connection Failed\n"));
      DEBUG ((DEBUG_ERROR, "                  The connection establishment operation failed (i.e, Network is not detected) - Status EFI_NO_RESPONSE\n"));
      Status = EFI_NO_RESPONSE;
      break;

    case ConnectFailureTimeout:
      DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Connection Timeout\n"));
      DEBUG ((DEBUG_ERROR, "                  The connection establishment operation was terminated on timeout - Status EFI_TIMEOUT\n"));
      Status = EFI_TIMEOUT;
      break;

    default:
      DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Connection Unspecified\n"));
      DEBUG ((DEBUG_ERROR, "                  The connection establishment operation failed on other reason - Status EFI_PROTOCOL_ERROR\n"));
      Status = EFI_PROTOCOL_ERROR;
      break;
  }

  return Status;
}

/**
  Primary function to initiate connection to a WiFi access point

  @param[in]  SSIdName      Network SSID to connect to
  @param[in]  SSIdPassword  ASCII string of password to use when connecting

  @retval EFI_STATUS
**/
EFI_STATUS
EFIAPI
ConnectToWiFiAccessPoint (
  IN CHAR8  *SSIdName,
  IN CHAR8  *SSIdPassword
  )
{
  EFI_STATUS                               Status              = EFI_SUCCESS;
  EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL  *WiFi2Protocol      = NULL;
  EFI_SUPPLICANT_PROTOCOL                  *SupplicantProtocol = NULL;
  EFI_80211_NETWORK_DESCRIPTION            *NetworkDescription = NULL;
  EFI_80211_GET_NETWORKS_RESULT            *NetworkList        = NULL;
  CHAR8                                    SSIdNameStr[EFI_MAX_SSID_LEN + 1];

  // Locate the WiFi2 Network and Supplicant Protocols
  //
  Status = gBS->LocateProtocol (&gEfiWiFi2ProtocolGuid, NULL, (VOID **)&WiFi2Protocol);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to find the WiFi2 protocol (%r).\r\n", Status));
    goto Exit;
  }

  Status = gBS->LocateProtocol (&gEfiSupplicantProtocolGuid, NULL, (VOID **)&SupplicantProtocol);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to find the Wi-Fi supplicant protocol (%r).\r\n", Status));
    goto Exit;
  }

  // Retrieve an EFI_80211_GET_NETWORKS_RESULT structure that indicates all networks in range.
  // NetworkList is allocated memory which must be freed.
  //
  Status = GetWiFiNetworkList (WiFi2Protocol, &NetworkList);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to get list of Wi-Fi networks (%r).\r\n", Status));
    goto Exit;
  }

  // Walk the NetworkList list to find the requested SSID's network description structure
  //
  for (UINTN i = 0; i < NetworkList->NumOfNetworkDesc; i++) {
    SSIdNameToStr (&(NetworkList->NetworkDesc[i].Network.SSId), SSIdNameStr);
    if (0 == AsciiStrCmp (SSIdName, SSIdNameStr)) {
      NetworkDescription = &(NetworkList->NetworkDesc[i]);
    }
  }

  if (NetworkDescription == NULL) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Requested network with SSID '%a' not found\n", SSIdName));
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  // Send the SSID structure retrieved from the WiFi to the supplicant protocol
  //
  Status = SupplicantProtocol->SetData (
                                 SupplicantProtocol,
                                 EfiSupplicant80211TargetSSIDName,
                                 &(NetworkDescription->Network.SSId),
                                 sizeof (EFI_80211_SSID)
                                 );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Supplicant->SetData( EfiSupplicant80211TargetSSIDName ) - Status %r\n", Status));
    goto Exit;
  }

  // Send the password to the supplicant protocol
  //
  Status = SupplicantProtocol->SetData (
                                 SupplicantProtocol,
                                 EfiSupplicant80211PskPassword,
                                 SSIdPassword,
                                 AsciiStrLen (SSIdPassword) + 1
                                 );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Supplicant->SetData( EfiSupplicant80211PskPassword ) - Status %r\n", Status));
    goto Exit;
  }

  // Initate the connection with the WiFi protocol
  //
  Status = AttemptWiFiConnection (WiFi2Protocol, &(NetworkDescription->Network));

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to connect to Wi-Fi - Status %r\n", Status));
    goto Exit;
  }

Exit:

  if (NetworkList != NULL) {
    FreePool (NetworkList);
  }

  return Status;
}
