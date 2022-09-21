/*++

Copyright (c) 2021 Microsoft Corporation

Module Name:

    utils.h

Abstract:

    This module implements format string print functions fixups b/w UEFI and Windows builds system

Author:

    Vineel Kovvuri (vineelko) 12-Jun-2021

Environment:

    UEFI mode only.

--*/
#ifndef _UTILS_H_
#define _UTILS_H_

#define STRING_LEN(string) ((_countof(string)) - 1)

EFI_STATUS
EFIAPI
StringPrintfW(OUT CHAR16* Buffer, IN UINTN CharCount, IN CONST CHAR16* FormatString, ...);

EFI_STATUS
EFIAPI
StringPrintfA(OUT CHAR8* Buffer, IN UINTN CharCount, IN CONST CHAR8* FormatString, ...);

EFI_STATUS
EFIAPI
StringVPrintfW(OUT CHAR16* Buffer,
               IN UINTN CharCount,
               IN CONST CHAR16* FormatString,
               va_list ArgList);

EFI_STATUS
EFIAPI
StringVPrintfA(OUT CHAR8* Buffer,
               IN UINTN CharCount,
               IN CONST CHAR8* FormatString,
               va_list ArgList);

BOOLEAN
IsRunningInVM(VOID);

UINT64
TimeDiff(_In_ EFI_TIME* StartTime,
         _In_ EFI_TIME* EndTime,
         _Inout_ UINTN* Hours,
         _Inout_ UINTN* Minutes,
         _Inout_ UINTN* Seconds);

UINT64 PrettySize(_In_ UINT64 Size);
CHAR8* PrettySizeStr(_In_ UINT64 Size);
CHAR16* GetDomain(_In_ CHAR16* Url);
CHAR16* GetFileName(_In_ CHAR16* Path);

// Keyboard related
EFI_INPUT_KEY GetCharNoEcho();
#endif // _UTILS_H_