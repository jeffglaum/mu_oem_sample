/** @file MsCbmrProcessSampleLib.h

  cBMR Process Sample Library

  Copyright (c) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  This library is intended to be a sample of how to support cBMR (Cloud Bare Metal Recovery) with this file
  specifically defining the API to initiate the process.
**/

#ifndef _CBMR_PROCESS_EXAMPLE_LIB__H_
#define _CBMR_PROCESS_EXAMPLE_LIB__H_

#include <Protocol/MsCloudBareMetalRecovery.h>


/**
  Primary entry point to the library to initiate the cBMR process

  @param[in]  UseWiFi           TRUE if the process should attempt to attach to a WiFi access point, FALSE for wired
  @param[in]  SSIdName          SSID string used to attach to the WiFi access point. May be NULL if UseWifi is FALSE.
  @param[in]  SSIdPwd           Password  string used to attach to the WiFi access point. May be NULL if UseWifi is FALSE.
  @param[in]  ProgressCallback  Callback function to receive progress information.  May be NULL to use this library's default handler.

  @retval     EFI_STATUS
**/
EFI_STATUS
EFIAPI
ExecuteCbmrProcess (
  IN BOOLEAN                        UseWiFi,
  IN CHAR8                          *SSIdName,          OPTIONAL
  IN CHAR8                          *SSIdPwd,           OPTIONAL
  IN EFI_MS_CBMR_PROGRESS_CALLBACK  ProgressCallback);  OPTIONAL

#endif // _CBMR_PROCESS_EXAMPLE_LIB__H_