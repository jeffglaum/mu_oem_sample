/** @file CbmrSampleUIAppWindow.h

  cBMR Sample Application main window routines.

  Copyright (c) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  The application is intended to be a sample of how to present cBMR (Cloud Bare Metal Recovery) process to the end user.
**/
#include "CbmrSampleUIApp.h"

#include <Pi/PiFirmwareFile.h>

#include <Library/BmpSupportLib.h>

#include <Library/DxeServicesLib.h>

#include <MsDisplayEngine.h>

#include <Protocol/OnScreenKeyboard.h>
#include <Protocol/SimpleWindowManager.h>

#include <UIToolKit/SimpleUIToolKit.h>
#include <Library/MsUiThemeLib.h>
#include <Library/MsColorTableLib.h>

// Dialog font sizes.  These represent vertical heights (in pixels) which in turn map to one of the custom fonts
// registered by the simple window manager.
//
#define SWM_MB_CUSTOM_FONT_BUTTONTEXT_HEIGHT  MsUiGetSmallFontHeight ()
#define SWM_MB_CUSTOM_FONT_TITLEBAR_HEIGHT    MsUiGetSmallFontHeight ()
#define SWM_MB_CUSTOM_FONT_CAPTION_HEIGHT     MsUiGetLargeFontHeight ()
#define SWM_MB_CUSTOM_FONT_BODY_HEIGHT        MsUiGetStandardFontHeight ()

struct _CBMR_UI_DYNAMIC_LABELS {
  Label    *cBMRState;
  Label    *DownloadFileCount;
  Label    *DownloadTotalSize;
  Label    *NetworkState;
  Label    *NetworkSSID;
  Label    *NetworkPolicy;
  Label    *NetworkIPAddr;
  Label    *NetworkGatewayAddr;
  Label    *NetworkDNSAddr;
} cBMRUIDataLabels;

ProgressBar  *DownloadProgress;

typedef struct _CBMR_UI {
  BOOLEAN    IsUIInitialized;
} CBMR_UI, *PCBMR_UI;

CBMR_UI  gCmbrUI = { 0 };

EFI_GRAPHICS_OUTPUT_PROTOCOL       *mGop;
MS_ONSCREEN_KEYBOARD_PROTOCOL      *mOSKProtocol;
MS_SIMPLE_WINDOW_MANAGER_PROTOCOL  *mSWMProtocol;

SWM_RECT                       WindowRect;
EFI_ABSOLUTE_POINTER_PROTOCOL  *mCbmrPointerProtocol;
EFI_EVENT                      mCbmrPaintEvent;

//
// Boot video resolution and text mode.
//
UINT32  mBootHorizontalResolution = 0;
UINT32  mBootVerticalResolution   = 0;

UINT32  mTitleBarWidth, mTitleBarHeight;
UINT32  mMasterFrameWidth, mMasterFrameHeight;

extern UINTN                       PcdCloudBMRCompanyLogoFile;
EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *gSimpleTextInEx;

Bitmap *
EFIAPI
CbmrUIFetchBitmap (
  UINT32   OrigX,
  UINT32   OrigY,
  EFI_GUID *FileGuid
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  UINT8                          *BMPData    = NULL;
  UINTN                          BMPDataSize = 0;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *BltBuffer  = NULL;
  UINTN                          BltBufferSize;
  UINTN                          BitmapHeight;
  UINTN                          BitmapWidth;

  // Get the specified image from FV.
  //
  Status = GetSectionFromAnyFv (
             FileGuid,
             EFI_SECTION_RAW,
             0,
             (VOID **)&BMPData,
             &BMPDataSize
             );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to find bitmap file (GUID=%g) (%r).\r\n", FileGuid, Status));
    return NULL;
  }

  // Convert the bitmap from BMP format to a GOP framebuffer-compatible form.
  //
  Status = TranslateBmpToGopBlt (
             BMPData,
             BMPDataSize,
             &BltBuffer,
             &BltBufferSize,
             &BitmapHeight,
             &BitmapWidth
             );
  if (EFI_ERROR (Status)) {
    FreePool (BMPData);
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to convert bitmap file to GOP format (%r).\r\n", Status));
    return NULL;
  }

  DEBUG ((DEBUG_INFO, "INFO [cBMR App]: Creating bitmap element (H=%d, W=%d).\r\n", BitmapHeight, BitmapWidth));

  Bitmap  *B = new_Bitmap (
                 OrigX,
                 OrigY,
                 (UINT32)BitmapWidth,
                 (UINT32)BitmapHeight,
                 BltBuffer
                 );

  // Clean-up memory before we go on.
  //
  FreePool (BMPData);
  FreePool (BltBuffer);

  return B;
}

EFI_STATUS
EFIAPI
CbmrUIUpdateDownloadProgress (
  UINT8 Percent
  )
{
  if (DownloadProgress == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  DownloadProgress->UpdateProgressPercent (DownloadProgress, Percent);
  DownloadProgress->Base.Draw (DownloadProgress, FALSE, NULL, NULL);

  return EFI_SUCCESS;
}

static
EFI_STATUS
EFIAPI
CbmrUIFillRect (
  SWM_RECT                      FillRect,
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *FillColor
  )
{
  return mSWMProtocol->BltWindow (
                         mSWMProtocol,
                         gImageHandle,
                         FillColor,
                         EfiBltVideoFill,
                         0,
                         0,
                         FillRect.Left,
                         FillRect.Top,
                         (FillRect.Right - FillRect.Left + 1),
                         (FillRect.Bottom - FillRect.Top + 1),
                         0
                         );
}

EFI_STATUS
EFIAPI
CbmrUIUpdateLabelValue (
  CBMR_UI_DATA_LABEL_TYPE LabelType,
  CHAR16                  *String
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  SWM_RECT    LabelFrame;

  switch (LabelType) {
    case cBMRState:
      cBMRUIDataLabels.cBMRState->Base.GetControlBounds (cBMRUIDataLabels.cBMRState, &LabelFrame);
      CbmrUIFillRect (LabelFrame, &gMsColorTable.FormCanvasBackgroundColor);
      Status = cBMRUIDataLabels.cBMRState->UpdateLabelText (cBMRUIDataLabels.cBMRState, String);
      cBMRUIDataLabels.cBMRState->Base.Draw (cBMRUIDataLabels.cBMRState, FALSE, NULL, NULL);
      break;
    case DownloadFileCount:
      cBMRUIDataLabels.DownloadFileCount->Base.GetControlBounds (cBMRUIDataLabels.DownloadFileCount, &LabelFrame);
      CbmrUIFillRect (LabelFrame, &gMsColorTable.FormCanvasBackgroundColor);
      Status = cBMRUIDataLabels.DownloadFileCount->UpdateLabelText (cBMRUIDataLabels.DownloadFileCount, String);
      cBMRUIDataLabels.DownloadFileCount->Base.Draw (cBMRUIDataLabels.DownloadFileCount, FALSE, NULL, NULL);
      break;
    case DownloadTotalSize:
      cBMRUIDataLabels.DownloadTotalSize->Base.GetControlBounds (cBMRUIDataLabels.DownloadTotalSize, &LabelFrame);
      CbmrUIFillRect (LabelFrame, &gMsColorTable.FormCanvasBackgroundColor);
      Status = cBMRUIDataLabels.DownloadTotalSize->UpdateLabelText (cBMRUIDataLabels.DownloadTotalSize, String);
      cBMRUIDataLabels.DownloadTotalSize->Base.Draw (cBMRUIDataLabels.DownloadTotalSize, FALSE, NULL, NULL);
      break;
    case NetworkState:
      cBMRUIDataLabels.NetworkState->Base.GetControlBounds (cBMRUIDataLabels.NetworkState, &LabelFrame);
      CbmrUIFillRect (LabelFrame, &gMsColorTable.FormCanvasBackgroundColor);
      Status = cBMRUIDataLabels.NetworkState->UpdateLabelText (cBMRUIDataLabels.NetworkState, String);
      cBMRUIDataLabels.NetworkState->Base.Draw (cBMRUIDataLabels.NetworkState, FALSE, NULL, NULL);
      break;
    case NetworkSSID:
      cBMRUIDataLabels.NetworkSSID->Base.GetControlBounds (cBMRUIDataLabels.NetworkSSID, &LabelFrame);
      CbmrUIFillRect (LabelFrame, &gMsColorTable.FormCanvasBackgroundColor);
      Status = cBMRUIDataLabels.NetworkSSID->UpdateLabelText (cBMRUIDataLabels.NetworkSSID, String);
      cBMRUIDataLabels.NetworkSSID->Base.Draw (cBMRUIDataLabels.NetworkSSID, FALSE, NULL, NULL);
      break;
    case NetworkPolicy:
      cBMRUIDataLabels.NetworkPolicy->Base.GetControlBounds (cBMRUIDataLabels.NetworkPolicy, &LabelFrame);
      CbmrUIFillRect (LabelFrame, &gMsColorTable.FormCanvasBackgroundColor);
      Status = cBMRUIDataLabels.NetworkPolicy->UpdateLabelText (cBMRUIDataLabels.NetworkPolicy, String);
      cBMRUIDataLabels.NetworkPolicy->Base.Draw (cBMRUIDataLabels.NetworkPolicy, FALSE, NULL, NULL);
      break;
    case NetworkIPAddr:
      cBMRUIDataLabels.NetworkIPAddr->Base.GetControlBounds (cBMRUIDataLabels.NetworkIPAddr, &LabelFrame);
      CbmrUIFillRect (LabelFrame, &gMsColorTable.FormCanvasBackgroundColor);
      Status = cBMRUIDataLabels.NetworkIPAddr->UpdateLabelText (cBMRUIDataLabels.NetworkIPAddr, String);
      cBMRUIDataLabels.NetworkIPAddr->Base.Draw (cBMRUIDataLabels.NetworkIPAddr, FALSE, NULL, NULL);
      break;
    case NetworkGatewayAddr:
      cBMRUIDataLabels.NetworkGatewayAddr->Base.GetControlBounds (cBMRUIDataLabels.NetworkGatewayAddr, &LabelFrame);
      CbmrUIFillRect (LabelFrame, &gMsColorTable.FormCanvasBackgroundColor);
      Status = cBMRUIDataLabels.NetworkGatewayAddr->UpdateLabelText (cBMRUIDataLabels.NetworkGatewayAddr, String);
      cBMRUIDataLabels.NetworkGatewayAddr->Base.Draw (cBMRUIDataLabels.NetworkGatewayAddr, FALSE, NULL, NULL);
      break;
    case NetworkDNSAddr:
      cBMRUIDataLabels.NetworkDNSAddr->Base.GetControlBounds (cBMRUIDataLabels.NetworkDNSAddr, &LabelFrame);
      CbmrUIFillRect (LabelFrame, &gMsColorTable.FormCanvasBackgroundColor);
      Status = cBMRUIDataLabels.NetworkDNSAddr->UpdateLabelText (cBMRUIDataLabels.NetworkDNSAddr, String);
      cBMRUIDataLabels.NetworkDNSAddr->Base.Draw (cBMRUIDataLabels.NetworkDNSAddr, FALSE, NULL, NULL);
      break;
    default:
      Status = EFI_INVALID_PARAMETER;
  }

  return Status;
}

EFI_STATUS
EFIAPI
CbmrUICreateWindow (
  Canvas **WindowCanvas
  )
{
  EFI_STATUS  Status  = EFI_SUCCESS;
  UINT32      OSKMode = 0;

  //
  // Get current video resolution and text mode.
  //
  GfxGetGraphicsResolution (&mBootHorizontalResolution, &mBootVerticalResolution);

  // Locate the on-screen keyboard (OSK) protocol.
  //
  Status = gBS->LocateProtocol (
                  &gMsOSKProtocolGuid,
                  NULL,
                  (VOID **)&mOSKProtocol
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to find the on-screen keyboard protocol (%r).\r\n", Status));
    goto Exit;
  }

  // Disable OSK icon auto-activation and self-refresh, and ensure keyboard is disabled.
  // NOTE: OSK will automatically be enabled (and icon will appear) when we want for simple text input later.
  //
  mOSKProtocol->GetKeyboardMode (mOSKProtocol, &OSKMode);
  OSKMode &= ~(OSK_MODE_AUTOENABLEICON | OSK_MODE_SELF_REFRESH);
  mOSKProtocol->ShowKeyboard (mOSKProtocol, FALSE);
  mOSKProtocol->ShowKeyboardIcon (mOSKProtocol, FALSE);

  // Locate the Simple Window Manager protocol.
  //
  Status = gBS->LocateProtocol (
                  &gMsSWMProtocolGuid,
                  NULL,
                  (VOID **)&mSWMProtocol
                  );

  if (EFI_ERROR (Status)) {
    mSWMProtocol = NULL;
    Status = EFI_UNSUPPORTED;
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to find the window manager protocol (%r).\r\n", Status));

    goto Exit;
  }

  if (gST->ConsoleInHandle != NULL) {
    Status = gBS->OpenProtocol (
                    gST->ConsoleInHandle,
                    &gEfiSimpleTextInputExProtocolGuid,
                    (VOID **)&gSimpleTextInEx,
                    NULL,
                    NULL,
                    EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
                    );
  } else {
    DEBUG ((DEBUG_ERROR, "%a: SystemTable ConsoleInHandle is NULL\n", __FUNCTION__));
    Status = EFI_NOT_READY;
  }

  WindowRect.Left   = 0;
  WindowRect.Top    = 0;
  WindowRect.Right  = (mBootHorizontalResolution - 1);
  WindowRect.Bottom = (mBootVerticalResolution - 1);

  // Register with the Simple Window Manager to get mouse and touch input events.
  //
  Status = mSWMProtocol->RegisterClient (
                           mSWMProtocol,
                           gImageHandle,
                           SWM_Z_ORDER_CLIENT,
                           &WindowRect,
                           NULL,
                           NULL,
                           &mCbmrPointerProtocol,
                           &mCbmrPaintEvent
                           );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to register application window as a SWM client: %r.\r\n", Status));
    goto Exit;
  }

  mSWMProtocol->ActivateWindow (
                  mSWMProtocol,
                  gImageHandle,
                  TRUE
                  );

  // Enable the mouse pointer to be displayed if a USB mouse or  trackpad is attached and is moved.
  //
  mSWMProtocol->EnableMousePointer (
                  mSWMProtocol,
                  TRUE
                  );

  mSWMProtocol->BltWindow (
                  mSWMProtocol,
                  gImageHandle,
                  &gMsColorTable.FormCanvasBackgroundColor,
                  EfiBltVideoFill,
                  0,
                  0,
                  WindowRect.Left,
                  WindowRect.Top,
                  (WindowRect.Right - WindowRect.Left + 1),
                  (WindowRect.Bottom - WindowRect.Top + 1),
                  0
                  );

  // Create a canvas for the main cBMR window.
  //
  Canvas  *DialogCanvas = new_Canvas (
                            WindowRect,
                            &gMsColorTable.FormCanvasBackgroundColor
                            );

  if (NULL == DialogCanvas) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to create application canvas: %r.\r\n", Status));
    goto Exit;
  }

  // Grid
  SWM_RECT  TitleGridRect = { WindowRect.Left, WindowRect.Top, WindowRect.Right, (WindowRect.Top + 128) };

  Grid  *TitleGrid = new_Grid (DialogCanvas, TitleGridRect, 1, 4, FALSE);

  DialogCanvas->AddControl (
                  DialogCanvas,
                  FALSE,               // Not highlightable.
                  TRUE,                // Invisible.
                  (VOID *)TitleGrid
                  );

  // NOTE: insert into your platform FDF file a reference to the company logo bitmap.  Something like this:
  //  # cBMR application company logo bitmap image.
  // FILE FREEFORM = PCD(gOemPkgTokenSpaceGuid.PcdCloudBMRCompanyLogoFile) {
  //   SECTION RAW = OemPkg/CloudBMR/Application/CbmrSampleUIApp/Resources/WindowsLogo.bmp
  // }

  TitleGrid->AddControl (TitleGrid, FALSE, FALSE, 0, 0, (VOID *)CbmrUIFetchBitmap (0, 0, PcdGetPtr (PcdCloudBMRCompanyLogoFile)));

  Label          *CaptionLabel = NULL;
  EFI_FONT_INFO  HeadingFontInfo;

  HeadingFontInfo.FontSize    = SWM_MB_CUSTOM_FONT_CAPTION_HEIGHT;
  HeadingFontInfo.FontStyle   = EFI_HII_FONT_STYLE_NORMAL;
  HeadingFontInfo.FontName[0] = L'\0';

  CaptionLabel = new_Label (
                   0,
                   0,
                   500,
                   100,
                   &HeadingFontInfo,
                   &gMsColorTable.LabelTextLargeColor,
                   &gMsColorTable.FormCanvasBackgroundColor,
                   L"Cloud Bare Metal Recovery"
                   );

  if (NULL == CaptionLabel) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  TitleGrid->AddControl (TitleGrid, FALSE, FALSE, 0, 1, (VOID *)CaptionLabel);

  EFI_FONT_INFO  BodyFontInfo;

  BodyFontInfo.FontSize    = SWM_MB_CUSTOM_FONT_BODY_HEIGHT;
  BodyFontInfo.FontStyle   = EFI_HII_FONT_STYLE_NORMAL;
  BodyFontInfo.FontName[0] = L'\0';

  // Stage Grid
  SWM_RECT  StageGridRect = { WindowRect.Left, (WindowRect.Top + 128), WindowRect.Right, (WindowRect.Top + 192) };

  Grid  *StageGrid = new_Grid (DialogCanvas, StageGridRect, 3, 4, FALSE);

  DialogCanvas->AddControl (
                  DialogCanvas,
                  FALSE,               // Not highlightable.
                  TRUE,                // Invisible.
                  (VOID *)StageGrid
                  );

  StageGrid->AddControl (StageGrid, FALSE, FALSE, 0, 1, (VOID *)new_Label (0, 0, 200, 50, &BodyFontInfo, &gMsColorTable.LabelTextNormalColor, &gMsColorTable.FormCanvasBackgroundColor, L"Stage:"));
  StageGrid->AddControl (StageGrid, FALSE, FALSE, 1, 1, (VOID *)new_Label (0, 0, 200, 50, &BodyFontInfo, &gMsColorTable.LabelTextNormalColor, &gMsColorTable.FormCanvasBackgroundColor, L"Number of Files:"));
  StageGrid->AddControl (StageGrid, FALSE, FALSE, 2, 1, (VOID *)new_Label (0, 0, 200, 50, &BodyFontInfo, &gMsColorTable.LabelTextNormalColor, &gMsColorTable.FormCanvasBackgroundColor, L"Total Size:"));

  cBMRUIDataLabels.cBMRState = new_Label (0, 0, 200, 50, &BodyFontInfo, &gMsColorTable.LabelTextLargeColor, &gMsColorTable.FormCanvasBackgroundColor, L" ");
  StageGrid->AddControl (StageGrid, FALSE, FALSE, 0, 2, (VOID *)cBMRUIDataLabels.cBMRState);
  cBMRUIDataLabels.DownloadFileCount = new_Label (0, 0, 200, 50, &BodyFontInfo, &gMsColorTable.LabelTextLargeColor, &gMsColorTable.FormCanvasBackgroundColor, L" ");
  StageGrid->AddControl (StageGrid, FALSE, FALSE, 1, 2, (VOID *)cBMRUIDataLabels.DownloadFileCount);
  cBMRUIDataLabels.DownloadTotalSize = new_Label (0, 0, 200, 50, &BodyFontInfo, &gMsColorTable.LabelTextLargeColor, &gMsColorTable.FormCanvasBackgroundColor, L" ");
  StageGrid->AddControl (StageGrid, FALSE, FALSE, 2, 2, (VOID *)cBMRUIDataLabels.DownloadTotalSize);

  // Grid
  SWM_RECT  NetworkStatusGridRect = { WindowRect.Left, (WindowRect.Top + 220), WindowRect.Right, (WindowRect.Top + 348) };

  Grid  *NetworkStatusGrid = new_Grid (DialogCanvas, NetworkStatusGridRect, 6, 4, FALSE);

  DialogCanvas->AddControl (
                  DialogCanvas,
                  FALSE,               // Not highlightable.
                  TRUE,                // Invisible.
                  (VOID *)NetworkStatusGrid
                  );

  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 0, 1, (VOID *)new_Label (0, 0, 200, 50, &BodyFontInfo, &gMsColorTable.LabelTextNormalColor, &gMsColorTable.FormCanvasBackgroundColor, L"Network:"));
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 1, 1, (VOID *)new_Label (0, 0, 200, 50, &BodyFontInfo, &gMsColorTable.LabelTextNormalColor, &gMsColorTable.FormCanvasBackgroundColor, L"SSID:"));
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 2, 1, (VOID *)new_Label (0, 0, 200, 50, &BodyFontInfo, &gMsColorTable.LabelTextNormalColor, &gMsColorTable.FormCanvasBackgroundColor, L"Policy:"));
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 3, 1, (VOID *)new_Label (0, 0, 200, 50, &BodyFontInfo, &gMsColorTable.LabelTextNormalColor, &gMsColorTable.FormCanvasBackgroundColor, L"IP Address:"));
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 4, 1, (VOID *)new_Label (0, 0, 200, 50, &BodyFontInfo, &gMsColorTable.LabelTextNormalColor, &gMsColorTable.FormCanvasBackgroundColor, L"Gateway:"));
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 5, 1, (VOID *)new_Label (0, 0, 200, 50, &BodyFontInfo, &gMsColorTable.LabelTextNormalColor, &gMsColorTable.FormCanvasBackgroundColor, L"DNS Server:"));

  cBMRUIDataLabels.NetworkState = new_Label (0, 0, 200, 50, &BodyFontInfo, &gMsColorTable.LabelTextLargeColor, &gMsColorTable.FormCanvasBackgroundColor, L" ");
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 0, 2, (VOID *)cBMRUIDataLabels.NetworkState);
  cBMRUIDataLabels.NetworkSSID = new_Label (0, 0, 200, 50, &BodyFontInfo, &gMsColorTable.LabelTextLargeColor, &gMsColorTable.FormCanvasBackgroundColor, L" ");
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 1, 2, (VOID *)cBMRUIDataLabels.NetworkSSID);
  cBMRUIDataLabels.NetworkPolicy = new_Label (0, 0, 200, 50, &BodyFontInfo, &gMsColorTable.LabelTextLargeColor, &gMsColorTable.FormCanvasBackgroundColor, L" ");
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 2, 2, (VOID *)cBMRUIDataLabels.NetworkPolicy);
  cBMRUIDataLabels.NetworkIPAddr = new_Label (0, 0, 200, 50, &BodyFontInfo, &gMsColorTable.LabelTextLargeColor, &gMsColorTable.FormCanvasBackgroundColor, L" ");
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 3, 2, (VOID *)cBMRUIDataLabels.NetworkIPAddr);
  cBMRUIDataLabels.NetworkGatewayAddr = new_Label (0, 0, 200, 50, &BodyFontInfo, &gMsColorTable.LabelTextLargeColor, &gMsColorTable.FormCanvasBackgroundColor, L" ");
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 4, 2, (VOID *)cBMRUIDataLabels.NetworkGatewayAddr);
  cBMRUIDataLabels.NetworkDNSAddr = new_Label (0, 0, 200, 50, &BodyFontInfo, &gMsColorTable.LabelTextLargeColor, &gMsColorTable.FormCanvasBackgroundColor, L" ");
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 5, 2, (VOID *)cBMRUIDataLabels.NetworkDNSAddr);

  // Grid
  SWM_RECT  DownloadProgressGridRect = { WindowRect.Left, (WindowRect.Top + 378), WindowRect.Right, (WindowRect.Top + 506) };

  Grid  *DownloadProgressGrid = new_Grid (DialogCanvas, DownloadProgressGridRect, 6, 4, FALSE);

  DialogCanvas->AddControl (
                  DialogCanvas,
                  FALSE,               // Not highlightable.
                  TRUE,                // Invisible.
                  (VOID *)DownloadProgressGrid
                  );

  DownloadProgressGrid->AddControl (DownloadProgressGrid, FALSE, FALSE, 0, 1, (VOID *)new_Label (0, 0, 200, 50, &BodyFontInfo, &gMsColorTable.LabelTextNormalColor, &gMsColorTable.FormCanvasBackgroundColor, L"Downloading:"));

  // Progress Bar
  DownloadProgress = new_ProgressBar (0, 0, 250, 5, &gMsColorTable.LabelTextLargeColor, &gMsColorTable.MasterFrameBackgroundColor, 0);
  DownloadProgressGrid->AddControl (DownloadProgressGrid, FALSE, FALSE, 0, 2, (VOID *)DownloadProgress);

  Button  *GoButton = new_Button (
                        200,
                        (WindowRect.Top + 440),
                        150,
                        40,
                        &BodyFontInfo,
                        &gMsColorTable.DefaultDialogBackGroundColor,
                        &gMsColorTable.DefaultDialogButtonHoverColor,
                        &gMsColorTable.DefaultDialogButtonSelectColor,
                        &gMsColorTable.DefaultDialogButtonGrayOutColor,      // GrayOut.
                        &gMsColorTable.DefaultDialogButtonRingColor,         // Button ring.
                        &gMsColorTable.DefaultDialogButtonTextColor,         // Normal text.
                        &gMsColorTable.DefaultDialogButtonSelectTextColor,   // Normal text.
                        L"Start Recovery",
                        (VOID *)(UINTN)SWM_MB_IDOK
                        );

  DialogCanvas->AddControl (
                  DialogCanvas,
                  FALSE,                // Not highlightable.
                  FALSE,                // Visible.
                  (VOID *)GoButton
                  );

  Button  *CancelButton = new_Button (
                            400,
                            (WindowRect.Top + 440),
                            150,
                            40,
                            &BodyFontInfo,
                            &gMsColorTable.DefaultDialogButtonGrayOutColor,
                            &gMsColorTable.DefaultDialogButtonHoverColor,
                            &gMsColorTable.DefaultDialogButtonSelectColor,
                            &gMsColorTable.DefaultDialogButtonGrayOutColor,    // GrayOut.
                            &gMsColorTable.DefaultDialogButtonRingColor,       // Button ring.
                            &gMsColorTable.DefaultDialogButtonTextColor,       // Normal text.
                            &gMsColorTable.DefaultDialogButtonSelectTextColor, // Normal text.
                            L"Cancel",
                            (VOID *)(UINTN)SWM_MB_IDCANCEL
                            );

  DialogCanvas->AddControl (
                  DialogCanvas,
                  FALSE,                // Not highlightable.
                  FALSE,                // Visible.
                  (VOID *)CancelButton
                  );

  *WindowCanvas = DialogCanvas;

Exit:

  return Status;
}

static
SWM_MB_RESULT
ProcessWindowInput (
  IN  MS_SIMPLE_WINDOW_MANAGER_PROTOCOL *this,
  IN  Canvas                            *WindowCanvas,
  IN  EFI_ABSOLUTE_POINTER_PROTOCOL     *PointerProtocol,
  IN  UINT64                            Timeout
  )
{
  EFI_STATUS       Status = EFI_SUCCESS;
  UINTN            Index;
  OBJECT_STATE     State = NORMAL;
  SWM_MB_RESULT    ButtonResult = 0;
  VOID             *pContext    = NULL;
  SWM_INPUT_STATE  InputState;
  // UINTN            NumberOfEvents = 2;
  UINTN  NumberOfEvents = 1;

  EFI_EVENT  WaitEvents[2];

  // Wait for user input.
  //
  WaitEvents[0] = gSimpleTextInEx->WaitForKeyEx;
  // WaitEvents[1] = PointerProtocol->WaitForInput;

  ZeroMem (&InputState, sizeof (SWM_INPUT_STATE));

  do {
    // Render the canvas and all child controls.
    //
    State = WindowCanvas->Base.Draw (
                                 WindowCanvas,
                                 FALSE,
                                 &InputState,
                                 &pContext
                                 );

    // If one of the controls indicated they were selected, take action.  Grab the associated context and if a button
    // was selected, decide the action to be taken.
    //
    if (SELECT == State) {
      // Determine which button was pressed by the context returned.
      //
      // TODO - avoid having to cast a constant value from a pointer.
      //
      ButtonResult = (SWM_MB_RESULT)(UINTN)pContext;

      // If user clicked either of the buttons, exit.
      if ((SWM_MB_IDCANCEL == ButtonResult) || (SWM_MB_IDOK == ButtonResult)) {
        DEBUG ((DEBUG_INFO, "INFO [cBMR App]: Button clicked.\n"));
        break;
      }
    }

    while (EFI_SUCCESS == Status) {
      // Wait for user input.
      //
      Status = this->WaitForEvent (
                       NumberOfEvents,
                       WaitEvents,
                       &Index,
                       Timeout,
                       FALSE
                       );

      if ((EFI_SUCCESS == Status) && (0 == Index)) {
        // Received KEYBOARD input.
        //
        InputState.InputType = SWM_INPUT_TYPE_KEY;

        // Read key press data.
        //
        Status = gSimpleTextInEx->ReadKeyStrokeEx (
                                    gSimpleTextInEx,
                                    &InputState.State.KeyState
                                    );

        // If the user pressed ESC, exit without doing anything.
        //
        if (SCAN_ESC == InputState.State.KeyState.Key.ScanCode) {
          ButtonResult = SWM_MB_IDCANCEL;
          break;
        }

        // If the user pressed Enter, proceed with cBMR.
        //
        if (CHAR_CARRIAGE_RETURN == InputState.State.KeyState.Key.UnicodeChar) {
          ButtonResult = SWM_MB_IDOK;
          break;
        }

        break;
      } else if ((EFI_SUCCESS == Status) && (1 == Index)) {
        // Received TOUCH input.
        //
        static BOOLEAN  WatchForFirstFingerUpEvent = FALSE;
        BOOLEAN         WatchForFirstFingerUpEvent2;

        InputState.InputType = SWM_INPUT_TYPE_TOUCH;

        Status = PointerProtocol->GetState (
                                    PointerProtocol,
                                    &InputState.State.TouchState
                                    );

        // Filter out all extra pointer moves with finger UP.
        WatchForFirstFingerUpEvent2 = WatchForFirstFingerUpEvent;
        WatchForFirstFingerUpEvent  = SWM_IS_FINGER_DOWN (InputState.State.TouchState);
        if (!SWM_IS_FINGER_DOWN (InputState.State.TouchState) && (FALSE == WatchForFirstFingerUpEvent2)) {
          continue;
        }

        break;
      } else if ((EFI_SUCCESS == Status) && (2 == Index)) {
        ButtonResult = SWM_MB_TIMEOUT;
        break;
      }
    }
  } while (0 == ButtonResult && EFI_SUCCESS == Status);

  return (ButtonResult);
}

SWM_MB_RESULT
EFIAPI
CbmrUIWindowMessageHandler (
  Canvas *WindowCanvas
  )
{
  return ProcessWindowInput (
           mSWMProtocol,
           WindowCanvas,
           NULL,
           0
           );
}
