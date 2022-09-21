/*++

Copyright (c) 2021 Microsoft Corporation

Module Name:

    utils.c

Abstract:

    This module abstracts the difference in printf* family of functions b/w
    Windows and UEFI build system

Author:

    Vineel Kovvuri (vineelko) 12-Jun-2021

Environment:

    UEFI mode only.

--*/
#include "cbmrincludes.h"

#include "utils.h"

#ifdef UEFI_BUILD_SYSTEM

struct {
    CHAR16* Key;
    CHAR16* Value;
} ReplacementsW[] = {
    {L"%zx", L"%x"},
    {L"%zu", L"%u"},
    {L"%zd", L"%d"},
};

static CHAR16* GetSanitizeFormatStringW(IN CONST CHAR16* String, IN UINTN MaxStringLength)
{
    EFI_STATUS Status = EFI_SUCCESS;
    CHAR16* Result = NULL;
    UINTN ResultSize = 0;

    // Allocating twice the size of the array
    ResultSize = 2 * sizeof(CHAR16) * StrnLenS(String, MaxStringLength);
    Result = AllocateZeroPool(ResultSize);
    if (Result == NULL) {
        return Result;
    }

    ZeroMem(Result, ResultSize);

    for (UINTN j = 0, k = 0; String[j];) {
        UINTN i = 0;
        for (; i < _countof(ReplacementsW); i++) {
            CHAR16* Key = ReplacementsW[i].Key;
            CHAR16* Value = ReplacementsW[i].Value;
            if (CompareMem(&String[j], Key, StrLen(Key) * sizeof(CHAR16)) == 0) {
                Status = CopyMemS(&Result[k],
                                  ResultSize - sizeof(CHAR16) * k,
                                  Value,
                                  StrLen(Value) * sizeof(CHAR16));
                if (EFI_ERROR(Status)) {
                    FreePool(Result);
                    Result = NULL;
                    return Result;
                }

                j += StrLen(Key);
                k += StrLen(Value);
                break;
            }
        }

        if (i >= _countof(ReplacementsW)) {
            Result[k++] = String[j++];
        }
    }

    return Result;
}

struct {
    CHAR8* Key;
    CHAR8* Value;
} ReplacementsA[] = {
    {"%s", "%a"},
    {"%zx", "%x"},
    {"%zu", "%u"},
    {"%zd", "%d"},
};

static CHAR8* GetSanitizeFormatStringA(IN CONST CHAR8* String, IN UINTN MaxStringLength)
{
    EFI_STATUS Status = EFI_SUCCESS;
    CHAR8* Result = NULL;
    UINTN ResultSize = 0;

    // Allocating twice the size of the array
    ResultSize = 2 * sizeof(CHAR8) * AsciiStrnLenS(String, MaxStringLength);
    Result = AllocateZeroPool(ResultSize);
    if (Result == NULL) {
        return Result;
    }

    ZeroMem(Result, 2 * sizeof(CHAR8) * AsciiStrnLenS(String, MaxStringLength));

    for (UINTN j = 0, k = 0; String[j];) {
        UINTN i = 0;
        for (; i < _countof(ReplacementsA); i++) {
            CHAR8* Key = ReplacementsA[i].Key;
            CHAR8* Value = ReplacementsA[i].Value;
            if (CompareMem(&String[j], Key, AsciiStrnLenS(Key, 8) * sizeof(CHAR8)) == 0) {
                Status = CopyMemS(&Result[k],
                                  ResultSize - k,
                                  Value,
                                  AsciiStrnLenS(Value, 8) * sizeof(CHAR8));
                if (EFI_ERROR(Status)) {
                    FreePool(Result);
                    Result = NULL;
                    return Result;
                }

                j += AsciiStrnLenS(Key, 8);
                k += AsciiStrnLenS(Value, 8);
                break;
            }
        }

        if (i >= _countof(ReplacementsA)) {
            Result[k++] = String[j++];
        }
    }

    return Result;
}

EFI_STATUS
EFIAPI
StringPrintfW(OUT CHAR16* Buffer, IN UINTN CharCount, IN CONST CHAR16* FormatString, ...)
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINTN Result = 0;
    VA_LIST ArgList;
    CHAR16* FormatString2 = NULL;

    VA_START(ArgList, FormatString);
    FormatString2 = GetSanitizeFormatStringW(FormatString, CharCount);
    Result = UnicodeVSPrint(Buffer, CharCount * sizeof(CHAR16), (CHAR16*)FormatString2, ArgList);
    VA_END(ArgList);

    FreePool(FormatString2);

    if (Result == 0) {
        Status = EFI_INVALID_PARAMETER;
    }

    return Status;
}

EFI_STATUS
EFIAPI
StringPrintfA(OUT CHAR8* Buffer, IN UINTN CharCount, IN CONST CHAR8* FormatString, ...)
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINTN Result = 0;
    VA_LIST ArgList;
    CHAR8* FormatString2 = NULL;

    VA_START(ArgList, FormatString);
    FormatString2 = GetSanitizeFormatStringA(FormatString, CharCount);
    Result = AsciiVSPrint(Buffer, CharCount * sizeof(CHAR8), (CHAR8*)FormatString2, ArgList);
    VA_END(ArgList);

    FreePool(FormatString2);

    if (Result == 0) {
        Status = EFI_INVALID_PARAMETER;
    }

    return Status;
}

EFI_STATUS
EFIAPI
StringVPrintfW(OUT CHAR16* Buffer,
               IN UINTN CharCount,
               IN CONST CHAR16* FormatString,
               VA_LIST ArgList)
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINTN Result = 0;
    CHAR16* FormatString2 = NULL;

    FormatString2 = GetSanitizeFormatStringW(FormatString, CharCount);

    Result = UnicodeVSPrint(Buffer, CharCount * sizeof(CHAR16), (CHAR16*)FormatString2, ArgList);

    FreePool(FormatString2);

    if (Result == 0) {
        Status = EFI_INVALID_PARAMETER;
    }

    return Status;
}

EFI_STATUS
EFIAPI
StringVPrintfA(OUT CHAR8* Buffer, IN UINTN CharCount, IN CONST CHAR8* FormatString, VA_LIST ArgList)
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINTN Result = 0;
    CHAR8* FormatString2 = NULL;

    FormatString2 = GetSanitizeFormatStringA(FormatString, CharCount);

    Result = AsciiVSPrint(Buffer, CharCount * sizeof(CHAR8), (CHAR8*)FormatString2, ArgList);

    FreePool(FormatString2);

    if (Result == 0) {
        Status = EFI_INVALID_PARAMETER;
    }

    return Status;
}

#else
EFI_STATUS
EFIAPI
StringPrintfW(OUT CHAR16* Buffer, IN UINTN CharCount, IN CONST CHAR16* FormatString, ...)
{
    EFI_STATUS Status = EFI_SUCCESS;
    HRESULT Hr;
    va_list ArgList;

    va_start(ArgList, FormatString);
    Hr = StringCchVPrintfW((STRSAFE_LPWSTR)Buffer, CharCount, (PCWSTR)FormatString, ArgList);
    va_end(ArgList);
    if (FAILED(Hr)) {
        // StringCchPrintfW((STRSAFE_LPWSTR)Buffer,
        //                  CharCount,
        //                  L"Cannot print message, HRESULT 0x%08x",
        //                  Hr);
        Status = EFI_INVALID_PARAMETER;
        // DBG_CMD_RAW_U(L"\r\nStringCchPrintfW Failed\r\n");
    }

    return Status;
}

EFI_STATUS
EFIAPI
StringPrintfA(OUT CHAR8* Buffer, IN UINTN CharCount, IN CONST CHAR8* FormatString, ...)
{
    EFI_STATUS Status = EFI_SUCCESS;
    HRESULT Hr;
    va_list ArgList;

    va_start(ArgList, FormatString);
    Hr = StringCchVPrintfA((STRSAFE_LPSTR)Buffer, CharCount, (PCSTR)FormatString, ArgList);
    va_end(ArgList);
    if (FAILED(Hr)) {
        // StringCchPrintfA((STRSAFE_LPSTR)Buffer,
        //                  CharCount,
        //                  "Cannot print message, HRESULT 0x%08x",
        //                  Hr);
        Status = EFI_INVALID_PARAMETER;
        // DBG_CMD_RAW_U(L"\r\nStringCchPrintfA Failed\r\n");
    }

    return Status;
}

EFI_STATUS
EFIAPI
StringVPrintfW(OUT CHAR16* Buffer,
               IN UINTN CharCount,
               IN CONST CHAR16* FormatString,
               va_list ArgList)
{
    EFI_STATUS Status = EFI_SUCCESS;
    HRESULT Hr;

    Hr = StringCchVPrintfW((STRSAFE_LPWSTR)Buffer, CharCount, (PCWSTR)FormatString, ArgList);
    if (FAILED(Hr)) {
        // StringCchPrintfW((STRSAFE_LPWSTR)Buffer,
        //                  CharCount,
        //                  L"Cannot print message, HRESULT 0x%08x",
        //                  Hr);
        Status = EFI_INVALID_PARAMETER;
        // DBG_CMD_RAW_U(L"\r\nStringCchPrintfW Failed\r\n");
    }

    return Status;
}

EFI_STATUS
EFIAPI
StringVPrintfA(OUT CHAR8* Buffer, IN UINTN CharCount, IN CONST CHAR8* FormatString, va_list ArgList)
{
    EFI_STATUS Status = EFI_SUCCESS;
    HRESULT Hr;

    Hr = StringCchVPrintfA((STRSAFE_LPSTR)Buffer, CharCount, (PCSTR)FormatString, ArgList);
    if (FAILED(Hr)) {
        // StringCchPrintfA((STRSAFE_LPSTR)Buffer,
        //                  CharCount,
        //                  "Cannot print message, HRESULT 0x%08x",
        //                  Hr);
        Status = EFI_INVALID_PARAMETER;
        // DBG_CMD_RAW_U(L"\r\nStringCchPrintfA Failed\r\n");
    }

    return Status;
}
#endif

//
// VM detection utils
//

static CHAR8* StringByIndex(_In_ CHAR8* StartAddrPtr, _In_ UINT8 StringIndex)
{
    CHAR8* StringPtr = StartAddrPtr;
    UINT8 Index = 1;
    UINTN StringLength = 0;

    if (StartAddrPtr == NULL) {
        DBG_ERROR("Invalid StartAddrPtr: 0x%p", StartAddrPtr);
        return NULL;
    }

    if (StringIndex == 0) {
        DBG_ERROR("Invalid StringIndex: %u", StringIndex);
        return NULL;
    }

    while (Index < StringIndex) {
#pragma prefast(push)
#pragma prefast(disable : 26018)
#pragma prefast(disable : 26007)
        if (*StringPtr == 0 && *(StringPtr + 1) == 0) {
            DBG_ERROR("Structure terminator found while searching index %u", Index);
            return NULL;
        }
#pragma prefast(pop)

        StringLength = AsciiStrLen(StringPtr) + 1;
        StringPtr += StringLength;
        Index++;
    }

    return StringPtr;
}

BOOLEAN
IsRunningInVM(VOID)
{
    static BOOLEAN IsRunningInVM = FALSE;

    EFI_STATUS Status = EFI_SUCCESS;
    EFI_SMBIOS_PROTOCOL* SmbiosProtocol = NULL;
    EFI_SMBIOS_HANDLE SmbiosHandle = 0;
    EFI_SMBIOS_TYPE SmbiosType = EFI_SMBIOS_TYPE_SYSTEM_INFORMATION;
    SMBIOS_TABLE_TYPE1* SmbiosTableType1Ptr = NULL;
    CHAR8* StringPtr = NULL;
    CHAR8* ProductName = NULL;
    CHAR8* Version = NULL;
    CHAR8* Family = NULL;

    if (IsRunningInVM == TRUE) {
        return IsRunningInVM;
    }

    Status = gBS->LocateProtocol(&gEfiSmbiosProtocolGuid, NULL, (PVOID*)&SmbiosProtocol);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("LocateProtocol() failed : 0x%zx", Status);
        goto Exit;
    }

    Status = SmbiosProtocol->GetNext(SmbiosProtocol,
                                     &SmbiosHandle,
                                     &SmbiosType,
                                     (EFI_SMBIOS_TABLE_HEADER**)&SmbiosTableType1Ptr,
                                     NULL);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Smbios GetNext() failed : 0x%zx", Status);
        goto Exit;
    }

    StringPtr = ((CHAR8*)SmbiosTableType1Ptr) + SmbiosTableType1Ptr->Hdr.Length;

    ProductName = StringByIndex(StringPtr, SmbiosTableType1Ptr->ProductName);
    Version = StringByIndex(StringPtr, SmbiosTableType1Ptr->Version);
    Family = StringByIndex(StringPtr, SmbiosTableType1Ptr->Family);

    if (AsciiStrStr(ProductName, t("Virtual Machine")) != NULL ||
        AsciiStrStr(Version, t("Hyper-V")) != NULL ||
        AsciiStrStr(Family, t("Virtual Machine")) != NULL) {
        IsRunningInVM = TRUE;
    }

Exit:
    return IsRunningInVM;
}

UINT64
TimeDiff(_In_ EFI_TIME* StartTime,
         _In_ EFI_TIME* EndTime,
         _Inout_ UINTN* Hours,
         _Inout_ UINTN* Minutes,
         _Inout_ UINTN* Seconds)
{
    UINT64 Result = 0;
    UINT64 RetResult = 0;

    Result += (EndTime->Hour - StartTime->Hour) * 60 * 60; // * NANOSECONDS;
    Result += (EndTime->Minute - StartTime->Minute) * 60;  // * NANOSECONDS;
    Result += (EndTime->Second - StartTime->Second);       // * NANOSECONDS;
    //    Result += ABS(EndTime->Nanosecond, StartTime->Nanosecond);

    RetResult = Result;
    *Hours = (UINTN)(Result / 3600);
    Result %= 3600;
    *Minutes = (UINTN)(Result / 60);
    Result %= 60;
    *Seconds = (UINTN)Result;

    return RetResult;
}

UINT64 PrettySize(_In_ UINT64 Size)
{
    if (Size > TB)
        return Size / TB;
    else if (Size > GB)
        return Size / GB;
    else if (Size > MB)
        return Size / MB;
    else if (Size > KB)
        return Size / KB;
    return Size;
}

CHAR8* PrettySizeStr(_In_ UINT64 Size)
{
    static CHAR8* Str[5] = {
        t("Bytes"),
        t("KB"),
        t("MB"),
        t("GB"),
        t("TB"),
    };

    if (Size > TB)
        return Str[4];
    else if (Size > GB)
        return Str[3];
    else if (Size > MB)
        return Str[2];
    else if (Size > KB)
        return Str[1];
    return Str[0];
}

CHAR16* GetDomain(_In_ CHAR16* Url)
{
    CHAR16* Ptr = StrStr(Url, L"//");
    if (Ptr == NULL)
        return NULL;

    Ptr = StrChr(Ptr + 2, L'/');
    if (Ptr == NULL)
        return NULL;

    CHAR16* Str = AllocateZeroPool(((Ptr - Url) + 1) * sizeof(CHAR16));
    return StrnCpy(Str, Url, (Ptr - Url));
}

CHAR16* GetFileName(_In_ CHAR16* Path)
{
    UINTN Len = StrLen(Path);

    for (UINTN i = Len - 1; i >= 0; i--) {
        if (Path[i] == L'/' || Path[i] == L'\\') {
            CHAR16* Str = NULL;
            StrDup(&Path[i + 1], &Str);
            return Str;
        }
    }

    return NULL;
}

//
// Keyboard related
//

EFI_INPUT_KEY GetCharNoEcho()
{
    EFI_INPUT_KEY Key;
    UINTN Index = 0;

    gBS->WaitForEvent(1, &gSystemTable->ConIn->WaitForKey, &Index);
    gSystemTable->ConIn->ReadKeyStroke(gSystemTable->ConIn, &Key);
    // DBG_INFO("ScanCode = %d UnicodeChar = %d", Key.ScanCode, Key.UnicodeChar);
    return Key;
}