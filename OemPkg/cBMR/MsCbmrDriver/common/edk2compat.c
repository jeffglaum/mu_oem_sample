#include "cbmrincludes.h"

#ifndef UEFI_BUILD_SYSTEM
#include <strsafe.h>

#if defined(_X86_)
#define MAX_INTN  ((INTN)0x7FFFFFFF)
#define MAX_UINTN ((UINTN)0xFFFFFFFF)
#elif defined(_AMD64_)
#define MAX_INTN  ((INTN)0x7FFFFFFFFFFFFFFFULL)
#define MAX_UINTN ((UINTN)0xFFFFFFFFFFFFFFFFULL)
#elif defined(_ARM_)
#define MAX_INTN  ((INTN)0x7FFFFFFF)
#define MAX_UINTN ((UINTN)0xFFFFFFFF)
#elif defined(_ARM64_)
#define MAX_INTN  ((INTN)0x7FFFFFFFFFFFFFFFULL)
#define MAX_UINTN ((UINTN)0xFFFFFFFFFFFFFFFFULL)
#else
#error Architecture not supported.
#endif
#endif

#ifndef UEFI_BUILD_SYSTEM
#pragma prefast(push)
#pragma prefast(disable : 26000, "Ignore false positive overflow warnings")
#endif

#define BIT(x) (1 << x)

#define HTTP_VERSION_CRLF_STR " HTTP/1.1\r\n"
#define EMPTY_SPACE           " "

#define NET_IS_HEX_CHAR(Ch)                                                  \
    ((('0' <= (Ch)) && ((Ch) <= '9')) || (('A' <= (Ch)) && ((Ch) <= 'F')) || \
     (('a' <= (Ch)) && ((Ch) <= 'f')))

#define NET_IS_DIGIT(Ch) (('0' <= (Ch)) && ((Ch) <= '9'))

//
// Field index of the HTTP URL parse result.
//
#define HTTP_URI_FIELD_SCHEME    0
#define HTTP_URI_FIELD_AUTHORITY 1
#define HTTP_URI_FIELD_PATH      2
#define HTTP_URI_FIELD_QUERY     3
#define HTTP_URI_FIELD_FRAGMENT  4
#define HTTP_URI_FIELD_USERINFO  5
#define HTTP_URI_FIELD_HOST      6
#define HTTP_URI_FIELD_PORT      7
#define HTTP_URI_FIELD_MAX       8

#define HTTP_URI_PORT_MAX_NUM 65535

static BOOLEAN InternalIsSpaceAsciiCharacter(IN CHAR8 Char);
static BOOLEAN InternalIsSpaceCharacter(IN CHAR16 Char);

#define SAFE_STRING_CONSTRAINT_CHECK(Expression, Status) \
    do {                                                 \
        ASSERT(Expression);                              \
        if (!(Expression)) {                             \
            return Status;                               \
        }                                                \
    } while (FALSE)

#define MAX_UNICODE_STRING_LEN 0x10000000
#define MAX_ASCII_STRING_LEN   0x10000000

#ifndef UEFI_BUILD_SYSTEM
UINTN
EFIAPI
AsciiStrnLenS(IN CONST CHAR8* String, IN UINTN MaxSize)
{
    UINTN Length;

    //
    // If String is a null pointer or MaxSize is 0, then the AsciiStrnLenS function returns zero.
    //
    if ((String == NULL) || (MaxSize == 0)) {
        return 0;
    }

    //
    // Otherwise, the AsciiStrnLenS function returns the number of characters that precede the
    // terminating null character. If there is no null character in the first MaxSize characters of
    // String then AsciiStrnLenS returns MaxSize. At most the first MaxSize characters of String
    // shall be accessed by AsciiStrnLenS.
    //
    Length = 0;
    while (String[Length] != 0) {
        if (Length >= MaxSize - 1) {
            return MaxSize;
        }
        Length++;
    }
    return Length;
}
#endif

// Declared in MdePkg\Include\Library\BaseMemoryLib.h
#ifdef UEFI_BUILD_SYSTEM
void* memset(void* dest, char ch, unsigned int count)
{
    return SetMem(dest, count, ch);
}
#endif

#ifndef UEFI_BUILD_SYSTEM
VOID* EFIAPI CopyMem(OUT VOID* DestinationBuffer, IN const VOID* SourceBuffer, IN UINTN Length)
{
    return memcpy(DestinationBuffer, SourceBuffer, Length);
}

VOID* EFIAPI MoveMem(OUT VOID* DestinationBuffer, IN const VOID* SourceBuffer, IN UINTN Length)
{
    return memmove(DestinationBuffer, SourceBuffer, Length);
}

VOID* EFIAPI SetMem(OUT VOID* Buffer, IN UINTN Length, IN UINT8 Value)
{
    return memset(Buffer, Value, Length);
}

VOID* EFIAPI ZeroMem(OUT VOID* Buffer, IN UINTN Length)
{
    ASSERT(!(Buffer == NULL && Length > 0));
    ASSERT(Length <= (MAX_ADDRESS - (UINTN)Buffer + 1));

    return SetMem(Buffer, Length, 0);
}

UINT64
EFIAPI
ReadUnaligned64(IN const UINT64* Buffer)
{
    ASSERT(Buffer != NULL);
    return *Buffer;
}

BOOLEAN
EFIAPI
CompareGuid(IN const EFI_GUID* Guid1, IN const EFI_GUID* Guid2)
{
    UINT64 LowPartOfGuid1;
    UINT64 LowPartOfGuid2;
    UINT64 HighPartOfGuid1;
    UINT64 HighPartOfGuid2;

    LowPartOfGuid1 = ReadUnaligned64((const UINT64*)Guid1);
    LowPartOfGuid2 = ReadUnaligned64((const UINT64*)Guid2);
    HighPartOfGuid1 = ReadUnaligned64((const UINT64*)Guid1 + 1);
    HighPartOfGuid2 = ReadUnaligned64((const UINT64*)Guid2 + 1);

    return (BOOLEAN)(LowPartOfGuid1 == LowPartOfGuid2 && HighPartOfGuid1 == HighPartOfGuid2);
}

BOOLEAN EFIAPI IsGuidEmpty(_In_ const EFI_GUID* Guid)
{
    return memcmp(Guid, &gEmptyGuid, sizeof(EFI_GUID)) == 0;
}

#endif

//
// Secure function versions
//

EFI_STATUS EFIAPI CopyMemS(OUT VOID* DestinationBuffer,
                           IN UINTN DestSize,
                           IN const VOID* SourceBuffer,
                           IN UINTN Length)
{
    if (DestinationBuffer == NULL || DestSize == 0 || SourceBuffer == NULL || Length == 0)
        return EFI_INVALID_PARAMETER;

    if (DestSize < Length)
        return EFI_INVALID_PARAMETER;

    if (Length >= (MAX_ADDRESS - (UINTN)DestinationBuffer + 1))
        return EFI_INVALID_PARAMETER;

    for (UINTN i = 0; i < Length; i++) {
        ((CHAR8*)DestinationBuffer)[i] = ((CHAR8*)SourceBuffer)[i];
    }

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI SetMemS(OUT VOID* DestinationBuffer,
                          IN UINTN DestSize,
                          IN UINTN Length,
                          IN UINT8 Value)
{
    if (DestinationBuffer == NULL || DestSize == 0 || Length == 0)
        return EFI_INVALID_PARAMETER;

    if (DestSize < Length)
        return EFI_INVALID_PARAMETER;

    if (Length >= (MAX_ADDRESS - (UINTN)DestinationBuffer + 1))
        return EFI_INVALID_PARAMETER;

    for (UINTN i = 0; i < Length; i++) {
        ((CHAR8*)DestinationBuffer)[i] = Value;
    }

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI ZeroMemS(OUT VOID* DestinationBuffer, IN UINTN DestSize, IN UINTN Length)
{
    return SetMemS(DestinationBuffer, DestSize, Length, 0);
}

EFI_STATUS EFIAPI StrTrimS(IN OUT CHAR16* String, IN UINTN MaxStringSize)
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINTN Start = 0, End = 0;
    UINTN StringLen = 0;

    if (String == NULL)
        return EFI_INVALID_PARAMETER;

    StringLen = StrnLenS(String, MaxStringSize);

    //
    // Nothing to trim, just return success
    //

    if (StringLen == 0)
        return EFI_SUCCESS;

    if (StringLen == MaxStringSize)
        return EFI_INVALID_PARAMETER;

    for (; String[Start] && Start < MaxStringSize; Start++)
        if (!InternalIsSpaceCharacter(String[Start]))
            break;

    for (End = StringLen - 1; String[End]; End--)
        if (!InternalIsSpaceCharacter(String[End]))
            break;

    Status = CopyMemS(String, MaxStringSize, &String[Start], (End - Start + 1) * sizeof(CHAR16));
    if (EFI_ERROR(Status))
        return Status;

    String[End - Start + 1] = L'\0';

    return Status;
}

EFI_STATUS EFIAPI AsciiStrTrimS(IN OUT CHAR8* String, IN UINTN MaxStringSize)
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINTN Start = 0, End = 0;
    UINTN StringLen = 0;

    if (String == NULL)
        return EFI_INVALID_PARAMETER;

    StringLen = AsciiStrnLenS(String, MaxStringSize);

    //
    // Nothing to trim, just return success
    //

    if (StringLen == 0)
        return EFI_SUCCESS;

    if (StringLen == MaxStringSize)
        return EFI_INVALID_PARAMETER;

    for (; String[Start] && Start < MaxStringSize; Start++)
        if (!InternalIsSpaceAsciiCharacter(String[Start]))
            break;

    for (End = StringLen - 1; String[End]; End--)
        if (!InternalIsSpaceAsciiCharacter(String[End]))
            break;

    Status = CopyMemS(String, MaxStringSize, &String[Start], (End - Start + 1) * sizeof(CHAR8));
    if (EFI_ERROR(Status))
        return Status;

    String[End - Start + 1] = '\0';

    return Status;
}

// Declared in MdePkg\Include\Library\MemoryAllocationLib.h
#ifndef UEFI_BUILD_SYSTEM
VOID* EFIAPI AllocatePool(IN UINTN AllocationSize)
{
    EFI_STATUS Status;
    VOID* Memory;

    Status = gBS->AllocatePool(EfiBootServicesData, AllocationSize, &Memory);
    if (EFI_ERROR(Status)) {
        Memory = NULL;
    }
    return Memory;
}

VOID* EFIAPI AllocateZeroPool(IN UINTN AllocationSize)
{
    EFI_STATUS Status;
    VOID* Memory;

    Status = gBS->AllocatePool(EfiBootServicesData, AllocationSize, &Memory);
    if (EFI_ERROR(Status)) {
        Memory = NULL;
    }

    if (Memory != NULL) {
        Memory = ZeroMem(Memory, AllocationSize);
    }
    return Memory;
}

VOID* EFIAPI AllocateCopyPool(IN UINTN AllocationSize, IN VOID* Buffer)
{
    EFI_STATUS Status;
    VOID* Memory;

    if (Buffer == NULL) {
        return NULL;
    }

    Status = gBS->AllocatePool(EfiBootServicesData, AllocationSize, &Memory);
    if (EFI_ERROR(Status)) {
        Memory = NULL;
    }

    if (Memory != NULL) {
        Memory = CopyMem(Memory, Buffer, AllocationSize);
    }
    return Memory;
}

VOID* EFIAPI ReallocatePool(IN UINTN OldSize, IN UINTN NewSize, IN VOID* Buffer)
{
    EFI_STATUS Status = EFI_SUCCESS;
    VOID* NewMemory = NULL;

    Status = gBS->AllocatePool(EfiBootServicesData, NewSize, &NewMemory);
    if (EFI_ERROR(Status)) {
        return NULL;
    }

    CopyMem(NewMemory, Buffer, OldSize);
    FreePool(Buffer);

    return NewMemory;
}

VOID EFIAPI FreePool(IN OPTIONAL VOID* Buffer)
{
    if (Buffer) {
        gBS->FreePool(Buffer);
    }
}

INTN EFIAPI CompareMem(IN const VOID* DestinationBuffer,
                       IN const VOID* SourceBuffer,
                       IN UINTN Length)
{
    if (Length == 0 || DestinationBuffer == SourceBuffer) {
        return 0;
    }
    ASSERT(DestinationBuffer != NULL);
    ASSERT(SourceBuffer != NULL);
    ASSERT((Length - 1) <= (MAX_ADDRESS - (UINTN)DestinationBuffer));
    ASSERT((Length - 1) <= (MAX_ADDRESS - (UINTN)SourceBuffer));

    return memcmp(DestinationBuffer, SourceBuffer, Length);
}

EFI_STATUS EFIAPI StrDup(IN const CHAR16* String, IN const CHAR16** OutString)
{
    EFI_STATUS Status = EFI_SUCCESS;
    CHAR16* RetString = NULL;
    UINTN Len = 0;
    UINTN AllocationSize = 0;

    if (String == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Len = StrLen(String);

    Status = UintnAdd(Len, 1, &Len);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = UintnMult(sizeof(CHAR16), Len, &AllocationSize);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    RetString = AllocateZeroPool(AllocationSize);
    if (RetString == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    StrnCpy(RetString, String, Len);
    *OutString = RetString;

    return EFI_SUCCESS;
}

INTN EFIAPI StrCmp(IN const CHAR16* FirstString, IN const CHAR16* SecondString)
{
    ASSERT(FirstString != NULL);
    ASSERT(SecondString != NULL);

    while ((*FirstString != L'\0') && (*FirstString == *SecondString)) {
        FirstString++;
        SecondString++;
    }
    return *FirstString - *SecondString;
}

INTN EFIAPI StrnCmp(IN const CHAR16* FirstString, IN const CHAR16* SecondString, IN UINTN Length)
{
    if (Length == 0) {
        return 0;
    }

    ASSERT(FirstString != NULL);
    ASSERT(SecondString != NULL);

    while ((*FirstString != L'\0') && (*FirstString == *SecondString) && (Length > 1)) {
        FirstString++;
        SecondString++;
        Length--;
    }

    return *FirstString - *SecondString;
}

INTN EFIAPI StrniCmp(IN const CHAR16* FirstString, IN const CHAR16* SecondString, IN UINTN Length)
{
    CHAR16 FirstUpper;
    CHAR16 SecondUpper;

    if (Length == 0) {
        return 0;
    }

    ASSERT(FirstString != NULL);
    ASSERT(SecondString != NULL);

    FirstUpper = CharToUpper(*FirstString);
    SecondUpper = CharToUpper(*SecondString);

    while ((*FirstString != L'\0') && (FirstUpper == SecondUpper) && (Length > 1)) {
        FirstString++;
        SecondString++;
        Length--;

        FirstUpper = CharToUpper(*FirstString);
        SecondUpper = CharToUpper(*SecondString);
    }

    return CharToUpper(*FirstString) - CharToUpper(*SecondString);
}

EFI_STATUS EFIAPI AsciiStrDup(IN const CHAR8* String, IN const CHAR8** OutString)
{
    EFI_STATUS Status = EFI_SUCCESS;
    CHAR8* RetString = NULL;
    UINTN Len = 0;
    UINTN AllocationSize = 0;

    Len = AsciiStrLen(String);

    Status = UintnAdd(Len, 1, &Len);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = UintnMult(sizeof(CHAR8), Len, &AllocationSize);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    RetString = AllocateZeroPool(AllocationSize);
    if (RetString == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    CopyMem(RetString, String, Len);
    *OutString = RetString;

    return EFI_SUCCESS;
}

INTN EFIAPI AsciiStrCmp(IN const CHAR8* FirstString, IN const CHAR8* SecondString)
{
    ASSERT(FirstString != NULL);
    ASSERT(SecondString != NULL);

    while ((*FirstString != '\0') && (*FirstString == *SecondString)) {
        FirstString++;
        SecondString++;
    }
    return *FirstString - *SecondString;
}

INTN EFIAPI AsciiStrnCmp(IN const CHAR8* FirstString, IN const CHAR8* SecondString, IN UINTN Length)
{
    if (Length == 0) {
        return 0;
    }

    ASSERT(FirstString != NULL);
    ASSERT(SecondString != NULL);

    while ((*FirstString != '\0') && (*FirstString == *SecondString) && (Length > 1)) {
        FirstString++;
        SecondString++;
        Length--;
    }

    return *FirstString - *SecondString;
}

INTN EFIAPI AsciiStrniCmp(IN const CHAR8* FirstString,
                          IN const CHAR8* SecondString,
                          IN UINTN Length)
{
    CHAR8 FirstUpper;
    CHAR8 SecondUpper;

    if (Length == 0) {
        return 0;
    }

    ASSERT(FirstString != NULL);
    ASSERT(SecondString != NULL);

    FirstUpper = AsciiCharToUpper(*FirstString);
    SecondUpper = AsciiCharToUpper(*SecondString);

    while ((*FirstString != '\0') && (FirstUpper == SecondUpper) && (Length > 1)) {
        FirstString++;
        SecondString++;
        Length--;

        FirstUpper = AsciiCharToUpper(*FirstString);
        SecondUpper = AsciiCharToUpper(*SecondString);
    }

    return AsciiCharToUpper(*FirstString) - AsciiCharToUpper(*SecondString);
}

//
// Avoid using this function. Use StrnLenS instead
//

UINTN
EFIAPI
StrLen(IN const CHAR16* String)
{
    UINTN Length = 0;

    if (String == NULL) {
        return 0;
    }

    while (String[Length] != L'\0' && Length < MAX_UNICODE_STRING_LEN) {
        Length++;
    }

    return Length;
}

UINTN
EFIAPI
StrnLenS(IN CONST CHAR16* String, IN UINTN MaxSize)
{
    UINTN Length;

    //
    // If String is a null pointer or MaxSize is 0, then the StrnLenS function returns zero.
    //
    if ((String == NULL) || (MaxSize == 0)) {
        return 0;
    }

    //
    // Otherwise, the StrnLenS function returns the number of characters that precede the
    // terminating null character. If there is no null character in the first MaxSize characters of
    // String then StrnLenS returns MaxSize. At most the first MaxSize characters of String shall
    // be accessed by StrnLenS.
    //
    Length = 0;
    while (String[Length] != 0) {
        if (Length >= MaxSize - 1) {
            return MaxSize;
        }
        Length++;
    }
    return Length;
}

CHAR16* EFIAPI StrStr(IN const CHAR16* String, IN const CHAR16* SearchString)
{
    const CHAR16* FirstMatch;
    const CHAR16* SearchStringTmp;

    if (*SearchString == L'\0') {
        return (CHAR16*)String;
    }

    while (*String != L'\0') {
        SearchStringTmp = SearchString;
        FirstMatch = String;

        while ((*String == *SearchStringTmp) && (*String != L'\0')) {
            String++;
            SearchStringTmp++;
        }

        if (*SearchStringTmp == L'\0') {
            return (CHAR16*)FirstMatch;
        }

        if (*String == L'\0') {
            return NULL;
        }

        String = FirstMatch + 1;
    }

    return NULL;
}

BOOLEAN EFIAPI StrEndsWith(IN const CHAR16* String, IN const CHAR16* SearchString)
{
    UINTN StringLength = 0;
    UINTN SearchStringLength = 0;

    if (String == NULL || SearchString == NULL)
        return FALSE;

    StringLength = StrLen(String);
    SearchStringLength = StrLen(SearchString);

    if (SearchStringLength > StringLength)
        return FALSE;

    return StrCmp(&String[StringLength - SearchStringLength], SearchString) == 0;
}
#endif

CHAR16* EFIAPI StrChr(IN const CHAR16* String, IN const CHAR16 SearchCharacter)
{
    if (SearchCharacter == L'\0') {
        return (CHAR16*)String;
    }

    while (*String != L'\0') {
        if (*String == SearchCharacter) {
            return (CHAR16*)String;
        }

        String += 1;
    }

    return NULL;
}
CHAR8* EFIAPI AsciiStrnCpy(OUT CHAR8* DestinationStr, IN const CHAR8* SourceStr, IN UINTN NumChars)
{
    return CopyMem(DestinationStr, SourceStr, sizeof(CHAR8) * NumChars);
}

CHAR16* EFIAPI StrnCpy(OUT CHAR16* DestinationStr, IN const CHAR16* SourceStr, IN UINTN NumChars)
{
    return CopyMem(DestinationStr, SourceStr, sizeof(CHAR16) * NumChars);
}

#ifndef UEFI_BUILD_SYSTEM
CHAR16* EFIAPI StrCpy(OUT CHAR16* DestinationStr, IN const CHAR16* SourceStr)
{
    return CopyMem(DestinationStr, SourceStr, sizeof(CHAR16) * (StrLen(SourceStr) + 1));
}

CHAR8* EFIAPI AsciiStrLower(IN CHAR8* String)
{
    CHAR8* Ptr = String;

    while (*Ptr != L'\0') {
        *Ptr = AsciiCharToLower(*Ptr);
        Ptr++;
    }

    return String;
}

CHAR8* EFIAPI AsciiStrUpper(IN CHAR8* String)
{
    CHAR8* Ptr = String;

    while (*Ptr != L'\0') {
        *Ptr = AsciiCharToUpper(*Ptr);
        Ptr++;
    }

    return String;
}

CHAR16* EFIAPI StrLower(IN CHAR16* String)
{
    CHAR16* Ptr = String;

    while (*Ptr != L'\0') {
        *Ptr = CharToLower(*Ptr);
        Ptr++;
    }

    return String;
}

CHAR16* EFIAPI StrUpper(IN CHAR16* String)
{
    CHAR16* Ptr = String;

    while (*Ptr != L'\0') {
        *Ptr = CharToUpper(*Ptr);
        Ptr++;
    }

    return String;
}

CHAR8
EFIAPI
AsciiCharToUpper(IN CHAR8 Char)
{
    if (Char >= 'a' && Char <= 'z') {
        return (CHAR8)(Char - ('a' - 'A'));
    }

    return Char;
}

CHAR8
EFIAPI
AsciiCharToLower(IN CHAR8 Char)
{
    if (Char >= 'A' && Char <= 'Z') {
        return (CHAR8)(Char + ('a' - 'A'));
    }

    return Char;
}

CHAR16
EFIAPI
CharToUpper(IN CHAR16 Char)
{
    if (Char >= L'a' && Char <= L'z') {
        return (CHAR16)(Char - (L'a' - L'A'));
    }

    return Char;
}

CHAR16
EFIAPI
CharToLower(IN CHAR16 Char)
{
    if (Char >= L'A' && Char <= L'Z') {
        return (CHAR16)(Char + (L'a' - L'A'));
    }

    return Char;
}

//
// Avoid using this function. Use AsciiStrnLenS instead
//

UINTN
EFIAPI
AsciiStrLen(IN const CHAR8* String)
{
    UINTN Length = 0;

    if (String == NULL) {
        return 0;
    }

    while (String[Length] != '\0' && Length < MAX_ASCII_STRING_LEN) {
        Length++;
    }

    return Length;
}

BOOLEAN EFIAPI StrIsEmpty(IN CHAR16* String)
{
    if (String == NULL) {
        return TRUE;
    }

    return StrLen(String) == 0;
}

BOOLEAN EFIAPI AsciiStrIsEmpty(IN CHAR8* String)
{
    if (String == NULL) {
        return TRUE;
    }

    return AsciiStrLen(String) == 0;
}

CHAR8* EFIAPI AsciiStrStr(IN const CHAR8* String, IN const CHAR8* SearchString)
{
    const CHAR8* FirstMatch;
    const CHAR8* SearchStringTmp;

    if (*SearchString == '\0') {
        return (CHAR8*)String;
    }

    while (*String != '\0') {
        SearchStringTmp = SearchString;
        FirstMatch = String;

        while ((*String == *SearchStringTmp) && (*String != '\0')) {
            String++;
            SearchStringTmp++;
        }

        if (*SearchStringTmp == '\0') {
            return (CHAR8*)FirstMatch;
        }

        if (*String == '\0') {
            return NULL;
        }

        String = FirstMatch + 1;
    }

    return NULL;
}

BOOLEAN EFIAPI AsciiStrEndsWith(IN const CHAR8* String, IN const CHAR8* SearchString)
{
    UINTN StringLength = 0;
    UINTN SearchStringLength = 0;

    if (String == NULL || SearchString == NULL)
        return FALSE;

    StringLength = AsciiStrLen(String);
    SearchStringLength = AsciiStrLen(SearchString);

    if (SearchStringLength > StringLength)
        return FALSE;

    return AsciiStrCmp(&String[StringLength - SearchStringLength], SearchString) == 0;
}

CHAR8* EFIAPI AsciiStrChr(IN const CHAR8* String, IN const CHAR8 SearchCharacter)
{
    if (SearchCharacter == '\0') {
        return (CHAR8*)String;
    }

    while (*String != '\0') {
        if (*String == SearchCharacter) {
            return (CHAR8*)String;
        }

        String += 1;
    }

    return NULL;
}

#endif

_Ret_z_ CHAR16* EFIAPI AsciiStrToUnicodeStr(IN const CHAR8* Source, OUT CHAR16* Destination)
{
    CHAR16* ReturnValue;

    ASSERT(Destination != NULL);

    //
    // Source and Destination should not overlap
    //
    ASSERT((UINTN)((CHAR8*)Destination - Source) > AsciiStrLen(Source));

    ReturnValue = Destination;
    while (*Source != '\0') {
        *(Destination++) = (CHAR16) * (Source++);
    }
    //
    // End the Destination with a NULL.
    //
    *Destination = L'\0';

    return ReturnValue;
}

_Ret_z_ CHAR8* EFIAPI UnicodeStrToAsciiStr(IN const CHAR16* Source, OUT CHAR8* Destination)
{
    CHAR8* ReturnValue;

    ASSERT(Destination != NULL);

    ReturnValue = Destination;
    while (*Source != L'\0') {
        *(Destination++) = (CHAR8) * (Source++);
    }

    //
    // End the Destination with a NULL.
    //
    *Destination = '\0';

    return ReturnValue;
}

#ifndef UEFI_BUILD_SYSTEM
EFI_DEVICE_PATH_PROTOCOL* EFIAPI DevicePathFromHandle(IN EFI_HANDLE Handle)
{
    EFI_DEVICE_PATH_PROTOCOL* DevicePath;
    EFI_STATUS Status;

    Status = gBS->HandleProtocol(Handle, &gEfiDevicePathProtocolGuid, (VOID*)&DevicePath);
    if (EFI_ERROR(Status)) {
        DevicePath = NULL;
    }
    return DevicePath;
}
#endif

#ifndef UEFI_BUILD_SYSTEM
VOID EFIAPI EfiAcquireLock(IN EFI_LOCK* Lock)
{
    ASSERT(Lock != NULL);
    ASSERT(Lock->Lock == EfiLockReleased);

    Lock->OwnerTpl = gBS->RaiseTPL(Lock->Tpl);
    Lock->Lock = EfiLockAcquired;
}

EFI_STATUS
EFIAPI
EfiAcquireLockOrFail(IN EFI_LOCK* Lock)
{
    ASSERT(Lock != NULL);
    ASSERT(Lock->Lock != EfiLockUninitialized);

    if (Lock->Lock == EfiLockAcquired) {
        //
        // Lock is already owned, so bail out
        //
        return EFI_ACCESS_DENIED;
    }

    Lock->OwnerTpl = gBS->RaiseTPL(Lock->Tpl);

    Lock->Lock = EfiLockAcquired;

    return EFI_SUCCESS;
}

VOID EFIAPI EfiReleaseLock(IN EFI_LOCK* Lock)
{
    EFI_TPL Tpl;

    Tpl = Lock->OwnerTpl;

    Lock->Lock = EfiLockReleased;

    gBS->RestoreTPL(Tpl);
}

EFI_TPL
EFIAPI
EfiGetCurrentTpl(VOID)
{
    EFI_TPL Tpl;

    Tpl = gBS->RaiseTPL(TPL_HIGH_LEVEL);
    gBS->RestoreTPL(Tpl);

    return Tpl;
}

#endif

// Declared in MdeModulePkg/Include/Library/SortLib.h
#ifndef UEFI_BUILD_SYSTEM

/**
  Worker function for QuickSorting.  This function is identical to PerformQuickSort,
  except that is uses the pre-allocated buffer so the in place sorting does not need to
  allocate and free buffers constantly.
  Each element must be equal sized.
  if BufferToSort is NULL, then ASSERT.
  if CompareFunction is NULL, then ASSERT.
  if Buffer is NULL, then ASSERT.
  if Count is < 2 then perform no action.
  if Size is < 1 then perform no action.
  @param[in, out] BufferToSort   on call a Buffer of (possibly sorted) elements
                                 on return a buffer of sorted elements
  @param[in] Count               the number of elements in the buffer to sort
  @param[in] ElementSize         Size of an element in bytes
  @param[in] CompareFunction     The function to call to perform the comparison
                                 of any 2 elements
  @param[in] Buffer              Buffer of size ElementSize for use in swapping
**/
VOID EFIAPI QuickSortWorker(IN OUT VOID* BufferToSort,
                            IN CONST UINTN Count,
                            IN CONST UINTN ElementSize,
                            IN SORT_COMPARE CompareFunction,
                            IN VOID* Buffer)
{
    VOID* Pivot;
    UINTN LoopCount;
    UINTN NextSwapLocation;

    ASSERT(BufferToSort != NULL);
    ASSERT(CompareFunction != NULL);
    ASSERT(Buffer != NULL);

    if (Count < 2 || ElementSize < 1) {
        return;
    }

    NextSwapLocation = 0;

    //
    // pick a pivot (we choose last element)
    //
    Pivot = ((UINT8*)BufferToSort + ((Count - 1) * ElementSize));

    //
    // Now get the pivot such that all on "left" are below it
    // and everything "right" are above it
    //
    for (LoopCount = 0; LoopCount < Count - 1; LoopCount++) {
        //
        // if the element is less than the pivot
        //
        if (CompareFunction((VOID*)((UINT8*)BufferToSort + ((LoopCount)*ElementSize)), Pivot) <=
            0) {
            //
            // swap
            //
            CopyMem(Buffer, (UINT8*)BufferToSort + (NextSwapLocation * ElementSize), ElementSize);
            CopyMem((UINT8*)BufferToSort + (NextSwapLocation * ElementSize),
                    (UINT8*)BufferToSort + ((LoopCount)*ElementSize),
                    ElementSize);
            CopyMem((UINT8*)BufferToSort + ((LoopCount)*ElementSize), Buffer, ElementSize);

            //
            // increment NextSwapLocation
            //
            NextSwapLocation++;
        }
    }
    //
    // swap pivot to it's final position (NextSwapLocaiton)
    //
    CopyMem(Buffer, Pivot, ElementSize);
    CopyMem(Pivot, (UINT8*)BufferToSort + (NextSwapLocation * ElementSize), ElementSize);
    CopyMem((UINT8*)BufferToSort + (NextSwapLocation * ElementSize), Buffer, ElementSize);

    //
    // Now recurse on 2 paritial lists.  neither of these will have the 'pivot' element
    // IE list is sorted left half, pivot element, sorted right half...
    //
    if (NextSwapLocation >= 2) {
        QuickSortWorker(BufferToSort, NextSwapLocation, ElementSize, CompareFunction, Buffer);
    }

    if ((Count - NextSwapLocation - 1) >= 2) {
        QuickSortWorker((UINT8*)BufferToSort + (NextSwapLocation + 1) * ElementSize,
                        Count - NextSwapLocation - 1,
                        ElementSize,
                        CompareFunction,
                        Buffer);
    }
    return;
}

/**
  Function to perform a Quick Sort alogrithm on a buffer of comparable elements.
  Each element must be equal sized.
  if BufferToSort is NULL, then ASSERT.
  if CompareFunction is NULL, then ASSERT.
  if Count is < 2 then perform no action.
  if Size is < 1 then perform no action.
  @param[in, out] BufferToSort   on call a Buffer of (possibly sorted) elements
                                 on return a buffer of sorted elements
  @param[in] Count               the number of elements in the buffer to sort
  @param[in] ElementSize         Size of an element in bytes
  @param[in] CompareFunction     The function to call to perform the comparison
                                 of any 2 elements
**/
VOID EFIAPI PerformQuickSort(IN OUT VOID* BufferToSort,
                             IN CONST UINTN Count,
                             IN CONST UINTN ElementSize,
                             IN SORT_COMPARE CompareFunction)
{
    VOID* Buffer;

    ASSERT(BufferToSort != NULL);
    ASSERT(CompareFunction != NULL);

    Buffer = AllocateZeroPool(ElementSize);
    ASSERT(Buffer != NULL);

    QuickSortWorker(BufferToSort, Count, ElementSize, CompareFunction, Buffer);

    FreePool(Buffer);
    return;
}
#endif

static UINTN InternalHexCharToUintn(IN CHAR16 Char)
{
    if (Char >= L'0' && Char <= L'9') {
        return Char - L'0';
    }

    return (10 + CharToUpper(Char) - L'A');
}

#ifdef UEFI_BUILD_SYSTEM
EFI_STATUS
EFIAPI
StrHexToUintnMsExtension(IN CONST CHAR16* String, OUT CHAR16** EndPointer, OUT UINTN* Data)
#else
EFI_STATUS
EFIAPI
StrHexToUintn(IN CONST CHAR16* String, OUT CHAR16** EndPointer, OUT UINTN* Data)
#endif
{
    if (EndPointer != NULL) {
        *EndPointer = (CHAR16*)String;
    }

    //
    // Ignore the pad spaces (space or tab)
    //
    while ((*String == L' ') || (*String == L'\t')) {
        String++;
    }

    //
    // Ignore leading Zeros after the spaces
    //
    while (*String == L'0') {
        String++;
    }

    if (*String == L'x' || *String == L'X') {
        if (*(String - 1) != L'0') {
            *Data = 0;
            return EFI_SUCCESS;
        }
        String++;
    }

    *Data = 0;

    while ((*String >= L'0' && *String <= L'9') || (*String >= L'a' && *String <= L'f') ||
           (*String >= L'A' && *String <= L'F')) {
        //
        // If the number represented by String overflows according to the range
        // defined by UINTN, then UINTN_MAX is stored in *Data and
        // EFI_ABORTED is returned.
        //
        if (*Data > ((UINTN_MAX - InternalHexCharToUintn(*String)) >> 4)) {
            *Data = UINTN_MAX;
            if (EndPointer != NULL) {
                *EndPointer = (CHAR16*)String;
            }
            return EFI_ABORTED;
        }

        *Data = (*Data << 4) + InternalHexCharToUintn(*String);
        String++;
    }

    if (EndPointer != NULL) {
        *EndPointer = (CHAR16*)String;
    }
    return EFI_SUCCESS;
}

#ifndef UEFI_BUILD_SYSTEM
static BOOLEAN InternalIsDecimalDigitAsciiCharacter(IN CHAR8 Char)
{
    return (BOOLEAN)(Char >= '0' && Char <= '9');
}
#endif

static BOOLEAN InternalIsSpaceAsciiCharacter(IN CHAR8 Char)
{
    return Char == ' ' || Char == '\t' || Char == '\r' || Char == '\n';
}

static BOOLEAN InternalIsSpaceCharacter(IN CHAR16 Char)
{
    return Char == L' ' || Char == L'\t' || Char == L'\r' || Char == L'\n';
}

#ifndef UEFI_BUILD_SYSTEM
EFI_STATUS
EFIAPI
AsciiStrDecimalToUintnS(IN CONST CHAR8* String, OUT CHAR8** EndPointer, OUT UINTN* Data)
{
    if (EndPointer != NULL) {
        *EndPointer = (CHAR8*)String;
    }

    //
    // Ignore the pad spaces (space or tab)
    //
    while ((*String == ' ') || (*String == '\t')) {
        String++;
    }

    //
    // Ignore leading Zeros after the spaces
    //
    while (*String == '0') {
        String++;
    }

    *Data = 0;

    while (InternalIsDecimalDigitAsciiCharacter(*String)) {
        //
        // If the number represented by String overflows according to the range
        // defined by UINTN, then UINTN_MAX is stored in *Data and
        // EFI_ABORTED is returned.
        //
        if (*Data > ((UINTN_MAX - (*String - '0')) / 10)) {
            *Data = UINTN_MAX;
            if (EndPointer != NULL) {
                *EndPointer = (CHAR8*)String;
            }
            return EFI_ABORTED;
        }

        *Data = *Data * 10 + (*String - '0');
        String++;
    }

    if (EndPointer != NULL) {
        *EndPointer = (CHAR8*)String;
    }
    return EFI_SUCCESS;
}

INTN EFIAPI AsciiStriCmp(IN CONST CHAR8* FirstString, IN CONST CHAR8* SecondString)
{
    CHAR8 UpperFirstString;
    CHAR8 UpperSecondString;

    ASSERT(FirstString != NULL);
    ASSERT(SecondString != NULL);

    UpperFirstString = AsciiCharToUpper(*FirstString);
    UpperSecondString = AsciiCharToUpper(*SecondString);
    while ((*FirstString != '\0') && (*SecondString != '\0') &&
           (UpperFirstString == UpperSecondString)) {
        FirstString++;
        SecondString++;
        UpperFirstString = AsciiCharToUpper(*FirstString);
        UpperSecondString = AsciiCharToUpper(*SecondString);
    }

    return UpperFirstString - UpperSecondString;
}
#endif

#ifndef UEFI_BUILD_SYSTEM
UINTN
EFIAPI
AsciiStrDecimalToUintn(IN CONST CHAR8* String)
{
    UINTN Result;

    AsciiStrDecimalToUintnS(String, (CHAR8**)NULL, &Result);
    return Result;
}
#endif

static BOOLEAN InternalIsDecimalDigitCharacter(IN CHAR16 Char)
{
    return (BOOLEAN)(Char >= L'0' && Char <= L'9');
}

#ifdef UEFI_BUILD_SYSTEM
EFI_STATUS
EFIAPI
StrDecimalToUintnMsExtension(IN CONST CHAR16* String, OUT CHAR16** EndPointer, OUT UINTN* Data)
#else
EFI_STATUS
EFIAPI
StrDecimalToUintn(IN CONST CHAR16* String, OUT CHAR16** EndPointer, OUT UINTN* Data)
#endif
{
    if (EndPointer != NULL) {
        *EndPointer = (CHAR16*)String;
    }

    //
    // Ignore the pad spaces (space or tab)
    //
    while ((*String == L' ') || (*String == L'\t')) {
        String++;
    }

    //
    // Ignore leading Zeros after the spaces
    //
    while (*String == L'0') {
        String++;
    }

    *Data = 0;

    while (InternalIsDecimalDigitCharacter(*String)) {
        //
        // If the number represented by String overflows according to the range
        // defined by UINTN, then UINTN_MAX is stored in *Data and
        // EFI_ABORTED is returned.
        //
        if (*Data > ((UINTN_MAX - (*String - L'0')) / 10)) {
            *Data = UINTN_MAX;
            if (EndPointer != NULL) {
                *EndPointer = (CHAR16*)String;
            }
            return EFI_ABORTED;
        }

        *Data = *Data * 10 + (*String - L'0');
        String++;
    }

    if (EndPointer != NULL) {
        *EndPointer = (CHAR16*)String;
    }
    return EFI_SUCCESS;
}

//
//    Function to convert an integer to string in the given radix.
//    It always returns a pointer to end of the converted string.
//
CHAR8* EFIAPI IntegerToString(_In_ INTN Number,
                              _In_ UINTN Radix,
                              _Out_writes_(StringBufferSize) CHAR8 StringBuffer[],
                              _In_ UINTN StringBufferSize)
{
    CHAR8 *Begin = StringBuffer, *End = StringBuffer;
    CHAR8* StringEnd = StringBuffer;
    INTN Num = Number;
    UINTN RequiredBuffer = 0;

    if (StringBuffer == NULL || Radix < 2 || Radix > 36) {
        return StringEnd;
    }

    if (Num < 0)
        Num = -Num;

    do {
        INTN Digit = Num % Radix;
        if (++RequiredBuffer > StringBufferSize) {
            return NULL;
        }
        *End++ = (CHAR8)((Digit > 9) ? (Digit - 10) + 'A' : Digit + '0');
        Num /= Radix;
    } while (Num);

    if (Number < 0) {
        if (++RequiredBuffer > StringBufferSize) {
            return NULL;
        }
        *End++ = '-';
    }
    if (++RequiredBuffer > StringBufferSize) {
        return NULL;
    }
    *End = '\0';

    // Reverse the number
    for (StringEnd = End--; Begin < End; Begin++, End--) {
        CHAR8 Temp = *End;
        *End = *Begin;
        *Begin = Temp;
    }

    return StringEnd;
}

static VOID EFIAPI TimerWaitCallback(_In_ EFI_EVENT Event, _In_ PVOID Context)
{
    UNREFERENCED_PARAMETER(Event);
    UNREFERENCED_PARAMETER(Context);

    //
    // Wait callbacks are triggered on every tick until the event is signaled.
    // So don't put anything here. Keep them empty!
    //
}

//
// NOTE: Below function will fail with EFI_UNSUPPORTED if current TPL is not
// TPL_APPLICATION. So do not call EfiSleep() in callbacks!
//

EFI_STATUS EFIAPI EfiSleep(UINTN DurationInNS)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_EVENT TimerEvent = NULL;
    UINTN Index = 0;

    Status = gBS->CreateEvent(EVT_TIMER | EVT_NOTIFY_WAIT,
                              TPL_CALLBACK,
                              TimerWaitCallback,
                              NULL,
                              &TimerEvent);
    if (EFI_ERROR(Status)) {
        goto Exit;
    }

    Status = gBS->SetTimer(TimerEvent, TimerRelative, DurationInNS);
    if (EFI_ERROR(Status)) {
        goto Exit;
    }

    Status = gBS->WaitForEvent(1, &TimerEvent, &Index);

Exit:
    if (TimerEvent != NULL) {
        gBS->CloseEvent(TimerEvent);
    }

    return Status;
}

#ifndef UEFI_BUILD_SYSTEM
//
// Description:
//      Helper for converting GUID to string.
//      128 bit GUID to human-readable string.
//
VOID EFIAPI GuidToWideString(_In_ const GUID* Guid,
                             _Out_writes_z_(BufferSize) CHAR16 Buffer[],
                             _In_ UINT8 BufferSize)
{
    UNREFERENCED_PARAMETER(BufferSize);

    // Get the beginning guid sequence (first 19 characters)
    size_t TempSize = BufferSize;
    CHAR16* TempBuffer = Buffer;
    StringCchPrintfExW((STRSAFE_LPWSTR)TempBuffer,
                       TempSize,
                       &TempBuffer,
                       &TempSize,
                       0,
                       L"%.8lX-%.4hX-%.4hX-",
                       Guid->Data1,
                       Guid->Data2,
                       Guid->Data3);
    for (int i = 0; i < sizeof(Guid->Data4); ++i) {
#pragma prefast(suppress : 26018, "TempSize will not be larger than BufferSize.");
        StringCchPrintfExW((STRSAFE_LPWSTR)TempBuffer,
                           TempSize,
                           &TempBuffer,
                           &TempSize,
                           0,
                           L"%.2hhX",
                           Guid->Data4[i]);
        if (i == 1) {
            StringCchPrintfExW((STRSAFE_LPWSTR)TempBuffer,
                               TempSize,
                               &TempBuffer,
                               &TempSize,
                               0,
                               L"-");
        }
    }
}
#endif

#ifndef UEFI_BUILD_SYSTEM

BOOLEAN
InternalSafeStringIsOverlap(IN VOID* Base1, IN UINTN Size1, IN VOID* Base2, IN UINTN Size2)
{
    if ((((UINTN)Base1 >= (UINTN)Base2) && ((UINTN)Base1 < (UINTN)Base2 + Size2)) ||
        (((UINTN)Base2 >= (UINTN)Base1) && ((UINTN)Base2 < (UINTN)Base1 + Size1))) {
        return TRUE;
    }
    return FALSE;
}

BOOLEAN
InternalSafeStringNoAsciiStrOverlap(IN CHAR8* Str1, IN UINTN Size1, IN CHAR8* Str2, IN UINTN Size2)
{
    return !InternalSafeStringIsOverlap(Str1, Size1, Str2, Size2);
}

EFI_STATUS
EFIAPI
AsciiStrCpyS(OUT CHAR8* Destination, IN UINTN DestMax, IN CONST CHAR8* Source)
{
    UINTN SourceLen;

    //
    // 1. Neither Destination nor Source shall be a null pointer.
    //
    SAFE_STRING_CONSTRAINT_CHECK((Destination != NULL), EFI_INVALID_PARAMETER);
    SAFE_STRING_CONSTRAINT_CHECK((Source != NULL), EFI_INVALID_PARAMETER);

    //
    // 2. DestMax shall not be greater than ASCII_RSIZE_MAX.
    //
    //   if (ASCII_RSIZE_MAX != 0) {
    //     SAFE_STRING_CONSTRAINT_CHECK ((DestMax <= ASCII_RSIZE_MAX), EFI_INVALID_PARAMETER);
    //   }

    //
    // 3. DestMax shall not equal zero.
    //
    SAFE_STRING_CONSTRAINT_CHECK((DestMax != 0), EFI_INVALID_PARAMETER);

    //
    // 4. DestMax shall be greater than AsciiStrnLenS(Source, DestMax).
    //
    SourceLen = AsciiStrnLenS(Source, DestMax);
    SAFE_STRING_CONSTRAINT_CHECK((DestMax > SourceLen), EFI_BUFFER_TOO_SMALL);

    //
    // 5. Copying shall not take place between objects that overlap.
    //
    SAFE_STRING_CONSTRAINT_CHECK(
        InternalSafeStringNoAsciiStrOverlap(Destination, DestMax, (CHAR8*)Source, SourceLen + 1),
        EFI_ACCESS_DENIED);

    //
    // The AsciiStrCpyS function copies the string pointed to by Source (including the terminating
    // null character) into the array pointed to by Destination.
    //
    while (*Source != 0) {
        *(Destination++) = *(Source++);
    }
    *Destination = 0;

    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
AsciiStrCatS(IN OUT CHAR8* Destination, IN UINTN DestMax, IN CONST CHAR8* Source)
{
    UINTN DestLen;
    UINTN CopyLen;
    UINTN SourceLen;

    //
    // Let CopyLen denote the value DestMax - AsciiStrnLenS(Destination, DestMax) upon entry to
    // AsciiStrCatS.
    //
    DestLen = AsciiStrnLenS(Destination, DestMax);
    CopyLen = DestMax - DestLen;

    //
    // 1. Neither Destination nor Source shall be a null pointer.
    //
    SAFE_STRING_CONSTRAINT_CHECK((Destination != NULL), EFI_INVALID_PARAMETER);
    SAFE_STRING_CONSTRAINT_CHECK((Source != NULL), EFI_INVALID_PARAMETER);

    //
    // 2. DestMax shall not be greater than ASCII_RSIZE_MAX.
    //
    //   if (ASCII_RSIZE_MAX != 0) {
    //     SAFE_STRING_CONSTRAINT_CHECK ((DestMax <= ASCII_RSIZE_MAX), EFI_INVALID_PARAMETER);
    //   }

    //
    // 3. DestMax shall not equal zero.
    //
    SAFE_STRING_CONSTRAINT_CHECK((DestMax != 0), EFI_INVALID_PARAMETER);

    //
    // 4. CopyLen shall not equal zero.
    //
    SAFE_STRING_CONSTRAINT_CHECK((CopyLen != 0), EFI_BAD_BUFFER_SIZE);

    //
    // 5. CopyLen shall be greater than AsciiStrnLenS(Source, CopyLen).
    //
    SourceLen = AsciiStrnLenS(Source, CopyLen);
    SAFE_STRING_CONSTRAINT_CHECK((CopyLen > SourceLen), EFI_BUFFER_TOO_SMALL);

    //
    // 6. Copying shall not take place between objects that overlap.
    //
    SAFE_STRING_CONSTRAINT_CHECK(
        InternalSafeStringNoAsciiStrOverlap(Destination, DestMax, (CHAR8*)Source, SourceLen + 1),
        EFI_ACCESS_DENIED);

    //
    // The AsciiStrCatS function appends a copy of the string pointed to by Source (including the
    // terminating null character) to the end of the string pointed to by Destination. The initial
    // character from Source overwrites the null character at the end of Destination.
    //
    Destination = Destination + DestLen;
    while (*Source != 0) {
        *(Destination++) = *(Source++);
    }
    *Destination = 0;

    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
AsciiStrnCpyS(OUT CHAR8* Destination, IN UINTN DestMax, IN CONST CHAR8* Source, IN UINTN Length)
{
    UINTN SourceLen;

    //
    // 1. Neither Destination nor Source shall be a null pointer.
    //
    SAFE_STRING_CONSTRAINT_CHECK((Destination != NULL), EFI_INVALID_PARAMETER);
    SAFE_STRING_CONSTRAINT_CHECK((Source != NULL), EFI_INVALID_PARAMETER);

    //
    // 2. Neither DestMax nor Length shall be greater than ASCII_RSIZE_MAX
    //
    //   if (ASCII_RSIZE_MAX != 0) {
    //     SAFE_STRING_CONSTRAINT_CHECK ((DestMax <= ASCII_RSIZE_MAX), EFI_INVALID_PARAMETER);
    //     SAFE_STRING_CONSTRAINT_CHECK ((Length <= ASCII_RSIZE_MAX), EFI_INVALID_PARAMETER);
    //   }

    //
    // 3. DestMax shall not equal zero.
    //
    SAFE_STRING_CONSTRAINT_CHECK((DestMax != 0), EFI_INVALID_PARAMETER);

    //
    // 4. If Length is not less than DestMax, then DestMax shall be greater than
    // AsciiStrnLenS(Source, DestMax).
    //
    SourceLen = AsciiStrnLenS(Source, MIN(DestMax, Length));
    if (Length >= DestMax) {
        SAFE_STRING_CONSTRAINT_CHECK((DestMax > SourceLen), EFI_BUFFER_TOO_SMALL);
    }

    //
    // 5. Copying shall not take place between objects that overlap.
    //
    if (SourceLen > Length) {
        SourceLen = Length;
    }
    SAFE_STRING_CONSTRAINT_CHECK(
        InternalSafeStringNoAsciiStrOverlap(Destination, DestMax, (CHAR8*)Source, SourceLen + 1),
        EFI_ACCESS_DENIED);

    //
    // The AsciiStrnCpyS function copies not more than Length successive characters (characters that
    // follow a null character are not copied) from the array pointed to by Source to the array
    // pointed to by Destination. If no null character was copied from Source, then
    // Destination[Length] is set to a null character.
    //
    while ((SourceLen > 0) && (*Source != 0)) {
        *(Destination++) = *(Source++);
        SourceLen--;
    }
    *Destination = 0;

    return EFI_SUCCESS;
}

CHAR8* EFIAPI AsciiStrCpy(OUT CHAR8* DestinationStr, IN const CHAR8* SourceStr)
{
    return CopyMem(DestinationStr, SourceStr, sizeof(CHAR8) * (AsciiStrLen(SourceStr) + 1));
}

CHAR8* EFIAPI AsciiStrCat(IN OUT CHAR8* Destination, IN CONST CHAR8* Source)
{
    AsciiStrCpy(Destination + AsciiStrLen(Destination), Source);

    //
    // Size of the resulting string should never be zero.
    // PcdMaximumUnicodeStringLength is tested inside StrLen().
    //
    // ASSERT (AsciiStrSize (Destination) != 0);
    return Destination;
}

BOOLEAN EFIAPI InternalAsciiIsDecimalDigitCharacter(IN CHAR8 Char)
{
    return (BOOLEAN)(Char >= '0' && Char <= '9');
}

BOOLEAN EFIAPI InternalAsciiIsHexaDecimalDigitCharacter(IN CHAR8 Char)
{
    return (BOOLEAN)(InternalAsciiIsDecimalDigitCharacter(Char) || (Char >= 'A' && Char <= 'F') ||
                     (Char >= 'a' && Char <= 'f'));
}

UINTN EFIAPI AsciiHexCharToUintn(IN CHAR8 Char)
{
    if (InternalIsDecimalDigitCharacter(Char)) {
        return Char - '0';
    }

    return (10 + AsciiCharToUpper(Char) - 'A');
}

UINTN EFIAPI AsciiStrHexToUintn(IN CONST CHAR8* String)
{
    UINTN Result;

    AsciiStrHexToUintnS(String, (CHAR8**)NULL, &Result);
    return Result;
}

EFI_STATUS EFIAPI AsciiStrHexToUintnS(IN CONST CHAR8* String,
                                      OUT CHAR8** EndPointer,
                                      OPTIONAL OUT UINTN* Data)
{
    // FIXME: Add back sanity checks

    // //
    // // 1. Neither String nor Data shall be a null pointer.
    // //
    // SAFE_STRING_CONSTRAINT_CHECK((String != NULL), RETURN_INVALID_PARAMETER);
    // SAFE_STRING_CONSTRAINT_CHECK((Data != NULL), RETURN_INVALID_PARAMETER);

    // //
    // // 2. The length of String shall not be greater than ASCII_RSIZE_MAX.
    // //
    // if (ASCII_RSIZE_MAX != 0) {
    //     SAFE_STRING_CONSTRAINT_CHECK((AsciiStrnLenS(String, ASCII_RSIZE_MAX + 1) <=
    //                                   ASCII_RSIZE_MAX),
    //                                  RETURN_INVALID_PARAMETER);
    //}

    if (EndPointer != NULL) {
        *EndPointer = (CHAR8*)String;
    }

    //
    // Ignore the pad spaces (space or tab)
    //
    while ((*String == ' ') || (*String == '\t')) {
        String++;
    }

    //
    // Ignore leading Zeros after the spaces
    //
    while (*String == '0') {
        String++;
    }

    if (AsciiCharToUpper(*String) == 'X') {
        if (*(String - 1) != '0') {
            *Data = 0;
            return EFI_SUCCESS;
        }
        //
        // Skip the 'X'
        //
        String++;
    }

    *Data = 0;

    while (InternalAsciiIsHexaDecimalDigitCharacter(*String)) {
        //
        // If the number represented by String overflows according to the range
        // defined by UINTN, then MAX_UINTN is stored in *Data and
        // EFI_UNSUPPORTED is returned.
        //
        if (*Data > ((MAX_UINTN - AsciiHexCharToUintn(*String)) >> 4)) {
            *Data = MAX_UINTN;
            if (EndPointer != NULL) {
                *EndPointer = (CHAR8*)String;
            }
            return EFI_UNSUPPORTED;
        }

        *Data = (*Data << 4) + AsciiHexCharToUintn(*String);
        String++;
    }

    if (EndPointer != NULL) {
        *EndPointer = (CHAR8*)String;
    }
    return EFI_SUCCESS;
}

// From NetworkPkg/Library/DxeHttpLib

//
// Structure to store the parse result of a HTTP URL.
//
typedef struct {
    UINT32 Offset;
    UINT32 Length;
} HTTP_URL_FILED_DATA;

typedef struct {
    UINT16 FieldBitMap;
    HTTP_URL_FILED_DATA FieldData[HTTP_URI_FIELD_MAX];
} HTTP_URL_PARSER;

typedef enum {
    UrlParserUrlStart,
    UrlParserScheme,
    UrlParserSchemeColon,           // ":"
    UrlParserSchemeColonSlash,      // ":/"
    UrlParserSchemeColonSlashSlash, // "://"
    UrlParserAuthority,
    UrlParserAtInAuthority,
    UrlParserPath,
    UrlParserQueryStart, // "?"
    UrlParserQuery,
    UrlParserFragmentStart, // "#"
    UrlParserFragment,
    UrlParserUserInfo,
    UrlParserHostStart, // "@"
    UrlParserHost,
    UrlParserHostIpv6,  // "["(Ipv6 address) "]"
    UrlParserPortStart, // ":"
    UrlParserPort,
    UrlParserStateMax
} HTTP_URL_PARSE_STATE;

/**
  Decode a percent-encoded URI component to the ASCII character.

  Decode the input component in Buffer according to RFC 3986. The caller is responsible to make
  sure ResultBuffer points to a buffer with size equal or greater than ((AsciiStrSize (Buffer))
  in bytes.

  @param[in]    Buffer           The pointer to a percent-encoded URI component.
  @param[in]    BufferLength     Length of Buffer in bytes.
  @param[out]   ResultBuffer     Point to the buffer to store the decode result.
  @param[out]   ResultLength     Length of decoded string in ResultBuffer in bytes.

  @retval EFI_SUCCESS            Successfully decoded the URI.
  @retval EFI_INVALID_PARAMETER  Buffer is not a valid percent-encoded string.

**/
EFI_STATUS EFIAPI UriPercentDecode(IN CHAR8* Buffer,
                                   IN UINT32 BufferLength,
                                   OUT CHAR8* ResultBuffer,
                                   OUT UINT32* ResultLength)
{
    UINTN Index;
    UINTN Offset;
    CHAR8 HexStr[3];

    if (Buffer == NULL || BufferLength == 0 || ResultBuffer == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Index = 0;
    Offset = 0;
    HexStr[2] = '\0';
    while (Index < BufferLength) {
        if (Buffer[Index] == '%') {
            if (Index + 1 >= BufferLength || Index + 2 >= BufferLength ||
                !NET_IS_HEX_CHAR(Buffer[Index + 1]) || !NET_IS_HEX_CHAR(Buffer[Index + 2])) {
                return EFI_INVALID_PARAMETER;
            }
            HexStr[0] = Buffer[Index + 1];
            HexStr[1] = Buffer[Index + 2];
            ResultBuffer[Offset] = (CHAR8)AsciiStrHexToUintn(HexStr);
            Index += 3;
        } else {
            ResultBuffer[Offset] = Buffer[Index];
            Index++;
        }
        Offset++;
    }

    *ResultLength = (UINT32)Offset;

    return EFI_SUCCESS;
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

/**
  This function return the updated state according to the input state and next character of
  the authority.

  @param[in]       Char           Next character.
  @param[in]       State          Current value of the parser state machine.
  @param[in]       IsRightBracket TRUE if there is an sign ']' in the authority component and
                                  indicates the next part is ':' before Port.

  @return          Updated state value.
**/
HTTP_URL_PARSE_STATE
NetHttpParseAuthorityChar(IN CHAR8 Char, IN HTTP_URL_PARSE_STATE State, IN BOOLEAN* IsRightBracket)
{
    //
    // RFC 3986:
    // The authority component is preceded by a double slash ("//") and is
    // terminated by the next slash ("/"), question mark ("?"), or number
    // sign ("#") character, or by the end of the URI.
    //
    if (Char == ' ' || Char == '\r' || Char == '\n') {
        return UrlParserStateMax;
    }

    //
    // authority   = [ userinfo "@" ] host [ ":" port ]
    //
    switch (State) {
        case UrlParserUserInfo:
            if (Char == '@') {
                return UrlParserHostStart;
            }
            break;

        case UrlParserHost:
        case UrlParserHostStart:
            if (Char == '[') {
                return UrlParserHostIpv6;
            }

            if (Char == ':') {
                return UrlParserPortStart;
            }

            return UrlParserHost;

        case UrlParserHostIpv6:
            if (Char == ']') {
                *IsRightBracket = TRUE;
            }

            if (Char == ':' && *IsRightBracket) {
                return UrlParserPortStart;
            }
            return UrlParserHostIpv6;

        case UrlParserPort:
        case UrlParserPortStart:
            return UrlParserPort;

        default:
            break;
    }

    return State;
}

/**
  This function parse the authority component of the input URL and update the parser.

  @param[in]       Url            The pointer to a HTTP URL string.
  @param[in]       FoundAt        TRUE if there is an at sign ('@') in the authority component.
  @param[in, out]  UrlParser      Pointer to the buffer of the parse result.

  @retval EFI_SUCCESS             Successfully parse the authority.
  @retval EFI_INVALID_PARAMETER   The Url is invalid to parse the authority component.

**/
EFI_STATUS
NetHttpParseAuthority(IN CHAR8* Url, IN BOOLEAN FoundAt, IN OUT HTTP_URL_PARSER* UrlParser)
{
    CHAR8* Char;
    CHAR8* Authority;
    UINT32 Length;
    HTTP_URL_PARSE_STATE State;
    UINT32 Field;
    UINT32 OldField;
    BOOLEAN IsrightBracket;

    ASSERT((UrlParser->FieldBitMap & BIT(HTTP_URI_FIELD_AUTHORITY)) != 0);

    //
    // authority   = [ userinfo "@" ] host [ ":" port ]
    //
    if (FoundAt) {
        State = UrlParserUserInfo;
    } else {
        State = UrlParserHost;
    }

    IsrightBracket = FALSE;
    Field = HTTP_URI_FIELD_MAX;
    OldField = Field;
    Authority = Url + UrlParser->FieldData[HTTP_URI_FIELD_AUTHORITY].Offset;
    Length = UrlParser->FieldData[HTTP_URI_FIELD_AUTHORITY].Length;
    for (Char = Authority; Char < Authority + Length; Char++) {
        State = NetHttpParseAuthorityChar(*Char, State, &IsrightBracket);
        switch (State) {
            case UrlParserStateMax:
                return EFI_INVALID_PARAMETER;

            case UrlParserHostStart:
            case UrlParserPortStart:
                continue;

            case UrlParserUserInfo:
                Field = HTTP_URI_FIELD_USERINFO;
                break;

            case UrlParserHost:
                Field = HTTP_URI_FIELD_HOST;
                break;

            case UrlParserHostIpv6:
                Field = HTTP_URI_FIELD_HOST;
                break;

            case UrlParserPort:
                Field = HTTP_URI_FIELD_PORT;
                break;

            default:
                ASSERT(FALSE);
        }

        //
        // Field not changed, count the length.
        //
        ASSERT(Field < HTTP_URI_FIELD_MAX);
        if (Field == OldField) {
#ifndef UEFI_BUILD_SYSTEM
#pragma prefast(suppress : 26000, "Expression will not overflow.");
#endif
            UrlParser->FieldData[Field].Length++;
            continue;
        }

        //
        // New field start
        //
        UrlParser->FieldBitMap |= BIT(Field);
        UrlParser->FieldData[Field].Offset = (UINT32)(Char - Url);
        UrlParser->FieldData[Field].Length = 1;
        OldField = Field;
    }

    return EFI_SUCCESS;
}

/**
  This function return the updated state according to the input state and next character of a URL.

  @param[in]       Char           Next character.
  @param[in]       State          Current value of the parser state machine.

  @return          Updated state value.

**/
HTTP_URL_PARSE_STATE
NetHttpParseUrlChar(IN CHAR8 Char, IN HTTP_URL_PARSE_STATE State)
{
    if (Char == ' ' || Char == '\r' || Char == '\n') {
        return UrlParserStateMax;
    }

    //
    // http_URL = "http:" "//" host [ ":" port ] [ abs_path [ "?" query ]]
    //
    // Request-URI    = "*" | absolute-URI | path-absolute | authority
    //
    // absolute-URI  = scheme ":" hier-part [ "?" query ]
    // path-absolute = "/" [ segment-nz *( "/" segment ) ]
    // authority   = [ userinfo "@" ] host [ ":" port ]
    //
    switch (State) {
        case UrlParserUrlStart:
            if (Char == '*' || Char == '/') {
                return UrlParserPath;
            }
            return UrlParserScheme;

        case UrlParserScheme:
            if (Char == ':') {
                return UrlParserSchemeColon;
            }
            break;

        case UrlParserSchemeColon:
            if (Char == '/') {
                return UrlParserSchemeColonSlash;
            }
            break;

        case UrlParserSchemeColonSlash:
            if (Char == '/') {
                return UrlParserSchemeColonSlashSlash;
            }
            break;

        case UrlParserAtInAuthority:
            if (Char == '@') {
                return UrlParserStateMax;
            }

        case UrlParserAuthority:
        case UrlParserSchemeColonSlashSlash:
            if (Char == '@') {
                return UrlParserAtInAuthority;
            }
            if (Char == '/') {
                return UrlParserPath;
            }
            if (Char == '?') {
                return UrlParserQueryStart;
            }
            if (Char == '#') {
                return UrlParserFragmentStart;
            }
            return UrlParserAuthority;

        case UrlParserPath:
            if (Char == '?') {
                return UrlParserQueryStart;
            }
            if (Char == '#') {
                return UrlParserFragmentStart;
            }
            break;

        case UrlParserQuery:
        case UrlParserQueryStart:
            if (Char == '#') {
                return UrlParserFragmentStart;
            }
            return UrlParserQuery;

        case UrlParserFragmentStart:
            return UrlParserFragment;

        default:
            break;
    }

    return State;
}
/**
  Create a URL parser for the input URL string.

  This function will parse and dereference the input HTTP URL into it components. The original
  content of the URL won't be modified and the result will be returned in UrlParser, which can
  be used in other functions like NetHttpUrlGetHostName().

  @param[in]    Url                The pointer to a HTTP URL string.
  @param[in]    Length             Length of Url in bytes.
  @param[in]    IsConnectMethod    Whether the Url is used in HTTP CONNECT method or not.
  @param[out]   UrlParser          Pointer to the returned buffer to store the parse result.

  @retval EFI_SUCCESS              Successfully dereferenced the HTTP URL.
  @retval EFI_INVALID_PARAMETER    UrlParser is NULL or Url is not a valid HTTP URL.
  @retval EFI_OUT_OF_RESOURCES     Could not allocate needed resources.

**/
EFI_STATUS
EFIAPI
HttpParseUrl(IN CHAR8* Url, IN UINT32 Length, IN BOOLEAN IsConnectMethod, OUT VOID** UrlParser)
{
    HTTP_URL_PARSE_STATE State;
    CHAR8* Char;
    UINT32 Field;
    UINT32 OldField;
    BOOLEAN FoundAt;
    EFI_STATUS Status;
    HTTP_URL_PARSER* Parser;

    Parser = NULL;

    if (Url == NULL || Length == 0 || UrlParser == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Parser = AllocateZeroPool(sizeof(HTTP_URL_PARSER));
    if (Parser == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    if (IsConnectMethod) {
        //
        // According to RFC 2616, the authority form is only used by the CONNECT method.
        //
        State = UrlParserAuthority;
    } else {
        State = UrlParserUrlStart;
    }

    Field = HTTP_URI_FIELD_MAX;
    OldField = Field;
    FoundAt = FALSE;
    for (Char = Url; Char < Url + Length; Char++) {
        //
        // Update state machine according to next char.
        //
        State = NetHttpParseUrlChar(*Char, State);

        switch (State) {
            case UrlParserStateMax:
                FreePool(Parser);
                return EFI_INVALID_PARAMETER;

            case UrlParserSchemeColon:
            case UrlParserSchemeColonSlash:
            case UrlParserSchemeColonSlashSlash:
            case UrlParserQueryStart:
            case UrlParserFragmentStart:
                //
                // Skip all the delimiting char: "://" "?" "@"
                //
                continue;

            case UrlParserScheme:
                Field = HTTP_URI_FIELD_SCHEME;
                break;

            case UrlParserAtInAuthority:
                FoundAt = TRUE;
            case UrlParserAuthority:
                Field = HTTP_URI_FIELD_AUTHORITY;
                break;

            case UrlParserPath:
                Field = HTTP_URI_FIELD_PATH;
                break;

            case UrlParserQuery:
                Field = HTTP_URI_FIELD_QUERY;
                break;

            case UrlParserFragment:
                Field = HTTP_URI_FIELD_FRAGMENT;
                break;

            default:
                ASSERT(FALSE);
        }

        //
        // Field not changed, count the length.
        //
        ASSERT(Field < HTTP_URI_FIELD_MAX);
        if (Field == OldField) {
            Parser->FieldData[Field].Length++;
            continue;
        }

        //
        // New field start
        //
        Parser->FieldBitMap |= BIT(Field);
        Parser->FieldData[Field].Offset = (UINT32)(Char - Url);
        Parser->FieldData[Field].Length = 1;
        OldField = Field;
    }

    //
    // If has authority component, continue to parse the username, host and port.
    //
    if ((Parser->FieldBitMap & BIT(HTTP_URI_FIELD_AUTHORITY)) != 0) {
        Status = NetHttpParseAuthority(Url, FoundAt, Parser);
        if (EFI_ERROR(Status)) {
            FreePool(Parser);
            return Status;
        }
    }

    *UrlParser = Parser;
    return EFI_SUCCESS;
}

/**
  Get the Hostname from a HTTP URL.

  This function will return the HostName according to the Url and previous parse result ,and
  it is the caller's responsibility to free the buffer returned in *HostName.

  @param[in]    Url                The pointer to a HTTP URL string.
  @param[in]    UrlParser          URL Parse result returned by NetHttpParseUrl().
  @param[out]   HostName           Pointer to a buffer to store the HostName.

  @retval EFI_SUCCESS              Successfully get the required component.
  @retval EFI_INVALID_PARAMETER    Uri is NULL or HostName is NULL or UrlParser is invalid.
  @retval EFI_NOT_FOUND            No hostName component in the URL.
  @retval EFI_OUT_OF_RESOURCES     Could not allocate needed resources.

**/
EFI_STATUS
EFIAPI
HttpUrlGetHostName(IN CHAR8* Url, IN VOID* UrlParser, OUT CHAR8** HostName)
{
    CHAR8* Name;
    EFI_STATUS Status;
    UINT32 ResultLength;
    HTTP_URL_PARSER* Parser;

    if (Url == NULL || UrlParser == NULL || HostName == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Parser = (HTTP_URL_PARSER*)UrlParser;

    if ((Parser->FieldBitMap & BIT(HTTP_URI_FIELD_HOST)) == 0) {
        return EFI_NOT_FOUND;
    }

    Name = AllocatePool(Parser->FieldData[HTTP_URI_FIELD_HOST].Length + 1);
    if (Name == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    Status = UriPercentDecode(Url + Parser->FieldData[HTTP_URI_FIELD_HOST].Offset,
                              Parser->FieldData[HTTP_URI_FIELD_HOST].Length,
                              Name,
                              &ResultLength);
    if (EFI_ERROR(Status)) {
        FreePool(Name);
        return Status;
    }

    Name[ResultLength] = '\0';
    *HostName = Name;
    return EFI_SUCCESS;
}

// /**
//   Get the IPv4 address from a HTTP URL.

//   This function will return the IPv4 address according to the Url and previous parse result.

//   @param[in]    Url                The pointer to a HTTP URL string.
//   @param[in]    UrlParser          URL Parse result returned by NetHttpParseUrl().
//   @param[out]   Ip4Address         Pointer to a buffer to store the IP address.

//   @retval EFI_SUCCESS              Successfully get the required component.
//   @retval EFI_INVALID_PARAMETER    Uri is NULL or Ip4Address is NULL or UrlParser is invalid.
//   @retval EFI_NOT_FOUND            No IPv4 address component in the URL.
//   @retval EFI_OUT_OF_RESOURCES     Could not allocate needed resources.

// **/
// EFI_STATUS
// EFIAPI
// HttpUrlGetIp4(IN CHAR8* Url, IN VOID* UrlParser, OUT EFI_IPv4_ADDRESS* Ip4Address)
// {
//     CHAR8* Ip4String;
//     EFI_STATUS Status;
//     UINT32 ResultLength;
//     HTTP_URL_PARSER* Parser;

//     if (Url == NULL || UrlParser == NULL || Ip4Address == NULL) {
//         return EFI_INVALID_PARAMETER;
//     }

//     Parser = (HTTP_URL_PARSER*)UrlParser;

//     if ((Parser->FieldBitMap & BIT(HTTP_URI_FIELD_HOST)) == 0) {
//         return EFI_NOT_FOUND;
//     }

//     Ip4String = AllocatePool(Parser->FieldData[HTTP_URI_FIELD_HOST].Length + 1);
//     if (Ip4String == NULL) {
//         return EFI_OUT_OF_RESOURCES;
//     }

//     Status = UriPercentDecode(Url + Parser->FieldData[HTTP_URI_FIELD_HOST].Offset,
//                               Parser->FieldData[HTTP_URI_FIELD_HOST].Length,
//                               Ip4String,
//                               &ResultLength);
//     if (EFI_ERROR(Status)) {
//         FreePool(Ip4String);
//         return Status;
//     }

//     Ip4String[ResultLength] = '\0';
//     Status = NetLibAsciiStrToIp4(Ip4String, Ip4Address);
//     FreePool(Ip4String);

//     return Status;
// }

// /**
//   Get the IPv6 address from a HTTP URL.

//   This function will return the IPv6 address according to the Url and previous parse result.

//   @param[in]    Url                The pointer to a HTTP URL string.
//   @param[in]    UrlParser          URL Parse result returned by NetHttpParseUrl().
//   @param[out]   Ip6Address         Pointer to a buffer to store the IP address.

//   @retval EFI_SUCCESS              Successfully get the required component.
//   @retval EFI_INVALID_PARAMETER    Uri is NULL or Ip6Address is NULL or UrlParser is invalid.
//   @retval EFI_NOT_FOUND            No IPv6 address component in the URL.
//   @retval EFI_OUT_OF_RESOURCES     Could not allocate needed resources.

// **/
// EFI_STATUS
// EFIAPI
// HttpUrlGetIp6(IN CHAR8* Url, IN VOID* UrlParser, OUT EFI_IPv6_ADDRESS* Ip6Address)
// {
//     CHAR8* Ip6String;
//     CHAR8* Ptr;
//     UINT32 Length;
//     EFI_STATUS Status;
//     UINT32 ResultLength;
//     HTTP_URL_PARSER* Parser;

//     if (Url == NULL || UrlParser == NULL || Ip6Address == NULL) {
//         return EFI_INVALID_PARAMETER;
//     }

//     Parser = (HTTP_URL_PARSER*)UrlParser;

//     if ((Parser->FieldBitMap & BIT(HTTP_URI_FIELD_HOST)) == 0) {
//         return EFI_NOT_FOUND;
//     }

//     //
//     // IP-literal = "[" ( IPv6address / IPvFuture  ) "]"
//     //
//     Length = Parser->FieldData[HTTP_URI_FIELD_HOST].Length;
//     if (Length < 2) {
//         return EFI_INVALID_PARAMETER;
//     }

//     Ptr = Url + Parser->FieldData[HTTP_URI_FIELD_HOST].Offset;
//     if ((Ptr[0] != '[') || (Ptr[Length - 1] != ']')) {
//         return EFI_INVALID_PARAMETER;
//     }

//     Ip6String = AllocatePool(Length);
//     if (Ip6String == NULL) {
//         return EFI_OUT_OF_RESOURCES;
//     }

//     Status = UriPercentDecode(Ptr + 1, Length - 2, Ip6String, &ResultLength);
//     if (EFI_ERROR(Status)) {
//         FreePool(Ip6String);
//         return Status;
//     }

//     Ip6String[ResultLength] = '\0';
//     Status = NetLibAsciiStrToIp6(Ip6String, Ip6Address);
//     FreePool(Ip6String);

//     return Status;
// }

/**
  Get the port number from a HTTP URL.

  This function will return the port number according to the Url and previous parse result.

  @param[in]    Url                The pointer to a HTTP URL string.
  @param[in]    UrlParser          URL Parse result returned by NetHttpParseUrl().
  @param[out]   Port               Pointer to a buffer to store the port number.

  @retval EFI_SUCCESS              Successfully get the required component.
  @retval EFI_INVALID_PARAMETER    Uri is NULL or Port is NULL or UrlParser is invalid.
  @retval EFI_NOT_FOUND            No port number in the URL.
  @retval EFI_OUT_OF_RESOURCES     Could not allocate needed resources.

**/
EFI_STATUS
EFIAPI
HttpUrlGetPort(IN CHAR8* Url, IN VOID* UrlParser, OUT UINT16* Port)
{
    CHAR8* PortString;
    EFI_STATUS Status;
    UINTN Index;
    UINTN Data;
    UINT32 ResultLength;
    HTTP_URL_PARSER* Parser;

    if (Url == NULL || UrlParser == NULL || Port == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    *Port = 0;
    Index = 0;

    Parser = (HTTP_URL_PARSER*)UrlParser;

    if ((Parser->FieldBitMap & BIT(HTTP_URI_FIELD_PORT)) == 0) {
        return EFI_NOT_FOUND;
    }

    PortString = AllocatePool(Parser->FieldData[HTTP_URI_FIELD_PORT].Length + 1);
    if (PortString == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    Status = UriPercentDecode(Url + Parser->FieldData[HTTP_URI_FIELD_PORT].Offset,
                              Parser->FieldData[HTTP_URI_FIELD_PORT].Length,
                              PortString,
                              &ResultLength);
    if (EFI_ERROR(Status)) {
        goto ON_EXIT;
    }

    PortString[ResultLength] = '\0';

    while (Index < ResultLength) {
        if (!NET_IS_DIGIT(PortString[Index])) {
            Status = EFI_INVALID_PARAMETER;
            goto ON_EXIT;
        }
        Index++;
    }

    Status = AsciiStrDecimalToUintnS(Url + Parser->FieldData[HTTP_URI_FIELD_PORT].Offset,
                                     (CHAR8**)NULL,
                                     &Data);

    if (Data > HTTP_URI_PORT_MAX_NUM) {
        Status = EFI_INVALID_PARAMETER;
        goto ON_EXIT;
    }

    *Port = (UINT16)Data;

ON_EXIT:
    FreePool(PortString);
    return Status;
}

// /**
//   Get the Path from a HTTP URL.

//   This function will return the Path according to the Url and previous parse result,and
//   it is the caller's responsibility to free the buffer returned in *Path.

//   @param[in]    Url                The pointer to a HTTP URL string.
//   @param[in]    UrlParser          URL Parse result returned by NetHttpParseUrl().
//   @param[out]   Path               Pointer to a buffer to store the Path.

//   @retval EFI_SUCCESS              Successfully get the required component.
//   @retval EFI_INVALID_PARAMETER    Uri is NULL or HostName is NULL or UrlParser is invalid.
//   @retval EFI_NOT_FOUND            No hostName component in the URL.
//   @retval EFI_OUT_OF_RESOURCES     Could not allocate needed resources.

// **/
// EFI_STATUS
// EFIAPI
// HttpUrlGetPath(IN CHAR8* Url, IN VOID* UrlParser, OUT CHAR8** Path)
// {
//     CHAR8* PathStr;
//     EFI_STATUS Status;
//     UINT32 ResultLength;
//     HTTP_URL_PARSER* Parser;

//     if (Url == NULL || UrlParser == NULL || Path == NULL) {
//         return EFI_INVALID_PARAMETER;
//     }

//     Parser = (HTTP_URL_PARSER*)UrlParser;

//     if ((Parser->FieldBitMap & BIT(HTTP_URI_FIELD_PATH)) == 0) {
//         return EFI_NOT_FOUND;
//     }

//     PathStr = AllocatePool(Parser->FieldData[HTTP_URI_FIELD_PATH].Length + 1);
//     if (PathStr == NULL) {
//         return EFI_OUT_OF_RESOURCES;
//     }

//     Status = UriPercentDecode(Url + Parser->FieldData[HTTP_URI_FIELD_PATH].Offset,
//                               Parser->FieldData[HTTP_URI_FIELD_PATH].Length,
//                               PathStr,
//                               &ResultLength);
//     if (EFI_ERROR(Status)) {
//         FreePool(PathStr);
//         return Status;
//     }

//     PathStr[ResultLength] = '\0';
//     *Path = PathStr;
//     return EFI_SUCCESS;
// }

/**
  Release the resource of the URL parser.

  @param[in]    UrlParser            Pointer to the parser.

**/
VOID EFIAPI HttpUrlFreeParser(IN VOID* UrlParser)
{
    FreePool(UrlParser);
}
#endif

#ifndef UEFI_BUILD_SYSTEM
#pragma prefast(pop)
#endif