/** @file CbmrSupportLib.h

  cBMR (Cloud Bare Metal Recovery) application support library.

  Copyright (c) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  The support library enables separation of the core cBMR application functionality from the UI/presentation.
**/

#ifndef _CBMR_APP_SUPPORT_LIB_H_
#define _CBMR_APP_SUPPORT_LIB_H_

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/CloudBareMetalRecovery.h>
#include <Protocol/Ip4Config2.h>
#include <Protocol/Supplicant.h>
#include <Protocol/WiFi2.h>

#define SSID_MAX_NAME_LENGTH      64
#define SSID_MAX_PASSWORD_LENGTH  64

typedef EFI_STATUS (EFIAPI *PFN_GET_SSID_AND_PASSWORD_FROM_USER)(
  OUT CHAR16  *SSIDName,
  IN UINT8    SSIDNameMaxLength,
  OUT CHAR16  *SSIDPassword,
  IN UINT8    SSIDPasswordMaxLength
  );

EFI_STATUS
EFIAPI
FindAndConnectToNetwork (
  IN PFN_GET_SSID_AND_PASSWORD_FROM_USER  GetWiFiCredentialsCallback,
  OUT EFI_IP4_CONFIG2_INTERFACE_INFO      **InterfaceInfo,
  OUT BOOLEAN                             *bIsWiFiConnection
  );

EFI_STATUS
EFIAPI
ConnectToNetwork (
  EFI_IP4_CONFIG2_INTERFACE_INFO  **InterfaceInfo
  );

EFI_STATUS
EFIAPI
ConnectToWiFiAccessPoint (
  IN CHAR8  *SSIdName,
  IN CHAR8  *SSIdPassword
  );

EFI_STATUS
EFIAPI
GetNetworkPolicy (
  OUT EFI_IP4_CONFIG2_POLICY   *Policy
  );

EFI_STATUS
EFIAPI
GetGatewayIpAddress (
  IN EFI_IP4_CONFIG2_INTERFACE_INFO  *InterfaceInfo,
  OUT EFI_IPv4_ADDRESS               *GatewayIpAddress
  );

EFI_STATUS
EFIAPI
GetDNSServerIpAddress (
  OUT EFI_IPv4_ADDRESS  *DNSIpAddress
  );

EFI_STATUS
EFIAPI
GetWiFiNetworkList (
  IN  EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL  *WiFi2Protocol,
  OUT EFI_80211_GET_NETWORKS_RESULT            **NetworkInfoPtr
  );

VOID
EFIAPI
SSIdNameToStr (
  IN  EFI_80211_SSID  *SSIdStruct,
  OUT CHAR8           *SSIdNameStr
  );

EFI_STATUS
EFIAPI
CbmrDriverConfigure (
  IN EFI_MS_CBMR_CONFIG_DATA        *CbmrConfigData,
  IN EFI_MS_CBMR_PROGRESS_CALLBACK  ProgressCallback
  );

EFI_STATUS
EFIAPI
CbmrDriverFetchCollateral (
  OUT EFI_MS_CBMR_COLLATERAL  **Collateral,
  OUT UINTN                   *CollateralSize
  );

EFI_STATUS
EFIAPI
CbmrDriverStartDownload (
  VOID
  );

#endif //_CBMR_APP_SUPPORT_LIB_H_.
