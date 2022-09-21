/*++

Copyright (c) 2021 Microsoft Corporation

Module Name:

    cbmr_config.c

Abstract:

    This module implements reading of CBMR config file.

Author:

    Vineel Kovvuri (vineelko) 10-Nov-2021

Environment:

    UEFI mode only.

--*/

//
// Global includes
//

#include "cbmrincludes.h"
#ifndef UEFI_BUILD_SYSTEM
#include "strsafe.h"
#endif

//
// Local includes
//

#include "cbmr.h"
#include "cbmr_config.h"
#include "file.h"

#ifndef UEFI_BUILD_SYSTEM
#pragma prefast(push)
#pragma prefast(disable : 6101, \
                "False positive - _Out_ parameter is always initialized upon success")
#endif

#define DEFAULT_DCAT_PROD_URL \
    L"https://fe3.delivery.mp.microsoft.com:443/UpdateMetadataService/updates/search/v1/bydeviceinfo/"
#define DEFAULT_DCAT_PPE_URL \
    L"https://glb.cws-int.dcat.dsp.mp.microsoft.com/UpdateMetadataService/updates/search/v1/bydeviceinfo/"

#define MAX_LINE_SIZE 1024

CBMR_CONFIG gCbmrConfig = {
    // [debug]
    .DebugMask = 0,
    .SpewTarget = 0,

    // [app]
#ifdef DEBUGMODE
    .ShowWiFiUX = FALSE,
#else
    .ShowWiFiUX = TRUE,
#endif

    // [driver]
    .EndpointType = CBMR_ENDPOINT_TYPE_DCAT,
    .DcatEndpointType = CBMR_DCAT_ENDPOINT_TYPE_PROD,
    .ForceHttps = FALSE,
    .SkipHashValidation = FALSE,
    .EnableTestSigningOnStubOS = FALSE,
    .ServiceViaLocalCbmrDriver = FALSE,
    .WriteCertListToFile = FALSE,
};

// clang-format off
//
// [debug]
// # Bitwise mask with ERROR=1|WARNING=2|INFO=3|VERBOSE=4
// # mask=value
// # spew=console,debugger,file,uefivar,serial
// # early_break=true|false
//
// [app]
// # Display Wi-Fi connection manager UX.
// # show_wifi_ux=true|false
//
// # Instead of showing connection manager UX, directly connect to below Wi-Fi access point
// # wifi_sid=value
// # wifi_password=value
//
// [driver]
// # Uncomment below if testing against local HTTP endpoint.
// # Swap in your own IP address or URL to an arbitrary HTTP/HTTPS endpoint.
// # url=http://10.137.200.72:50000/
//
// # force_https=true|false
//
// # skip_hash_validation=true|false
//
// # dcat_endpoint_type=prod|ppe
//
// # endpoint_type=dcat|http|usbkey
//
// # If either of the following are true cbmr driver will try to locate si.wim and/or si2.wim
// # files in the root of the attached volumes and write it to the 'SoftwareInventory' and
// # 'SoftwareInventorySecondary' UEFI variables, respectively.
// # write_si_uefi_variable=true|false
// # write_si2_uefi_variable=true|false
//
// # This config dictates from where the drivers should be downloaded
// # and placed in to ramdisk.
// #    'dcat' - The drivers are downloaded from dcat.
// #    'usbkey' - The drivers are copied from usbkey\drivers to STUBOS\drivers.
// #    'none'   - The drivers download is skipped.
// # driver_download_endpoint_type=dcat|usbkey|none
//
// # This config enables test signing on stubos. Mainly used for testing test
// # signed drivers
// # enable_test_signing_on_stubos=true|false
//
// # service_via_local_cbmr_driver=true|false
//
// # This config writes EFI_SIGNATURE_LIST TLS payload to a 'certlist.bin' file.
// # write_cert_list_to_file=true|false
//
// # Below UEFI variables configure the spew target and debug mask for baked in driver.
// # Helpful to reconfigure the baked in driver to dump debug prints on failures
//
// # setvar "CbmrDebugMask" -guid "887481f5-fa49-4f65-b03c-551db53c8c23" -bs -rt -nv =0x7
// # setvar "CbmrSpewTarget" -guid "887481f5-fa49-4f65-b03c-551db53c8c23" -bs -rt -nv =0x4
// # dmpstore "CbmrUefiLogs" -guid "887481f5-fa49-4f65-b03c-551db53c8c23"
//
// clang-format on

static EFI_STATUS CbmrReadConfigSection(_In_ CHAR8* ConfigSection, EFI_FILE_PROTOCOL* ConfigFile)
{
    EFI_STATUS Status = EFI_SUCCESS;
    CHAR8 Line[MAX_LINE_SIZE];
    UINTN LineLength = 0;
    BOOLEAN EndOfFile = FALSE;

    while (TRUE) {
        Line[0] = 0;
        LineLength = _countof(Line);
        Status = FileReadLine(ConfigFile, &LineLength, Line, &EndOfFile);
        if (EndOfFile == TRUE) {
            Status = EFI_SUCCESS;
            break;
        }

        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileReadLine() Failed 0x%zx", Status);
            goto Exit;
        }

        Status = AsciiStrTrimS(Line, _countof(Line));
        if (EFI_ERROR(Status)) {
            DBG_ERROR("AsciiStrTrimS() Failed 0x%zx", Status);
            goto Exit;
        }

        if (AsciiStrLen(Line) == 0) // Skip blank lines
            continue;

        if (Line[0] == '#') { // comment skip to next line
            continue;
        }

        if (AsciiStriCmp(Line, ConfigSection) == 0) {
            goto Exit;
        }
    }

    Status = EFI_NOT_FOUND;
Exit:
    return Status;
}

static EFI_STATUS CbmrReadSpewTargetDebugMaskUefiVariable()
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINTN BufferSize = 0;
    CHAR8* Buffer = NULL;

    Status = gRT->GetVariable(L"CbmrSpewTarget",
                              &(EFI_GUID)EFI_MS_CBMR_PROTOCOL_GUID,
                              NULL,
                              &BufferSize,
                              NULL);
    if (Status == EFI_BUFFER_TOO_SMALL) {
        Buffer = AllocateZeroPool(BufferSize);
        if (Buffer == NULL) {
            goto Exit;
        }

        gRT->GetVariable(L"CbmrSpewTarget",
                         &(EFI_GUID)EFI_MS_CBMR_PROTOCOL_GUID,
                         NULL,
                         &BufferSize,
                         Buffer);
        gCbmrConfig.SpewTarget = Buffer[0];
        FreePool(Buffer);
        Buffer = NULL;
        BufferSize = 0;
    }

    Status = gRT->GetVariable(L"CbmrDebugMask",
                              &(EFI_GUID)EFI_MS_CBMR_PROTOCOL_GUID,
                              NULL,
                              &BufferSize,
                              NULL);
    if (Status == EFI_BUFFER_TOO_SMALL) {
        Buffer = AllocateZeroPool(BufferSize);
        if (Buffer == NULL) {
            goto Exit;
        }

        gRT->GetVariable(L"CbmrDebugMask",
                         &(EFI_GUID)EFI_MS_CBMR_PROTOCOL_GUID,
                         NULL,
                         &BufferSize,
                         Buffer);
        gCbmrConfig.DebugMask = Buffer[0];
        FreePool(Buffer);
        Buffer = NULL;
        BufferSize = 0;
    }

Exit:

    FreePool(Buffer);
    return Status;
}

EFI_STATUS EFIAPI CbmrReadConfig(_In_ CHAR8* ConfigSection)
{
    EFI_STATUS Status = EFI_SUCCESS;

    UNREFERENCED_PARAMETER(ConfigSection);

    if (gCbmrConfig.Url == NULL) {
        gCbmrConfig.Url = AllocateZeroPool(MAX_JSON_REQUEST_URL_SIZE);
        if (gCbmrConfig.Url == NULL) {
            DBG_ERROR("Out of memory", NULL);
            Status = EFI_OUT_OF_RESOURCES;
            goto Exit;
        }
        StrnCpy(gCbmrConfig.Url,
                DEFAULT_DCAT_PROD_URL,
                StrnLenS(DEFAULT_DCAT_PROD_URL, _countof(DEFAULT_DCAT_PROD_URL)));
    }

#ifdef DEBUGMODE
    EFI_FILE_PROTOCOL* ConfigFile = NULL;
    CHAR8 Line[MAX_LINE_SIZE];
    UINTN LineLength = 0;
    BOOLEAN EndOfFile = FALSE;

    Status = FileLocateAndOpen(CBMR_CONFIG_FILENAME, EFI_FILE_MODE_READ, &ConfigFile);
    if (EFI_ERROR(Status)) {
        DBG_ERROR_U(L"%s not found", CBMR_CONFIG_FILENAME);
        Status = EFI_SUCCESS;
        goto Exit;
    }

    Status = CbmrReadConfigSection(ConfigSection, ConfigFile);
    if (EFI_ERROR(Status)) {
        DBG_ERROR("%s section not found", ConfigSection);
        goto Exit;
    }

    DBG_INFO_U(L"%s found. Reading %S section", CBMR_CONFIG_FILENAME, ConfigSection);
    while (!EndOfFile) {
        Line[0] = 0;
        LineLength = _countof(Line);
        Status = FileReadLine(ConfigFile, &LineLength, Line, &EndOfFile);
        if (EFI_ERROR(Status)) {
            DBG_ERROR("FileReadLine() Failed 0x%zx", Status);
            goto Exit;
        }

        Status = AsciiStrTrimS(Line, _countof(Line));
        if (EFI_ERROR(Status)) {
            DBG_ERROR("AsciiStrTrimS() Failed 0x%zx", Status);
            goto Exit;
        }

        if (AsciiStrLen(Line) == 0) // Skip blank lines
            continue;

        if (Line[0] == '#') { // Skip comment
            continue;
        }

        LineLength = AsciiStrnLenS(Line, _countof(Line));
        if (LineLength == _countof(Line)) {
            DBG_ERROR("Invalid line length", NULL);
            Status = EFI_INVALID_PARAMETER;
            goto Exit;
        }

        if (Line[0] == '[' && Line[LineLength - 1] == ']') { // Start of next section
            Status = EFI_SUCCESS;
            goto Exit;
        }

        if (AsciiStrStr(Line, t("mask=")) == Line) {
            CHAR8* Value = Line + STRING_LEN("mask=");
            gCbmrConfig.DebugMask = AsciiStrHexToUintn(Value);
        } else if (AsciiStrStr(Line, t("spew=")) == Line) {
            CHAR8* Value = Line + STRING_LEN("spew=");
            gCbmrConfig.SpewTarget = 0;

            if (AsciiStrStr(Value, t("console"))) {
                gCbmrConfig.SpewTarget |= SPEW_CONSOLE;
            }

            if (AsciiStrStr(Value, t("debugger"))) {
                gCbmrConfig.SpewTarget |= SPEW_DEBUGGER;
            }

            if (AsciiStrStr(Value, t("file"))) {
                gCbmrConfig.SpewTarget |= SPEW_FILE;
            }

            if (AsciiStrStr(Value, t("uefivar"))) {
                gCbmrConfig.SpewTarget |= SPEW_UEFI_VAR;
            }

            if (AsciiStrStr(Value, t("serial"))) {
                gCbmrConfig.SpewTarget |= SPEW_SERIAL;
            }
        } else if (AsciiStrStr(Line, t("early_break=")) == Line) {
            CHAR8* Value = Line + STRING_LEN("early_break=");

            if (AsciiStriCmp(Value, t("true")) == 0) {
                gCbmrConfig.EarlyBreak = TRUE;
            } else if (AsciiStriCmp(Value, t("false")) == 0) {
                gCbmrConfig.EarlyBreak = FALSE;
            } else {
                DBG_ERROR("Invalid value '%s' for config 'early_break'", Value);
                Status = EFI_INVALID_PARAMETER;
                goto Exit;
            }
        } else if (AsciiStrStr(Line, t("show_wifi_ux=")) == Line) {
            CHAR8* Value = Line + STRING_LEN("show_wifi_ux=");

            if (AsciiStriCmp(Value, t("true")) == 0) {
                gCbmrConfig.ShowWiFiUX = TRUE;
            } else if (AsciiStriCmp(Value, t("false")) == 0) {
                gCbmrConfig.ShowWiFiUX = FALSE;
            } else {
                DBG_ERROR("Invalid value '%s' for config 'show_wifi_ux'", Value);
                Status = EFI_INVALID_PARAMETER;
                goto Exit;
            }
        } else if (AsciiStrStr(Line, t("wifi_sid=")) == Line) {
            CHAR8* Value = Line + STRING_LEN("wifi_sid=");

            if (AsciiStrnLenS(Value, MAX_LINE_SIZE - STRING_LEN("wifi_sid=")) >=
                _countof(gCbmrConfig.WifiSid)) {
                DBG_ERROR("Value(%s) cannot be greater than %zu",
                          Value,
                          _countof(gCbmrConfig.WifiSid));
                Status = EFI_INVALID_PARAMETER;
                goto Exit;
            }

            AsciiStrCpyS(gCbmrConfig.WifiSid, _countof(gCbmrConfig.WifiSid), Value);
        } else if (AsciiStrStr(Line, t("wifi_password=")) == Line) {
            CHAR8* Value = Line + STRING_LEN("wifi_password=");

            if (AsciiStrnLenS(Value, MAX_LINE_SIZE - STRING_LEN("wifi_password=")) >=
                _countof(gCbmrConfig.WifiPassword)) {
                DBG_ERROR("Value(%s) cannot be greater than %zu",
                          Value,
                          _countof(gCbmrConfig.WifiPassword));
                Status = EFI_INVALID_PARAMETER;
                goto Exit;
            }

            AsciiStrCpyS(gCbmrConfig.WifiPassword, _countof(gCbmrConfig.WifiPassword), Value);
        } else if (AsciiStrStr(Line, t("url=")) == Line) {
            CHAR8* Value = Line + STRING_LEN("url=");

            if (AsciiStrnLenS(Value, MAX_LINE_SIZE - STRING_LEN("url=")) >=
                MAX_JSON_REQUEST_URL_SIZE) {
                DBG_ERROR("Value(%s) cannot be greater than %d", Value, MAX_JSON_REQUEST_URL_SIZE);
                Status = EFI_INVALID_PARAMETER;
                goto Exit;
            }

            AsciiStrToUnicodeStr(Value, gCbmrConfig.Url);

            // Check if URL ends with '/'. If not, add it. This facilitates
            // concatenation with relative server file paths later on.
            if (gCbmrConfig.Url[AsciiStrnLenS(Value, MAX_LINE_SIZE - STRING_LEN("url="))] != L'/') {
                gCbmrConfig
                    .Url[AsciiStrnLenS(Value, MAX_LINE_SIZE - STRING_LEN("url=")) + 1] = L'/';
            }
        } else if (AsciiStrStr(Line, t("endpoint_type=")) == Line) {
            CHAR8* Value = Line + STRING_LEN("endpoint_type=");

            if (AsciiStriCmp(Value, t("dcat")) == 0) {
                gCbmrConfig.EndpointType = CBMR_ENDPOINT_TYPE_DCAT;
            } else if (AsciiStriCmp(Value, t("http")) == 0) {
                gCbmrConfig.EndpointType = CBMR_ENDPOINT_TYPE_LOCAL_HTTP;
            } else if (AsciiStriCmp(Value, t("usbkey")) == 0) {
                gCbmrConfig.EndpointType = CBMR_ENDPOINT_TYPE_USBKEY;
            } else {
                DBG_ERROR("Invalid value '%s' for config 'endpoint_type'", Value);
                Status = EFI_INVALID_PARAMETER;
                goto Exit;
            }
        } else if (AsciiStrStr(Line, t("dcat_endpoint_type=")) == Line) {
            CHAR8* Value = Line + STRING_LEN("dcat_endpoint_type=");

            if (AsciiStriCmp(Value, CBMR_DCAT_ENDPOINT_TYPE_PROD_STR) == 0) {
                gCbmrConfig.DcatEndpointType = CBMR_DCAT_ENDPOINT_TYPE_PROD;
                StrnCpy(gCbmrConfig.Url,
                        DEFAULT_DCAT_PROD_URL,
                        StrnLenS(DEFAULT_DCAT_PROD_URL, _countof(DEFAULT_DCAT_PROD_URL)));
            } else if (AsciiStriCmp(Value, CBMR_DCAT_ENDPOINT_TYPE_PPE_STR) == 0) {
                gCbmrConfig.DcatEndpointType = CBMR_DCAT_ENDPOINT_TYPE_PPE;
                StrnCpy(gCbmrConfig.Url,
                        DEFAULT_DCAT_PPE_URL,
                        StrnLenS(DEFAULT_DCAT_PPE_URL, _countof(DEFAULT_DCAT_PPE_URL)));
            } else {
                DBG_ERROR("Invalid value '%s' for config 'dcat_endpoint_type'", Value);
                Status = EFI_INVALID_PARAMETER;
                goto Exit;
            }
        } else if (AsciiStrStr(Line, t("force_https=")) == Line) {
            CHAR8* Value = Line + STRING_LEN("force_https=");

            if (AsciiStriCmp(Value, t("true")) == 0) {
                gCbmrConfig.ForceHttps = TRUE;
            } else if (AsciiStriCmp(Value, t("false")) == 0) {
                gCbmrConfig.ForceHttps = FALSE;
            } else {
                DBG_ERROR("Invalid value '%s' for config 'force_https'", Value);
                Status = EFI_INVALID_PARAMETER;
                goto Exit;
            }
        } else if (AsciiStrStr(Line, t("skip_hash_validation=")) == Line) {
            CHAR8* Value = Line + STRING_LEN("skip_hash_validation=");

            if (AsciiStriCmp(Value, t("true")) == 0) {
                gCbmrConfig.SkipHashValidation = TRUE;
            } else if (AsciiStriCmp(Value, t("false")) == 0) {
                gCbmrConfig.SkipHashValidation = FALSE;
            } else {
                DBG_ERROR("Invalid value '%s' for config 'skip_hash_validation'", Value);
                Status = EFI_INVALID_PARAMETER;
                goto Exit;
            }
        } else if (AsciiStrStr(Line, t("write_si_uefi_variable=")) == Line) {
            CHAR8* Value = Line + STRING_LEN("write_si_uefi_variable=");

            if (AsciiStriCmp(Value, t("true")) == 0) {
                gCbmrConfig.WriteSiUefiVariable = TRUE;
            } else if (AsciiStriCmp(Value, t("false")) == 0) {
                gCbmrConfig.WriteSiUefiVariable = FALSE;
            } else {
                DBG_ERROR("Invalid value '%s' for config 'write_si_uefi_variable'", Value);
                Status = EFI_INVALID_PARAMETER;
                goto Exit;
            }
        } else if (AsciiStrStr(Line, t("write_si2_uefi_variable=")) == Line) {
            CHAR8* Value = Line + STRING_LEN("write_si2_uefi_variable=");

            if (AsciiStriCmp(Value, t("true")) == 0) {
                gCbmrConfig.WriteSi2UefiVariable = TRUE;
            } else if (AsciiStriCmp(Value, t("false")) == 0) {
                gCbmrConfig.WriteSi2UefiVariable = FALSE;
            } else {
                DBG_ERROR("Invalid value '%s' for config 'write_si2_uefi_variable'", Value);
                Status = EFI_INVALID_PARAMETER;
                goto Exit;
            }
        } else if (AsciiStrStr(Line, t("enable_test_signing_on_stubos=")) == Line) {
            CHAR8* Value = Line + STRING_LEN("enable_test_signing_on_stubos=");

            if (AsciiStriCmp(Value, t("true")) == 0) {
                gCbmrConfig.EnableTestSigningOnStubOS = TRUE;
            } else if (AsciiStriCmp(Value, t("false")) == 0) {
                gCbmrConfig.EnableTestSigningOnStubOS = FALSE;
            } else {
                DBG_ERROR("Invalid value '%s' for config 'enable_test_signing_on_stubos'", Value);
                Status = EFI_INVALID_PARAMETER;
                goto Exit;
            }
        } else if (AsciiStrStr(Line, t("service_via_local_cbmr_driver=")) == Line) {
            CHAR8* Value = Line + STRING_LEN("service_via_local_cbmr_driver=");

            if (AsciiStriCmp(Value, t("true")) == 0) {
                gCbmrConfig.ServiceViaLocalCbmrDriver = TRUE;
            } else if (AsciiStriCmp(Value, t("false")) == 0) {
                gCbmrConfig.ServiceViaLocalCbmrDriver = FALSE;
            } else {
                DBG_ERROR("Invalid value '%s' for config 'service_via_local_cbmr_driver'", Value);
                Status = EFI_INVALID_PARAMETER;
                goto Exit;
            }
        } else if (AsciiStrStr(Line, t("write_cert_list_to_file=")) == Line) {
            CHAR8* Value = Line + STRING_LEN("write_cert_list_to_file=");

            if (AsciiStriCmp(Value, t("true")) == 0) {
                gCbmrConfig.WriteCertListToFile = TRUE;
            } else if (AsciiStriCmp(Value, t("false")) == 0) {
                gCbmrConfig.WriteCertListToFile = FALSE;
            } else {
                DBG_ERROR("Invalid value '%s' for config 'service_via_local_cbmr_driver'", Value);
                Status = EFI_INVALID_PARAMETER;
                goto Exit;
            }
        }
    }

#endif
Exit:

    //
    // If no spew target(spew=) or debug mask(mask=) options are specified (as
    // it is the case for baked in CBMR driver),  fall back to reading
    // CbmrSpewTarget and CbmrDebugMask UEFI variables. As these variables will
    // become handy to control the debug messages during failures. So for a
    // baked in driver, if CBMR encounters a failure, by default no logs are
    // captured. By setting the below variables and rebooting the device, the
    // logs can be captured as below
    //
    // setvar "CbmrDebugMask" -guid "887481f5-fa49-4f65-b03c-551db53c8c23" -bs -rt -nv =0x7
    // setvar "CbmrSpewTarget" -guid "887481f5-fa49-4f65-b03c-551db53c8c23" -bs -rt -nv =0x4  # write logs to below uefi variable
    // dmpstore "CbmrUefiLogs" -guid "887481f5-fa49-4f65-b03c-551db53c8c23"
    //

    if (gCbmrConfig.SpewTarget == 0 || gCbmrConfig.DebugMask == 0) {
        CbmrReadSpewTargetDebugMaskUefiVariable();
    }

    if (EFI_ERROR(Status)) {
        CbmrFreeConfig();
    }

    return Status;
}

VOID EFIAPI CbmrFreeConfig()
{
    FreePool(gCbmrConfig.Url);
    gCbmrConfig.Url = NULL;
}

#ifndef UEFI_BUILD_SYSTEM
#pragma prefast(pop)
#endif
