/** @file -- AbsoluteConfigDxe.inf

  This module is a sample DXE driver to install the Absolute configuration policy and initialize the feature.
  Search for the text "OEM TODO" to find all locations that need examination prior to ingestion.

  Copyright (C) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>

#include <AbtVariables.h>
#include <Protocol/AbtConfiguration.h>
#include <Protocol/AbtVariableLockProvider.h>


//
// Structure used in the linked list to track ABT variable names found in variable services
//

typedef struct _VAR_NAME_LIST_ENTRY {
  LIST_ENTRY ListEntry;   // LIST_ENTRY used in the BaseLib link list handlers
  CHAR16 *Name;           // Allocated buffer containing the name of the variable
} VAR_NAME_LIST_ENTRY;


/**
  Wrapper for the runtime service call GetNextVariableName that will reallocate the in/out string buffer if needed.

  @param[in out]  BufferSize   Pointer to a variable containing the size of the VarName buffer.
  @param[in out]  VarNamePtr   Pointer to the VarName buffer pointer.  Is reallocated if size is too small.
  @param[in out]  VarGuid      Pointer to a GUID buffer containing the previous GUID and receives the next GUID

  @return         EFI_STATUS
**/
EFI_STATUS
EFIAPI
GetNextVar(
  UINTN *BufferSize,
  CHAR16 **VarNamePtr,
  EFI_GUID *VarGuid)
{
  EFI_STATUS Status;
  UINTN Size;

  Size = *BufferSize;
  Status = gRT->GetNextVariableName (&Size,
                                     *VarNamePtr,
                                     VarGuid);

  if (Status == EFI_BUFFER_TOO_SMALL) {

    *VarNamePtr = ReallocatePool (*BufferSize, Size, *VarNamePtr);
    if (*VarNamePtr == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    *BufferSize = Size;
    Status = gRT->GetNextVariableName (&Size,
                                       *VarNamePtr,
                                       VarGuid);
  }

  return Status;
}

/**
  Walks through variable services looking for all variables in the ABT namespace and adds their names to the
  input linked list

  @param[in]  ListAnchor  Linked list to be updated with all ABT variable names

  @return     EFI_STATUS
**/
EFI_STATUS
EFIAPI
GetListOfAbtVarNames(
  LIST_ENTRY *ListAnchor)
{
  VAR_NAME_LIST_ENTRY *VarEntry;
  CHAR16 *VarName;
  EFI_GUID VarGuid;
  UINTN BufferSize;
  EFI_STATUS Status;

  // Start with a buffer 64 chars long
  BufferSize = 64 * sizeof (CHAR16);
  VarName = (CHAR16*) AllocatePool (BufferSize);
  if (VarName == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  // Get first variable from Variable Services
  VarName[0] = 0x0000;
  Status = GetNextVar(&BufferSize, &VarName, &VarGuid);

  // Loop while Status is success
  while (!EFI_ERROR(Status)) {

    // If this is an ABT variable, save it to the linked list
    if (CompareGuid (&VarGuid, &gAbtVariableGuid)) {

      VarEntry = (VAR_NAME_LIST_ENTRY*) AllocatePool (sizeof (VAR_NAME_LIST_ENTRY));
      VarEntry->Name = AllocateCopyPool (BufferSize, VarName);

      InsertTailList(ListAnchor, (LIST_ENTRY*)VarEntry);
    }

    // Get the next variable
    Status = GetNextVar(&BufferSize, &VarName, &VarGuid);
  }

  // Not found indicates success
  if (Status == EFI_NOT_FOUND) {
    return EFI_SUCCESS;
  }
  return Status;
}

/**
  Removes all variables from Variable Services that use the gAbtVendorGuid namespace GUID.

  @param[in]  None

  @return     EFI_STATUS
**/
EFI_STATUS
EFIAPI
ClearAllAbsoluteVariables ()
{
  LIST_ENTRY ListAnchor = INITIALIZE_LIST_HEAD_VARIABLE(ListAnchor);
  VAR_NAME_LIST_ENTRY *VarEntry;
  EFI_STATUS Status;
  EFI_STATUS EraseStatus;
  UINT32 Attributes;
  UINTN Size;

  DEBUG ((DEBUG_INFO, "[ABT Config] Clearing all ABT variables\n"));

  // Update the linked list with all variables that use the ABT namespace GUID
  Status = GetListOfAbtVarNames(&ListAnchor);
  if (EFI_ERROR(Status)) {

    // On error warn, but keep going to erase the ones properly found
    DEBUG ((DEBUG_INFO, "[ABT Config] WARNING: Could not retrieve all variables using the ABT namespace GUID - Status %r\n", Status));
  }

  // Loop through the linked list
  while (!IsListEmpty(&ListAnchor)) {

    // Extract the last entry from the linked list
    VarEntry = (VAR_NAME_LIST_ENTRY*)GetPreviousNode (&ListAnchor, &ListAnchor);
    RemoveEntryList ((LIST_ENTRY*)VarEntry);

    // Attempt to erase this variable
    Size = 0;
    EraseStatus = gRT->GetVariable (VarEntry->Name, &gAbtVariableGuid, &Attributes, &Size, NULL);
    if (EraseStatus == EFI_BUFFER_TOO_SMALL) {
      EraseStatus = gRT->SetVariable (VarEntry->Name, &gAbtVariableGuid, Attributes, 0, NULL);
    }
    DEBUG ((DEBUG_INFO, "[ABT Config] Removing '%s' - Status %r\n", VarEntry->Name, EraseStatus));

    // If no errors yet, use the EraseStatus as the return code
    if (!EFI_ERROR(Status)) {
      Status = EraseStatus;
    }

    FreePool(VarEntry->Name);
    FreePool(VarEntry);
  }

  // Return
  return Status;
}

