#ifndef _CAB_EXTRACT_H_
#define _CAB_EXTRACT_H_

typedef struct _CAB_EXTRACT_CONTEXT CAB_EXTRACT_CONTEXT;

// FIXME: Abstract away EFI_FILE_PROTOCOL into a to-be-defined STREAM object.
// This way we don't need to worry about where the CAB is stored (in memory buffer, file,
// network, etc).
EFI_STATUS EFIAPI CabExtractInit(_In_ EFI_FILE_PROTOCOL* CabFile,
                                 _Outptr_ CAB_EXTRACT_CONTEXT** Context);
EFI_STATUS EFIAPI CabExtractFree(_Inout_ CAB_EXTRACT_CONTEXT* Context,
                                 _In_ BOOLEAN DeleteOriginalCab);
EFI_STATUS EFIAPI CabExtractFiles(_In_ CAB_EXTRACT_CONTEXT* Context,
                                  _In_z_ CHAR16* PartitionName,
                                  _In_z_ CHAR16* DestinationDirectory);

#endif // _CAB_EXTRACT_H_