/** @file MsCbmrSampleShellApp.c

  Copyright (c) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  cBMR Process Initiation Sample Shell Application
**/

#include <Uefi.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/ShellLib.h>
#include <Library/PrintLib.h>
#include <Protocol/Shell.h>

#include <Library/MsCbmrProcessSampleLib.h>

#define MAX_CMD_LINE_ARG_SIZE     128


/**
  Converts an Unicode command line argument to ASCII.  If the input Arg is longer than MAX_CMD_LINE_ARG_SIZE, the
  string is truncated and the output Arg buffer is still properly NULL terminated at the max length.

  @param[in]  UnicodeArgStr - Argument to convert/copy to the destination
  @param[in]  AsciiArgBuffer - Destination ASCII buffer that has at least MAX_CMD_LINE_ARG_SIZE bytes available
**/
VOID
EFIAPI
UnicodeArgToAsciiArgN (
  CONST CHAR16 *UnicodeArgStr,
  CHAR8        *AsciiArgBuffer)
{
  UINTN DstMax = MAX_CMD_LINE_ARG_SIZE - 1;
  UINTN x;

  for (x = 0; (x < DstMax) && (UnicodeArgStr[x] != 0x00); x++) {
    AsciiArgBuffer[x] = (CHAR8)UnicodeArgStr[x];
  }

  AsciiArgBuffer[x] = 0x00;
}

/**
  Shell app entry point.
**/
EFI_STATUS
EFIAPI
CbmrSampleShellAppEntry (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE*  SystemTable)
{
  EFI_SHELL_PARAMETERS_PROTOCOL *ShellParams;
  EFI_STATUS Status;
  CHAR8 AsciiArgV1[MAX_CMD_LINE_ARG_SIZE];
  CHAR8 AsciiArgV2[MAX_CMD_LINE_ARG_SIZE];

  //
  // Init app
  //

  Print (L"Cloud Bare Metal Recovery - Sample Process Shell Application\n");
  Print (L"Copyright (c) Microsoft Corporation. All rights reserved.\n\n");
  Status = gBS->HandleProtocol (ImageHandle, &gEfiShellParametersProtocolGuid, (VOID **)&ShellParams);
  ASSERT_EFI_ERROR(Status);

  //
  // Option 1) One argument of 'Wired' to indicate the app should use a wired connection
  //

  if (ShellParams->Argc == 2) {
    UnicodeArgToAsciiArgN (ShellParams->Argv[1], AsciiArgV1);

    if (0 == AsciiStriCmp ("Wired", AsciiArgV1)) {
      Print (L"Initating a wired connection download...\n");

      return ExecuteCbmrProcess (FALSE,
                                 NULL,
                                 NULL,
                                 NULL);          // NULL indicates use the sample library callback function
    }
  }

  //
  // Option 2) Two arguments indicate the app should use a wireless connection where Arg1 is SSID and Arg2 is password
  //

  if (ShellParams->Argc == 3) {
    Print (L"Initating a WiFi connection download...\n");
    Print (L"    SSID:      %s\n", ShellParams->Argv[1]);
    Print (L"    Password:  %s\n", ShellParams->Argv[2]);

    UnicodeArgToAsciiArgN (ShellParams->Argv[1], AsciiArgV1);
    UnicodeArgToAsciiArgN (ShellParams->Argv[2], AsciiArgV2);
    return ExecuteCbmrProcess (TRUE,
                               AsciiArgV1,
                               AsciiArgV2,
                               NULL);          // NULL indicates use the sample library callback function
  }

  //
  // Fall through, the command line is invalid
  //

  Print (L"Invalid command line parameters, expecting one of two choices:\n");
  Print (L"    '%s Wired'              Attempt cBMR with a wired connection\n", ShellParams->Argv[0]);
  Print (L"    '%s <SSID> <Password>'  Attempt cBMR using WIFI SSID & PWD\n\n", ShellParams->Argv[0]);
  return EFI_INVALID_PARAMETER;
}

