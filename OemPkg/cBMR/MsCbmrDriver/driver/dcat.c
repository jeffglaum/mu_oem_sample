/*++

Copyright (c) 2021 Microsoft Corporation

Module Name:

    dcat.c

Abstract:

    This module implements module for communicating with and receiving
    resources from DCAT.

Author:

    Jancarlo Perez (jpere) 28-July-2021

Environment:

    UEFI mode only.

--*/

#include "cbmrincludes.h"
#include "http.h"
#include "strsafe.h"
#include "error.h"
#include "dcat.h"

#ifndef UEFI_BUILD_SYSTEM
#pragma prefast(push)
#pragma prefast(disable : 6101, \
                "False positive - _Out_ parameter is always initialized upon success")
#endif

#define HEADER_AGENT_VALUE           "CBMR-Agent"
#define HEADER_ACCEPT_VALUE          "*/*"
#define HEADER_CONTENT_JSON          "application/json"
#define DIGEST_BASE64_NUM_CHARACTERS 44

#define STOP_PARSING_IF_NUL(String)                  \
    {                                                \
        if (String != NULL && *String == '\0') {     \
            DBG_ERROR("NUL char found, exit early"); \
            Status = EFI_NOT_FOUND;                  \
            goto Exit;                               \
        }                                            \
    }

typedef struct _DCAT_CONTEXT {
    BOOLEAN Initialized;
    UINT8* JsonBlob;

    // TODO: Add remaining fields
} DCAT_CONTEXT, *PDCAT_CONTEXT;

typedef struct _DCAT_FILE_INFO {
    CHAR8* FileName;
    UINTN Size;
    CHAR8* Url;
    UINTN UrlLength;
    UINT8 Digest[HASH_LENGTH];

    // TODO: Add remaining fields
} DCAT_FILE_INFO, *PDCAT_FILE_INFO;

static EFI_STATUS EFIAPI DcatBuildRequestHeaders(_In_z_ CHAR8* Url,
                                                 _In_ UINTN BodyLength,
                                                 _In_reads_z_(ContentTypeLength) CHAR8* ContentType,
                                                 _In_ UINTN ContentTypeLength,
                                                 _Outptr_result_buffer_(*Count)
                                                     EFI_HTTP_HEADER** Headers,
                                                 _Out_ UINTN* Count);

EFI_STATUS EFIAPI DcatInit(_Outptr_ DCAT_CONTEXT** Context)
{
    EFI_STATUS Status = EFI_SUCCESS;
    DCAT_CONTEXT* RetContext = NULL;

    RetContext = AllocateZeroPool(sizeof(DCAT_CONTEXT));
    if (RetContext == NULL) {
        DBG_ERROR("Out of memory");
        Status = EFI_OUT_OF_RESOURCES;
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_DCAT_INITIALIZATION_FAILED);
        return Status;
    }

    //
    // Initialize any other values here.
    //

    RetContext->Initialized = TRUE;

    *Context = RetContext;
    return Status;
}

EFI_STATUS EFIAPI DcatRetrieveJsonBlob(_Inout_ DCAT_CONTEXT* Context,
                                       _In_ HTTP_CONTEXT* HttpContext,
                                       _In_z_ CHAR16* Url,
                                       _In_z_ CHAR8* RequestJson)
{
    EFI_STATUS Status = EFI_SUCCESS;
    HTTP_RESPONSE* Response = NULL;
    CHAR8* AsciiUrl = NULL;
    EFI_HTTP_HEADER* HttpHeaders = NULL;
    UINTN HeaderCount = 0;
    UINTN BodyLength = 0;
    UINTN JsonSize = 0;
    UINT8* JsonBlob = NULL;
    UINTN ChunkSize = 0;
    UINT8* Chunk = NULL;
    UINTN ByteOffset = 0;

    if (Context == NULL || HttpContext == NULL || Url == NULL || RequestJson == NULL) {
        DBG_ERROR("Invalid parameter");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    BodyLength = AsciiStrnLenS(RequestJson, MAX_JSON_REQUEST_SIZE);

    AsciiUrl = AllocateZeroPool(StrnLenS(Url, MAX_JSON_REQUEST_URL_SIZE) + sizeof(CHAR8));
    if (AsciiUrl == NULL) {
        DBG_ERROR("Out of memory");
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    UnicodeStrToAsciiStr(Url, AsciiUrl);

    Status = DcatBuildRequestHeaders(AsciiUrl,
                                     BodyLength,
                                     (CHAR8*)HEADER_CONTENT_JSON,
                                     _countof(HEADER_CONTENT_JSON),
                                     &HttpHeaders,
                                     &HeaderCount);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("DcatBuildRequestHeaders() failed 0x%zx", Status);
        goto Exit;
    }

    DBG_INFO("Sending request to DCAT");
    DBG_INFO("RequestJson: %s", RequestJson);

    Status = HttpIssueRequest(HttpContext,
                              Url,
                              StrnLenS(Url, MAX_JSON_REQUEST_URL_SIZE),
                              HttpMethodPost,
                              HttpHeaders,
                              HeaderCount,
                              RequestJson,
                              BodyLength,
                              0,
                              &Response);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("HttpIssueRequest() failed 0x%zx", Status);
        goto Exit;
    }

    JsonSize = HttpGetContentLength(Response);

    //
    // Add extra CHAR8 to ensure JsonBlob is NUL terminated
    //

    Status = UintnAdd(JsonSize, sizeof(CHAR8), &JsonSize);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("UintnAdd() failed 0x%zx", Status);
        goto Exit;
    }

    JsonBlob = AllocateZeroPool(JsonSize);
    if (JsonBlob == NULL) {
        DBG_ERROR("Unable to allocate memory");
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    //
    // Continue extracting JSON response
    //

    do {
        //
        // TODO: Pass in ProgressCallback
        //

        ChunkSize = HttpGetChunkSize(Response);
        Chunk = HttpGetChunk(Response);

        Status = CopyMemS(JsonBlob + ByteOffset, JsonSize - ByteOffset, Chunk, ChunkSize);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("CopyMemS() failed 0x%zx", Status);
            goto Exit;
        }

        ByteOffset += ChunkSize;

        Status = HttpGetNext(HttpContext, Response);
        if (EFI_ERROR(Status) && Status != EFI_END_OF_FILE) {
            DBG_ERROR("HttpGetNext() failed 0x%zx", Status);
            goto Exit;
        }
    } while (Status != EFI_END_OF_FILE);

    Status = EFI_SUCCESS;
    DBG_INFO("JSON blob successfully obtained from DCAT");

    Context->JsonBlob = JsonBlob;
    JsonBlob = NULL;

Exit:

    FreePool(AsciiUrl);
    HttpFreeResponse(HttpContext, Response);
    HttpFreeHeaderFields(HttpHeaders, HeaderCount);
    FreePool(JsonBlob);

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_DCAT_UNABLE_TO_RETRIEVE_JSON);
    }

    return Status;
}

EFI_STATUS EFIAPI DcatExtractFileInfoFromJson(_In_ DCAT_CONTEXT* Context,
                                              _In_reads_z_(FileNameLength) CHAR8* FileName,
                                              _In_ UINTN FileNameLength,
                                              _Outptr_ DCAT_FILE_INFO** DcatFileInfo)
{
    EFI_STATUS Status = EFI_SUCCESS;
    DCAT_FILE_INFO* FileInfo = NULL;
    CHAR8* StringMatch = NULL;
    CHAR8* StringBegin = NULL;
    CHAR8* StringEnd = NULL;
    UINTN ValueLen = 0;
    UINTN DigestBufferLength = HASH_LENGTH;

    if (Context == NULL || FileName == NULL || DcatFileInfo == NULL || FileNameLength == 0) {
        DBG_ERROR("Invalid parameter");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (!Context->Initialized) {
        DBG_ERROR("Context is not initialized");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    //
    // Perform a very rudimentary JSON parse, if it can even be called that.
    //

    FileInfo = AllocateZeroPool(sizeof(DCAT_FILE_INFO));
    if (FileInfo == NULL) {
        DBG_ERROR("Out of memory");
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    //
    // First, find FileName substring. JsonBlob is guaranteed to be NUL terminated.
    //

    StringMatch = AsciiStrStr(Context->JsonBlob, FileName);
    if (StringMatch == NULL) {
        DBG_ERROR("No file match in JSON blob");
        Status = EFI_NOT_FOUND;
        goto Exit;
    }

    //
    // Ok great, there is a match. Now find the actual values we want.
    //

    //
    // Locate Size.
    //

    StringMatch = AsciiStrStr(StringMatch, t("Size"));
    if (StringMatch == NULL) {
        DBG_ERROR("No Size match for %s file element", FileName);
        Status = EFI_NOT_FOUND;
        goto Exit;
    }

    //
    // Skip past ':' character.
    //

    StringMatch = AsciiStrStr(StringMatch, t(":"));
    if (StringMatch == NULL) {
        DBG_ERROR("No : character found");
        Status = EFI_NOT_FOUND;
        goto Exit;
    }

    StringMatch++;

    STOP_PARSING_IF_NUL(StringMatch);

    //
    // Skip any whitespace until we reach a number. Make sure to check for NUL to avoid
    // heap overflow.
    //

    while (*StringMatch != '\0' &&
           (*StringMatch == ' ' || *StringMatch == 0x0A /*Line feed*/ ||
            *StringMatch == 0x0D /*Carriage return*/ || *StringMatch == 0x09 /*Horizontal tab*/)) {
        StringMatch++;
    }

    STOP_PARSING_IF_NUL(StringMatch);

    //
    // Ok, we found the first digit.
    //

    StringBegin = StringMatch;

    //
    // Now find the end of the number. If there is a decimal point, stop there since UEFI doesn't
    // support floating point.
    //

    while (*StringMatch != '\0' && *StringMatch != '.' && *StringMatch != ',') {
        StringMatch++;
    }

    STOP_PARSING_IF_NUL(StringMatch);

    StringEnd = StringMatch;

    ValueLen = StringEnd - StringBegin;
    Status = AsciiStrDecimalToUintnS(StringBegin, NULL, &FileInfo->Size);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("AsciiStrDecimalToUintnS() failed 0x%zx", Status);
        goto Exit;
    }

    //
    // Locate Digest.
    //

    StringMatch = AsciiStrStr(StringMatch, t("Digest"));
    if (StringMatch == NULL) {
        DBG_ERROR("No Url match for %s file element", FileName);
        Status = EFI_NOT_FOUND;
        goto Exit;
    }

    //
    // Skip past ':' character.
    //

    StringMatch = AsciiStrStr(StringMatch, t(":"));
    if (StringMatch == NULL) {
        DBG_ERROR("No : character found");
        Status = EFI_NOT_FOUND;
        goto Exit;
    }

    //
    // Find first quotation mark.
    //

    while (*StringMatch != '\0' && *StringMatch != '\"') {
        StringMatch++;
    }

    STOP_PARSING_IF_NUL(StringMatch);

    StringMatch++;

    STOP_PARSING_IF_NUL(StringMatch);

    StringBegin = StringMatch;

    //
    // Find second quotation mark.
    //

    while (*StringMatch != '\0' && *StringMatch != '\"') {
        StringMatch++;
    }

    STOP_PARSING_IF_NUL(StringMatch);

    StringEnd = StringMatch;
    ValueLen = StringEnd - StringBegin;

    if (ValueLen != DIGEST_BASE64_NUM_CHARACTERS) {
        DBG_ERROR("Incorrect Base64 SHA256 digest length %zu", ValueLen);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    Status = Base64DecodeEdk(StringBegin, ValueLen, FileInfo->Digest, &DigestBufferLength);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Base64DecodeEdk() failed 0x%zx", Status);
        goto Exit;
    }

    //
    // Locate Url.
    //

    StringMatch = AsciiStrStr(StringMatch, t("Url"));
    if (StringMatch == NULL) {
        DBG_ERROR("No Url match for %s file element", FileName);
        Status = EFI_NOT_FOUND;
        goto Exit;
    }

    //
    // Skip past ':' character.
    //

    StringMatch = AsciiStrStr(StringMatch, t(":"));
    if (StringMatch == NULL) {
        DBG_ERROR("No : character found");
        Status = EFI_NOT_FOUND;
        goto Exit;
    }

    //
    // Find first quotation mark.
    //

    while (*StringMatch != '\0' && *StringMatch != '\"') {
        StringMatch++;
    }

    STOP_PARSING_IF_NUL(StringMatch);

    StringMatch++;

    STOP_PARSING_IF_NUL(StringMatch);

    StringBegin = StringMatch;

    //
    // Find second quotation mark.
    //

    while (*StringMatch != '\0' && *StringMatch != '\"') {
        StringMatch++;
    }

    STOP_PARSING_IF_NUL(StringMatch);

    StringEnd = StringMatch;
    ValueLen = StringEnd - StringBegin;

    FileInfo->Url = AllocateZeroPool(ValueLen + sizeof(CHAR8));
    if (FileInfo->Url == NULL) {
        DBG_ERROR("Out of memory");
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    FileInfo->UrlLength = ValueLen;

    Status = CopyMemS(FileInfo->Url, ValueLen + sizeof(CHAR8), StringBegin, ValueLen);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CopyMemS() failed 0x%zx", Status);
        goto Exit;
    }

    //
    // Lastly, assign the file name for bookkeeping purposes.
    //

    FileInfo->FileName = AllocateZeroPool(FileNameLength + sizeof(CHAR8));
    if (FileInfo->FileName == NULL) {
        DBG_ERROR("Out of memory");
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    Status = CopyMemS(FileInfo->FileName, FileNameLength + sizeof(CHAR8), FileName, FileNameLength);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CopyMemS() failed 0x%zx", Status);
        goto Exit;
    }

    *DcatFileInfo = FileInfo;
Exit:

    if (EFI_ERROR(Status)) {
        if (FileInfo) {
            FreePool(FileInfo->FileName);
            FreePool(FileInfo->Url);

            FreePool(FileInfo);
        }
    }

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_DCAT_UNABLE_TO_PARSE_JSON);
    }

    return Status;
}

EFI_STATUS EFIAPI DcatFileInfoFree(_Inout_ DCAT_FILE_INFO* DcatFileInfo)
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (DcatFileInfo == NULL) {
        goto Exit;
    }

    FreePool(DcatFileInfo->FileName);
    FreePool(DcatFileInfo->Url);
    FreePool(DcatFileInfo);

Exit:
    return Status;
}

EFI_STATUS EFIAPI DcatExtractSizeFromFileInfo(_In_ DCAT_FILE_INFO* DcatFileInfo, _Out_ UINTN* Size)
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (DcatFileInfo == NULL) {
        DBG_ERROR("DcatFileInfo is NULL");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (Size == NULL) {
        DBG_ERROR("Invalid parameter");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    *Size = DcatFileInfo->Size;

Exit:
    return Status;
}

EFI_STATUS EFIAPI DcatExtractUrlFromFileInfo(_In_ DCAT_FILE_INFO* DcatFileInfo,
                                             _Outptr_ CHAR8** Url,
                                             _Out_ UINTN* UrlLength)
{
    EFI_STATUS Status = EFI_SUCCESS;
    CHAR8* RetUrl = NULL;

    if (DcatFileInfo == NULL) {
        DBG_ERROR("DcatFileInfo is NULL");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (DcatFileInfo->Url == NULL) {
        DBG_ERROR("DcatFileInfo->Url is NULL");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (Url == NULL || UrlLength == NULL) {
        DBG_ERROR("Invalid parameter");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    RetUrl = AllocateZeroPool(DcatFileInfo->UrlLength + sizeof(CHAR8));
    if (RetUrl == NULL) {
        DBG_ERROR("Out of memory");
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    Status = CopyMemS(RetUrl,
                      DcatFileInfo->UrlLength + sizeof(CHAR8),
                      DcatFileInfo->Url,
                      DcatFileInfo->UrlLength);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CopyMemS() failed 0x%zx", Status);
        goto Exit;
    }

    *Url = RetUrl;
    *UrlLength = DcatFileInfo->UrlLength;

Exit:
    return Status;
}

EFI_STATUS EFIAPI DcatExtractDigestFromFileInfo(_In_ DCAT_FILE_INFO* DcatFileInfo,
                                                _Out_writes_bytes_(HASH_LENGTH)
                                                    UINT8 Digest[HASH_LENGTH])
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (DcatFileInfo == NULL) {
        DBG_ERROR("DcatFileInfo is NULL");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (Digest == NULL) {
        DBG_ERROR("Invalid parameter");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    Status = CopyMemS(Digest, HASH_LENGTH, DcatFileInfo->Digest, HASH_LENGTH);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CopyMemS() failed 0x%zx", Status);
        goto Exit;
    }

Exit:
    return Status;
}

EFI_STATUS EFIAPI DcatFree(_Inout_ DCAT_CONTEXT* Context)
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (Context == NULL) {
        DBG_ERROR("Context is NULL");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (!Context->Initialized) {
        DBG_ERROR("Context has not been initialized");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    FreePool(Context->JsonBlob);

    Context->Initialized = FALSE;
    FreePool(Context);

Exit:
    return Status;
}

//
// Local functions
//

static EFI_STATUS EFIAPI DcatBuildRequestHeaders(_In_z_ CHAR8* Url,
                                                 _In_ UINTN BodyLength,
                                                 _In_z_ CHAR8* ContentType,
                                                 _In_ UINTN ContentTypeLength,
                                                 _Outptr_result_buffer_(Count)
                                                     EFI_HTTP_HEADER** Headers,
                                                 _Out_ UINTN* Count)
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINTN Result = 0;
    VOID* UrlParser = NULL;
    EFI_HTTP_HEADER* RequestHeaders = NULL;
    UINTN HeaderCount = 0;
    CHAR8 ContentLengthString[21]; // 2**64 is 1.8E19, or 20 digits. +1 for a NULL.

    if (Url == NULL || ContentType == NULL || Headers == NULL || Count == NULL ||
        ContentTypeLength == 0) {
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (BodyLength != 0) {
        HeaderCount = 5;
    } else {
        HeaderCount = 3;
    }

    RequestHeaders = AllocateZeroPool(sizeof(EFI_HTTP_HEADER) * HeaderCount); // Allocate headers
    if (NULL == RequestHeaders) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    UrlParser = NULL;

    Status = HttpParseUrl(Url,
                          (UINT32)AsciiStrnLenS(Url, MAX_JSON_REQUEST_URL_SIZE),
                          FALSE,
                          &UrlParser);
    if (EFI_ERROR(Status)) {
        goto Exit;
    }

    RequestHeaders[0].FieldName = AllocateCopyPool(_countof(HTTP_HEADER_HOST) + 1,
                                                   HTTP_HEADER_HOST);
    if (RequestHeaders[0].FieldName == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    RequestHeaders[1].FieldName = AllocateCopyPool(_countof(HTTP_HEADER_USER_AGENT) + 1,
                                                   HTTP_HEADER_USER_AGENT);
    if (RequestHeaders[1].FieldName == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    RequestHeaders[1].FieldValue = AllocateCopyPool(_countof(HEADER_AGENT_VALUE) + 1,
                                                    HEADER_AGENT_VALUE);
    if (RequestHeaders[1].FieldValue == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    RequestHeaders[2].FieldName = AllocateCopyPool(_countof(HTTP_HEADER_ACCEPT) + 1,
                                                   HTTP_HEADER_ACCEPT);
    if (RequestHeaders[2].FieldName == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    RequestHeaders[2].FieldValue = AllocateCopyPool(_countof(HEADER_ACCEPT_VALUE) + 1,
                                                    HEADER_ACCEPT_VALUE);
    if (RequestHeaders[2].FieldValue == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    Status = HttpUrlGetHostName(Url, UrlParser, (CHAR8**)(VOID**)&RequestHeaders[0].FieldValue);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Unable to get Host Name from URL");
        goto Exit;
    }

    if (0 != BodyLength) {
        RequestHeaders[3].FieldName = AllocateCopyPool(_countof(HTTP_HEADER_CONTENT_LENGTH) + 1,
                                                       HTTP_HEADER_CONTENT_LENGTH);
        if (RequestHeaders[3].FieldName == NULL) {
            Status = EFI_OUT_OF_RESOURCES;
            goto Exit;
        }

        Result = StringCchPrintfA((STRSAFE_LPSTR)ContentLengthString,
                                  _countof(ContentLengthString),
                                  (STRSAFE_LPCSTR) "%zu",
                                  BodyLength);
        if (FAILED(Result)) {
            DBG_ERROR("StringCchPrintfA failed 0x%zx", Result);
            Status = EFI_INVALID_PARAMETER;
            goto Exit;
        }

        RequestHeaders[3].FieldValue = AllocateCopyPool(_countof(ContentLengthString) + 1,
                                                        ContentLengthString);
        if (RequestHeaders[3].FieldValue == NULL) {
            Status = EFI_OUT_OF_RESOURCES;
            goto Exit;
        }

        RequestHeaders[4].FieldName = AllocateCopyPool(_countof(HTTP_HEADER_CONTENT_TYPE) + 1,
                                                       HTTP_HEADER_CONTENT_TYPE);
        if (RequestHeaders[4].FieldName == NULL) {
            Status = EFI_OUT_OF_RESOURCES;
            goto Exit;
        }

        RequestHeaders[4].FieldValue = AllocateCopyPool(ContentTypeLength + 1, ContentType);
        if (RequestHeaders[4].FieldValue == NULL) {
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

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_DCAT_UNABLE_TO_BUILD_JSON_REQUEST);
    }

    return Status;
}

#ifdef _WIN32
#pragma prefast(pop)
#endif