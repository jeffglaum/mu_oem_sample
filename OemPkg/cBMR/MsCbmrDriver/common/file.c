//
// Global includes
//
#include "cbmrincludes.h"
#include <strsafe.h>

//
// Local includes
//
#include "file.h"
#include "buffer.h"
#include "utils.h"

#ifndef UEFI_BUILD_SYSTEM
#pragma prefast(push)
#pragma prefast(disable : 6101, \
                "False positive - _Out_ parameter is always initialized upon success")
#endif

//
// Constants/Macros
//

#define DEFAULT_READ_BUFFER_SIZE 1024 * 1024
#define MAX_FILE_NAME_LEN \
    522 // (20 * (6+5+2))+1) unicode characters from EFI FAT spec (doubled for bytes)
#define FIND_XXXXX_FILE_BUFFER_SIZE (sizeof(EFI_FILE_INFO) + MAX_FILE_NAME_LEN)

#ifdef UEFI_BUILD_SYSTEM
EFI_GUID gEfiPartitionRecordGuid = {0xfe2555be,
                                    0xd716,
                                    0x4686,
                                    0xb9,
                                    0xd0,
                                    0x79,
                                    0xdb,
                                    0x59,
                                    0x21,
                                    0xb7,
                                    0x0d}; // EFI_PARTITION_RECORD_GUID
#endif

//
// Structs
//

//
// Variables
//

//
// Prototypes
//

static EFI_STATUS EFIAPI FilePartitionOpen(_In_z_ CHAR16* PartitionName,
                                           _Out_ EFI_FILE_PROTOCOL** EfiFileProtocol,
                                           _Out_opt_ EFI_HANDLE* PartitionHandle);
static EFI_STATUS EFIAPI FileIsDirectory(_In_ EFI_FILE_HANDLE DirFs);
static EFI_STATUS FileReadByte(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol,
                               _Out_ CHAR8* Char,
                               _Out_ BOOLEAN* EndOfFile);
static EFI_STATUS FileCreateDevicePathFromString(_In_z_ CHAR16* FileName,
                                                 _Out_ EFI_DEVICE_PATH_PROTOCOL** DevicePath);
static EFI_STATUS FileGetDevicePathInternal(_In_ EFI_HANDLE PartitionHandle,
                                            _In_z_ CHAR16* FileName,
                                            _Out_ EFI_DEVICE_PATH_PROTOCOL** DevicePathProtocol);

static CHAR16* EFIAPI WcstokCompat(_In_z_ CHAR16* String,
                                   _In_z_ const CHAR16* Delim,
                                   _Out_ CHAR16** SavePtr);
static UINTN EFIAPI WcsspnCompat(_In_z_ CHAR16* String, _In_z_ const CHAR16* String2);
static CHAR16* EFIAPI WcspbrkCompat(_In_z_ CHAR16* String, _In_z_ const CHAR16* String2);

//
// Interfaces
//
EFI_STATUS EFIAPI FileOpen(_In_z_ CHAR16* PartitionName,
                           _In_z_ CHAR16* FileName,
                           _In_ UINT64 OpenMode,
                           _In_ UINT64 Attributes,
                           _Out_ EFI_FILE_PROTOCOL** EfiFileProtocol)
{
    EFI_STATUS Status = EFI_NOT_FOUND;
    EFI_FILE_PROTOCOL* RootFs = NULL;

    if (PartitionName == NULL || FileName == NULL || EfiFileProtocol == NULL) {
        DBG_ERROR("Invalid parameter: PartitionName 0x%p, FileName 0x%p, EfiFileProtocol 0x%p",
                  PartitionName,
                  FileName,
                  EfiFileProtocol);
        return EFI_INVALID_PARAMETER;
    }

    DBG_INFO_U(
        L"PartitionName: %s, FileName: %s, OpenMode: 0x%016llx, Attributes: 0x%016llx, EfiFileProtocol: 0x%p",
        PartitionName,
        FileName,
        OpenMode,
        Attributes,
        EfiFileProtocol);

    *EfiFileProtocol = NULL;

    Status = FilePartitionOpen(PartitionName, &RootFs, NULL);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FilePartitionOpen() failed with status 0x%zx", Status);
        return Status;
    }

    return FileOpenEx(RootFs, FileName, OpenMode, Attributes, EfiFileProtocol);
}

EFI_STATUS EFIAPI FileOpenEx(_In_ EFI_FILE_PROTOCOL* Directory,
                             _In_z_ CHAR16* FileName,
                             _In_ UINT64 OpenMode,
                             _In_ UINT64 Attributes,
                             _Out_ EFI_FILE_PROTOCOL** EfiFileProtocol)
{
    EFI_STATUS Status = EFI_NOT_FOUND;

    if (Directory == NULL || FileName == NULL || EfiFileProtocol == NULL) {
        DBG_ERROR("Invalid parameter: Directory 0x%p, FileName 0x%p, EfiFileProtocol 0x%p",
                  Directory,
                  FileName,
                  EfiFileProtocol);
        return EFI_INVALID_PARAMETER;
    }

    Status = Directory->Open(Directory, EfiFileProtocol, FileName, OpenMode, Attributes);

    if (*EfiFileProtocol == NULL) {
        Status = EFI_NOT_FOUND;
    }

    return Status;
}

EFI_STATUS EFIAPI FileDoesPartitionExist(_In_z_ CHAR16* PartitionName, _Out_ BOOLEAN* Exist)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* RootFs = NULL;

    *Exist = FALSE;

    Status = FilePartitionOpen(PartitionName, &RootFs, NULL);
    if (EFI_ERROR(Status)) {
        if (Status == EFI_NOT_FOUND) {
            Status = EFI_SUCCESS;
            *Exist = FALSE;
        }
    } else {
        *Exist = (RootFs != NULL);
    }

    return Status;
}

EFI_STATUS EFIAPI FileClose(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol)
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (EfiFileProtocol != NULL) {
        Status = EfiFileProtocol->Close(EfiFileProtocol);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("Close() failed with status 0x%zx", Status);
        }
    }

    return Status;
}

EFI_STATUS EFIAPI FileDelete(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol)
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (EfiFileProtocol == NULL) {
        DBG_ERROR("Invalid parameter: EfiFileProtocol 0x%p", EfiFileProtocol);
        return EFI_INVALID_PARAMETER;
    }

    Status = EfiFileProtocol->Delete(EfiFileProtocol);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Delete() failed with status 0x%zx", Status);
    }

    return Status;
}

EFI_STATUS EFIAPI FileRead(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol,
                           _Inout_ UINTN* ReadSize,
                           _Out_writes_bytes_(*ReadSize) UINT8* OutBuf)
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (EfiFileProtocol == NULL || ReadSize == NULL || OutBuf == NULL) {
        DBG_ERROR("Invalid parameter: EfiFileProtocol 0x%p, ReadSize: 0x%p, OutBuf: 0x%p",
                  EfiFileProtocol,
                  ReadSize,
                  OutBuf);
        return EFI_INVALID_PARAMETER;
    }

    DBG_VERBOSE("Read size: %zu", *ReadSize);

    Status = EfiFileProtocol->Read(EfiFileProtocol, ReadSize, OutBuf);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Read() failed with status 0x%zx", Status);
    }

    return Status;
}

EFI_STATUS EFIAPI FileReadLine(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol,
                               _Inout_ UINTN* ReadSize,
                               _Out_writes_bytes_(*ReadSize) CHAR8* Buffer,
                               _Out_ BOOLEAN* EndOfFile)
{
    EFI_STATUS Status = EFI_SUCCESS;
    CHAR8 Char = 0;
    UINTN i = 0;

    if (EfiFileProtocol == NULL || ReadSize == NULL || Buffer == NULL) {
        DBG_ERROR("Invalid parameter: EfiFileProtocol 0x%p, ReadSize: 0x%p, Buffer: 0x%p",
                  EfiFileProtocol,
                  ReadSize,
                  Buffer);
        return EFI_INVALID_PARAMETER;
    }

    *EndOfFile = FALSE;
    DBG_VERBOSE("Read size: %zu", *ReadSize);

    while (!EFI_ERROR((Status = FileReadByte(EfiFileProtocol, &Char, EndOfFile)))) {
        if (*EndOfFile == TRUE)
            break;

        if (Char == '\r') // Possibly left over from previous read
            continue;
        if (Char == '\n')
            break;

        if (i >= *ReadSize) {
            Buffer[i - 1] = 0;
            Status = EFI_BUFFER_TOO_SMALL;
            goto Exit;
        }
        Buffer[i++] = Char;
    }

    if (i >= *ReadSize) {
        Status = EFI_BUFFER_TOO_SMALL;
        goto Exit;
    }

    Buffer[i] = 0;
    *ReadSize = i;
Exit:
    return Status;
}

EFI_STATUS EFIAPI FileWrite(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol,
                            _In_ UINTN* BufferSize,
                            _In_reads_bytes_(*BufferSize) VOID* Buffer)
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (EfiFileProtocol == NULL || BufferSize == NULL || Buffer == NULL) {
        DBG_ERROR("Invalid parameter: EfiFileProtocol 0x%p, BufferSize: 0x%p, Buffer: 0x%p",
                  EfiFileProtocol,
                  BufferSize,
                  Buffer);
        return EFI_INVALID_PARAMETER;
    }

    DBG_VERBOSE("Buffer size: %zu", *BufferSize);

    Status = EfiFileProtocol->Write(EfiFileProtocol, BufferSize, Buffer);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Write() failed with status 0x%zx", Status);
    }

    DBG_VERBOSE("Bytes written: %zu", *BufferSize);

    return Status;
}

EFI_STATUS EFIAPI FileSetPosition(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol, _In_ UINT64 Position)
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (EfiFileProtocol == NULL) {
        DBG_ERROR("Invalid parameter: EfiFileProtocol 0x%p", EfiFileProtocol);
        return EFI_INVALID_PARAMETER;
    }

    DBG_VERBOSE("Position: %llu", Position);

    Status = EfiFileProtocol->SetPosition(EfiFileProtocol, Position);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("SetPosition() failed with status 0x%zx", Status);
    }

    return Status;
}

EFI_STATUS EFIAPI FileGetPosition(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol, _Out_ UINT64* Position)
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (EfiFileProtocol == NULL || Position == NULL) {
        DBG_ERROR("Invalid parameter: EfiFileProtocol 0x%p, Position 0x%p",
                  EfiFileProtocol,
                  Position);
        return EFI_INVALID_PARAMETER;
    }

    Status = EfiFileProtocol->GetPosition(EfiFileProtocol, Position);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("GetPosition() failed with status 0x%zx", Status);
    }

    DBG_VERBOSE("Position: %llu", *Position);

    return Status;
}

EFI_STATUS EFIAPI FileGetInfo(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol,
                              _Outptr_ EFI_FILE_INFO** FileInfo)
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINTN BufferSize = 0;
    VOID* Buffer = NULL;

    Status = EfiFileProtocol->GetInfo(EfiFileProtocol, &gEfiFileInfoGuid, &BufferSize, Buffer);
    if (Status != EFI_BUFFER_TOO_SMALL && EFI_ERROR(Status)) {
        DBG_ERROR("GetInfo() failed 0x%zx", Status);
        goto Exit;
    }

    Buffer = AllocateZeroPool(BufferSize);
    if (Buffer == NULL) {
        DBG_ERROR("Unable to allocate memory");
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    Status = EfiFileProtocol->GetInfo(EfiFileProtocol, &gEfiFileInfoGuid, &BufferSize, Buffer);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("GetInfo() failed 0x%zx", Status);
        goto Exit;
    }

    *FileInfo = (EFI_FILE_INFO*)Buffer;
    return Status;
Exit:
    FreePool(Buffer);
    return Status;
}

EFI_STATUS EFIAPI FileGetFileSystemInfo(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol,
                                        _Outptr_ EFI_FILE_SYSTEM_INFO** FileInfo)
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINTN BufferSize = 0;
    VOID* Buffer = NULL;

    Status = EfiFileProtocol->GetInfo(EfiFileProtocol,
                                      &gEfiFileSystemInfoGuid,
                                      &BufferSize,
                                      Buffer);
    if (Status != EFI_BUFFER_TOO_SMALL && EFI_ERROR(Status)) {
        DBG_ERROR("GetInfo() failed 0x%zx", Status);
        goto Exit;
    }

    Buffer = AllocateZeroPool(BufferSize);
    if (Buffer == NULL) {
        DBG_ERROR("Unable to allocate memory");
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    Status = EfiFileProtocol->GetInfo(EfiFileProtocol,
                                      &gEfiFileSystemInfoGuid,
                                      &BufferSize,
                                      Buffer);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("GetInfo() failed 0x%zx", Status);
        goto Exit;
    }

    *FileInfo = (EFI_FILE_SYSTEM_INFO*)Buffer;
    return Status;
Exit:
    FreePool(Buffer);
    return Status;
}

EFI_STATUS EFIAPI FileSetInfo(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol, _In_ EFI_FILE_INFO* FileInfo)
{
    EFI_STATUS Status = EFI_SUCCESS;

    Status = EfiFileProtocol->SetInfo(EfiFileProtocol,
                                      &gEfiFileInfoGuid,
                                      (UINTN)FileInfo->Size,
                                      FileInfo);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("SetInfo() failed with status 0x%zx", Status);
    }

    return Status;
}

EFI_STATUS EFIAPI FileFlush(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol)
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (EfiFileProtocol == NULL) {
        DBG_ERROR("Invalid parameter: EfiFileProtocol 0x%p", EfiFileProtocol);
        return EFI_INVALID_PARAMETER;
    }

    Status = EfiFileProtocol->Flush(EfiFileProtocol);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Flush() failed with status 0x%zx", Status);
    }

    return Status;
}

EFI_STATUS EFIAPI FileGetSize(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol, _Out_ UINT64* FileSize)
{
    EFI_FILE_INFO* EfiFileInfo = NULL;
    EFI_STATUS Status = EFI_SUCCESS;

    if (EfiFileProtocol == NULL || FileSize == NULL) {
        DBG_ERROR("Invalid parameter: EfiFileProtocol 0x%p, FileSize 0x%p",
                  EfiFileProtocol,
                  FileSize);
        return EFI_INVALID_PARAMETER;
    }

    Status = FileGetInfo(EfiFileProtocol, (VOID*)&EfiFileInfo);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileGetInfo() failed with status 0x%zx", Status);
    } else {
        *FileSize = EfiFileInfo->FileSize;
    }

    FreePool(EfiFileInfo);

    return Status;
}

EFI_STATUS EFIAPI FileSetSize(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol, _In_ UINT64 FileSize)
{
    EFI_FILE_INFO* EfiFileInfo = NULL;
    EFI_STATUS Status = EFI_SUCCESS;

    if (EfiFileProtocol == NULL) {
        DBG_ERROR("Invalid parameter: EfiFileProtocol 0x%p", EfiFileProtocol);
        return EFI_INVALID_PARAMETER;
    }

    DBG_VERBOSE("File Size: %llu bytes", FileSize);

    Status = FileGetInfo(EfiFileProtocol, (VOID*)&EfiFileInfo);
    if (Status == EFI_SUCCESS) {
        if (EfiFileInfo->FileSize == FileSize) {
            // No need to do anything...
            Status = EFI_SUCCESS;
        } else if (EfiFileInfo->FileSize > FileSize) {
            // Set the new (smaller) size

            DBG_VERBOSE("Set new file size: %llu -> %llu", EfiFileInfo->FileSize, FileSize);

            EfiFileInfo->FileSize = FileSize;

            Status = FileSetInfo(EfiFileProtocol, (VOID*)EfiFileInfo);
        } else {
            // New size can't be bigger that old size
            DBG_ERROR("File size can't be increased: %llu -> %llu",
                      EfiFileInfo->FileSize,
                      FileSize);

            Status = EFI_INVALID_PARAMETER;
        }
    }

    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileSetSize() failed with status 0x%zx", Status);
    }

    FreePool(EfiFileInfo);

    return Status;
}

EFI_STATUS EFIAPI FileGetFreeSpace(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol, _Out_ UINT64* FreeSpace)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_SYSTEM_INFO* EfiFileSystemInfo = NULL;

    if (EfiFileProtocol == NULL || FreeSpace == NULL) {
        DBG_ERROR("Invalid parameter: EfiFileProtocol 0x%p, FreeSpace 0x%p",
                  EfiFileProtocol,
                  FreeSpace);
        return EFI_INVALID_PARAMETER;
    }

    *FreeSpace = 0;

    Status = FileGetFileSystemInfo(EfiFileProtocol, &EfiFileSystemInfo);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileGetFileSystemInfo() failed with status 0x%zx", Status);
        return Status;
    }

    *FreeSpace = EfiFileSystemInfo->FreeSpace;
    FreePool((VOID*)EfiFileSystemInfo);

    DBG_VERBOSE("FreeSpace: %llu bytes", *FreeSpace);

    return Status;
}

EFI_STATUS EFIAPI FileGetVolumeSize(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol,
                                    _Out_ UINT64* VolumeSize)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_SYSTEM_INFO* EfiFileSystemInfo = NULL;

    if (EfiFileProtocol == NULL || VolumeSize == NULL) {
        DBG_ERROR("Invalid parameter: EfiFileProtocol 0x%p, VolumeSize 0x%p",
                  EfiFileProtocol,
                  VolumeSize);
        return EFI_INVALID_PARAMETER;
    }

    *VolumeSize = 0;

    Status = FileGetFileSystemInfo(EfiFileProtocol, &EfiFileSystemInfo);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileGetFileSystemInfo() failed with status 0x%zx", Status);
        return Status;
    }

    *VolumeSize = EfiFileSystemInfo->VolumeSize;
    FreePool((VOID*)EfiFileSystemInfo);

    DBG_VERBOSE("VolumeSize: %llu bytes", *VolumeSize);

    return Status;
}

EFI_STATUS EFIAPI FileGetDevicePath(_In_z_ CHAR16* PartitionName,
                                    _In_z_ CHAR16* FileName,
                                    _Out_ EFI_DEVICE_PATH_PROTOCOL** DevicePathProtocol)
{
    EFI_STATUS Status = EFI_NOT_FOUND;
    EFI_HANDLE EfiHandle = NULL;
    EFI_FILE_PROTOCOL* RootFs = NULL;
    EFI_FILE_PROTOCOL* EfiFileProtocol = NULL;

    if (PartitionName == NULL || FileName == NULL) {
        DBG_ERROR("Invalid parameter: PartitionName 0x%p, FileName 0x%p, DevicePathProtocol 0x%p",
                  PartitionName,
                  FileName,
                  DevicePathProtocol);
        return EFI_INVALID_PARAMETER;
    }

    DBG_INFO_U(L"PartitionName: %s, FileName: %s, DevicePathProtocol: 0x%p",
               PartitionName,
               FileName,
               DevicePathProtocol);

    // Verify partition existence
    Status = FilePartitionOpen(PartitionName, &RootFs, &EfiHandle);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FilePartitionOpen() failed with status 0x%zx", Status);
        return Status;
    }

    // Verify file existence
    Status = FileOpen(PartitionName, FileName, EFI_FILE_MODE_READ, 0, &EfiFileProtocol);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileOpen() failed with status 0x%zx", Status);
        return Status;
    }

    return FileGetDevicePathInternal(EfiHandle, FileName, DevicePathProtocol);
}

EFI_STATUS EFIAPI FileGetDirEntriesSize(_In_z_ CHAR16* PartitionName,
                                        _In_z_ CHAR16* DirectoryName,
                                        _Out_ UINT64* DirectorySize)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* EfiFileProtocol = NULL;

    if (PartitionName == NULL || DirectoryName == NULL || DirectorySize == NULL) {
        DBG_ERROR("Invalid parameter: PartitionName 0x%p, DirectoryName: 0x%p, DirectorySize: 0x%p",
                  PartitionName,
                  DirectoryName,
                  DirectorySize);
        return EFI_INVALID_PARAMETER;
    }

    Status = FileOpen(PartitionName, DirectoryName, EFI_FILE_MODE_READ, 0, &EfiFileProtocol);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileOpen() failed with status 0x%zx", Status);
        goto Exit;
    }

    Status = FileGetDirEntriesSizeEx(EfiFileProtocol, DirectorySize);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileGetDirEntriesSizeEx() failed with status 0x%zx", Status);
        goto Exit;
    }

Exit:
    if (EfiFileProtocol) {
        (VOID) FileClose(EfiFileProtocol);
        EfiFileProtocol = NULL;
    }

    return Status;
}

EFI_STATUS EFIAPI FileGetDirEntriesSizeEx(_In_ EFI_FILE_PROTOCOL* Directory,
                                          _Out_ UINT64* DirectorySize)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_INFO* FileInfo = NULL;
    UINTN BufferSize = 0;
    UINT64 Size = 0;

    if (Directory == NULL || DirectorySize == NULL) {
        DBG_ERROR("Invalid parameter: Directory 0x%p, DirectorySize: 0x%p",
                  Directory,
                  DirectorySize);
        return EFI_INVALID_PARAMETER;
    }

    // Make sure Directory is a directory
    Status = FileIsDirectory(Directory);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Directory is not a directory 0x%zx", Status);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    // Reset to the beginning of the directory
    Status = FileSetPosition(Directory, 0);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileSetPosition() failed with status 0x%zx", Status);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    // Allocate a buffer sized to struct size + enough for the string at the end
    BufferSize = FIND_XXXXX_FILE_BUFFER_SIZE;
    FileInfo = AllocateZeroPool(BufferSize);
    if (FileInfo == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    for (;;) {
        BufferSize = FIND_XXXXX_FILE_BUFFER_SIZE;

        // Read in the info about the file
        Status = Directory->Read(Directory, &BufferSize, FileInfo);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileRead() failed with status 0x%zx", Status);
            break;
        }

        // No more entries
        if (BufferSize == 0) {
            break;
        }

        Size += BufferSize;
    }

    if (EFI_ERROR(Status)) {
        goto Exit;
    }

Exit:
    FreePool(FileInfo);

    *DirectorySize = Size;

    return Status;
}

EFI_STATUS EFIAPI FileGetDirEntries(_In_z_ CHAR16* PartitionName,
                                    _In_z_ CHAR16* DirectoryName,
                                    _Inout_ UINT32* ReadSize,
                                    _Out_writes_bytes_(*ReadSize) UINT8* OutBuf,
                                    _In_ UINT64 Position)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* EfiFileProtocol = NULL;

    if (PartitionName == NULL || DirectoryName == NULL || ReadSize == NULL || OutBuf == NULL) {
        DBG_ERROR(
            "Invalid parameter: PartitionName 0x%p, DirectoryName: 0x%p, ReadSize: 0x%p, OutBuf: 0x%p",
            PartitionName,
            DirectoryName,
            ReadSize,
            OutBuf);
        return EFI_INVALID_PARAMETER;
    }

    Status = FileOpen(PartitionName, DirectoryName, EFI_FILE_MODE_READ, 0, &EfiFileProtocol);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileOpen() failed with status 0x%zx", Status);
        goto Exit;
    }

    Status = FileGetDirEntriesEx(EfiFileProtocol, ReadSize, OutBuf, Position);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileGetDirEntriesSizeEx() failed with status 0x%zx", Status);
        goto Exit;
    }

Exit:
    if (EfiFileProtocol) {
        (VOID) FileClose(EfiFileProtocol);
        EfiFileProtocol = NULL;
    }

    return Status;
}

EFI_STATUS EFIAPI FileGetDirEntriesEx(_In_ EFI_FILE_PROTOCOL* Directory,
                                      _Inout_ UINT32* ReadSize,
                                      _Out_writes_bytes_(*ReadSize) UINT8* OutBuf,
                                      _In_ UINT64 Position)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_INFO* FileInfo = NULL;
    UINTN BufferSize = 0;
    UINT64 Offset = 0;
    UINTN Read;

    if (Directory == NULL || ReadSize == NULL || OutBuf == NULL) {
        DBG_ERROR("Invalid parameter: Directory: 0x%p, ReadSize: 0x%p, OutBuf: 0x%p",
                  Directory,
                  ReadSize,
                  OutBuf);
        return EFI_INVALID_PARAMETER;
    }

    // Make sure Directory is a directory
    Status = FileIsDirectory(Directory);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Directory is not a directory 0x%zx", Status);
        goto Exit;
    }

    // Reset to the beginning of the directory
    Status = FileSetPosition(Directory, 0);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileSetPosition() failed with status 0x%zx", Status);
        goto Exit;
    }

    // Allocate a buffer sized to struct size + enough for the string at the end
    BufferSize = FIND_XXXXX_FILE_BUFFER_SIZE;
    FileInfo = AllocateZeroPool(BufferSize);
    if (FileInfo == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    while (Position > Offset) {
        BufferSize = FIND_XXXXX_FILE_BUFFER_SIZE;

        // Read in the info about the file
        Status = Directory->Read(Directory, &BufferSize, FileInfo);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileRead() failed with status 0x%zx", Status);
            break;
        }

        // No more entries
        if (BufferSize == 0) {
            break;
        }

        Offset += BufferSize;
    }

    if (EFI_ERROR(Status)) {
        goto Exit;
    }

    if (Position > Offset) {
        DBG_ERROR("No more directory entries");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (Position != Offset) {
        DBG_ERROR("Position is not at directory entry boundary");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    UINTN Remaining = *ReadSize;
    Offset = 0;

    for (;;) {
        Read = Remaining;

        // Read in the info about the file
        Status = Directory->Read(Directory, &Read, OutBuf + Offset);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileRead() failed with status 0x%zx", Status);
            break;
        }

        // No more entries
        if (Read == 0) {
            break;
        }

        Offset += Read;
        Remaining -= Read;
    }

    if (Status == EFI_SUCCESS) {
        *ReadSize = (UINT32)Offset;
    }

Exit:
    FreePool(FileInfo);

    return Status;
}

EFI_STATUS EFIAPI FileGetSizeEx(_In_z_ CHAR16* PartitionName,
                                _In_z_ CHAR16* FileName,
                                _Out_ UINT64* FileSize)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* EfiFileProtocol = NULL;

    if (PartitionName == NULL || FileName == NULL || FileSize == NULL) {
        DBG_ERROR("Invalid parameter: PartitionName 0x%p, FileName: 0x%p, FileSize: 0x%p",
                  PartitionName,
                  FileName,
                  FileSize);
        return EFI_INVALID_PARAMETER;
    }

    Status = FileOpen(PartitionName, FileName, EFI_FILE_MODE_READ, 0, &EfiFileProtocol);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileOpen() for servicing log failed with status 0x%zx", Status);
        goto Exit;
    }

    Status = FileGetSize(EfiFileProtocol, FileSize);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileGetSize() failed with status 0x%zx", Status);
        goto Exit;
    }

    DBG_VERBOSE("File size: %llu", *FileSize);

Exit:
    if (EfiFileProtocol) {
        Status = FileClose(EfiFileProtocol);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileClose() failed with status 0x%zx", Status);
        }
    }

    return Status;
}

EFI_STATUS EFIAPI FileReadEx(_In_z_ CHAR16* PartitionName,
                             _In_z_ CHAR16* FileName,
                             _Inout_ UINT32* ReadSize,
                             _Out_writes_bytes_(*ReadSize) UINT8* OutBuf,
                             _In_ UINT64 Position)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* EfiFileProtocol = NULL;
    UINTN Size = 0;

    if (PartitionName == NULL || FileName == NULL || ReadSize == NULL || OutBuf == NULL) {
        DBG_ERROR(
            "Invalid parameter: PartitionName 0x%p, FileName: 0x%p, ReadSize: 0x%p, OutBuf: 0x%p",
            PartitionName,
            FileName,
            ReadSize,
            OutBuf);
        return EFI_INVALID_PARAMETER;
    } else if (*ReadSize == 0 && Position != 0) {
        DBG_ERROR("Invalid parameter: *ReadSize: 0x%x, Position: 0x%llu", *ReadSize, Position);
        return EFI_INVALID_PARAMETER;
    }

    Status = FileOpen(PartitionName, FileName, EFI_FILE_MODE_READ, 0, &EfiFileProtocol);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileOpen() failed with status 0x%zx", Status);
        goto Exit;
    }

    if (Position != 0) {
        Status = FileSetPosition(EfiFileProtocol, Position);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileSetPosition() failed with status 0x%zx", Status);
            goto Exit;
        }
    }

    Size = *ReadSize;

    Status = EfiFileProtocol->Read(EfiFileProtocol, &Size, OutBuf);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileRead() failed with status 0x%zx", Status);
        goto Exit;
    }

    *ReadSize = (UINT32)Size;
    DBG_VERBOSE("Read %u bytes at offset %llu", *ReadSize, Position);

Exit:
    if (EfiFileProtocol) {
        Status = FileClose(EfiFileProtocol);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileClose() failed with status 0x%zx", Status);
        }
    }

    return Status;
}

EFI_STATUS EFIAPI FileWriteEx(_In_z_ CHAR16* PartitionName,
                              _In_z_ CHAR16* FileName,
                              _Inout_ UINT32* WriteSize,
                              _In_reads_bytes_(*WriteSize) VOID* InBuf,
                              _In_ UINT64 Position,
                              _In_ BOOLEAN Overwrite)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* EfiFileProtocol = NULL;
    UINTN Size = 0;

    if (PartitionName == NULL || FileName == NULL || WriteSize == NULL || InBuf == NULL) {
        DBG_ERROR(
            "Invalid parameter: PartitionName 0x%p, FileName: 0x%p, WriteSize: 0x%p, InBuf: 0x%p",
            PartitionName,
            FileName,
            WriteSize,
            InBuf);
        return EFI_INVALID_PARAMETER;
    } else if (*WriteSize == 0) {
        DBG_ERROR("Invalid parameter: *WriteSize: 0x%x", *WriteSize);
        return EFI_INVALID_PARAMETER;
    }

    Status = FileOpen(PartitionName,
                      FileName,
                      EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
                      0,
                      &EfiFileProtocol);
    if (!EFI_ERROR(Status)) {
        if (!Overwrite) {
            DBG_ERROR("File already exists and Overwrite is not specified 0x%zx", Status);
            Status = EFI_INVALID_PARAMETER;
            goto Exit;
        }
    } else {
        // Create directory and file
        Status = FileCreateSubdirectoriesAndFile(PartitionName, FileName, &EfiFileProtocol);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileCreate() failed with status 0x%zx", Status);
            goto Exit;
        }
    }

    if (Position != 0) {
        Status = FileSetPosition(EfiFileProtocol, Position);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileSetPosition() failed with status 0x%zx", Status);
            goto Exit;
        }
    }

    Size = *WriteSize;

    Status = EfiFileProtocol->Write(EfiFileProtocol, &Size, InBuf);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileWrite() failed with status 0x%zx", Status);
        goto Exit;
    }

    *WriteSize = (UINT32)Size;
    DBG_VERBOSE("Write %u bytes at offset %llu", *WriteSize, Position);

Exit:
    if (EfiFileProtocol) {
        Status = FileClose(EfiFileProtocol);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileClose() failed with status 0x%zx", Status);
        }
    }

    return Status;
}

EFI_STATUS EFIAPI FileGetSfsVolumes(_Outptr_opt_result_buffer_to_(sizeof(EFI_FILE_PROTOCOL*),
                                                                  *VolumeCount)
                                        EFI_FILE_PROTOCOL*** SfsVolumes,
                                    _Out_ UINTN* VolumeCount,
                                    _In_opt_ EFI_HANDLE** VolumeHandles,
                                    _In_ BOOLEAN RemovableOnly)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* RootFs;
    EFI_DEVICE_PATH_PROTOCOL* DevicePathIf;
    EFI_DEVICE_PATH_TO_TEXT_PROTOCOL* DevicePathToTextIf;
    CHAR16* DevicePath;
    EFI_BLOCK_IO_MEDIA* BlockIoMedia;
    EFI_BLOCK_IO_PROTOCOL* BlockIoIf;
    BOOLEAN RunningInVm;
    BUFFER* SimpleFileSystemBuffer;
    EFI_HANDLE* Handles;
    EFI_HANDLE* RetVolumeHandles;
    UINTN VolumeHandleIndex;
    UINTN HandleCount;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* SimpleFs;
    EFI_FILE_PROTOCOL** Volumes;
    UINTN NumVolumes;
    UINTN BufferSize;

    RootFs = NULL;
    RunningInVm = FALSE;
    DevicePathToTextIf = NULL;
    SimpleFileSystemBuffer = NULL;
    Handles = NULL;
    HandleCount = 0;
    Volumes = NULL;
    RetVolumeHandles = NULL;
    VolumeHandleIndex = 0;
    BufferSize = 0;

    if (SfsVolumes == NULL || VolumeCount == NULL) {
        DBG_ERROR("Invalid parameter: SfsVolumes 0x%p, VolumeCount 0x%p", SfsVolumes, VolumeCount);
        return EFI_INVALID_PARAMETER;
    }

    Status = BufferCreate(sizeof(EFI_FILE_PROTOCOL*), &SimpleFileSystemBuffer);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("BufferCreate failed 0x%zx", Status);
        return EFI_OUT_OF_RESOURCES;
    }

    Status = gBS->LocateProtocol(&gEfiDevicePathToTextProtocolGuid,
                                 NULL,
                                 (PVOID*)&DevicePathToTextIf);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Unable to locate DevicePathToTextIf protocol 0x%zx", Status);
        goto Exit;
    }

    Status = gBS->LocateHandleBuffer(ByProtocol,
                                     &gEfiSimpleFileSystemProtocolGuid,
                                     NULL,
                                     &HandleCount,
                                     &Handles);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Unable to get handles to simple filesystems: 0x%zx", Status);
        goto Exit;
    } else {
        DBG_VERBOSE("Handle count: %zd", HandleCount);

        //
        // Allow detecting file systems from VM VHDs for testing purposes.
        // Debug app only.
        //

#ifdef DEBUGMODE
        RunningInVm = IsRunningInVM();
#endif

        RetVolumeHandles = AllocateZeroPool(sizeof(EFI_HANDLE) * HandleCount);

        Status = EFI_NOT_FOUND;
        BlockIoIf = NULL;
        for (UINTN Index = 0; Index < HandleCount; Index++) {
            Status = gBS->HandleProtocol(Handles[Index],
                                         &gEfiBlockIoProtocolGuid,
                                         (PVOID*)&BlockIoIf);
            if (EFI_ERROR(Status)) {
                Status = EFI_SUCCESS;
                continue;
            }

            //
            // If RemovableOnly == TRUE, the block io device must be removable. Otherwise,
            // grab any simple file system volume.
            //

            BlockIoMedia = BlockIoIf->Media;
            if (RemovableOnly == FALSE || BlockIoMedia->RemovableMedia == TRUE ||
                RunningInVm == TRUE) {
                Status = EFI_SUCCESS;

                DevicePathIf = DevicePathFromHandle(Handles[Index]);
                if (DevicePathIf != NULL) {
                    DevicePath = DevicePathToTextIf->ConvertDevicePathToText(DevicePathIf,
                                                                             FALSE,
                                                                             FALSE);
                    DBG_VERBOSE_U("Removable media: Device path %s", DevicePath);

                    FreePool(DevicePath);
                } else {
                    //
                    // Block io device was not properly initialized, so try other
                    // handles.
                    //

                    Status = EFI_NOT_FOUND;
                    continue;
                }

                //
                // Try to open simple file system protocol.
                //

                SimpleFs = NULL;
                Status = gBS->HandleProtocol(Handles[Index],
                                             &gEfiSimpleFileSystemProtocolGuid,
                                             (VOID**)&SimpleFs);
                if (EFI_ERROR(Status)) {
                    DBG_ERROR("Unable to get handle for simple filesystem protocol (0x%zx): 0x%zx",
                              Index,
                              Status);
                    continue;
                }

                Status = SimpleFs->OpenVolume(SimpleFs, &RootFs);
                if (EFI_ERROR(Status)) {
                    DBG_ERROR("OpenVolume() failed with status 0x%zx", Status);
                    continue;
                }

                Status = BufferAppendContent(SimpleFileSystemBuffer, &RootFs, sizeof(RootFs));
                if (EFI_ERROR(Status)) {
                    DBG_ERROR("BufferAppendContent failed 0x%zx", Status);
                    FileClose(RootFs);
                    goto Exit;
                }

                RetVolumeHandles[VolumeHandleIndex++] = Handles[Index];
                RootFs = NULL;
            }
        }
    }

    if (EFI_ERROR(Status)) {
        //
        // No volumes found.
        //

        goto Exit;
    }

    BufferSize = BufferGetSize(SimpleFileSystemBuffer);

    Volumes = AllocateZeroPool(BufferSize);
    if (Volumes == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    } else {
        Status = CopyMemS(Volumes,
                          BufferSize,
                          BufferGetContent(SimpleFileSystemBuffer),
                          BufferSize);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("CopyMemS() failed : 0x%zx", Status);
            goto Exit;
        }

        *SfsVolumes = Volumes;
        *VolumeCount = BufferSize / sizeof(EFI_FILE_PROTOCOL*);

        if (VolumeHandles != NULL) {
            *VolumeHandles = RetVolumeHandles;
        }
    }

Exit:

    if (EFI_ERROR(Status)) {
        if (Status == EFI_NOT_FOUND) {
            DBG_ERROR("No volumes found");
        }

        if (RootFs != NULL) {
            FileClose(RootFs);
        }

        if (SimpleFileSystemBuffer) {
            Volumes = BufferGetContent(SimpleFileSystemBuffer);
            NumVolumes = BufferSize / sizeof(EFI_FILE_PROTOCOL*);

            for (UINT32 Index = 0; Index < NumVolumes; Index++) {
                (VOID) FileClose(Volumes[Index]);
            }
        }

        FreePool(RetVolumeHandles);
    }

    FreePool(Handles);
    BufferFree(SimpleFileSystemBuffer);

    return Status;
}

EFI_STATUS EFIAPI FileLocateAndOpen(_In_z_ CHAR16* FileName,
                                    _In_ UINT64 OpenMode,
                                    _Out_ EFI_FILE_PROTOCOL** EfiFileProtocol)
{
    CHAR16* FilePath = NULL;
    CHAR16* VolumeLabel = NULL;
    EFI_DEVICE_PATH_PROTOCOL* DevicePathProtocol = NULL;
    EFI_DEVICE_PATH_TO_TEXT_PROTOCOL* DevicePathToTextIf = NULL;
    EFI_FILE_PROTOCOL* File = NULL;
    EFI_FILE_PROTOCOL** Volumes;
    EFI_HANDLE* VolumeHandles = NULL;
    EFI_STATUS Status = EFI_NOT_FOUND;
    UINTN Index = 0;
    UINTN VolumeCount;
    UINTN VolumeLabelSize = 0;
    CHAR16 BufferWide[1024];
    CHAR8 Buffer[1024];

    Status = FileGetSfsVolumes(&Volumes, &VolumeCount, &VolumeHandles, FALSE);
    if (EFI_ERROR(Status)) {
        goto Exit;
    }

    for (Index = 0; Index < VolumeCount; Index++) {
        Status = FileOpenEx(Volumes[Index], FileName, OpenMode, 0, &File);
        if (!EFI_ERROR(Status)) {
            *EfiFileProtocol = File;
            break;
        }
    }

    if (Index >= VolumeCount) {
        Status = EFI_NOT_FOUND;
        goto Exit;
    }

    //
    // Find the volume label
    //

    Status = Volumes[Index]->GetInfo(Volumes[Index],
                                     &gEfiFileSystemVolumeLabelInfoIdGuid,
                                     &VolumeLabelSize,
                                     VolumeLabel);
    if (Status == EFI_BUFFER_TOO_SMALL) { // Ignore other failures
        VolumeLabel = AllocateZeroPool(VolumeLabelSize);
        Status = Volumes[Index]->GetInfo(Volumes[Index],
                                         &gEfiFileSystemVolumeLabelInfoIdGuid,
                                         &VolumeLabelSize,
                                         VolumeLabel);
    }

    //
    // Find ACPI device path
    //

    Status = FileGetDevicePathInternal(VolumeHandles[Index], FileName, &DevicePathProtocol);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileGetDevicePathInternal() Failed 0x%zx", Status);
        goto Exit;
    }

    Status = gBS->LocateProtocol(&gEfiDevicePathToTextProtocolGuid,
                                 NULL,
                                 (PVOID*)&DevicePathToTextIf);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("LocateProtocol() for DevicePathToText protocol failed with status 0x%zx",
                  Status);
        goto Exit;
    }

    FilePath = DevicePathToTextIf->ConvertDevicePathToText(DevicePathProtocol, FALSE, FALSE);

    Status = StringPrintfW((PWSTR)BufferWide,
                           _countof(BufferWide),
                           L"%s file is located on volume '%s' at %s",
                           FileName,
                           VolumeLabel ? VolumeLabel : L"<empty>",
                           FilePath);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("StringPrintfW() failed");
        goto Exit;
    }

    UnicodeStrToAsciiStr(BufferWide, Buffer);

    DBG_INFO("%s", Buffer);

Exit:
    FreePool(FilePath);
    FreePool(VolumeLabel);
    FreePool(VolumeHandles);

    //
    // Volumes are not closed manually. As per UEFI spec, "The volume is closed
    // by closing all the open file handles"
    //

    return Status;
}

EFI_STATUS EFIAPI FileCreateSubdirectories(_In_z_ CHAR16* PartitionName,
                                           _In_reads_z_(DirectoryPathLength) CHAR16* DirectoryPath,
                                           _In_ UINTN DirectoryPathLength,
                                           _Out_ EFI_FILE_PROTOCOL** EfiFileProtocol)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* Directory = NULL;
    EFI_FILE_PROTOCOL* NewDirectory = NULL;
    CHAR16 Delimiters[] = L"\\/";
    CHAR16* DirectoryPathLocal = NULL;
    CHAR16* Context = NULL;
    CHAR16* Token = NULL;
    UINTN DirectoryPathSize = 0;

    Status = FilePartitionOpen(PartitionName, &Directory, NULL);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FilePartitionOpen() failed with status 0x%zx", Status);
        return Status;
    }

    DirectoryPathSize = (DirectoryPathLength + 1) * sizeof(CHAR16);

    DirectoryPathLocal = AllocateZeroPool(DirectoryPathSize);
    if (!DirectoryPathLocal) {
        return EFI_OUT_OF_RESOURCES;
    }

    Status = CopyMemS(DirectoryPathLocal, DirectoryPathSize, DirectoryPath, DirectoryPathSize);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CopyMemS() failed : 0x%zx", Status);
        goto Exit;
    }

    Token = WcstokCompat(DirectoryPathLocal, Delimiters, &Context);

    while (Token) {
        Status = Directory->Open(Directory,
                                 &NewDirectory,
                                 Token,
                                 EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                                 EFI_FILE_DIRECTORY);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("Open() failed with status 0x%zx", Status);
            goto Exit;
        }

        FileClose(Directory);

        Directory = NewDirectory;
        NewDirectory = NULL;

        Token = WcstokCompat(NULL, Delimiters, &Context);
    }

    *EfiFileProtocol = Directory;
    Directory = NULL;

Exit:
    FreePool(DirectoryPathLocal);

    if (Directory) {
        FileClose(Directory);
    }
    if (NewDirectory) {
        FileClose(NewDirectory);
    }

    return Status;
}

EFI_STATUS EFIAPI FileCreateSubdirectoriesAndFile(_In_z_ CHAR16* PartitionName,
                                                  _In_z_ CHAR16* FileName,
                                                  _Out_ EFI_FILE_PROTOCOL** EfiFileProtocol)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* DirFs = NULL;
    EFI_FILE_PROTOCOL* FileFs = NULL;
    CHAR16 Delimiters[] = L"\\/";
    CHAR16* FileNameLocal = NULL;
    CHAR16* Context = NULL;
    CHAR16* Token1 = NULL;
    CHAR16* Token2 = NULL;
    UINTN FileNameSize = 0;

    Status = FilePartitionOpen(PartitionName, &DirFs, NULL);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FilePartitionOpen() failed with status 0x%zx", Status);
        return Status;
    }

    FileNameSize = (StrnLenS(FileName, MAX_FILE_NAME_LEN) + 1) * sizeof(CHAR16);

    FileNameLocal = AllocateZeroPool(FileNameSize);
    if (!FileNameLocal) {
        return EFI_OUT_OF_RESOURCES;
    }

    Status = CopyMemS(FileNameLocal, FileNameSize, FileName, FileNameSize);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CopyMemS() failed : 0x%zx", Status);
        goto Exit;
    }

    Token1 = WcstokCompat(FileNameLocal, Delimiters, &Context);

    while (Token1) {
        Token2 = WcstokCompat(NULL, Delimiters, &Context);

        if (Token2) {
            // Token1 is a directory
            Status = DirFs->Open(DirFs,
                                 &FileFs,
                                 Token1,
                                 EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                                 EFI_FILE_DIRECTORY);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("Open() failed with status 0x%zx", Status);
                goto Exit;
            }

            FileClose(DirFs);

            DirFs = FileFs;
            FileFs = NULL;
        }

        Token1 = Token2;
        Token2 = NULL;
    }

    Status = FileOpen(PartitionName,
                      FileName,
                      EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                      0,
                      &FileFs);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileOpen() failed with status 0x%zx", Status);
        goto Exit;
    }

    *EfiFileProtocol = FileFs;
    FileFs = NULL;

Exit:
    FreePool(FileNameLocal);

    if (DirFs) {
        FileClose(DirFs);
    }
    if (FileFs) {
        FileClose(FileFs);
    }

    return Status;
}

EFI_STATUS EFIAPI FileDuplicate(_In_ EFI_FILE_PROTOCOL* SourceFile,
                                _In_ UINTN SourceOffset,
                                _In_ UINTN ByteCount,
                                _In_z_ CHAR16* PartitionName,
                                _In_z_ CHAR16* DestinationFilePath,
                                _Out_ EFI_FILE_PROTOCOL** DestinationFile)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* RetFile = NULL;
    UINT8* ReadBuffer = NULL;
    UINTN Offset = 0;
    UINT64 FileSize = 0;
    UINT64 BytesToCopy = 0;
    UINTN RemainingBytes = 0;
    UINTN ReadSize = 0;

    if (SourceFile == NULL || PartitionName == NULL || DestinationFilePath == NULL ||
        DestinationFile == NULL) {
        DBG_ERROR("Invalid parameter");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    Status = FileCreateSubdirectoriesAndFile(PartitionName, DestinationFilePath, &RetFile);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileCreateSubdirectoriesAndFile() failed with status 0x%zx", Status);
        goto Exit;
    }

    Status = FileGetSize(SourceFile, &FileSize);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileGetSize() failed 0x%zx", Status);
        goto Exit;
    }

    BytesToCopy = FileSize;

    //
    // Caller can choose to pass in number of bytes to copy. If ByteCount is
    // 0, then we copy the entire source file.
    //

    if (ByteCount != 0) {
        BytesToCopy = ByteCount;
        DBG_INFO_U(L"Copying %llu bytes to destination file %s", BytesToCopy, DestinationFilePath);
    }

    Offset = SourceOffset;

    if (Offset + BytesToCopy > FileSize) {
        DBG_ERROR("Offset(%zu) + BytesToCopy(%llu) > FileSize (%llu)",
                  Offset,
                  BytesToCopy,
                  FileSize);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    Status = FileSetPosition(SourceFile, Offset);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileSetPosition() failed 0x%zx", Status);
        goto Exit;
    }

    ReadBuffer = AllocatePool(DEFAULT_READ_BUFFER_SIZE);
    if (ReadBuffer == NULL) {
        DBG_ERROR("Out of memory");
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    RemainingBytes = (UINTN)BytesToCopy;
    ReadSize = (RemainingBytes > DEFAULT_READ_BUFFER_SIZE) ? DEFAULT_READ_BUFFER_SIZE :
                                                             RemainingBytes;

    while (RemainingBytes) {
        Status = FileRead(SourceFile, &ReadSize, ReadBuffer);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileRead() failed 0x%zx", Status);
            goto Exit;
        }

        Status = FileWrite(RetFile, &ReadSize, ReadBuffer);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileWrite() failed 0x%zx", Status);
            goto Exit;
        }

        RemainingBytes -= ReadSize;
        ReadSize = (RemainingBytes > DEFAULT_READ_BUFFER_SIZE) ? DEFAULT_READ_BUFFER_SIZE :
                                                                 RemainingBytes;
    }

    Status = FileSetPosition(RetFile, 0);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileSetPosition() failed 0x%zx", Status);
        goto Exit;
    }

    *DestinationFile = RetFile;

Exit:

    FreePool(ReadBuffer);
    return Status;
}

//
// Local functions
//

static EFI_STATUS EFIAPI FilePartitionOpen(_In_z_ CHAR16* PartitionName,
                                           _Out_ EFI_FILE_PROTOCOL** EfiFileProtocol,
                                           _Out_opt_ EFI_HANDLE* PartitionHandle)
{
    EFI_HANDLE* Handles = NULL;
    UINTN HandleCount = 0;
    EFI_STATUS Status = EFI_NOT_FOUND;

    *EfiFileProtocol = NULL;

    // Get all handles to existing simple filesystem protocol instances
    Status = gBS->LocateHandleBuffer(ByProtocol,
                                     &gEfiSimpleFileSystemProtocolGuid,
                                     NULL,
                                     &HandleCount,
                                     &Handles);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Unable to get handles for simple filesystem protocols: 0x%zx", Status);
    } else {
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* SimpleFs;
        EFI_PARTITION_INFO* PartitionInfo = NULL;
        EFI_PARTITION_ENTRY* PartitionEntry = NULL;

        DBG_VERBOSE("Handle count: %zd", HandleCount);

        for (UINTN Index = 0; Index < HandleCount; Index++) {
            Status = gBS->HandleProtocol(Handles[Index],
                                         &gEfiPartitionInfoProtocolGuid,
                                         (VOID**)&PartitionInfo);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("EfiPartitionInfoGuid not found for %zd: 0x%zx", Index, Status);

                // Check partition name
                Status = gBS->HandleProtocol(Handles[Index],
                                             &gEfiPartitionRecordGuid,
                                             (VOID**)&PartitionEntry);
                if (EFI_ERROR(Status)) {
                    DBG_ERROR("EfiPartitionRecordGuid not found for %zd: 0x%zx", Index, Status);
                    continue;
                }
            } else {
                if (PartitionInfo->Type != EFI_PARTITION_TYPE_GPT) {
                    // DBG_WARNING("Skipping non-GPT disks");
                    continue;
                }
                PartitionEntry = &PartitionInfo->Info.Gpt;
            }

            DBG_VERBOSE_U(L"Partition[0x%zx]: %s", Index, PartitionEntry->PartitionName);

            if (StrnLenS(PartitionEntry->PartitionName, MAX_PARTITION_NAME_LENGTH) !=
                    StrnLenS(PartitionName, MAX_PARTITION_NAME_LENGTH) ||
                CompareMem(PartitionEntry->PartitionName,
                           PartitionName,
                           (StrnLenS(PartitionName, MAX_PARTITION_NAME_LENGTH) + 1) *
                               sizeof(CHAR16))) {
                DBG_VERBOSE("Partition name does not match, 0x%zx", Index);
                continue;
            }

            DBG_VERBOSE("Partition name matches, 0x%zx", Index);

            // Correct partition was found, try to open file
            Status = gBS->HandleProtocol(Handles[Index],
                                         &gEfiSimpleFileSystemProtocolGuid,
                                         (VOID**)&SimpleFs);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("Unable to get handle for simple filesystem protocol (0x%zx): 0x%zx",
                          Index,
                          Status);
                return Status;
            }

            Status = SimpleFs->OpenVolume(SimpleFs, EfiFileProtocol);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("OpenVolume() 0x%zx failed with status 0x%zx", Index, Status);
                return Status;
            }

            if (PartitionHandle != NULL) {
                *PartitionHandle = Handles[Index];
            }

            break;
        }
    }

    FreePool(Handles);

    if (*EfiFileProtocol == NULL) {
        Status = EFI_NOT_FOUND;
    }

    if (EFI_ERROR(Status)) {
        DBG_ERROR("FilePartitionOpen() failed with status 0x%zx", Status);
    }

    return Status;
}

static EFI_STATUS EFIAPI FileIsDirectory(_In_ EFI_FILE_PROTOCOL* DirFs)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_INFO* DirInfo = NULL;

    Status = FileGetInfo(DirFs, (VOID*)&DirInfo);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileGetInfo() failed with status 0x%zx", Status);
        goto Exit;
    }

    if (DirInfo == NULL) {
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if ((DirInfo->Attribute & EFI_FILE_DIRECTORY) == 0) {
        Status = EFI_NOT_FOUND;
        goto Exit;
    }

Exit:
    FreePool(DirInfo);

    return Status;
}

static EFI_STATUS FileReadByte(_In_ EFI_FILE_PROTOCOL* EfiFileProtocol,
                               _Out_ CHAR8* Char,
                               _Out_ BOOLEAN* EndOfFile)
{
    UINTN Size = 1;
    EFI_STATUS Status = EFI_SUCCESS;

    Status = EfiFileProtocol->Read(EfiFileProtocol, &Size, Char);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    *EndOfFile = Size != 1;

    return Status;
}

static EFI_STATUS FileCreateDevicePathFromString(_In_z_ CHAR16* FileName,
                                                 _Out_ EFI_DEVICE_PATH_PROTOCOL** DevicePath)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_DEVICE_PATH_PROTOCOL* FilePathDevicePath = NULL;
    UINTN FilePathDevicePathSize = 0;
    EFI_DEVICE_PATH_PROTOCOL* DevicePathEndNode = NULL;
    UINTN DevicePathNodeSize = 0;

    DevicePathNodeSize = StrnLenS(FileName, MAX_FILE_NAME_LEN);
    DevicePathNodeSize = sizeof(CHAR16) * (DevicePathNodeSize + 1) + sizeof(EFI_DEVICE_PATH);
    FilePathDevicePathSize = DevicePathNodeSize + sizeof(EFI_DEVICE_PATH);

    FilePathDevicePath = AllocateZeroPool(FilePathDevicePathSize);
    if (FilePathDevicePath == NULL) {
        DBG_ERROR("Unable to allocate memory for device path");
        return EFI_OUT_OF_RESOURCES;
    }

    // Fill out device path information
    FilePathDevicePath->Type = MEDIA_DEVICE_PATH;
    FilePathDevicePath->SubType = MEDIA_FILEPATH_DP;
    FilePathDevicePath->Length[0] = (UINT8)DevicePathNodeSize;
    FilePathDevicePath->Length[1] = (UINT8)(DevicePathNodeSize >> 8);
    Status = CopyMemS((PUINT8)FilePathDevicePath + sizeof(EFI_DEVICE_PATH),
                      FilePathDevicePathSize - sizeof(EFI_DEVICE_PATH),
                      FileName,
                      sizeof(CHAR16) * (StrnLenS(FileName, MAX_FILE_NAME_LEN) + 1));
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CopyMemS() failed : 0x%zx", Status);
        goto Exit;
    }

    DevicePathEndNode = (EFI_DEVICE_PATH*)((PUINT8)FilePathDevicePath + DevicePathNodeSize);
    DevicePathEndNode->Type = END_DEVICE_PATH_TYPE;
    DevicePathEndNode->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
    DevicePathEndNode->Length[0] = sizeof(EFI_DEVICE_PATH);
    DevicePathEndNode->Length[1] = 0;

    *DevicePath = FilePathDevicePath;

Exit:
    if (EFI_ERROR(Status)) {
        FreePool(FilePathDevicePath);
    }

    return Status;
}

static EFI_STATUS FileGetDevicePathInternal(_In_ EFI_HANDLE PartitionHandle,
                                            _In_z_ CHAR16* FileName,
                                            _Out_ EFI_DEVICE_PATH_PROTOCOL** DevicePathProtocol)
{
    EFI_STATUS Status = EFI_NOT_FOUND;
    EFI_DEVICE_PATH_PROTOCOL* PartitionDevicePath = NULL;
    EFI_DEVICE_PATH_PROTOCOL* FilePathDevicePath = NULL;
    EFI_DEVICE_PATH_PROTOCOL* PartitionAndFileDevicePath = NULL;
    EFI_DEVICE_PATH_UTILITIES_PROTOCOL* DevicePathUtilitiesIf = NULL;
    EFI_DEVICE_PATH_TO_TEXT_PROTOCOL* DevicePathToTextIf = NULL;
    CHAR16* CombinedDevicePath = NULL;

    if (PartitionHandle == NULL || FileName == NULL) {
        DBG_ERROR("Invalid parameter: PartitionHandle 0x%p, FileName 0x%p, DevicePathProtocol 0x%p",
                  PartitionHandle,
                  FileName,
                  DevicePathProtocol);
        return EFI_INVALID_PARAMETER;
    }

    Status = gBS->LocateProtocol(&gEfiDevicePathToTextProtocolGuid,
                                 NULL,
                                 (PVOID*)&DevicePathToTextIf);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("LocateProtocol() for DevicePathToText protocol failed with status 0x%zx",
                  Status);
        return Status;
    }

    Status = gBS->LocateProtocol(&gEfiDevicePathUtilitiesProtocolGuid,
                                 NULL,
                                 (PVOID*)&DevicePathUtilitiesIf);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("LocateProtocol() for DevicePathUtilities protocol failed with status 0x%zx",
                  Status);
        return Status;
    }

    // Get partition device path
    PartitionDevicePath = DevicePathFromHandle(PartitionHandle);
    if (PartitionDevicePath == NULL) {
        DBG_ERROR("DevicePathFromHandle() returned NULL pointer for handle 0x%p", PartitionHandle);
        return Status;
    }

    Status = FileCreateDevicePathFromString(FileName, &FilePathDevicePath);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileCreateDevicePathFromString() failed 0x%zx", Status);
        return Status;
    }

    // Append partition device path + file path device path
    PartitionAndFileDevicePath = DevicePathUtilitiesIf->AppendDevicePath(PartitionDevicePath,
                                                                         FilePathDevicePath);

    FreePool(FilePathDevicePath);

    // Print out combined device path
    CombinedDevicePath = DevicePathToTextIf->ConvertDevicePathToText(PartitionAndFileDevicePath,
                                                                     FALSE,
                                                                     FALSE);
    DBG_VERBOSE_U(L"Combined device path: %s", CombinedDevicePath);
    *DevicePathProtocol = PartitionAndFileDevicePath;

    return Status;
}

EFI_STATUS EFIAPI FileFindFirst(_In_ EFI_FILE_PROTOCOL* Directory)
{
    EFI_STATUS Status = EFI_SUCCESS;

    Status = FileSetPosition(Directory, 0);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileSetPosition() failed with status 0x%zx", Status);
        goto Exit;
    }

Exit:
    return Status;
}

EFI_STATUS EFIAPI FileFindNext(_In_ EFI_FILE_PROTOCOL* Directory, _Outptr_ EFI_FILE_INFO** FileInfo)
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINTN BufferSize = FIND_XXXXX_FILE_BUFFER_SIZE;
    EFI_FILE_INFO* RetFileInfo = NULL;

    *FileInfo = NULL;

    RetFileInfo = AllocateZeroPool(BufferSize);
    if (RetFileInfo == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    Status = Directory->Read(Directory, &BufferSize, RetFileInfo);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Read() failed with status 0x%zx", Status);
        goto Exit;
    }

    if (BufferSize == 0) {
        *FileInfo = NULL;
        goto Exit;
    }

    *FileInfo = RetFileInfo;
    return Status;

Exit:
    FreePool(RetFileInfo);
    return Status;
}

//
// Useful to just create an empty directory in the destination directory
//
EFI_STATUS EFIAPI FileCreateDirectory(_In_ EFI_FILE_PROTOCOL* DestDirectory,
                                      _In_ CHAR16* DirectoryName)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* NewDirectory = NULL;

    Status = DestDirectory->Open(DestDirectory,
                                 &NewDirectory,
                                 DirectoryName,
                                 EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                                 EFI_FILE_DIRECTORY);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Open() failed 0x%zx", Status);
        goto Exit;
    }

Exit:
    if (NewDirectory != NULL) {
        NewDirectory->Close(NewDirectory);
    }

    return Status;
}

//
// This method copies a file from Source location as a file in the destination
// It only deals with file to file copy. No directories involved. This include
// overwriting existing file.
//

static EFI_STATUS FileCopyToFile(_In_ EFI_FILE_PROTOCOL* SourceFile,
                                 _In_ EFI_FILE_PROTOCOL* DestFile)
{
    EFI_STATUS Status = EFI_SUCCESS;
    VOID* Buffer = NULL;
    UINTN BytesRead = 0;

#define READ_SIZE (1024 * 1024) // 1 MB

    Status = FileSetSize(DestFile, 0); // Truncate the destination file first
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileSetSize() failed 0x%zx", Status);
        goto Exit;
    }

    Buffer = AllocateZeroPool(READ_SIZE);
    if (Buffer == NULL) {
        DBG_ERROR("Unable to allocate memory");
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    while (TRUE) {
        BytesRead = READ_SIZE;
        Status = FileRead(SourceFile, &BytesRead, Buffer);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileRead() failed 0x%zx", Status);
            goto Exit;
        }

        if (BytesRead == 0) {
            break;
        }

        do {
            UINTN BytesWrote = BytesRead;
            Status = FileWrite(DestFile, &BytesWrote, Buffer);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("FileWrite() failed 0x%zx", Status);
                goto Exit;
            }
            BytesRead -= BytesWrote;
        } while (BytesRead > 0);
    }

Exit:
    FreePool(Buffer);
    return Status;
}

//
// This method copies a file from Source location to a destination directory.
// The destination have to be a directory.
//
static EFI_STATUS FileCopyToDirectory(_In_ EFI_FILE_PROTOCOL* SourceFile,
                                      _In_ EFI_FILE_PROTOCOL* DestDirectory)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* DestFile = NULL;
    EFI_FILE_INFO* FileInfo = NULL;

    Status = FileGetInfo(SourceFile, &FileInfo);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileGetInfo() failed 0x%zx", Status);
        goto Exit;
    }

    Status = DestDirectory->Open(DestDirectory,
                                 &DestFile,
                                 FileInfo->FileName,
                                 EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                                 0);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Open() failed 0x%zx", Status);
        goto Exit;
    }

    Status = FileCopyToFile(SourceFile, DestFile);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileCopyToFile() failed 0x%zx", Status);
        goto Exit;
    }

Exit:
    if (DestFile != NULL) {
        DestFile->Close(DestFile);
    }

    FreePool(FileInfo);
    return Status;
}

//
// This method recursively copies contents from source directory in to the
// destination directory. The source and destination have to be directories.
//
static EFI_STATUS FileCopyRecursive(_In_ EFI_FILE_PROTOCOL* SourceDir,
                                    _In_ EFI_FILE_PROTOCOL* DestDir)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_INFO* FileInfo = NULL;
    EFI_FILE_PROTOCOL* File = NULL;
    EFI_FILE_PROTOCOL* Directory1 = NULL;
    EFI_FILE_PROTOCOL* Directory2 = NULL;

    Status = FileFindFirst(SourceDir);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Read() failed with status 0x%zx", Status);
        goto Exit;
    }

    do {
        File = NULL;
        FileInfo = NULL;
        Directory1 = NULL;
        Directory2 = NULL;

        Status = FileFindNext(SourceDir, &FileInfo);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("Read() failed with status 0x%zx", Status);
            goto Cleanup;
        }

        if (FileInfo == NULL) { // No more files
            break;
        }

        if (StrCmp(FileInfo->FileName, L".") == 0 || StrCmp(FileInfo->FileName, L"..") == 0) {
            goto Cleanup;
        }

        DBG_INFO_U(L"Copying %-15s %s",
                   ((FileInfo->Attribute & EFI_FILE_DIRECTORY) == EFI_FILE_DIRECTORY) ?
                       L"Directory" :
                       L"File",
                   FileInfo->FileName);

        if ((FileInfo->Attribute & EFI_FILE_DIRECTORY) == EFI_FILE_DIRECTORY) {
            //
            // Open the directory entry present in the source directory
            //

            Status = SourceDir->Open(SourceDir,
                                     &Directory1,
                                     FileInfo->FileName,
                                     EFI_FILE_MODE_READ,
                                     EFI_FILE_DIRECTORY);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("Open() failed 0x%zx", Status);
                goto Cleanup;
            }

            //
            // Create a directory inside the destination directory
            //

            Status = FileCreateDirectory(DestDir, FileInfo->FileName);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("FileCreateDirectory() failed 0x%zx", Status);
                goto Cleanup;
            }

            //
            // Open the directory entry present in the destination directory
            //

            Status = DestDir->Open(DestDir,
                                   &Directory2,
                                   FileInfo->FileName,
                                   EFI_FILE_MODE_READ,
                                   EFI_FILE_DIRECTORY);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("Open() failed 0x%zx", Status);
                goto Cleanup;
            }

            //
            // Invoke recursive copy with both the directory entries
            //

            Status = FileCopyRecursive(Directory1, Directory2);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("FileCopy() failed 0x%zx", Status);
                goto Cleanup;
            }
        } else {
            //
            // Open the file entry present in the source directory
            //

            Status = SourceDir->Open(SourceDir, &File, FileInfo->FileName, EFI_FILE_MODE_READ, 0);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("Open() failed 0x%zx", Status);
                goto Cleanup;
            }

            //
            // Copy the file entry present in the source directory to
            // destination directory
            //

            Status = FileCopy(File, DestDir);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("FileCopy() failed 0x%zx", Status);
                goto Cleanup;
            }
        }

    Cleanup:
        FreePool(FileInfo);

        if (File != NULL) {
            File->Close(File);
        }

        if (Directory1 != NULL) {
            Directory1->Close(Directory1);
        }

        if (Directory2 != NULL) {
            Directory2->Close(Directory2);
        }
    } while (FileInfo != NULL);

Exit:
    return Status;
}

static CHAR16* EFIAPI WcstokCompat(_In_z_ CHAR16* String,
                                   _In_z_ const CHAR16* Delim,
                                   _Out_ CHAR16** SavePtr)
{
    CHAR16* Result;

    if (String == NULL) {
        if (*SavePtr == NULL) {
            return NULL;
        } else
            String = *SavePtr;
    }

    //
    // Scan leading delimiters.
    //

    String += WcsspnCompat(String, Delim);
    if (*String == L'\0') {
        *SavePtr = NULL;
        return NULL;
    }

    //
    // Find the end of the token.
    //

    Result = String;
    String = WcspbrkCompat(Result, Delim);
    if (String == NULL)
        //
        // This token finishes the string.
        //

        *SavePtr = NULL;
    else {
        //
        // Terminate the token and make *SavePtr point past it.
        //

        *String = L'\0';
        *SavePtr = String + 1;
    }

    return Result;
}

//
// Returns the length of the initial portion of String which consists only of wide characters that are
// part of String2. The search does not include the terminating null wide characters of either strings,
// but ends there.
//

static UINTN EFIAPI WcsspnCompat(_In_z_ CHAR16* String, _In_z_ const CHAR16* String2)
{
    const CHAR16* p;
    const CHAR16* a;
    UINTN Count = 0;

    for (p = String; *p != L'\0'; ++p) {
        for (a = String2; *a != L'\0'; ++a)
            if (*p == *a)
                break;
        if (*a == L'\0')
            return Count;
        else
            ++Count;
    }

    return Count;
}

//
// Returns a pointer to the first occurrence in String of any of the wide characters that are part of
// String2, or a null pointer if there are no matches. The search does not include the terminating null
// wide characters of either wide strings, but ends there.
//

static CHAR16* EFIAPI WcspbrkCompat(_In_z_ CHAR16* String, _In_z_ const CHAR16* String2)
{
    CHAR16* p;
    const CHAR16* a;

    for (p = String; *p != L'\0'; ++p) {
        for (a = String2; *a != L'\0'; ++a) {
            if (*p == *a) {
                return p;
            }
        }
    }

    return NULL;
}

//
// This is the public facing API to perform file/directory copy(including
// recursive copy).
//
//  Source     |  Destination  | Comment
//-------------+---------------+-----------
//  File       |  Directory    | Valid - Create or Overwrite
//  File       |  File         | Valid - Create or Overwrite
//  Directory  |  Directory    | Valid - Create Recursive
//  Directory  |  File         | Invalid
//
//
EFI_STATUS EFIAPI FileCopy(_In_ EFI_FILE_PROTOCOL* Source, _In_ EFI_FILE_PROTOCOL* Destination)
{
    EFI_STATUS Status = EFI_SUCCESS;
    BOOLEAN IsSourceDirectory = FALSE;
    BOOLEAN IsDestinationDirectory = FALSE;

    IsSourceDirectory = !EFI_ERROR(FileIsDirectory(Source));
    IsDestinationDirectory = !EFI_ERROR(FileIsDirectory(Destination));

    if (!IsSourceDirectory && IsDestinationDirectory) {
        Status = FileCopyToDirectory(Source, Destination);
    } else if (!IsSourceDirectory && !IsDestinationDirectory) {
        Status = FileCopyToFile(Source, Destination);
    } else if (IsSourceDirectory && IsDestinationDirectory) {
        Status = FileCopyRecursive(Source, Destination);
    } else {
        Status = EFI_INVALID_PARAMETER;
    }

    return Status;
}

#ifndef UEFI_BUILD_SYSTEM
#pragma prefast(pop)
#endif
