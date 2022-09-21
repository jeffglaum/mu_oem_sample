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
#ifndef UEFI_BUILD_SYSTEM
#include "strsafe.h"
#endif
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
            DBG_ERROR("NUL char found, exit early", NULL); \
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
        DBG_ERROR("Out of memory", NULL);
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
        DBG_ERROR("Invalid parameter", NULL);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    BodyLength = AsciiStrnLenS(RequestJson, MAX_JSON_REQUEST_SIZE);

    AsciiUrl = AllocateZeroPool(StrnLenS(Url, MAX_JSON_REQUEST_URL_SIZE) + sizeof(CHAR8));
    if (AsciiUrl == NULL) {
        DBG_ERROR("Out of memory", NULL);
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

    DBG_INFO("Sending request to DCAT", NULL);
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
        DBG_ERROR("Unable to allocate memory", NULL);
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
    DBG_INFO("JSON blob successfully obtained from DCAT", NULL);

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


/**
  Decode Base64 ASCII encoded data to 8-bit binary representation, based on
  RFC4648.

  Decoding occurs according to "Table 1: The Base 64 Alphabet" in RFC4648.

  Whitespace is ignored at all positions:
  - 0x09 ('\t') horizontal tab
  - 0x0A ('\n') new line
  - 0x0B ('\v') vertical tab
  - 0x0C ('\f') form feed
  - 0x0D ('\r') carriage return
  - 0x20 (' ')  space

  The minimum amount of required padding (with ASCII 0x3D, '=') is tolerated
  and enforced at the end of the Base64 ASCII encoded data, and only there.

  Other characters outside of the encoding alphabet cause the function to
  reject the Base64 ASCII encoded data.

  @param[in] Source               Array of CHAR8 elements containing the Base64
                                  ASCII encoding. May be NULL if SourceSize is
                                  zero.

  @param[in] SourceSize           Number of CHAR8 elements in Source.

  @param[out] Destination         Array of UINT8 elements receiving the decoded
                                  8-bit binary representation. Allocated by the
                                  caller. May be NULL if DestinationSize is
                                  zero on input. If NULL, decoding is
                                  performed, but the 8-bit binary
                                  representation is not stored. If non-NULL and
                                  the function returns an error, the contents
                                  of Destination are indeterminate.

  @param[in,out] DestinationSize  On input, the number of UINT8 elements that
                                  the caller allocated for Destination. On
                                  output, if the function returns
                                  EFI_SUCCESS or EFI_BUFFER_TOO_SMALL,
                                  the number of UINT8 elements that are
                                  required for decoding the Base64 ASCII
                                  representation. If the function returns a
                                  value different from both EFI_SUCCESS and
                                  EFI_BUFFER_TOO_SMALL, then DestinationSize
                                  is indeterminate on output.

  @retval EFI_SUCCESS            SourceSize CHAR8 elements at Source have
                                    been decoded to on-output DestinationSize
                                    UINT8 elements at Destination. Note that
                                    EFI_SUCCESS covers the case when
                                    DestinationSize is zero on input, and
                                    Source decodes to zero bytes (due to
                                    containing at most ignored whitespace).

  @retval EFI_BUFFER_TOO_SMALL   The input value of DestinationSize is not
                                    large enough for decoding SourceSize CHAR8
                                    elements at Source. The required number of
                                    UINT8 elements has been stored to
                                    DestinationSize.

  @retval EFI_INVALID_PARAMETER  DestinationSize is NULL.

  @retval EFI_INVALID_PARAMETER  Source is NULL, but SourceSize is not zero.

  @retval EFI_INVALID_PARAMETER  Destination is NULL, but DestinationSize is
                                    not zero on input.

  @retval EFI_INVALID_PARAMETER  Source is non-NULL, and (Source +
                                    SourceSize) would wrap around MAX_ADDRESS.

  @retval EFI_INVALID_PARAMETER  Destination is non-NULL, and (Destination +
                                    DestinationSize) would wrap around
                                    MAX_ADDRESS, as specified on input.

  @retval EFI_INVALID_PARAMETER  None of Source and Destination are NULL,
                                    and CHAR8[SourceSize] at Source overlaps
                                    UINT8[DestinationSize] at Destination, as
                                    specified on input.

  @retval EFI_INVALID_PARAMETER  Invalid CHAR8 element encountered in
                                    Source.
**/
EFI_STATUS EFIAPI Base64DecodeEdk(IN CONST CHAR8* Source OPTIONAL,
                                  IN UINTN SourceSize,
                                  OUT UINT8* Destination OPTIONAL,
                                  IN OUT UINTN* DestinationSize)
{
    BOOLEAN PaddingMode;
    UINTN SixBitGroupsConsumed;
    UINT32 Accumulator;
    UINTN OriginalDestinationSize;
    UINTN SourceIndex;
    CHAR8 SourceChar;
    UINT32 Base64Value;
    UINT8 DestinationOctet;

    if (DestinationSize == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Check Source array validity.
    //
    if (Source == NULL) {
        if (SourceSize > 0) {
            //
            // At least one CHAR8 element at NULL Source.
            //
            return EFI_INVALID_PARAMETER;
        }
    } else if (SourceSize > MAX_ADDRESS - (UINTN)Source) {
        //
        // Non-NULL Source, but it wraps around.
        //
        return EFI_INVALID_PARAMETER;
    }

    //
    // Check Destination array validity.
    //
    if (Destination == NULL) {
        if (*DestinationSize > 0) {
            //
            // At least one UINT8 element at NULL Destination.
            //
            return EFI_INVALID_PARAMETER;
        }
    } else if (*DestinationSize > MAX_ADDRESS - (UINTN)Destination) {
        //
        // Non-NULL Destination, but it wraps around.
        //
        return EFI_INVALID_PARAMETER;
    }

    //
    // Check for overlap.
    //
    if (Source != NULL && Destination != NULL) {
        //
        // Both arrays have been provided, and we know from earlier that each array
        // is valid in itself.
        //
        if ((UINTN)Source + SourceSize <= (UINTN)Destination) {
            //
            // Source array precedes Destination array, OK.
            //
        } else if ((UINTN)Destination + *DestinationSize <= (UINTN)Source) {
            //
            // Destination array precedes Source array, OK.
            //
        } else {
            //
            // Overlap.
            //
            return EFI_INVALID_PARAMETER;
        }
    }

    //
    // Decoding loop setup.
    //
    PaddingMode = FALSE;
    SixBitGroupsConsumed = 0;
    Accumulator = 0;
    OriginalDestinationSize = *DestinationSize;
    *DestinationSize = 0;

    //
    // Decoding loop.
    //
    for (SourceIndex = 0; SourceIndex < SourceSize; SourceIndex++) {
        SourceChar = Source[SourceIndex];

        //
        // Whitespace is ignored at all positions (regardless of padding mode).
        //
        if (SourceChar == '\t' || SourceChar == '\n' || SourceChar == '\v' || SourceChar == '\f' ||
            SourceChar == '\r' || SourceChar == ' ') {
            continue;
        }

        //
        // If we're in padding mode, accept another padding character, as long as
        // that padding character completes the quantum. This completes case (2)
        // from RFC4648, Chapter 4. "Base 64 Encoding":
        //
        // (2) The final quantum of encoding input is exactly 8 bits; here, the
        //     final unit of encoded output will be two characters followed by two
        //     "=" padding characters.
        //
        if (PaddingMode) {
            if (SourceChar == '=' && SixBitGroupsConsumed == 3) {
                SixBitGroupsConsumed = 0;
                continue;
            }
            return EFI_INVALID_PARAMETER;
        }

        //
        // When not in padding mode, decode Base64Value based on RFC4648, "Table 1:
        // The Base 64 Alphabet".
        //
        if ('A' <= SourceChar && SourceChar <= 'Z') {
            Base64Value = SourceChar - 'A';
        } else if ('a' <= SourceChar && SourceChar <= 'z') {
            Base64Value = 26 + (SourceChar - 'a');
        } else if ('0' <= SourceChar && SourceChar <= '9') {
            Base64Value = 52 + (SourceChar - '0');
        } else if (SourceChar == '+') {
            Base64Value = 62;
        } else if (SourceChar == '/') {
            Base64Value = 63;
        } else if (SourceChar == '=') {
            //
            // Enter padding mode.
            //
            PaddingMode = TRUE;

            if (SixBitGroupsConsumed == 2) {
                //
                // If we have consumed two 6-bit groups from the current quantum before
                // encountering the first padding character, then this is case (2) from
                // RFC4648, Chapter 4. "Base 64 Encoding". Bump SixBitGroupsConsumed,
                // and we'll enforce another padding character.
                //
                SixBitGroupsConsumed = 3;
            } else if (SixBitGroupsConsumed == 3) {
                //
                // If we have consumed three 6-bit groups from the current quantum
                // before encountering the first padding character, then this is case
                // (3) from RFC4648, Chapter 4. "Base 64 Encoding". The quantum is now
                // complete.
                //
                SixBitGroupsConsumed = 0;
            } else {
                //
                // Padding characters are not allowed at the first two positions of a
                // quantum.
                //
                return EFI_INVALID_PARAMETER;
            }

            //
            // Wherever in a quantum we enter padding mode, we enforce the padding
            // bits pending in the accumulator -- from the last 6-bit group just
            // preceding the padding character -- to be zero. Refer to RFC4648,
            // Chapter 3.5. "Canonical Encoding".
            //
            if (Accumulator != 0) {
                return EFI_INVALID_PARAMETER;
            }

            //
            // Advance to the next source character.
            //
            continue;
        } else {
            //
            // Other characters outside of the encoding alphabet are rejected.
            //
            return EFI_INVALID_PARAMETER;
        }

        //
        // Feed the bits of the current 6-bit group of the quantum to the
        // accumulator.
        //
        Accumulator = (Accumulator << 6) | Base64Value;
        SixBitGroupsConsumed++;
        switch (SixBitGroupsConsumed) {
            case 1:
                //
                // No octet to spill after consuming the first 6-bit group of the
                // quantum; advance to the next source character.
                //
                continue;
            case 2:
                //
                // 12 bits accumulated (6 pending + 6 new); prepare for spilling an
                // octet. 4 bits remain pending.
                //
                DestinationOctet = (UINT8)(Accumulator >> 4);
                Accumulator &= 0xF;
                break;
            case 3:
                //
                // 10 bits accumulated (4 pending + 6 new); prepare for spilling an
                // octet. 2 bits remain pending.
                //
                DestinationOctet = (UINT8)(Accumulator >> 2);
                Accumulator &= 0x3;
                break;
            default:
                ASSERT(SixBitGroupsConsumed == 4);
                //
                // 8 bits accumulated (2 pending + 6 new); prepare for spilling an octet.
                // The quantum is complete, 0 bits remain pending.
                //
                DestinationOctet = (UINT8)Accumulator;
                Accumulator = 0;
                SixBitGroupsConsumed = 0;
                break;
        }

        //
        // Store the decoded octet if there's room left. Increment
        // (*DestinationSize) unconditionally.
        //
        if (*DestinationSize < OriginalDestinationSize) {
            ASSERT(Destination != NULL);
            Destination[*DestinationSize] = DestinationOctet;
        }
        (*DestinationSize)++;

        //
        // Advance to the next source character.
        //
    }

    //
    // If Source terminates mid-quantum, then Source is invalid.
    //
    if (SixBitGroupsConsumed != 0) {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Done.
    //
    if (*DestinationSize <= OriginalDestinationSize) {
        return EFI_SUCCESS;
    }
    return EFI_BUFFER_TOO_SMALL;
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
        DBG_ERROR("Invalid parameter", NULL);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (!Context->Initialized) {
        DBG_ERROR("Context is not initialized", NULL);
        Status = EFI_NOT_READY;
        goto Exit;
    }

    //
    // Perform a very rudimentary JSON parse, if it can even be called that.
    //

    FileInfo = AllocateZeroPool(sizeof(DCAT_FILE_INFO));
    if (FileInfo == NULL) {
        DBG_ERROR("Out of memory", NULL);
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    //
    // First, find FileName substring. JsonBlob is guaranteed to be NUL terminated.
    //

    StringMatch = AsciiStrStr((CHAR8*)Context->JsonBlob, FileName);
    if (StringMatch == NULL) {
        DBG_ERROR("No file match in JSON blob", NULL);
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
        DBG_ERROR("No : character found", NULL);
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
        DBG_ERROR("No : character found", NULL);
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
        DBG_ERROR("No : character found", NULL);
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
        DBG_ERROR("Out of memory", NULL);
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
        DBG_ERROR("Out of memory", NULL);
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
        DBG_ERROR("DcatFileInfo is NULL", NULL);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (Size == NULL) {
        DBG_ERROR("Invalid parameter", NULL);
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
        DBG_ERROR("DcatFileInfo is NULL", NULL);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (DcatFileInfo->Url == NULL) {
        DBG_ERROR("DcatFileInfo->Url is NULL", NULL);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (Url == NULL || UrlLength == NULL) {
        DBG_ERROR("Invalid parameter", NULL);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    RetUrl = AllocateZeroPool(DcatFileInfo->UrlLength + sizeof(CHAR8));
    if (RetUrl == NULL) {
        DBG_ERROR("Out of memory", NULL);
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
        DBG_ERROR("DcatFileInfo is NULL", NULL);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (Digest == NULL) {
        DBG_ERROR("Invalid parameter", NULL);
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
        DBG_ERROR("Context is NULL", NULL);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (!Context->Initialized) {
        DBG_ERROR("Context has not been initialized", NULL);
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
        DBG_ERROR("Unable to get Host Name from URL", NULL);
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
            DBG_ERROR("Out of memory", NULL);
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