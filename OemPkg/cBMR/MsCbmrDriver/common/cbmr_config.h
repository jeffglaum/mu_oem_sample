#ifndef _CBMR_CONFIG_H_
#define _CBMR_CONFIG_H_

//
// Global includes
//

#include "cbmrincludes.h"
#include "cbmr.h"

#define CBMR_CONFIG_FILENAME             L"cbmr_config.txt"
#define CBMR_CONFIG_DEBUG_SECTION        t("[debug]")
#define CBMR_CONFIG_APP_SECTION          t("[app]")
#define CBMR_CONFIG_DRIVER_SECTION       t("[driver]")
#define CBMR_DCAT_ENDPOINT_TYPE_PROD_STR t("PROD")
#define CBMR_DCAT_ENDPOINT_TYPE_PPE_STR  t("PPE")

#define MAX_JSON_REQUEST_URL_SIZE 2048

typedef struct _URL_LIST {
    CHAR16** Urls;
    UINT32 UrlCount;
} URL_LIST, *PURL_LIST;

typedef enum _CBMR_ENDPOINT_TYPE {
    CBMR_ENDPOINT_TYPE_DCAT,
    CBMR_ENDPOINT_TYPE_LOCAL_HTTP, // for HTTP also mean HTTPS
    // CBMR_ENDPOINT_TYPE_LOCAL_HTTPS,
    CBMR_ENDPOINT_TYPE_USBKEY,
} CBMR_ENDPOINT_TYPE,
    *PCBMR_ENDPOINT_TYPE;

typedef enum _CBMR_DCAT_ENDPOINT_TYPE {
    CBMR_DCAT_ENDPOINT_TYPE_PROD,
    CBMR_DCAT_ENDPOINT_TYPE_PPE,
} CBMR_DCAT_ENDPOINT_TYPE,
    *PCBMR_DCAT_ENDPOINT_TYPE;

typedef enum _SPEW_TARGET {
    SPEW_CONSOLE = 1 << 0,
    SPEW_FILE = 1 << 1,
    SPEW_UEFI_VAR = 1 << 2, // CbmrUefiLogs | 887481f5-fa49-4f65-b03c-551db53c8c23
    SPEW_SERIAL = 1 << 3,
    SPEW_DEBUGGER = 1 << 4,
} SPEW_TARGET;

typedef struct _CBMR_CONFIG {
    //
    // Debug config
    //

    UINTN DebugMask;
    SPEW_TARGET SpewTarget;
    BOOLEAN EarlyBreak;

    //
    // App config
    //

    BOOLEAN ShowWiFiUX;
    CHAR8 WifiSid[EFI_MAX_SSID_LEN + 1];
    CHAR8 WifiPassword[MAX_80211_PWD_LEN + 1];

    //
    // Driver config
    //

    CHAR16* Url;
    CBMR_ENDPOINT_TYPE EndpointType;
    CBMR_DCAT_ENDPOINT_TYPE DcatEndpointType;
    BOOLEAN ForceHttps;
    BOOLEAN SkipHashValidation;
    BOOLEAN WriteSiUefiVariable;
    BOOLEAN WriteSi2UefiVariable;
    BOOLEAN EnableTestSigningOnStubOS;
    BOOLEAN ServiceViaLocalCbmrDriver;
    BOOLEAN WriteCertListToFile;
} CBMR_CONFIG, *PCBMR_CONFIG;

EFI_STATUS EFIAPI CbmrReadConfig(_In_ CHAR8* ConfigSection);
VOID EFIAPI CbmrFreeConfig();

extern CBMR_CONFIG gCbmrConfig;
#endif // _CBMR_CONFIG_H_
