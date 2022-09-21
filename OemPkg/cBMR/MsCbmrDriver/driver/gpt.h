#ifndef _GPT_H_
#define _GPT_H_

#define PROTECTIVE_MBR_SECTOR 0

EFI_GUID gsBasicDataPartitionGuid =
    {0xEBD0A0A2, 0xB9E5, 0x4433, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7};

#pragma pack(push, 1)
typedef struct _MBR_PARTITION_RECORD2 {
    UINT8 BootIndicator;
    UINT8 StartingCHS[3];
    UINT8 OSType;
    UINT8 EndingCHS[3];
    UINT32 StartingLBA;
    UINT32 SizeInLBA;
} MBR_PARTITION_RECORD2, *PMBR_PARTITION_RECORD2;

// Master boot record
typedef struct _MBR_HEADER {
    UINT8 ArrBootstrap[446];
    MBR_PARTITION_RECORD2 PartionRecord1;
    MBR_PARTITION_RECORD2 PartionRecord2;
    MBR_PARTITION_RECORD2 PartionRecord3;
    MBR_PARTITION_RECORD2 PartionRecord4;
    UINT16 Signature;
} MBR_HEADER, *PMBR_HEADER;

// GPT header
typedef struct _GPT_HEADER {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 Crc32;
    UINT8 ArrReserved1[4];
    UINT64 MyLba;
    UINT64 BackupLba;
    UINT64 FirstLba;
    UINT64 LastLba;
    EFI_GUID DiskGuid;
    UINT64 PartitionEntriesLba;
    UINT32 NumberOfPartitionsEntries;
    UINT32 SizeOfPartitionEntry;
    UINT32 PartitionEntriesCrc32;
    UINT8 ArrReserved2[420];
} GPT_HEADER, *PGPT_HEADER;

// GPT partition entry
typedef struct _GUID_PARTITION_ENTRY {
    EFI_GUID PartitionTypeGuid;
    EFI_GUID UniquePartitionGuid;
    UINT64 FirstLba;
    UINT64 LastLba;
    UINT64 AttributeFlags;
    CHAR16 ArrPartitionName[36];
} GUID_PARTITION_ENTRY, *PGUID_PARTITION_ENTRY;

// MBR + GPT
typedef struct _MBR_GPT {
    MBR_HEADER MbrHeader;
    GPT_HEADER GptHeader;
    GUID_PARTITION_ENTRY PartitionEntry[128];
} MBR_GPT, *PMBR_GPT;
#pragma pack(pop)

typedef struct _RW_PARTITION_ACCESS_LIST {
    CHAR16* PartitionName; // Partition name
    UINT64 StartSector;    // Start sector
    UINT64 LastSector;     // Last sector
    BOOLEAN Closed;        // Is partiton closed
} RW_PARTITION_ACCESS_LIST, *PRW_PARTITION_ACCESS_LIST;

#endif // _GPT_H_
