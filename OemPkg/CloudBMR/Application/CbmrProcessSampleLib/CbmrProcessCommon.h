/** @file CbmrProcessCommon.h

  cBMR Process Sample Library

  Copyright (c) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  This library is intended to be a sample of how to initiate the cBMR (Cloud Bare Metal Recovery) process.
**/

#include <Uefi.h>

#include <Uefi/UefiBaseType.h>

#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>

#include <Protocol/Ip4Config2.h>
#include <Protocol/Supplicant.h>
#include <Protocol/WiFi2.h>
#include <Protocol/Shell.h>

#include <Protocol/CloudBareMetalRecovery.h>
#include <Library/CbmrProcessSampleLib.h>


/**
  Primary function to initiate connection to a WiFi access point

  @param[in]  SSIdName      Network SSID to connect to
  @param[in]  SSIdPassword  ASCII string of password to use when connecting

  @retval     EFI_STATUS
**/
EFI_STATUS
EFIAPI
ConnectToWiFiAccessPoint (
  CHAR8  *SSIdName,
  CHAR8  *SSIdPassword);

/**
  Primary function to initiate connection to a network

  @retval  EFI_STATUS
**/
EFI_STATUS
EFIAPI
ConnectToNetwork (EFI_IP4_CONFIG2_INTERFACE_INFO **InterfaceInfo);



/**
  Locates the cBMR protocol and verifies the driver's revision matches the protocol being used in this compilation.

  @param[out] CbmrProtocol       Pointer to the cBMR protocol to use

  @retval     EFI_STATUS
**/
EFI_STATUS
EFIAPI
LocateCbmrProtocol(
  OUT EFI_MS_CBMR_PROTOCOL **CbmrProtocolPtr);

/**
  Sends the configuration block to the cBMR driver in preparation for the Stub-OS launch.

  @param[in]  CbmrProtocol      Pointer to the cBMR protocol to use
  @param[in]  UseWiFi           TRUE if the process should attempt to attach to a WiFi access point, FALSE for wired
  @param[in]  SSIdName          SSID string used to attach to the WiFi access point. May be NULL if UseWifi is FALSE.
  @param[in]  SSIdPassword      Password  string used to attach to the WiFi access point. May be NULL if UseWifi is FALSE.
  @param[in]  ProgressCallback  Callback function to receive progress information.  May be NULL to use this library's default handler.

  @retval     EFI_STATUS
**/
EFI_STATUS
EFIAPI
InitCbmrDriver (
  IN EFI_MS_CBMR_PROTOCOL           *CbmrProtocol,
  IN BOOLEAN                        UseWiFi,
  IN CHAR8                          *SSIdName,
  IN CHAR8                          *SSIdPassword,
  IN EFI_MS_CBMR_PROGRESS_CALLBACK  ProgressCallback);

/**
  Collects the collateral list from the cBMR protocol and returns the data in a buffer the caller is reponsible for
  freeing.

  @param[in]  CbmrProtocol       Pointer to the cBMR protocol to use
  @param[out] CollateralDataPtr  Returns a pointer to the allocated buffer containing the collateral list
  @param[out] CollateralCount    Number of collateral structures returned

  @retval     EFI_STATUS
**/
EFI_STATUS
EFIAPI
DownloadCbmrCollaterals(
  IN  EFI_MS_CBMR_PROTOCOL    *CbmrProtocol,
  OUT EFI_MS_CBMR_COLLATERAL  **CollateralDataPtr,
  OUT UINTN                   *CollateralCount);

/**
  Initiates the cBMR driver's Start command.  Since that command should not return if the Stub-OS sucessfully launches,
  this function should never return.

  @param[in]  CbmrProtocol       Pointer to the cBMR protocol to use

  @retval     EFI_STATUS
**/
EFI_STATUS
EFIAPI
LaunchStubOS (
  IN EFI_MS_CBMR_PROTOCOL  *CbmrProtocol);

