/** @file -- AbsoluteConfigDxe.inf

  This module is a sample DXE driver to install the Absolute configuration policy and initialize the feature.
  Search for the text "OEM TODO" to find all locations that need examination prior to ingestion.

  Copyright (C) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>

#include <AbtVariables.h>
#include <Protocol/AbtConfiguration.h>
#include <Protocol/AbtVariableLockProvider.h>

// Forward declaration
EFI_STATUS EFIAPI ClearAllAbsoluteVariables ();


//
// Absolute configuration protocol
//

ABT_CONFIGURATION_PROTOCOL AbtConfig = {
  ABT_CONFIGURATION_FLAGS_LAUNCHER,     // ::Flags      Launcher directs ABT DXE driver to automatically launch the Agent Installer
  0x00000000,                           // ::Reserved   Set to 0x00
  ABT_SIGNATURE                         // ::Signature  "ABSOLUTE"
};


/**
  The module Entry Point of the Absolute Configuration DXE Driver

  @param[in]  ImageHandle    The firmware allocated handle for the EFI image
  @param[in]  SystemTable    A pointer to the EFI System Table

  @retval     EFI_SUCCESS    The entry point is executed successfully.
  @retval     Other          Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
AbsoluteConfigDxeEntry(
  IN  EFI_HANDLE        ImageHandle,
  IN  EFI_SYSTEM_TABLE  *SystemTable)
{
  EFI_STATUS Status;

  DEBUG ((DEBUG_INFO, "[ABT Config] DXE Driver Entry\n"));

  //
  // [ OEM TO_DO ] - Supress intallation of the policy
  //                 This is a good place to block installation of the policy for situations where Absolute should not
  //                 be supported for a specific boot.  For instance when in the manufacturing process.
  //
#if 0
  if ( <Not Supported> ) {
    DEBUG ((DEBUG_INFO, "[ABT Config] Bypassing initialization\n"));
    return EFI_SUCCESS;
  }
#endif

  //
  // [ OEM TO_DO ] - Clear Absolute variables
  //                 This is a good place to check for a boot where all Absolute persistence variables should be
  //                 cleared from Variable Services.  For instance a boot process where a customer return needs
  //                 removal of customer data.
  //
#if 0
  if ( < Variable Clear Necessary > ) {
    Status = ClearAllAbsoluteVariables();
    if (EFI_ERROR(Status)) {
      DEBUG ((DEBUG_INFO, "[ABT Config] Clear of all variables failed, Status = %r\n", Status));
    }
    return Status;
  }
#endif

  //
  // Install the ABT configuration protocol.
  //

  Status = gBS->InstallProtocolInterface (&ImageHandle,
                                          &gAbtConfigurationProtocolGuid,
                                          EFI_NATIVE_INTERFACE,
                                          &AbtConfig);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[ABT Config] Failed to install the AbtConfigurationProtocol, Status = %r\n", Status));
  }
  DEBUG ((DEBUG_INFO, "[ABT Config] DXE Driver Exit, Status = %r\n", Status));
  return Status;
}

