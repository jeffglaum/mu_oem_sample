#ifndef _EDK2COMPAT_UEFIGPT_H_
#define _EDK2COMPAT_UEFIGPT_H_

// MdePkg\Include\Uefi\UefiGpt.h

#pragma pack(1)

typedef struct {
    EFI_TABLE_HEADER Header;
    EFI_LBA MyLBA;
    EFI_LBA AlternateLBA;
    EFI_LBA FirstUsableLBA;
    EFI_LBA LastUsableLBA;
    EFI_GUID DiskGUID;
    EFI_LBA PartitionEntryLBA;
    UINT32 NumberOfPartitionEntries;
    UINT32 SizeOfPartitionEntry;
    UINT32 PartitionEntryArrayCRC32;
} EFI_PARTITION_TABLE_HEADER;

#pragma pack()

#endif // _EDK2COMPAT_UEFIGPT_H_
