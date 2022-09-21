//
// Global includes
//
#include "cbmrincludes.h"
#include "safe_arithmetic.h"

#ifndef UEFI_BUILD_SYSTEM
#pragma prefast(push)
#pragma prefast(disable : 6101, \
                "False positive - _Out_ parameter is always initialized upon success")
#endif

//
// Variables
//

//
// Prototypes
//

//
// Interfaces
//

EFI_STATUS EFIAPI UintnAdd(_In_ UINTN Arg1, _In_ UINTN Arg2, _Out_ UINTN* Sum)
{
    if (Sum == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Integer overflow check
    //

    if (UINTN_MAX - Arg1 < Arg2) {
        return EFI_INVALID_PARAMETER;
    }

    *Sum = Arg1 + Arg2;

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI Uint32Add(_In_ UINT32 Arg1, _In_ UINT32 Arg2, _Out_ UINT32* Sum)
{
    if (Sum == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Integer overflow check
    //

    if (UINT32_MAX - Arg1 < Arg2) {
        return EFI_INVALID_PARAMETER;
    }

    *Sum = Arg1 + Arg2;

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI UintnMult(_In_ UINTN Arg1, _In_ UINTN Arg2, _Out_ UINTN* Product)
{
    UINTN Result = 0;

    if (Product == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Integer overflow check
    //

    Result = Arg1 * Arg2;

    if (Arg1 != 0 && Result / Arg1 != Arg2) {
        return EFI_INVALID_PARAMETER;
    }

    *Product = Result;

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI Uint32Mult(_In_ UINT32 Arg1, _In_ UINT32 Arg2, _Out_ UINT32* Product)
{
    UINT32 Result = 0;

    if (Product == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Integer overflow check
    //

    Result = Arg1 * Arg2;

    if (Arg1 != 0 && Result / Arg1 != Arg2) {
        return EFI_INVALID_PARAMETER;
    }

    *Product = Result;

    return EFI_SUCCESS;
}

#ifndef UEFI_BUILD_SYSTEM
#pragma prefast(pop)
#endif
