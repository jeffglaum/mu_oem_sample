#include "buffer.h"

#ifndef UEFI_BUILD_SYSTEM
#pragma prefast(push)
#pragma prefast(disable : 6101, \
                "False positive - _Out_ parameter is always initialized upon success")
#endif

typedef struct _BUFFER {
    PVOID Content;
    UINT32 Capacity;
    UINT32 Size;
} BUFFER, *PBUFFER;

EFI_STATUS EFIAPI BufferCreate(_In_ UINT32 Capacity, _Outptr_ BUFFER** Buffer)
{
    BUFFER* RetBuffer = NULL;

    RetBuffer = AllocateZeroPool(sizeof(BUFFER));
    if (!RetBuffer) {
        return EFI_OUT_OF_RESOURCES;
    }

    RetBuffer->Size = 0;
    RetBuffer->Capacity = Capacity;
    RetBuffer->Content = AllocateZeroPool(Capacity);
    if (!RetBuffer->Content) {
        FreePool(RetBuffer);
        return EFI_OUT_OF_RESOURCES;
    }

    *Buffer = RetBuffer;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI BufferAppendContent(_In_ BUFFER* Buffer, _In_ PVOID Content, _In_ UINT32 Size)
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINT32 NewCapacity = 0;

    Status = Uint32Add(Buffer->Size, Size, &NewCapacity);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = BufferEnsureCapacity(Buffer, NewCapacity);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = CopyMemS((CHAR8*)(Buffer->Content) + Buffer->Size, Buffer->Capacity, Content, Size);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Buffer->Size += Size;
    return Status;
}

PVOID EFIAPI BufferGetContent(_In_ BUFFER* Buffer)
{
    return Buffer->Content;
}

UINT32 EFIAPI BufferGetSize(_In_ BUFFER* Buffer)
{
    return Buffer->Size;
}

VOID EFIAPI BufferSetSize(_In_ BUFFER* Buffer, _In_ UINT32 Size)
{
    Buffer->Size = Size;
}

UINT32 EFIAPI BufferGetCapacity(_In_ BUFFER* Buffer)
{
    return Buffer->Capacity;
}

EFI_STATUS EFIAPI BufferEnsureCapacity(_In_ BUFFER* Buffer, _In_ UINT32 NewCapacity)
{
    VOID* NewBuffer = NULL;

    if (Buffer->Capacity >= NewCapacity) {
        return EFI_SUCCESS;
    }

    NewBuffer = ReallocatePool(Buffer->Size, NewCapacity, Buffer->Content);
    if (NewBuffer == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    Buffer->Content = NewBuffer;
    Buffer->Capacity = NewCapacity;

    return EFI_SUCCESS;
}

VOID EFIAPI BufferClear(_In_ BUFFER* Buffer)
{
    Buffer->Size = 0;
}

VOID EFIAPI BufferFree(_In_ BUFFER* Buffer)
{
    if (Buffer != NULL) {
        FreePool(Buffer->Content);
        FreePool(Buffer);
    }
}

#ifndef UEFI_BUILD_SYSTEM
#pragma prefast(pop)
#endif