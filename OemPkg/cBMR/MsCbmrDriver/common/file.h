#ifndef _FILE_H_
#define _FILE_H_

#include "cbmrincludes.h"

#define FILE_OFFSET_EOF           0xFFFFFFFFFFFFFFFF
#define MAX_PARTITION_NAME_LENGTH 36

#define DPP_PARTITION_NAME              L"DPP"
#define OPP_PARTITION_NAME              L"OPP"
#define WINDOWS_SYSTEM_PARTITION_NAME   L"EFIESP"
#define BOOTSTRAP_EFIESP_PARTITION_NAME L"BS_EFIESP"

#ifndef UEFI_BUILD_SYSTEM
#if defined(_X86_)
#define BOOTMGR_NAME       L"bootia32.efi"
#define BOOTMGR_NAME_ASCII "bootia32.efi"
#elif defined(_AMD64_)
#define BOOTMGR_NAME       L"bootx64.efi"
#define BOOTMGR_NAME_ASCII "bootx64.efi"
#elif defined(_ARM_)
#define BOOTMGR_NAME       L"bootarm.efi"
#define BOOTMGR_NAME_ASCII "bootarm.efi"
#elif defined(_ARM64_)
#define BOOTMGR_NAME       L"bootaa64.efi"
#define BOOTMGR_NAME_ASCII "bootaa64.efi"
#else
#error Architecture not supported.
#endif
#endif

#define BOOTMGR_PATH       L"\\EFI\\Boot\\" BOOTMGR_NAME
#define BOOTMGR_PATH_ASCII "\\EFI\\Boot\\" BOOTMGR_NAME_ASCII

EFI_STATUS EFIAPI FileOpen(_In_z_ CHAR16* PartitionName,
                           _In_z_ CHAR16* FileName,
                           _In_ UINT64 OpenMode,
                           _In_ UINT64 Attributes,
                           _Out_ EFI_FILE_PROTOCOL** EfiFileProtocol);
EFI_STATUS EFIAPI FileOpenEx(_In_ EFI_FILE_PROTOCOL* Directory,
                             _In_z_ CHAR16* FileName,
                             _In_ UINT64 OpenMode,
                             _In_ UINT64 Attributes,
                             _Out_ EFI_FILE_PROTOCOL** EfiFileProtocol);
EFI_STATUS EFIAPI FileClose(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol);
EFI_STATUS EFIAPI FileDelete(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol);
EFI_STATUS EFIAPI FileRead(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol,
                           _Inout_ UINTN* ReadSize,
                           _Out_writes_bytes_(*ReadSize) UINT8* OutBuf);
EFI_STATUS EFIAPI FileReadLine(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol,
                               _Inout_ UINTN* ReadSize,
                               _Out_writes_bytes_(*ReadSize) CHAR8* OutBuf,
                               _Out_ BOOLEAN* EndOfFile);
EFI_STATUS EFIAPI FileWrite(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol,
                            _In_ UINTN* BufferSize,
                            _In_reads_bytes_(*BufferSize) VOID* Buffer);
EFI_STATUS EFIAPI FileSetPosition(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol, _In_ UINT64 Position);
EFI_STATUS EFIAPI FileGetPosition(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol, _Out_ UINT64* Position);
EFI_STATUS EFIAPI FileGetInfo(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol,
                              _Outptr_ EFI_FILE_INFO** FileInfo);
EFI_STATUS EFIAPI FileGetFileSystemInfo(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol,
                                        _Outptr_ EFI_FILE_SYSTEM_INFO** FileInfo);
EFI_STATUS EFIAPI FileSetInfo(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol,
                              _In_ EFI_FILE_INFO* FileInfo);
EFI_STATUS EFIAPI FileFlush(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol);

EFI_STATUS EFIAPI FileGetSize(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol, _Out_ UINT64* FileSize);
EFI_STATUS EFIAPI FileSetSize(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol, _In_ UINT64 FileSize);
EFI_STATUS EFIAPI FileGetFreeSpace(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol,
                                   _Out_ UINT64* FreeSpace);
EFI_STATUS EFIAPI FileGetVolumeSize(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol,
                                    _Out_ UINT64* VolumeSize);

EFI_STATUS EFIAPI FileDoesPartitionExist(_In_z_ CHAR16* PartitionName, _Out_ BOOLEAN* IsExist);

EFI_STATUS EFIAPI FileGetSizeEx(_In_z_ CHAR16* PartitionName,
                                _In_z_ CHAR16* FileName,
                                _Out_ UINT64* FileSize);
EFI_STATUS EFIAPI FileGetDirEntriesSize(_In_z_ CHAR16* PartitionName,
                                        _In_z_ CHAR16* DirectoryName,
                                        _Out_ UINT64* DirectorySize);
EFI_STATUS EFIAPI FileGetDirEntriesSizeEx(_In_ EFI_FILE_PROTOCOL* Directory,
                                          _Out_ UINT64* DirectorySize);
EFI_STATUS EFIAPI FileGetDirEntries(_In_z_ CHAR16* PartitionName,
                                    _In_z_ CHAR16* DirectoryName,
                                    _Inout_ UINT32* ReadSize,
                                    _Out_writes_bytes_(*ReadSize) UINT8* OutBuf,
                                    _In_ UINT64 Position);
EFI_STATUS EFIAPI FileGetDirEntriesEx(_In_ EFI_FILE_PROTOCOL* Directory,
                                      _Inout_ UINT32* ReadSize,
                                      _Out_writes_bytes_(*ReadSize) UINT8* OutBuf,
                                      _In_ UINT64 Position);
EFI_STATUS EFIAPI FileReadEx(_In_z_ CHAR16* PartitionName,
                             _In_z_ CHAR16* FileName,
                             _Inout_ UINT32* ReadSize,
                             _Out_writes_bytes_(*ReadSize) UINT8* OutBuf,
                             _In_ UINT64 Position);
EFI_STATUS EFIAPI FileWriteEx(_In_z_ CHAR16* PartitionName,
                              _In_z_ CHAR16* FileName,
                              _Inout_ UINT32* WriteSize,
                              _In_reads_bytes_(*WriteSize) VOID* InBuf,
                              _In_ UINT64 Position,
                              _In_ BOOLEAN Overwrite);
EFI_STATUS EFIAPI FileGetSfsVolumes(_Outptr_opt_result_buffer_to_(sizeof(EFI_FILE_PROTOCOL*),
                                                                  *VolumeCount)
                                        EFI_FILE_PROTOCOL*** SfsVolumes,
                                    _Out_ UINTN* VolumeCount,
                                    _In_opt_ EFI_HANDLE** VolumeHandles,
                                    _In_ BOOLEAN RemovableOnly);
EFI_STATUS EFIAPI FileGetDevicePath(_In_z_ CHAR16* PartitionName,
                                    _In_z_ CHAR16* FileName,
                                    _Out_ EFI_DEVICE_PATH_PROTOCOL** DevicePathProtocol);
EFI_STATUS EFIAPI FileLocateAndOpen(_In_z_ CHAR16* FileName,
                                    _In_ UINT64 OpenMode,
                                    _Out_ EFI_FILE_PROTOCOL** EfiFileProtocol);
EFI_STATUS EFIAPI FileCreateSubdirectories(_In_z_ CHAR16* PartitionName,
                                           _In_reads_z_(DirectoryPathLength) CHAR16* DirectoryPath,
                                           _In_ UINTN DirectoryPathLength,
                                           _Out_ EFI_FILE_PROTOCOL** EfiFileProtocol);
EFI_STATUS EFIAPI FileCreateSubdirectoriesAndFile(_In_z_ CHAR16* PartitionName,
                                                  _In_z_ CHAR16* FileName,
                                                  _Out_ EFI_FILE_PROTOCOL** EfiFileProtocol);
EFI_STATUS EFIAPI FileDuplicate(_In_ EFI_FILE_PROTOCOL* SourceFile,
                                _In_ UINTN SourceOffset,
                                _In_ UINTN ByteCount,
                                _In_z_ CHAR16* PartitionName,
                                _In_z_ CHAR16* DestinationFilePath,
                                _Out_ EFI_FILE_PROTOCOL** DestinationFile);
EFI_STATUS EFIAPI FileFindFirst(_In_ EFI_FILE_PROTOCOL* Directory);
EFI_STATUS EFIAPI FileFindNext(_In_ EFI_FILE_PROTOCOL* Directory,
                               _Outptr_ EFI_FILE_INFO** FileInfo);
EFI_STATUS EFIAPI FileCreateDirectory(_In_ EFI_FILE_PROTOCOL* DestDirectory,
                                      _In_ CHAR16* DirectoryName);
EFI_STATUS EFIAPI FileCopy(_In_ EFI_FILE_PROTOCOL* Source, _In_ EFI_FILE_PROTOCOL* Destination);

#endif // _FILE_H_
