#ifndef _WIM_H_
#define _WIM_H_

#ifndef A_SHA_DIGEST_LEN
#define A_SHA_DIGEST_LEN 20
#endif

typedef struct _WIM_CONTEXT WIM_CONTEXT;

// FIXME: Abstract away EFI_FILE_PROTOCOL into a to-be-defined STREAM object.
// This way we don't need to worry about where the WIM is stored (in memory buffer, file,
// network, etc).
EFI_STATUS EFIAPI WimInit(_In_ EFI_FILE_PROTOCOL* WimFile, _Outptr_ WIM_CONTEXT** Context);
EFI_STATUS EFIAPI WimFree(_Inout_ WIM_CONTEXT* Context);
EFI_STATUS EFIAPI WimExtractFileIntoDestination(_In_ WIM_CONTEXT* Context,
                                                _In_reads_z_(FilePathLength) CHAR8* FilePath,
                                                _In_ UINTN FilePathLength,
                                                _In_z_ CHAR16* DestinationPartitionName,
                                                _In_z_ CHAR16* DestinationFilePath);
EFI_STATUS EFIAPI WimExtractCbmrNode(_In_ WIM_CONTEXT* Context, _Outptr_ XmlNode2** CbmrNode);

#endif // _WIM_H_