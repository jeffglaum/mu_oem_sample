#ifndef _BUFFER_H_
#define _BUFFER_H_

#include "cbmrincludes.h"

typedef struct _BUFFER BUFFER;

EFI_STATUS EFIAPI BufferCreate(_In_ UINT32 Capacity, _Outptr_ BUFFER** Buffer);
EFI_STATUS EFIAPI BufferAppendContent(_In_ BUFFER* Buffer, _In_ PVOID Content, _In_ UINT32 Size);
PVOID EFIAPI BufferGetContent(_In_ BUFFER* Buffer);
UINT32 EFIAPI BufferGetSize(_In_ BUFFER* Buffer);
VOID EFIAPI BufferSetSize(_In_ BUFFER* Buffer, _In_ UINT32 Size);
UINT32 EFIAPI BufferGetCapacity(_In_ BUFFER* Buffer);
EFI_STATUS EFIAPI BufferEnsureCapacity(_In_ BUFFER* Buffer, _In_ UINT32 NewCapacity);
VOID EFIAPI BufferClear(_In_ BUFFER* Buffer);
VOID EFIAPI BufferFree(_In_ BUFFER* Buffer);

#endif // _BUFFER_H_
