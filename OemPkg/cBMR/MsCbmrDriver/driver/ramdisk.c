//
// Global includes
//
#include "cbmrincludes.h"

//
// Local includes
//
#include "ramdisk.h"
#include "file.h"
#include "gpt.h"
#include "error.h"

#ifndef UEFI_BUILD_SYSTEM
#pragma prefast(push)
#pragma prefast(disable : 6101, \
                "False positive - _Out_ parameter is always initialized upon success")
#endif

//
// Constants/Macros
//

#define OEMTEXT            "MSDOS5.0"
#define OEMTEXT_LENGTH     8
#define VOLUMELABEL        "STUBOS"
#define VOLUMELABEL_LENGTH 6
#define TWO_MEGABYTES      (2 * 1024 * 1024)

static EFI_GUID RamdiskDiskGuid =
    {0x7c7c7fda, 0x200e, 0x4074, 0x93, 0x8f, 0xc4, 0x00, 0xbd, 0x26, 0x67, 0xc3};
static EFI_GUID RamdiskPartitionEntryGuid =
    {0x1fac5d39, 0xfea3, 0x4669, 0xa9, 0x7c, 0x31, 0x37, 0x68, 0xd1, 0xd7, 0x2a};

//
// Structs
//
typedef struct _RAMDISK_CONTEXT {
    BOOLEAN Initialized;

    // Ramdisk info
    EFI_PHYSICAL_ADDRESS Buffer;
    UINTN BufferSize;
    UINT32 SectorSize;

    // Physical memory
    UINTN NumPages;
    EFI_PHYSICAL_ADDRESS BaseAddress;
    EFI_PHYSICAL_ADDRESS BaseAddress2MBAligned;

    // Registration info
    BOOLEAN Registered;
    EFI_DEVICE_PATH_PROTOCOL* DevicePath;
    CHAR16* DevicePathString;

    // Simple File System
    EFI_DEVICE_PATH_PROTOCOL* SfsDevicePath;
    EFI_FILE_PROTOCOL* SystemVolume;

    // Loaded info
    BOOLEAN Loaded;
    EFI_DEVICE_PATH_PROTOCOL* RamdiskAndFilePathDevicePath;
} RAMDISK_CONTEXT, *PRAMDISK_CONTEXT;

typedef enum {
    ATTR_READ_ONLY = 0x01,
    ATTR_HIDDEN = 0x02,
    ATTR_SYSTEM = 0x04,
    ATTR_VOLUME_ID = 0x08,
    ATTR_DIRECTORY = 0x10,
    ATTR_LONG_NAME = 0x0F // ATTR_READ_ONLY | ATTR_HIDDEN |  ATTR_SYSTEM | ATTR_VOLUME_ID
} DIR_ATTR;

#pragma pack(push, 1)
typedef struct _PACKED_BIOS_PARAMETER_BLOCK_EX {
    UINT16 BytesPerSector;        // offset = 0x000  0
    UINT8 SectorsPerCluster;      // offset = 0x002  2
    UINT16 ReservedSectors;       // offset = 0x003  3
    UINT8 Fats;                   // offset = 0x005  5
    UINT16 RootEntries;           // offset = 0x006  6
    UINT16 Sectors;               // offset = 0x008  8
    UINT8 Media;                  // offset = 0x00A 10
    UINT16 SectorsPerFat;         // offset = 0x00B 11
    UINT16 SectorsPerTrack;       // offset = 0x00D 13
    UINT16 Heads;                 // offset = 0x00F 15
    UINT32 HiddenSectors;         // offset = 0x011 17
    UINT32 LargeSectors;          // offset = 0x015 21
    UINT32 LargeSectorsPerFat;    // offset = 0x019 25
    UINT16 ExtendedFlags;         // offset = 0x01D 29
    UINT16 FsVersion;             // offset = 0x01F 31
    UINT32 RootDirFirstCluster;   // offset = 0x021 33
    UINT16 FsInfoSector;          // offset = 0x025 37
    UINT16 BackupBootSector;      // offset = 0x027 39
    UINT8 Reserved[12];           // offset = 0x029 41
} PACKED_BIOS_PARAMETER_BLOCK_EX; // sizeof = 0x035 53

typedef struct _PACKED_BOOT_SECTOR_EX {
    UINT8 Jump[3];                            // offset = 0x000   0
    CHAR8 Oem[8];                             // offset = 0x003   3
    PACKED_BIOS_PARAMETER_BLOCK_EX PackedBpb; // offset = 0x00B  11
    UINT8 PhysicalDriveNumber;                // offset = 0x040  64
    UINT8 CurrentHead;                        // offset = 0x041  65
    UINT8 Signature;                          // offset = 0x042  66
    UINT32 Id;                                // offset = 0x043  67
    CHAR8 VolumeLabel[11];                    // offset = 0x047  71
    CHAR8 SystemId[8];                        // offset = 0x058  88
} PACKED_BOOT_SECTOR_EX;                      // sizeof = 0x060  96

typedef PACKED_BOOT_SECTOR_EX* PPACKED_BOOT_SECTOR_EX;

typedef struct {
    DWORD dLeadSig;       // 0x41615252
    BYTE sReserved1[480]; // zeros
    DWORD dStrucSig;      // 0x61417272
    DWORD dFree_Count;    // 0xFFFFFFFF
    DWORD dNxt_Free;      // 0xFFFFFFFF
    BYTE sReserved2[12];  // zeros
    DWORD dTrailSig;      // 0xAA550000
} FAT_FSINFO;

typedef struct {
    BYTE Name[11];
    BYTE Attr;
    BYTE NTRes;
    BYTE CrtTimeTenth;
    WORD CrtTime;
    WORD CrtDate;
    WORD LstAccDate;
    WORD FstClusHI;
    WORD WrtTime;
    WORD WrtDate;
    WORD FstClusLO;
    DWORD FileSize;
} DIR_ENTRY;
#pragma pack(pop)

//
// Variables
//
static EFI_RAM_DISK_PROTOCOL* gspRamDiskProtocol = NULL;

//
// Prototypes
//
static EFI_STATUS EFIAPI RamdiskLocateProtocol(VOID);
static DWORD GetFATSizeSectors(DWORD DskSize,
                               DWORD ReservedSecCnt,
                               DWORD SecPerClus,
                               DWORD NumFATs,
                               DWORD BytesPerSect);
static DWORD GetVolumeID(void);

//
// Interfaces
//
EFI_STATUS EFIAPI RamdiskInit(_In_ UINTN RamdiskSize,
                              _In_ UINT32 SectorSize,
                              _Outptr_ RAMDISK_CONTEXT** RamdiskContext)
{
    EFI_STATUS Status = EFI_SUCCESS;
    RAMDISK_CONTEXT* RetRamdiskContext = NULL;

    if (RamdiskContext == NULL) {
        DBG_ERROR("RamdiskContext is NULL");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    // Make sure EFI_RAM_DISK_PROTOCOL is available, otherwise any work we do here is for
    // nothing.
    Status = RamdiskLocateProtocol();
    if (EFI_ERROR(Status)) {
        DBG_ERROR("RamdiskLocateProtocol failed with error 0x%zx", Status);
        goto Exit;
    }

    // Initialize ramdisk context
    RetRamdiskContext = AllocateZeroPool(sizeof(RAMDISK_CONTEXT));
    if (RetRamdiskContext == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        DBG_ERROR("Failed to allocate ramdisk context");
        goto Exit;
    }

    // Windows persistent memory stack requirement (pmem.sys):
    // Ramdisk size needs to align to 2MB boundary.
    UINTN NumSlabs = RamdiskSize / TWO_MEGABYTES;
    DBG_INFO("Num slabs %zd", NumSlabs);

    UINTN SizeFloor = NumSlabs * TWO_MEGABYTES;
    UINTN ModifiedSize = (RamdiskSize > SizeFloor) ? SizeFloor + TWO_MEGABYTES : SizeFloor;
    DBG_INFO("Ramdisk Size %zd", ModifiedSize);

    // Add an extra 2MB padding to give room to align BaseAddress returned by AllocatePages to
    // 2MB boundary.
    UINTN PaddedSize = ModifiedSize + TWO_MEGABYTES;
    DBG_INFO("Padded size %zd", PaddedSize);

    // Calculate number of pages needed.
    UINTN NumPages = PaddedSize / 4096;

    DBG_INFO("Number of pages (2MB aligned) %zd", NumPages);

    // Allocate pages for ramdisk.
    RetRamdiskContext->NumPages = NumPages;
    Status = gBS->AllocatePages(AllocateAnyPages,
                                EfiReservedMemoryType,
                                RetRamdiskContext->NumPages,
                                &RetRamdiskContext->BaseAddress);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("AllocatePages failed with error 0x%zx", Status);
        goto Exit;
    }

    DBG_INFO("Default BaseAddress: %llu", RetRamdiskContext->BaseAddress);

    // The following shifts the base address to a 2MB boundary so that we can later call
    // EFI_RAM_DISK_PROTOCOL->Register() with 2MB aligned starting offset. This is possible
    // because we added 2MB padding prior to calling AllocatePages above. This means there might
    // be up to 2MB - 4KB (natural page size in UEFI) of unused memory prior to ramdisk starting
    // offset. See below for visual illustration:
    //
    // 000000000000XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
    // <-------------------- NumPages allocated pages----------------->
    // <- Unused ->^
    // ^           |
    // |           |-> 2MB aligned address (BaseAddress2MBAligned)
    // |-> EFI_PHYSICAL_ADDRESS returned by gBS->AllocatePages

    RetRamdiskContext->BaseAddress2MBAligned = (EFI_PHYSICAL_ADDRESS)
        ALIGN_UP_BY(RetRamdiskContext->BaseAddress, TWO_MEGABYTES);
    DBG_INFO("2MB-aligned BaseAddress: %llu", RetRamdiskContext->BaseAddress2MBAligned);

    RetRamdiskContext->Buffer = RetRamdiskContext->BaseAddress2MBAligned;
    RetRamdiskContext->BufferSize = ModifiedSize;
    RetRamdiskContext->SectorSize = SectorSize;

    RetRamdiskContext->Initialized = TRUE;

    *RamdiskContext = (VOID*)RetRamdiskContext;
    RetRamdiskContext = NULL;

Exit:
    if (RetRamdiskContext) {
        if (RetRamdiskContext->BaseAddress) {
            gBS->FreePages(RetRamdiskContext->BaseAddress, RetRamdiskContext->NumPages);
        }

        FreePool(RetRamdiskContext);
    }

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_RAMDISK_INITIALIZATION_FAILED);
    }

    return Status;
}

EFI_STATUS EFIAPI RamdiskFree(_Inout_ RAMDISK_CONTEXT* RamdiskContext)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_STATUS EfiStatusTemp;

    if (RamdiskContext == NULL) {
        DBG_ERROR("RetRamdiskContext is NULL");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (!RamdiskContext->Initialized) {
        DBG_ERROR("Ramdisk has not been initialized");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    if (RamdiskContext->Registered) {
        EfiStatusTemp = RamdiskUnregister(RamdiskContext);
        if (EFI_ERROR(EfiStatusTemp)) {
            DBG_WARNING("Ramdisk unregister failed with error 0x%zx", EfiStatusTemp);
            if (!EFI_ERROR(Status)) {
                Status = EfiStatusTemp;
            }
        }

        FreePool(RamdiskContext->DevicePathString);
    }

    gBS->FreePages(RamdiskContext->BaseAddress, RamdiskContext->NumPages);

    FreePool(RamdiskContext);

Exit:
    return Status;
}

EFI_STATUS EFIAPI RamdiskRegister(_Inout_ RAMDISK_CONTEXT* RamdiskContext)
{
    EFI_STATUS Status = EFI_SUCCESS;

    EFI_DEVICE_PATH_TO_TEXT_PROTOCOL* DevicePathToTextIf = NULL;

    if (RamdiskContext == NULL) {
        DBG_ERROR("RetRamdiskContext is NULL");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (!RamdiskContext->Initialized) {
        DBG_ERROR("Ramdisk has not been initialized");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    Status = gspRamDiskProtocol->Register(RamdiskContext->Buffer,
                                          RamdiskContext->BufferSize,
                                          &gEfiVirtualDiskGuid,
                                          NULL,
                                          &RamdiskContext->DevicePath);

    if (EFI_ERROR(Status)) {
        DBG_ERROR("Ramdisk registration failed with error 0x%zx", Status);
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

    CHAR16* RamdiskDevicePath = DevicePathToTextIf->ConvertDevicePathToText(
        RamdiskContext->DevicePath,
        FALSE,
        FALSE);

    if (RamdiskDevicePath == NULL) {
        DBG_ERROR("ConvertDevicePathToText() returned NULL string");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    DBG_INFO_U(L"%s", RamdiskDevicePath);

    RamdiskContext->DevicePathString = RamdiskDevicePath;
    RamdiskContext->Registered = TRUE;

Exit:
    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_RAMDISK_REGISTRATION_FAILED);
    }

    return Status;
}

EFI_STATUS EFIAPI RamdiskUnregister(_Inout_ RAMDISK_CONTEXT* RamdiskContext)
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (RamdiskContext == NULL) {
        DBG_ERROR("RetRamdiskContext is NULL");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (!RamdiskContext->Initialized) {
        DBG_ERROR("Ramdisk has not been initialized");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    if (!RamdiskContext->Registered) {
        DBG_ERROR("Ramdisk has not been registered");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    Status = gspRamDiskProtocol->Unregister(RamdiskContext->DevicePath);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Ramdisk unregistration failed with error 0x%zx", Status);
    }

Exit:
    return Status;
}

EFI_STATUS EFIAPI RamdiskRead(_In_ RAMDISK_CONTEXT* RamdiskContext,
                              _In_ UINTN Offset,
                              _In_ UINTN Length,
                              _Out_writes_bytes_(BufferLength) UINT8* Buffer,
                              _In_ UINTN BufferLength)
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (RamdiskContext == NULL) {
        DBG_ERROR("RetRamdiskContext is NULL");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (Buffer == NULL) {
        DBG_ERROR("Buffer is NULL");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (Length == 0) {
        DBG_ERROR("Length is 0");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (!RamdiskContext->Initialized) {
        DBG_ERROR("Ramdisk has not been initialized");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    if (Offset + Length > RamdiskContext->BufferSize) {
        DBG_ERROR("Read exceeds ramdisk size. Offset (%zd) + Length (%zd) > Ramdisk size (%zd)",
                  Offset,
                  Length,
                  RamdiskContext->BufferSize);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    Status = CopyMemS(Buffer, BufferLength, (UINT8*)(UINTN)RamdiskContext->Buffer + Offset, Length);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CopyMemS() failed 0x%zx", Status);
        goto Exit;
    }

Exit:
    return Status;
}

EFI_STATUS EFIAPI RamdiskWrite(_Inout_ RAMDISK_CONTEXT* RamdiskContext,
                               _In_ UINTN Offset,
                               _In_ UINTN Length,
                               _In_ UINT8* Data)
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINTN FinalOffset = 0;

    if (RamdiskContext == NULL) {
        DBG_ERROR("RetRamdiskContext is NULL");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (Data == NULL) {
        DBG_ERROR("Data is NULL");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (Length == 0) {
        DBG_ERROR("Length is 0");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (!RamdiskContext->Initialized) {
        DBG_ERROR("Ramdisk has not been initialized");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    Status = UintnAdd(Offset, Length, &FinalOffset);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("UintnAdd() failed 0x%zx", Status);
        goto Exit;
    }

    if (FinalOffset > RamdiskContext->BufferSize) {
        DBG_ERROR("Write exceeds ramdisk size. Offset (%zd) + Length (%zd) > Ramdisk size (%zd)",
                  Offset,
                  Length,
                  RamdiskContext->BufferSize);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    Status = CopyMemS((UINT8*)(UINTN)RamdiskContext->Buffer + Offset,
                      RamdiskContext->BufferSize - Offset,
                      Data,
                      Length);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CopyMemS() failed 0x%zx", Status);
        goto Exit;
    }

Exit:
    return Status;
}

EFI_STATUS EFIAPI RamdiskBoot(_In_ RAMDISK_CONTEXT* RamdiskContext)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_HANDLE* Handles = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* SimpleFs = NULL;
    EFI_FILE_PROTOCOL* SystemVolume = NULL;
    EFI_FILE_PROTOCOL* EfiFileProtocol = NULL;
    EFI_DEVICE_PATH_PROTOCOL* DevicePathIf = NULL;
    EFI_DEVICE_PATH_PROTOCOL* FilePathDevicePath = NULL;
    UINTN FilePathDevicePathSize = 0;
    EFI_DEVICE_PATH_TO_TEXT_PROTOCOL* DevicePathToTextIf = NULL;
    EFI_DEVICE_PATH_UTILITIES_PROTOCOL* DevicePathUtilitiesIf = NULL;
    CHAR16* CombinedDevicePath = NULL;
    EFI_HANDLE BootmgrHandle = NULL;
    UINTN HandleCount = 0;

    if (RamdiskContext == NULL) {
        DBG_ERROR("RetRamdiskContext is NULL");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    if (!RamdiskContext->Initialized) {
        DBG_ERROR("Ramdisk has not been initialized");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    if (!RamdiskContext->Registered) {
        DBG_ERROR("Ramdisk has not been registered");
        Status = EFI_NOT_READY;
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

    Status = gBS->LocateProtocol(&gEfiDevicePathUtilitiesProtocolGuid,
                                 NULL,
                                 (PVOID*)&DevicePathUtilitiesIf);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("LocateProtocol() for DevicePathUtilities protocol failed with status 0x%zx",
                  Status);
        goto Exit;
    }

    // Get all handles to existing simple filesystem protocol instances
    Status = gBS->LocateHandleBuffer(ByProtocol,
                                     &gEfiSimpleFileSystemProtocolGuid,
                                     NULL,
                                     &HandleCount,
                                     &Handles);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Unable to get Handles for simple filesystem protocols: 0x%zx", Status);
        goto Exit;
    }

    // Grab handle that matches Ramdisk device path
    DBG_INFO_U(L"Ramdisk device path:%s", RamdiskContext->DevicePathString);

    for (UINTN i = 0; i < HandleCount; i++) {
        DevicePathIf = DevicePathFromHandle(Handles[i]);
        if (DevicePathIf == NULL) {
            DBG_INFO("DevicePathFromHandle() returned NULL pointer for handle 0x%p", Handles[i]);
            continue;
        }

        // Print device path
        CHAR16* DevicePath = DevicePathToTextIf->ConvertDevicePathToText(DevicePathIf,
                                                                         FALSE,
                                                                         FALSE);

        if (DevicePath == NULL) {
            DBG_INFO("ConvertDevicePathToText() returned NULL pointer for handle 0x%p", Handles[i]);
            continue;
        }

        DBG_INFO_U(L"Handle[%zd]: DevicePath: %s", i, DevicePath);

        // Compare device path. If matched, grab SFS protocol
        if (StrniCmp(RamdiskContext->DevicePathString,
                     DevicePath,
                     StrLen(RamdiskContext->DevicePathString)) == 0) {
            DBG_INFO("Matched device path!");
            RamdiskContext->SfsDevicePath = DevicePathIf;

            FreePool(DevicePath);
            DevicePath = NULL;

            Status = gBS->HandleProtocol(Handles[i],
                                         &gEfiSimpleFileSystemProtocolGuid,
                                         (VOID**)&SimpleFs);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("Failed to get SimpleFileSystem protocol from device handle, error 0x%zx",
                          Status);
                goto Exit;
            }

            break;
        }

        FreePool(DevicePath);
        DevicePath = NULL;
    }

    if (SimpleFs == NULL) {
        DBG_ERROR("Unable to find Simple File System for ramdisk");
        goto Exit;
    }

    // Open system volume
    Status = SimpleFs->OpenVolume(SimpleFs, &SystemVolume);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Failed to open volume, error 0x%zx", Status);
        goto Exit;
    }

    // Find bootmgr via SimpleFileSystemProtocol
    Status = SystemVolume->Open(SystemVolume,
                                &EfiFileProtocol,
                                BOOTMGR_PATH,
                                EFI_FILE_MODE_READ,
                                0);
    if (EFI_ERROR(Status)) {
        DBG_ERROR_U(L"Failed to create file handle to %s, error 0x%zx", BOOTMGR_PATH, Status);
        goto Exit;
    }

    // Generate file path device path
    UINTN DevicePathNodeSize = 0;
    EFI_DEVICE_PATH* DevicePathEndNode;

    DevicePathNodeSize = _countof(BOOTMGR_PATH);
    DevicePathNodeSize = sizeof(CHAR16) * (DevicePathNodeSize + 1) + sizeof(EFI_DEVICE_PATH);
    FilePathDevicePathSize = DevicePathNodeSize + sizeof(EFI_DEVICE_PATH);

    FilePathDevicePath = AllocateZeroPool(FilePathDevicePathSize);
    if (FilePathDevicePath == NULL) {
        DBG_ERROR("Unable to allocate memory for device path");
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    // Fill out device path information
    FilePathDevicePath->Type = MEDIA_DEVICE_PATH;
    FilePathDevicePath->SubType = MEDIA_FILEPATH_DP;
    FilePathDevicePath->Length[0] = (UINT8)DevicePathNodeSize;
    FilePathDevicePath->Length[1] = (UINT8)(DevicePathNodeSize >> 8);

    Status = CopyMemS((PUINT8)FilePathDevicePath + sizeof(EFI_DEVICE_PATH),
                      FilePathDevicePathSize - sizeof(EFI_DEVICE_PATH),
                      BOOTMGR_PATH,
                      sizeof(CHAR16) * (_countof(BOOTMGR_PATH) + 1));
    DevicePathEndNode = (EFI_DEVICE_PATH*)((PUINT8)FilePathDevicePath + DevicePathNodeSize);
    DevicePathEndNode->Type = END_DEVICE_PATH_TYPE;
    DevicePathEndNode->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
    DevicePathEndNode->Length[0] = sizeof(EFI_DEVICE_PATH);
    DevicePathEndNode->Length[1] = 0;

    // Append ramdisk device path + file path device path
    RamdiskContext->RamdiskAndFilePathDevicePath = DevicePathUtilitiesIf->AppendDevicePath(
        (EFI_DEVICE_PATH_PROTOCOL*)RamdiskContext->SfsDevicePath,
        (EFI_DEVICE_PATH_PROTOCOL*)FilePathDevicePath);

    FreePool(FilePathDevicePath);

    // Print out combined device path
    CombinedDevicePath = DevicePathToTextIf->ConvertDevicePathToText(
        RamdiskContext->RamdiskAndFilePathDevicePath,
        FALSE,
        FALSE);
    DBG_INFO_U(L"Combined device path: %s", CombinedDevicePath);

    FreePool(CombinedDevicePath);
    CombinedDevicePath = NULL;

    Status = gBS->LoadImage(FALSE,
                            gImageHandle,
                            RamdiskContext->RamdiskAndFilePathDevicePath,
                            NULL,
                            0,
                            &BootmgrHandle);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Failed to load bootmgr image, error 0x%zx", Status);
        goto Exit;
    }

    DBG_INFO(
        "About to ram boot. closing the debug module, no more prints here after from CBMR driver!");
    DebugClose(); // To flush debug log file buffers

    Status = gBS->StartImage(BootmgrHandle, 0, NULL);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Failed to start bootmgr, error 0x%zx", Status);
        goto Exit;
    }

Exit:

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_RAMDISK_BOOT_FAILED);
    }

    return Status;
}

EFI_STATUS EFIAPI RamdiskGetSectorCount(_In_ const RAMDISK_CONTEXT* RamdiskContext,
                                        _Out_ UINT32* SectorCount)
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (RamdiskContext == NULL) {
        DBG_ERROR("RetRamdiskContext is NULL");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    if (!RamdiskContext->Initialized) {
        DBG_ERROR("Ramdisk not yet initialized");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    if (SectorCount == NULL) {
        DBG_ERROR("SectorCount is NULL");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    *SectorCount = (UINT32)(RamdiskContext->BufferSize / RamdiskContext->SectorSize);

Exit:
    return Status;
}

EFI_STATUS EFIAPI RamdiskGetSectorSize(_In_ const RAMDISK_CONTEXT* RamdiskContext,
                                       _Out_ UINT32* SectorSize)
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (RamdiskContext == NULL) {
        DBG_ERROR("RamdiskContext is NULL");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    if (!RamdiskContext->Initialized) {
        DBG_ERROR("Ramdisk not yet initialized");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    if (SectorSize == NULL) {
        DBG_ERROR("SectorSize is NULL");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    *SectorSize = RamdiskContext->SectorSize;

Exit:
    return Status;
}

EFI_STATUS EFIAPI RamdiskInitializeSingleFat32Volume(_Inout_ RAMDISK_CONTEXT* RamdiskContext)
{
    EFI_STATUS Status = EFI_SUCCESS;
    MBR_GPT* MbrGpt = NULL;
    UINT8 StartingChs[3] = {0x00, 0x02, 0x00};
    UINT8 EndingChs[3] = {0xFF, 0xFF, 0xFF};
    UINT32 SectorSize = 0;
    UINT32 SectorCount = 0;
    CHAR16* PartitionName = L"STUBOS";
    UINT32 CalculatedCrc = 0;

    //
    // FAT32 related values
    //

    UINT32 TotalSectors = 0;
    UINTN VolumeOffset = 0;
    DWORD ReservedSectCount = 32;
    DWORD NumFATs = 2;
    DWORD BackupBootSect = 6;
    DWORD VolumeId = 0;
    CHAR8 VolId[12] = "NO NAME    ";
    DWORD BurstSize = 128; // Zero in blocks of 64K typically
    PACKED_BOOT_SECTOR_EX* BootSector = NULL;
    FAT_FSINFO* pFAT32FsInfo = NULL;
    DWORD* pFirstSectOfFat = NULL;
    BYTE* pZeroSect = NULL;
    UINT32 FirstDataSector = 0;
    DIR_ENTRY* VolumeLabelEntry = NULL;
    UINT32 ClusterSize = 0;
    UINT32 SectorsPerCluster = 0;

    //
    // Calculated later
    //

    UINT32 SectorStart = 0;
    UINT32 FatSize = 0;
    DWORD SystemAreaSize = 0;
    DWORD UserAreaSize = 0;
    ULONGLONG FatNeeded, ClusterCount;

    if (RamdiskContext == NULL) {
        DBG_ERROR("RamdiskContext is NULL");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    if (!RamdiskContext->Initialized) {
        DBG_ERROR("Ramdisk not yet initialized");
        Status = EFI_NOT_READY;
        goto Exit;
    }

    Status = RamdiskGetSectorSize(RamdiskContext, &SectorSize);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("RamdiskGetSectorSize() failed 0x%zx", Status);
        goto Exit;
    }

    if (SectorSize != 512) {
        DBG_ERROR("Invalid sector size %d", SectorSize);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    Status = RamdiskGetSectorCount(RamdiskContext, &SectorCount);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("RamdiskGetSectorCount() failed 0x%zx", Status);
        goto Exit;
    }

    //
    // Initalize protective MBR
    //

    MbrGpt = AllocateZeroPool(sizeof(MBR_GPT));
    if (MbrGpt == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    MbrGpt->MbrHeader.PartionRecord1.BootIndicator = 0x00;
    Status = CopyMemS(&MbrGpt->MbrHeader.PartionRecord1.StartingCHS, 3, &StartingChs, 3);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CopyMemS() failed 0x%zx", Status);
        goto Exit;
    }

    MbrGpt->MbrHeader.PartionRecord1.OSType = 0xEE;
    Status = CopyMemS(&MbrGpt->MbrHeader.PartionRecord1.EndingCHS, 3, &EndingChs, 3);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CopyMemS() failed 0x%zx", Status);
        goto Exit;
    }

    MbrGpt->MbrHeader.PartionRecord1.StartingLBA = 0x1;
    MbrGpt->MbrHeader.PartionRecord1.SizeInLBA = 0xFFFFFFFF;
    MbrGpt->MbrHeader.Signature = 0xAA55;

    //
    // Initialize GPT
    //

    MbrGpt->GptHeader.Signature = 0x5452415020494645; // "EFI PART"
    MbrGpt->GptHeader.Revision = 0x00010000;
    MbrGpt->GptHeader.HeaderSize = 0X5C;
    MbrGpt->GptHeader.Crc32 = 0x00; // This gets calculated later.
    // MbrGpt->GptHeader.ArrReserved1[4];
    MbrGpt->GptHeader.MyLba = 0x1;

    //
    // Below values are explicitly for 512 sector size disks. 4k is not
    // currently supported (not too critical since EFI_RAM_DISK_PROTOCOL
    // doesn't currently support it either).
    //

    MbrGpt->GptHeader.BackupLba = SectorCount - 1;
    MbrGpt->GptHeader.FirstLba = 0x22; // 1(Protective MBR) + 33(GPT Header + Partition entries)
    MbrGpt->GptHeader.LastLba = SectorCount - 33 - 1;
    MbrGpt->GptHeader.DiskGuid = RamdiskDiskGuid;
    MbrGpt->GptHeader.PartitionEntriesLba = 0x2;
    MbrGpt->GptHeader.NumberOfPartitionsEntries = 128;
    MbrGpt->GptHeader.SizeOfPartitionEntry = 0x80;

    //
    // Initialize single partition entry
    //

    MbrGpt->PartitionEntry[0].PartitionTypeGuid = gsBasicDataPartitionGuid;
    MbrGpt->PartitionEntry[0].UniquePartitionGuid = RamdiskPartitionEntryGuid;
    MbrGpt->PartitionEntry[0].FirstLba = 0x22;

    //
    // Unorthodox, but lets make partition entire length as disk.
    // Simplifies things for us.
    //

    MbrGpt->PartitionEntry[0].LastLba = MbrGpt->GptHeader.LastLba;
    MbrGpt->PartitionEntry[0].AttributeFlags;

    Status = CopyMemS(&MbrGpt->PartitionEntry[0].ArrPartitionName,
                      sizeof(MbrGpt->PartitionEntry[0].ArrPartitionName),
                      PartitionName,
                      StrnLenS(PartitionName, MAX_PARTITION_NAME_LENGTH) * sizeof(CHAR16));
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CopyMemS() failed 0x%zx", Status);
        goto Exit;
    }

    //
    // Calculate MbrGpt->GptHeader.PartitionEntriesCrc32
    //

    Status = gBS->CalculateCrc32((UINT8*)&MbrGpt->PartitionEntry,
                                 MbrGpt->GptHeader.NumberOfPartitionsEntries *
                                     MbrGpt->GptHeader.SizeOfPartitionEntry,
                                 &CalculatedCrc);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CalculateCrc32() failed 0x%zx", Status);
        goto Exit;
    }

    MbrGpt->GptHeader.PartitionEntriesCrc32 = CalculatedCrc;

    //
    // Calculate GPT header CRC since header has been populated.
    //

    Status = gBS->CalculateCrc32((UINT8*)&MbrGpt->GptHeader,
                                 MbrGpt->GptHeader.HeaderSize,
                                 &CalculatedCrc);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CalculateCrc32() failed 0x%zx", Status);
        goto Exit;
    }

    MbrGpt->GptHeader.Crc32 = CalculatedCrc;

    //
    // Copy entire MBR_GPT header into ramdisk at offset 0.
    //

    Status = RamdiskWrite(RamdiskContext, 0, sizeof(MBR_GPT), (UINT8*)MbrGpt);
    if (EFI_ERROR(Status)) {
        goto Exit;
    }

    VolumeOffset = (UINTN)MbrGpt->GptHeader.FirstLba * SectorSize;

    // Format it with FAT32 structure. Below structures are written to
    BootSector = AllocateZeroPool(SectorSize);
    pFAT32FsInfo = (FAT_FSINFO*)AllocateZeroPool(SectorSize);
    pFirstSectOfFat = (DWORD*)AllocateZeroPool(SectorSize);

    if (BootSector == NULL || pFAT32FsInfo == NULL || pFirstSectOfFat == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    // A FAT file system volume is composed of four basic regions,
    // which are laid out in this order on the volume :

    //     0 – Reserved Region
    //     1 – FAT Region
    //     2 – Root Directory Region (doesn’t exist on FAT32 volumes)
    //     3 – File and Directory Data Region

    // Note that the FAT and FAT32 files systems impose the
    // following restrictions on the number of clusters on a volume:

    // FAT: Number of clusters <= 65526
    // FAT32: 65526 < Number of clusters < 4177918

    //
    // The FAT32 BPB exactly matches the FAT12/FAT16 BPB up to and including the BPB_TotSec32 field.
    //

    BootSector->Jump[0] = 0xEB;
    BootSector->Jump[1] = 0x58;
    BootSector->Jump[2] = 0x90;
    Status = CopyMemS(BootSector->Oem, sizeof(BootSector->Oem), OEMTEXT, OEMTEXT_LENGTH);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CopyMemS() failed 0x%zx", Status);
        goto Exit;
    }

    Status = CopyMemS(BootSector->VolumeLabel,
                      sizeof(BootSector->VolumeLabel),
                      VOLUMELABEL,
                      VOLUMELABEL_LENGTH);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CopyMemS() failed 0x%zx", Status);
        goto Exit;
    }

    //
    // Configure the Bios Parameter Block
    //

    TotalSectors = (UINT32)(MbrGpt->GptHeader.LastLba - MbrGpt->GptHeader.FirstLba + 1);

    //
    // 4096 is the default cluster size for 256MB-8GB FAT32 volume sizes, per
    // https://support.microsoft.com/en-us/topic/default-cluster-size-for-ntfs-fat-and-exfat-9772e6f1-e31a-00d7-e18f-73169155af95
    //

    ClusterSize = 4096;
    SectorsPerCluster = ClusterSize / SectorSize;

    BootSector->PackedBpb.BytesPerSector = (UINT16)SectorSize;
    BootSector->PackedBpb.SectorsPerCluster = (UINT8)SectorsPerCluster;
    BootSector->PackedBpb.ReservedSectors = (UINT16)ReservedSectCount;

    //
    // Everywhere says to set this to 2 to provide redundancy in case of failures.
    //

    BootSector->PackedBpb.Fats = (UINT8)NumFATs;

    BootSector->PackedBpb.RootEntries = 0;
    BootSector->PackedBpb.Sectors = 0;
    BootSector->PackedBpb.Media = 0xF8;
    BootSector->PackedBpb.SectorsPerFat = 0;

    // TODO: Not clear what these should be for volume that's on ramdisk geometry.
    BootSector->PackedBpb.SectorsPerTrack = 0x80;
    BootSector->PackedBpb.Heads = 0x10;
    BootSector->PackedBpb.HiddenSectors = 0;

    BootSector->PackedBpb.LargeSectors = TotalSectors;

    //
    // This is where BPB diverges for FAT32
    //

    FatSize = GetFATSizeSectors(TotalSectors,
                                BootSector->PackedBpb.ReservedSectors,
                                SectorsPerCluster,
                                BootSector->PackedBpb.Fats,
                                SectorSize);
    BootSector->PackedBpb.LargeSectorsPerFat = FatSize;

    BootSector->PackedBpb.ExtendedFlags = 0;
    BootSector->PackedBpb.FsVersion = 0;
    BootSector->PackedBpb.RootDirFirstCluster = 2;
    BootSector->PackedBpb.FsInfoSector = 1;
    BootSector->PackedBpb.BackupBootSector = (UINT16)BackupBootSect;

    BootSector->PhysicalDriveNumber = 0x80;
    BootSector->CurrentHead = 0;
    BootSector->Signature = 0x29;

    VolumeId = GetVolumeID();
    BootSector->Id = VolumeId;

    Status = CopyMemS(BootSector->VolumeLabel, sizeof(BootSector->VolumeLabel), VolId, 11);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CopyMemS() failed 0x%zx", Status);
        goto Exit;
    }

    Status = CopyMemS(BootSector->SystemId, sizeof(BootSector->SystemId), "FAT32   ", 8);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CopyMemS() failed 0x%zx", Status);
        goto Exit;
    }

    ((BYTE*)BootSector)[510] = 0x55;
    ((BYTE*)BootSector)[511] = 0xaa;

    //
    // FATGEN103.DOC says "NOTE: Many FAT documents mistakenly say that this 0xAA55 signature
    // occupies the "last 2 bytes of the boot sector". This statement is correct if - and only if -
    // BPB_BytsPerSec is 512. If BPB_BytsPerSec is greater than 512, the offsets of these signature
    // bytes do not change (although it is perfectly OK for the last two bytes at the end of the
    // boot sector to also contain this signature)."
    //
    // Windows seems to only check the bytes at offsets 510 and 511. Other OSs might check the ones
    // at the end of the sector, so we'll put them there too.
    //

    if (SectorSize != 512) {
        ((BYTE*)BootSector)[SectorSize - 2] = 0x55;
        ((BYTE*)BootSector)[SectorSize - 1] = 0xaa;
    }

    //
    // FSInfo sect
    //

    pFAT32FsInfo->dLeadSig = 0x41615252;
    pFAT32FsInfo->dStrucSig = 0x61417272;
    pFAT32FsInfo->dFree_Count = (DWORD)-1;
    pFAT32FsInfo->dNxt_Free = (DWORD)-1;
    pFAT32FsInfo->dTrailSig = 0xaa550000;

    //
    // First FAT Sector
    //

    pFirstSectOfFat[0] = 0x0ffffff8; // Reserved cluster 1 media id in low byte
    pFirstSectOfFat[1] = 0x0fffffff; // Reserved cluster 2 EOC
    pFirstSectOfFat[2] = 0x0fffffff; // end of cluster chain for root dir

    //
    // Copy FAT32 structure to FirstUsableLba, which is where STUBOS partition starts.
    // Write boot sector, FATs
    // Sector 0 Boot Sector
    // Sector 1 FSInfo
    // Sector 2 More boot code - we write zeros here
    // Sector 3 unused
    // Sector 4 unused
    // Sector 5 unused
    // Sector 6 Backup boot sector
    // Sector 7 Backup FSInfo sector
    // Sector 8 Backup 'more boot code'
    // zeroed sectors upto ReservedSectCount
    // FAT1  ReservedSectCount to ReservedSectCount + FatSize
    // ...
    // FATn  ReservedSectCount to ReservedSectCount + FatSize
    // RootDir - allocated to cluster2

    UserAreaSize = TotalSectors - ReservedSectCount - (NumFATs * FatSize);
    ClusterCount = UserAreaSize / SectorsPerCluster;

    //
    // Sanity check for a cluster count of >2^28, since the upper 4 bits of the cluster values in
    // the FAT are reserved.
    //

    if (ClusterCount > 0x0FFFFFFF) {
        DBG_ERROR(
            "This drive has more than 2^28 clusters, try to specify a larger cluster size or use the default");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    //
    // Sanity check - < 64K clusters means that the volume will be misdetected as FAT16
    //

    if (ClusterCount < 65536) {
        DBG_ERROR(
            "FAT32 must have at least 65536 clusters, try to specify a smaller cluster size or use the default");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    //
    // Sanity check, make sure the FAT is big enough.
    // Convert the cluster count into a FAT sector count, and check that the FAT size value we
    // calculated earlier is OK.
    //

    FatNeeded = ClusterCount * 4;
    FatNeeded += (SectorSize - 1);
    FatNeeded /= SectorSize;
    if (FatNeeded > FatSize) {
        DBG_ERROR("This drive is too big for large FAT32 format");
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    DBG_INFO("Ready to format volume");
    DBG_INFO("Volume sector count : %u sectors", TotalSectors);
    DBG_INFO("Cluster size %d bytes, %d bytes per sector",
             SectorsPerCluster * SectorSize,
             SectorSize);
    DBG_INFO("Volume ID is %x:%x", VolumeId >> 16, VolumeId & 0xffff);
    DBG_INFO("%d Reserved sectors, %d sectors per FAT, %d FATs",
             ReservedSectCount,
             FatSize,
             NumFATs);
    DBG_INFO("%llu Total clusters", ClusterCount);

    //
    // Fix up the FSInfo sector.
    //

    pFAT32FsInfo->dFree_Count = (UserAreaSize / SectorsPerCluster) - 1;
    pFAT32FsInfo->dNxt_Free = 3; // clusters 0-1 reserved, we used cluster 2 for the root dir

    DBG_INFO("%d Free clusters", pFAT32FsInfo->dFree_Count);

    //
    // Zero out ReservedSect + FatSize * NumFats + SectorsPerCluster
    //

    SystemAreaSize = ReservedSectCount + (NumFATs * FatSize) + SectorsPerCluster;
    DBG_INFO("Clearing out %d sectors for reserved sectors, FATs and root cluster...",
             SystemAreaSize);

    //
    // Not the most efficient, but easy on RAM.
    //

    pZeroSect = (BYTE*)AllocateZeroPool(SectorSize * BurstSize);
    if (!pZeroSect) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    for (UINT32 i = 0; i < (SystemAreaSize + BurstSize - 1); i += BurstSize) {
        Status = RamdiskWrite(RamdiskContext,
                              VolumeOffset + i * SectorSize,
                              SectorSize * BurstSize,
                              pZeroSect);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("Error clearing reserved sectors 0x%zx", Status);
            goto Exit;
        }
    }

    DBG_INFO("Initializing reserved sectors and FATs...");

    //
    // Now we should write the boot sector and fsinfo twice, once at 0 and once at the backup boot
    // sector offset
    //

    for (UINT32 i = 0; i < 2; i++) {
        SectorStart = (i == 0) ? 0 : BackupBootSect;
        Status = RamdiskWrite(RamdiskContext,
                              VolumeOffset + SectorStart * SectorSize,
                              SectorSize,
                              (UINT8*)BootSector);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("Error writing Boot Sector to sector offset %d, error 0x%zx",
                      SectorStart,
                      Status);
            goto Exit;
        }

        Status = RamdiskWrite(RamdiskContext,
                              VolumeOffset + (SectorStart + 1) * SectorSize,
                              SectorSize,
                              (UINT8*)pFAT32FsInfo);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("Error writing Boot Sector to sector offset %d, error 0x%zx",
                      SectorStart,
                      Status);
            goto Exit;
        }
    }

    //
    // Write the first FAT sector in the right places.
    //

    for (UINT32 i = 0; i < NumFATs; i++) {
        SectorStart = ReservedSectCount + (i * FatSize);
        DBG_INFO("FAT #%d sector at address: %d", i, SectorStart);

        Status = RamdiskWrite(RamdiskContext,
                              VolumeOffset + SectorStart * SectorSize,
                              SectorSize,
                              (UINT8*)pFirstSectOfFat);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("Error writing FAT sector to sector offset %d, error 0x%zx",
                      SectorStart,
                      Status);
            goto Exit;
        }
    }

    FirstDataSector = ReservedSectCount + (NumFATs * FatSize) + BootSector->PackedBpb.RootEntries;

    //
    // FATGEN103.DOC says: "When a directory is created, a file with the ATTR_DIRECTORY bit set in
    // its DIR_Attr field, you set its DIR_FileSize to 0. DIR_FileSize is not used and is always 0
    // on a file with the ATTR_DIRECTORY attribute (directories are sized by simply following their
    // cluster chains to the EOC mark). One cluster is allocated to the directory (unless it is the
    // root directory on a FAT16/FAT12 volume), and you set DIR_FstClusLO and DIR_FstClusHI to that
    // cluster number and place an EOC mark in that clusters entry in the FAT. Next, you initialize
    // all bytes of that cluster to 0. If the directory is the root directory, you are done (there
    // are no dot or dotdot entries in the root directory). If the directory is not the root
    // directory, you need to create two special entries in the first two 32-byte directory entries
    // of the directory (the first two 32 byte entries in the data region of the cluster you just
    // allocated)."
    //

    //
    // Wikipedia says: "Ideally, the volume label should be the first entry in the directory
    // (after reserved entries) in order to avoid problems with VFAT LFNs". So, that is what
    // we'll do.
    //

    VolumeLabelEntry = AllocateZeroPool(sizeof(DIR_ENTRY));
    if (VolumeLabelEntry == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    //
    // 0x20 is space character.
    //

    Status = SetMemS(&VolumeLabelEntry->Name,
                     sizeof(VolumeLabelEntry->Name),
                     sizeof(VolumeLabelEntry->Name),
                     0x20);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("SetMemS() failed 0x%zx", Status);
        goto Exit;
    }

    Status = CopyMemS(&VolumeLabelEntry->Name,
                      sizeof(VolumeLabelEntry->Name),
                      VOLUMELABEL,
                      VOLUMELABEL_LENGTH);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("CopyMemS() failed 0x%zx", Status);
        goto Exit;
    }

    VolumeLabelEntry->Attr = ATTR_VOLUME_ID;
    VolumeLabelEntry->NTRes = 0; // Reserved value.
    VolumeLabelEntry->CrtTimeTenth = 0;
    VolumeLabelEntry->CrtTime = 0;
    VolumeLabelEntry->CrtDate = 0;
    VolumeLabelEntry->LstAccDate = 0;
    VolumeLabelEntry->FstClusHI = 0;
    VolumeLabelEntry->WrtTime; // TODO
    VolumeLabelEntry->WrtDate; // TODO
    VolumeLabelEntry->FstClusLO = 0;
    VolumeLabelEntry->FileSize = 0;

    //
    // Copy the FAT32 volume label directory entry to root directory.
    //

    Status = RamdiskWrite(RamdiskContext,
                          VolumeOffset + FirstDataSector * SectorSize,
                          sizeof(DIR_ENTRY),
                          (UINT8*)VolumeLabelEntry);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Error writing VolumeLabelEntry to root directory, error 0x%zx", Status);
        goto Exit;
    }

    //
    // No need to update FAT, as index 2 (cluster 2) has already been filled
    // with EoC value.
    //

    //
    // Note: The diskmgmt.msc FAT32 format utility also plops in a hidden
    // "System Volume Information" directory entry in the root directory.
    // I'm opting to not add it, as it complicates directory initialization
    // since it requires adding several LFN entries to the root directory,
    // plus the \. and \.. directories required by the FAT spec. I have also
    // confirmed the system can boot and function properly without it.
    // According to online resources, it does seem like Windows creates
    // it for us if not found, so we should be ok.
    //

    DBG_INFO("Format completed.");

Exit:

    FreePool(MbrGpt);
    FreePool(VolumeLabelEntry);
    FreePool(pZeroSect);
    FreePool(BootSector);
    FreePool(pFAT32FsInfo);
    FreePool(pFirstSectOfFat);

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_RAMDISK_FAT32_VOLUME_CREATION_FAILED);
    }

    return Status;
}

//
// Local functions
//
static EFI_STATUS EFIAPI RamdiskLocateProtocol(VOID)
{
    EFI_STATUS Status = EFI_SUCCESS;

    if (gspRamDiskProtocol == NULL) {
        Status = gBS->LocateProtocol(&gEfiRamDiskProtocolGuid, NULL, (VOID**)&gspRamDiskProtocol);
        if (EFI_ERROR(Status)) {
            DBG_ERROR(
                "Could not locate EFI_RAM_DISK_PROTOCOL. Likely RamDiskDxe driver is missing 0x%zx",
                Status);
            gspRamDiskProtocol = NULL;
        } else {
            DBG_INFO("Located ramdisk protocol");
        }
    }

    return Status;
}

/*
 * Proper computation of FAT size
 * See: http://www.syslinux.org/archives/2016-February/024850.html
 * and subsequent replies.
 */
static DWORD GetFATSizeSectors(DWORD DskSize,
                               DWORD ReservedSecCnt,
                               DWORD SecPerClus,
                               DWORD NumFATs,
                               DWORD BytesPerSect)
{
    ULONGLONG Numerator, Denominator;
    ULONGLONG FatElementSize = 4;
    ULONGLONG ReservedClusCnt = 2;
    ULONGLONG FatSz;

    Numerator = DskSize - ReservedSecCnt + ReservedClusCnt * SecPerClus;
    Denominator = SecPerClus * BytesPerSect / FatElementSize + NumFATs;
    FatSz = Numerator / Denominator + 1; // +1 to ensure we are rounded up

    return (DWORD)FatSz;
}

static DWORD GetVolumeID(void)
{
    EFI_TIME Time;
    EFI_STATUS Status = EFI_SUCCESS;

    DWORD Id;
    WORD Low, High, Temp;

    Status = gRT->GetTime(&Time, NULL);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("GetTime() failed : 0x%zx", Status);
    }

    Low = Time.Day + (Time.Month << 8);
    Temp = (WORD)(Time.Nanosecond / 10000000) + (Time.Second << 8);
    Low += Temp;

    High = Time.Minute + (Time.Hour << 8);
    High += Time.Year;

    Id = Low + (High << 16);
    return Id;
}

#ifndef UEFI_BUILD_SYSTEM
#pragma prefast(pop)
#endif
