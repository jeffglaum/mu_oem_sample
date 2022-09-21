//
// Copyright (c) 2016  Microsoft Corporation
//
// This file contains stubs of various functions that are normally defined in bootlibs\misc.lib,
// but because efi_min doesn't link to that, the stubs are required to keep the linker happy.
//

#include <bootlib.h>
#include <strsafe.h>

// --------------------------------------------------------------------- Macros

#define UPCASE(_c) (WCHAR)(('a' <= (_c) && (_c) <= 'z') ? ((_c) - ('a' - 'A')) : (_c))

#ifdef NO_BOOTLIBRARY_CALLS

extern EFI_BOOT_SERVICES* gBootServices;
extern EFI_RUNTIME_SERVICES* gRuntimeServices;

//
// The SymCrypt Bootlib environment (which is the default if you link with misc.lib)
// pulls in too many pieces of Boot library.
// The generic environment works without dependencies.
//
SYMCRYPT_ENVIRONMENT_GENERIC

// Stubs required by  SYMCRYPT_ENVIRONMENT_GENERIC
void __fastcall __security_check_cookie(DWORD_PTR cookie)
{
    UNREFERENCED_PARAMETER(cookie);
    return;
}

// Stubs required by libcboot.lib
#ifdef _WIN64
#define DEFAULT_SECURITY_COOKIE 0x00002B992DDFA232ui64
#else
#define DEFAULT_SECURITY_COOKIE 0x0000BB40u
#endif // _WIN64
DECLSPEC_SELECTANY DWORD_PTR __security_cookie = DEFAULT_SECURITY_COOKIE;
DECLSPEC_SELECTANY DWORD_PTR __security_cookie_complement = ~DEFAULT_SECURITY_COOKIE;

void __cdecl _invalid_parameter(const wchar_t* pszExpression,
                                const wchar_t* pszFunction,
                                const wchar_t* pszFile,
                                unsigned int nLine,
                                uintptr_t pReserved)

{
    UNREFERENCED_PARAMETER(pszExpression);
    UNREFERENCED_PARAMETER(pszFunction);
    UNREFERENCED_PARAMETER(pszFile);
    UNREFERENCED_PARAMETER(nLine);
    UNREFERENCED_PARAMETER(pReserved);

    ASSERT(FALSE);
}

NTSTATUS
RtlUnicodeToMultiByteN(__out_bcount_part(MaxBytesInMultiByteString, *BytesInMultiByteString)
                           PCHAR MultiByteString,
                       __in ULONG MaxBytesInMultiByteString,
                       __out_opt PULONG BytesInMultiByteString,
                       __in_bcount(BytesInUnicodeString) PCWCH UnicodeString,
                       __in ULONG BytesInUnicodeString)

{
    UNREFERENCED_PARAMETER(MultiByteString);
    UNREFERENCED_PARAMETER(MaxBytesInMultiByteString);
    UNREFERENCED_PARAMETER(BytesInMultiByteString);
    UNREFERENCED_PARAMETER(UnicodeString);
    UNREFERENCED_PARAMETER(BytesInUnicodeString);

    return STATUS_NOT_SUPPORTED;
}

// Stubs required by safecrtnt.lib
WCHAR
RtlAnsiCharToUnicodeChar(__deref_inout PUCHAR* SourceCharacter)
{
    WCHAR UnicodeCharacter;

    UnicodeCharacter = (WCHAR) * *SourceCharacter;
    (*SourceCharacter)++;
    return UnicodeCharacter;
}

//
// Stubs required bldrrtl.lib
//

//
// Required by RTL's time format conversion functions. Value will be NULL,
// which is OK since this module should ignore leap seconds.
//

PLEAP_SECOND_DATA BlLeapSecondData;

// RtlAssert calls both ZwTerminateProcess and ZwTerminateThread.  These have
// no meaning in the boot environment.  They are implemented in ke.lib and
// we could include ke.lib for their definitions.  But doing so introduces
// a whole new can of dependencies that should not exist in the boot
// environment.  Instead, thunk them out here.
//

NTSTATUS
ZwTerminateProcess(__in HANDLE Process OPTIONAL, __in NTSTATUS Status)

{
    UNREFERENCED_PARAMETER(Process);
    UNREFERENCED_PARAMETER(Status);

    return STATUS_SUCCESS;
}

NTSTATUS
ZwTerminateThread(__in HANDLE Thread OPTIONAL, __in NTSTATUS Status)

{
    UNREFERENCED_PARAMETER(Thread);
    UNREFERENCED_PARAMETER(Status);

    return STATUS_SUCCESS;
}

// Stubs required by  asn1util.obj
__declspec(noreturn) void __cdecl __report_securityfailure(ULONG FailureCode)
{
    UNREFERENCED_PARAMETER(FailureCode);
}

__declspec(noreturn) void __cdecl __report_rangecheckfailure(void)
{
#define RANGE_CHECKS_FAILURE 0
    __report_securityfailure(RANGE_CHECKS_FAILURE);
}

// Stubs required by  EFIFlash_min.lib
EFI_STATUS
IsPlatformCheckDisabled(_Out_ BOOLEAN* pfPlatformCheckDisabled)
{
    *pfPlatformCheckDisabled = FALSE;
    return EFI_SUCCESS;
}

NTSTATUS
EfiBootModeMgmtSetBootModeInfo(__in EFI_BOOT_MODE_MGMT_PROTOCOL* Protocol,
                               __in EFI_OS_BOOT_MODE BootMode,
                               __in UINT32 ProfileNameElements,
                               __in_ecount_opt(ProfileNameElements) PWCHAR ProfileName)
{
    UNREFERENCED_PARAMETER(Protocol);
    UNREFERENCED_PARAMETER(BootMode);
    UNREFERENCED_PARAMETER(ProfileNameElements);
    UNREFERENCED_PARAMETER(ProfileName);

    return EfiGetNtStatusCode(EFI_UNSUPPORTED);
}

VOID RtlRaiseException(IN PEXCEPTION_RECORD ExceptionRecord)

/*++

Routine Description:

    Provides a stub version of this RTL API which is referenced by a CRT
    function.

Arguments:

    ExceptionRecord - Provides a pointer to the exception record.

Returns:

    None.

--*/

{
    UNREFERENCED_PARAMETER(ExceptionRecord);
    __debugbreak(); // was ASSERT(FALSE) in NT sources
}

PVOID
BlMmAllocateHeap(__in UINTN Size)
{
    PVOID Ptr = NULL;
    gBootServices->AllocatePool(EfiBootServicesData, Size, &Ptr);
    return Ptr;
}

NTSTATUS
BlMmFreeHeap(__in PVOID Ptr)
{
    if (NULL == Ptr) {
        return STATUS_INVALID_PARAMETER;
    }

    gBootServices->FreePool(Ptr);
    return STATUS_SUCCESS;
}

// The UEFI spec requires that functions use cdecl, but the RSA library
// is expecting these functions (RSA32Alloc & RSA32Free) to be stdcall.
// Since these functions are not called directly from any UEFI functions
// that would be expecting cdecl, leaving as stdcall to allow linking
// to succeed.
PVOID
__stdcall RSA32Alloc(__in ULONG Size)

/*++

Routine Description:

    Allocates a Size byte buffer using the Heap Allocator.

    N.B. Returned memory buffers are not zeroed.

Arguments:

    Size - Size (in bytes) of the buffer to allocate.

Return Value:

    A pointer to the allocated memory buffer.
    NULL if request fails

--*/

{
    PVOID Ptr = NULL;
    gBootServices->AllocatePool(EfiBootServicesData, Size, &Ptr);
    return Ptr;
}

VOID __stdcall RSA32Free(__in PVOID Ptr)

/*++

Routine Description:

    Frees an allocation made by RSA32Alloc.

Arguments:

    Ptr - Buffer to free.

Return Value:

    None

--*/

{
    if (Ptr) {
        gBootServices->FreePool(Ptr);
    }

    return;
}

// Copied from efifw.c
const EFI_GUID EfiCertX509Guid = EFI_CERT_X509_GUID;
const EFI_GUID EfiCertRsa2048Guid = EFI_CERT_RSA2048_GUID;
const EFI_GUID EfiGlobalVariable = EFI_GLOBAL_VARIABLE;

// Copied from efilib.c
EFI_STATUS
EfiGetEfiStatusCode(__in NTSTATUS NtStatus)

/*++

Routine Description:

    This routine returns the closest equivalent EFI status code for the
    specified NT status code.

Arguments:

    NtStatus - Supplies a NT status code.

Return Value:

    EFI status code.

--*/

{
    switch (NtStatus) {
        case STATUS_SUCCESS:
            return EFI_SUCCESS;

        case STATUS_DRIVER_UNABLE_TO_LOAD:
            return EFI_LOAD_ERROR;

        case STATUS_INVALID_PARAMETER:
            return EFI_INVALID_PARAMETER;

        case STATUS_NOT_SUPPORTED:
            return EFI_UNSUPPORTED;

        case STATUS_INVALID_BUFFER_SIZE:
            return EFI_BAD_BUFFER_SIZE;

        case STATUS_BUFFER_TOO_SMALL:
            return EFI_BUFFER_TOO_SMALL;

        case STATUS_IO_DEVICE_ERROR:
            return EFI_DEVICE_ERROR;

        case STATUS_MEDIA_WRITE_PROTECTED:
            return EFI_WRITE_PROTECTED;

        case STATUS_INSUFFICIENT_RESOURCES:
            return EFI_OUT_OF_RESOURCES;

        case STATUS_DISK_CORRUPT_ERROR:
            return EFI_VOLUME_CORRUPTED;

        case STATUS_DISK_FULL:
            return EFI_VOLUME_FULL;

        case STATUS_NO_MEDIA:
            return EFI_NO_MEDIA;

        case STATUS_MEDIA_CHANGED:
            return EFI_MEDIA_CHANGED;

        case STATUS_NOT_FOUND:
            return EFI_NOT_FOUND;

        case STATUS_ACCESS_DENIED:
            return EFI_ACCESS_DENIED;

        case STATUS_NO_MATCH:
            return EFI_NO_MAPPING;

        case STATUS_TIMEOUT:
            return EFI_TIMEOUT;

        case STATUS_DEVICE_NOT_READY:
            return EFI_NOT_STARTED;

        case STATUS_DEVICE_ALREADY_ATTACHED:
            return EFI_ALREADY_STARTED;

        case STATUS_REQUEST_ABORTED:
            return EFI_ABORTED;

        default:
            return EFI_NO_MAPPING;
    }
}

NTSTATUS
EfiGetNtStatusCode(__in EFI_STATUS Status)

/*++

Routine Description:

    This routine returns the closest equivalent NT status code for the
    specified EFI status code.

Arguments:

    Status - Supplies an EFI status code.

Return Value:

    NT status code.

--*/

{
    switch (Status) {
        case EFI_SUCCESS:
            return STATUS_SUCCESS;

        case EFI_LOAD_ERROR:
            return STATUS_DRIVER_UNABLE_TO_LOAD;

        case EFI_INVALID_PARAMETER:
            return STATUS_INVALID_PARAMETER;

        case EFI_UNSUPPORTED:
            return STATUS_NOT_SUPPORTED;

        case EFI_BAD_BUFFER_SIZE:
            return STATUS_INVALID_BUFFER_SIZE;

        case EFI_BUFFER_TOO_SMALL:
            return STATUS_BUFFER_TOO_SMALL;

        case EFI_NOT_READY:
            return STATUS_NOT_FOUND;

        case EFI_DEVICE_ERROR:
            return STATUS_IO_DEVICE_ERROR;

        case EFI_WRITE_PROTECTED:
            return STATUS_MEDIA_WRITE_PROTECTED;

        case EFI_OUT_OF_RESOURCES:
            return STATUS_INSUFFICIENT_NVRAM_RESOURCES;

        case EFI_VOLUME_CORRUPTED:
            return STATUS_DISK_CORRUPT_ERROR;

        case EFI_VOLUME_FULL:
            return STATUS_DISK_FULL;

        case EFI_NO_MEDIA:
            return STATUS_NO_MEDIA;

        case EFI_MEDIA_CHANGED:
            return STATUS_MEDIA_CHANGED;

        case EFI_NOT_FOUND:
            return STATUS_NOT_FOUND;

        case EFI_ACCESS_DENIED:
        case EFI_SECURITY_VIOLATION:
            return STATUS_ACCESS_DENIED;

        case EFI_NO_RESPONSE:
            return STATUS_TIMEOUT;

        case EFI_NO_MAPPING:
            return STATUS_NO_MATCH;

        case EFI_TIMEOUT:
            return STATUS_TIMEOUT;

        case EFI_NOT_STARTED:
            return STATUS_DEVICE_NOT_READY;

        case EFI_ALREADY_STARTED:
            return STATUS_DEVICE_ALREADY_ATTACHED;

        case EFI_ABORTED:
            return STATUS_REQUEST_ABORTED;

        case EFI_ICMP_ERROR:
        case EFI_TFTP_ERROR:
        case EFI_PROTOCOL_ERROR:
        // case EFI_INCOMPATIBLE_VERSION:
        // case EFI_CRC_ERROR:
        default:
            return STATUS_UNSUCCESSFUL;
    }
}

// adapted from efiapi.c
NTSTATUS
EfiGetVariable(__in PCWCHAR VariableName,
               __in const EFI_GUID* VendorGuid,
               __out_opt PULONG Attributes,
               __inout UINTN* DataSize,
               __out_bcount_opt(*DataSize) PVOID Data)

/*++

Routine Description:

    This routine is a wrapper for the EFI service, GetVariable which will
    get the vendor data stored in NVRAM.

Arguments:

    VariableName - Null-terminated unicode string that is the name of the
                   vendor's variable.

    VendorGuid - A unique identifier for the vendor.  The namespace is
                 partitioned by vendor to avoid namespace collision.
                 This value specifies the partition containing the variable.

    Attributes - Specifies the access conditions for the variable.  Possible
                 values include:

                 EFI_VARIABLE_NON_VOLATILE - Variable is persistent across
                                             reboot.

                 EFI_VARIABLE_BOOTSERVICE_ACCESS - Variable can only be
                                                   accessed during boot
                                                   services.

                 EFI_VARIABLE_RUNTIME_ACCESS - Variable can be accessed after
                                               ExitBootServices has been
                                               called.

    DataSize - On input, contains the size (in bytes) of Data.  On output,
               contains the size of the data returned in Data.

    Data - Buffer to receive the contents of the variable.

Return Value:

    STATUS_SUCCESS when successful.
    STATUS_NOT_FOUND if variable was not found.
    STATUS_BUFFER_TOO_SMALL if DataSize is too small for the result.
    STATUS_INVALID_PARAMETER if one of the parameters was invalid.

--*/

{
    UINT32 Attrib;
    EFI_STATUS Status;

    //
    // Make the requested service call.
    //

    Status = gRuntimeServices->GetVariable((PWSTR)VariableName,
                                           (EFI_GUID*)VendorGuid,
                                           &Attrib,
                                           DataSize,
                                           Data);

    //
    // Return attributes.
    //

    if (Attributes != NULL) {
        *Attributes = Attrib;
    }

    return EfiGetNtStatusCode(Status);
}

//
// This is the maximum MaximumLength for a UNICODE_STRING.
//

#define MAX_USTRING (sizeof(WCHAR) * (MAXUSHORT / sizeof(WCHAR)))

VOID RtlInitUnicodeString(__out PUNICODE_STRING DestinationString, __in_z_opt PCWSTR SourceString)

/*++

Routine Description:

    The RtlInitUnicodeString function initializes an NT counted
    unicode string.  The DestinationString is initialized to point to
    the SourceString and the Length and MaximumLength fields of
    DestinationString are initialized to the length of the SourceString,
    which is zero if SourceString is not specified.

Arguments:

    DestinationString - Pointer to the counted string to initialize

    SourceString - Optional pointer to a null terminated unicode string that
        the counted string is to point to.


Return Value:

    None.

--*/

{
    ULONG Length;

    DestinationString->Buffer = (PWSTR)SourceString;
    if (ARGUMENT_PRESENT(SourceString)) {
        Length = (ULONG)wcslen(SourceString) * sizeof(WCHAR);

        ASSERT(Length < MAX_USTRING);

        if (Length >= MAX_USTRING) {
            Length = MAX_USTRING - sizeof(UNICODE_NULL);
        }

        DestinationString->Length = (USHORT)Length;
        DestinationString->MaximumLength = (USHORT)(Length + sizeof(UNICODE_NULL));

    } else {
        DestinationString->MaximumLength = 0;
        DestinationString->Length = 0;
    }

    return;
}

VOID RtlCopyUnicodeString(__inout PUNICODE_STRING DestinationString,
                          __in_opt PCUNICODE_STRING SourceString)

/*++

Routine Description:

    The RtlCopyUnicodeString function copies the SourceString to the
    DestinationString.  If SourceString is not specified, then
    the Length field of DestinationString is set to zero.  The
    MaximumLength and Buffer fields of DestinationString are not
    modified by this function.

    The number of bytes copied from the SourceString is either the
    Length of SourceString or the MaximumLength of DestinationString,
    whichever is smaller.

    Copied verbatim from minkernel\ntos\rtl\nls.c but with BlpInterface* added.

Arguments:

    DestinationString - Pointer to the destination string.

    SourceString - Optional pointer to the source string.

Return Value:

    None.

--*/

{
    UNALIGNED WCHAR *src, *dst;
    ULONG n;

    if (ARGUMENT_PRESENT(SourceString)) {
        dst = DestinationString->Buffer;
        src = SourceString->Buffer;
        n = SourceString->Length;
        if ((USHORT)n > DestinationString->MaximumLength) {
            n = DestinationString->MaximumLength;
        }

        DestinationString->Length = (USHORT)n;
        RtlCopyMemory(dst, src, n);
        if ((DestinationString->Length + sizeof(WCHAR)) <= DestinationString->MaximumLength) {
            dst[n / sizeof(WCHAR)] = UNICODE_NULL;
        }

    } else {
        DestinationString->Length = 0;
    }

    return;
}

const PRTL_ALLOCATE_STRING_ROUTINE RtlAllocateStringRoutine = (PRTL_ALLOCATE_STRING_ROUTINE)
    BlMmAllocateHeap;
const PRTL_FREE_STRING_ROUTINE RtlFreeStringRoutine = BlMmFreeHeap;

#ifndef NTSTRSAFE_UNICODE_STRING_MAX_CCH
#define NTSTRSAFE_UNICODE_STRING_MAX_CCH \
    (0xffff / sizeof(wchar_t)) // max buffer size, in characters, for a UNICODE_STRING
#endif

#define STRSAFE_IGNORE_NULLS           0x00000100
#define STRSAFE_FILL_BEHIND            0x00000200
#define STRSAFE_ZERO_LENGTH_ON_FAILURE 0x00000800
#define STRSAFE_UNICODE_STRING_VALID_FLAGS                                               \
    (0x000000FF | STRSAFE_IGNORE_NULLS | STRSAFE_FILL_BEHIND | STRSAFE_FILL_ON_FAILURE | \
     STRSAFE_ZERO_LENGTH_ON_FAILURE | STRSAFE_NO_TRUNCATION)

STRSAFEWORKERAPI
RtlUnicodeStringValidateWorker(_In_opt_ PCUNICODE_STRING SourceString,
                               _In_ const size_t cchMax,
                               _In_ DWORD dwFlags)
{
    NTSTATUS status = STATUS_SUCCESS;

    if (SourceString || !(dwFlags & STRSAFE_IGNORE_NULLS)) {
        if (((SourceString->Length % sizeof(wchar_t)) != 0) ||
            ((SourceString->MaximumLength % sizeof(wchar_t)) != 0) ||
            (SourceString->Length > SourceString->MaximumLength) ||
            (SourceString->MaximumLength > (cchMax * sizeof(wchar_t)))) {
            status = STATUS_INVALID_PARAMETER;
        } else if ((SourceString->Buffer == NULL) &&
                   ((SourceString->Length != 0) || (SourceString->MaximumLength != 0))) {
            status = STATUS_INVALID_PARAMETER;
        }
    }

    return status;
}

STRSAFEAPI
RtlUnicodeStringValidateEx(_In_ PCUNICODE_STRING SourceString, _In_ DWORD dwFlags)

/*++

    NTSTATUS
    RtlUnicodeStringValidateEx(
    _In_ PCUNICODE_STRING    SourceString     OPTIONAL,
    _In_ DWORD               dwFlags
    );

    Routine Description:

    In addition to functionality provided by RtlUnicodeStringValidate, this routine
    includes the flags parameter allows additional controls.

    This function returns an NTSTATUS value.  It returns STATUS_SUCCESS if the
    counted unicode string is valid.

Arguments:

SourceString   - pointer to the counted unicode string to be checked

dwFlags        - controls some details of the validation:

STRSAFE_IGNORE_NULLS
allows SourceString to be NULL (will return STATUS_SUCCESS for this case).

Return Value:

STATUS_SUCCESS -   SourceString is a valid counted unicode string

failure        -   the operation did not succeed

STATUS_INVALID_PARAMETER
-   this return value is an indication that the source string
is not a valide counted unicode string given the flags passed.

It is strongly recommended to use the NT_SUCCESS() macro to test the
return value of this function.

--*/

{
    NTSTATUS status;

    if (dwFlags & (~STRSAFE_UNICODE_STRING_VALID_FLAGS)) {
        status = STATUS_INVALID_PARAMETER;
    } else {
        status = RtlUnicodeStringValidateWorker(SourceString,
                                                NTSTRSAFE_UNICODE_STRING_MAX_CCH,
                                                dwFlags);
    }

    return status;
}

_Must_inspect_result_ NTSTATUS RtlValidateUnicodeString(__reserved ULONG Flags,
                                                        __in PCUNICODE_STRING String)
{
    NTSTATUS Status = STATUS_SUCCESS;

    NT_ASSERT(Flags == 0);

    if (Flags != 0) {
        Status = STATUS_INVALID_PARAMETER;
    } else {
        Status = RtlUnicodeStringValidateEx(String, STRSAFE_IGNORE_NULLS);
    }

    return Status;
}

NTSTATUS
BlDuplicateUnicodeString(__in ULONG Flags,
                         __in PCUNICODE_STRING StringIn,
                         __out __drv_at(StringOut->Buffer, __drv_allocatesMem(Mem))
                             PUNICODE_STRING StringOut)
{
    NTSTATUS Status = STATUS_SUCCESS;
    USHORT Length = 0;
    USHORT NewMaximumLength = 0;
    PWSTR Buffer = NULL;

    if (((Flags & ~(RTL_DUPLICATE_UNICODE_STRING_NULL_TERMINATE |
                    RTL_DUPLICATE_UNICODE_STRING_ALLOCATE_NULL_STRING)) != 0) ||
        (StringOut == NULL)) {
        Status = STATUS_INVALID_PARAMETER;
        goto Cleanup;
    }

    // It doesn't make sense to force allocation of a null string unless you
    // want null termination.
    if ((Flags & RTL_DUPLICATE_UNICODE_STRING_ALLOCATE_NULL_STRING) &&
        !(Flags & RTL_DUPLICATE_UNICODE_STRING_NULL_TERMINATE)) {
        Status = STATUS_INVALID_PARAMETER;
        goto Cleanup;
    }

    Status = RtlValidateUnicodeString(0, StringIn);
    if (!NT_SUCCESS(Status)) {
        goto Cleanup;
    }

    if (StringIn != NULL) {
        Length = StringIn->Length;
    }

    if ((Flags & RTL_DUPLICATE_UNICODE_STRING_NULL_TERMINATE) &&
        (Length == UNICODE_STRING_MAX_BYTES)) {
        Status = STATUS_NAME_TOO_LONG;
        goto Cleanup;
    }

    if (Flags & RTL_DUPLICATE_UNICODE_STRING_NULL_TERMINATE) {
        NewMaximumLength = (USHORT)(Length + sizeof(WCHAR));

    } else {
        NewMaximumLength = Length;
    }

    // If it's a zero length string in, force the allocation length to zero
    // unless the caller said that they want zero length strings allocated.
    if (((Flags & RTL_DUPLICATE_UNICODE_STRING_ALLOCATE_NULL_STRING) == 0) && (Length == 0)) {
        NewMaximumLength = 0;
    }

    if (NewMaximumLength != 0) {
        Buffer = (RtlAllocateStringRoutine)(NewMaximumLength);
        if (Buffer == NULL) {
            Status = STATUS_NO_MEMORY;
            goto Cleanup;
        }

        // If there's anything to copy, copy it.  We explicitly test Length because
        // StringIn could be a NULL pointer, so dereferencing it to get the Buffer
        // pointer would access violate.
        if (Length != 0) {
            RtlCopyMemory(Buffer, StringIn->Buffer, Length);
        }

        if (Flags & RTL_DUPLICATE_UNICODE_STRING_NULL_TERMINATE) {
            Buffer[Length / sizeof(WCHAR)] = L'\0';
        }
    }

    StringOut->MaximumLength = NewMaximumLength;
    StringOut->Length = Length;
    StringOut->Buffer = Buffer;
    Status = STATUS_SUCCESS;

Cleanup:
    if (!NT_SUCCESS(Status)) {
        if (Buffer != NULL) {
            (RtlFreeStringRoutine)(Buffer);
        }
    }

    return Status;
}

NTSTATUS
BlAnsiStringToUnicodeString(__inout PUNICODE_STRING DestinationString,
                            __in PCANSI_STRING SourceString,
                            __in BOOLEAN AllocateDestinationString)

/*++

Routine Description:

    This functions converts the specified ansi source string into a
    Unicode string. The translation is done with respect to the
    current system locale information.

Arguments:

    DestinationString - Returns a unicode string that is equivalent to
        the ansi source string. The maximum length field is only
        set if AllocateDestinationString is TRUE.

    SourceString - Supplies the ansi source string that is to be
        converted to unicode.

    AllocateDestinationString - Supplies a flag that controls whether or
        not this API allocates the buffer space for the destination
        string.  If it does, then the buffer must be deallocated using
        RtlFreeUnicodeString (note that only storage for
        DestinationString->Buffer is allocated by this API).

Return Value:

    SUCCESS - The conversion was successful

    !SUCCESS - The operation failed.  No storage was allocated and no
        conversion was done.  None.

--*/

{
    ULONG UnicodeLength;
    ULONG Index;
    NTSTATUS Status;

    UnicodeLength = (SourceString->Length * sizeof(WCHAR)) + sizeof(UNICODE_NULL);

    if (UnicodeLength > MAX_USTRING) {
        Status = STATUS_INVALID_PARAMETER_2;
        goto RtlAnsiStringToUnicodeStringEnd;
    }

    if (AllocateDestinationString != FALSE) {
        DestinationString->MaximumLength = (USHORT)UnicodeLength;
        DestinationString->Buffer = (RtlAllocateStringRoutine)(UnicodeLength);
        if (DestinationString->Buffer == NULL) {
            Status = STATUS_NO_MEMORY;
            goto RtlAnsiStringToUnicodeStringEnd;
        }

    } else {
        if (UnicodeLength > DestinationString->MaximumLength) {
            Status = STATUS_BUFFER_OVERFLOW;
            goto RtlAnsiStringToUnicodeStringEnd;
        }
    }

    DestinationString->Length = (USHORT)(UnicodeLength - sizeof(UNICODE_NULL));
    Index = 0;
    while (Index < SourceString->Length) {
        DestinationString->Buffer[Index] = (WCHAR)SourceString->Buffer[Index];
        Index++;
    }

    if (DestinationString->Length < DestinationString->MaximumLength) {
        DestinationString->Buffer[Index] = UNICODE_NULL;
    }

    Status = STATUS_SUCCESS;

RtlAnsiStringToUnicodeStringEnd:
    return Status;
}

LONG RtlCompareUnicodeString(__in PCUNICODE_STRING String1,
                             __in PCUNICODE_STRING String2,
                             __in BOOLEAN CaseInSensitive)

/*++

Routine Description:

    The RtlCompareUnicodeString function compares two counted strings.  The
    return value indicates if the strings are equal or String1 is less than
    String2 or String1 is greater than String2.

    The CaseInSensitive parameter specifies if case is to be ignored when
    doing the comparison.

Arguments:

    String1 - Pointer to the first string.

    String2 - Pointer to the second string.

    CaseInsensitive - TRUE if case should be ignored when doing the
        comparison.

Return Value:

    Signed value that gives the results of the comparison:

        Zero - String1 equals String2

        < Zero - String1 less than String2

        > Zero - String1 greater than String2


--*/

{
    return RtlCompareUnicodeStrings(String1->Buffer,
                                    String1->Length / sizeof(WCHAR),
                                    String2->Buffer,
                                    String2->Length / sizeof(WCHAR),
                                    CaseInSensitive);
}

LONG RtlCompareUnicodeStrings(__in_ecount(String1Length) PCWCH String1,
                              __in SIZE_T String1Length,
                              __in_ecount(String2Length) PCWCH String2,
                              __in SIZE_T String2Length,
                              __in BOOLEAN CaseInSensitive)

/*++

Routine Description:

    The RtlCompareUnicodeStrings function compares two counted strings.  The
    return value indicates if the strings are equal or String1 is less than
    String2 or String1 is greater than String2.

    The CaseInSensitive parameter specifies if case is to be ignored when
    doing the comparison.

Arguments:

    String1 - Pointer to the first string.

    String1Length - Supplies the length, in characters, of the first string.

    String2 - Pointer to the second string.

    String2Length - Supplies the length, in characters, of the second string.

    CaseInsensitive - TRUE if case should be ignored when doing the
        comparison.

Return Value:

    Signed value that gives the results of the comparison:

        Zero - String1 equals String2

        < Zero - String1 less than String2

        > Zero - String1 greater than String2

--*/

{
    PCWSTR s1, s2, Limit;
    SIZE_T n1, n2;
    WCHAR c1, c2;
    LONG ReturnValue;

    s1 = String1;
    s2 = String2;
    n1 = String1Length;
    n2 = String2Length;

    ASSERT(!(((((ULONG_PTR)s1 & 1) != 0) || (((ULONG_PTR)s2 & 1) != 0))));

    Limit = s1 + (n1 <= n2 ? n1 : n2);
    ReturnValue = (LONG)(n1 - n2);
    if (CaseInSensitive) {
        while (s1 < Limit) {
            c1 = *s1++;
            c2 = *s2++;
            if (c1 != c2) {
                //
                // Note that this needs to reference the translation table!
                //

                c1 = UPCASE(c1);
                c2 = UPCASE(c2);
                if (c1 != c2) {
                    ReturnValue = (LONG)(c1) - (LONG)(c2);
                    break;
                }
            }
        }

    } else {
        while (s1 < Limit) {
            c1 = *s1++;
            c2 = *s2++;
            if (c1 != c2) {
                ReturnValue = (LONG)(c1) - (LONG)(c2);
                break;
            }
        }
    }

    return ReturnValue;
}

VOID RtlFreeUnicodeString(_Inout_ PUNICODE_STRING UnicodeString)

/*++

Routine Description:

    This API is used to free storage allocated by
    RtlAnsiStringToUnicodeString.  Note that only UnicodeString->Buffer
    is free'd by this routine.

Arguments:

    UnicodeString - Supplies the address of the unicode string whose
        buffer was previously allocated by RtlAnsiStringToUnicodeString.

Return Value:

    None.

--*/

{
    if (UnicodeString->Buffer) {
        (RtlFreeStringRoutine)(UnicodeString->Buffer);
        RtlZeroMemory(UnicodeString, sizeof(*UnicodeString));
    }

    return;
}

#else

#ifdef _ARM64_

VOID ExtEnvZeroMemory(_In_ VOID UNALIGNED* Destination, _In_ SIZE_T Length)

/*++

Routine Description:

    This routine is required to implement the functionality provided by
    RtlZeroMemory for the PSCI library.

--*/

{
    RtlZeroMemory(Destination, Length);
    return;
}

#endif // _ARM64_

#endif // NO_BOOTLIBRARY_CALLS
