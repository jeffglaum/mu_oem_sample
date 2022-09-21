#ifndef _EDK2COMPAT_SMBIOS_H_
#define _EDK2COMPAT_SMBIOS_H_

// MdePkg\Include\IndustryStandard\SmBios.h

///
/// The Smbios structure header.
///
typedef struct {
    UINT8 Type;
    UINT8 Length;
    UINT16 Handle;
} SMBIOS_STRUCTURE;

typedef UINT8 SMBIOS_TABLE_STRING;

///
/// BIOS Characteristics
/// Defines which functions the BIOS supports. PCI, PCMCIA, Flash, etc.
///
typedef struct {
    UINT32 Reserved : 2; ///< Bits 0-1.
    UINT32 Unknown : 1;
    UINT32 BiosCharacteristicsNotSupported : 1;
    UINT32 IsaIsSupported : 1;
    UINT32 McaIsSupported : 1;
    UINT32 EisaIsSupported : 1;
    UINT32 PciIsSupported : 1;
    UINT32 PcmciaIsSupported : 1;
    UINT32 PlugAndPlayIsSupported : 1;
    UINT32 ApmIsSupported : 1;
    UINT32 BiosIsUpgradable : 1;
    UINT32 BiosShadowingAllowed : 1;
    UINT32 VlVesaIsSupported : 1;
    UINT32 EscdSupportIsAvailable : 1;
    UINT32 BootFromCdIsSupported : 1;
    UINT32 SelectableBootIsSupported : 1;
    UINT32 RomBiosIsSocketed : 1;
    UINT32 BootFromPcmciaIsSupported : 1;
    UINT32 EDDSpecificationIsSupported : 1;
    UINT32 JapaneseNecFloppyIsSupported : 1;
    UINT32 JapaneseToshibaFloppyIsSupported : 1;
    UINT32 Floppy525_360IsSupported : 1;
    UINT32 Floppy525_12IsSupported : 1;
    UINT32 Floppy35_720IsSupported : 1;
    UINT32 Floppy35_288IsSupported : 1;
    UINT32 PrintScreenIsSupported : 1;
    UINT32 Keyboard8042IsSupported : 1;
    UINT32 SerialIsSupported : 1;
    UINT32 PrinterIsSupported : 1;
    UINT32 CgaMonoIsSupported : 1;
    UINT32 NecPc98 : 1;
    UINT32 ReservedForVendor : 32; ///< Bits 32-63. Bits 32-47 reserved for BIOS
                                   ///< vendor and bits 48-63 reserved for System
                                   ///< Vendor.
} MISC_BIOS_CHARACTERISTICS;

///
/// Extended BIOS ROM size.
///
typedef struct {
    UINT16 Size : 14;
    UINT16 Unit : 2;
} EXTENDED_BIOS_ROM_SIZE;

///
/// BIOS Information (Type 0).
///
typedef struct {
    SMBIOS_STRUCTURE Hdr;
    SMBIOS_TABLE_STRING Vendor;
    SMBIOS_TABLE_STRING BiosVersion;
    UINT16 BiosSegment;
    SMBIOS_TABLE_STRING BiosReleaseDate;
    UINT8 BiosSize;
    MISC_BIOS_CHARACTERISTICS BiosCharacteristics;
    UINT8 BIOSCharacteristicsExtensionBytes[2];
    UINT8 SystemBiosMajorRelease;
    UINT8 SystemBiosMinorRelease;
    UINT8 EmbeddedControllerFirmwareMajorRelease;
    UINT8 EmbeddedControllerFirmwareMinorRelease;
    //
    // Add for smbios 3.1.0
    //
    EXTENDED_BIOS_ROM_SIZE ExtendedBiosSize;
} SMBIOS_TABLE_TYPE0;

///
/// System Information (Type 1).
///
/// The information in this structure defines attributes of the overall system
/// and is intended to be associated with the Component ID group of the system's
/// MIF. An SMBIOS implementation is associated with a single system instance
/// and contains one and only one System Information (Type 1) structure.
///
typedef struct {
    SMBIOS_STRUCTURE Hdr;
    SMBIOS_TABLE_STRING Manufacturer;
    SMBIOS_TABLE_STRING ProductName;
    SMBIOS_TABLE_STRING Version;
    SMBIOS_TABLE_STRING SerialNumber;
    EFI_GUID Uuid;
    UINT8 WakeUpType;
    SMBIOS_TABLE_STRING SKUNumber;
    SMBIOS_TABLE_STRING Family;
} SMBIOS_TABLE_TYPE1;

///
///  Base Board - Feature Flags.
///
typedef struct {
    UINT8 Motherboard : 1;
    UINT8 RequiresDaughterCard : 1;
    UINT8 Removable : 1;
    UINT8 Replaceable : 1;
    UINT8 HotSwappable : 1;
    UINT8 Reserved : 3;
} BASE_BOARD_FEATURE_FLAGS;

///
/// Base Board (or Module) Information (Type 2).
///
/// The information in this structure defines attributes of a system baseboard -
/// for example a motherboard, planar, or server blade or other standard system
/// module.
///
typedef struct {
    SMBIOS_STRUCTURE Hdr;
    SMBIOS_TABLE_STRING Manufacturer;
    SMBIOS_TABLE_STRING ProductName;
    SMBIOS_TABLE_STRING Version;
    SMBIOS_TABLE_STRING SerialNumber;
    SMBIOS_TABLE_STRING AssetTag;
    BASE_BOARD_FEATURE_FLAGS FeatureFlag;
    SMBIOS_TABLE_STRING LocationInChassis;
    UINT16 ChassisHandle;
    UINT8 BoardType; ///< The enumeration value from BASE_BOARD_TYPE.
    UINT8 NumberOfContainedObjectHandles;
    UINT16 ContainedObjectHandles[1];
} SMBIOS_TABLE_TYPE2;

///
/// Processor Information (Type 4)
///
/// This structure defines attributes of a single processor
///
typedef struct {
    SMBIOS_STRUCTURE Hdr;
    SMBIOS_TABLE_STRING SocketDesignation;
    UINT8 ProcessorType;
    UINT8 ProcessorFamily;
    SMBIOS_TABLE_STRING ProcessorManufacturer;
    UINT64 ProcessorId;
    SMBIOS_TABLE_STRING ProcessorVersion;
    UINT8 Voltage;
    UINT16 ExternalClock;
    UINT16 MaxSpeed;
    UINT16 CurrentSpeed;
    UINT8 Status;
    UINT8 ProcessorUpgrade;
    UINT16 L1CacheHandle;
    UINT16 L2CacheHandle;
    UINT16 L3CacheHandle;
    SMBIOS_TABLE_STRING SerialNumber;
    SMBIOS_TABLE_STRING AssetTag;
    SMBIOS_TABLE_STRING PartNumber;
    UINT8 CoreCount;
    UINT8 CoreEnabled;
    UINT8 ThreadCount;
    UINT16 ProcessorCharacteristics;
    UINT16 ProcessorFamily2;
    UINT16 CoreCount2;
    UINT16 CoreEnabled2;
    UINT16 ThreadCount2;
} SMBIOS_TABLE_TYPE4;

///
/// OEM Strings (Type 11).
/// This structure contains free form strings defined by the OEM. Examples of
/// this are: Part Numbers for Reference Documents for the system, contact
/// information for the manufacturer, etc.
///
typedef struct {
    SMBIOS_STRUCTURE Hdr;
    UINT8 StringCount;
} SMBIOS_TABLE_TYPE11;

///
/// Physical Memory Array - Error Correction Types.
///
typedef enum {
    MemoryArrayUseOther = 0x01,
    MemoryArrayUseUnknown = 0x02,
    MemoryArrayUseSystemMemory = 0x03,
    MemoryArrayUseVideoMemory = 0x04,
    MemoryArrayUseFlashMemory = 0x05,
    MemoryArrayUseNonVolatileRam = 0x06,
    MemoryArrayUseCacheMemory = 0x07
} MEMORY_ARRAY_USE;

///
/// Physical Memory Array (Type 16).
///
/// This structure describes a collection of memory devices that operate
/// together to form a memory address space.
///
typedef struct {
    SMBIOS_STRUCTURE Hdr;
    UINT8 Location;
    UINT8 Use;
    UINT8 MemoryErrorCorrection;
    UINT32 MaximumCapacity;
    UINT16 MemoryErrorInformationHandle;
    UINT16 NumberOfMemoryDevices;
    //
    // Add for smbios 2.7
    //
    UINT64 ExtendedMaximumCapacity;
} SMBIOS_TABLE_TYPE16;

#endif // _EDK2COMPAT_SMBIOS_H_
