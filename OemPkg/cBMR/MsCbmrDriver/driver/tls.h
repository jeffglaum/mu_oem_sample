#ifndef _TLS_H_
#define _TLS_H_

typedef struct _CERT {
    UINT32 Size;
    _Field_size_(Size) const UINT8* Buffer;
    BOOLEAN Revoked;
} CERT, *PCERT;

EFI_STATUS EFIAPI TlsSetCACertList(_In_reads_(CertCount) CERT CertArray[], _In_ UINTN CertCount);

#ifdef DEBUGMODE
EFI_STATUS EFIAPI TlsSetCACertListDebug();
#endif

#endif // _TLS_H_