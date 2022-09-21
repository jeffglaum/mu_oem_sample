/*++

Copyright (c) 2021  Microsoft Corporation

Module Name:

    utils.c

Abstract:

    This module implements miscellaneous string helper functions

Author:

    Jancarlo Perez (jpere) 26-Oct-2021

Environment:

    UEFI mode only.

--*/

#include "string_helper.h"

INT EFIAPI AsciiStrLastIndexOf(_In_z_ CHAR8* String, _In_ CHAR8 Ch)
{
    INT LastIndex = -1;
    INT CurrentIndex = 0;
    CHAR8* TempString = String;

    while (*TempString != '\0') {
        if (*TempString == Ch) {
            LastIndex = CurrentIndex;
        }

        TempString++;
        CurrentIndex++;
    }

    return LastIndex;
}

INT EFIAPI StrLastIndexOf(_In_z_ CHAR16* String, _In_ CHAR16 Ch)
{
    INT LastIndex = -1;
    INT CurrentIndex = 0;
    CHAR16* TempString = String;

    while (*TempString != L'\0') {
        if (*TempString == Ch) {
            LastIndex = CurrentIndex;
        }

        TempString++;
        CurrentIndex++;
    }

    return LastIndex;
}