/** @file CbmrAppWiFiDialog.c

  cBMR Sample Application Wi-Fi dialog routines.

  Copyright (c) Microsoft Corporation. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  The application is intended to be a sample of how to present cBMR (Cloud Bare Metal Recovery) process to the end user.
**/
#include "CbmrApp.h"

#include <Pi/PiFirmwareFile.h>

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

static EFI_GRAPHICS_OUTPUT_PROTOCOL       *mGop;
static MS_SIMPLE_WINDOW_MANAGER_PROTOCOL  *mSWMProtocol;

static SWM_RECT                       DialogRect;
static EFI_ABSOLUTE_POINTER_PROTOCOL  *mCbmrPointerProtocol;
static EFI_EVENT                      mCbmrPaintEvent;

ListBox  *WifiSSIDList    = NULL;
EditBox  *PasswordEditBox = NULL;

//
// Boot video resolution and text mode.
//
static UINT32  mBootHorizontalResolution = 0;
static UINT32  mBootVerticalResolution   = 0;

static EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *gSimpleTextInEx;

extern EFI_HANDLE  gDialogHandle;

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
  EFI_STATUS  Status = EFI_SUCCESS;

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

  EFI_FONT_INFO  BodyFontInfo;

  BodyFontInfo.FontSize    = SWM_MB_CUSTOM_FONT_BODY_HEIGHT;
  BodyFontInfo.FontStyle   = EFI_HII_FONT_STYLE_NORMAL;
  BodyFontInfo.FontName[0] = L'\0';

  DialogCanvas->AddControl (DialogCanvas, FALSE, FALSE, (VOID *)new_Label (DialogRect.Left + 20, DialogRect.Top + 20, 300, 50, &BodyFontInfo, &gMsColorTable.MessageBoxTextColor, &gMsColorTable.MessageBoxBackgroundColor, L"Unable to find a wired LAN connection."));
  DialogCanvas->AddControl (DialogCanvas, FALSE, FALSE, (VOID *)new_Label (DialogRect.Left + 20, DialogRect.Top + 60, 300, 50, &BodyFontInfo, &gMsColorTable.MessageBoxTextColor, &gMsColorTable.MessageBoxBackgroundColor, L"Available Wi-Fi networks:"));

  #define SWM_SS_LISTBOX_CELL_HEIGHT         MsUiScaleByTheme (80)
  #define SWM_SS_LISTBOX_CELL_TEXT_X_OFFSET  MsUiScaleByTheme (10)
  #define SWM_SS_LISTBOX_CELL_WIDTH          MsUiScaleByTheme (700)

  WifiSSIDList = new_ListBox (
                   DialogRect.Left + 20,
                   DialogRect.Top + 100,
                   SWM_SS_LISTBOX_CELL_WIDTH,
                   SWM_SS_LISTBOX_CELL_HEIGHT,
                   0,                         // Flags
                   &BodyFontInfo,
                   SWM_SS_LISTBOX_CELL_TEXT_X_OFFSET,
                   &gMsColorTable.SingleSelectDialogButtonTextColor,
                   &gMsColorTable.SingleSelectDialogButtonHoverColor,
                   &gMsColorTable.SingleSelectDialogButtonSelectColor,
                   &gMsColorTable.SingleSelectDialogListBoxGreyoutColor,                                    // Grayed
                   WifiOptionCells,
                   NULL
                   );

  DialogCanvas->AddControl (
                  DialogCanvas,
                  TRUE,                // Not highlightable.
                  FALSE,               // Visible.
                  (VOID *)WifiSSIDList
                  );

  DialogCanvas->AddControl (DialogCanvas, FALSE, FALSE, (VOID *)new_Label (DialogRect.Left + 20, DialogRect.Top + 250, 300, 50, &BodyFontInfo, &gMsColorTable.MessageBoxTextColor, &gMsColorTable.MessageBoxBackgroundColor, L"Network Password:"));

  #define SWM_PWD_DIALOG_MAX_PWD_DISPLAY_CHARS  15                        // Password dialog maximum number of password characters to display (editbox size).

  PasswordEditBox =   new_EditBox (
                        DialogRect.Left + 20,
                        DialogRect.Top + 280,
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

  DialogCanvas->AddControl (
                  DialogCanvas,
                  TRUE,                // Not highlightable.
                  FALSE,               // Visible.
                  (VOID *)PasswordEditBox
                  );

  Button  *ConnectButton = new_Button (
                             (DialogRect.Left + 120),
                             (DialogRect.Top + 350),
                             150,
                             40,
                             &BodyFontInfo,
                             &gMsColorTable.DefaultDialogBackGroundColor,
                             &gMsColorTable.DefaultDialogButtonHoverColor,
                             &gMsColorTable.DefaultDialogButtonSelectColor,
                             &gMsColorTable.DefaultDialogButtonGrayOutColor,    // GrayOut.
                             &gMsColorTable.DefaultDialogButtonRingColor,       // Button ring.
                             &gMsColorTable.DefaultDialogButtonTextColor,       // Normal text.
                             &gMsColorTable.DefaultDialogButtonSelectTextColor, // Normal text.
                             L"Connect",
                             (VOID *)(UINTN)SWM_MB_IDOK
                             );

  DialogCanvas->AddControl (
                  DialogCanvas,
                  TRUE,                // Not highlightable.
                  FALSE,               // Visible.
                  (VOID *)ConnectButton
                  );

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

static
SWM_MB_RESULT
ProcessDialogInput (
  IN  MS_SIMPLE_WINDOW_MANAGER_PROTOCOL  *this,
  IN  Canvas                             *DialogCanvas,
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
    State = DialogCanvas->Base.Draw (
                                 DialogCanvas,
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
      if (SWM_MB_IDOK == ButtonResult) {
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

        // If the user pressed Enter, proceed with cBMR.
        //
        if (CHAR_CARRIAGE_RETURN == InputState.State.KeyState.Key.UnicodeChar) {
          ButtonResult = SWM_MB_IDOK;
          break;
        }

        // If user pressed SHIFT-TAB, move the highlight to the previous control.
        //
        if ((CHAR_TAB == InputState.State.KeyState.Key.UnicodeChar) && (0 != (InputState.State.KeyState.KeyState.KeyShiftState & (EFI_LEFT_SHIFT_PRESSED | EFI_RIGHT_SHIFT_PRESSED)))) {
          // Send the key to the form canvas for processing.
          //
          Status = DialogCanvas->MoveHighlight (
                                   DialogCanvas,
                                   FALSE
                                   );

          // If the highlight moved past the top control, clear control highlight and try again - this will wrap the highlight around
          // to the bottom.  The reason we don't do this automatically is because in other
          // scenarios, the TAB order needs to include controls outside the canvas (ex:
          // the Front Page's Top-Menu.
          //
          if (EFI_NOT_FOUND == Status) {
            DialogCanvas->ClearHighlight (DialogCanvas);

            Status = DialogCanvas->MoveHighlight (
                                     DialogCanvas,
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
          Status = DialogCanvas->MoveHighlight (
                                   DialogCanvas,
                                   TRUE
                                   );

          // If we moved the highlight to the end of the list of controls, move it back
          // to the top by clearing teh current highlight and moving to next.  The reason we don't do
          // this automatically is because in other scenarios, the TAB order needs to include controls
          // outside the canvas (ex: the Front Page's Top-Menu.
          //
          if (EFI_NOT_FOUND == Status) {
            DialogCanvas->ClearHighlight (DialogCanvas);

            Status = DialogCanvas->MoveHighlight (
                                     DialogCanvas,
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
  EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL  *WiFi2Protocol;
  EFI_80211_GET_NETWORKS_RESULT            *NetworkList;

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

  // TODO - limit number of SSIDs presented (sort them?)

  UIT_LB_CELLDATA  *WifiOptionCells = AllocateZeroPool (NetworkList->NumOfNetworkDesc * sizeof (UIT_LB_CELLDATA));

  if (WifiOptionCells == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  CHAR8  SSIDNameA[EFI_MAX_SSID_LEN + 1];

  for (UINTN i = 0; i < NetworkList->NumOfNetworkDesc; i++) {
    WifiOptionCells[i].CheckBoxSelected = FALSE;
    WifiOptionCells[i].TrashcanEnabled  = FALSE;
    WifiOptionCells[i].CellText         = AllocateZeroPool (sizeof (CHAR16) * (EFI_MAX_SSID_LEN + 1));

    SSIdNameToStr (&NetworkList->NetworkDesc[i].Network.SSId, SSIDNameA);
    AsciiStrToUnicodeStrS (SSIDNameA, WifiOptionCells[i].CellText, (EFI_MAX_SSID_LEN + 1));
  }

  // Change UI toolkit handle to dialog handle.
  InitializeUIToolKit (gDialogHandle);

  // TODO - based on screen size percentages.
  DialogRect.Left   = 200;
  DialogRect.Top    = 100;
  DialogRect.Right  = 600;
  DialogRect.Bottom = 500;

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

  SWM_MB_RESULT  Result = ProcessDialogInput (
                            mSWMProtocol,
                            WiFiDialogCanvas,
                            NULL,
                            0
                            );

  if (Result == SWM_MB_IDOK) {
    LB_RETURN_DATA  SelectedCellData;
    WifiSSIDList->GetSelectedCellIndex (WifiSSIDList, &SelectedCellData);
    StrCpyS (SSIDName, SSIDNameMaxLength, WifiOptionCells[SelectedCellData.SelectedCell].CellText);

    StrCpyS (SSIDPassword, SSIDPasswordMaxLength, PasswordEditBox->GetCurrentTextString (PasswordEditBox));

    Status = EFI_SUCCESS;
  }

Exit:

  if (DialogWindowRegistered) {
    mSWMProtocol->ActivateWindow (
                    mSWMProtocol,
                    gDialogHandle,
                    FALSE
                    );

    // Unregister with the window manager as a client.
    //
    mSWMProtocol->UnregisterClient (
                    mSWMProtocol,
                    gDialogHandle
                    );
  }

  // Restore UI Handle to normal.
  InitializeUIToolKit (gImageHandle);

  // TODO delete canvas.
  // TODO clean up WifiOptionCells.

  return (Status);
}
