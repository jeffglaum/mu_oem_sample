#ifndef _CBMR_CONFIG_H_
#define _CBMR_CONFIG_H_

#include <Uefi.h>

#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/SortLib.h>
#include <Library/MemoryAllocationLib.h>

#include <Protocol/Ip4Config2.h>
#include <Protocol/Supplicant.h>
#include <Protocol/WiFi2.h>
#include <Protocol/MsCloudBareMetalRecovery.h>
#include <Protocol/HiiImage.h>
#include <Protocol/HiiFont.h>
#include <Protocol/Shell.h>
#include <Protocol/GraphicsOutput.h>

// WARNING: NetworkPkg does not put this file in the proper include directory.  Using '..' to allow reaching out of the package include directory.
#include <..\WifiConnectionManagerDxe\WifiConnectionMgrDxe.h>


#define MAX_80211_PWD_LEN           63
#define SEC_TO_uS(Sec)              (1000000 * Sec)

typedef struct _WIFI_CM_UI_STATE {
  CHAR8** SsidList;
  UINTN SsidListLength;
  UINTN SelectedIndex;

  CHAR8 Password[MAX_80211_PWD_LEN + 1];
  UINTN PasswordLength;
} WIFI_CM_UI_STATE, *PWIFI_CM_UI_STATE;

//
// Configuration support
//

// App configuration data context structure
typedef struct _CBMR_CONFIG {
  BOOLEAN ShowWiFiUX;
  CHAR8 *WifiSid;
  CHAR8 *WifiPwd;
} CBMR_CONFIG;

//
// cBMR App Function prototypes
//

EFI_STATUS
EFIAPI
CbmrUIInitialize ();

EFI_STATUS
EFIAPI
CbmrUIUpdateDownloadProgress (
  IN CHAR16* DownloadStatus,
  IN UINTN Percentage1,
  IN UINTN Percentage2);

EFI_STATUS
EFIAPI
CbmrUIUpdateApplicationStatus (
  IN CHAR16* ApplicationStatus);

EFI_STATUS
EFIAPI
CbmrUIFreeResources ();

//
// Wi-Fi Connection Manager Function prototypes
//

EFI_STATUS
EFIAPI
WifiCmUIMain (
  IN OUT PEFI_MS_CBMR_WIFI_NETWORK_PROFILE Profile);

EFI_STATUS
EFIAPI
WifiCmConnect (
  IN CHAR8* SsidName,
  IN CHAR8* Password);

#endif // _CBMR_CONFIG_H_
