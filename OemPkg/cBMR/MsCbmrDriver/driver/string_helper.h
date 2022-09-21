#ifndef _STRING_HELPER_H_
#define _STRING_HELPER_H_

#include "cbmrincludes.h"

INT EFIAPI AsciiStrLastIndexOf(_In_z_ CHAR8* String, _In_ CHAR8 Ch);
INT EFIAPI StrLastIndexOf(_In_z_ CHAR16* String, _In_ CHAR16 Ch);

#endif // _STRING_HELPER_H_
