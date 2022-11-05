/** @file CbmrAppWindow.h

  cBMR (Cloud Bare Metal Recovery) sample application main window implementation.  The window
  is used to present status, network information, cBMR payload details, and download progress.

  Copyright (c) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  The application is a sample, demonstrating how one might present the cBMR process to a user.
**/
#include "CbmrApp.h"
#include <Pi/PiFirmwareFile.h>
#include <Library/DxeServicesLib.h>

// Global definitions.
//
// Application window UI elements that will be updated during cBMR.
static struct _CBMR_DYNAMIC_UI_ELEMENTS {
  struct {
    Label    *CbmrState;
    Label    *DownloadFileCount;
    Label    *DownloadTotalSize;
    Label    *NetworkState;
    Label    *NetworkSSID;
    Label    *NetworkPolicy;
    Label    *NetworkIPAddr;
    Label    *NetworkGatewayAddr;
    Label    *NetworkDNSAddr;
  } DataLabels;

  ProgressBar    *DownloadProgress;
} gCbmrDynamicUIElements;

// Protocol-related variables.
static MS_SIMPLE_WINDOW_MANAGER_PROTOCOL  *mSWMProtocol;
static EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *gSimpleTextInEx;
static EFI_ABSOLUTE_POINTER_PROTOCOL      *gCbmrPointerProtocol;
static EFI_EVENT                          gCbmrPaintEvent;

// External definitions.
//
extern UINTN             PcdCloudBMRCompanyLogoFile;
extern CBMR_APP_CONTEXT  gAppContext;

static
Bitmap *
EFIAPI
CbmrUIFetchBitmap (
  UINT32    OrigX,
  UINT32    OrigY,
  EFI_GUID  *FileGuid
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
  UINT8  Percent
  )
{
  if (gCbmrDynamicUIElements.DownloadProgress == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  gCbmrDynamicUIElements.DownloadProgress->UpdateProgressPercent (gCbmrDynamicUIElements.DownloadProgress, Percent);
  gCbmrDynamicUIElements.DownloadProgress->Base.Draw (gCbmrDynamicUIElements.DownloadProgress, FALSE, NULL, NULL);

  return EFI_SUCCESS;
}

static
EFI_STATUS
EFIAPI
CbmrUIFillRect (
  SWM_RECT                       FillRect,
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *FillColor
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
  CBMR_UI_DATA_LABEL_TYPE  LabelType,
  CHAR16                   *String
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  SWM_RECT    LabelFrame;

  switch (LabelType) {
    case cBMRState:
      gCbmrDynamicUIElements.DataLabels.CbmrState->Base.GetControlBounds (gCbmrDynamicUIElements.DataLabels.CbmrState, &LabelFrame);
      CbmrUIFillRect (LabelFrame, &gMsColorTable.FormCanvasBackgroundColor);
      Status = gCbmrDynamicUIElements.DataLabels.CbmrState->UpdateLabelText (gCbmrDynamicUIElements.DataLabels.CbmrState, String);
      gCbmrDynamicUIElements.DataLabels.CbmrState->Base.Draw (gCbmrDynamicUIElements.DataLabels.CbmrState, FALSE, NULL, NULL);
      break;
    case DownloadFileCount:
      gCbmrDynamicUIElements.DataLabels.DownloadFileCount->Base.GetControlBounds (gCbmrDynamicUIElements.DataLabels.DownloadFileCount, &LabelFrame);
      CbmrUIFillRect (LabelFrame, &gMsColorTable.FormCanvasBackgroundColor);
      Status = gCbmrDynamicUIElements.DataLabels.DownloadFileCount->UpdateLabelText (gCbmrDynamicUIElements.DataLabels.DownloadFileCount, String);
      gCbmrDynamicUIElements.DataLabels.DownloadFileCount->Base.Draw (gCbmrDynamicUIElements.DataLabels.DownloadFileCount, FALSE, NULL, NULL);
      break;
    case DownloadTotalSize:
      gCbmrDynamicUIElements.DataLabels.DownloadTotalSize->Base.GetControlBounds (gCbmrDynamicUIElements.DataLabels.DownloadTotalSize, &LabelFrame);
      CbmrUIFillRect (LabelFrame, &gMsColorTable.FormCanvasBackgroundColor);
      Status = gCbmrDynamicUIElements.DataLabels.DownloadTotalSize->UpdateLabelText (gCbmrDynamicUIElements.DataLabels.DownloadTotalSize, String);
      gCbmrDynamicUIElements.DataLabels.DownloadTotalSize->Base.Draw (gCbmrDynamicUIElements.DataLabels.DownloadTotalSize, FALSE, NULL, NULL);
      break;
    case NetworkState:
      gCbmrDynamicUIElements.DataLabels.NetworkState->Base.GetControlBounds (gCbmrDynamicUIElements.DataLabels.NetworkState, &LabelFrame);
      CbmrUIFillRect (LabelFrame, &gMsColorTable.FormCanvasBackgroundColor);
      Status = gCbmrDynamicUIElements.DataLabels.NetworkState->UpdateLabelText (gCbmrDynamicUIElements.DataLabels.NetworkState, String);
      gCbmrDynamicUIElements.DataLabels.NetworkState->Base.Draw (gCbmrDynamicUIElements.DataLabels.NetworkState, FALSE, NULL, NULL);
      break;
    case NetworkSSID:
      gCbmrDynamicUIElements.DataLabels.NetworkSSID->Base.GetControlBounds (gCbmrDynamicUIElements.DataLabels.NetworkSSID, &LabelFrame);
      CbmrUIFillRect (LabelFrame, &gMsColorTable.FormCanvasBackgroundColor);
      Status = gCbmrDynamicUIElements.DataLabels.NetworkSSID->UpdateLabelText (gCbmrDynamicUIElements.DataLabels.NetworkSSID, String);
      gCbmrDynamicUIElements.DataLabels.NetworkSSID->Base.Draw (gCbmrDynamicUIElements.DataLabels.NetworkSSID, FALSE, NULL, NULL);
      break;
    case NetworkPolicy:
      gCbmrDynamicUIElements.DataLabels.NetworkPolicy->Base.GetControlBounds (gCbmrDynamicUIElements.DataLabels.NetworkPolicy, &LabelFrame);
      CbmrUIFillRect (LabelFrame, &gMsColorTable.FormCanvasBackgroundColor);
      Status = gCbmrDynamicUIElements.DataLabels.NetworkPolicy->UpdateLabelText (gCbmrDynamicUIElements.DataLabels.NetworkPolicy, String);
      gCbmrDynamicUIElements.DataLabels.NetworkPolicy->Base.Draw (gCbmrDynamicUIElements.DataLabels.NetworkPolicy, FALSE, NULL, NULL);
      break;
    case NetworkIPAddr:
      gCbmrDynamicUIElements.DataLabels.NetworkIPAddr->Base.GetControlBounds (gCbmrDynamicUIElements.DataLabels.NetworkIPAddr, &LabelFrame);
      CbmrUIFillRect (LabelFrame, &gMsColorTable.FormCanvasBackgroundColor);
      Status = gCbmrDynamicUIElements.DataLabels.NetworkIPAddr->UpdateLabelText (gCbmrDynamicUIElements.DataLabels.NetworkIPAddr, String);
      gCbmrDynamicUIElements.DataLabels.NetworkIPAddr->Base.Draw (gCbmrDynamicUIElements.DataLabels.NetworkIPAddr, FALSE, NULL, NULL);
      break;
    case NetworkGatewayAddr:
      gCbmrDynamicUIElements.DataLabels.NetworkGatewayAddr->Base.GetControlBounds (gCbmrDynamicUIElements.DataLabels.NetworkGatewayAddr, &LabelFrame);
      CbmrUIFillRect (LabelFrame, &gMsColorTable.FormCanvasBackgroundColor);
      Status = gCbmrDynamicUIElements.DataLabels.NetworkGatewayAddr->UpdateLabelText (gCbmrDynamicUIElements.DataLabels.NetworkGatewayAddr, String);
      gCbmrDynamicUIElements.DataLabels.NetworkGatewayAddr->Base.Draw (gCbmrDynamicUIElements.DataLabels.NetworkGatewayAddr, FALSE, NULL, NULL);
      break;
    case NetworkDNSAddr:
      gCbmrDynamicUIElements.DataLabels.NetworkDNSAddr->Base.GetControlBounds (gCbmrDynamicUIElements.DataLabels.NetworkDNSAddr, &LabelFrame);
      CbmrUIFillRect (LabelFrame, &gMsColorTable.FormCanvasBackgroundColor);
      Status = gCbmrDynamicUIElements.DataLabels.NetworkDNSAddr->UpdateLabelText (gCbmrDynamicUIElements.DataLabels.NetworkDNSAddr, String);
      gCbmrDynamicUIElements.DataLabels.NetworkDNSAddr->Base.Draw (gCbmrDynamicUIElements.DataLabels.NetworkDNSAddr, FALSE, NULL, NULL);
      break;
    default:
      Status = EFI_INVALID_PARAMETER;
  }

  return Status;
}

EFI_STATUS
EFIAPI
CbmrUICreateWindow (
  Canvas  **WindowCanvas
  )
{
  EFI_STATUS                     Status         = EFI_SUCCESS;
  UINT32                         OSKMode        = 0;
  UINT32                         VerticalOffset = 0;
  SWM_RECT                       WindowRect;
  MS_ONSCREEN_KEYBOARD_PROTOCOL  *mOSKProtocol = NULL;

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
    Status       = EFI_UNSUPPORTED;
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to find the window manager protocol (%r).\r\n", Status));

    goto Exit;
  }

  // Locate simple text input protocol.
  //
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
  WindowRect.Right  = (gAppContext.HorizontalResolution - 1);
  WindowRect.Bottom = (gAppContext.VerticalResolution - 1);

  // Register with the Simple Window Manager to get mouse and touch input events.
  //
  Status = mSWMProtocol->RegisterClient (
                           mSWMProtocol,
                           gImageHandle,
                           SWM_Z_ORDER_CLIENT,
                           &WindowRect,
                           NULL,
                           NULL,
                           &gCbmrPointerProtocol,
                           &gCbmrPaintEvent
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
  Canvas  *LocalWindowCanvas = new_Canvas (
                                 WindowRect,
                                 &gMsColorTable.FormCanvasBackgroundColor
                                 );

  if (NULL == LocalWindowCanvas) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to create application canvas: %r.\r\n", Status));
    goto Exit;
  }

  // Start the Vertical Offset at 5% screen height from the top.
  //
  VerticalOffset = ((gAppContext.VerticalResolution * 5) / 100);

  // Create a company bitmap element from the file embedded in the UEFI resource section.
  //
  // NOTE: insert into your platform FDF file a reference to the company logo bitmap.  Something like this:
  //  # cBMR application company logo bitmap image.
  // FILE FREEFORM = PCD(gOemPkgTokenSpaceGuid.PcdCloudBMRCompanyLogoFile) {
  //   SECTION RAW = OemPkg/CloudBMR/Application/CbmrSampleUIApp/Resources/WindowsLogo.bmp
  // }
  //
  Bitmap  *CompanyLogoBitmap = CbmrUIFetchBitmap (0, 0, PcdGetPtr (PcdCloudBMRCompanyLogoFile));

  // Get the size of the bitmap.
  //
  UINT32  LogoBitmapHeight = 128;  // Set a minimum standard size for the logo bitmap (pixels).

  if (CompanyLogoBitmap != NULL) {
    SWM_RECT  LogoBitmapFrame;
    CompanyLogoBitmap->Base.GetControlBounds (CompanyLogoBitmap, &LogoBitmapFrame);
    LogoBitmapHeight = (LogoBitmapFrame.Bottom - LogoBitmapFrame.Top + 1);
  }

  // Create a header grid for the company logo and header text.  Grid height needs to be enough to accomodate the
  // company logo bitmap (the tallest element).
  //
  SWM_RECT  HeaderGridRect = { WindowRect.Left, VerticalOffset, WindowRect.Right, (VerticalOffset + LogoBitmapHeight) };
  Grid      *HeaderGrid    = new_Grid (LocalWindowCanvas, HeaderGridRect, 1, 8, FALSE);

  LocalWindowCanvas->AddControl (LocalWindowCanvas, FALSE, TRUE, (VOID *)HeaderGrid);
  VerticalOffset += (LogoBitmapHeight + SECTION_VERTICAL_PADDING_PIXELS);

  // Add the company logo bitmap to the grid.
  //
  HeaderGrid->AddControl (HeaderGrid, FALSE, FALSE, 0, (gAppContext.HorizontalResolution <= 800 ? 0 : 1), (VOID *)CompanyLogoBitmap);

  // Define the header font.
  //
  EFI_FONT_INFO  HeadingFontInfo;

  HeadingFontInfo.FontSize    = SWM_MB_CUSTOM_FONT_CAPTION_HEIGHT;
  HeadingFontInfo.FontStyle   = EFI_HII_FONT_STYLE_NORMAL;
  HeadingFontInfo.FontName[0] = L'\0';

  // Add title text to the grid.
  //
  HeaderGrid->AddControl (HeaderGrid, FALSE, FALSE, 0, 2, (VOID *)new_Label (0, 0, 800, SWM_MB_CUSTOM_FONT_CAPTION_HEIGHT, &HeadingFontInfo, &gMsColorTable.LabelTextLargeColor, &gMsColorTable.FormCanvasBackgroundColor, L"Cloud Bare Metal Recovery"));

  // Define the body font.
  //
  EFI_FONT_INFO  BodyFontInfo;

  BodyFontInfo.FontSize    = SWM_MB_CUSTOM_FONT_BODY_HEIGHT;
  BodyFontInfo.FontStyle   = EFI_HII_FONT_STYLE_NORMAL;
  BodyFontInfo.FontName[0] = L'\0';

  // Create cBMR state grid (3 rows of text).
  //
  SWM_RECT  StateGridRect = { WindowRect.Left, VerticalOffset, WindowRect.Right, (VerticalOffset + ((SWM_MB_CUSTOM_FONT_BODY_HEIGHT + NORMAL_VERTICAL_PADDING_PIXELS) * 3)) };
  Grid      *StateGrid    = new_Grid (LocalWindowCanvas, StateGridRect, 3, 4, FALSE);

  VerticalOffset += (((SWM_MB_CUSTOM_FONT_BODY_HEIGHT + NORMAL_VERTICAL_PADDING_PIXELS) * 3) + SECTION_VERTICAL_PADDING_PIXELS);
  LocalWindowCanvas->AddControl (LocalWindowCanvas, FALSE, TRUE, (VOID *)StateGrid);

  // Add state, download file count, and total download size to state grid.
  //
  StateGrid->AddControl (StateGrid, FALSE, FALSE, 0, 1, (VOID *)new_Label (0, 0, 500, SWM_MB_CUSTOM_FONT_BODY_HEIGHT, &BodyFontInfo, &gMsColorTable.LabelTextNormalColor, &gMsColorTable.FormCanvasBackgroundColor, L"Stage:"));
  StateGrid->AddControl (StateGrid, FALSE, FALSE, 1, 1, (VOID *)new_Label (0, 0, 500, SWM_MB_CUSTOM_FONT_BODY_HEIGHT, &BodyFontInfo, &gMsColorTable.LabelTextNormalColor, &gMsColorTable.FormCanvasBackgroundColor, L"Number of Files:"));
  StateGrid->AddControl (StateGrid, FALSE, FALSE, 2, 1, (VOID *)new_Label (0, 0, 500, SWM_MB_CUSTOM_FONT_BODY_HEIGHT, &BodyFontInfo, &gMsColorTable.LabelTextNormalColor, &gMsColorTable.FormCanvasBackgroundColor, L"Total Size:"));

  gCbmrDynamicUIElements.DataLabels.CbmrState = new_Label (0, 0, 500, SWM_MB_CUSTOM_FONT_BODY_HEIGHT, &BodyFontInfo, &gMsColorTable.LabelTextLargeColor, &gMsColorTable.FormCanvasBackgroundColor, L" ");
  StateGrid->AddControl (StateGrid, FALSE, FALSE, 0, 2, (VOID *)gCbmrDynamicUIElements.DataLabels.CbmrState);
  gCbmrDynamicUIElements.DataLabels.DownloadFileCount = new_Label (0, 0, 500, SWM_MB_CUSTOM_FONT_BODY_HEIGHT, &BodyFontInfo, &gMsColorTable.LabelTextLargeColor, &gMsColorTable.FormCanvasBackgroundColor, L"-");
  StateGrid->AddControl (StateGrid, FALSE, FALSE, 1, 2, (VOID *)gCbmrDynamicUIElements.DataLabels.DownloadFileCount);
  gCbmrDynamicUIElements.DataLabels.DownloadTotalSize = new_Label (0, 0, 500, SWM_MB_CUSTOM_FONT_BODY_HEIGHT, &BodyFontInfo, &gMsColorTable.LabelTextLargeColor, &gMsColorTable.FormCanvasBackgroundColor, L"-");
  StateGrid->AddControl (StateGrid, FALSE, FALSE, 2, 2, (VOID *)gCbmrDynamicUIElements.DataLabels.DownloadTotalSize);

  // Create network status grid (6 rows of text).
  //
  SWM_RECT  NetworkStatusGridRect = { WindowRect.Left, VerticalOffset, WindowRect.Right, (VerticalOffset + ((SWM_MB_CUSTOM_FONT_BODY_HEIGHT + NORMAL_VERTICAL_PADDING_PIXELS) * 6)) };
  Grid      *NetworkStatusGrid    = new_Grid (LocalWindowCanvas, NetworkStatusGridRect, 6, 4, FALSE);

  VerticalOffset += (((SWM_MB_CUSTOM_FONT_BODY_HEIGHT + NORMAL_VERTICAL_PADDING_PIXELS) * 6) + SECTION_VERTICAL_PADDING_PIXELS);
  LocalWindowCanvas->AddControl (LocalWindowCanvas, FALSE, TRUE, (VOID *)NetworkStatusGrid);

  // Add network state, SSID, policy, IP address, Gateway address, and DNS server address to network status grid.
  //
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 0, 1, (VOID *)new_Label (0, 0, 500, SWM_MB_CUSTOM_FONT_BODY_HEIGHT, &BodyFontInfo, &gMsColorTable.LabelTextNormalColor, &gMsColorTable.FormCanvasBackgroundColor, L"Network:"));
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 1, 1, (VOID *)new_Label (0, 0, 500, SWM_MB_CUSTOM_FONT_BODY_HEIGHT, &BodyFontInfo, &gMsColorTable.LabelTextNormalColor, &gMsColorTable.FormCanvasBackgroundColor, L"SSID:"));
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 2, 1, (VOID *)new_Label (0, 0, 500, SWM_MB_CUSTOM_FONT_BODY_HEIGHT, &BodyFontInfo, &gMsColorTable.LabelTextNormalColor, &gMsColorTable.FormCanvasBackgroundColor, L"Policy:"));
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 3, 1, (VOID *)new_Label (0, 0, 500, SWM_MB_CUSTOM_FONT_BODY_HEIGHT, &BodyFontInfo, &gMsColorTable.LabelTextNormalColor, &gMsColorTable.FormCanvasBackgroundColor, L"IP Address:"));
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 4, 1, (VOID *)new_Label (0, 0, 500, SWM_MB_CUSTOM_FONT_BODY_HEIGHT, &BodyFontInfo, &gMsColorTable.LabelTextNormalColor, &gMsColorTable.FormCanvasBackgroundColor, L"Gateway:"));
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 5, 1, (VOID *)new_Label (0, 0, 500, SWM_MB_CUSTOM_FONT_BODY_HEIGHT, &BodyFontInfo, &gMsColorTable.LabelTextNormalColor, &gMsColorTable.FormCanvasBackgroundColor, L"DNS Server:"));

  gCbmrDynamicUIElements.DataLabels.NetworkState = new_Label (0, 0, 500, SWM_MB_CUSTOM_FONT_BODY_HEIGHT, &BodyFontInfo, &gMsColorTable.LabelTextLargeColor, &gMsColorTable.FormCanvasBackgroundColor, L"Disconnected");
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 0, 2, (VOID *)gCbmrDynamicUIElements.DataLabels.NetworkState);
  gCbmrDynamicUIElements.DataLabels.NetworkSSID = new_Label (0, 0, 500, SWM_MB_CUSTOM_FONT_BODY_HEIGHT, &BodyFontInfo, &gMsColorTable.LabelTextLargeColor, &gMsColorTable.FormCanvasBackgroundColor, L"-");
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 1, 2, (VOID *)gCbmrDynamicUIElements.DataLabels.NetworkSSID);
  gCbmrDynamicUIElements.DataLabels.NetworkPolicy = new_Label (0, 0, 500, SWM_MB_CUSTOM_FONT_BODY_HEIGHT, &BodyFontInfo, &gMsColorTable.LabelTextLargeColor, &gMsColorTable.FormCanvasBackgroundColor, L"-");
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 2, 2, (VOID *)gCbmrDynamicUIElements.DataLabels.NetworkPolicy);
  gCbmrDynamicUIElements.DataLabels.NetworkIPAddr = new_Label (0, 0, 500, SWM_MB_CUSTOM_FONT_BODY_HEIGHT, &BodyFontInfo, &gMsColorTable.LabelTextLargeColor, &gMsColorTable.FormCanvasBackgroundColor, L"-");
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 3, 2, (VOID *)gCbmrDynamicUIElements.DataLabels.NetworkIPAddr);
  gCbmrDynamicUIElements.DataLabels.NetworkGatewayAddr = new_Label (0, 0, 500, SWM_MB_CUSTOM_FONT_BODY_HEIGHT, &BodyFontInfo, &gMsColorTable.LabelTextLargeColor, &gMsColorTable.FormCanvasBackgroundColor, L"-");
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 4, 2, (VOID *)gCbmrDynamicUIElements.DataLabels.NetworkGatewayAddr);
  gCbmrDynamicUIElements.DataLabels.NetworkDNSAddr = new_Label (0, 0, 500, SWM_MB_CUSTOM_FONT_BODY_HEIGHT, &BodyFontInfo, &gMsColorTable.LabelTextLargeColor, &gMsColorTable.FormCanvasBackgroundColor, L"-");
  NetworkStatusGrid->AddControl (NetworkStatusGrid, FALSE, FALSE, 5, 2, (VOID *)gCbmrDynamicUIElements.DataLabels.NetworkDNSAddr);

  // Create download progress bar grid (1 row of text).
  //
  SWM_RECT  DownloadProgressGridRect = { WindowRect.Left, VerticalOffset, WindowRect.Right, (VerticalOffset + (SWM_MB_CUSTOM_FONT_BODY_HEIGHT + NORMAL_VERTICAL_PADDING_PIXELS)) };
  Grid      *DownloadProgressGrid    = new_Grid (LocalWindowCanvas, DownloadProgressGridRect, 1, 4, FALSE);

  VerticalOffset += (SWM_MB_CUSTOM_FONT_BODY_HEIGHT + NORMAL_VERTICAL_PADDING_PIXELS + SECTION_VERTICAL_PADDING_PIXELS);
  LocalWindowCanvas->AddControl (LocalWindowCanvas, FALSE, TRUE, (VOID *)DownloadProgressGrid);

  // Add download progress title text to grid.
  //
  DownloadProgressGrid->AddControl (DownloadProgressGrid, FALSE, FALSE, 0, 1, (VOID *)new_Label (0, 0, 500, SWM_MB_CUSTOM_FONT_BODY_HEIGHT, &BodyFontInfo, &gMsColorTable.LabelTextNormalColor, &gMsColorTable.FormCanvasBackgroundColor, L"Download %"));

  // Add download progress bar to grid.
  //
  gCbmrDynamicUIElements.DownloadProgress = new_ProgressBar (0, 0, 300, 5, &gMsColorTable.LabelTextLargeColor, &gMsColorTable.MasterFrameBackgroundColor, 0);
  DownloadProgressGrid->AddControl (DownloadProgressGrid, FALSE, FALSE, 0, 2, (VOID *)gCbmrDynamicUIElements.DownloadProgress);

  // Create buttons to start recovery and to cancel.
  //
  Button  *GoButton = new_Button (
                        (gAppContext.HorizontalResolution / 2) - (300 + 40),
                        VerticalOffset,
                        300,
                        SWM_MB_CUSTOM_FONT_BODY_HEIGHT + 40,
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

  LocalWindowCanvas->AddControl (LocalWindowCanvas, TRUE, FALSE, (VOID *)GoButton);

  Button  *CancelButton = new_Button (
                            (gAppContext.HorizontalResolution / 2) + 40,
                            VerticalOffset,
                            300,
                            SWM_MB_CUSTOM_FONT_BODY_HEIGHT + 40,
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

  LocalWindowCanvas->AddControl (LocalWindowCanvas, TRUE, FALSE, (VOID *)CancelButton);

  LocalWindowCanvas->SetHighlight (
                       LocalWindowCanvas,
                       GoButton
                       );

  LocalWindowCanvas->SetDefaultControl (
                       LocalWindowCanvas,
                       (VOID *)GoButton
                       );

  *WindowCanvas = LocalWindowCanvas;

Exit:

  return Status;
}

SWM_MB_RESULT
ProcessWindowInput (
  IN  MS_SIMPLE_WINDOW_MANAGER_PROTOCOL  *this,
  IN  Canvas                             *WindowCanvas,
  IN  EFI_ABSOLUTE_POINTER_PROTOCOL      *PointerProtocol,
  IN  UINT64                             Timeout
  )
{
  EFI_STATUS       Status = EFI_SUCCESS;
  UINTN            Index;
  OBJECT_STATE     State        = NORMAL;
  SWM_MB_RESULT    ButtonResult = 0;
  VOID             *pContext    = NULL;
  SWM_INPUT_STATE  InputState;
  UINTN            NumberOfEvents = 2;

  EFI_EVENT  WaitEvents[2];

  // Wait for user input.
  //
  WaitEvents[0] = gSimpleTextInEx->WaitForKeyEx;
  WaitEvents[1] = PointerProtocol->WaitForInput;

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
      ButtonResult = (SWM_MB_RESULT)(UINTN)pContext;

      // If user clicked either of the buttons, exit.
      if ((SWM_MB_IDCANCEL == ButtonResult) || (SWM_MB_IDOK == ButtonResult)) {
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

        // If user pressed SHIFT-TAB, move the highlight to the previous control.
        //
        if ((CHAR_TAB == InputState.State.KeyState.Key.UnicodeChar) && (0 != (InputState.State.KeyState.KeyState.KeyShiftState & (EFI_LEFT_SHIFT_PRESSED | EFI_RIGHT_SHIFT_PRESSED)))) {
          // Send the key to the form canvas for processing.
          //
          Status = WindowCanvas->MoveHighlight (
                                   WindowCanvas,
                                   FALSE
                                   );

          // If the highlight moved past the top control, clear control highlight and try again - this will wrap the highlight around
          // to the bottom.  The reason we don't do this automatically is because in other
          // scenarios, the TAB order needs to include controls outside the canvas (ex:
          // the Front Page's Top-Menu.
          //
          if (EFI_NOT_FOUND == Status) {
            WindowCanvas->ClearHighlight (WindowCanvas);

            Status = WindowCanvas->MoveHighlight (
                                     WindowCanvas,
                                     FALSE
                                     );
          }

          continue;
        }

        // If user pressed TAB, move the highlight to the next control.
        //
        if (CHAR_TAB == InputState.State.KeyState.Key.UnicodeChar) {
          // Send the key to the form canvas for processing.
          //
          Status = WindowCanvas->MoveHighlight (
                                   WindowCanvas,
                                   TRUE
                                   );

          // If we moved the highlight to the end of the list of controls, move it back
          // to the top by clearing teh current highlight and moving to next.  The reason we don't do
          // this automatically is because in other scenarios, the TAB order needs to include controls
          // outside the canvas (ex: the Front Page's Top-Menu.
          //
          if (EFI_NOT_FOUND == Status) {
            WindowCanvas->ClearHighlight (WindowCanvas);

            Status = WindowCanvas->MoveHighlight (
                                     WindowCanvas,
                                     TRUE
                                     );
          }

          continue;
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
  Canvas  *WindowCanvas
  )
{
  return ProcessWindowInput (
           mSWMProtocol,
           WindowCanvas,
           gCbmrPointerProtocol,
           0
           );
}
