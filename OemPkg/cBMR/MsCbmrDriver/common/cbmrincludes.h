#ifndef _CBMRINCLUDES_H_
#define _CBMRINCLUDES_H_

#ifdef UEFI_BUILD_SYSTEM
#include <Uefi.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/SortLib.h>
#include <Library/PrintLib.h>
#include <Guid/FileSystemInfo.h>
#include <Guid/FileSystemVolumeLabelInfo.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DebugPort.h>
#include <Protocol/DevicePathUtilities.h>
#include <Protocol/DevicePathToText.h>
#include <Protocol/DevicePathFromText.h>
#include <Protocol/HiiFont.h>
#include <Protocol/Ip4Config2.h>
#include <Protocol/WiFi.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/PartitionInfo.h>
#include <Protocol/Shell.h>
#include <Protocol/Supplicant.h>
#include <Protocol/Smbios.h>

#define _In_     IN
#define _In_opt_ IN
#define _In_z_   IN
#define _Printf_format_string_
#define _Inout_                     IN OUT
#define _Out_                       OUT
#define _Outptr_                    OUT
#define _Outptr_result_maybenull_z_ OUT
#define _Ret_z_
#define _Out_writes_z_(x)
#define _Out_writes_(x)
#define _Out_writes_bytes_(x)
#define _In_reads_bytes_(x)
#define _In_reads_z_(x)
#define _Outptr_opt_result_buffer_to_(x, y)
#define _Return_type_success_(x)

#define PWSTR CHAR16*
#define UNREFERENCED_PARAMETER(x)
typedef void* PVOID;
typedef int INT;
#define PUINT8 UINT8*

#define gEfiWirelessMacConnection2ProtocolGuid gEfiWiFi2ProtocolGuid
#define EFI_DEBUGPORT                          EFI_DEBUGPORT_PROTOCOL
#define EFI_PARTITION_INFO                     EFI_PARTITION_INFO_PROTOCOL
extern EFI_GUID gEfiPartitionRecordGuid;
#define EFI_PARTITION_TYPE_GPT PARTITION_TYPE_GPT

#define _countof(x) ARRAY_SIZE(x)
#define va_list     VA_LIST
#define va_start    VA_START
#define va_end      VA_END

#define ASSERT(x)

#define StrHexToUintn     StrHexToUintnMsExtension
#define StrDecimalToUintn StrDecimalToUintnMsExtension

#define ConvertTextToDevicPath ConvertTextToDevicePath

#else
#include <bootlib.h>
#include <strsafe.h>

#define EFI_END_OF_MEDIA     EFIERR(28)
#define EFI_END_OF_FILE      EFIERR(29)
#define EFI_INVALID_LANGUAGE EFIERR(30)
#define EFI_COMPROMISED_DATA EFIERR(31)
#define EFI_HTTP_ERROR       EFIERR(32)

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define ABS(x, y) ((x) > (y) ? ((x) - (y)) : ((y) - (x)))

#if defined(_X86_) || defined(_AMD64_)
#define GetTickCount() (__rdtsc())
#elif defined(_ARM_)
#define GetTickCount() (__rdpmccntr64())
#elif defined(_ARM64_)
#define GetTickCount() ((unsigned __int64)_ReadStatusReg(ARM64_PMCCNTR_EL0))
#else
#error "Architecture not supported"
#endif
#endif

#define t(x) (CHAR8*)(x)
#define T(x) (CHAR16*)(L##x)

#define MICROSECONDS       1000000ULL
#define NANOSECONDS        1000000000ULL
#define SEC_TO_US(Sec)     (MICROSECONDS * Sec)
#define SEC_TO_100_NS(Sec) ((NANOSECONDS * Sec) / 100)

#define HASH_LENGTH       32
#define MAX_80211_PWD_LEN 63

extern EFI_BOOT_SERVICES* gBS;
extern EFI_SYSTEM_TABLE* gSystemTable;
extern EFI_RUNTIME_SERVICES* gRT;
extern EFI_HANDLE* gImageHandle;

#include "cbmr.h"
#include "utils.h"
#include "edk2compat.h"
#include "cbmr_config.h"
#include "cbmrdebug.h"
#include "safe_arithmetic.h"

#define STRINGIFY(Name) t(#Name)
typedef struct _ENUM_TO_STRING {
    INT Value;
    CHAR8* String;
} ENUM_TO_STRING;

#define KB (1024ULL)
#define MB (KB * 1024ULL)
#define GB (MB * 1024ULL)
#define TB (GB * 1024ULL)

#endif // _CBMRINCLUDES_H_
