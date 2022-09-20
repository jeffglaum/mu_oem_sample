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

#include "CbmrApp.h"
#include "CbmrAppVfr.h"
#include "graphics_common.h"

#include <MsDisplayEngine.h>


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

EFI_HII_CONFIG_ROUTING_PROTOCOL  *mHiiConfigRouting;
EFI_FORM_BROWSER2_PROTOCOL         *mFormBrowser2;
EFI_HII_HANDLE  mFormHandle;
DISPLAY_ENGINE_SHARED_STATE      mDisplayEngineState;

// Form GUID
//
EFI_GUID  gCbmrAppFormSetGuid = CBMR_APP_FORMSET_GUID;

//
// These are the VFR compiler generated data representing our VFR data.
//
extern UINT8  CbmrAppVfrBin[];

//
// This is the VFR compiler generated header file which defines the
// string identifiers.
//

extern UINT8  CbmrAppStrings[];

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
  EFI_BROWSER_ACTION_REQUEST  ActionRequest;

  if (gCmbrUI.IsUIInitialized == TRUE) {
    DEBUG ((DEBUG_WARN, "CbmrUIInitialize () already initialized"));
    return Status;
  }

    //
    // Locate Hii relative protocols
    //
    Status = gBS->LocateProtocol (&gEfiFormBrowser2ProtocolGuid, NULL, (VOID **)&mFormBrowser2);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = gBS->LocateProtocol (&gEfiHiiConfigRoutingProtocolGuid, NULL, (VOID **)&mHiiConfigRouting);
    if (EFI_ERROR (Status)) {
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

  // Set shared pointer to user input context structure in a PCD so it can be shared.
  //
  PcdSet64S (PcdCurrentPointerState, (UINT64)(UINTN)&mDisplayEngineState);

    //
    // Publish our HII data
    //
    mFormHandle = HiiAddPackages (&gCbmrAppFormSetGuid,
                                  NULL,
                                  CbmrAppVfrBin,
                                  CbmrAppStrings,
                                  NULL
                                  );

    if (mFormHandle == NULL) {
      DEBUG ((DEBUG_ERROR, "HiiAddPackages () failed\n"));
      return EFI_OUT_OF_RESOURCES;
    }

  // Call the browser to display the selected form.
  //
  Status = mFormBrowser2->SendForm (
                            mFormBrowser2,
                            &mFormHandle,
                            1,  // Handle Count.
                            NULL,
                            0,    // FormID.
                            (EFI_SCREEN_DESCRIPTOR *)NULL,
                            &ActionRequest
                            );


  // JDG
  while (TRUE) {}

  
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
