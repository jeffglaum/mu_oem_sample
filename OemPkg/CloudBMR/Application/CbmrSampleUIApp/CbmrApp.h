/** @file CbmrApp.h

  cBMR (Cloud Bare Metal Recovery) sample application common definitions

  Copyright (c) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  The application is a sample, demonstrating how one might present the cBMR process to a user.
**/

#ifndef _CBMR_SAMPLE_UI_APP_H_
#define _CBMR_SAMPLE_UI_APP_H_

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BmpSupportLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MsColorTableLib.h>
#include <Library/MsUiThemeLib.h>
#include <Protocol/OnScreenKeyboard.h>
#include <Library/PrintLib.h>
#include <Library/SortLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/Ip4Config2.h>
#include <Protocol/Supplicant.h>
#include <Protocol/CloudBareMetalRecovery.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/HiiFont.h>
#include <Protocol/HiiImage.h>
#include <Protocol/Shell.h>
#include <Protocol/SimpleTextInEx.h>
#include <Protocol/SimpleWindowManager.h>
#include <Protocol/WiFi2.h>

#include <UIToolKit/SimpleUIToolKit.h>

#define NORMAL_VERTICAL_PADDING_PIXELS   10     // 10 pixels normal padding.
#define SECTION_VERTICAL_PADDING_PIXELS  20     // 20 pixels padding between sections.

#define DATA_LABEL_MAX_LENGTH     64
#define SSID_MAX_NAME_LENGTH      64
#define SSID_MAX_PASSWORD_LENGTH  64

// Dialog Protocol Guid
#define CBMR_APP_DIALOG_PROTOCOL_GUID  /* 567d4f03-6ff1-45cd-8fc5-9f192bc1450b */     \
{                                                                                  \
    0x567d4f03, 0x6ff1, 0x45cd, { 0x8f, 0xc5, 0x9f, 0x19, 0x2b, 0xc1, 0x45, 0x0b } \
}

// Dialog font sizes.  These represent vertical heights (in pixels) which in turn map to one of the custom fonts
// registered by the simple window manager.
//
#define SWM_MB_CUSTOM_FONT_BUTTONTEXT_HEIGHT  MsUiGetSmallFontHeight ()
#define SWM_MB_CUSTOM_FONT_TITLEBAR_HEIGHT    MsUiGetSmallFontHeight ()
#define SWM_MB_CUSTOM_FONT_CAPTION_HEIGHT     MsUiGetLargeFontHeight ()
#define SWM_MB_CUSTOM_FONT_BODY_HEIGHT        MsUiGetStandardFontHeight ()

typedef struct _CBMR_APP_CONTEXT {
  BOOLEAN                   bUseWiFiConnection;
  EFI_IP4_CONFIG2_POLICY    NetworkPolicy;
  CHAR8                     SSIDNameA[SSID_MAX_NAME_LENGTH];
  CHAR8                     SSIDPasswordA[SSID_MAX_PASSWORD_LENGTH];
  UINT32                    HorizontalResolution;
  UINT32                    VerticalResolution;
} CBMR_APP_CONTEXT, *PCBMR_APP_CONTEXT;

typedef enum {
  cBMRState = 0,
  DownloadFileCount,
  DownloadTotalSize,
  NetworkState,
  NetworkSSID,
  NetworkPolicy,
  NetworkIPAddr,
  NetworkGatewayAddr,
  NetworkDNSAddr
} CBMR_UI_DATA_LABEL_TYPE;

typedef enum {
  SWM_MB_IDOK = 1,                  // The OK button was selected.
  SWM_MB_IDCANCEL,                  // The Cancel button was selected.
  SWM_MB_TIMEOUT,                   // MessageBox with Timeout timed out
} SWM_MB_RESULT;

//
// cBMR App Function prototypes
//

EFI_STATUS
EFIAPI
GfxSetGraphicsResolution (
  IN UINT32   DesiredMode,
  OUT UINT32  *PreviousMode
  );

EFI_STATUS
EFIAPI
GfxGetGraphicsResolution (
  OUT UINT32  *Width,
  OUT UINT32  *Height
  );

EFI_STATUS
EFIAPI
CbmrUICreateWindow (
  Canvas  **WindowCanvas
  );

EFI_STATUS
EFIAPI
CbmrUIUpdateLabelValue (
  CBMR_UI_DATA_LABEL_TYPE  LabelType,
  CHAR16                   *String
  );

EFI_STATUS
EFIAPI
CbmrUIUpdateDownloadProgress (
  UINT8  Percent
  );

SWM_MB_RESULT
EFIAPI
CbmrUIWindowMessageHandler (
  Canvas  *WindowCanvas
  );

EFI_STATUS
EFIAPI
FindAndConnectToNetwork (
  OUT EFI_IP4_CONFIG2_INTERFACE_INFO  **InterfaceInfo
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
CbmrUIGetSSIDAndPassword (
  OUT CHAR16  *SSIDName,
  IN UINT8    SSIDNameMaxLength,
  OUT CHAR16  *SSIdPassword,
  IN UINT8    SSIDPasswordMaxLength
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

SWM_MB_RESULT
ProcessWindowInput (
  IN  MS_SIMPLE_WINDOW_MANAGER_PROTOCOL  *this,
  IN  Canvas                             *WindowCanvas,
  IN  EFI_ABSOLUTE_POINTER_PROTOCOL      *PointerProtocol,
  IN  UINT64                             Timeout
  );

#endif // _CBMR_SAMPLE_UI_APP_H_
