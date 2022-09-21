#ifndef _EDK2COMPAT_BASELIB_H_
#define _EDK2COMPAT_BASELIB_H_

#ifndef UEFI_BUILD_SYSTEM
#if defined(_WIN64)
#define UINTN_MAX UINT64_MAX
#else
#define UINTN_MAX UINT32_MAX
#endif
#else
#define UINTN_MAX  MAX_UINTN
#define UINT32_MAX MAX_UINT32
#endif

#ifndef UEFI_BUILD_SYSTEM
// Declared in MdePkg\Include\Library\BaseMemoryLib.h
VOID* EFIAPI CopyMem(OUT VOID* DestinationBuffer, IN const VOID* SourceBuffer, IN UINTN Length);
VOID* EFIAPI MoveMem(OUT VOID* DestinationBuffer, IN const VOID* SourceBuffer, IN UINTN Length);
VOID* EFIAPI SetMem(OUT VOID* Buffer, IN UINTN Length, IN UINT8 Value);
VOID* EFIAPI ZeroMem(OUT VOID* Buffer, IN UINTN Length);
INTN EFIAPI CompareMem(IN const VOID* DestinationBuffer,
                       IN const VOID* SourceBuffer,
                       IN UINTN Length);
BOOLEAN EFIAPI CompareGuid(IN const EFI_GUID* Guid1, IN const EFI_GUID* Guid2);
BOOLEAN EFIAPI IsGuidEmpty(_In_ const EFI_GUID* Guid);

// Declared in MdePkg\Include\Library\MemoryAllocationLib.h
VOID* EFIAPI AllocatePool(IN UINTN AllocationSize);
VOID* EFIAPI AllocateZeroPool(IN UINTN AllocationSize);
VOID* EFIAPI AllocateCopyPool(IN UINTN AllocationSize, IN VOID* Buffer);
VOID* EFIAPI ReallocatePool(IN UINTN OldSize, IN UINTN NewSize, IN VOID* Buffer);
VOID EFIAPI FreePool(IN OPTIONAL VOID* Buffer);

// Declared in MdePkg\Include\Library\BaseLib.h
EFI_STATUS EFIAPI StrDup(IN const CHAR16* FirstString, IN const CHAR16** OutString);
INTN EFIAPI StrCmp(IN const CHAR16* FirstString, IN const CHAR16* SecondString);
INTN EFIAPI StrnCmp(IN const CHAR16* FirstString, IN const CHAR16* SecondString, IN UINTN Length);
INTN EFIAPI StrniCmp(IN const CHAR16* FirstString, IN const CHAR16* SecondString, IN UINTN Length);
EFI_STATUS EFIAPI AsciiStrDup(IN const CHAR8* FirstString, IN const CHAR8** OutString);
INTN EFIAPI AsciiStrCmp(IN const CHAR8* FirstString, IN const CHAR8* SecondString);
INTN EFIAPI AsciiStrnCmp(IN const CHAR8* FirstString,
                         IN const CHAR8* SecondString,
                         IN UINTN Length);
INTN EFIAPI AsciiStrniCmp(IN const CHAR8* FirstString,
                          IN const CHAR8* SecondString,
                          IN UINTN Length);
UINTN EFIAPI StrLen(IN const CHAR16* String);
CHAR16* EFIAPI StrStr(IN const CHAR16* String, IN const CHAR16* SearchString);
BOOLEAN EFIAPI StrEndsWith(IN const CHAR16* String, IN const CHAR16* SearchString);
CHAR16* EFIAPI StrCpy(OUT CHAR16* DestinationStr, IN const CHAR16* SourceStr);
BOOLEAN EFIAPI StrIsEmpty(IN CHAR16* String);
BOOLEAN EFIAPI AsciiStrIsEmpty(IN CHAR8* String);
CHAR16* EFIAPI StrUpper(IN CHAR16* String);
CHAR16* EFIAPI StrLower(IN CHAR16* String);
CHAR8* EFIAPI AsciiStrUpper(IN CHAR8* String);
CHAR8* EFIAPI AsciiStrLower(IN CHAR8* String);
CHAR8 EFIAPI AsciiCharToUpper(IN CHAR8 Char);
CHAR8 EFIAPI AsciiCharToLower(IN CHAR8 Char);
CHAR16 EFIAPI CharToUpper(IN CHAR16 Char);
CHAR16 EFIAPI CharToLower(IN CHAR16 Char);
UINTN EFIAPI AsciiStrLen(IN const CHAR8* String);
CHAR8* EFIAPI AsciiStrStr(IN const CHAR8* String, IN const CHAR8* SearchString);
BOOLEAN EFIAPI AsciiStrEndsWith(IN const CHAR8* String, IN const CHAR8* SearchString);
CHAR8* EFIAPI AsciiStrChr(IN const CHAR8* String, IN const CHAR8 SearchCharacter);
#endif

CHAR8* EFIAPI AsciiStrnCpy(OUT CHAR8* DestinationStr, IN const CHAR8* SourceStr, IN UINTN NumChars);

//
// Secure function versions
//

EFI_STATUS EFIAPI CopyMemS(OUT VOID* DestinationBuffer,
                           IN UINTN DestSize,
                           IN const VOID* SourceBuffer,
                           IN UINTN Length);
EFI_STATUS EFIAPI SetMemS(OUT VOID* DestinationBuffer,
                          IN UINTN DestSize,
                          IN UINTN Length,
                          IN UINT8 Value);
EFI_STATUS EFIAPI ZeroMemS(OUT VOID* DestinationBuffer, IN UINTN DestSize, IN UINTN Length);
EFI_STATUS EFIAPI StrTrimS(IN OUT CHAR16* String, IN UINTN MaxStringSize);
EFI_STATUS EFIAPI AsciiStrTrimS(IN OUT CHAR8* String, IN UINTN MaxStringSize);

CHAR16* EFIAPI StrChr(IN const CHAR16* String, IN const CHAR16 SearchCharacter);
CHAR16* EFIAPI StrnCpy(OUT CHAR16* DestinationStr, IN const CHAR16* SourceStr, IN UINTN NumChars);

_Ret_z_ CHAR16* EFIAPI AsciiStrToUnicodeStr(IN const CHAR8* Source, OUT CHAR16* Destination);
_Ret_z_ CHAR8* EFIAPI UnicodeStrToAsciiStr(IN const CHAR16* Source, OUT CHAR8* Destination);

#ifdef UEFI_BUILD_SYSTEM
EFI_STATUS
EFIAPI
StrHexToUintnMsExtension(IN CONST CHAR16* String, OUT CHAR16** EndPointer, OUT UINTN* Data);
#else
EFI_STATUS
EFIAPI
StrHexToUintn(IN CONST CHAR16* String, OUT CHAR16** EndPointer, OUT UINTN* Data);
#endif

#ifndef UEFI_BUILD_SYSTEM
EFI_STATUS EFIAPI AsciiStrDecimalToUintnS(IN CONST CHAR8* String,
                                          OUT CHAR8** EndPointer,
                                          OUT UINTN* Data);
#endif

#ifdef UEFI_BUILD_SYSTEM
EFI_STATUS
EFIAPI
StrDecimalToUintnMsExtension(IN CONST CHAR16* String, OUT CHAR16** EndPointer, OUT UINTN* Data);
#else
EFI_STATUS
EFIAPI
StrDecimalToUintn(IN CONST CHAR16* String, OUT CHAR16** EndPointer, OUT UINTN* Data);
UINTN EFIAPI AsciiHexCharToUintn(IN CHAR8 Char);
UINTN EFIAPI AsciiStrHexToUintn(IN CONST CHAR8* String);
EFI_STATUS EFIAPI AsciiStrHexToUintnS(IN CONST CHAR8* String,
                                      OUT CHAR8** EndPointer,
                                      OPTIONAL OUT UINTN* Data);
#endif
CHAR8* EFIAPI IntegerToString(_In_ INTN Number,
                              _In_ UINTN Radix,
                              _Out_writes_(StringBufferSize) CHAR8 StringBuffer[],
                              _In_ UINTN StringBufferSize);
UINTN
EFIAPI
AsciiStrDecimalToUintn(IN CONST CHAR8* String);
INTN EFIAPI AsciiStriCmp(IN CONST CHAR8* FirstString, IN CONST CHAR8* SecondString);

#ifndef UEFI_BUILD_SYSTEM
// Guid to String Helper
VOID EFIAPI GuidToWideString(_In_ const GUID* Guid,
                             _Out_writes_z_(BufferSize) CHAR16 Buffer[],
                             _In_ UINT8 BufferSize);

// Declared in MdePkg\Include\Library\UefiLib.h
typedef enum { EfiLockUninitialized = 0, EfiLockReleased = 1, EfiLockAcquired = 2 } EFI_LOCK_STATE;

typedef struct {
    EFI_TPL Tpl;
    EFI_TPL OwnerTpl;
    EFI_LOCK_STATE Lock;
} EFI_LOCK;

#define EFI_INITIALIZE_LOCK_VARIABLE(Priority)     \
    {                                              \
        Priority, TPL_APPLICATION, EfiLockReleased \
    }

VOID EFIAPI EfiAcquireLock(IN EFI_LOCK* Lock);

EFI_STATUS
EFIAPI
EfiAcquireLockOrFail(IN EFI_LOCK* Lock);

VOID EFIAPI EfiReleaseLock(IN EFI_LOCK* Lock);
EFI_TPL EFIAPI EfiGetCurrentTpl(VOID);
#endif

EFI_STATUS
EFIAPI
EfiSleep(UINTN DurationInNS);

#ifndef UEFI_BUILD_SYSTEM
// Declared in MdeModulePkg/Include/Library/SortLib.h
#ifndef __SORT_LIB_H__
#define __SORT_LIB_H__

/**
  Prototype for comparison function for any two element types.
  @param[in] Buffer1                  The pointer to first buffer.
  @param[in] Buffer2                  The pointer to second buffer.
  @retval 0                           Buffer1 equal to Buffer2.
  @return <0                          Buffer1 is less than Buffer2.
  @return >0                          Buffer1 is greater than Buffer2.
**/
typedef INTN(EFIAPI* SORT_COMPARE)(IN CONST VOID* Buffer1, IN CONST VOID* Buffer2);

/**
  Function to perform a Quick Sort on a buffer of comparable elements.
  Each element must be equally sized.
  If BufferToSort is NULL, then ASSERT.
  If CompareFunction is NULL, then ASSERT.
  If Count is < 2 , then perform no action.
  If Size is < 1 , then perform no action.
  @param[in, out] BufferToSort   On call, a Buffer of (possibly sorted)
elements; on return, a buffer of sorted elements.
  @param[in]  Count              The number of elements in the buffer to sort.
  @param[in]  ElementSize        The size of an element in bytes.
  @param[in]  CompareFunction    The function to call to perform the comparison
                                 of any two elements.
**/
VOID EFIAPI PerformQuickSort(IN OUT VOID* BufferToSort,
                             IN CONST UINTN Count,
                             IN CONST UINTN ElementSize,
                             IN SORT_COMPARE CompareFunction);

#endif //__SORT_LIB_H__
#endif

#ifndef UEFI_BUILD_SYSTEM
CHAR8* EFIAPI AsciiStrCpy(OUT CHAR8* DestinationStr, IN const CHAR8* SourceStr);
CHAR8* EFIAPI AsciiStrCat(IN OUT CHAR8* Destination, IN CONST CHAR8* Source);
EFI_STATUS EFIAPI AsciiStrCatS(IN OUT CHAR8* Destination, IN UINTN DestMax, IN CONST CHAR8* Source);
EFI_STATUS EFIAPI AsciiStrCpyS(OUT CHAR8* Destination, IN UINTN DestMax, IN CONST CHAR8* Source);
EFI_STATUS EFIAPI AsciiStrnCpyS(OUT CHAR8* Destination,
                                IN UINTN DestMax,
                                IN CONST CHAR8* Source,
                                IN UINTN Length);
UINTN EFIAPI AsciiStrnLenS(IN CONST CHAR8* String, IN UINTN MaxSize);
UINTN EFIAPI StrnLenS(IN CONST CHAR16* String, IN UINTN MaxSize);
EFI_STATUS EFIAPI Base64DecodeEdk(IN CONST CHAR8* Source OPTIONAL,
                                  IN UINTN SourceSize,
                                  OUT UINT8* Destination OPTIONAL,
                                  IN OUT UINTN* DestinationSize);

// Declared in NetworkPkg/Include/Library/HttpLib.h

EFI_STATUS EFIAPI HttpParseUrl(IN CHAR8* Url,
                               IN UINT32 Length,
                               IN BOOLEAN IsConnectMethod,
                               OUT VOID** UrlParser);
EFI_STATUS EFIAPI HttpUrlGetHostName(IN CHAR8* Url, IN VOID* UrlParser, OUT CHAR8** HostName);
EFI_STATUS EFIAPI HttpUrlGetPort(IN CHAR8* Url, IN VOID* UrlParser, OUT UINT16* Port);
VOID EFIAPI HttpUrlFreeParser(IN VOID* UrlParser);
#endif
#endif // _EDK2COMPAT_BASELIB_H_
