/*++

Copyright (c) 2021 Microsoft Corporation

Module Name:

    cbmr_core.c

Abstract:

    This module implements the CBMR core functionality that performs
    Cloud Bare Metal Recovery.

Author:

    Jancarlo Perez (jpere) 29-March-2021

Environment:

    UEFI mode only.

--*/

//
// Global includes
//

#include "cbmrincludes.h"
#include "strsafe.h"

//
// Local includes
//

#include "cbmr.h"
#include "cbmr_core.h"
#include "http.h"
#include "ramdisk.h"
#include "tls.h"
#include "file.h"
#include "dcat.h"
#include "XmlTypes.h"
#include "xmltreelib.h"
#include "xmltreequerylib.h"
#include "wim.h"
#include "string_helper.h"
#include "patched_bcd.h"
#include "error.h"
#include "tls_certs.h"

#ifndef UEFI_BUILD_SYSTEM
#pragma prefast(push)
#pragma prefast(disable : 6101, \
                "False positive - _Out_ parameter is always initialized upon success")
#endif

#ifdef DEBUGMODE
#define CBMR_DRIVER_FILE_NAME L"cbmr_driver_debug.efi"
#else
#define CBMR_DRIVER_FILE_NAME L"cbmr_driver.efi"
#endif

#define STUBOS_VOLUME_LABEL L"STUBOS"

#define STUBOS_WIM_BOOT_SDI_PATH    "\\Windows\\Boot\\DVD\\EFI\\boot.sdi"
#define STUBOS_WIM_BOOTMGR_PATH     "\\Windows\\Boot\\EFI\\bootmgfw.efi"
#define STUBOS_WIM_BCD_PATH         "\\Windows\\Boot\\DVD\\EFI\\BCD"
#define STUBOS_WIM_CBMR_DRIVER_PATH "\\Windows\\Boot\\EFI\\cbmr_driver.efi"

#define RAMDISK_CBMR_DIRECTORY         L"cbmr"
#define RAMDISK_CBMR_DRIVERS_DIRECTORY RAMDISK_CBMR_DIRECTORY L"\\drivers"
#define RAMDISK_WIFI_PROFILE_PATH      RAMDISK_CBMR_DIRECTORY L"\\wifi.txt"
#define RAMDISK_SI_WIM_PATH            RAMDISK_CBMR_DIRECTORY L"\\si.wim"
#define RAMDISK_SI2_WIM_PATH           RAMDISK_CBMR_DIRECTORY L"\\si2.wim"
#define RAMDISK_DCAT_INFO_PATH         RAMDISK_CBMR_DIRECTORY L"\\dcat.txt"
#define RAMDISK_CBMR_DRIVER_PATH       RAMDISK_CBMR_DIRECTORY L"\\cbmr_driver.efi"
#define RAMDISK_WIM_PATH               L"\\sources\\boot.wim"
#define RAMDISK_BOOT_SDI_PATH          L"\\boot\\boot.sdi"
#define RAMDISK_BCD_PATH               L"\\efi\\microsoft\\boot\\bcd"

#define EFI_MS_CBMR_SOFTWARE_INVENTORY_VARIABLE           L"SoftwareInventory"
#define EFI_MS_CBMR_SOFTWARE_INVENTORY_SECONDARY_VARIABLE L"SoftwareInventorySecondary"

typedef struct _WIM_TO_RAMDISK_FILE {
    //
    // Relative file path in WIM (relative to root).
    //

    _Field_size_(FilePathInWimLength) CHAR8* FilePathInWim;

    //
    // Number of characters in file path
    //

    UINTN FilePathInWimLength;

    //
    // Local location where the collateral is saved. In our case, it will be the
    // path inside the Ramboot fat32 volume
    //

    CHAR16* FilePathInRamDisk;

    //
    // Critical for boot process?
    //

    BOOLEAN Critical;
} WIM_TO_RAMDISK_FILE, *PWIM_TO_RAMDISK_FILE;

//
// DcatMetadataChannelTlsCaCerts should contain intermediate (or more scoped)
// certs used for cert pinning against metadata channel only. This is a very
// strict list and should only be updated if adding additional metadata channel
// CA certs, and nothing else.
//

static CERT DcatMetadataChannelTlsCaCerts[] = {
    {.Size = ARRAYSIZE(MicrosoftUpdateSecureServerCA),
     .Buffer = MicrosoftUpdateSecureServerCA,
     .Revoked = FALSE},
};

//
// DcatContentChannelTlsCaCerts should contain certs used for cert pinning
// against DCAT content channel only. This array is not generally used for
// content download as it is done via HTTP, but HTTPS option does exist so
// we'll leave this option open.
//

static CERT DcatContentChannelTlsCaCerts[] = {
    {.Size = ARRAYSIZE(MicrosoftUpdateSecureServerCA),
     .Buffer = MicrosoftUpdateSecureServerCA,
     .Revoked = FALSE},
    {.Size = ARRAYSIZE(MicrosoftUpdateSecureServerCAExtOriginInt),
     .Buffer = MicrosoftUpdateSecureServerCAExtOriginInt,
     .Revoked = FALSE},
};

//
// Local functions
//

static EFI_STATUS CbmrBuildRequestHeaders(_In_ CHAR8* Url,
                                          _In_ UINTN UrlLength,
                                          _Outptr_result_buffer_(*Count) EFI_HTTP_HEADER** Headers,
                                          _Out_ UINTN* Count)
{
    EFI_STATUS Status = EFI_SUCCESS;
    VOID* UrlParser = NULL;
    CHAR8* Hostname = NULL;
    UINT16 Port = 0;
    CHAR8* FormatString = t("%s:%d"); // Host = hostname[:port]
    EFI_HTTP_HEADER* RequestHeaders;
    UINTN HeaderCount;

    // Static header fields

    struct HeaderNameValue {
        CHAR8* Name;
        CHAR8* Value;
    } NameValues[] = {
        {t("User-Agent"),
         t("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/103.0.5060.134 Safari/537.36 Edg/103.0.1264.71")},
        {t("Connection"), t("keep-alive")},
        {t("Keep-Alive"), t("timeout=3600, max=1000")},
    };

    HeaderCount = 1 + _countof(NameValues); // Host field + other static fields

    RequestHeaders = AllocateZeroPool(sizeof(EFI_HTTP_HEADER) * HeaderCount); // Allocate headers
    if (RequestHeaders == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    //
    // Populate 'Host' header field
    //

    Status = HttpParseUrl(Url, (UINT32)UrlLength, FALSE, &UrlParser);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("HttpParseUrl() failed 0x%zx", Status);
        goto Exit;
    }

    Status = HttpUrlGetHostName(Url, UrlParser, &Hostname);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("HttpUrlGetHostName() failed 0x%zx", Status);
        goto Exit;
    }

    Status = HttpUrlGetPort(Url, UrlParser, &Port);
    if (EFI_ERROR(Status)) {
        if (Status == EFI_NOT_FOUND) {
            //
            // No port found, reset format specifier to include
            // just hostname and proceed.
            //

            Status = EFI_SUCCESS;
            FormatString = t("%s");
        } else {
            DBG_ERROR("HttpUrlGetPort() failed 0x%zx", Status);
            goto Exit;
        }
    }

    UINTN Result = 0;
    CHAR8 AsciiHostHeaderValue[1024] = {0};

    Result = StringCchPrintfA((STRSAFE_LPSTR)AsciiHostHeaderValue,
                              _countof(AsciiHostHeaderValue),
                              (STRSAFE_LPCSTR)FormatString,
                              Hostname,
                              Port);
    if (FAILED(Result)) {
        DBG_ERROR("StringCchPrintfA failed 0x%zx", Result);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    RequestHeaders[0].FieldName = AllocateCopyPool(_countof(HTTP_HEADER_HOST), HTTP_HEADER_HOST);
    if (RequestHeaders[0].FieldName == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    RequestHeaders[0].FieldValue = AllocateCopyPool(AsciiStrnLenS(AsciiHostHeaderValue,
                                                                  _countof(AsciiHostHeaderValue)) +
                                                        1,
                                                    AsciiHostHeaderValue);
    if (RequestHeaders[0].FieldValue == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    //
    // Populate static header fields
    //

    for (UINTN i = 0, j = 1; i < _countof(NameValues); i++, j++) {
        RequestHeaders[j].FieldName = AllocateCopyPool(AsciiStrLen(NameValues[i].Name) + 1,
                                                       NameValues[i].Name);
        if (RequestHeaders[j].FieldName == NULL) {
            Status = EFI_OUT_OF_RESOURCES;
            goto Exit;
        }

        RequestHeaders[j].FieldValue = AllocateCopyPool(AsciiStrLen(NameValues[i].Value) + 1,
                                                        NameValues[i].Value);
        if (RequestHeaders[j].FieldValue == NULL) {
            Status = EFI_OUT_OF_RESOURCES;
            goto Exit;
        }
    }

    *Headers = RequestHeaders;
    *Count = HeaderCount;

Exit:

    if (EFI_ERROR(Status)) {
        if (Status == EFI_OUT_OF_RESOURCES) {
            DBG_ERROR("Out of memory");
        }

        HttpFreeHeaderFields(RequestHeaders, HeaderCount);
    }

    HttpUrlFreeParser(UrlParser);
    FreePool(Hostname);
    return Status;
}

#ifdef DEBUGMODE
static EFI_STATUS CbmrFetchCollateralsSizeFromHttp(_In_ PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal)
{
    EFI_STATUS Status = EFI_SUCCESS;
    CHAR8 AsciiUrl[4096] = {0};
    EFI_HTTP_HEADER* HttpHeaders = NULL;
    UINTN HeaderCount = 0;
    HTTP_RESPONSE* Response = NULL;

    //
    // Check for local TLS certs and set them if found.
    //

    Status = TlsSetCACertListDebug();
    if (EFI_ERROR(Status)) {
        DBG_ERROR("TlsSetCACertListDebug() failed 0x%zx", Status);
        goto Exit;
    }

    for (UINTN i = 0; i < Internal->NumberOfCollaterals; i++) {
        DBG_INFO_U(L"Getting Size for: %s", Internal->Collaterals[i].RootUrl);

        UnicodeStrToAsciiStr(Internal->Collaterals[i].RootUrl, AsciiUrl);

        Status = CbmrBuildRequestHeaders(AsciiUrl,
                                         AsciiStrnLenS(AsciiUrl, _countof(AsciiUrl)),
                                         &HttpHeaders,
                                         &HeaderCount);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("CbmrBuildRequestHeaders() failed 0x%zx", Status);
            goto Exit;
        }

        Status = HttpIssueRequest(Internal->HttpContext,
                                  Internal->Collaterals[i].RootUrl,
                                  Internal->Collaterals[i].RootUrlLength,
                                  HttpMethodHead,
                                  HttpHeaders,
                                  HeaderCount,
                                  NULL,
                                  0,
                                  0,
                                  &Response);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("HttpIssueRequest() failed x0%zx", Status);
            goto Exit;
        }

        HttpFreeHeaderFields(HttpHeaders, HeaderCount);
        HttpHeaders = NULL;
        HeaderCount = 0;

        Internal->Collaterals[i].CollateralSize = HttpGetContentLength(Response);

        DBG_INFO_U(L"Size for: %zu", Internal->Collaterals[i].CollateralSize);

        HttpFreeResponse(Internal->HttpContext, Response);
        Response = NULL;
    }

Exit:

    HttpFreeResponse(Internal->HttpContext, Response);
    HttpFreeHeaderFields(HttpHeaders, HeaderCount);

    return Status;
}
#endif

#ifdef DEBUGMODE
static EFI_STATUS CbmrFetchCollateralsFromUSBKey(_In_ PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal)
{
    EFI_STATUS Status = EFI_SUCCESS;

    EFI_MS_CBMR_COLLATERAL Collaterals[] = {
        // clang-format off
        {sizeof(EFI_MS_CBMR_COLLATERAL), L"usbkey\\boot.wim", _countof(L"usbkey\\boot.wim"), L"usbkey\\boot.wim", _countof(L"usbkey\\boot.wim"), RAMDISK_WIM_PATH},
        // TODO: Enable below once servicing story is finalized and http module know how to handle 404 errors
        // {sizeof(EFI_MS_CBMR_COLLATERAL), NULL, CBMR_DRIVER_FILE_NAME, L"memory", 0, TRUE, NULL},
        // clang-format on
    };

    Internal->NumberOfCollaterals = _countof(Collaterals);

    Internal->Collaterals = AllocateZeroPool(sizeof(EFI_MS_CBMR_COLLATERAL) *
                                             Internal->NumberOfCollaterals);
    if (Internal->Collaterals == NULL) {
        DBG_ERROR("Unable to allocate memory for Collaterals");
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    for (UINTN i = 0; i < Internal->NumberOfCollaterals; i++) {
        EFI_FILE_PROTOCOL* File = NULL;
        Status = StrDup(Collaterals[i].FilePath, &Internal->Collaterals[i].FilePath);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("StrDup() failed");
            goto Exit;
        }

        Status = StrDup(Collaterals[i].RootUrl, &Internal->Collaterals[i].RootUrl);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("StrDup() failed");
            goto Exit;
        }

        Status = StrDup(Collaterals[i].RelativeUrl, &Internal->Collaterals[i].RelativeUrl);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("StrDup() failed");
            goto Exit;
        }

        Internal->Collaterals[i].StoreInMemory = Collaterals[i].StoreInMemory;

        Status = FileLocateAndOpen(Collaterals[i].RelativeUrl, EFI_FILE_MODE_READ, &File);
        if (EFI_ERROR(Status)) {
            DBG_ERROR_U(L"FileLocateAndOpen() Failed 0x%zx %s", Status, Collaterals[i].RelativeUrl);
            goto CloseFile;
        }

        Status = FileGetSize(File, (UINT64*)&Internal->Collaterals[i].CollateralSize);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileGetSize() Failed 0x%zx", Status);
            goto CloseFile;
        }

    CloseFile:
        FileClose(File);
    }

    DBG_INFO("Fetched collaterals from USB Key");

    return Status;

Exit:
    if (Internal->Collaterals != NULL) {
        for (UINTN i = 0; i < Internal->NumberOfCollaterals; i++) {
            FreePool(Internal->Collaterals[i].RootUrl);
            FreePool(Internal->Collaterals[i].FilePath);
        }

        FreePool(Internal->Collaterals);
        Internal->Collaterals = NULL;
    }

    return Status;
}
#endif

#ifdef DEBUGMODE
static EFI_STATUS CbmrFetchCollateralsFromHttpEndpoint(_In_ PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal)
{
    EFI_STATUS Status = EFI_SUCCESS;
    CHAR16 AbsoluteUrl[4096] = {0};

    EFI_MS_CBMR_COLLATERAL Collaterals[] = {
        // clang-format off
        {sizeof(EFI_MS_CBMR_COLLATERAL), NULL, 0, L"boot.wim", _countof(L"boot.wim"), RAMDISK_WIM_PATH},
        // TODO: Enable below once servicing story is finalized and http module know how to handle 404 errors
        // {sizeof(EFI_MS_CBMR_COLLATERAL), NULL, CBMR_DRIVER_FILE_NAME, L"memory", 0, TRUE, NULL},
        // clang-format on
    };

    Internal->NumberOfCollaterals = _countof(Collaterals);

    Internal->Collaterals = AllocateZeroPool(sizeof(EFI_MS_CBMR_COLLATERAL) *
                                             Internal->NumberOfCollaterals);
    if (Internal->Collaterals == NULL) {
        DBG_ERROR("Unable to allocate memory for Collaterals");
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    for (UINTN i = 0; i < Internal->NumberOfCollaterals; i++) {
        //
        // Construct absolute collateral URL by concatenating
        // gCbmrConfig.Url with Collaterals[i].RelativeUrl.
        // Ex: URL = https://microsoft.com,
        // RelativeUrl = collaterals/bootmgr.efi.
        // AbsoluteURL = https://microsoft.com/collaterals/bootmgr.efi.
        //

        UINTN Result = 0;

        Result = StringCchPrintfW(AbsoluteUrl,
                                  _countof(AbsoluteUrl),
                                  (CHAR16*)L"%s%s",
                                  gCbmrConfig.Url,
                                  Collaterals[i].RelativeUrl);
        if (FAILED(Result)) {
            DBG_ERROR("StringCchPrintfW failed 0x%zx", Result);
            Status = EFI_INVALID_PARAMETER;
            goto Exit;
        }

        EFI_STATUS UrlStatus = StrDup(AbsoluteUrl, &Internal->Collaterals[i].RootUrl);
        Internal->Collaterals[i].RootUrlLength = StrnLenS(Internal->Collaterals[i].RootUrl, 4096);
        EFI_STATUS FilePathStatus = StrDup(Collaterals[i].FilePath,
                                           &Internal->Collaterals[i].FilePath);
        if (EFI_ERROR(UrlStatus) || EFI_ERROR(FilePathStatus)) {
            DBG_ERROR("StrDup() failed");
            Status = EFI_OUT_OF_RESOURCES;
            goto Exit;
        }

        Internal->Collaterals[i].StoreInMemory = Collaterals[i].StoreInMemory;
    }

    Status = CbmrFetchCollateralsSizeFromHttp(Internal);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Unable to get collateral sizes");
        goto Exit;
    }

    DBG_INFO("Fetched collaterals from HTTP endpoint");

    return Status;

Exit:
    if (Internal->Collaterals != NULL) {
        for (UINTN i = 0; i < Internal->NumberOfCollaterals; i++) {
            FreePool(Internal->Collaterals[i].RootUrl);
            FreePool(Internal->Collaterals[i].FilePath);
        }

        FreePool(Internal->Collaterals);
        Internal->Collaterals = NULL;
    }

    return Status;
}
#endif

static EFI_STATUS CbmrFetchCollateralsFromDcatEndpoint(_In_ PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal)
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINTN MaxSoftwareInventories = _countof(Internal->SoftwareInventories);

    // Note: For local endpoints the RootUrl is generally known beforehand (e.g. configured in
    // cbmr_config.txt), which is not the case for DCAT collaterals.
    EFI_MS_CBMR_COLLATERAL Collaterals[] = {
        // clang-format off
        {sizeof(EFI_MS_CBMR_COLLATERAL), NULL, 0, L"winre.wim", _countof(L"winre.wim"), RAMDISK_WIM_PATH, 0},
        // clang-format on
    };

    //
    // Cert-pin against metadata channel TLS CA certs.
    //

    Status = TlsSetCACertList(DcatMetadataChannelTlsCaCerts,
                              ARRAYSIZE(DcatMetadataChannelTlsCaCerts));
    if (EFI_ERROR(Status)) {
        DBG_ERROR("TlsSetCACertList() failed 0x%zx", Status);
        goto Exit;
    }

    DBG_INFO("Configured TLS certs for metadata channel");

    //
    // Loop over available SI starting from SI2
    //

    for (INTN Index = MaxSoftwareInventories - 1; Index >= 0; Index--) {
        PSOFTWARE_INVENTORY_INFO SiInfo = &Internal->SoftwareInventories[Index];
        DCAT_FILE_INFO* DcatFileInfo = NULL;
        CHAR8* AsciiStr = NULL;
        UINTN UrlLength = 0;
        DCAT_CONTEXT* DcatContext = NULL;

        if (SiInfo->Valid == FALSE) {
            continue;
        }

        Internal->NumberOfCollaterals = _countof(Collaterals);
        Internal->Collaterals = AllocateZeroPool(sizeof(EFI_MS_CBMR_COLLATERAL) *
                                                 Internal->NumberOfCollaterals);
        if (Internal->Collaterals == NULL) {
            DBG_ERROR("Unable to allocate memory for Collaterals");
            Status = EFI_OUT_OF_RESOURCES;
            goto Exit;
        }

        //
        // Retrieve JSON blob with well formed request to DCAT endpoint.
        //

        Status = DcatInit(&DcatContext);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("DcatInit() failed 0x%zx", Status);
            goto Exit;
        }

        Status = DcatRetrieveJsonBlob(DcatContext,
                                      Internal->HttpContext,
                                      gCbmrConfig.Url,
                                      SiInfo->RequestJson);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("DcatRetrieveJsonBlob() failed 0x%zx", Status);
            goto SiExit;
        }

        //
        // Extract DCAT file metadata from JSON blob
        //

        for (UINTN i = 0; i < Internal->NumberOfCollaterals; i++) {
            AsciiStr = AllocateZeroPool(Collaterals[i].RelativeUrlLength + sizeof(CHAR8));
            if (AsciiStr == NULL) {
                DBG_ERROR("Out of memory");
                Status = EFI_OUT_OF_RESOURCES;
                goto SiExit;
            }

            UnicodeStrToAsciiStr(Collaterals[i].RelativeUrl, AsciiStr);
            Status = DcatExtractFileInfoFromJson(DcatContext,
                                                 AsciiStr,
                                                 Collaterals[i].RelativeUrlLength,
                                                 &DcatFileInfo);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("DcatRetrieveJsonBlob() failed 0x%zx", Status);
                goto SiExit;
            }

            FreePool(AsciiStr);
            AsciiStr = NULL;

            //
            // Assign URL and file size info to Internal collaterals for use during
            // download phase.
            //

            Status = DcatExtractSizeFromFileInfo(DcatFileInfo,
                                                 &Internal->Collaterals[i].CollateralSize);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("DcatExtractSizeFromFileInfo() failed 0x%zx", Status);
                goto SiExit;
            }

            Status = DcatExtractDigestFromFileInfo(DcatFileInfo, Internal->Collaterals[i].Digest);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("DcatExtractDigestFromFileInfo() failed 0x%zx", Status);
                goto SiExit;
            }

            Status = DcatExtractUrlFromFileInfo(DcatFileInfo, &AsciiStr, &UrlLength);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("DcatExtractUrlFromFileInfo() failed 0x%zx", Status);
                goto SiExit;
            }

            DcatFileInfoFree(DcatFileInfo);
            DcatFileInfo = NULL;

            Internal->Collaterals[i].RootUrl = AllocateZeroPool(UrlLength * sizeof(CHAR16) +
                                                                sizeof(CHAR16));
            if (Internal->Collaterals[i].RootUrl == NULL) {
                DBG_ERROR("Out of memory");
                Status = EFI_OUT_OF_RESOURCES;
                goto SiExit;
            }

            Internal->Collaterals[i].RootUrlLength = UrlLength;

            AsciiStrToUnicodeStr(AsciiStr, Internal->Collaterals[i].RootUrl);

            Status = StrDup(Collaterals[i].FilePath, &Internal->Collaterals[i].FilePath);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("StrDup() failed");
                goto SiExit;
            }

            FreePool(AsciiStr);
            AsciiStr = NULL;
        }

    SiExit:
        FreePool(AsciiStr);
        DcatFileInfoFree(DcatFileInfo);

        if (EFI_ERROR(Status)) {
            if (Internal->Collaterals != NULL) {
                for (UINTN i = 0; i < Internal->NumberOfCollaterals; i++) {
                    FreePool(Internal->Collaterals[i].RootUrl);
                    FreePool(Internal->Collaterals[i].FilePath);
                }

                FreePool(Internal->Collaterals);
                Internal->Collaterals = NULL;
            }

            SiInfo->Valid = FALSE;
            DBG_ERROR("Failed to fetch collaterals from DCAT with Software Inventory %zd",
                      Index + 1);
        } else {
            SiInfo->Valid = TRUE;
            DBG_INFO("Fetched collaterals from DCAT with Software Inventory %zd", Index + 1);
            break;
        }

        //
        // DCAT context no longer needed.
        //

        DcatFree(DcatContext);
        DcatContext = NULL;
    }

Exit:

    //
    // Delete stale/malformed SI deposited to RamDisk
    //

    for (INTN Index = MaxSoftwareInventories - 1; Index >= 0; Index--) {
        PSOFTWARE_INVENTORY_INFO SiInfo = &Internal->SoftwareInventories[Index];
        EFI_STATUS Status2 = EFI_SUCCESS;
        EFI_FILE_PROTOCOL* File = NULL;

        if (SiInfo->Valid == TRUE)
            continue;

        Status2 = FileOpen(STUBOS_VOLUME_LABEL,
                           SiInfo->RamdiskFilePath,
                           EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
                           0,
                           &File);
        if (!EFI_ERROR(Status2)) {
            DBG_INFO("Found stale SI %zd, attempting to delete it.", Index + 1);
            Status2 = FileDelete(File);
            if (EFI_ERROR(Status2)) {
                DBG_WARNING("FileDelete failed 0x%zx", Status2);
            }
        }
    }

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_DRIVER_DCAT_COLLATERAL_FETCH_FAILED);
    }

    return Status;
}

static EFI_STATUS CbmrFetchCollaterals(_In_ PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal)
{
    switch (gCbmrConfig.EndpointType) {
        case CBMR_ENDPOINT_TYPE_DCAT:
            return CbmrFetchCollateralsFromDcatEndpoint(Internal);
#ifdef DEBUGMODE
        case CBMR_ENDPOINT_TYPE_LOCAL_HTTP:
            return CbmrFetchCollateralsFromHttpEndpoint(Internal);
        case CBMR_ENDPOINT_TYPE_USBKEY:
            return CbmrFetchCollateralsFromUSBKey(Internal);
#endif
    }

    return EFI_INVALID_PARAMETER;
}

static EFI_STATUS CbmrConfigureRamdisk(_In_ PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal)
{
    EFI_STATUS Status = EFI_SUCCESS;

    Internal->RamdiskSize = 1 * 1024 * 1024 * 1024; // 1GB
    Status = RamdiskInit(Internal->RamdiskSize, 512, &Internal->RamdiskContext);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("RamdiskInit() failed 0x%zx", Status);
        goto Exit;
    }

    Status = RamdiskInitializeSingleFat32Volume(Internal->RamdiskContext);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("RamdiskInitializeSingleFat32Volume() failed 0x%zx", Status);
        goto Exit;
    }

    Status = RamdiskRegister(Internal->RamdiskContext);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("RamdiskRegister() failed 0x%zx", Status);
        goto Exit;
    }

    DBG_INFO("Configured Ramdisk");
    return Status;
Exit:
    CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_DRIVER_RAMDISK_CONFIGURATION_FAILED);
    return Status;
}

static EFI_STATUS
CbmrDepositSoftwareInventoryToRamdisk(_In_ PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal)
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINTN MaxSoftwareInventories = _countof(Internal->SoftwareInventories);

    //
    // Try to copy si2.wim first ramdisk and if it do not exist ignore the error
    // and continue to copy si.wim(should exist) to ramdisk
    //

    for (INTN Index = MaxSoftwareInventories - 1; Index >= 0; Index--) {
        UINTN SoftwareInventorySize = 0;
        VOID* SoftwareInventory = NULL;
        EFI_FILE_PROTOCOL* WimFile = NULL;
        PSOFTWARE_INVENTORY_INFO SiInfo = &Internal->SoftwareInventories[Index];

        Status = gRT->GetVariable(SiInfo->UefiVariableName,
                                  &(EFI_GUID)EFI_MS_CBMR_VARIABLES_INTERNAL_GUID,
                                  NULL,
                                  &SoftwareInventorySize,
                                  NULL);
        if (Status == EFI_NOT_FOUND) {
            DBG_ERROR_U(L"GetVariable() failed. Unabled to locate %s variable",
                        SiInfo->UefiVariableName);
            goto Exit;
        }

        if (EFI_ERROR(Status) && Status != EFI_BUFFER_TOO_SMALL) {
            goto Exit;
        }

        SoftwareInventory = AllocateZeroPool(SoftwareInventorySize);
        if (SoftwareInventory == NULL) {
            DBG_ERROR("AllocateZeroPool() failed to allocate buffer of size %zu",
                      SoftwareInventorySize);
            Status = EFI_OUT_OF_RESOURCES;
            goto Exit;
        }

        Status = gRT->GetVariable(SiInfo->UefiVariableName,
                                  &(EFI_GUID)EFI_MS_CBMR_VARIABLES_INTERNAL_GUID,
                                  NULL,
                                  &SoftwareInventorySize,
                                  SoftwareInventory);
        if (EFI_ERROR(Status)) {
            goto Exit;
        }

        //
        // Save the in memory SI.WIM blob as STUBOS\cbmr\si.wim to make wim.c happy
        // for processing it later
        //

        Status = FileCreateSubdirectoriesAndFile(STUBOS_VOLUME_LABEL,
                                                 SiInfo->RamdiskFilePath,
                                                 &WimFile);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileCreateSubdirectoriesAndFile() failed with status 0x%zx", Status);
            goto Exit;
        }

        Status = FileWrite(WimFile, &SoftwareInventorySize, SoftwareInventory);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileWrite() failed 0x%zx", Status);
            Status = EFI_NOT_READY;
            goto Exit;
        }

    Exit:
        if (EFI_ERROR(Status)) {
            SiInfo->Valid = FALSE;
            DBG_ERROR("Failed to deposit Software Inventory %zd", Index + 1);
        } else {
            SiInfo->Valid = TRUE;
            DBG_INFO("Deposited Software Inventory %zd", Index + 1);
        }

        FileClose(WimFile);
        FreePool(SoftwareInventory);
    }

    //
    // The status reflects the status for si.wim. As any failures related to
    // si2.wim are not fatal
    //

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_DRIVER_SOFTWARE_INVENTORY_DEPOSITION_FAILED);
    }

    return Status;
}

static EFI_STATUS CbmrProcessSoftwareInventory(_In_ PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal)
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINTN MaxSoftwareInventories = _countof(Internal->SoftwareInventories);

    //
    // Process each software inventory
    //

    for (UINTN Index = 0; Index < MaxSoftwareInventories; Index++) {
        EFI_FILE_PROTOCOL* WimFile = NULL;
        WIM_CONTEXT* WimContext = NULL;
        XmlNode2* CbmrNode = NULL;
        XmlNode2* VersionNode = NULL;
        XmlNode2* TempNode = NULL;
        CHAR8* Architecture = NULL;
        CHAR8* MajorVersion = NULL;
        CHAR8* MinorVersion = NULL;
        CHAR8* Build = NULL;
        CHAR8* Revision = NULL;
        CHAR8* Edition = NULL;
        CHAR8* Branch = NULL;
        CHAR8* VersionFormatString = t("%s.%s.%s.%s");
        UINTN Result = 0;
        CHAR8 FullVersion[64] = {0};
        PSOFTWARE_INVENTORY_INFO SiInfo = &Internal->SoftwareInventories[Index];

        if (SiInfo->Valid == FALSE) {
            continue;
        }

        Status = FileOpen(STUBOS_VOLUME_LABEL,
                          SiInfo->RamdiskFilePath,
                          EFI_FILE_MODE_READ,
                          0,
                          &WimFile);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileOpen() failed with status 0x%zx", Status);
            goto Exit;
        }

        Status = WimInit(WimFile, &WimContext);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("WimInit() failed with status 0x%zx", Status);
            goto Exit;
        }

        //
        // WimContext has ownership of WimFile, so don't use it anymore.
        //

        WimFile = NULL;

        Status = WimExtractCbmrNode(WimContext, &CbmrNode);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("WimExtractCbmrNode() failed with status 0x%zx", Status);
            goto Exit;
        }

        //
        // Extract CBMR info to construct JSON request
        //

        VersionNode = FindFirstChildNodeByName(CbmrNode, t("VERSION"));
        if (VersionNode == NULL) {
            DBG_ERROR("<VERSION> node not found, invalid XML");
            Status = EFI_INVALID_PARAMETER;
            goto Exit;
        }

        TempNode = FindFirstChildNodeByName(VersionNode, t("ARCHITECTURE"));
        if (TempNode == NULL) {
            DBG_ERROR("<ARCHITECTURE> node not found, invalid XML");
            Status = EFI_INVALID_PARAMETER;
            goto Exit;
        }

        Architecture = TempNode->Value;

        TempNode = FindFirstChildNodeByName(VersionNode, t("MAJOR"));
        if (TempNode == NULL) {
            DBG_ERROR("<MAJOR> node not found, invalid XML");
            Status = EFI_INVALID_PARAMETER;
            goto Exit;
        }

        MajorVersion = TempNode->Value;

        TempNode = FindFirstChildNodeByName(VersionNode, t("MINOR"));
        if (TempNode == NULL) {
            DBG_ERROR("<MINOR> node not found, invalid XML");
            Status = EFI_INVALID_PARAMETER;
            goto Exit;
        }

        MinorVersion = TempNode->Value;

        TempNode = FindFirstChildNodeByName(VersionNode, t("BUILD"));
        if (TempNode == NULL) {
            DBG_ERROR("<BUILD> node not found, invalid XML");
            Status = EFI_INVALID_PARAMETER;
            goto Exit;
        }

        Build = TempNode->Value;

        TempNode = FindFirstChildNodeByName(VersionNode, t("REVISION"));
        if (TempNode == NULL) {
            DBG_ERROR("<REVISION> node not found, invalid XML");
            Status = EFI_INVALID_PARAMETER;
            goto Exit;
        }

        Revision = TempNode->Value;

        TempNode = FindFirstChildNodeByName(VersionNode, t("EDITION"));
        if (TempNode == NULL) {
            DBG_ERROR("<EDITION> node not found, invalid XML");
            Status = EFI_INVALID_PARAMETER;
            goto Exit;
        }

        Edition = TempNode->Value;

        TempNode = FindFirstChildNodeByName(VersionNode, t("BRANCH"));
        if (TempNode == NULL) {
            DBG_ERROR("<BRANCH> node not found, invalid XML");
            Status = EFI_INVALID_PARAMETER;
            goto Exit;
        }

        Branch = TempNode->Value;

        //
        // Construct 4-part version string
        //

        Result = StringCchPrintfA((STRSAFE_LPSTR)FullVersion,
                                  _countof(FullVersion),
                                  (STRSAFE_LPCSTR)VersionFormatString,
                                  MajorVersion,
                                  MinorVersion,
                                  Build,
                                  Revision);
        if (FAILED(Result)) {
            DBG_ERROR("StringCchPrintfA failed 0x%zx", Result);
            Status = EFI_INVALID_PARAMETER;
            goto Exit;
        }

        //
        // Construct JSON request
        //

        Result = StringCchPrintfA((STRSAFE_LPSTR)SiInfo->RequestJson,
                                  MAX_JSON_REQUEST_SIZE,
                                  (STRSAFE_LPCSTR)DCAT_REQUEST_JSON_FORMAT_STRING,
                                  FullVersion,
                                  FullVersion,
                                  Branch,
                                  Edition,
                                  gCbmrConfig.DcatEndpointType == CBMR_DCAT_ENDPOINT_TYPE_PPE);
        if (FAILED(Result)) {
            DBG_ERROR("StringCchPrintfA failed 0x%zx", Result);
            Status = EFI_INVALID_PARAMETER;
            goto Exit;
        }

    Exit:
        if (EFI_ERROR(Status)) {
            SiInfo->Valid = FALSE;
            DBG_ERROR("Processing of Software Inventory %zu failed", Index + 1);
        } else {
            SiInfo->Valid = TRUE;
            DBG_INFO("Processing of Software Inventory %zu succeeded", Index + 1);
        }

        FileClose(WimFile);
        WimFree(WimContext);

        if (SiInfo->InventoryType == SOFTWARE_INVENTORY_SECONDARY) {
            Status = EFI_SUCCESS; // Do not consider errors while processing si2.wim as fatal
        }
    }

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_DRIVER_SOFTWARE_INVENTORY_PROCESSING_FAILED);
    }

    return Status;
}

static EFI_STATUS CbmrDepositDcatInfoToRamdisk(_In_ PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal)
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINTN Result = 0;
    EFI_FILE_PROTOCOL* File = NULL;
    CHAR8 DcatInfoContent[1024] = {0};
    UINTN DcatInfoContentSize = 0;

    Status = FileCreateSubdirectoriesAndFile(STUBOS_VOLUME_LABEL, RAMDISK_DCAT_INFO_PATH, &File);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileCreateSubdirectoriesAndFile() failed with status 0x%zx", Status);
        goto Exit;
    }

    UINTN MaxSoftwareInventories = _countof(Internal->SoftwareInventories);

    //
    // Loop over available SI starting from SI2. Only deposit the SI that worked
    // for UEFI
    //

    for (INTN Index = MaxSoftwareInventories - 1; Index >= 0; Index--) {
        PSOFTWARE_INVENTORY_INFO SiInfo = &Internal->SoftwareInventories[Index];

        if (SiInfo->Valid == FALSE)
            continue;

        Result = StringCchPrintfA((STRSAFE_LPSTR)DcatInfoContent,
                                  _countof(DcatInfoContent),
                                  (STRSAFE_LPCSTR) "%s\n%s\n", // Padding with NULL to help Reset
                                                               // engine parse the file
                                  gCbmrConfig.DcatEndpointType == CBMR_DCAT_ENDPOINT_TYPE_PROD ?
                                      CBMR_DCAT_ENDPOINT_TYPE_PROD_STR :
                                      CBMR_DCAT_ENDPOINT_TYPE_PPE_STR,
                                  SiInfo->RequestJson);
        if (FAILED(Result)) {
            DBG_ERROR("StringCchPrintfA failed 0x%zx", Result);
            Status = EFI_INVALID_PARAMETER;
            goto Exit;
        }

        DcatInfoContentSize = AsciiStrnLenS(DcatInfoContent, _countof(DcatInfoContent));

        Status = FileWrite(File, &DcatInfoContentSize, DcatInfoContent);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileWrite() failed 0x%zx", Status);
            Status = EFI_NOT_READY;
            goto Exit;
        }

        DBG_INFO("Deposited DCAT Request info from SI %zd", Index + 1);
        break;
    }
Exit:

    FileClose(File);

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_DRIVER_DCAT_INFO_DEPOSITION_FAILED);
    }

    return Status;
}

static EFI_STATUS
CbmrDownloadOsDriversToRamdiskFromDcat(_In_ PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal)
{
    EFI_STATUS Status = EFI_SUCCESS;

    UNREFERENCED_PARAMETER(Internal);

#if 0 // Enable this block once driver downloading from DCAT via UMS is figured out
    UINTN Result = 0;
    CHAR16* FileName = NULL;
    CHAR16* FullUrl = NULL;
    INT FileNameOffset = 0;
    CHAR16 DestinationFilePath[1024] = {0};
    EFI_FILE_PROTOCOL* File = NULL;
    CHAR8 AsciiUrl[1024] = {0};
    EFI_HTTP_HEADER* HttpHeaders = NULL;
    UINTN HeaderCount = 0;
    HTTP_RESPONSE* Response = NULL;

    //
    // Check if there are any drivers to download. If not, just return early.
    //

    if (gCbmrConfig.OsDriverUrls.UrlCount == 0) {
        DBG_INFO("No drivers to download.");
        return EFI_SUCCESS;
    }

    for (UINT32 i = 0; i < gCbmrConfig.OsDriverUrls.UrlCount; i++) {
        //
        // Construct destination file path
        //

        FullUrl = gCbmrConfig.OsDriverUrls.Urls[i];
        FileNameOffset = StrLastIndexOf(gCbmrConfig.OsDriverUrls.Urls[i], L'/');
        FileName = &FullUrl[FileNameOffset + 1];

        Result = StringCchPrintfW(DestinationFilePath,
                                  _countof(DestinationFilePath),
                                  (CHAR16*)L"%s\\%s",
                                  RAMDISK_CBMR_DRIVERS_DIRECTORY,
                                  FileName);
        if (FAILED(Result)) {
            DBG_ERROR("StringCchPrintfW failed 0x%zx", Result);
            Status = EFI_INVALID_PARAMETER;
            goto Exit;
        }

        //
        // Create target file
        //

        Status = FileCreateSubdirectoriesAndFile(STUBOS_VOLUME_LABEL, DestinationFilePath, &File);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileCreateSubdirectoriesAndFile() failed with status 0x%zx", Status);
            goto Exit;
        }

        DBG_INFO_U(L"Downloading %s", FullUrl);

        UnicodeStrToAsciiStr(FullUrl, AsciiUrl);

        Status = CbmrBuildRequestHeaders(AsciiUrl, AsciiStrnLenS(AsciiUrl,_countof(AsciiUrl)), &HttpHeaders, &HeaderCount);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("CbmrBuildRequestHeaders() failed 0x%zx", Status);
            goto Exit;
        }

        Status = HttpIssueRequest(Internal->HttpContext,
                                  FullUrl,
                                  HttpMethodGet,
                                  HttpHeaders,
                                  HeaderCount,
                                  NULL,
                                  0,
                                  0,
                                  &Response);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("HttpIssueRequest() failed 0x%zx", Status);
            goto Exit;
        }

        HttpFreeHeaderFields(HttpHeaders, HeaderCount);

        HttpHeaders = NULL;
        HeaderCount = 0;

        do {
            UINTN ChunkSize = HttpGetChunkSize(Response);
            UINT8* Chunk = HttpGetChunk(Response);

            Status = FileWrite(File, &ChunkSize, Chunk);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("FileWrite() failed 0x%zx", Status);
                Status = EFI_NOT_READY;
                goto Exit;
            }

            Status = HttpGetNext(Internal->HttpContext, Response);
            if (EFI_ERROR(Status) && Status != EFI_END_OF_FILE) {
                DBG_ERROR("HttpGetNext() failed 0x%zx", Status);
                goto Exit;
            }
        } while (Status != EFI_END_OF_FILE);

        Status = EFI_SUCCESS;

        DBG_INFO_U(L"Downloaded %s", FullUrl);

        HttpFreeResponse(Internal->HttpContext, Response);
        Response = NULL;
    }

    DBG_INFO("Downloaded OS drivers");

Exit:

    FileClose(File);
    HttpFreeResponse(Internal->HttpContext, Response);
    HttpFreeHeaderFields(HttpHeaders, HeaderCount);

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_DRIVER_OS_DRIVER_DOWNLOAD_FAILED);
    }
#endif
    return Status;
}

#ifdef DEBUGMODE
static EFI_STATUS
CbmrDownloadCbmrDriverToRamdiskFromUSBKey(_In_ PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* Source = NULL;
    EFI_FILE_PROTOCOL* Dest = NULL;

    UNREFERENCED_PARAMETER(Internal);

    Status = FileLocateAndOpen(L"\\usbkey\\cbmr_driver.efi", EFI_FILE_MODE_READ, &Source);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileLocateAndOpen() failed. Unable to locate \\usbkey\\cbmr_driver.efi 0x%zx",
                  Status);
        goto Exit;
    }

    Status = FileCreateSubdirectoriesAndFile(STUBOS_VOLUME_LABEL, RAMDISK_CBMR_DRIVER_PATH, &Dest);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileCreateSubdirectoriesAndFile() failed for %S with status 0x%zx",
                  RAMDISK_CBMR_DRIVER_PATH,
                  Status);
        goto Exit;
    }

    Status = FileCopy(Source, Dest);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileCopy() failed 0x%zx", Status);
        goto Exit;
    }

Exit:

    if (Source != NULL) {
        Source->Close(Source);
    }

    if (Dest != NULL) {
        Dest->Close(Dest);
    }

    return Status;
}
#endif

static EFI_STATUS CbmrDepositWiFiProfileToRamdisk(_In_ PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal,
                                                  _In_ PEFI_MS_CBMR_CONFIG_DATA CbmrConfigData)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* File = NULL;
    PEFI_MS_CBMR_WIFI_NETWORK_PROFILE WiFiProfile = &CbmrConfigData->WifiProfile;
    CHAR8 WiFiProfileContent[256] = {0};
    UINTN WiFiProfileContentSize = 0;
    HRESULT Hr = S_OK;

    UNREFERENCED_PARAMETER(Internal);

    if (WiFiProfile->SSIdLength == 0 || WiFiProfile->PasswordLength == 0) {
        DBG_INFO("No Wifi profile available");
        goto Exit;
    }

    Status = FileCreateSubdirectoriesAndFile(STUBOS_VOLUME_LABEL, RAMDISK_WIFI_PROFILE_PATH, &File);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileCreateSubdirectoriesAndFile() failed with status 0x%zx", Status);
        goto Exit;
    }

    WiFiProfile->SSId[WiFiProfile->SSIdLength] = '\0';
    WiFiProfile->Password[WiFiProfile->PasswordLength] = '\0';

    Hr = StringCchPrintfA((STRSAFE_LPSTR)WiFiProfileContent,
                          _countof(WiFiProfileContent),
                          (STRSAFE_LPCSTR) "%s\n%s\n",
                          WiFiProfile->SSId,
                          WiFiProfile->Password);
    if (FAILED(Hr)) {
        Status = EFI_INVALID_PARAMETER;
        DBG_ERROR("StringCchPrintfW failed");
        goto Exit;
    }

    WiFiProfileContentSize = AsciiStrnLenS(WiFiProfileContent, _countof(WiFiProfileContent));
    Status = FileWrite(File, &WiFiProfileContentSize, WiFiProfileContent);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileWrite() failed 0x%zx", Status);
        Status = EFI_NOT_READY;
        goto Exit;
    }

    DBG_INFO("Deposited Wi-Fi Profile");

Exit:

    //
    // We should not keep SSId and Password in memory here after
    //

    ZeroMem(WiFiProfile->SSId, sizeof(WiFiProfile->SSId));
    ZeroMem(WiFiProfile->Password, sizeof(WiFiProfile->Password));
    ZeroMem(WiFiProfileContent, sizeof(WiFiProfileContent));

    if (File != NULL) {
        FileClose(File);
    }

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_DRIVER_WIFI_DEPOSITION_FAILED);
    }

    return Status;
}

static EFI_STATUS CbmrServiceDriver(_In_ PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal)
{
    EFI_STATUS Status = EFI_SUCCESS;
    CBMR_SERVICING_INFO ServicingInfo = {0};
    EFI_FILE_PROTOCOL* File = NULL;
    UINT64 FileSize = 0;
    EFI_HANDLE LoadedDriverHandle = NULL;
    EFI_GUID MsCbmrVariablesInternalGuid = EFI_MS_CBMR_VARIABLES_INTERNAL_GUID;

#ifdef DEBUGMODE
    if (gCbmrConfig.ServiceViaLocalCbmrDriver) {
        DBG_INFO("Using CBMR driver from usbkey (overrides previous CBMR driver) ");
        CbmrDownloadCbmrDriverToRamdiskFromUSBKey(Internal);
    }
#endif

    //
    // Grab driver from memory, if available.
    //

    if (Internal->CbmrDriver == NULL) {
        //
        // Try checking in the ramdisk. The driver should have been extracted there.
        //

        Status = FileOpen(STUBOS_VOLUME_LABEL,
                          RAMDISK_CBMR_DRIVER_PATH,
                          EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
                          0,
                          &File);
        if (EFI_ERROR(Status)) {
            DBG_INFO("No downloaded CBMR driver found 0x%zx.", Status);
            Status = EFI_NOT_FOUND;
            goto Exit;
        }

        DBG_INFO("Found downloaded CBMR driver. Attempting to load it.");

        //
        // Get driver size and allocate memory for it
        //

        Status = FileGetSize(File, &FileSize);
        if (EFI_ERROR(Status)) {
            DBG_INFO("FileGetSize() failed 0x%zx.", Status);
            goto Exit;
        }

        Internal->CbmrDriverSize = (UINTN)FileSize;
        Internal->CbmrDriver = AllocateZeroPool(Internal->CbmrDriverSize);
        if (Internal->CbmrDriver == NULL) {
            DBG_ERROR("Out of resources");
            Status = EFI_OUT_OF_RESOURCES;
            goto Exit;
        }

        Status = FileRead(File, &Internal->CbmrDriverSize, Internal->CbmrDriver);
        if (EFI_ERROR(Status)) {
            DBG_INFO("FileRead() failed 0x%zx.", Status);
            goto Exit;
        }

        Status = FileDelete(File);
        if (EFI_ERROR(Status)) {
            DBG_INFO("FileDelete() failed 0x%zx.", Status);
            goto Exit;
        }

        File = NULL;
    }

    //
    // Store driver versioning info and other data (like PEFI_MS_CBMR_PROTOCOL_INTERNAL
    // pointer) into MsCbmrServicingInfo variable.
    //

    ServicingInfo.ServicingInitiated = TRUE;
    ServicingInfo.PriorVersion.Major = CBMR_MAJOR_VERSION;
    ServicingInfo.PriorVersion.Minor = CBMR_MINOR_VERSION;
    ServicingInfo.Internal = Internal;

    Status = gRT->SetVariable(EFI_MS_CBMR_SERVICING_INFO_VARIABLE,
                              &MsCbmrVariablesInternalGuid,
                              EFI_VARIABLE_BOOTSERVICE_ACCESS,
                              sizeof(CBMR_SERVICING_INFO),
                              &ServicingInfo);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Unable to set servicing info variable. 0x%zx", Status);
        goto Exit;
    }

    Status = gBS->LoadImage(FALSE,
                            gImageHandle,
                            NULL,
                            Internal->CbmrDriver,
                            Internal->CbmrDriverSize,
                            &LoadedDriverHandle);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("LoadImage() failed. 0x%zx", Status);
        goto Exit;
    }

    //
    // LoadImage performs copy of the driver, so delete the original.
    //

    FreePool(Internal->CbmrDriver);
    Internal->CbmrDriver = NULL;
    Internal->CbmrDriverSize = 0;

    Status = gBS->StartImage(LoadedDriverHandle, NULL, NULL);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("StartImage() failed. 0x%zx", Status);
        goto Exit;
    }

Exit:

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_DRIVER_SERVICEING_FAILED);
    }

    if (File != NULL) {
        FileDelete(File);
    }

    FreePool(Internal->CbmrDriver);
    Internal->CbmrDriver = NULL;
    Internal->CbmrDriverSize = 0;

    return Status;
}

static EFI_STATUS CbmrStartStubOsRambooting(_In_ PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_MS_CBMR_PROGRESS_CALLBACK ProgressCallback = Internal->ProgressCallBack;
    PEFI_MS_CBMR_PROGRESS Progress = &Internal->Progress;
    // PEFI_MS_CBMR_RAMBOOT_STUBOS_PROGRESS RambootProgress = NULL;

    //
    // Rambooting stubos phase
    //

    Progress->CurrentPhase = MsCbmrPhaseStubOsRamboot;
    // RambootProgress = &Progress->RambootProgress;

    //
    // Invoke the application/caller
    //

    if (ProgressCallback != NULL) {
        Status = ProgressCallback((PEFI_MS_CBMR_PROTOCOL)Internal, Progress);
        if (EFI_ERROR(Status)) {
            //
            // Terminate the download process if the caller asked us not
            // to proceed any further
            //
            Status = EFI_SUCCESS;
            goto Exit;
        }
    }

    Status = RamdiskBoot(Internal->RamdiskContext);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("RamdiskBoot() failed x0%zx", Status);
        goto Exit;
    }

Exit:

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_DRIVER_RAMBOOTING_FAILED);
    }

    return Status;
}

#ifdef DEBUGMODE
static EFI_STATUS
CbmrStartCollateralDownloadFromUSBKey(_In_ PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal)
{
    EFI_STATUS Status = EFI_SUCCESS;
    PEFI_MS_CBMR_COLLATERALS_DOWNLOAD_PROGRESS DownloadProgress = NULL;
    EFI_MS_CBMR_PROGRESS_CALLBACK ProgressCallback = Internal->ProgressCallBack;
    PEFI_MS_CBMR_PROGRESS Progress = &Internal->Progress;

    //
    // Collateral download phase
    //

    Progress->CurrentPhase = MsCbmrPhaseCollateralsDownloading;
    DownloadProgress = &Progress->ProgressData.DownloadProgress;

    for (UINTN i = 0; i < Internal->NumberOfCollaterals; i++) {
        EFI_FILE_PROTOCOL* File = NULL;
        EFI_FILE_PROTOCOL* RamdiskFile = NULL;

        DownloadProgress->CollateralIndex = i;

        Status = FileLocateAndOpen(Internal->Collaterals[i].RelativeUrl, EFI_FILE_MODE_READ, &File);
        if (EFI_ERROR(Status)) {
            DBG_ERROR_U(L"FileLocateAndOpen() Failed 0x%zx %s",
                        Status,
                        Internal->Collaterals[i].RelativeUrl);
            goto CloseFile;
        }

        Status = FileDuplicate(File,
                               0,
                               0,
                               STUBOS_VOLUME_LABEL,
                               Internal->Collaterals[i].FilePath,
                               &RamdiskFile);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileDuplicate() failed with status 0x%zx", Status);
            goto CloseFile;
        }

        DownloadProgress->CollateralDownloadedSize = Internal->Collaterals[i].CollateralSize;

        //
        // Invoke the application/caller
        //

        if (ProgressCallback != NULL) {
            Status = ProgressCallback((PEFI_MS_CBMR_PROTOCOL)Internal, Progress);
            if (EFI_ERROR(Status)) {
                //
                // Terminate the download process if the caller asked us not
                // to proceed any further
                //

                DBG_ERROR(
                    "Aborting CBMR collateral download phase as caller callback returned 0x%zx",
                    Status);
                goto Exit;
            }
        }

    CloseFile:
        FileClose(File);
        FileClose(RamdiskFile);
    }

    //
    // Give application a chance to render its UI to show that the collateral
    // download has completed
    //

    Progress->CurrentPhase = MsCbmrPhaseCollateralsDownloaded;
    if (ProgressCallback != NULL) {
        Status = ProgressCallback((PEFI_MS_CBMR_PROTOCOL)Internal, Progress);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("Aborting CBMR collateral download phase as caller callback returned 0x%zx",
                      Status);
            goto Exit;
        }
    }

Exit:

    return Status;
}
#endif

#ifdef DEBUGMODE
static EFI_STATUS
CbmrStartCollateralDownloadFromHttpEndpoint(_In_ PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal)
{
    EFI_STATUS Status = EFI_SUCCESS;
    PEFI_MS_CBMR_COLLATERALS_DOWNLOAD_PROGRESS DownloadProgress = NULL;
    EFI_FILE_PROTOCOL* File = NULL;
    CHAR8 AsciiUrl[4096] = {0};
    EFI_HTTP_HEADER* HttpHeaders = NULL;
    UINTN HeaderCount = 0;
    HTTP_RESPONSE* Response = NULL;
    EFI_MS_CBMR_PROGRESS_CALLBACK ProgressCallback = Internal->ProgressCallBack;
    PEFI_MS_CBMR_PROGRESS Progress = &Internal->Progress;

    //
    // Check for local TLS certs and set them if found.
    //

    Status = TlsSetCACertListDebug();
    if (EFI_ERROR(Status)) {
        DBG_ERROR("TlsSetCACertListDebug() failed 0x%zx", Status);
        goto Exit;
    }

    //
    // Collateral download phase
    //

    Progress->CurrentPhase = MsCbmrPhaseCollateralsDownloading;
    DownloadProgress = &Progress->ProgressData.DownloadProgress;

    for (UINTN i = 0; i < Internal->NumberOfCollaterals; i++) {
        UINTN Position = 0;

        DownloadProgress->CollateralIndex = i;
        DBG_INFO_U(L"Downloading %s", Internal->Collaterals[i].RootUrl);

        UnicodeStrToAsciiStr(Internal->Collaterals[i].RootUrl, AsciiUrl);

        Status = CbmrBuildRequestHeaders(AsciiUrl,
                                         AsciiStrnLenS(AsciiUrl, _countof(AsciiUrl)),
                                         &HttpHeaders,
                                         &HeaderCount);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("CbmrBuildRequestHeaders() failed 0x%zx", Status);
            goto Exit;
        }

        Status = HttpIssueRequest(Internal->HttpContext,
                                  Internal->Collaterals[i].RootUrl,
                                  Internal->Collaterals[i].RootUrlLength,
                                  HttpMethodGet,
                                  HttpHeaders,
                                  HeaderCount,
                                  NULL,
                                  0,
                                  0,
                                  &Response);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("HttpIssueRequest() failed 0x%zx", Status);
            goto Exit;
        }

        HttpFreeHeaderFields(HttpHeaders, HeaderCount);

        HttpHeaders = NULL;
        HeaderCount = 0;

        if (Internal->Collaterals[i].StoreInMemory == TRUE) {
            Internal->Collaterals[i].MemoryLocation = AllocatePool(
                Internal->Collaterals[i].CollateralSize);
            if (Internal->Collaterals[i].MemoryLocation == NULL) {
                DBG_ERROR("Out of memory");
                Status = EFI_OUT_OF_RESOURCES;
                goto Exit;
            }

            //
            // Check if this is the CBMR driver. If so, store it in easy-to-access
            // location.
            //

            if (StrStr(Internal->Collaterals[i].RootUrl, CBMR_DRIVER_FILE_NAME) != NULL) {
                Internal->CbmrDriver = Internal->Collaterals[i].MemoryLocation;
                Internal->CbmrDriverSize = Internal->Collaterals[i].CollateralSize;
            }
        } else {
            //
            // Create and open file just once to avoid file open overhead.
            //

            Status = FileCreateSubdirectoriesAndFile(STUBOS_VOLUME_LABEL,
                                                     Internal->Collaterals[i].FilePath,
                                                     &File);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("FileCreateSubdirectoriesAndFile() failed with status 0x%zx", Status);
                goto Exit;
            }
        }

        UINT64 TotalTickCount = 0;
        UINT64 ProgressCallbackTickCount = 0;
        UINT64 Begin = 0;
        UINT64 End = 0;
        UINTN OnePercentOfCollateralSize = Internal->Collaterals[i].CollateralSize / 100;
        UINTN NextProgressUpdate = OnePercentOfCollateralSize;

        do {
            UINTN ChunkSize = HttpGetChunkSize(Response);
            UINT8* Chunk = HttpGetChunk(Response);

            //
            // This checks for potential out-of-bounds writes against the initial CollateralSize
            // (or heap overflow for memory buffer or integer overflow). There are also checks in
            // HttpGetResponse/HttpGetNext that validate against the expected content length,
            // but the more checks the better.
            //

            if (Position + ChunkSize > Internal->Collaterals[i].CollateralSize) {
                DBG_ERROR("Position (%zu) + ChunkSize (%zu) exceeds Collateral size (%zu)",
                          Position,
                          ChunkSize,
                          Internal->Collaterals[i].CollateralSize);
                Status = EFI_ABORTED;
                goto Exit;
            }

            if (Position > Position + ChunkSize) {
                DBG_ERROR("Integer overflow, Position (%zu) + ChunkSize (%zu)",
                          Position,
                          ChunkSize);
                Status = EFI_ABORTED;
                goto Exit;
            }

            Begin = GetTickCount();
            if (Internal->Collaterals[i].StoreInMemory == TRUE) {
                Status = CopyMemS(Internal->Collaterals[i].MemoryLocation + Position,
                                  Internal->Collaterals[i].CollateralSize - Position,
                                  Chunk,
                                  ChunkSize);
                if (EFI_ERROR(Status)) {
                    DBG_ERROR("CopyMemS() failed 0x%zx", Status);
                    goto Exit;
                }
            } else {
                Status = FileWrite(File, &ChunkSize, Chunk);
                if (EFI_ERROR(Status)) {
                    DBG_ERROR("FileWrite() failed 0x%zx", Status);
                    Status = EFI_NOT_READY;
                    goto Exit;
                }
            }
            End = GetTickCount();

            TotalTickCount += End - Begin;
            Position += ChunkSize;

            DownloadProgress->CollateralDownloadedSize = Position;
            // DownloadProgress->CollateralCurrentChunkSize = ChunkSize;
            // DownloadProgress->CollateralCurrentChunkData = Chunk;

            //
            // Invoke the application/caller
            //

            //
            // Note: Below CollateralDownloadedSize check is used to throttle calls to
            // ProgressCallback, as calling it too often can have terrible perf
            // impact (mostly due to updating UI). Don't remove the check unless
            // you know what you're doing. Proof: When testing against VM, by
            // adding the simple check below, the download speed shot up from
            // 1.2 MB/s to 45 MB/s.
            //

            if (ProgressCallback != NULL &&
                DownloadProgress->CollateralDownloadedSize >= NextProgressUpdate) {
                NextProgressUpdate = DownloadProgress->CollateralDownloadedSize +
                                     OnePercentOfCollateralSize;
                Begin = GetTickCount();
                Status = ProgressCallback((PEFI_MS_CBMR_PROTOCOL)Internal, Progress);
                if (EFI_ERROR(Status)) {
                    //
                    // Terminate the download process if the caller asked us not
                    // to proceed any further
                    //

                    DBG_ERROR(
                        "Aborting CBMR collateral download phase as caller callback returned 0x%zx",
                        Status);
                    goto Exit;
                }
                End = GetTickCount();

                ProgressCallbackTickCount += End - Begin;
            }

            Status = HttpGetNext(Internal->HttpContext, Response);
            if (EFI_ERROR(Status) && Status != EFI_END_OF_FILE) {
                DBG_ERROR("HttpGetNext() failed 0x%zx", Status);
                goto Exit;
            }
        } while (Status != EFI_END_OF_FILE);

        Status = EFI_SUCCESS;
        DBG_INFO("Total elapsed tick count (FileWrite): %llu", TotalTickCount);
        DBG_INFO("Total elapsed tick count (ProgressCallback): %llu", ProgressCallbackTickCount);

        HttpFreeResponse(Internal->HttpContext, Response);
        Response = NULL;

        FileClose(File);
        File = NULL;
    }

    //
    // Give application a chance to render its UI to show that the collateral
    // download has completed
    //

    Progress->CurrentPhase = MsCbmrPhaseCollateralsDownloaded;
    if (ProgressCallback != NULL) {
        Status = ProgressCallback((PEFI_MS_CBMR_PROTOCOL)Internal, Progress);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("Aborting CBMR collateral download phase as caller callback returned 0x%zx",
                      Status);
            goto Exit;
        }
    }

Exit:

    FileClose(File);
    HttpFreeResponse(Internal->HttpContext, Response);
    HttpFreeHeaderFields(HttpHeaders, HeaderCount);

    return Status;
}
#endif

static EFI_STATUS
CbmrStartCollateralDownloadFromDcatEndpoint(_In_ PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal)
{
    EFI_STATUS Status = EFI_SUCCESS;
    PEFI_MS_CBMR_COLLATERALS_DOWNLOAD_PROGRESS DownloadProgress = NULL;
    EFI_FILE_PROTOCOL* File = NULL;
    CHAR8 AsciiUrl[4096] = {0};
    EFI_HTTP_HEADER* HttpHeaders = NULL;
    UINTN HeaderCount = 0;
    HTTP_RESPONSE* Response = NULL;
    EFI_HASH2_PROTOCOL* Hash2Protocol = NULL;
    EFI_SERVICE_BINDING_PROTOCOL* ServiceBinding = NULL;
    EFI_HANDLE Handle = NULL;
    EFI_HASH2_OUTPUT Output = {0};
    EFI_MS_CBMR_PROGRESS_CALLBACK ProgressCallback = Internal->ProgressCallBack;
    PEFI_MS_CBMR_PROGRESS Progress = &Internal->Progress;

    if (gCbmrConfig.ForceHttps) {
        //
        // Cert-pin against content channel TLS CA certs. They are not required
        // if downloading via HTTP (which is the default URL type in DCAT's JSON response).
        //

        Status = TlsSetCACertList(DcatContentChannelTlsCaCerts,
                                  ARRAYSIZE(DcatContentChannelTlsCaCerts));
        if (EFI_ERROR(Status)) {
            DBG_ERROR("TlsSetCACertList() failed 0x%zx", Status);
            goto Exit;
        }

        DBG_INFO("Configured TLS certs for content channel");
    }

    //
    // Collateral download phase
    //

    Progress->CurrentPhase = MsCbmrPhaseCollateralsDownloading;
    DownloadProgress = &Progress->ProgressData.DownloadProgress;

    if (gCbmrConfig.SkipHashValidation == FALSE) {
        Status = gBS->LocateProtocol(&gEfiHash2ServiceBindingProtocolGuid, NULL, &ServiceBinding);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("LocateProtocol() for Hash2 servicing binding protocol failed 0x%zx", Status);
            goto Exit;
        }

        Status = ServiceBinding->CreateChild(ServiceBinding, &Handle);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("CreateChild() for EFI_HASH2_PROTOCOL failed 0x%zx", Status);
            goto Exit;
        }

        Status = gBS->HandleProtocol(Handle, &gEfiHash2ProtocolGuid, &Hash2Protocol);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("HandleProtocol() for EFI_HASH2_PROTOCOL failed 0x%zx", Status);
            goto Exit;
        }
    }

    for (UINTN i = 0; i < Internal->NumberOfCollaterals; i++) {
        UINTN Position = 0;

        DownloadProgress->CollateralIndex = i;
        DBG_INFO_U(L"Downloading %s", Internal->Collaterals[i].RootUrl);

        UnicodeStrToAsciiStr(Internal->Collaterals[i].RootUrl, AsciiUrl);

        Status = CbmrBuildRequestHeaders(AsciiUrl,
                                         Internal->Collaterals[i].RootUrlLength,
                                         &HttpHeaders,
                                         &HeaderCount);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("CbmrBuildRequestHeaders() failed 0x%zx", Status);
            goto Exit;
        }

        Status = HttpIssueRequest(Internal->HttpContext,
                                  Internal->Collaterals[i].RootUrl,
                                  Internal->Collaterals[i].RootUrlLength,
                                  HttpMethodGet,
                                  HttpHeaders,
                                  HeaderCount,
                                  NULL,
                                  0,
                                  Internal->Collaterals[i].CollateralSize,
                                  &Response);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("HttpIssueRequest() failed 0x%zx", Status);
            goto Exit;
        }

        HttpFreeHeaderFields(HttpHeaders, HeaderCount);

        HttpHeaders = NULL;
        HeaderCount = 0;

        if (Internal->Collaterals[i].StoreInMemory == TRUE) {
            Internal->Collaterals[i].MemoryLocation = AllocatePool(
                Internal->Collaterals[i].CollateralSize);
            if (Internal->Collaterals[i].MemoryLocation == NULL) {
                DBG_ERROR("Out of memory");
                Status = EFI_OUT_OF_RESOURCES;
                goto Exit;
            }

            //
            // Check if this is the CBMR driver. If so, store it in easy-to-access
            // location.
            //

            if (StrStr(Internal->Collaterals[i].RootUrl, CBMR_DRIVER_FILE_NAME) != NULL) {
                Internal->CbmrDriver = Internal->Collaterals[i].MemoryLocation;
                Internal->CbmrDriverSize = Internal->Collaterals[i].CollateralSize;
            }
        } else {
            //
            // Create and open file just once to avoid file open overhead.
            //

            Status = FileCreateSubdirectoriesAndFile(STUBOS_VOLUME_LABEL,
                                                     Internal->Collaterals[i].FilePath,
                                                     &File);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("FileCreateSubdirectoriesAndFile() failed with status 0x%zx", Status);
                goto Exit;
            }
        }

        UINT64 TotalTickCount = 0;
        UINT64 ProgressCallbackTickCount = 0;
        UINT64 Begin = 0;
        UINT64 End = 0;
        UINTN OnePercentOfCollateralSize = Internal->Collaterals[i].CollateralSize / 100;
        UINTN NextProgressUpdate = OnePercentOfCollateralSize;

        if (gCbmrConfig.SkipHashValidation == FALSE) {
            Status = Hash2Protocol->HashInit(Hash2Protocol, &gEfiHashAlgorithmSha256Guid);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("HashInit() failed with status 0x%zx", Status);
                goto Exit;
            }
        }

        do {
            UINTN ChunkSize = HttpGetChunkSize(Response);
            UINT8* Chunk = HttpGetChunk(Response);

            if (gCbmrConfig.SkipHashValidation == FALSE) {
                Status = Hash2Protocol->HashUpdate(Hash2Protocol, Chunk, ChunkSize);
                if (EFI_ERROR(Status)) {
                    DBG_ERROR("HashUpdate() failed with status 0x%zx", Status);
                    goto Exit;
                }
            }

            //
            // This checks for potential out-of-bounds writes against the initial CollateralSize
            // (or heap overflow for memory buffer or integer overflow). There are also checks in
            // HttpGetResponse/HttpGetNext that validate against the expected content length,
            // but the more checks the better.
            //

            if (Position + ChunkSize > Internal->Collaterals[i].CollateralSize) {
                DBG_ERROR("Position (%zu) + ChunkSize (%zu) exceeds Collateral size (%zu)",
                          Position,
                          ChunkSize,
                          Internal->Collaterals[i].CollateralSize);
                Status = EFI_ABORTED;
                goto Exit;
            }

            if (Position > Position + ChunkSize) {
                DBG_ERROR("Integer overflow, Position (%zu) + ChunkSize (%zu)",
                          Position,
                          ChunkSize);
                Status = EFI_ABORTED;
                goto Exit;
            }

            Begin = GetTickCount();
            if (Internal->Collaterals[i].StoreInMemory == TRUE) {
                Status = CopyMemS(Internal->Collaterals[i].MemoryLocation + Position,
                                  Internal->Collaterals[i].CollateralSize - Position,
                                  Chunk,
                                  ChunkSize);
                if (EFI_ERROR(Status)) {
                    DBG_ERROR("CopyMemS() failed 0x%zx", Status);
                    goto Exit;
                }
            } else {
                Status = FileWrite(File, &ChunkSize, Chunk);
                if (EFI_ERROR(Status)) {
                    DBG_ERROR("FileWrite() failed 0x%zx", Status);
                    Status = EFI_NOT_READY;
                    goto Exit;
                }
            }
            End = GetTickCount();

            TotalTickCount += End - Begin;
            Position += ChunkSize;

            DownloadProgress->CollateralDownloadedSize = Position;
            // DownloadProgress->CollateralCurrentChunkSize = ChunkSize;
            // DownloadProgress->CollateralCurrentChunkData = Chunk;

            //
            // Invoke the application/caller
            //

            //
            // Note: Below CollateralDownloadedSize check is used to throttle calls to
            // ProgressCallback, as calling it too often can have terrible perf
            // impact (mostly due to updating UI). Don't remove the check unless
            // you know what you're doing. Proof: When testing against VM, by
            // adding the simple check below, the download speed shot up from
            // 1.2 MB/s to 45 MB/s.
            //

            if (ProgressCallback != NULL &&
                DownloadProgress->CollateralDownloadedSize >= NextProgressUpdate) {
                NextProgressUpdate = DownloadProgress->CollateralDownloadedSize +
                                     OnePercentOfCollateralSize;
                Begin = GetTickCount();
                Status = ProgressCallback((PEFI_MS_CBMR_PROTOCOL)Internal, Progress);
                if (EFI_ERROR(Status)) {
                    //
                    // Terminate the download process if the caller asked us not
                    // to proceed any further
                    //

                    DBG_ERROR(
                        "Aborting CBMR collateral download phase as caller callback returned 0x%zx",
                        Status);
                    goto Exit;
                }
                End = GetTickCount();

                ProgressCallbackTickCount += End - Begin;
            }

            Status = HttpGetNext(Internal->HttpContext, Response);
            if (EFI_ERROR(Status) && Status != EFI_END_OF_FILE) {
                DBG_ERROR("HttpGetNext() failed 0x%zx", Status);
                goto Exit;
            }
        } while (Status != EFI_END_OF_FILE);

        Status = EFI_SUCCESS;
        DBG_INFO("Total elapsed tick count (FileWrite): %llu", TotalTickCount);
        DBG_INFO("Total elapsed tick count (ProgressCallback): %llu", ProgressCallbackTickCount);

        if (gCbmrConfig.SkipHashValidation == FALSE) {
            Status = Hash2Protocol->HashFinal(Hash2Protocol, &Output);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("HashFinal() failed with status 0x%zx", Status);
                goto Exit;
            }

            //
            // Now compare computed hash with previously retrieved hash
            //

            if (CompareMem(Internal->Collaterals[i].Digest, Output.Sha256Hash, HASH_LENGTH) != 0) {
                DBG_ERROR("Hash mismatch");
                Status = EFI_ABORTED;
                goto Exit;
            }
        }

        HttpFreeResponse(Internal->HttpContext, Response);
        Response = NULL;

        FileClose(File);
        File = NULL;
    }

    //
    // Give application a chance to render its UI to show that the collateral
    // download has completed
    //

    Progress->CurrentPhase = MsCbmrPhaseCollateralsDownloaded;
    if (ProgressCallback != NULL) {
        Status = ProgressCallback((PEFI_MS_CBMR_PROTOCOL)Internal, Progress);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("Aborting CBMR collateral download phase as caller callback returned 0x%zx",
                      Status);
            goto Exit;
        }
    }

Exit:

    if (Handle != NULL) {
        EFI_STATUS TempStatus = ServiceBinding->DestroyChild(ServiceBinding, Handle);
        if (EFI_ERROR(TempStatus)) {
            DBG_ERROR("DestroyChild() failed 0x%zx", TempStatus);
        }
    }

    FileClose(File);
    HttpFreeResponse(Internal->HttpContext, Response);
    HttpFreeHeaderFields(HttpHeaders, HeaderCount);

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_DRIVER_DCAT_COLLATERAL_DOWNLOAD_FAILED);
    }

    return Status;
}

static EFI_STATUS CbmrStartCollateralDownload(_In_ PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_TIME StartTime = {0};
    EFI_TIME EndTime = {0};
    UINTN Hours = 0, Minutes = 0, Seconds = 0;

    gRT->GetTime(&StartTime, NULL);

    switch (gCbmrConfig.EndpointType) {
        case CBMR_ENDPOINT_TYPE_DCAT:
            Status = CbmrStartCollateralDownloadFromDcatEndpoint(Internal);
            break;
#ifdef DEBUGMODE
        case CBMR_ENDPOINT_TYPE_LOCAL_HTTP:
            Status = CbmrStartCollateralDownloadFromHttpEndpoint(Internal);
            break;
        case CBMR_ENDPOINT_TYPE_USBKEY:
            Status = CbmrStartCollateralDownloadFromUSBKey(Internal);
            break;
        default:
            Status = EFI_INVALID_PARAMETER;
            break;
#endif
    }

    gRT->GetTime(&EndTime, NULL);

    TimeDiff(&StartTime, &EndTime, &Hours, &Minutes, &Seconds);

    DBG_INFO("Total collateral download duration(hh:mm:ss): %zu:%zu:%zu", Hours, Minutes, Seconds);

    //
    // This is useful to know at what percentage the download failed.
    //

    if (EFI_ERROR(Status)) {
        UINTN CollateralIndex = Internal->Progress.ProgressData.DownloadProgress.CollateralIndex;
        UINTN CurrentDownloadSize = Internal->Progress.ProgressData.DownloadProgress
                                        .CollateralDownloadedSize;
        UINTN TotalCollateralSize = Internal->Collaterals[CollateralIndex].CollateralSize;

        DBG_INFO_U((CHAR16*)L"Currently downloading %s to %s (%zu/%zu) bytes %zu%%",
                   Internal->Collaterals[CollateralIndex].RootUrl,
                   Internal->Collaterals[CollateralIndex].FilePath,
                   CurrentDownloadSize,
                   TotalCollateralSize,
                   (UINTN)((100 * CurrentDownloadSize) / TotalCollateralSize));
        DBG_INFO("Total collaterals download progress: %zu%%",
                 (CollateralIndex * 100) / Internal->NumberOfCollaterals);
    }

    return Status;
}

static EFI_STATUS CbmrExtractBootCollateralsFromWim(_In_ PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal)
{
    UNREFERENCED_PARAMETER(Internal);

    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* WimFile = NULL;
    WIM_CONTEXT* WimContext = NULL;

    WIM_TO_RAMDISK_FILE BootFiles[] = {
        // clang-format off
        {t(STUBOS_WIM_BOOT_SDI_PATH), STRING_LEN(STUBOS_WIM_BOOT_SDI_PATH), RAMDISK_BOOT_SDI_PATH, TRUE},
        {t(STUBOS_WIM_BOOTMGR_PATH), STRING_LEN(STUBOS_WIM_BOOTMGR_PATH),    BOOTMGR_PATH, TRUE},
        {t(STUBOS_WIM_BCD_PATH), STRING_LEN(STUBOS_WIM_BCD_PATH),        RAMDISK_BCD_PATH, TRUE},
        {t(STUBOS_WIM_CBMR_DRIVER_PATH), STRING_LEN(STUBOS_WIM_CBMR_DRIVER_PATH), RAMDISK_CBMR_DRIVER_PATH, FALSE},
        // clang-format on
    };

    //
    // Find boot.wim on ramdisk
    //

    Status = FileOpen(STUBOS_VOLUME_LABEL, RAMDISK_WIM_PATH, EFI_FILE_MODE_READ, 0, &WimFile);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileOpen() failed 0x%zx", Status);
        goto Exit;
    }

    //
    // Initialize WIM context
    //

    Status = WimInit(WimFile, &WimContext);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("WimInit() failed 0x%zx", Status);
        goto Exit;
    }

    //
    // WimContext has ownership of WimFile, so don't use it anymore.
    //

    WimFile = NULL;

    //
    // Create destination files in the ramdisk volume, extract
    // them from the WIM and then write them back to the destination.
    //

    for (UINTN Index = 0; Index < _countof(BootFiles); Index++) {
        //
        // Extract file from WIM
        //

        Status = WimExtractFileIntoDestination(WimContext,
                                               BootFiles[Index].FilePathInWim,
                                               BootFiles[Index].FilePathInWimLength,
                                               STUBOS_VOLUME_LABEL,
                                               BootFiles[Index].FilePathInRamDisk);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("WimExtractFileIntoDestination() failed 0x%zx", Status);
            if (BootFiles[Index].Critical == FALSE) {
                DBG_INFO("Not critical for boot to succeed, ignore failure");
                Status = EFI_SUCCESS;
            } else {
                goto Exit;
            }
        }
    }

Exit:

    FileClose(WimFile);
    WimFree(WimContext);

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_DRIVER_BOOT_COLLATERAL_EXTRACTION_FAILED);
    }

    return Status;
}

#ifdef DEBUGMODE
static EFI_STATUS CbmrWriteSIUefiVariable(_In_ SOFTWARE_INVENTORY_TYPE InventoryType)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* SiWimFile = NULL;
    UINT64 SiWimFileSize = 0;
    VOID* SiWimFileBuffer = NULL;
    UINT32 Attributes = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS |
                        EFI_VARIABLE_RUNTIME_ACCESS;

    CHAR16* SiWimFileName = NULL;
    CHAR16* SiVariableName = NULL;

#define SI_WIM_FILENAME  L"si.wim"
#define SI2_WIM_FILENAME L"si2.wim"

    if (InventoryType == SOFTWARE_INVENTORY_PRIMARY && gCbmrConfig.WriteSiUefiVariable == FALSE) {
        DBG_INFO("Skip writing si.wim");
        goto Exit;
    }

    if (InventoryType == SOFTWARE_INVENTORY_SECONDARY &&
        gCbmrConfig.WriteSi2UefiVariable == FALSE) {
        DBG_INFO("Skip writing si2.wim");
        goto Exit;
    }

    if (InventoryType == SOFTWARE_INVENTORY_PRIMARY) {
        SiWimFileName = SI_WIM_FILENAME;
        SiVariableName = EFI_MS_CBMR_SOFTWARE_INVENTORY_VARIABLE;
    } else if (InventoryType == SOFTWARE_INVENTORY_SECONDARY) {
        SiWimFileName = SI2_WIM_FILENAME;
        SiVariableName = EFI_MS_CBMR_SOFTWARE_INVENTORY_SECONDARY_VARIABLE;
    }

    DBG_INFO_U("Locating %s", SiWimFileName);
    Status = FileLocateAndOpen(SiWimFileName, EFI_FILE_MODE_READ, &SiWimFile);
    if (EFI_ERROR(Status)) {
        DBG_ERROR_U(L"FileLocateAndOpen() Failed 0x%zx %s", Status, SiWimFileName);
        goto Exit;
    }

    DBG_INFO_U("Getting %s file size", SiWimFileName);
    Status = FileGetSize(SiWimFile, &SiWimFileSize);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileGetSize() failed : 0x%zx", Status);
        goto Exit;
    }

    SiWimFileBuffer = AllocateZeroPool((UINTN)SiWimFileSize);
    if (SiWimFileBuffer == NULL) {
        DBG_ERROR("AllocateZeroPool() failed to allocate buffer of size %llu", SiWimFileSize);
        goto Exit;
    }

    DBG_INFO_U("Reading %s", SiWimFileName);
    Status = FileRead(SiWimFile, (UINTN*)&SiWimFileSize, SiWimFileBuffer);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileRead() failed : 0x%zx", Status);
        goto Exit;
    }

    DBG_INFO_U("Storing %s in to SoftwareInventory UEFI variable", SiWimFileName);
    Status = gRT->SetVariable(SiVariableName,
                              &(EFI_GUID)EFI_MS_CBMR_VARIABLES_INTERNAL_GUID,
                              Attributes,
                              (UINTN)SiWimFileSize,
                              SiWimFileBuffer);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("SetVariable() failed : 0x%zx", Status);
        goto Exit;
    }

    DBG_INFO_U("Successfully stored %s into Software Inventory UEFI variable", SiWimFileName);

Exit:
    FileClose(SiWimFile);
    FreePool(SiWimFileBuffer);

    return Status;
}
#endif

EFI_STATUS
EFIAPI
CbmrConfigureInternal(_In_ PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal,
                      _In_opt_ EFI_MS_CBMR_PROGRESS_CALLBACK ProgressCallback)
{
    EFI_STATUS Status = EFI_SUCCESS;
    PSOFTWARE_INVENTORY_INFO SiInfo = NULL;

    //
    // Create space for Software Inventories
    //

    SiInfo = &Internal->SoftwareInventories[SOFTWARE_INVENTORY_PRIMARY];
    SiInfo->InventoryType = SOFTWARE_INVENTORY_PRIMARY;
    SiInfo->UefiVariableName = EFI_MS_CBMR_SOFTWARE_INVENTORY_VARIABLE;
    SiInfo->RamdiskFilePath = RAMDISK_SI_WIM_PATH;
    SiInfo->RequestJson = AllocateZeroPool(MAX_JSON_REQUEST_SIZE);
    if (SiInfo->RequestJson == NULL) {
        DBG_ERROR("Out of memory");
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    SiInfo = &Internal->SoftwareInventories[SOFTWARE_INVENTORY_SECONDARY];
    SiInfo->InventoryType = SOFTWARE_INVENTORY_SECONDARY;
    SiInfo->UefiVariableName = EFI_MS_CBMR_SOFTWARE_INVENTORY_SECONDARY_VARIABLE;
    SiInfo->RamdiskFilePath = RAMDISK_SI2_WIM_PATH;
    SiInfo->RequestJson = AllocateZeroPool(MAX_JSON_REQUEST_SIZE);
    if (SiInfo->RequestJson == NULL) {
        DBG_ERROR("Out of memory");
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    //
    // Initialize the progress callback
    //

    Internal->ProgressCallBack = ProgressCallback;

Exit:
    return Status;
}
//
// Public functions
//

EFI_STATUS
EFIAPI
CbmrConfigure(_In_ PEFI_MS_CBMR_PROTOCOL This,
              _In_ PEFI_MS_CBMR_CONFIG_DATA CbmrConfigData,
              _In_opt_ EFI_MS_CBMR_PROGRESS_CALLBACK ProgressCallback)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_STATUS CloseStatus = EFI_SUCCESS;
    PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal = (PEFI_MS_CBMR_PROTOCOL_INTERNAL)This;
    PEFI_MS_CBMR_PROGRESS Progress = &Internal->Progress;

    if (Internal->IsDriverConfigured == TRUE) {
        DBG_WARNING("Cbmr driver is already configured");
        goto Exit;
    }

    Status = CbmrConfigureInternal(Internal, ProgressCallback);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CbmrConfigureInternal() failed 0x%zx", Status);
        goto Exit;
    }

    Progress->CurrentPhase = MsCbmrPhaseConfiguring;
    if (ProgressCallback != NULL) {
        ProgressCallback((PEFI_MS_CBMR_PROTOCOL)Internal, Progress);
    }

    CbmrInitializeErrorModule(This);

    Status = CbmrReadConfig(CBMR_CONFIG_DRIVER_SECTION);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CbmrReadConfig() failed 0x%zx", Status);
        goto Exit;
    }

#ifdef DEBUGMODE
    Status = CbmrWriteSIUefiVariable(SOFTWARE_INVENTORY_PRIMARY);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CbmrWriteSIUefiVariable() failed 0x%zx", Status);
        goto Exit;
    }

    Status = CbmrWriteSIUefiVariable(SOFTWARE_INVENTORY_SECONDARY);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CbmrWriteSIUefiVariable() failed 0x%zx", Status);
        goto Exit;
    }
#endif

    Status = CbmrConfigureRamdisk(Internal);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CbmrConfigureRamdisk() failed 0x%zx", Status);
        goto Exit;
    }

    Status = HttpCreate(&Internal->HttpContext);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("HttpCreate() failed 0x%zx", Status);
        goto Exit;
    }

    Status = CbmrDepositWiFiProfileToRamdisk(Internal, CbmrConfigData);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CbmrDepositWiFiProfileToRamdisk() failed 0x%zx", Status);
        goto Exit;
    }

    Status = CbmrDepositSoftwareInventoryToRamdisk(Internal);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CbmrDepositSoftwareInventoryToRamdisk() failed 0x%zx", Status);
        goto Exit;
    }

    Status = CbmrProcessSoftwareInventory(Internal);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CbmrProcessSoftwareInventory() for SOFTWARE_INVENTORY_PRIMARY failed 0x%zx",
                  Status);
        goto Exit;
    }

    Status = CbmrFetchCollaterals(Internal);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CbmrFetchCollaterals() failed 0x%zx", Status);
        goto Exit;
    }

    Status = CbmrDepositDcatInfoToRamdisk(Internal);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CbmrDepositDcatInfoToRamdisk() failed 0x%zx", Status);
        goto Exit;
    }

    Internal->IsDriverConfigured = TRUE;

    Progress->CurrentPhase = MsCbmrPhaseConfigured;
    if (ProgressCallback != NULL) {
        ProgressCallback((PEFI_MS_CBMR_PROTOCOL)Internal, Progress);
    }

    return Status;

Exit:
    CloseStatus = CbmrClose(This);
    if (EFI_ERROR(CloseStatus)) {
        DBG_ERROR("CbmrClose() failed 0x%zx", CloseStatus);
    }

    // FIXME: Ignoring above CloseStatus error?
    return Status;
}

#ifdef DEBUGMODE
static EFI_STATUS CbmrCopyPatchedBCD(_In_ PEFI_MS_CBMR_PROTOCOL_INTERNAL This)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* PatchedDestBCD = NULL;
    UINTN PatchedDestBCDSize = 0;

    UNREFERENCED_PARAMETER(This);

    if (gCbmrConfig.EnableTestSigningOnStubOS == FALSE) {
        goto Exit;
    }

    Status = FileOpen(STUBOS_VOLUME_LABEL,
                      RAMDISK_BCD_PATH,
                      EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                      0,
                      &PatchedDestBCD);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileLocateAndOpen() failed. Unable to locate \\efi\\microsoft\\boot\\bcd 0x%zx",
                  Status);
        goto Exit;
    }

    PatchedDestBCDSize = _countof(TestSignedAndNoIntegrityChecksBcd);
    Status = FileWrite(PatchedDestBCD,
                       &PatchedDestBCDSize,
                       (VOID*)TestSignedAndNoIntegrityChecksBcd);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileWrite() failed. Unable to write \\efi\\microsoft\\boot\\bcd 0x%zx", Status);
        goto Exit;
    }

    DBG_INFO("Wrote patched BCD to ramdisk \\efi\\microsoft\\boot\\bcd");

Exit:

    if (PatchedDestBCD != NULL) {
        PatchedDestBCD->Close(PatchedDestBCD);
    }

    return Status;
}

static EFI_STATUS CbmrCopyUSBKeyContentsToRamdisk(_In_ PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal)
{
    EFI_STATUS Status = EFI_SUCCESS;
    struct {
        CHAR16* From;
        UINTN FromLength;
        CHAR16* To;
        UINTN ToLength;
    } UsbKeyToRamdiskMap[] = {
        {L"\\usbkey\\os", _countof(L"\\usbkey\\os"), L"\\cbmr\\os", _countof(L"\\cbmr\\os")},
        {L"\\usbkey\\drivers",
         _countof(L"\\usbkey\\drivers"),
         L"\\cbmr\\drivers",
         _countof(L"\\cbmr\\drivers")},
        {L"\\usbkey\\reset.ini", _countof(L"\\usbkey\\reset.ini"), L"\\cbmr", _countof(L"\\cbmr")},
    };

    UNREFERENCED_PARAMETER(Internal);

    for (UINTN Index = 0; Index < _countof(UsbKeyToRamdiskMap); Index++) {
        EFI_FILE_PROTOCOL* Source = NULL;
        EFI_FILE_PROTOCOL* Dest = NULL;
        CHAR16* From = UsbKeyToRamdiskMap[Index].From;
        CHAR16* To = UsbKeyToRamdiskMap[Index].To;
        UINTN ToLength = UsbKeyToRamdiskMap[Index].ToLength;

        Status = FileLocateAndOpen(From, EFI_FILE_MODE_READ, &Source);
        if (EFI_ERROR(Status)) {
            DBG_ERROR_U(L"FileLocateAndOpen() failed. Unable to locate %s 0x%zx", From, Status);
            Status = EFI_SUCCESS;
            goto Exit;
        }

        Status = FileCreateSubdirectories(STUBOS_VOLUME_LABEL, To, ToLength, &Dest);
        if (EFI_ERROR(Status)) {
            DBG_ERROR_U(L"FileCreateSubdirectories() failed for %s with status 0x%zx", To, Status);
            goto Exit;
        }

        Status = FileCopy(Source, Dest);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileCopy() failed 0x%zx", Status);
            goto Exit;
        }

        DBG_INFO_U(L"Successfully copied %s to %s", From, To);

    Exit:
        if (Source != NULL) {
            Source->Close(Source);
        }

        if (Dest != NULL) {
            Dest->Close(Dest);
        }
    }

    return Status;
}

#endif

EFI_STATUS
EFIAPI
CbmrStart(_In_ PEFI_MS_CBMR_PROTOCOL This)
{
    EFI_STATUS Status = EFI_SUCCESS;
    PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal = (PEFI_MS_CBMR_PROTOCOL_INTERNAL)This;

    if (Internal->IsDriverConfigured == FALSE) {
        DBG_ERROR("Cbmr driver is not configured");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    CbmrClearExtendedErrorInfo();

    //
    // Collateral download phase
    //

    Status = CbmrStartCollateralDownload(Internal);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CbmrStartCollateralDownload() failed 0x%zx", Status);
        goto Exit;
    }

    //
    // Wim extraction phase
    //

    Status = CbmrExtractBootCollateralsFromWim(Internal);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CbmrExtractBootCollateralsFromWim() failed 0x%zx", Status);
        goto Exit;
    }

#ifdef DEBUGMODE
    //
    // Copy patched BCD if needed
    //
#ifndef _ARM64_
    Status = CbmrCopyPatchedBCD(Internal);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CbmrCopyPatchedBCD() failed 0x%zx", Status);
        goto Exit;
    }
#endif
    Status = CbmrCopyUSBKeyContentsToRamdisk(Internal);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CbmrCopyUSBKeyContentsToRamdisk() failed 0x%zx", Status);
        goto Exit;
    }

#endif

    //
    // Service the CBMR driver if it was downloaded or found embedded in the WinRE.wim.
    //

    Status = CbmrServiceDriver(Internal);
    if (EFI_ERROR(Status)) {
        if (Status == EFI_NOT_FOUND) {
            DBG_INFO("No cbmr_driver found, skip servicing");
            Status = EFI_SUCCESS;
        } else {
            DBG_ERROR("CbmrServiceDriver() failed 0x%zx", Status);
            goto Exit;
        }
    }

    //
    // Rambooting StubOS phase
    //

    Status = CbmrStartStubOsRambooting(Internal);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CbmrStartStubOsRambooting() failed 0x%zx", Status);
        goto Exit;
    }

Exit:
    return Status;
}

static EFI_STATUS CbmrGetVersion(_In_ PEFI_MS_CBMR_PROTOCOL This,
                                 _Inout_ UINT64* Data,
                                 _Inout_ UINTN* DataSize)
{
    EFI_STATUS Status = EFI_SUCCESS;

    UNREFERENCED_PARAMETER(This);

    if (DataSize == NULL) {
        DBG_ERROR("Invalid DataSize parameter");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (*DataSize < sizeof(UINT64)) {
        *DataSize = sizeof(UINT64);
        Status = EFI_BUFFER_TOO_SMALL;
        goto Exit;
    }

    *Data = EFI_MS_CBMR_PROTOCOL_REVISION;
    *DataSize = sizeof(UINT64);
    return Status;

Exit:
    return Status;
}

static EFI_STATUS CbmrGetCollaterals(_In_ PEFI_MS_CBMR_PROTOCOL This,
                                     _Inout_ PEFI_MS_CBMR_COLLATERAL Data,
                                     _Inout_ UINTN* DataSize)
{
    EFI_STATUS Status = EFI_SUCCESS;
    PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal = (PEFI_MS_CBMR_PROTOCOL_INTERNAL)This;

    if (Internal->IsDriverConfigured == FALSE) {
        DBG_ERROR("Cbmr driver is not configured");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    if (DataSize == NULL) {
        DBG_ERROR("Invalid DataSize parameter");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (*DataSize < sizeof(EFI_MS_CBMR_COLLATERAL) * Internal->NumberOfCollaterals) {
        *DataSize = sizeof(EFI_MS_CBMR_COLLATERAL) * Internal->NumberOfCollaterals;
        Status = EFI_BUFFER_TOO_SMALL;
        goto Exit;
    }

    *DataSize = 0;
    for (UINTN i = 0; i < Internal->NumberOfCollaterals; i++) {
        EFI_STATUS UrlStatus = StrDup(Internal->Collaterals[i].RootUrl, &Data[i].RootUrl);
        EFI_STATUS FilePathStatus = StrDup(Internal->Collaterals[i].FilePath, &Data[i].FilePath);
        if (EFI_ERROR(UrlStatus) || EFI_ERROR(FilePathStatus)) {
            Status = EFI_OUT_OF_RESOURCES;
            goto Exit;
        }

        Data[i].RootUrlLength = Internal->Collaterals[i].RootUrlLength;
        Data[i].CollateralSize = Internal->Collaterals[i].CollateralSize;
    }

    *DataSize = sizeof(EFI_MS_CBMR_COLLATERAL) * Internal->NumberOfCollaterals;
    return Status;

Exit:
    return Status;
}

EFI_STATUS
EFIAPI
CbmrGetData(_In_ PEFI_MS_CBMR_PROTOCOL This,
            _In_ EFI_MS_CBMR_DATA_TYPE DataType,
            _Inout_ VOID* Data,
            _Inout_ UINTN* DataSize)
{
    EFI_STATUS Status = EFI_SUCCESS;

    switch (DataType) {
        case EfiMsCbmrVersion: {
            Status = CbmrGetVersion(This, Data, DataSize);
        } break;
        case EfiMsCbmrCollaterals: {
            Status = CbmrGetCollaterals(This, Data, DataSize);
        } break;
        case EfiMsCbmrExtendedErrorData: {
            Status = CbmrGetExtendedErrorInfo(Data, DataSize);
        } break;
    }

    return Status;
}

EFI_STATUS
EFIAPI
CbmrClose(_In_ PEFI_MS_CBMR_PROTOCOL This)
{
    EFI_STATUS Status = EFI_SUCCESS;
    PEFI_MS_CBMR_PROTOCOL_INTERNAL Internal = (PEFI_MS_CBMR_PROTOCOL_INTERNAL)This;

    //
    // Free HTTP resources
    //

    if (Internal->HttpContext != NULL) {
        Status = HttpFree(Internal->HttpContext);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("HttpFree() failed 0x%zx", Status);
            goto Exit;
        }

        Internal->HttpContext = NULL;
    }

    //
    // Free collateral resources
    //

    if (Internal->Collaterals != NULL) {
        for (UINTN i = 0; i < Internal->NumberOfCollaterals; i++) {
            FreePool(Internal->Collaterals[i].RootUrl);
            FreePool(Internal->Collaterals[i].FilePath);
        }

        FreePool(Internal->Collaterals);
        Internal->Collaterals = NULL;
    }

    //
    // Clear software inventory space
    //

    FreePool(Internal->SoftwareInventories[SOFTWARE_INVENTORY_PRIMARY].RequestJson);
    Internal->SoftwareInventories[SOFTWARE_INVENTORY_PRIMARY].RequestJson = NULL;
    FreePool(Internal->SoftwareInventories[SOFTWARE_INVENTORY_SECONDARY].RequestJson);
    Internal->SoftwareInventories[SOFTWARE_INVENTORY_SECONDARY].RequestJson = NULL;

    CbmrFreeConfig();

    //
    // Free ramdisk context, registered device path and installed
    // block io protocols.
    //

    RamdiskFree(Internal->RamdiskContext);
    Internal->RamdiskContext = NULL;

    Internal->IsDriverConfigured = FALSE;

    //
    // After this the CBMR driver is unusable without a call to Configure()
    // again.
    //

Exit:
    return Status;
}

#ifndef UEFI_BUILD_SYSTEM
#pragma prefast(pop)
#endif
