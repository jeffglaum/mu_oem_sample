
#ifndef _PROTOCOLS_H_
#define _PROTOCOLS_H_

#include "cbmrincludes.h"

#define PROTO(ProtocolGUID, ProtocolGUIDStr)                                                 \
    {                                                                                        \
        ProtocolGUID, {0}, (CHAR8*)ProtocolGUIDStr, NULL, NULL, NULL, EFI_INVALID_PARAMETER, \
    }

#define SB_PROTO(ProtocolGUID, ProtocolGUIDStr, SbProtocolGUID, SbProtocolGUIDStr)              \
    {                                                                                           \
        ProtocolGUID, SbProtocolGUID, (CHAR8*)ProtocolGUIDStr, (CHAR8*)SbProtocolGUIDStr, NULL, \
            NULL, EFI_INVALID_PARAMETER,                                                        \
    }

typedef struct _PROTOCOL_INFO {
    //
    //  In Parameters
    //

    EFI_GUID* ProtocolGuid;
    EFI_GUID* ServiceBindingProtocolGuid;
    CHAR8* ProtocolName;
    CHAR8* ServiceBindingProtocolName;

    //
    //  Out Parameters
    //

    PVOID Protocol;
    EFI_SERVICE_BINDING_PROTOCOL* ServiceBindingProtocol;
    EFI_STATUS ProtocolStatus;
    EFI_STATUS ServiceBindingProtocolStatus;
    EFI_HANDLE DeviceHandle;
    EFI_HANDLE ChildHandle;

} PROTOCOL_INFO, *PPROTOCOL_INFO;

EFI_STATUS EFIAPI ProtocolGetInfo(_Inout_ PPROTOCOL_INFO ProtocolInfo);
EFI_STATUS EFIAPI ProtocolServiceBindingClose(_In_ PPROTOCOL_INFO ProtocolInfo);

#endif // _PROTOCOLS_H_