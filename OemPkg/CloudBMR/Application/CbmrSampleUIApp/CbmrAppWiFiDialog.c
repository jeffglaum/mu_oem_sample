/** @file CbmrAppWiFiDialog.c

  cBMR (Cloud Bare Metal Recovery) sample application Wi-Fi dialog implementation.  The dialog
  is used to present a list of available access points that the user can select from and optionally
  takes a password for the selected access point.

  Copyright (c) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  The application is a sample, demonstrating how one might present the cBMR process to a user.
**/
#include "CbmrApp.h"

static MS_SIMPLE_WINDOW_MANAGER_PROTOCOL  *mSWMProtocol;

static SWM_RECT                       DialogRect;
static EFI_ABSOLUTE_POINTER_PROTOCOL  *mCbmrPointerProtocol;
static EFI_EVENT                      mCbmrPaintEvent;

ListBox  *WifiSSIDList    = NULL;
EditBox  *PasswordEditBox = NULL;

static EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *gSimpleTextInEx;

extern EFI_HANDLE        gDialogHandle;
extern CBMR_APP_CONTEXT  gAppContext;

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
                         gDialogHandle,
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

static EFI_STATUS
EFIAPI
CbmrUICreateWiFiDialog (
  SWM_RECT         *DialogFrame,
  UIT_LB_CELLDATA  *WifiOptionCells,
  Canvas           **pDialogCanvas
  )
{
  EFI_STATUS  Status         = EFI_SUCCESS;
  UINT32      VerticalOffset = 0;

  mSWMProtocol->BltWindow (
                  mSWMProtocol,
                  gDialogHandle,
                  &gMsColorTable.MessageBoxBackgroundColor,
                  EfiBltVideoFill,
                  0,
                  0,
                  DialogRect.Left,
                  DialogRect.Top,
                  (DialogRect.Right - DialogRect.Left + 1),
                  (DialogRect.Bottom - DialogRect.Top + 1),
                  0
                  );

  // Create a canvas for presenting the wi-fi dialog elements.
  //
  Canvas  *DialogCanvas = new_Canvas (
                            DialogRect,
                            &gMsColorTable.MessageBoxBackgroundColor
                            );

  if (NULL == DialogCanvas) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to create wi-fi dialog canvas: %r.\r\n", Status));
    goto Exit;
  }

  // Vertical offset for the first UI element is at 5% of the total screen height.
  //
  VerticalOffset = ((gAppContext.VerticalResolution * 5) / 100);

  EFI_FONT_INFO  BodyFontInfo;

  BodyFontInfo.FontSize    = SWM_MB_CUSTOM_FONT_BODY_HEIGHT;
  BodyFontInfo.FontStyle   = EFI_HII_FONT_STYLE_NORMAL;
  BodyFontInfo.FontName[0] = L'\0';

  DialogCanvas->AddControl (DialogCanvas, FALSE, FALSE, (VOID *)new_Label (DialogRect.Left + 20, VerticalOffset, 800, SWM_MB_CUSTOM_FONT_BODY_HEIGHT, &BodyFontInfo, &gMsColorTable.MessageBoxTextColor, &gMsColorTable.MessageBoxBackgroundColor, L"Unable to find a wired LAN connection."));
  VerticalOffset += (SWM_MB_CUSTOM_FONT_BODY_HEIGHT + NORMAL_VERTICAL_PADDING_PIXELS);
  DialogCanvas->AddControl (DialogCanvas, FALSE, FALSE, (VOID *)new_Label (DialogRect.Left + 20, VerticalOffset, 800, SWM_MB_CUSTOM_FONT_BODY_HEIGHT, &BodyFontInfo, &gMsColorTable.MessageBoxTextColor, &gMsColorTable.MessageBoxBackgroundColor, L"Available Wi-Fi networks:"));
  VerticalOffset += (SWM_MB_CUSTOM_FONT_BODY_HEIGHT + NORMAL_VERTICAL_PADDING_PIXELS + SECTION_VERTICAL_PADDING_PIXELS);

  #define SWM_SS_LISTBOX_CELL_HEIGHT         MsUiScaleByTheme (80)
  #define SWM_SS_LISTBOX_CELL_TEXT_X_OFFSET  MsUiScaleByTheme (10)
  #define SWM_SS_LISTBOX_CELL_WIDTH          MsUiScaleByTheme (700)

  WifiSSIDList = new_ListBox (
                   DialogRect.Left + 20,
                   VerticalOffset,
                   SWM_SS_LISTBOX_CELL_WIDTH,
                   SWM_SS_LISTBOX_CELL_HEIGHT,
                   0,                         // Flags
                   &BodyFontInfo,
                   SWM_SS_LISTBOX_CELL_TEXT_X_OFFSET,
                   &gMsColorTable.SingleSelectDialogButtonTextColor,
                   &gMsColorTable.SingleSelectDialogButtonHoverColor,
                   &gMsColorTable.SingleSelectDialogButtonSelectColor,
                   &gMsColorTable.SingleSelectDialogListBoxGreyoutColor,
                   WifiOptionCells,
                   NULL
                   );

  DialogCanvas->AddControl (DialogCanvas, TRUE, FALSE, (VOID *)WifiSSIDList);

  // Get total list box height.
  //
  SWM_RECT  ListBoxFrame;
  WifiSSIDList->Base.GetControlBounds (WifiSSIDList, &ListBoxFrame);
  VerticalOffset += ((ListBoxFrame.Bottom - ListBoxFrame.Top + 1) + SECTION_VERTICAL_PADDING_PIXELS);

  DialogCanvas->AddControl (DialogCanvas, FALSE, FALSE, (VOID *)new_Label (DialogRect.Left + 20, VerticalOffset, 500, SWM_MB_CUSTOM_FONT_BODY_HEIGHT, &BodyFontInfo, &gMsColorTable.MessageBoxTextColor, &gMsColorTable.MessageBoxBackgroundColor, L"Network Password:"));
  VerticalOffset += (SWM_MB_CUSTOM_FONT_BODY_HEIGHT + SECTION_VERTICAL_PADDING_PIXELS);

  #define SWM_PWD_DIALOG_MAX_PWD_DISPLAY_CHARS  15                        // Password dialog maximum number of password characters to display (editbox size).

  PasswordEditBox =   new_EditBox (
                        DialogRect.Left + 20,
                        VerticalOffset,
                        SWM_PWD_DIALOG_MAX_PWD_DISPLAY_CHARS,
                        UIT_EDITBOX_TYPE_PASSWORD,
                        &BodyFontInfo,
                        &gMsColorTable.EditBoxNormalColor,
                        &gMsColorTable.EditBoxTextColor,
                        &gMsColorTable.EditBoxGrayoutColor,
                        &gMsColorTable.EditBoxTextGrayoutColor,
                        &gMsColorTable.EditBoxSelectColor,
                        L"Password",
                        NULL
                        );

  DialogCanvas->AddControl (DialogCanvas, TRUE, FALSE, (VOID *)PasswordEditBox);

  // Get total edit box height.
  //
  SWM_RECT  EditBoxFrame;
  PasswordEditBox->Base.GetControlBounds (PasswordEditBox, &EditBoxFrame);
  VerticalOffset += ((EditBoxFrame.Bottom - EditBoxFrame.Top + 1) + (SECTION_VERTICAL_PADDING_PIXELS * 2));

  Button  *ConnectButton = new_Button (
                             (DialogRect.Left + 120),
                             VerticalOffset,
                             300,
                             SWM_MB_CUSTOM_FONT_BODY_HEIGHT + 40,
                             &BodyFontInfo,
                             &gMsColorTable.DefaultDialogBackGroundColor,
                             &gMsColorTable.DefaultDialogButtonHoverColor,
                             &gMsColorTable.DefaultDialogButtonSelectColor,
                             &gMsColorTable.DefaultDialogButtonGrayOutColor,
                             &gMsColorTable.DefaultDialogButtonRingColor,
                             &gMsColorTable.DefaultDialogButtonTextColor,
                             &gMsColorTable.DefaultDialogButtonSelectTextColor,
                             L"Connect",
                             (VOID *)(UINTN)SWM_MB_IDOK
                             );

  DialogCanvas->AddControl (DialogCanvas, TRUE, FALSE, (VOID *)ConnectButton);

  DialogCanvas->SetHighlight (
                  DialogCanvas,
                  WifiSSIDList
                  );

  DialogCanvas->SetDefaultControl (
                  DialogCanvas,
                  (VOID *)ConnectButton
                  );

  *pDialogCanvas = DialogCanvas;

Exit:

  return Status;
}

EFI_STATUS
EFIAPI
CbmrUIGetSSIDAndPassword (
  OUT CHAR16  *SSIDName,
  IN UINT8    SSIDNameMaxLength,
  OUT CHAR16  *SSIDPassword,
  IN UINT8    SSIDPasswordMaxLength
  )
{
  EFI_STATUS                               Status                 = EFI_SUCCESS;
  BOOLEAN                                  DialogWindowRegistered = FALSE;
  Canvas                                   *WiFiDialogCanvas      = NULL;
  UIT_LB_CELLDATA                          *WifiOptionCells       = NULL;
  EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL  *WiFi2Protocol         = NULL;
  EFI_80211_GET_NETWORKS_RESULT            *NetworkList           = NULL;

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
    DEBUG ((DEBUG_ERROR, "ERROR [FP]: Failed to find the window manager protocol (%r).\r\n", Status));
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

  // Locate the WiFi2 protocol.
  //
  Status = gBS->LocateProtocol (&gEfiWiFi2ProtocolGuid, NULL, (VOID **)&WiFi2Protocol);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to locate WiFi2 protocol (%r).\r\n", Status));
    goto Exit;
  }

  // Retrieve an EFI_80211_GET_NETWORKS_RESULT structure that indicates all networks in range.
  // NetworkList is allocated memory which must be freed.
  //
  Status = GetWiFiNetworkList (WiFi2Protocol, &NetworkList);

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to get active Wi-Fi SSID list (%r).\r\n", Status));
    goto Exit;
  }

  // For presentation purposes, since the list box isn't scrollable, limit the number of SSIDs presented.
  //
  UINT8  NumberOfWiFiNetworks = (NetworkList->NumOfNetworkDesc > 5 ? 5 : NetworkList->NumOfNetworkDesc);

  // IMPORTANT: Allocate one additional entry so the list is null terminated.
  //
  WifiOptionCells = AllocateZeroPool ((NumberOfWiFiNetworks + 1) * sizeof (UIT_LB_CELLDATA));

  if (WifiOptionCells == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  CHAR8  SSIDNameA[EFI_MAX_SSID_LEN + 1];

  for (UINTN i = 0; i < NumberOfWiFiNetworks; i++) {
    WifiOptionCells[i].CheckBoxSelected = FALSE;
    WifiOptionCells[i].TrashcanEnabled  = FALSE;
    WifiOptionCells[i].CellText         = AllocateZeroPool (sizeof (CHAR16) * (EFI_MAX_SSID_LEN + 1));

    SSIdNameToStr (&NetworkList->NetworkDesc[i].Network.SSId, SSIDNameA);

    AsciiStrToUnicodeStrS (SSIDNameA, WifiOptionCells[i].CellText, (EFI_MAX_SSID_LEN + 1));
  }

  // Change UI toolkit handle to dialog handle.
  InitializeUIToolKit (gDialogHandle);

  // Calculate pop-up dialog frame size.
  //
  DialogRect.Left   = (gAppContext.HorizontalResolution / 4);
  DialogRect.Top    = 0;
  DialogRect.Right  = (DialogRect.Left + (gAppContext.HorizontalResolution / 2));
  DialogRect.Bottom = (gAppContext.VerticalResolution - 1);

  // Register with the Simple Window Manager to get mouse and touch input events.
  //
  Status = mSWMProtocol->RegisterClient (
                           mSWMProtocol,
                           gDialogHandle,
                           SWM_Z_ORDER_POPUP,
                           &DialogRect,
                           NULL,
                           NULL,
                           &mCbmrPointerProtocol,
                           &mCbmrPaintEvent
                           );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "ERROR [cBMR App]: Failed to register wi-fi dialog as a SWM client: %r.\r\n", Status));
    goto Exit;
  }

  DialogWindowRegistered = TRUE;
  mSWMProtocol->ActivateWindow (
                  mSWMProtocol,
                  gDialogHandle,
                  TRUE
                  );

  // Show Wi-Fi selection dialog.
  Status = CbmrUICreateWiFiDialog (
             &DialogRect,
             WifiOptionCells,
             &WiFiDialogCanvas
             );

  SWM_MB_RESULT  Result = ProcessWindowInput (
                            mSWMProtocol,
                            WiFiDialogCanvas,
                            mCbmrPointerProtocol,
                            0
                            );

  if (Result == SWM_MB_IDOK) {
    LB_RETURN_DATA  SelectedCellData;
    WifiSSIDList->GetSelectedCellIndex (WifiSSIDList, &SelectedCellData);
    StrCpyS (SSIDName, SSIDNameMaxLength, WifiOptionCells[SelectedCellData.SelectedCell].CellText);
    StrCpyS (SSIDPassword, SSIDPasswordMaxLength, PasswordEditBox->GetCurrentTextString (PasswordEditBox));

    StrCpyS (gAppContext.SSIDNameW, SSIDNameMaxLength, WifiOptionCells[SelectedCellData.SelectedCell].CellText);
    StrCpyS (gAppContext.SSIDPasswordW, SSIDPasswordMaxLength, PasswordEditBox->GetCurrentTextString (PasswordEditBox));

    Status = EFI_SUCCESS;
  }

Exit:

  // Deactivate and unregister with the window manager as a client.
  //
  if (DialogWindowRegistered) {
    mSWMProtocol->ActivateWindow (
                    mSWMProtocol,
                    gDialogHandle,
                    FALSE
                    );

    mSWMProtocol->UnregisterClient (
                    mSWMProtocol,
                    gDialogHandle
                    );
  }

  // Restore UI Handle to normal.
  //
  InitializeUIToolKit (gImageHandle);

  // Clean-up allocations.
  //
  if (WifiOptionCells != NULL) {
    for (UINT8 i = 0; WifiOptionCells[i].CellText != NULL; i++) {
      FreePool (WifiOptionCells[i].CellText);
    }

    FreePool (WifiOptionCells);
  }

  // Delete the dialog canvas and all associated UI elements on it.
  //
  if (WiFiDialogCanvas != NULL) {
    delete_Canvas (WiFiDialogCanvas);
  }

  return (Status);
}
