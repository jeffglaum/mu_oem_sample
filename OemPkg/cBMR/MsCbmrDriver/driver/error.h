#ifndef _ERROR_H_
#define _ERROR_H_

#include "cbmrincludes.h"

// clang-format off

#define CBMR_ERROR_SUCCESS                                              0x00000000

//
// Driver
//

#define CBMR_ERROR_DRIVER_NOT_CONFIGURED                                0x00001001
#define CBMR_ERROR_DRIVER_WIFI_DEPOSITION_FAILED                        0x00001002
#define CBMR_ERROR_DRIVER_SOFTWARE_INVENTORY_DEPOSITION_FAILED          0x00001003
#define CBMR_ERROR_DRIVER_SOFTWARE_INVENTORY_PROCESSING_FAILED          0x00001004
#define CBMR_ERROR_DRIVER_DCAT_INFO_DEPOSITION_FAILED                   0x00001005
#define CBMR_ERROR_DRIVER_OS_DRIVER_DOWNLOAD_FAILED                     0x00001006
#define CBMR_ERROR_DRIVER_DCAT_COLLATERAL_FETCH_FAILED                  0x00001007
#define CBMR_ERROR_DRIVER_DCAT_COLLATERAL_DOWNLOAD_FAILED               0x00001008
#define CBMR_ERROR_DRIVER_BOOT_COLLATERAL_EXTRACTION_FAILED             0x00001009
#define CBMR_ERROR_DRIVER_SERVICEING_FAILED                             0x0000100A
#define CBMR_ERROR_DRIVER_RAMBOOTING_FAILED                             0x0000100B
#define CBMR_ERROR_DRIVER_RAMDISK_CONFIGURATION_FAILED                  0x0000100C

//
// DCAT
//

#define CBMR_ERROR_DCAT_INITIALIZATION_FAILED                           0x00002001
#define CBMR_ERROR_DCAT_UNABLE_TO_RETRIEVE_JSON                         0x00002002
#define CBMR_ERROR_DCAT_UNABLE_TO_PARSE_JSON                            0x00002003
#define CBMR_ERROR_DCAT_UNABLE_TO_BUILD_JSON_REQUEST                    0x00002004

//
// RAMDISK
//

#define CBMR_ERROR_RAMDISK_INITIALIZATION_FAILED                        0x00003001
#define CBMR_ERROR_RAMDISK_REGISTRATION_FAILED                          0x00003002
#define CBMR_ERROR_RAMDISK_BOOT_FAILED                                  0x00003003
#define CBMR_ERROR_RAMDISK_FAT32_VOLUME_CREATION_FAILED                 0x00003004

//
// TLS
//

#define CBMR_ERROR_TLS_CONFIGURATION_FAILED                             0x00004001
#define CBMR_ERROR_TLS_UNABLE_TO_UPDATE_TLS_CERT_VAR                    0x00004002

//
// WIM
//

#define CBMR_ERROR_WIM_INITIALIZATION_FAILED                            0x00005001
#define CBMR_ERROR_WIM_EXTRACTION_FAILED                                0x00005002

//
// HTTP
//

#define CBMR_ERROR_HTTP_INITIALIZATION_FAILED                           0x00006001
#define CBMR_ERROR_HTTP_INSTANCE_CREATION_FAILED                        0x00006002
#define CBMR_ERROR_HTTP_CONFIGURE_FAILED                                0x00006003
#define CBMR_ERROR_HTTP_REQUEST_ISSUE_FAILED                            0x00006004
#define CBMR_ERROR_HTTP_UNABLE_TO_READ_RESPONSE                         0x00006005

//
// CAB
//

#define CBMR_ERROR_CAB_INITIALIZATION_FAILED                           0x00007001
#define CBMR_ERROR_CAB_EXTRACTION_FAILED                               0x00007002

// clang-format on

VOID CbmrInitializeErrorModule(_In_ PEFI_MS_CBMR_PROTOCOL This);
EFI_STATUS CbmrGetExtendedErrorInfo(_Inout_ PEFI_MS_CBMR_ERROR_DATA Data, _Inout_ UINTN* DataSize);
VOID CbmrSetExtendedErrorInfo(_In_ EFI_STATUS ErrorStatus, _In_ UINTN StopCode);
VOID CbmrClearExtendedErrorInfo();

#endif // _ERROR_H_
