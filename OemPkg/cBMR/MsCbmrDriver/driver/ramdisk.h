#ifndef _RAMDISK_H_
#define _RAMDISK_H_

typedef struct _RAMDISK_CONTEXT RAMDISK_CONTEXT;

EFI_STATUS EFIAPI RamdiskInit(_In_ UINTN RamdiskSize,
                              _In_ UINT32 SectorSize,
                              _Outptr_ RAMDISK_CONTEXT** RamdiskContext);
EFI_STATUS EFIAPI RamdiskFree(_Inout_ RAMDISK_CONTEXT* RamdiskContext);

EFI_STATUS EFIAPI RamdiskRegister(_Inout_ RAMDISK_CONTEXT* RamdiskContext);
EFI_STATUS EFIAPI RamdiskUnregister(_Inout_ RAMDISK_CONTEXT* RamdiskContext);

EFI_STATUS EFIAPI RamdiskRead(_In_ RAMDISK_CONTEXT* RamdiskContext,
                              _In_ UINTN Offset,
                              _In_ UINTN Length,
                              _Out_writes_bytes_(BufferLength) UINT8* Buffer,
                              _In_ UINTN BufferLength);
EFI_STATUS EFIAPI RamdiskWrite(_Inout_ RAMDISK_CONTEXT* RamdiskContext,
                               _In_ UINTN Offset,
                               _In_ UINTN Length,
                               _In_ UINT8* Data);

EFI_STATUS EFIAPI RamdiskBoot(_In_ RAMDISK_CONTEXT* RamdiskContext);

EFI_STATUS EFIAPI RamdiskGetSectorCount(_In_ const RAMDISK_CONTEXT* RamdiskContext,
                                        _Out_ UINT32* SectorCount);
EFI_STATUS EFIAPI RamdiskGetSectorSize(_In_ const RAMDISK_CONTEXT* RamdiskContext,
                                       _Out_ UINT32* SectorSize);
EFI_STATUS EFIAPI RamdiskInitializeSingleFat32Volume(_Inout_ RAMDISK_CONTEXT* RamdiskContext);

#endif // _RAMDISK_H_
