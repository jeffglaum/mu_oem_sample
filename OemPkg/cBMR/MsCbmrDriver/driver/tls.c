/*++

Copyright (c) 2021 Microsoft Corporation

Module Name:

    tls.c

Abstract:

    This module implements tls support for cbmr

Author:

    Jancarlo Perez (jpere) 23-May-2021

Environment:

    UEFI mode only.

--*/

//
// Global includes
//
#include "cbmrincludes.h"
#include "cbmr.h"

//
// Local includes
//
#include "tls.h"
#include "file.h"
#include "error.h"

//
// Constants/Macros
//

#define EDKII_HTTP_TLS_CIPHER_LIST_VARIABLE L"HttpTlsCipherList"
#define EFI_TLS_CA_CERTIFICATE_VARIABLE     L"TlsCaCertificate"
#define ROOT_CA_CERT_FILENAME               L"rootCertificate.cer"
#define INTERMEDIATE_CA_CERT_FILENAME       L"intermediateCertificate.cer"

//
// Structs
//

#pragma pack(push, 1)
typedef struct _EFI_SIGNATURE_DATA2 {
    //
    // An identifier which identifies the agent which added the signature to the list.
    //
    GUID SignatureOwner;

    //
    // The format of the signature is defined by the SignatureType.
    //
    UINT8 SignatureData[1];
} EFI_SIGNATURE_DATA2;
#pragma pack(pop)

//
// Variables
//

EFI_GUID EDKII_HTTP_TLS_CIPHER_LIST_GUID = {0x46ddb415,
                                            0x5244,
                                            0x49c7,
                                            {0x93, 0x74, 0xf0, 0xe2, 0x98, 0xe7, 0xd3, 0x86}};
EFI_GUID EFI_TLS_CA_CERTIFICATE_GUID = {0xfd2340D0,
                                        0x3dab,
                                        0x4349,
                                        {0xa6, 0xc7, 0x3b, 0x4f, 0x12, 0xb4, 0x8e, 0xae}};

//
// Prototypes
//

static BOOLEAN EFIAPI TlsUefiVariableContainsRequiredCerts(_In_reads_(CertCount) CERT CertArray[],
                                                           _In_ UINTN CertCount);
static EFI_STATUS EFIAPI TlsDeleteCACertList();

//
// Interfaces
//

EFI_STATUS EFIAPI TlsSetCACertList(_In_reads_(CertCount) CERT CertArray[], _In_ UINTN CertCount)
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINTN TotalCertsSize = 0;
    UINTN CertDatabaseSize = 0;
    EFI_SIGNATURE_LIST* Cert = NULL;
    EFI_SIGNATURE_LIST* LocalCert = NULL;
    EFI_SIGNATURE_DATA2* CertData = NULL;

    if (CertArray == NULL || CertCount == 0) {
        DBG_ERROR("Invalid parameter(s): CertArray(%p), CertCount %zu", CertArray, CertCount);
        Status = EFI_INVALID_PARAMETER;
        goto Exit;
    }

    for (UINT8 i = 0; i < CertCount; i++) {
        if (!CertArray[i].Revoked) {
            TotalCertsSize += CertArray[i].Size;
        }
    }

    CertDatabaseSize = CertCount * sizeof(EFI_SIGNATURE_LIST) +
                       CertCount * FIELD_OFFSET(EFI_SIGNATURE_DATA2, SignatureData) +
                       TotalCertsSize;

    LocalCert = Cert = AllocateZeroPool(CertDatabaseSize);
    if (Cert == NULL) {
        DBG_ERROR("Out of memory.");
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    for (UINT8 i = 0; i < CertCount; i++) {
        if (!CertArray[i].Revoked) {
            Cert->SignatureListSize = sizeof(EFI_SIGNATURE_LIST) +
                                      FIELD_OFFSET(EFI_SIGNATURE_DATA2, SignatureData) +
                                      (UINT32)CertArray[i].Size;
            Cert->SignatureHeaderSize = 0;
            Cert->SignatureSize = FIELD_OFFSET(EFI_SIGNATURE_DATA2, SignatureData) +
                                  (UINT32)CertArray[i].Size;
            Status = CopyMemS(&Cert->SignatureType,
                              sizeof(EFI_GUID),
                              &gEfiCertX509Guid,
                              sizeof(EFI_GUID));
            if (EFI_ERROR(Status)) {
                DBG_ERROR("CopyMemS() failed 0x%zx", Status);
                goto Exit;
            }

            CertData = (EFI_SIGNATURE_DATA2*)((UINT8*)Cert + sizeof(EFI_SIGNATURE_LIST));
            Status = CopyMemS(&CertData->SignatureOwner,
                              sizeof(EFI_GUID),
                              &(EFI_GUID)EFI_MS_CBMR_PROTOCOL_GUID,
                              sizeof(EFI_GUID));
            if (EFI_ERROR(Status)) {
                DBG_ERROR("CopyMemS() failed 0x%zx", Status);
                goto Exit;
            }

            Status = CopyMemS((UINT8*)(CertData->SignatureData),
                              CertArray[i].Size,
                              CertArray[i].Buffer,
                              CertArray[i].Size);
            if (EFI_ERROR(Status)) {
                DBG_ERROR("CopyMemS() failed 0x%zx", Status);
                goto Exit;
            }

            Cert = (EFI_SIGNATURE_LIST*)((UINT8*)Cert + sizeof(EFI_SIGNATURE_LIST) +
                                         FIELD_OFFSET(EFI_SIGNATURE_DATA2, SignatureData) +
                                         CertArray[i].Size);
        }
    }

    if (gCbmrConfig.WriteCertListToFile) {
        EFI_FILE_PROTOCOL* CertListFile = NULL;
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* SimpleFileSystem = NULL;
        EFI_FILE_PROTOCOL* Root = NULL;
        EFI_LOADED_IMAGE* LoadedImage = NULL;

        Status = gBS->HandleProtocol(gImageHandle,
                                     &gEfiLoadedImageProtocolGuid,
                                     (void**)&LoadedImage);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("Failed to obtaine EFI_LOADED_IMAGE protocol, 0x%zx", Status);
            goto Exit;
        }

        // Open SIMPLE_FILE_SYSTEM_PROTOCOL for the volume from which the
        // current image is loaded
        Status = gBS->HandleProtocol(LoadedImage->DeviceHandle,
                                     &gEfiSimpleFileSystemProtocolGuid,
                                     (void**)&SimpleFileSystem);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("Failed to obtaine SIMPLE_FILE_SYSTEM_PROTOCOL, 0x%zx", Status);
            goto Exit;
        }

        Status = SimpleFileSystem->OpenVolume(SimpleFileSystem, &Root);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("Failed to open root volume, 0x%zx", Status);
            goto Exit;
        }

        Status = Root->Open(Root,
                            &CertListFile,
                            L"certlist.bin",
                            EFI_FILE_MODE_CREATE | EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
                            0);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("Failed to create certlist.bin file, 0x%zx", Status);
            goto Exit;
        }

        Status = FileWrite(CertListFile, &CertDatabaseSize, LocalCert);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileWrite() failed 0x%zx", Status);
            goto Exit;
        }

        Status = FileClose(CertListFile);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileClose() failed 0x%zx", Status);
            goto Exit;
        }

        DBG_INFO("Successfully wrote EFI_SIGNATURE_LIST payload to certlist.bin file");
    }

    //
    // We set data only with BS attribute, so if the variable exists with NV + (BS | RT),
    // the SetVariable call will fail (per UEFI spec). Hence why we delete the variable in
    // advance if it is found.
    //

    Status = TlsDeleteCACertList();
    if (Status == EFI_WRITE_PROTECTED) {
        DBG_INFO("Existing TLS variable cannot be modified, 0x%zx", Status);
        //
        // Check if TlsCaCertificate variable is already populated and matches expected cert list.
        // Some FW policies prevent writes to the TlsCaCertficate variable, so this is an attempt
        // to handle this scenario and have some level of confidence we are cert pinning against
        // correct certificates.
        //

        if (TlsUefiVariableContainsRequiredCerts(CertArray, CertCount)) {
            Status = EFI_SUCCESS;
            DBG_INFO("Existing cert list contains required certs, skip write");
            goto Exit;
        } else {
            DBG_INFO("TLS variable is writed protected and does not contain required certs.");
            goto Exit;
        }
    } else if (EFI_ERROR(Status)) {
        DBG_ERROR("TlsDeleteCACertList() failed 0x%zx", Status);
        goto Exit;
    } else {
        Status = gRT->SetVariable(EFI_TLS_CA_CERTIFICATE_VARIABLE,
                                  &EFI_TLS_CA_CERTIFICATE_GUID,
                                  EFI_VARIABLE_BOOTSERVICE_ACCESS,
                                  CertDatabaseSize,
                                  LocalCert);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("Unable to set CBMR TLS certificate(s). 0x%zx", Status);
        }

        DBG_INFO("Successfully set TLS certificate(s).");
    }

Exit:

    FreePool(LocalCert);

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_TLS_CONFIGURATION_FAILED);
    }

    return Status;
}

#ifdef DEBUGMODE
EFI_STATUS EFIAPI TlsSetCACertListDebug()
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINTN CertDatabaseSize = 0;
    EFI_SIGNATURE_LIST* Cert = NULL;
    EFI_SIGNATURE_LIST* LocalCert = NULL;
    EFI_SIGNATURE_DATA2* CertData = NULL;
    UINTN CertCount = 0;

    //
    // TODO: Make below more generic by allowing a list of certs instead
    // of searching for a single root and a single intermediate cert.
    //

    //
    // Below logic checks for root and intermediate certs in root directory
    // of any available file system. If it finds them, it will add create a
    // EFI_SIGNATURE_LIST and provision it into TlsCaCertificate variable, overriding
    // the default cert store. Useful for testing against local HTTPS endpoints.
    //

    EFI_FILE_PROTOCOL* RootCertificateFile = NULL;
    EFI_FILE_PROTOCOL* IntermediateCertificateFile = NULL;
    UINT64 RootCertificateFileSize = 0;
    UINT64 IntermediateCertificateFileSize = 0;

    //
    // Check for existence of CA certificate in root directory
    //

    Status = FileLocateAndOpen(ROOT_CA_CERT_FILENAME, EFI_FILE_MODE_READ, &RootCertificateFile);
    if (EFI_ERROR(Status)) {
        DBG_WARNING("Did not find root CA certificate. Skip setting. 0x%zx", Status);
        Status = EFI_SUCCESS;
    } else {
        CertCount++;
    }

    Status = FileLocateAndOpen(INTERMEDIATE_CA_CERT_FILENAME,
                               EFI_FILE_MODE_READ,
                               &IntermediateCertificateFile);
    if (EFI_ERROR(Status)) {
        DBG_WARNING("Did not find intermediate CA certificate. Skip setting. 0x%zx", Status);
        Status = EFI_SUCCESS;
    } else {
        CertCount++;
    }

    if (CertCount == 0) {
        DBG_INFO("No external certificates found.");
        goto Exit;
    }

    if (RootCertificateFile) {
        Status = FileGetSize(RootCertificateFile, &RootCertificateFileSize);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("Error acquiring certificate file size. 0x%zx", Status);
            goto Exit;
        }
    }

    if (IntermediateCertificateFile) {
        Status = FileGetSize(IntermediateCertificateFile, &IntermediateCertificateFileSize);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("Error acquiring certificate file size. 0x%zx", Status);
            goto Exit;
        }
    }

    CertDatabaseSize = CertCount * sizeof(EFI_SIGNATURE_LIST) +
                       CertCount * FIELD_OFFSET(EFI_SIGNATURE_DATA2, SignatureData) +
                       (UINTN)RootCertificateFileSize + (UINTN)IntermediateCertificateFileSize;

    LocalCert = Cert = AllocateZeroPool(CertDatabaseSize);
    if (Cert == NULL) {
        DBG_ERROR("Out of memory.");
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
    }

    if (RootCertificateFile) {
        Cert->SignatureListSize = sizeof(EFI_SIGNATURE_LIST) +
                                  FIELD_OFFSET(EFI_SIGNATURE_DATA2, SignatureData) +
                                  (UINT32)RootCertificateFileSize;
        Cert->SignatureHeaderSize = 0;
        Cert->SignatureSize = FIELD_OFFSET(EFI_SIGNATURE_DATA2, SignatureData) +
                              (UINT32)RootCertificateFileSize;
        Status = CopyMemS(&Cert->SignatureType,
                          sizeof(EFI_GUID),
                          &gEfiCertX509Guid,
                          sizeof(EFI_GUID));
        if (EFI_ERROR(Status)) {
            DBG_ERROR("CopyMemS() failed 0x%zx", Status);
            goto Exit;
        }

        CertData = (EFI_SIGNATURE_DATA2*)((UINT8*)Cert + sizeof(EFI_SIGNATURE_LIST));
        Status = CopyMemS(&CertData->SignatureOwner,
                          sizeof(EFI_GUID),
                          &(EFI_GUID)EFI_MS_CBMR_PROTOCOL_GUID,
                          sizeof(EFI_GUID));
        if (EFI_ERROR(Status)) {
            DBG_ERROR("CopyMemS() failed 0x%zx", Status);
            goto Exit;
        }

        Status = FileRead(RootCertificateFile,
                          (UINTN*)&RootCertificateFileSize,
                          (UINT8*)(CertData->SignatureData));
        if (EFI_ERROR(Status)) {
            DBG_ERROR("Error reading certificate payload. 0x%zx", Status);
            goto Exit;
        }
    }

    if (IntermediateCertificateFile) {
        Cert = RootCertificateFile == NULL ?
                   Cert :
                   (EFI_SIGNATURE_LIST*)((UINT8*)Cert + sizeof(EFI_SIGNATURE_LIST) +
                                         FIELD_OFFSET(EFI_SIGNATURE_DATA2, SignatureData) +
                                         RootCertificateFileSize);
        Cert->SignatureListSize = sizeof(EFI_SIGNATURE_LIST) +
                                  FIELD_OFFSET(EFI_SIGNATURE_DATA2, SignatureData) +
                                  (UINT32)IntermediateCertificateFileSize;
        Cert->SignatureHeaderSize = 0;
        Cert->SignatureSize = FIELD_OFFSET(EFI_SIGNATURE_DATA2, SignatureData) +
                              (UINT32)IntermediateCertificateFileSize;
        Status = CopyMemS(&Cert->SignatureType,
                          sizeof(EFI_GUID),
                          &gEfiCertX509Guid,
                          sizeof(EFI_GUID));
        if (EFI_ERROR(Status)) {
            DBG_ERROR("CopyMemS() failed 0x%zx", Status);
            goto Exit;
        }

        CertData = (EFI_SIGNATURE_DATA2*)((UINT8*)Cert + sizeof(EFI_SIGNATURE_LIST));
        Status = CopyMemS(&CertData->SignatureOwner,
                          sizeof(EFI_GUID),
                          &(EFI_GUID)EFI_MS_CBMR_PROTOCOL_GUID,
                          sizeof(EFI_GUID));
        if (EFI_ERROR(Status)) {
            DBG_ERROR("CopyMemS() failed 0x%zx", Status);
            goto Exit;
        }

        Status = FileRead(IntermediateCertificateFile,
                          (UINTN*)&IntermediateCertificateFileSize,
                          (UINT8*)(CertData->SignatureData));
        if (EFI_ERROR(Status)) {
            DBG_ERROR("Error reading certificate payload. 0x%zx", Status);
            goto Exit;
        }
    }

    //
    // We set data only with BS attribute, so if the variable exists with NV + (BS | RT),
    // the SetVariable call will fail (per UEFI spec). Hence why we delete the variable in advance
    // if it is found.
    //

    Status = TlsDeleteCACertList();
    if (EFI_ERROR(Status)) {
        DBG_ERROR("TlsDeleteCACertList() failed 0x%zx", Status);
        goto Exit;
    }

    Status = gRT->SetVariable(EFI_TLS_CA_CERTIFICATE_VARIABLE,
                              &EFI_TLS_CA_CERTIFICATE_GUID,
                              EFI_VARIABLE_BOOTSERVICE_ACCESS,
                              CertDatabaseSize,
                              LocalCert);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("Unable to override CBMR TLS certificate(s). 0x%zx", Status);
    }

Exit:
    if (RootCertificateFile) {
        FileClose(RootCertificateFile);
    }

    if (IntermediateCertificateFile) {
        FileClose(IntermediateCertificateFile);
    }

    FreePool(LocalCert);

    if (EFI_ERROR(Status)) {
        CbmrSetExtendedErrorInfo(Status, CBMR_ERROR_TLS_CONFIGURATION_FAILED);
    }

    return Status;
}
#endif

static BOOLEAN EFIAPI TlsUefiVariableContainsRequiredCerts(_In_reads_(CertCount) CERT CertArray[],
                                                           _In_ UINTN CertCount)
{
    UINTN Status = EFI_SUCCESS;
    BOOLEAN IsPresent = FALSE;
    UINTN CertListSize = 0;
    UINT8* UefiVariableCertList = NULL;
    UINT8* UefiVariableCert = NULL;
    EFI_SIGNATURE_LIST* SignatureList = NULL;
    EFI_SIGNATURE_DATA2* SignatureData = NULL;
    UINTN CertSize = 0;
    UINTN SignatureListOffset = 0;

    if (CertArray == NULL || CertCount == 0) {
        return FALSE;
    }

    Status = gRT->GetVariable(EFI_TLS_CA_CERTIFICATE_VARIABLE,
                              &EFI_TLS_CA_CERTIFICATE_GUID,
                              0,
                              &CertListSize,
                              NULL);
    if (Status == EFI_NOT_FOUND) {
        DBG_INFO("Certificate list not present");
        goto Exit;
    } else if (Status == EFI_BUFFER_TOO_SMALL && CertListSize != 0) {
        //
        // Try to read the variable
        //

        UefiVariableCertList = AllocateZeroPool(CertListSize);
        if (UefiVariableCertList == NULL) {
            DBG_ERROR("Out of memory.");
            Status = EFI_OUT_OF_RESOURCES;
            goto Exit;
        }

        Status = gRT->GetVariable(EFI_TLS_CA_CERTIFICATE_VARIABLE,
                                  &EFI_TLS_CA_CERTIFICATE_GUID,
                                  0,
                                  &CertListSize,
                                  UefiVariableCertList);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("GetVariable() failed 0x%zx", Status);
            goto Exit;
        }

        //
        // Every cert passed in by the caller must be found in the TlsCaCertificate variable cert
        // list, otherwise we should fail.
        //

        for (UINT8 i = 0; i < CertCount; i++) {
            if (!CertArray[i].Revoked) {
                IsPresent = FALSE;
                CERT CallerSuppliedCert = CertArray[i];
                while (SignatureListOffset != CertListSize) {
                    SignatureList = (EFI_SIGNATURE_LIST*)(UefiVariableCertList +
                                                          SignatureListOffset);

                    //
                    // Update the signature list offset before performing checks, as failures
                    // will cause us to go back to the beginning of the loop so we may check
                    // against additional certs.
                    //

                    Status = UintnAdd(SignatureListOffset,
                                      sizeof(EFI_SIGNATURE_LIST),
                                      &SignatureListOffset);
                    if (EFI_ERROR(Status)) {
                        DBG_ERROR("UintnAdd() failed, 0x%zx", Status);
                        goto Exit;
                    }

                    if (SignatureListOffset > CertListSize) {
                        DBG_ERROR("Signature list is missing header");
                        goto Exit;
                    }

                    SignatureData = (EFI_SIGNATURE_DATA2*)((UINT8*)SignatureList +
                                                           sizeof(EFI_SIGNATURE_LIST));

                    Status = UintnAdd(SignatureListOffset,
                                      FIELD_OFFSET(EFI_SIGNATURE_DATA2, SignatureData),
                                      &SignatureListOffset);
                    if (EFI_ERROR(Status)) {
                        DBG_ERROR("UintnAdd() failed, 0x%zx", Status);
                        goto Exit;
                    }

                    if (SignatureListOffset > CertListSize) {
                        DBG_ERROR("Signature owner is missing");
                        goto Exit;
                    }

                    if (SignatureList->SignatureSize <
                        (UINTN)FIELD_OFFSET(EFI_SIGNATURE_DATA2, SignatureData)) {
                        DBG_ERROR("Signature size too small %d", SignatureList->SignatureSize);
                        goto Exit;
                    }

                    CertSize = SignatureList->SignatureSize -
                               FIELD_OFFSET(EFI_SIGNATURE_DATA2, SignatureData);

                    if (CertSize == 0) {
                        DBG_ERROR("Cert size cannot be 0!");
                        goto Exit;
                    }

                    Status = UintnAdd(SignatureListOffset, CertSize, &SignatureListOffset);
                    if (EFI_ERROR(Status)) {
                        DBG_ERROR("UintnAdd() failed, 0x%zx", Status);
                        goto Exit;
                    }

                    if (SignatureListOffset > CertListSize) {
                        DBG_ERROR("SignatureListOffset exceeds variable size, bail out");
                        goto Exit;
                    }

                    //
                    // Signature header size should be 0 for x509 certs
                    //

                    if (SignatureList->SignatureHeaderSize != 0) {
                        DBG_ERROR("Signature header size should be zero! Actual size: 0x%x",
                                  SignatureList->SignatureHeaderSize);
                        goto Exit;
                    }

                    UefiVariableCert = SignatureData->SignatureData;

                    //
                    // Compare cert size
                    //

                    if (CertSize != CallerSuppliedCert.Size) {
                        DBG_INFO("Mismatching cert size (actual %zd, expected %d), skipping",
                                 CertSize,
                                 CallerSuppliedCert.Size);
                        continue;
                    }

                    //
                    // Compare cert binary data
                    //

                    if (CompareMem(UefiVariableCert, CallerSuppliedCert.Buffer, CertSize) == 0) {
                        DBG_INFO("Found cert %u", i);
                        IsPresent = TRUE;
                        break;
                    } else {
                        DBG_INFO("Mismatching certs");
                    }
                }

                if (!IsPresent) {
                    DBG_ERROR("Cert %u was not found", i);
                    goto Exit;
                }

                SignatureListOffset = 0;
            }
        }
    } else {
        DBG_ERROR("Query of TLS variable returned an unexpected status. 0x%zx", Status);
    }

Exit:

    FreePool(UefiVariableCertList);
    return IsPresent;
}

static EFI_STATUS EFIAPI TlsDeleteCACertList()
{
    UINTN Status = EFI_SUCCESS;
    UINTN TempSize = 0;

    Status = gRT->GetVariable(EFI_TLS_CA_CERTIFICATE_VARIABLE,
                              &EFI_TLS_CA_CERTIFICATE_GUID,
                              0,
                              &TempSize,
                              NULL);
    if (Status == EFI_NOT_FOUND) {
        //
        // Do nothing and proceed to setting the variable
        //
        DBG_INFO("No stale TLS certificates found.");
        Status = EFI_SUCCESS;
    } else if (Status == EFI_BUFFER_TOO_SMALL) {
        //
        // Delete existing variable
        //

        Status = gRT->SetVariable(EFI_TLS_CA_CERTIFICATE_VARIABLE,
                                  &EFI_TLS_CA_CERTIFICATE_GUID,
                                  0,
                                  0,
                                  NULL);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("Deletion of stale TLS certificate(s) failed. 0x%zx", Status);
        }

        DBG_INFO("Deleted stale TLS certificate(s)");
    } else {
        DBG_ERROR("Query of TLS variable returned an unexpected status. 0x%zx", Status);
    }

    return Status;
}