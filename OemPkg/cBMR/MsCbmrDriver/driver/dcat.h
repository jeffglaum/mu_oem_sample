#ifndef _DCAT_H_
#define _DCAT_H_

#include "http.h"

#ifndef _ARM64_
#define DCAT_REQUEST_JSON_FORMAT_STRING \
    t("{ \"Products\" : \"PN=Client.OS.RS2.amd64&V=%s\", \"DeviceAttributes\" : \"MediaVersion=%s;MediaBranch=%s;OSSkuId=%s;App=Setup360;AppVer=10.0;CBMRScan=1;DUInternal=%d\" }")
#else
#define DCAT_REQUEST_JSON_FORMAT_STRING \
    t("{ \"Products\" : \"PN=Client.OS.RS2.arm64&V=%s\", \"DeviceAttributes\" : \"MediaVersion=%s;MediaBranch=%s;OSSkuId=%s;App=Setup360;AppVer=10.0;CBMRScan=1;DUInternal=%d\" }")
#endif

#define MAX_JSON_REQUEST_SIZE 2048

typedef struct _DCAT_CONTEXT DCAT_CONTEXT;
typedef struct _DCAT_FILE_INFO DCAT_FILE_INFO;

EFI_STATUS EFIAPI DcatInit(_Outptr_ DCAT_CONTEXT** Context);
EFI_STATUS EFIAPI DcatRetrieveJsonBlob(_Inout_ DCAT_CONTEXT* Context,
                                       _In_ HTTP_CONTEXT* HttpContext,
                                       _In_z_ CHAR16* Url,
                                       _In_z_ CHAR8* RequestJson);
EFI_STATUS EFIAPI DcatExtractFileInfoFromJson(_In_ DCAT_CONTEXT* Context,
                                              _In_reads_z_(FileNameLength) CHAR8* FileName,
                                              _In_ UINTN FileNameLength,
                                              _Outptr_ DCAT_FILE_INFO** DcatFileInfo);
EFI_STATUS EFIAPI DcatExtractSizeFromFileInfo(_In_ DCAT_FILE_INFO* DcatFileInfo, _Out_ UINTN* Size);
EFI_STATUS EFIAPI DcatExtractUrlFromFileInfo(_In_ DCAT_FILE_INFO* DcatFileInfo,
                                             _Outptr_ CHAR8** Url,
                                             _Out_ UINTN* UrlLength);
EFI_STATUS EFIAPI DcatExtractDigestFromFileInfo(_In_ DCAT_FILE_INFO* DcatFileInfo,
                                                _Out_writes_bytes_(HASH_LENGTH)
                                                    UINT8 Digest[HASH_LENGTH]);
EFI_STATUS EFIAPI DcatFileInfoFree(_Inout_ DCAT_FILE_INFO* DcatFileInfo);
EFI_STATUS EFIAPI DcatFree(_Inout_ DCAT_CONTEXT* Context);

#endif // _DCAT_H_