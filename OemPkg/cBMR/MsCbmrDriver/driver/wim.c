/*++

Copyright (c) 2021 Microsoft Corporation

Module Name:

    wim.c

Abstract:

    This module implements WIM file extraction support.

Author:

    Jancarlo Perez (jpere) 22-June-2021

Environment:

    UEFI mode only.

--*/

#include "cbmrincludes.h"
#include "file.h"
#include "XmlTypes.h"
#include "xmltreelib.h"
#include "xmltreequerylib.h"
#include "wim.h"
#include "wimfile.h"
#include "strsafe.h"
#include "error.h"

#ifndef UEFI_BUILD_SYSTEM
#pragma prefast(push)
#pragma prefast(disable : 6101, \
                "False positive - _Out_ parameter is always initialized upon success")
#endif

#define DUMP_WIM_HEADER(x)                                                                         \
    {                                                                                              \
        DBG_INFO("ImageTag: %s", ((LPWIMHEADER_PACKED)(x))->ImageTag);                             \
        DBG_INFO("Size: %d", ((LPWIMHEADER_PACKED)(x))->cbSize);                                   \
        DBG_INFO("Version: %08x", ((LPWIMHEADER_PACKED)(x))->dwVersion);                           \
        DBG_INFO("Flags: %08x", ((LPWIMHEADER_PACKED)(x))->dwFlags);                               \
        DBG_INFO("CompressionSize: %d", ((LPWIMHEADER_PACKED)(x))->dwCompressionSize);             \
        DBG_INFO("WIMGuid: {%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}", \
                 ((LPWIMHEADER_PACKED)(x))->gWIMGuid.Data1,                                        \
                 ((LPWIMHEADER_PACKED)(x))->gWIMGuid.Data2,                                        \
                 ((LPWIMHEADER_PACKED)(x))->gWIMGuid.Data3,                                        \
                 ((LPWIMHEADER_PACKED)(x))->gWIMGuid.Data4[0],                                     \
                 ((LPWIMHEADER_PACKED)(x))->gWIMGuid.Data4[1],                                     \
                 ((LPWIMHEADER_PACKED)(x))->gWIMGuid.Data4[2],                                     \
                 ((LPWIMHEADER_PACKED)(x))->gWIMGuid.Data4[3],                                     \
                 ((LPWIMHEADER_PACKED)(x))->gWIMGuid.Data4[4],                                     \
                 ((LPWIMHEADER_PACKED)(x))->gWIMGuid.Data4[5],                                     \
                 ((LPWIMHEADER_PACKED)(x))->gWIMGuid.Data4[6],                                     \
                 ((LPWIMHEADER_PACKED)(x))->gWIMGuid.Data4[7]);                                    \
        DBG_INFO("PartNumber: %d", ((LPWIMHEADER_PACKED)(x))->usPartNumber);                       \
        DBG_INFO("TotalParts: %d", ((LPWIMHEADER_PACKED)(x))->usTotalParts);                       \
        DBG_INFO("ImageCount: %d", ((LPWIMHEADER_PACKED)(x))->dwImageCount);                       \
        DBG_INFO("OffsetTable: Offset:%lld",                                                       \
                 ((LPWIMHEADER_PACKED)(x))->rhOffsetTable.Base.liOffset.QuadPart);                 \
        DBG_INFO("XmlData: Offset:%lld",                                                           \
                 ((LPWIMHEADER_PACKED)(x))->rhXmlData.Base.liOffset.QuadPart);                     \
        DBG_INFO("BootMetadata: Offset:%lld",                                                      \
                 ((LPWIMHEADER_PACKED)(x))->rhBootMetadata.Base.liOffset.QuadPart);                \
        DBG_INFO("BootIndex: %d", ((LPWIMHEADER_PACKED)(x))->dwBootIndex);                         \
        DBG_INFO("Integrity: Offset:%lld",                                                         \
                 ((LPWIMHEADER_PACKED)(x))->rhIntegrity.Base.liOffset.QuadPart);                   \
        DBG_INFO("CryptHashData: Offset:%lld",                                                     \
                 ((LPWIMHEADER_PACKED)(x))->rhCryptHashData.liOffset.QuadPart);                    \
    }

typedef struct _WIM_CONTEXT {
    BOOLEAN Initialized;

    WIMHEADER_PACKED WimHeader;
    EFI_FILE_PROTOCOL* WimFile;
    XmlNode2* XmlRoot;
    XmlNode2* ResourcesNode;
    XmlNode2* CbmrNode;
} WIM_CONTEXT, *PWIM_CONTEXT;

EFI_STATUS EFIAPI WimInit(_In_ EFI_FILE_PROTOCOL* WimFile, _Outptr_ WIM_CONTEXT** Context)
{
    EFI_STATUS Status = EFI_SUCCESS;
    WIM_CONTEXT* RetContext = NULL;
    UINTN ReadSize = 0;
    UINT64 XmlOffset = 0;
    UINT64 XmlSize = 0;
    UINT64 IntegritySize = 0;
    UINT32 CryptHashDataSize = 0;
    UINTN FileSizeCheck = 0;
    CHAR16* XmlBuffer = NULL;
    XmlNode2* XmlRoot = NULL;
    XmlNode2* ImageNode = NULL;
    XmlNode2* ResourcesNode = NULL;
    XmlNode2* CbmrNode = NULL;
    UINTN StringLength = 0;
    UINTN BufferSize = 0;
    UINT64 FileSize = 0;
    UINTN XmlStringSizeInBytes = 0;

    //
    // Sanity check
    //

    if (WimFile == NULL || Context == NULL) {
        Status = EFI_INVALID_PARAMETER;
        DBG_ERROR("WimFile %p, Context %p", WimFile, Context);
        goto Exit;
    }

    Status = FileGetSize(WimFile, &FileSize);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileGetSize() failed 0x%zx", Status);
        goto Exit;
    }

    if (FileSize == 0) {
        Status = EFI_INVALID_PARAMETER;
        DBG_ERROR("Invalid WIM size");
        goto Exit;
    }

    RetContext = AllocateZeroPool(sizeof(WIM_CONTEXT));
    if (RetContext == NULL) {
        DBG_ERROR("Out of memory");
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    //
    // Read WIM header
    //

    ReadSize = sizeof(RetContext->WimHeader);
    Status = FileRead(WimFile, &ReadSize, (UINT8*)&RetContext->WimHeader);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileRead() failed 0x%zx", Status);
        goto Exit;
    }

    //
    // Dump WIM information
    //

    DUMP_WIM_HEADER(&RetContext->WimHeader);

    //
    // Extract WIM XML data and deserialize it for later use. Extract integrity data size
    // and crypt hash data size as well.
    //

    XmlOffset = RetContext->WimHeader.rhXmlData.Base.liOffset.QuadPart;
    XmlSize = RetContext->WimHeader.rhXmlData.liOriginalSize.QuadPart;
    IntegritySize = RetContext->WimHeader.rhIntegrity.liOriginalSize.QuadPart;
    CryptHashDataSize = RetContext->WimHeader.rhCryptHashData.dwSize;

    if (XmlOffset == 0 || XmlOffset > FileSize) {
        DBG_ERROR("Invalid XML offset %llu. Full WIM size %llu", XmlOffset, FileSize);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (XmlSize == 0) {
        DBG_ERROR("Invalid XML Size %llu. Full WIM size %llu, XmlOffset %llu",
                  XmlSize,
                  FileSize,
                  XmlOffset);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    Status = UintnAdd((UINTN)XmlOffset, (UINTN)XmlSize, &FileSizeCheck);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("UintnAdd() failed 0x%zx", Status);
        goto Exit;
    }

    Status = UintnAdd(FileSizeCheck, (UINTN)IntegritySize, &FileSizeCheck);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("UintnAdd() failed 0x%zx", Status);
        goto Exit;
    }

    Status = UintnAdd(FileSizeCheck, CryptHashDataSize, &FileSizeCheck);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("UintnAdd() failed 0x%zx", Status);
        goto Exit;
    }

    //
    // Make sure the file size matches the sum of WIM header offset/size values
    //

    if (FileSize != FileSizeCheck) {
        DBG_ERROR(
            "Mismatching WIM size. Actual: %llu, Calculated %zu (XmlOffset %llu + XmlSize %llu + IntegritySize %llu + CryptHashDataSize %u)",
            FileSize,
            FileSizeCheck,
            XmlOffset,
            XmlSize,
            IntegritySize,
            CryptHashDataSize);

        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    Status = UintnAdd((UINTN)XmlSize, sizeof(CHAR16), &BufferSize);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("UintnAdd() failed 0x%zx", Status);
        goto Exit;
    }

    XmlBuffer = AllocateZeroPool(BufferSize);
    if (XmlBuffer == NULL) {
        DBG_ERROR("Out of memory");
        goto Exit;
    }

    ReadSize = (UINTN)XmlSize;

    Status = FileSetPosition(WimFile, XmlOffset);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileSetPosition() failed 0x%zx", Status);
        goto Exit;
    }

    Status = FileRead(WimFile, &ReadSize, (UINT8*)XmlBuffer);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("FileRead() failed 0x%zx", Status);
        goto Exit;
    }

    if (XmlSize != ReadSize) {
        Status = EFI_INVALID_PARAMETER;

        //
        // Somehow the purported XmlSize obtained from WIM header does not match
        // number of bytes returned by FileRead. Exit early, as this could indicate
        // tampering.
        //

        DBG_ERROR("Mismatching XML size. Expected (%zu), Actual (%zu)", XmlSize, ReadSize);
        goto Exit;
    }

    StringLength = StrnLenS(XmlBuffer, BufferSize / sizeof(CHAR16));

    //
    // Check if WIM XML has invalid NUL character at the beginning.
    //

    if (StringLength == 0) {
        Status = EFI_INVALID_PARAMETER;
        DBG_ERROR("Unexpected NUL character in WIM XML");
        goto Exit;
    }

    Status = UintnMult(StringLength, sizeof(CHAR16), &XmlStringSizeInBytes);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("UintnMult() failed 0x%zx", Status);
        goto Exit;
    }

    if (XmlStringSizeInBytes != XmlSize) {
        Status = EFI_INVALID_PARAMETER;
        DBG_ERROR("XML string length in bytes (%zu) does not match original XML size (%llu)",
                  StringLength * sizeof(CHAR16),
                  XmlSize);
        goto Exit;
    }

    Status = CreateXmlTreeW(XmlBuffer, StringLength, &XmlRoot);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CreateXmlTree() failed : 0x%zx", Status);
        goto Exit;
    }

    DebugPrintXmlTree(XmlRoot, 2);

    //
    // Check if <RESOURCES> node exists in WIM XML. If so, store it for easy lookup.
    // Find <IMAGE> node first, then attempt to find <RESOURCES> node. If not found,
    // continue without failing.
    //

    ImageNode = FindFirstChildNodeByName(XmlRoot, t("IMAGE"));
    if (ImageNode != NULL) {
        ResourcesNode = FindFirstChildNodeByName(ImageNode, t("RESOURCES"));
        if (ResourcesNode != NULL) {
            DBG_INFO("Found <RESOURCES> node!");
        }
    }

    //
    // Extract CBMR node for easy lookup later. Only si.wim should have this
    // node, so it's ok if other WIMs return NULL.
    //

    CbmrNode = FindFirstChildNodeByName(XmlRoot, t("CBMR"));

    RetContext->WimFile = WimFile;
    RetContext->XmlRoot = XmlRoot;
    RetContext->ResourcesNode = ResourcesNode;
    RetContext->CbmrNode = CbmrNode;
    RetContext->Initialized = TRUE;

    *Context = RetContext;

    FreePool(XmlBuffer);

    return Status;

Exit:

    *Context = NULL;
    FreePool(RetContext);
    FreePool(XmlBuffer);
    FreeXmlTree(&XmlRoot);

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_WIM_INITIALIZATION_FAILED);
    }

    return Status;
}

EFI_STATUS EFIAPI WimFree(_Inout_ WIM_CONTEXT* Context)
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

    FileClose(Context->WimFile);
    FreeXmlTree(&Context->XmlRoot);

    Context->Initialized = FALSE;
    FreePool(Context);

Exit:
    return Status;
}

EFI_STATUS EFIAPI WimExtractFileIntoDestination(_In_ WIM_CONTEXT* Context,
                                                _In_reads_z_(FilePathLength) CHAR8* FilePath,
                                                _In_ UINTN FilePathLength,
                                                _In_z_ CHAR16* DestinationPartitionName,
                                                _In_z_ CHAR16* DestinationFilePath)
{
    EFI_STATUS Status = EFI_NOT_FOUND;
    XmlNode2* FileNode = NULL;
    XmlNode2* PathNode = NULL;
    XmlNode2* FileOffsetNode = NULL;
    XmlNode2* FileSizeNode = NULL;
    UINTN FileOffset = 0;
    UINTN FileSize = 0;
    EFI_FILE_PROTOCOL* DestinationFile = NULL;

    if (Context == NULL || FilePath == NULL || DestinationPartitionName == NULL ||
        DestinationFilePath == NULL) {
        DBG_ERROR("Invalid parameter");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (!Context->Initialized) {
        DBG_ERROR("Context is not initialized");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    if (Context->ResourcesNode == NULL) {
        DBG_ERROR("<RESOURCES> node was not found during XML deserialization");
        Status = EFI_UNSUPPORTED;
        goto Exit;
    }

    //
    // Note, this function explicitly looks for a <RESOURCES> node inside a WIM, stored
    // in its <IMAGE> node. The <RESOURCES> node consists of one or more
    // <FILE> nodes, each containing a <PATH>, <OFFSET> and <SIZE> node. See below for an
    // example WIM XML structure containing a <RESOURCES> node.
    // <WIM>
    //     <TOTALBYTES>...</TOTALBYTES>
    //     <IMAGE>
    //         ...
    //         <RESOURCES>
    //             <FILE>
    //                 <PATH>\Windows\Boot\DVD\EFI\boot.sdi</PATH>
    //                 <OFFSET>0x60d0</OFFSET>
    //                 <SIZE>0x306000</SIZE>
    //                 <COMPRESSION>0x0</COMPRESSION>
    //             </FILE>
    //             <FILE>
    //                 <PATH>\Windows\Boot\DVD\EFI\BCD</PATH>
    //                 <OFFSET>0xd0</OFFSET>
    //                 <SIZE>0x6000</SIZE>
    //                 <COMPRESSION>0x0</COMPRESSION>
    //             </FILE>
    //             <FILE>
    //                 <PATH>\Windows\Boot\EFI\bootmgfw.efi</PATH>
    //                 <OFFSET>0x30c0d0</OFFSET>
    //                 <SIZE>0x218f48</SIZE>
    //                 <COMPRESSION>0x0</COMPRESSION>
    //             </FILE>
    //         </RESOURCES>
    //     </IMAGE>
    // </WIM>
    //

    //
    // FIXME: Add sanity checks to XML parsing (e.g. check if there are multiple entries of the same
    // file)
    //

    FileNode = FindFirstChildNodeByName(Context->ResourcesNode, t("FILE"));

    while (FileNode != NULL) {
        //
        // Look for <PATH> node
        //

        PathNode = FindFirstChildNodeByName(FileNode, t("PATH"));
        if (PathNode == NULL) {
            DBG_ERROR("<PATH> node not found, invalid XML");
            Status = EFI_INVALID_PARAMETER;
            goto Exit;
        } else {
            //
            // Try to match <PATH> value against FilePath
            //
            const CHAR8* Path = PathNode->Value;
            if (AsciiStrnCmp(Path, FilePath, FilePathLength) == 0) {
                //
                // Found a match! Now extract file offset and file size
                //

                FileOffsetNode = FindFirstChildNodeByName(FileNode, t("OFFSET"));
                if (FileOffsetNode == NULL) {
                    DBG_ERROR("<OFFSET> node not found, invalid XML");
                    Status = EFI_INVALID_PARAMETER;
                    goto Exit;
                }

                FileSizeNode = FindFirstChildNodeByName(FileNode, t("SIZE"));
                if (FileSizeNode == NULL) {
                    DBG_ERROR("<SIZE> node not found, invalid XML");
                    Status = EFI_INVALID_PARAMETER;
                    goto Exit;
                }

                //
                // Convert hex strings to integers we can use
                //

                FileOffset = AsciiStrHexToUintn(FileOffsetNode->Value);
                FileSize = AsciiStrHexToUintn(FileSizeNode->Value);

                //
                // Read from WIM file and
                // write to the destination file.
                //
                Status = FileDuplicate(Context->WimFile,
                                       FileOffset,
                                       FileSize,
                                       DestinationPartitionName,
                                       DestinationFilePath,
                                       &DestinationFile);
                if (EFI_ERROR(Status)) {
                    DBG_ERROR("FileDuplicate() failed with status 0x%zx", Status);
                    goto Exit;
                }

                DBG_INFO("Successfully read file %s from WIM", Path);
                Status = EFI_SUCCESS;
                goto Exit;
            }
        }

        //
        // Current node didn't match, try next node.
        //

        FileNode = FindNextChildNodeByName(Context->ResourcesNode, FileNode, t("FILE"));
    }

Exit:

    if (DestinationFile != NULL) {
        FileClose(DestinationFile);
    }

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_WIM_EXTRACTION_FAILED);
    }

    return Status;
}

EFI_STATUS EFIAPI WimExtractCbmrNode(_In_ WIM_CONTEXT* Context, _Outptr_ XmlNode2** CbmrNode)
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (Context == NULL || CbmrNode == NULL) {
        DBG_ERROR("Invalid parameter");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (!Context->Initialized) {
        DBG_ERROR("Context is not initialized");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    if (Context->CbmrNode == NULL) {
        DBG_ERROR("<CBMR> node was not found during XML deserialization");
        Status = EFI_UNSUPPORTED;
        goto Exit;
    }

    *CbmrNode = Context->CbmrNode;

Exit:

    return Status;
}

#ifndef UEFI_BUILD_SYSTEM
#pragma prefast(pop)
#endif
