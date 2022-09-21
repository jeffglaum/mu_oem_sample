/*++

Copyright (c) 2021 Microsoft Corporation

Module Name:

    cbmr_protocols.c

Abstract:

    This module implements CBMR protocol readiness

Author:

    Vineel Kovvuri (vineelko) 6-Oct-2021

Environment:

    UEFI mode only.

--*/

#include "cbmrincludes.h"
#ifndef UEFI_BUILD_SYSTEM
#include "strsafe.h"
#endif
#include "network_common.h"

#include "protocols.h"
#include "error.h"

// clang-format off

enum CBMR_PROTOCOL_INDEX {

    //
    // BOOT MANAGER PROTOCOLS - Chapter 3
    //

    EFI_BOOT_MANAGER_POLICY_PROTOCOL_INDEX,

    //
    // BOOT SERVICES - Chapter 7
    //

    EFI_BOOT_SERVICES_PROTOCOL_INDEX,

    //
    // LOADED IMAGE PROTOCOL - Chapter 9
    //

    EFI_LOADED_IMAGE_PROTOCOL_INDEX,
    EFI_LOADED_IMAGE_DEVICE_PATH_PROTOCOL_INDEX,

    //
    // PROTOCOLS - DEVICE PATH - Chapter 10
    //

    EFI_DEVICE_PATH_PROTOCOL_INDEX,
    EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_INDEX,
    EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL_INDEX,
    EFI_DEVICE_PATH_UTILITIES_PROTOCOL_INDEX,

    //
    // PROTOCOLS - DRIVER BINDING - Chapter 11
    //

    EFI_DRIVER_BINDING_PROTOCOL_INDEX,
    EFI_PLATFORM_DRIVER_OVERRIDE_PROTOCOL_INDEX,
    EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_PROTOCOL_INDEX,
    EFI_DRIVER_DIAGNOSTICS2_PROTOCOL_INDEX,
    EFI_COMPONENT_NAME2_PROTOCOL_INDEX,
    EFI_PLATFORM_TO_DRIVER_CONFIGURATION_PROTOCOL_INDEX,
    EFI_DRIVER_SUPPORTED_EFI_VERSION_PROTOCOL_INDEX,
    EFI_DRIVER_FAMILY_OVERRIDE_PROTOCOL_INDEX,
    EFI_DRIVER_HEALTH_PROTOCOL_INDEX,
    EFI_ADAPTER_INFORMATION_PROTOCOL_INDEX,

    //
    // PROTOCOLS - CONSOLE - Chapter 12
    //

    EFI_SIMPLE_TEXT_INPUT_PROTOCOL_INDEX,
    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_INDEX,
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_INDEX,
    EFI_SIMPLE_POINTER_PROTOCOL_INDEX,
    EFI_ABSOLUTE_POINTER_PROTOCOL_INDEX,
    EFI_SERIAL_IO_PROTOCOL_INDEX,
    EFI_GRAPHICS_OUTPUT_PROTOCOL_INDEX,

    //
    // PROTOCOLS - MEDIA ACCESS - Chapter 13
    //

    EFI_LOAD_FILE_PROTOCOL_INDEX,
    EFI_LOAD_FILE2_PROTOCOL_INDEX,
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_INDEX,
    EFI_FILE_INFO_ID_PROTOCOL_INDEX,
    EFI_TAPE_IO_PROTOCOL_INDEX,
    EFI_DISK_IO_PROTOCOL_INDEX,
    EFI_DISK_IO2_PROTOCOL_INDEX,
    EFI_BLOCK_IO_PROTOCOL_INDEX,
    EFI_BLOCK_IO2_PROTOCOL_INDEX,
    EFI_BLOCK_IO_CRYPTO_PROTOCOL_INDEX,
    EFI_ERASE_BLOCK_PROTOCOL_INDEX,
    EFI_ATA_PASS_THRU_PROTOCOL_INDEX,
    EFI_STORAGE_SECURITY_COMMAND_PROTOCOL_INDEX,
    EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL_INDEX,
    EFI_SD_MMC_PASS_THRU_PROTOCOL_INDEX,
    EFI_RAM_DISK_PROTOCOL_INDEX,
    EFI_PARTITION_INFO_PROTOCOL_INDEX,
    EFI_NVDIMM_LABEL_PROTOCOL_INDEX,
    EFI_UFS_DEVICE_CONFIG_PROTOCOL_INDEX,

    //
    // PROTOCOLS - PCI BUS - Chapter 14
    //

    EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_INDEX,
    EFI_PCI_IO_PROTOCOL_INDEX,

    //
    // PROTOCOLS - SCSI Driver - Chapter 15
    //

    EFI_SCSI_IO_PROTOCOL_INDEX,
    EFI_EXT_SCSI_PASS_THRU_PROTOCOL_INDEX,

    //
    // PROTOCOLS - iSCSI Driver - Chapter 16
    //

    EFI_ISCSI_INITIATOR_NAME_PROTOCOL_INDEX,

    //
    // PROTOCOLS - USB - Chapter 17
    //

    EFI_USB2_HC_PROTOCOL_INDEX,
    EFI_USB_IO_PROTOCOL_INDEX,
    EFI_USBFN_IO_PROTOCOL_INDEX,
    EFI_USB_INIT_PROTOCOL_INDEX,

    //
    // PROTOCOLS - DEBUGGER - Chapter 18
    //

    EFI_DEBUGPORT_PROTOCOL_INDEX,
    EFI_DEBUG_SUPPORT_PROTOCOL_INDEX,

    //
    // PROTOCOLS - COMPRESSION - Chapter 19
    //

    EFI_DECOMPRESS_PROTOCOL_INDEX,

    //
    // PROTOCOLS - ACPI - Chapter 20
    //

    EFI_ACPI_TABLE_PROTOCOL_INDEX,

    //
    // PROTOCOLS - STRING SERVICES - Chapter 21
    //

    EFI_UNICODE_COLLATION_PROTOCOL_INDEX,
    EFI_REGULAR_EXPRESSION_PROTOCOL_INDEX,

    //
    // PROTOCOLS - BYTE CODE VM - Chapter 22
    //

    //
    // PROTOCOLS - FIRMWARE SERVICES - Chapter 23
    //

    EFI_SYSTEM_RESOURCE_TABLE_INDEX,

    //
    // PROTOCOLS - NETWORK - SNP, PXE, BIS and HTTP Boot - Chapter 24
    //

    EFI_SIMPLE_NETWORK_PROTOCOL_INDEX,
    EFI_NETWORK_INTERFACE_IDENTIFIER_PROTOCOL_INDEX,
    EFI_PXE_BASE_CODE_PROTOCOL_INDEX,
    EFI_PXE_BASE_CODE_CALLBACK_PROTOCOL_INDEX,
    EFI_BIS_PROTOCOL_INDEX,
    EFI_HTTP_BOOT_CALLBACK_PROTOCOL_INDEX,

    //
    // PROTOCOLS - NETWORK - Managed Network - Chapter 25
    //

    EFI_MANAGED_NETWORK_PROTOCOL_INDEX,

    //
    // PROTOCOLS - NETWORK - Bluetooth - Chapter 26
    //

    EFI_BLUETOOTH_HC_PROTOCOL_INDEX,
    EFI_BLUETOOTH_IO_PROTOCOL_INDEX,
    EFI_BLUETOOTH_CONFIG_PROTOCOL_INDEX,
    EFI_BLUETOOTH_ATTRIBUTE_PROTOCOL_INDEX,
    EFI_BLUETOOTH_LE_CONFIG_PROTOCOL_INDEX,

    //
    // PROTOCOLS - NETWORK - VLAN, EAP, Wi-Fi and Supplicant - Chapter 27
    //

    EFI_VLAN_CONFIG_PROTOCOL_INDEX,
    EFI_EAP_PROTOCOL_INDEX,
    EFI_EAP_MANAGEMENT_PROTOCOL_INDEX,
    EFI_EAP_MANAGEMENT2_PROTOCOL_INDEX,
    EFI_EAP_CONFIGURATION_PROTOCOL_INDEX,
    EFI_WIRELESS_MAC_CONNECTION_PROTOCOL_INDEX,
    EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL_INDEX,
    EFI_SUPPLICANT_PROTOCOL_INDEX,

    //
    // PROTOCOLS - NETWORK - TCP, IP, IPsec, FTP, TLS and Configurations - Chapter 28
    //

    EFI_TCP4_PROTOCOL_INDEX,
    EFI_TCP6_PROTOCOL_INDEX,
    EFI_IP4_PROTOCOL_INDEX,
    EFI_IP4_CONFIG_PROTOCOL_INDEX,
    EFI_IP4_CONFIG2_PROTOCOL_INDEX,
    EFI_IP6_PROTOCOL_INDEX,
    EFI_IP6_CONFIG_PROTOCOL_INDEX,
    EFI_IPSEC_CONFIG_PROTOCOL_INDEX,
    EFI_IPSEC_PROTOCOL_INDEX,
    EFI_IPSEC2_PROTOCOL_INDEX,
    EFI_FTP4_PROTOCOL_INDEX,
    EFI_TLS_PROTOCOL_INDEX,
    EFI_TLS_CONFIGURATION_PROTOCOL_INDEX,

    //
    // PROTOCOLS - NETWORK - ARP, DHCP, DNS, HTTP and REST - Chapter 29
    //

    EFI_ARP_PROTOCOL_INDEX,
    EFI_DHCP4_PROTOCOL_INDEX,
    EFI_DHCP6_PROTOCOL_INDEX,
    EFI_DNS4_PROTOCOL_INDEX,
    EFI_DNS6_PROTOCOL_INDEX,
    EFI_HTTP_PROTOCOL_INDEX,
    EFI_HTTP_UTILITIES_PROTOCOL_INDEX,
    EFI_REST_PROTOCOL_INDEX,
    EFI_REST_EX_PROTOCOL_INDEX,
    EFI_REST_JSON_STRUCTURE_PROTOCOL_INDEX,

    //
    // PROTOCOLS - NETWORK - UDP and MTFTP - Chapter 30
    //

    EFI_UDP4_PROTOCOL_INDEX,
    EFI_UDP6_PROTOCOL_INDEX,
    EFI_MTFTP4_PROTOCOL_INDEX,
    EFI_MTFTP6_PROTOCOL_INDEX,

    //
    // PROTOCOLS - HII - Chapter 34
    //

    EFI_HII_FONT_PROTOCOL_INDEX,
    EFI_HII_FONT_EX_PROTOCOL_INDEX,
    EFI_HII_STRING_PROTOCOL_INDEX,
    EFI_HII_IMAGE_PROTOCOL_INDEX,
    EFI_HII_IMAGE_EX_PROTOCOL_INDEX,
    EFI_HII_IMAGE_DECODER_PROTOCOL_INDEX,
    EFI_HII_FONT_GLYPH_GENERATOR_PROTOCOL_INDEX,
    EFI_HII_DATABASE_PROTOCOL_INDEX,
    EFI_CONFIG_KEYWORD_HANDLER_PROTOCOL_INDEX,
    EFI_HII_CONFIG_ROUTING_PROTOCOL_INDEX,
    EFI_HII_CONFIG_ACCESS_PROTOCOL_INDEX,
    EFI_FORM_BROWSER2_PROTOCOL_INDEX,
    EFI_HII_POPUP_PROTOCOL_INDEX,
    EFI_HII_PACKAGE_LIST_PROTOCOL_INDEX,

    //
    // PROTOCOLS - Secure Technologies - Chapter 37
    //

    EFI_HASH_PROTOCOL_INDEX,
    EFI_HASH2_PROTOCOL_INDEX,
    EFI_KEY_MANAGEMENT_SERVICE_PROTOCOL_INDEX,
    EFI_PKCS7_VERIFY_PROTOCOL_INDEX,
    EFI_RNG_PROTOCOL_INDEX,
    EFI_SMART_CARD_READER_PROTOCOL_INDEX,

    //
    // PROTOCOLS - Secure Technologies - Chapter 38
    //

    EFI_TIMESTAMP_PROTOCOL_INDEX,
    EFI_RESET_NOTIFICATION_PROTOCOL_INDEX,

    //
    // MISCELLANOUS PROTOCOLS
    //

    EFI_SMBIOS_PROTOCOL_INDEX,
    EFI_SHELL_PROTOCOL_INDEX,

    //
    // NON STANDARD PROTOCOLS
    //

    EDKII_VARIABLE_LOCK_PROTOCOL_INDEX,

    EFI_MAX_PROTOCOL_INDEX,
};

EFI_GUID NullGuid = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0} };

PROTOCOL_INFO CbmrProtocolArray[] = {

    PROTO(&gEfiDevicePathFromTextProtocolGuid,               t("EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL")),
    PROTO(&gEfiDevicePathProtocolGuid,                       t("EFI_DEVICE_PATH_PROTOCOL")),
    PROTO(&gEfiDevicePathToTextProtocolGuid,                 t("EFI_DEVICE_PATH_TO_TEXT_PROTOCOL")),
    PROTO(&gEfiDevicePathUtilitiesProtocolGuid,              t("EFI_DEVICE_PATH_UTILITIES_PROTOCOL")),
    PROTO(&gEfiDriverBindingProtocolGuid,                    t("EFI_DRIVER_BINDING_PROTOCOL")),
    SB_PROTO(&gEfiHttpProtocolGuid,                          t("EFI_HTTP_PROTOCOL"),                    &gEfiHttpServiceBindingProtocolGuid,               t("EFI_HTTP_SERVICE_BINDING_PROTOCOL")),
    PROTO(&gEfiIp4Config2ProtocolGuid,                       t("EFI_IP4_CONFIG2_PROTOCOL")),
    SB_PROTO(&gEfiIp4ProtocolGuid,                           t("EFI_IP4_PROTOCOL"),                     &gEfiIp4ServiceBindingProtocolGuid,                t("EFI_IP4_SERVICE_BINDING_PROTOCOL")),
    PROTO(&gEfiLoadedImageProtocolGuid,                      t("EFI_LOADED_IMAGE_PROTOCOL")),
    PROTO(&gEfiSimpleFileSystemProtocolGuid,                 t("EFI_SIMPLE_FILE_SYSTEM_PROTOCOL")),
    PROTO(&gEfiSimpleTextOutProtocolGuid,                    t("EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL")),
    SB_PROTO(&gEfiTcp4ProtocolGuid,                          t("EFI_TCP4_PROTOCOL"),                    &gEfiTcp4ServiceBindingProtocolGuid,               t("EFI_TCP4_SERVICE_BINDING_PROTOCOL")),
    SB_PROTO(&gEfiTlsProtocolGuid,                           t("EFI_TLS_PROTOCOL"),                     &gEfiTlsServiceBindingProtocolGuid,                t("EFI_TLS_SERVICE_BINDING_PROTOCOL")),
    PROTO(&gEfiTlsConfigurationProtocolGuid,                 t("EFI_TLS_CONFIGURATION_PROTOCOL")),
    PROTO(&gEfiRamDiskProtocolGuid,                          t("EFI_RAM_DISK_PROTOCOL")),


    // PROTO(&gEfiWiFi2ProtocolGuid,                         t("EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL")),
    // PROTO(&gEfiSupplicantProtocolGuid,                    t("EFI_SUPPLICANT_PROTOCOL")),
    // PROTO(&gEfiLoadedImageDevicePathProtocolGuid,         t("EFI_LOADED_IMAGE_DEVICE_PATH_PROTOCOL")),
    // PROTO(&gEfiBlockIo2ProtocolGuid,                      t("EFI_BLOCK_IO2_PROTOCOL")),
    // PROTO(&gsEdk2VariableLockProtocolGuid,                t("EDKII_VARIABLE_LOCK_PROTOCOL")),
    // PROTO(&gEfiGraphicsOutputProtocolGuid,                t("EFI_GRAPHICS_OUTPUT_PROTOCOL")),
    // PROTO(&gEfiAbsolutePointerProtocolGuid,               t("EFI_ABSOLUTE_POINTER_PROTOCOL")),
    // PROTO(&gEfiAcpiTableProtocolGuid,                     t("EFI_ACPI_TABLE_PROTOCOL")),
    // PROTO(&gEfiAdapterInformationProtocolGuid,            t("EFI_ADAPTER_INFORMATION_PROTOCOL")),
    // SB_PROTO(&gEfiArpProtocolGuid,                        t("EFI_ARP_PROTOCOL"),                     &gEfiArpServiceBindingProtocolGuid,                t("EFI_ARP_SERVICE_BINDING_PROTOCOL")),
    // PROTO(&gEfiAtaPassThruProtocolGuid,                   t("EFI_ATA_PASS_THRU_PROTOCOL")),
    // PROTO(&gEfiBisProtocolGuid,                           t("EFI_BIS_PROTOCOL")),
    // PROTO(&gEfiBlockIoCryptoProtocolGuid,                 t("EFI_BLOCK_IO_CRYPTO_PROTOCOL")),
    // PROTO(&gEfiBlockIoProtocolGuid,                       t("EFI_BLOCK_IO_PROTOCOL")),
    // SB_PROTO(&gEfiBluetoothAttributeProtocolGuid,         t("EFI_BLUETOOTH_ATTRIBUTE_PROTOCOL"),     &gEfiBluetoothAttributeServiceBindingProtocolGuid, t("EFI_BLUETOOTH_ATTRIBUTE_SERVICE_BINDING_PROTOCOL")),
    // PROTO(&gEfiBluetoothConfigProtocolGuid,               t("EFI_BLUETOOTH_CONFIG_PROTOCOL")),
    // PROTO(&gEfiBluetoothHcProtocolGuid,                   t("EFI_BLUETOOTH_HC_PROTOCOL")),
    // SB_PROTO(&gEfiBluetoothIoProtocolGuid,                t("EFI_BLUETOOTH_IO_PROTOCOL"),            &gEfiBluetoothIoServiceBindingProtocolGuid,        t("EFI_BLUETOOTH_IO_SERVICE_BINDING_PROTOCOL")),
    // PROTO(&gEfiBluetoothLeConfigProtocolGuid,             t("EFI_BLUETOOTH_LE_CONFIG_PROTOCOL")),
    // PROTO(&gEfiBootManagerPolicyProtocolGuid,             t("EFI_BOOT_MANAGER_POLICY_PROTOCOL")),
    // PROTO(&gEfiBusSpecificDriverOverrideProtocolGuid,     t("EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_PROTOCOL")),
    // PROTO(&gEfiComponentName2ProtocolGuid,                t("EFI_COMPONENT_NAME2_PROTOCOL")),
    // PROTO(&gEfiConfigKeywordHandlerProtocolGuid,          t("EFI_CONFIG_KEYWORD_HANDLER_PROTOCOL")),
    // PROTO(&gEfiDebugSupportProtocolGuid,                  t("EFI_DEBUG_SUPPORT_PROTOCOL")),
    // PROTO(&gEfiDebugPortProtocolGuid,                     t("EFI_DEBUGPORT_PROTOCOL")),
    // PROTO(&gEfiDecompressProtocolGuid,                    t("EFI_DECOMPRESS_PROTOCOL")),
    // SB_PROTO(&gEfiDhcp4ProtocolGuid,                      t("EFI_DHCP4_PROTOCOL"),                   &gEfiDhcp4ServiceBindingProtocolGuid,              t("EFI_DHCP4_SERVICE_BINDING_PROTOCOL")),
    // SB_PROTO(&gEfiDhcp6ProtocolGuid,                      t("EFI_DHCP6_PROTOCOL"),                   &gEfiDhcp6ServiceBindingProtocolGuid,              t("EFI_DHCP6_SERVICE_BINDING_PROTOCOL")),
    // PROTO(&gEfiDiskIo2ProtocolGuid,                       t("EFI_DISK_IO2_PROTOCOL")),
    // PROTO(&gEfiDiskIoProtocolGuid,                        t("EFI_DISK_IO_PROTOCOL")),
    // SB_PROTO(&gEfiDns4ProtocolGuid,                       t("EFI_DNS4_PROTOCOL"),                    &gEfiDns4ServiceBindingProtocolGuid,               t("EFI_DNS4_SERVICE_BINDING_PROTOCOL")),
    // SB_PROTO(&gEfiDns6ProtocolGuid,                       t("EFI_DNS6_PROTOCOL"),                    &gEfiDns6ServiceBindingProtocolGuid,               t("EFI_DNS6_SERVICE_BINDING_PROTOCOL")),
    // PROTO(&gEfiDriverDiagnostics2ProtocolGuid,            t("EFI_DRIVER_DIAGNOSTICS2_PROTOCOL")),
    // PROTO(&gEfiDriverFamilyOverrideProtocolGuid,          t("EFI_DRIVER_FAMILY_OVERRIDE_PROTOCOL")),
    // PROTO(&gEfiDriverHealthProtocolGuid,                  t("EFI_DRIVER_HEALTH_PROTOCOL")),
    // PROTO(&gEfiDriverSupportedEfiVersionProtocolGuid,     t("EFI_DRIVER_SUPPORTED_EFI_VERSION_PROTOCOL")),
    // PROTO(&gEfiEapConfigurationProtocolGuid,              t("EFI_EAP_CONFIGURATION_PROTOCOL")),
    // PROTO(&gEfiEapManagement2ProtocolGuid,                t("EFI_EAP_MANAGEMENT2_PROTOCOL")),
    // PROTO(&gEfiEapManagementProtocolGuid,                 t("EFI_EAP_MANAGEMENT_PROTOCOL")),
    // PROTO(&gEfiEapProtocolGuid,                           t("EFI_EAP_PROTOCOL")),
    // PROTO(&gEfiEraseBlockProtocolGuid,                    t("EFI_ERASE_BLOCK_PROTOCOL")),
    // PROTO(&gEfiExtScsiPassThruProtocolGuid,               t("EFI_EXT_SCSI_PASS_THRU_PROTOCOL")),
    // PROTO(&gEfiFileInfoGuid,                              t("EFI_FILE_INFO_ID")),
    // PROTO(&gEfiFormBrowser2ProtocolGuid,                  t("EFI_FORM_BROWSER2_PROTOCOL")),
    // SB_PROTO(&gEfiFtp4ProtocolGuid,                       t("EFI_FTP4_PROTOCOL"),                    &gEfiFtp4ServiceBindingProtocolGuid,               t("EFI_FTP4_SERVICE_BINDING_PROTOCOL")),
    // SB_PROTO(&gEfiHash2ProtocolGuid,                      t("EFI_HASH2_PROTOCOL"),                   &gEfiHash2ServiceBindingProtocolGuid,              t("EFI_HASH2_SERVICE_BINDING_PROTOCOL")),
    // SB_PROTO(&gEfiHashProtocolGuid,                       t("EFI_HASH_PROTOCOL"),                    &gEfiHashServiceBindingProtocolGuid,               t("EFI_HASH_SERVICE_BINDING_PROTOCOL")),
    // PROTO(&gEfiHiiConfigAccessProtocolGuid,               t("EFI_HII_CONFIG_ACCESS_PROTOCOL")),
    // PROTO(&gEfiHiiConfigRoutingProtocolGuid,              t("EFI_HII_CONFIG_ROUTING_PROTOCOL")),
    // PROTO(&gEfiHiiDatabaseProtocolGuid,                   t("EFI_HII_DATABASE_PROTOCOL")),
    // PROTO(&gEfiHiiFontExProtocolGuid,                     t("EFI_HII_FONT_EX_PROTOCOL")),
    // PROTO(&gEfiHiiFontGlyphGeneratorProtocolGuid,         t("EFI_HII_FONT_GLYPH_GENERATOR_PROTOCOL")),
    // PROTO(&gEfiHiiFontProtocolGuid,                       t("EFI_HII_FONT_PROTOCOL")),
    // PROTO(&gEfiHiiImageDecoderProtocolGuid,               t("EFI_HII_IMAGE_DECODER_PROTOCOL")),
    // PROTO(&gEfiHiiImageExProtocolGuid,                    t("EFI_HII_IMAGE_EX_PROTOCOL")),
    // PROTO(&gEfiHiiImageProtocolGuid,                      t("EFI_HII_IMAGE_PROTOCOL")),
    // PROTO(&gEfiHiiPopupProtocolGuid,                      t("EFI_HII_POPUP_PROTOCOL")),
    // PROTO(&gEfiHiiStringProtocolGuid,                     t("EFI_HII_STRING_PROTOCOL")),
    // PROTO(&gEfiHttpBootCallbackProtocolGuid,              t("EFI_HTTP_BOOT_CALLBACK_PROTOCOL")),
    // PROTO(&gEfiHttpUtilitiesProtocolGuid,                 t("EFI_HTTP_UTILITIES_PROTOCOL")),
    // PROTO(&gEfiIp4ConfigProtocolGuid,                     t("EFI_IP4_CONFIG_PROTOCOL")),
    // PROTO(&gEfiIp6ConfigProtocolGuid,                     t("EFI_IP6_CONFIG_PROTOCOL")),
    // SB_PROTO(&gEfiIp6ProtocolGuid,                        t("EFI_IP6_PROTOCOL"),                     &gEfiIp6ServiceBindingProtocolGuid,                t("EFI_IP6_SERVICE_BINDING_PROTOCOL")),
    // PROTO(&gEfiIpsec2ProtocolGuid,                        t("EFI_IPSEC2_PROTOCOL")),
    // PROTO(&gEfiIpsecConfigProtocolGuid,                   t("EFI_IPSEC_CONFIG_PROTOCOL")),
    // PROTO(&gEfiIpsecProtocolGuid,                         t("EFI_IPSEC_PROTOCOL")),
    // PROTO(&gEfiIscsiInitiatorNameProtocolGuid,            t("EFI_ISCSI_INITIATOR_NAME_PROTOCOL")),
    // PROTO(&gEfiKmsProtocolGuid,                           t("EFI_KEY_MANAGEMENT_SERVICE_PROTOCOL")),
    // PROTO(&gEfiLoadFile2ProtocolGuid,                     t("EFI_LOAD_FILE2_PROTOCOL")),
    // PROTO(&gEfiLoadFileProtocolGuid,                      t("EFI_LOAD_FILE_PROTOCOL")),
    // SB_PROTO(&gEfiManagedNetworkProtocolGuid,             t("EFI_MANAGED_NETWORK_PROTOCOL"),         &gEfiManagedNetworkServiceBindingProtocolGuid,     t("EFI_MANAGED_NETWORK_SERVICE_BINDING_PROTOCOL")),
    // SB_PROTO(&gEfiMtftp4ProtocolGuid,                     t("EFI_MTFTP4_PROTOCOL"),                  &gEfiMtftp4ServiceBindingProtocolGuid,             t("EFI_MTFTP4_SERVICE_BINDING_PROTOCOL")),
    // SB_PROTO(&gEfiMtftp6ProtocolGuid,                     t("EFI_MTFTP6_PROTOCOL"),                  &gEfiMtftp4ServiceBindingProtocolGuid,             t("EFI_MTFTP4_SERVICE_BINDING_PROTOCOL")),
    // PROTO(&gEfiNetworkInterfaceIdentifierProtocolGuid,    t("EFI_NETWORK_INTERFACE_IDENTIFIER_PROTOCOL")),
    // PROTO(&gEfiNvdimmLabelProtocolGuid,                   t("EFI_NVDIMM_LABEL_PROTOCOL")),
    // PROTO(&gEfiNvmExpressPassThruProtocolGuid,            t("EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL")),
    // PROTO(&gEfiPartitionInfoProtocolGuid,                 t("EFI_PARTITION_INFO_PROTOCOL")),
    // PROTO(&gEfiPciIoProtocolGuid,                         t("EFI_PCI_IO_PROTOCOL")),
    // PROTO(&gEfiPciRootBridgeIoProtocolGuid,               t("EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL")),
    // PROTO(&gEfiPkcs7VerifyProtocolGuid,                   t("EFI_PKCS7_VERIFY_PROTOCOL")),
    // PROTO(&gEfiPlatformDriverOverrideProtocolGuid,        t("EFI_PLATFORM_DRIVER_OVERRIDE_PROTOCOL")),
    // PROTO(&gEfiPlatformToDriverConfigurationProtocolGuid, t("EFI_PLATFORM_TO_DRIVER_CONFIGURATION_PROTOCOL")),
    // PROTO(&gEfiPxeBaseCodeCallbackProtocolGuid,           t("EFI_PXE_BASE_CODE_CALLBACK_PROTOCOL")),
    // PROTO(&gEfiPxeBaseCodeProtocolGuid,                   t("EFI_PXE_BASE_CODE_PROTOCOL")),
    // PROTO(&gEfiRegularExpressionProtocolGuid,             t("EFI_REGULAR_EXPRESSION_PROTOCOL")),
    // PROTO(&gEfiResetNotificationProtocolGuid,             t("EFI_RESET_NOTIFICATION_PROTOCOL")),
    // SB_PROTO(&gEfiRestExProtocolGuid,                     t("EFI_REST_EX_PROTOCOL"),                 &gEfiRestExServiceBindingProtocolGuid,             t("EFI_REST_EX_SERVICE_BINDING_PROTOCOL")),
    // PROTO(&gEfiRestJsonStructureProtocolGuid,             t("EFI_REST_JSON_STRUCTURE_PROTOCOL")),
    // PROTO(&gEfiRestProtocolGuid,                          t("EFI_REST_PROTOCOL")),
    // PROTO(&gEfiRngProtocolGuid,                           t("EFI_RNG_PROTOCOL")),
    // PROTO(&gEfiScsiIoProtocolGuid,                        t("EFI_SCSI_IO_PROTOCOL")),
    // PROTO(&gEfiSdMmcPassThruProtocolGuid,                 t("EFI_SD_MMC_PASS_THRU_PROTOCOL")),
    // PROTO(&gEfiSerialIoProtocolGuid,                      t("EFI_SERIAL_IO_PROTOCOL")),
    // PROTO(&gEfiShellProtocolGuid,                         t("EFI_SHELL_PROTOCOL")),
    // PROTO(&gEfiSimpleNetworkProtocolGuid,                 t("EFI_SIMPLE_NETWORK_PROTOCOL")),
    // PROTO(&gEfiSimplePointerProtocolGuid,                 t("EFI_SIMPLE_POINTER_PROTOCOL")),
    // PROTO(&gEfiSimpleTextInputExProtocolGuid,             t("EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL")),
    // PROTO(&gEfiSimpleTextInputProtocolGuid,               t("EFI_SIMPLE_TEXT_INPUT_PROTOCOL")),
    // PROTO(&gEfiSmartCardReaderProtocolGuid,               t("EFI_SMART_CARD_READER_PROTOCOL")),
    // PROTO(&gEfiSmbiosProtocolGuid,                        t("EFI_SMBIOS_PROTOCOL")),
    // PROTO(&gEfiStorageSecurityCommandProtocolGuid,        t("EFI_STORAGE_SECURITY_COMMAND_PROTOCOL")),
    // PROTO(&gEfiSystemResourceTableGuid,                   t("EFI_SYSTEM_RESOURCE_TABLE")),
    // PROTO(&gEfiTapeIoProtocolGuid,                        t("EFI_TAPE_IO_PROTOCOL")),
    // SB_PROTO(&gEfiTcp6ProtocolGuid,                       t("EFI_TCP6_PROTOCOL"),                    &gEfiTcp6ServiceBindingProtocolGuid,               t("EFI_TCP6_SERVICE_BINDING_PROTOCOL")),
    // PROTO(&gEfiTimeStampProtocolGuid,                     t("EFI_TIMESTAMP_PROTOCOL")),
    // SB_PROTO(&gEfiUdp4ProtocolGuid,                       t("EFI_UDP4_PROTOCOL"),                    &gEfiUdp4ServiceBindingProtocolGuid,               t("EFI_UDP4_SERVICE_BINDING_PROTOCOL")),
    // SB_PROTO(&gEfiUdp6ProtocolGuid,                       t("EFI_UDP6_PROTOCOL"),                    &gEfiUdp6ServiceBindingProtocolGuid,               t("EFI_UDP6_SERVICE_BINDING_PROTOCOL")),
    // PROTO(&gEfiUfsDeviceConfigProtocolGuid,               t("EFI_UFS_DEVICE_CONFIG_PROTOCOL")),
    // PROTO(&gEfiUnicodeCollationProtocol2Guid,             t("EFI_UNICODE_COLLATION_PROTOCOL")),
    // PROTO(&gEfiUsb2HcProtocolGuid,                        t("EFI_USB2_HC_PROTOCOL")),
    // PROTO(&gEfiUsbInitProtocolGuid,                       t("EFI_USB_INIT_PROTOCOL")),
    // PROTO(&gEfiUsbIoProtocolGuid,                         t("EFI_USB_IO_PROTOCOL")),
    // PROTO(&gEfiUsbfnIoProtocolGuid,                       t("EFI_USBFN_IO_PROTOCOL")),
    // PROTO(&gEfiVlanConfigProtocolGuid,                    t("EFI_VLAN_CONFIG_PROTOCOL")),
    // PROTO(&gEfiWiFi2ProtocolGuid,                         t("EFI_WIRELESS_MAC_CONNECTION_PROTOCOL")),
};

// clang-format on

EFI_STATUS
EFIAPI
CbmrProtocolProbeAll(VOID)
{
    EFI_STATUS Status = EFI_SUCCESS;

    NetworkCommonInitStack();

    //
    // Probe all required CBMR protocols
    //

    for (UINTN Index = 0; Index < _countof(CbmrProtocolArray); Index++) {
        ProtocolGetInfo(&CbmrProtocolArray[Index]);
    }

    //
    // Capture/Dump all the failures
    //

    for (UINTN Index = 0; Index < _countof(CbmrProtocolArray); Index++) {
        PROTOCOL_INFO* ProtocolInfo = &CbmrProtocolArray[Index];

        if (ProtocolInfo->ProtocolGuid == NULL)
            continue;

        if (EFI_ERROR(ProtocolInfo->ProtocolStatus)) {
            DBG_ERROR("%-45s Not Supported", ProtocolInfo->ProtocolName);
            if (ProtocolInfo->ServiceBindingProtocolName != NULL &&
                EFI_ERROR(ProtocolInfo->ServiceBindingProtocolStatus)) {
                DBG_ERROR("%-45s Not Supported", ProtocolInfo->ServiceBindingProtocolName);
            }

            Status = EFI_NOT_FOUND;
        }
    }

    //
    // Close all service binding protocols
    //

    for (UINTN Index = 0; Index < _countof(CbmrProtocolArray); Index++) {
        ProtocolServiceBindingClose(&CbmrProtocolArray[Index]);
    }

    return Status;
}
