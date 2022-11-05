/** @file CbmrAppGraphics.c

  cBMR (Cloud Bare Metal Recovery) sample application graphics helper functions

  Copyright (c) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  The application is a sample, demonstrating how one might present the cBMR process to a user.
**/
#include "CbmrApp.h"

extern CBMR_APP_CONTEXT  gAppContext;

typedef struct _EFI_GRAPHICS_OUTPUT_MODE_INFORMATION_WRAPPER {
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION    *Mode;
  UINT32                                  Index;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION_WRAPPER;

static INTN
EFIAPI
GfxModeCompareFunc (
  IN CONST VOID  *Mode1,
  IN CONST VOID  *Mode2
  )
{
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *GfxMode1 = ((EFI_GRAPHICS_OUTPUT_MODE_INFORMATION_WRAPPER *)Mode1)->Mode;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *GfxMode2 = ((EFI_GRAPHICS_OUTPUT_MODE_INFORMATION_WRAPPER *)Mode2)->Mode;
  INTN                                  Ret       = (INTN)(GfxMode1->HorizontalResolution) - (INTN)(GfxMode2->HorizontalResolution);

  return Ret;
}

EFI_STATUS
EFIAPI
GfxGetGraphicsResolution (
  OUT UINT32  *Width,
  OUT UINT32  *Height
  )
{
  EFI_STATUS                    Status            = EFI_SUCCESS;
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *GraphicsProtocol = NULL;

  //
  // After the console is ready, get current video resolution
  // and text mode before launching setup at first time.
  //
  Status = gBS->LocateProtocol (
                  &gEfiGraphicsOutputProtocolGuid,
                  NULL,
                  (VOID **)&GraphicsProtocol
                  );

  if (EFI_ERROR (Status)) {
    GraphicsProtocol = (EFI_GRAPHICS_OUTPUT_PROTOCOL *)NULL;
    goto Exit;
  }

  //
  // Get current video resolution and text mode.
  //
  *Width  = GraphicsProtocol->Mode->Info->HorizontalResolution;
  *Height = GraphicsProtocol->Mode->Info->VerticalResolution;

Exit:

  return Status;
}

EFI_STATUS
EFIAPI
GfxSetGraphicsResolution (
  IN UINT32   DesiredMode,
  OUT UINT32  *PreviousMode
  )
{
  EFI_STATUS                                    Status            = EFI_SUCCESS;
  EFI_GRAPHICS_OUTPUT_PROTOCOL                  *GraphicsProtocol = NULL;
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE             *GraphicsMode     = NULL;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION_WRAPPER  *GraphicsModes    = NULL;

  // Get hold of graphics protocol
  //
  Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&GraphicsProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "LocateProtocol() failed : (%r)\n", Status));
    goto Exit;
  }

  GraphicsMode  = GraphicsProtocol->Mode;
  *PreviousMode = GraphicsMode->Mode;

  GraphicsModes = AllocateZeroPool (sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION_WRAPPER) * GraphicsMode->MaxMode);
  if (GraphicsModes == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  DEBUG ((DEBUG_INFO, "INFO: GOP maximum modes = 0x%x\r\n", GraphicsMode->MaxMode));

  for (UINT32 i = 0; i < GraphicsMode->MaxMode; i++) {
    UINTN  ModeInfoSize = 0;

    Status = GraphicsProtocol->QueryMode (
                                 GraphicsProtocol,
                                 i,
                                 &ModeInfoSize,
                                 &GraphicsModes[i].Mode
                                 );
    if (EFI_ERROR (Status)) {
      Status = EFI_SUCCESS;
    }

    GraphicsModes[i].Index = i;
    DEBUG ((DEBUG_INFO, "INFO [cBMR App]: GOP Mode %d (Horizontal=%d, Vertical=%d).\r\n", GraphicsModes[i].Index, GraphicsModes[i].Mode->HorizontalResolution, GraphicsModes[i].Mode->VerticalResolution));
  }

  DEBUG ((DEBUG_INFO, "INFO [cBMR App]: Settings graphics mode: %d\n", DesiredMode));
  Status = GraphicsProtocol->SetMode (GraphicsProtocol, DesiredMode);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to set graphics mode (%r).\n", Status));
    goto Exit;
  }

  // Capture selected resolution in application context.
  //
  gAppContext.HorizontalResolution = GraphicsModes[DesiredMode].Mode->HorizontalResolution;
  gAppContext.VerticalResolution   = GraphicsModes[DesiredMode].Mode->VerticalResolution;

Exit:
  if (GraphicsModes != NULL) {
    for (UINTN i = 0; i < GraphicsMode->MaxMode; i++) {
      FreePool (GraphicsModes[i].Mode);
    }

    FreePool (GraphicsModes);
  }

  return Status;
}
