/*++

Copyright (c) 2021 Microsoft Corporation

Module Name:

    cab_extract.c

Abstract:

    This module implements limited CAB file extraction support.

Author:

    Jancarlo Perez (jpere) 18-Nov-2021

Environment:

    UEFI mode only.

--*/

#include "cbmrincludes.h"
#include "file.h"
#include "strsafe.h"
#include "cabinet.h"
#include "cab_extract.h"
#include "error.h"

#ifndef UEFI_BUILD_SYSTEM
#pragma prefast(push)
#pragma prefast(disable : 6101, \
                "False positive - _Out_ parameter is always initialized upon success")
#endif

// TODO: Finish macro
#define DUMP_CAB_HEADER(x)                                              \
    {                                                                   \
        DBG_INFO("sig: 0x%04X", ((CFHEADER*)(x))->sig);                 \
        DBG_INFO("csumHeader: 0x%08X", ((CFHEADER*)(x))->csumHeader);   \
        DBG_INFO("cbCabinet: %d", ((CFHEADER*)(x))->cbCabinet);         \
        DBG_INFO("csumFolders: 0x%08X", ((CFHEADER*)(x))->csumFolders); \
        DBG_INFO("coffFiles: 0x%08X", ((CFHEADER*)(x))->coffFiles);     \
        DBG_INFO("csumFiles: 0x%08X", ((CFHEADER*)(x))->csumFiles);     \
        DBG_INFO("version: 0x%04X", ((CFHEADER*)(x))->version);         \
        DBG_INFO("cFolders: %d", ((CFHEADER*)(x))->cFolders);           \
        DBG_INFO("cFiles: %d", ((CFHEADER*)(x))->cFiles);               \
        DBG_INFO("flags: 0x%04X", ((CFHEADER*)(x))->flags);             \
        DBG_INFO("setID: 0x%04X", ((CFHEADER*)(x))->setID);             \
        DBG_INFO("iCabinet: 0x%04X", ((CFHEADER*)(x))->iCabinet);       \
    }

typedef struct _CAB_EXTRACT_CONTEXT {
    BOOLEAN Initialized;

    CFHEADER CabHeader;
    EFI_FILE_PROTOCOL* CabFile;
} CAB_EXTRACT_CONTEXT, *PCAB_EXTRACT_CONTEXT;

static EFI_STATUS EFIAPI Decompress(UINT16 TypeCompress,
                                    UINT8* CompressedBlock,
                                    UINTN CompressedBlockSize,
                                    UINT8* UncompressedBlock,
                                    UINTN UncompressedBlockSize)
{
    UNREFERENCED_PARAMETER(TypeCompress);
    UNREFERENCED_PARAMETER(CompressedBlock);
    UNREFERENCED_PARAMETER(CompressedBlockSize);
    UNREFERENCED_PARAMETER(UncompressedBlock);
    UNREFERENCED_PARAMETER(UncompressedBlockSize);

    return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI CabExtractInit(_In_ EFI_FILE_PROTOCOL* CabFile,
                                 _Outptr_ CAB_EXTRACT_CONTEXT** Context)
{
    EFI_STATUS Status = EFI_SUCCESS;
    CAB_EXTRACT_CONTEXT* RetContext = NULL;
    UINTN ReadSize = 0;
    UINT64 FileSize = 0;

    if (CabFile == NULL) {
        DBG_ERROR("Invalid parameter");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    RetContext = AllocateZeroPool(sizeof(CAB_EXTRACT_CONTEXT));
    if (RetContext == NULL) {
        DBG_ERROR("Out of memory");
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    //
    // Read CAB header
    //

    Status = FileSetPosition(CabFile, 0);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileSetPosition() failed 0x%zx", Status);
        goto Exit;
    }

    ReadSize = sizeof(RetContext->CabHeader);
    Status = FileRead(CabFile, &ReadSize, (UINT8*)&RetContext->CabHeader);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileRead() failed 0x%zx", Status);
        goto Exit;
    }

    //
    // Perform some sanity checks
    //

    //
    // Make sure this is a CAB file
    //

    if (RetContext->CabHeader.sig != sigCFHEADER) {
        DBG_ERROR("Not a CAB (signature 0x%04X), skipping", RetContext->CabHeader.sig);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    //
    // File size check
    //

    Status = FileGetSize(CabFile, &FileSize);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileGetSize() failed 0x%zx", Status);
        goto Exit;
    }

    if (FileSize != RetContext->CabHeader.cbCabinet) {
        DBG_ERROR("Invalid CAB file size. Expected: %lu, Actual:%llu",
                  RetContext->CabHeader.cbCabinet,
                  FileSize);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    //
    // We currently only support flags == 0. Fail out if other flags found (like
    // cfhdrRESERVE_PRESENT)
    //

    if (RetContext->CabHeader.flags != 0) {
        DBG_ERROR("Unsupported CAB header flags present 0x%04X", RetContext->CabHeader.flags);
        Status = EFI_UNSUPPORTED;
        goto Exit;
    }

    //
    // Dump CAB header
    //

    DUMP_CAB_HEADER(&RetContext->CabHeader);

    RetContext->CabFile = CabFile;
    RetContext->Initialized = TRUE;

    *Context = RetContext;

    return Status;

Exit:

    *Context = NULL;
    FreePool(RetContext);

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_CAB_INITIALIZATION_FAILED);
    }

    return Status;
}

EFI_STATUS EFIAPI CabExtractFree(_Inout_ CAB_EXTRACT_CONTEXT* Context,
                                 _In_ BOOLEAN DeleteOriginalCab)
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (Context == NULL) {
        DBG_ERROR("Context is NULL");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (!Context->Initialized) {
        DBG_ERROR("Context has not been initialized");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    if (DeleteOriginalCab) {
        Status = FileDelete(Context->CabFile);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileDelete() failed 0x%zx", Status);
        }
    } else {
        Status = FileClose(Context->CabFile);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileClose() failed 0x%zx", Status);
        }
    }

    Context->Initialized = FALSE;
    FreePool(Context);

Exit:
    return Status;
}

EFI_STATUS EFIAPI CabExtractFiles(_In_ CAB_EXTRACT_CONTEXT* Context,
                                  _In_z_ CHAR16* PartitionName,
                                  _In_z_ CHAR16* DestinationDirectory)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_FILE_PROTOCOL* DestinationFile = NULL;
    UINTN NumFolders = 0;
    UINTN NumFiles = 0;
    CFFOLDER Folder = {0};
    CFFILE File = {0};
    CFDATA DataBlock = {0};
    CHAR8 AsciiFileName[1024] = {0};
    CHAR16 FileName[1024] = {0};
    CHAR16 FullPath[1024] = {0};
    UINTN CfFolderOffset = 0;
    UINTN CfFileOffset = 0;
    UINTN DataBlockOffset = 0;
    UINTN NumDataBlocksForFolder = 0;
    UINTN ReadSize = 0;
    UINTN CurrentFolder = 0;
    UINTN FilesProcessed = 0;
    UINTN FileNameLength = 0;
    CHAR8 c = 0;
    UINT16 TypeCompress = 0;
    UINT8* CompressedBlock = NULL;
    UINTN CompressedBlockSize = 0;
    UINT8* UncompressedBlock = NULL;
    UINTN UncompressedBlockSize = 0;
    UINTN BytesRemaining = 0;
    UINTN CurrentBlockOffset = 0;
    UINTN BytesToRead = 0;
    BOOLEAN ReadNextDataBlock = TRUE;
    UINTN DataBlocksProcessed = 0;

    if (Context == NULL || PartitionName == NULL || DestinationDirectory == NULL) {
        DBG_ERROR("Invalid parameter");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (!Context->Initialized) {
        DBG_ERROR("Context is not initialized");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    NumFolders = Context->CabHeader.cFolders;
    NumFiles = Context->CabHeader.cFiles;

    //
    // Set offset to first CFFOLDER, which immediately follows
    // the CFHEADER.
    //

    CfFolderOffset = sizeof(CFHEADER);

    //
    // Set offset to first CFFILE struct, as specified in CFHEADER
    //

    CfFileOffset = Context->CabHeader.coffFiles;

    for (UINTN i = 0; i < NumFolders; i++) {
        Status = FileSetPosition(Context->CabFile, CfFolderOffset);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileSetPosition() failed 0x%zx", Status);
            goto Exit;
        }

        ReadSize = sizeof(CFFOLDER);
        Status = FileRead(Context->CabFile, &ReadSize, (UINT8*)&Folder);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileRead() failed 0x%zx", Status);
            goto Exit;
        }

        if (ReadSize != sizeof(CFFOLDER)) {
            DBG_ERROR("Invalid read size %zu, expected %zu", ReadSize, sizeof(CFFOLDER));
            Status = EFI_BAD_BUFFER_SIZE;
            goto Exit;
        }

        //
        // Cache folder position for next CFFOLDER struct read in this loop.
        //

        CfFolderOffset += ReadSize;

        //
        // Only uncompressed payload is currently supported.
        //

        TypeCompress = Folder.typeCompress;
        if (TypeCompress != 0) {
            DBG_ERROR("Unsupported compression type %d found", TypeCompress);
            Status = EFI_UNSUPPORTED;
            goto Exit;
        }

        DataBlockOffset = Folder.coffCabStart;
        NumDataBlocksForFolder = Folder.cCFData;
        DataBlocksProcessed = 0;

        //
        // Enumerate through all files for this folder
        //

        do {
            Status = FileSetPosition(Context->CabFile, CfFileOffset);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("FileGetPosition() failed 0x%zx", Status);
                goto Exit;
            }

            ReadSize = sizeof(CFFILE);
            Status = FileRead(Context->CabFile, &ReadSize, (UINT8*)&File);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("FileRead() failed 0x%zx", Status);
                goto Exit;
            }

            CurrentFolder = File.iFolder;
            if (CurrentFolder != i) {
                //
                // The current file is no longer part of this folder
                //
                break;
            }

            //
            // Read file name
            //

            FileNameLength = 0;
            ReadSize = sizeof(CHAR8);

            do {
                Status = FileRead(Context->CabFile, &ReadSize, &c);
                if (EFI_ERROR(Status)) {
                    DBG_ERROR("FileRead() failed 0x%zx", Status);
                    goto Exit;
                }

                if (ReadSize != sizeof(CHAR8)) {
                    DBG_ERROR("Invalid read size %zu, expected %zu", ReadSize, sizeof(CHAR8));
                    Status = EFI_BAD_BUFFER_SIZE;
                    goto Exit;
                }

                AsciiFileName[FileNameLength] = c;
                FileNameLength++;

                if (c == 0) {
                    //
                    // Reached end of file name
                    //

                    break;
                }
            } while (c != 0);

            //
            // Update file offset. This now points to the next CFFILE (assuming more still remain).
            //

            CfFileOffset += sizeof(CFFILE) + FileNameLength;

            //
            // Concatenate target directory with retrieved file name
            //

            AsciiStrToUnicodeStr(AsciiFileName, FileName);

            UINTN Result = 0;
            Result = StringCchPrintfW(FullPath,
                                      _countof(FullPath),
                                      (CHAR16*)L"%s\\%s",
                                      DestinationDirectory,
                                      FileName);
            if (FAILED(Result)) {
                DBG_ERROR("StringCchPrintfW failed 0x%zx", Result);
                Status = EFI_INVALID_PARAMETER;
                goto Exit;
            }

            //
            // Create target file
            //

            Status = FileCreateSubdirectoriesAndFile(PartitionName, FullPath, &DestinationFile);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("FileCreateSubdirectoriesAndFile() failed 0x%zx", Status);
                goto Exit;
            }

            BytesRemaining = File.cbFile;

            while (BytesRemaining) {
                //
                // Check if there are bytes remaining to be read from a previous CFDATA block.
                // If so, don't read new CFDATA blocks until the previous block has been processed.
                //
                if (ReadNextDataBlock) {
                    ReadNextDataBlock = FALSE;
                    CurrentBlockOffset = 0;

                    //
                    // Read new CFDATA header
                    //

                    Status = FileSetPosition(Context->CabFile, DataBlockOffset);
                    if (EFI_ERROR(Status)) {
                        DBG_ERROR("FileSetPosition() failed 0x%zx", Status);
                        goto Exit;
                    }

                    ReadSize = sizeof(CFDATA);
                    Status = FileRead(Context->CabFile, &ReadSize, (UINT8*)&DataBlock);
                    if (EFI_ERROR(Status)) {
                        DBG_ERROR("FileRead() failed 0x%zx", Status);
                        goto Exit;
                    }

                    if (ReadSize != sizeof(CFDATA)) {
                        DBG_ERROR("Invalid read size %zu, expected %zu", ReadSize, sizeof(CFDATA));
                        Status = EFI_BAD_BUFFER_SIZE;
                        goto Exit;
                    }

                    CompressedBlockSize = DataBlock.cbData;
                    UncompressedBlockSize = DataBlock.cbUncomp;
                    CompressedBlock = AllocatePool(CompressedBlockSize);
                    if (CompressedBlock == NULL) {
                        DBG_ERROR("Out of memory");
                        Status = EFI_OUT_OF_RESOURCES;
                        goto Exit;
                    }

                    //
                    // Read the compressed block
                    //

                    ReadSize = CompressedBlockSize;
                    Status = FileRead(Context->CabFile, &ReadSize, CompressedBlock);
                    if (EFI_ERROR(Status)) {
                        DBG_ERROR("FileRead() failed 0x%zx", Status);
                        goto Exit;
                    }

                    if (ReadSize != CompressedBlockSize) {
                        DBG_ERROR("Invalid read size %zu, expected %zu",
                                  ReadSize,
                                  CompressedBlockSize);
                        Status = EFI_BAD_BUFFER_SIZE;
                        goto Exit;
                    }

                    //
                    // Note: Following Decompress routine is just a stub. Decompression is not
                    // supported today.
                    //

                    if (TypeCompress == 0) {
                        UncompressedBlock = CompressedBlock;
                        CompressedBlock = NULL;
                    } else {
                        UncompressedBlock = AllocatePool(UncompressedBlockSize);
                        if (UncompressedBlock == NULL) {
                            DBG_ERROR("Out of memory");
                            Status = EFI_OUT_OF_RESOURCES;
                            goto Exit;
                        }

                        Status = Decompress(TypeCompress,
                                            CompressedBlock,
                                            CompressedBlockSize,
                                            UncompressedBlock,
                                            UncompressedBlockSize);
                        if (EFI_ERROR(Status)) {
                            DBG_ERROR("FileSetPosition() failed 0x%zx", Status);
                            goto Exit;
                        }
                    }

                    //
                    // Update offset so we're ready for the next CFDATA block
                    //

                    DataBlockOffset += sizeof(CFDATA) + DataBlock.cbData;
                    DataBlocksProcessed++;
                }

                //
                // Calculate number of bytes to read
                //

                if (BytesRemaining > UncompressedBlockSize - CurrentBlockOffset) {
                    BytesToRead = UncompressedBlockSize - CurrentBlockOffset;
                } else {
                    BytesToRead = BytesRemaining;
                }

                //
                // Write bytes to target file
                //

                Status = FileWrite(DestinationFile,
                                   &BytesToRead,
                                   UncompressedBlock + CurrentBlockOffset);
                if (EFI_ERROR(Status)) {
                    DBG_ERROR("FileWrite() failed 0x%zx", Status);
                    goto Exit;
                }

                CurrentBlockOffset += BytesToRead;
                BytesRemaining -= BytesToRead;

                if (CurrentBlockOffset == UncompressedBlockSize) {
                    ReadNextDataBlock = TRUE;

                    FreePool(CompressedBlock);
                    FreePool(UncompressedBlock);

                    CompressedBlock = NULL;
                    UncompressedBlock = NULL;
                }

                //
                // Check to see if full file has been written. If so, close handle and
                // break loop.
                //

                if (BytesRemaining == 0) {
                    FileClose(DestinationFile);
                    DestinationFile = NULL;

                    FilesProcessed++;

                    break;
                }
            }
        } while (CurrentFolder == i && FilesProcessed < NumFiles);

        if (DataBlocksProcessed != NumDataBlocksForFolder) {
            Status = EFI_ABORTED;
            DBG_ERROR("Folder (%zu): Incorrect number of data blocks processed %zu (expected %zu)",
                      CurrentFolder,
                      DataBlocksProcessed,
                      NumDataBlocksForFolder);
            goto Exit;
        }
    }

Exit:

    if (DestinationFile != NULL) {
        FileClose(DestinationFile);
    }

    FreePool(CompressedBlock);
    FreePool(UncompressedBlock);

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_CAB_EXTRACTION_FAILED);
    }

    return Status;
}

#ifndef UEFI_BUILD_SYSTEM
#pragma prefast(pop)
#endif