/*++

Copyright (c) 2021 Microsoft Corporation

Module Name:

  cbmr_ui.c

Abstract:

  This module implements CBMR UI

Author:

  Vineel Kovvuri (vineelko) 21-Apr-2021

Environment:

  UEFI mode only.

--*/

#include "cbmrapp.h"
#include "graphics_common.h"


typedef struct _CBMR_UI {
  GFX_LABEL DownloadStatus;
  GFX_PROGRESS_BAR EachFileProgress;
  GFX_PROGRESS_BAR TotalProgress;
  GFX_LABEL ApplicationStatus;

  GFX_FRAMEBUFFER FrameBuffer;
  GFX_FONT_INFO FontInfo;

  BOOLEAN IsUIInitialized;
} CBMR_UI, *PCBMR_UI;

CBMR_UI gCmbrUI = {0};


EFI_STATUS
EFIAPI
CbmrUIInitializeElements ()
{
  EFI_STATUS Status = EFI_SUCCESS;
  UINTN Height = gCmbrUI.FrameBuffer.Height;
  UINTN Width = gCmbrUI.FrameBuffer.Width;

  DEBUG ((DEBUG_INFO, "FB Width = %d, Height = %d\n", Width, Height));

  gCmbrUI.ApplicationStatus.Bounds.X = 2;
  gCmbrUI.ApplicationStatus.Bounds.Y = Height / 2 - DEFAULT_PROGRESS_BAR_HEIGHT / 2 - 80;
  gCmbrUI.ApplicationStatus.Bounds.Width = Width;
  gCmbrUI.ApplicationStatus.Bounds.Height = DEFAULT_LABEL_HEIGHT;

  gCmbrUI.DownloadStatus.Bounds.X = 2;
  gCmbrUI.DownloadStatus.Bounds.Y = Height / 2 - DEFAULT_PROGRESS_BAR_HEIGHT / 2 - 60;
  gCmbrUI.DownloadStatus.Bounds.Width = Width;
  gCmbrUI.DownloadStatus.Bounds.Height = DEFAULT_LABEL_HEIGHT;

  GfxInitRectangle (&gCmbrUI.EachFileProgress.Bounds,
                    2,
                    Height / 2 - DEFAULT_PROGRESS_BAR_HEIGHT / 2 - 40,
                    Width - 2 * 5,
                    DEFAULT_PROGRESS_BAR_HEIGHT);

  GfxInitRectangle (&gCmbrUI.TotalProgress.Bounds,
                    2,
                    Height / 2 - DEFAULT_PROGRESS_BAR_HEIGHT / 2,
                    Width - 2 * 5,
                    DEFAULT_PROGRESS_BAR_HEIGHT);

  return Status;
}

EFI_STATUS
EFIAPI
CbmrUIInitialize ()
{
  EFI_STATUS Status = EFI_SUCCESS;
  UINT32 PreviousMode = 0;

  if (gCmbrUI.IsUIInitialized == TRUE) {
    DEBUG ((DEBUG_WARN, "CbmrUIInitialize () already initialized"));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "Setting CBMR Graphics resolution\n"));
  Status = GfxSetGraphicsResolution (&PreviousMode);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GfxSetGraphicsResolution () failed: (%r)\n", Status));
    goto Exit;
  }

  Status = GfxGetSystemFont (&gCmbrUI.FontInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GfxGetSystemFont () failed: (%r)\n", Status));
    goto Exit;
  }

  DEBUG ((DEBUG_INFO, "Allocating frame buffer\n"));
  Status = GfxAllocateFrameBuffer (&gCmbrUI.FrameBuffer);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GfxAllocateFrameBuffer () failed: (%r)\n", Status));
    goto Exit;
  }

  DEBUG ((DEBUG_INFO, "Clearing screen\n"));
  Status = GfxClearScreen (&gCmbrUI.FrameBuffer, BLACK_COLOR);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GfxClearScreen () failed: (%r)\n", Status));
    goto Exit;
  }

  DEBUG ((DEBUG_INFO, "Allocating CBMR UI elements\n"));
  Status = CbmrUIInitializeElements ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "CbmrUIInitializeElements () failed: (%r)\n", Status));
    goto Exit;
  }

  gCmbrUI.IsUIInitialized = TRUE;
  return Status;

Exit:
  CbmrUIFreeResources ();
  return Status;
}

EFI_STATUS
EFIAPI
CbmrUIUpdateDownloadProgress (
  IN CHAR16* DownloadStatusText,
  IN UINTN Percentage1,
  IN UINTN Percentage2)
{
  EFI_STATUS Status = EFI_SUCCESS;
  PGFX_FRAMEBUFFER FrameBuffer = &gCmbrUI.FrameBuffer;

  if (gCmbrUI.IsUIInitialized == FALSE) {
    Status = EFI_SUCCESS;
    goto Exit;
  }

  DEBUG ((DEBUG_VERBOSE,
          "%s CurrentFileProgress=%d TotalProgress=%d",
          DownloadStatusText,
          Percentage1,
          Percentage2));

  //
  // Update UI elements state
  //

  gCmbrUI.DownloadStatus.Text = DownloadStatusText;
  gCmbrUI.EachFileProgress.Percentage = Percentage1;
  gCmbrUI.TotalProgress.Percentage = Percentage2;

  //
  // Update UI elements on to frame buffer
  //

  GfxDrawLabel (FrameBuffer, &gCmbrUI.DownloadStatus, &gCmbrUI.FontInfo, WHITE_COLOR);
  GfxDrawProgressBar (FrameBuffer, &gCmbrUI.EachFileProgress, WHITE_COLOR);
  GfxDrawProgressBar (FrameBuffer, &gCmbrUI.TotalProgress, WHITE_COLOR);

  //
  // Render UI elements to screen
  //

  GfxUpdateFrameBufferToScreen (FrameBuffer);

Exit:
  return Status;
}

EFI_STATUS
EFIAPI
CbmrUIUpdateApplicationStatus (
  IN CHAR16* ApplicationStatusText)
{
  EFI_STATUS Status = EFI_SUCCESS;
  PGFX_FRAMEBUFFER FrameBuffer = &gCmbrUI.FrameBuffer;

  if (gCmbrUI.IsUIInitialized == FALSE) {
    DEBUG ((DEBUG_INFO, "%s\n", ApplicationStatusText));
    Status = EFI_SUCCESS;
    goto Exit;
  }

  //
  // Update UI elements state
  //

  gCmbrUI.ApplicationStatus.Text = ApplicationStatusText;

  //
  // Update UI elements on to frame buffer
  //

  GfxDrawLabel (FrameBuffer, &gCmbrUI.ApplicationStatus, &gCmbrUI.FontInfo, WHITE_COLOR);

  //
  // Render UI elements to screen
  //

  GfxUpdateFrameBufferToScreen (FrameBuffer);

Exit:
  return Status;
}

EFI_STATUS
EFIAPI
CbmrUIFreeResources ()
{
  if (gCmbrUI.FrameBuffer.Bitmap != NULL) {
    FreePool (gCmbrUI.FrameBuffer.Bitmap);
    gCmbrUI.FrameBuffer.Bitmap = NULL;
  }
  if (gCmbrUI.FrameBuffer.BackBuffer != NULL) {
    FreePool (gCmbrUI.FrameBuffer.BackBuffer);
    gCmbrUI.FrameBuffer.BackBuffer = NULL;
  }
  if (gCmbrUI.FontInfo.Font != NULL) {
    FreePool (gCmbrUI.FontInfo.Font);
    gCmbrUI.FontInfo.Font = NULL;
  }

  gCmbrUI.IsUIInitialized = FALSE;
  return EFI_SUCCESS;
}
