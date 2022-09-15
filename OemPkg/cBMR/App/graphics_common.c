/*++

Copyright (c) 2021 Microsoft Corporation

Module Name:

  graphics_common.c

Abstract:

  This module implements common routines shared between UI code

Author:

  Vineel Kovvuri (vineelko) 10-Mar-2021

Environment:

  UEFI mode only.

--*/

#include "cbmrapp.h"
#include "graphics_common.h"

//
// Low level 2D Drawing functions
//

EFI_STATUS
EFIAPI
GfxFillRectangle (
  IN PGFX_FRAMEBUFFER FrameBuffer,
  IN PGFX_RECT Rect,
  IN UINT32 RGB)
{
  UINTN X = Rect->X;
  UINTN Y = Rect->Y;
  UINTN Width = Rect->Width;
  UINTN Height = Rect->Height;
  UINTN HRes = FrameBuffer->Width;
  UINTN VRes = FrameBuffer->Height;

  if (X + Width > HRes || Y + Height > VRes) {
    return EFI_INVALID_PARAMETER;
  }

  for (UINTN i = Y; i < Y + Height; i++) {
    for (UINTN j = X; j < X + Width; j++) {
      FrameBuffer->Bitmap[i * HRes + j].Red = (RGB >> 16) & 0xFF;
      FrameBuffer->Bitmap[i * HRes + j].Green = (RGB >> 8) & 0xFF;
      FrameBuffer->Bitmap[i * HRes + j].Blue = (RGB >> 0) & 0xFF;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
GfxDrawRectangle (
  IN PGFX_FRAMEBUFFER FrameBuffer,
  IN PGFX_RECT Rect,
  IN UINT32 RGB)
{
  UINTN X = Rect->X;
  UINTN Y = Rect->Y;
  UINTN Width = Rect->Width;
  UINTN Height = Rect->Height;
  UINTN HRes = FrameBuffer->Width;
  UINTN VRes = FrameBuffer->Height;

  if (X + Width > HRes || Y + Height > VRes) {
    return EFI_INVALID_PARAMETER;
  }

  // top
  // for (UINTN i = Y; i < Y + Height; i++)
  {
    UINTN i = Y;
    for (UINTN j = X; j < X + Width; j++) {
      FrameBuffer->Bitmap[i * HRes + j].Red = (RGB >> 16) & 0xFF;
      FrameBuffer->Bitmap[i * HRes + j].Green = (RGB >> 8) & 0xFF;
      FrameBuffer->Bitmap[i * HRes + j].Blue = (RGB >> 0) & 0xFF;
    }
  }

  // bottom
  // for (UINTN i = Y; i < Y + Height; i++)
  {
    UINTN i = Y + Height - 1;
    for (UINTN j = X; j < X + Width; j++) {
      FrameBuffer->Bitmap[i * HRes + j].Red = (RGB >> 16) & 0xFF;
      FrameBuffer->Bitmap[i * HRes + j].Green = (RGB >> 8) & 0xFF;
      FrameBuffer->Bitmap[i * HRes + j].Blue = (RGB >> 0) & 0xFF;
    }
  }

    // left
  for (UINTN i = Y; i < Y + Height; i++) {
    UINTN j = X;
    // for (UINTN j = X; j < X + Width; j++)
    {
      FrameBuffer->Bitmap[i * HRes + j].Red = (RGB >> 16) & 0xFF;
      FrameBuffer->Bitmap[i * HRes + j].Green = (RGB >> 8) & 0xFF;
      FrameBuffer->Bitmap[i * HRes + j].Blue = (RGB >> 0) & 0xFF;
    }
  }

  // right
  for (UINTN i = Y; i < Y + Height; i++) {
    UINTN j = X + Width - 1;
    // for (UINTN j = X; j < X + Width; j++)
    {
      FrameBuffer->Bitmap[i * HRes + j].Red = (RGB >> 16) & 0xFF;
      FrameBuffer->Bitmap[i * HRes + j].Green = (RGB >> 8) & 0xFF;
      FrameBuffer->Bitmap[i * HRes + j].Blue = (RGB >> 0) & 0xFF;
    }
  }

  return EFI_SUCCESS;
}

//
// UI component functions
//

EFI_STATUS
EFIAPI
GfxDrawProgressBar (
  IN PGFX_FRAMEBUFFER FrameBuffer,
  IN PGFX_PROGRESS_BAR ProgressBar,
  IN UINT32 RGB)
{
  EFI_STATUS Status = EFI_SUCCESS;
  UINTN Padding = 2;
  PGFX_RECT BorderRect = &ProgressBar->Bounds;
  GFX_RECT InnerRect = { BorderRect->X + Padding,
                         BorderRect->Y + Padding,
                         (BorderRect->Width * ProgressBar->Percentage) / 100 - 2 * Padding,
                         BorderRect->Height - 2 * Padding};

  //
  // Clear previous progress content if any to prevent reuse of the same
  // progress bar
  //

  Status = GfxFillRectangle (FrameBuffer, BorderRect, 0);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GfxFillRectangle() call failed : (%r)\n", Status));
    goto Exit;
  }

  Status = GfxDrawRectangle (FrameBuffer, BorderRect, RGB);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GfxDrawRectangle() call failed : (%r)\n", Status));
    goto Exit;
  }

  Status = GfxFillRectangle (FrameBuffer, &InnerRect, RGB);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GfxFillRectangle() call failed : (%r)\n", Status));
    goto Exit;
  }

Exit:
  return Status;
}

EFI_STATUS
EFIAPI
GfxDrawLabel (
  IN PGFX_FRAMEBUFFER FrameBuffer,
  IN PGFX_LABEL Label,
  IN PGFX_FONT_INFO FontInfo,
  IN UINT32 RGB)
{
  EFI_STATUS Status = EFI_SUCCESS;
  UINTN i = 0, j = 0;

  for (j = Label->Bounds.X, i = 0; Label->Text[i]; i++, j += GLYPH_WIDTH) {
    Status = GfxRasterCharacter (FrameBuffer, FontInfo, Label->Text[i], j, Label->Bounds.Y, 0);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
      goto Exit;
    }
  }

  // clear rest of the label
  for (; j < Label->Bounds.Width; j += GLYPH_WIDTH) {
    Status = GfxRasterCharacter (FrameBuffer, FontInfo, L' ', j, Label->Bounds.Y, 0);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "GfxRasterCharacter() failed: (%r)\n", Status));
      goto Exit;
    }
  }

Exit:
  return Status;
}

//
// Direct screen rendering utility functions
//

EFI_STATUS
EFIAPI
GfxUpdateFrameBufferToScreen (
  IN PGFX_FRAMEBUFFER FrameBuffer)
{
  EFI_STATUS Status = EFI_SUCCESS;

  //
  // Update to the screen
  //

  Status = FrameBuffer->GraphicsProtocol->Blt (FrameBuffer->GraphicsProtocol,
                                               (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)FrameBuffer->Bitmap,
                                               EfiBltBufferToVideo,
                                               0,
                                               0,
                                               0,
                                               0,
                                               FrameBuffer->Width,
                                               FrameBuffer->Height,
                                               0);
  return Status;
}

EFI_STATUS
EFIAPI
GfxClearScreen (
  IN PGFX_FRAMEBUFFER FrameBuffer,
  IN UINT32 RGB)
{
  EFI_STATUS Status = EFI_SUCCESS;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel = {0};

  Pixel.Red = (RGB >> 16) & 0xFF;
  Pixel.Green = (RGB >> 8) & 0xFF;
  Pixel.Blue = (RGB >> 0) & 0xFF;

  //
  // Clear the screen
  //

  Status = FrameBuffer->GraphicsProtocol->Blt (FrameBuffer->GraphicsProtocol,
                                               &Pixel,
                                               EfiBltVideoFill,
                                               0,
                                               0,
                                               0,
                                               0,
                                               FrameBuffer->Width,
                                               FrameBuffer->Height,
                                               0);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Blt() failed : (%r)\n", Status));
    goto Exit;
  }

  //
  // Reset frame buffer with new screen content
  //

  Status = FrameBuffer->GraphicsProtocol->Blt (FrameBuffer->GraphicsProtocol,
                                               (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)FrameBuffer->Bitmap,
                                               EfiBltVideoToBltBuffer,
                                               0,
                                               0,
                                               0,
                                               0,
                                               FrameBuffer->Width,
                                               FrameBuffer->Height,
                                               0);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Blt() failed : (%r)\n", Status));
    goto Exit;
  }

  //
  // Reset back buffer with new screen content
  //

  Status = FrameBuffer->GraphicsProtocol->Blt (FrameBuffer->GraphicsProtocol,
                                               (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)FrameBuffer->BackBuffer,
                                               EfiBltVideoToBltBuffer,
                                               0,
                                               0,
                                               0,
                                               0,
                                               FrameBuffer->Width,
                                               FrameBuffer->Height,
                                               0);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Blt() failed : (%r)\n", Status));
    goto Exit;
  }

Exit:
  return Status;
}

//
// Color utility functions
//

VOID
EFIAPI
GfxInitRectangle (
  IN PGFX_RECT Rect,
  IN UINTN X,
  IN UINTN Y,
  IN UINTN Width,
  IN UINTN Height)
{
  Rect->X = X;
  Rect->Y = Y;
  Rect->Width = Width;
  Rect->Height = Height;
}

EFI_GRAPHICS_OUTPUT_BLT_PIXEL
EFIAPI
GfxInvertColor (
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel)
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL InvertPixel = {0};
  InvertPixel.Reserved = 0xFF - Pixel.Reserved;
  InvertPixel.Red = 0xFF - Pixel.Red;
  InvertPixel.Green = 0xFF - Pixel.Green;
  InvertPixel.Blue = 0xFF - Pixel.Blue;

  return InvertPixel;
}

//
// Frame buffer utility functions
//

typedef struct _EFI_GRAPHICS_OUTPUT_MODE_INFORMATION_WRAPPER {
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* Mode;
  UINT32 Index;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION_WRAPPER;

INTN
EFIAPI
GfxModeCompareFunc (
  IN CONST VOID* Mode1,
  IN CONST VOID* Mode2)
{
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* GfxMode1 = ((EFI_GRAPHICS_OUTPUT_MODE_INFORMATION_WRAPPER*)Mode1)->Mode;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* GfxMode2 = ((EFI_GRAPHICS_OUTPUT_MODE_INFORMATION_WRAPPER*)Mode2)->Mode;
  INTN Ret = (INTN)(GfxMode1->HorizontalResolution) - (INTN)(GfxMode2->HorizontalResolution);

  // if (Ret == 0) {
  //     Ret = (*(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION**)Mode1)->VerticalResolution -
  //           (*(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION**)Mode2)->VerticalResolution;
  // }

  return Ret;
}

EFI_STATUS
EFIAPI
GfxSetGraphicsResolution (
  OUT UINT32* PreviousMode)
{
  EFI_STATUS Status = EFI_SUCCESS;
  EFI_GRAPHICS_OUTPUT_PROTOCOL* GraphicsProtocol = NULL;
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE* GraphicsMode = NULL;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION_WRAPPER* GraphicsModes = NULL;

  //
  // Get hold of graphics protocol
  //

  Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID**)&GraphicsProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "LocateProtocol() failed : (%r)\n", Status));
    goto Exit;
  }

  GraphicsMode = GraphicsProtocol->Mode;
  *PreviousMode = GraphicsMode->Mode;

  GraphicsModes = AllocateZeroPool (sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION_WRAPPER) * GraphicsMode->MaxMode);
  if (GraphicsModes == NULL) {
    DEBUG ((DEBUG_ERROR, "AllocateZeroPool() failed\n"));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  for (UINT32 i = 0; i < GraphicsMode->MaxMode; i++) {
    UINTN ModeInfoSize = 0;

    Status = GraphicsProtocol->QueryMode (GraphicsProtocol,
                                          i,
                                          &ModeInfoSize,
                                          &GraphicsModes[i].Mode);
    if (EFI_ERROR (Status)) {
      Status = EFI_SUCCESS;
    }

    GraphicsModes[i].Index = i;
  }

  //
  // Sort the resolutions based on HorizontalResolution
  //

  PerformQuickSort (GraphicsModes,
                    GraphicsMode->MaxMode,
                    sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION_WRAPPER),
                    GfxModeCompareFunc);

  //
  // Pick the middle resolution from the available list of resolutions
  //

  UINT32 MidIndex = GraphicsMode->MaxMode / 2;
  DEBUG ((DEBUG_INFO,
          "Picking graphics mode(%d x %d)\n",
          GraphicsModes[MidIndex].Mode->HorizontalResolution,
          GraphicsModes[MidIndex].Mode->VerticalResolution));
  Status = GraphicsProtocol->SetMode (GraphicsProtocol, GraphicsModes[MidIndex].Index);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SetMode() failed : (%r)\n", Status));
    goto Exit;
  }

Exit:
  if (GraphicsModes != NULL) {
    for (UINTN i = 0; i < GraphicsMode->MaxMode; i++) {
      FreePool (GraphicsModes[i].Mode);
    }
    FreePool (GraphicsModes);
  }

  return Status;
}

EFI_STATUS
EFIAPI
GfxAllocateFrameBuffer (
  IN PGFX_FRAMEBUFFER FrameBuffer)
{
  EFI_STATUS Status = EFI_SUCCESS;
  EFI_GRAPHICS_OUTPUT_PROTOCOL* GraphicsProtocol = NULL;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* GraphicsModeInfo = NULL;
  UINT32 FrameBufferSize = 0;

  Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID**)&GraphicsProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "LocateProtocol() failed : (%r)\n", Status));
    goto Exit;
  }

  GraphicsModeInfo = GraphicsProtocol->Mode->Info;
  FrameBuffer->Width = GraphicsModeInfo->HorizontalResolution;
  FrameBuffer->Height = GraphicsModeInfo->VerticalResolution;
  FrameBuffer->GraphicsProtocol = GraphicsProtocol;

  DEBUG ((DEBUG_INFO, "Width=%d Height=%d", FrameBuffer->Width, FrameBuffer->Height));

  //
  // Allocate frame buffer
  //

  FrameBufferSize = GraphicsModeInfo->HorizontalResolution * GraphicsModeInfo->VerticalResolution * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
  FrameBuffer->Bitmap = AllocateZeroPool (FrameBufferSize);
  if (FrameBuffer->Bitmap == NULL) {
    DEBUG ((DEBUG_ERROR, "AllocateZeroPool() failed to allocate buffer of size %d\n", FrameBufferSize));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  //
  // Allocate back buffer
  //

  FrameBuffer->BackBuffer = AllocateZeroPool (FrameBufferSize);
  if (FrameBuffer->BackBuffer == NULL) {
    DEBUG ((DEBUG_ERROR, "AllocateZeroPool() failed to allocate back buffer of size %d\n", FrameBufferSize));
    Status = EFI_OUT_OF_RESOURCES;
    goto Exit;
  }

  //
  // Blt the screen contents to frame buffer
  //

  Status = FrameBuffer->GraphicsProtocol->Blt (FrameBuffer->GraphicsProtocol,
                                               (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)FrameBuffer->Bitmap,
                                               EfiBltVideoToBltBuffer,
                                               0,
                                               0,
                                               0,
                                               0,
                                               FrameBuffer->Width,
                                               FrameBuffer->Height,
                                               0);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Blt() failed : (%r)\n", Status));
    goto Exit;
  }

  //
  // Blt the screen contents to back buffer
  //

  Status = FrameBuffer->GraphicsProtocol->Blt (FrameBuffer->GraphicsProtocol,
                                               (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)FrameBuffer->BackBuffer,
                                               EfiBltVideoToBltBuffer,
                                               0,
                                               0,
                                               0,
                                               0,
                                               FrameBuffer->Width,
                                               FrameBuffer->Height,
                                               0);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Blt() failed : (%r)\n", Status));
    goto Exit;
  }

Exit:
  return Status;
}

EFI_STATUS
EFIAPI
GfxFillColor (
  IN PGFX_FRAMEBUFFER FrameBuffer,
  IN PGFX_RECT Rect,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL SamplePixel)
{
  GFX_RECT Destination = {0, 0, FrameBuffer->Width, FrameBuffer->Height};
  GFX_RECT ClipRect = {0};

  ClipRect = GfxGetClipRectangle (Rect, &Destination);

  for (UINTN i = ClipRect.Y; i < ClipRect.Y + ClipRect.Height; i++) {
    for (UINTN j = ClipRect.X; j < ClipRect.X + ClipRect.Width; j++) {
      FrameBuffer->Bitmap[i * FrameBuffer->Width + j] = SamplePixel;
    }
  }

  return EFI_SUCCESS;
}

//
// General 2D utility functions
//

GFX_RECT
EFIAPI
GfxGetClipRectangle (
  IN PGFX_RECT Source,
  IN PGFX_RECT Destination)
{
  //
  // Assume Destination's X and Y is at zero
  //

  GFX_RECT ClipRect = {
    Source->X,
    Source->Y,
    (Source->X + Source->Width < Destination->Width) ? Source->Width : Destination->Width - Source->X,
    (Source->Y + Source->Height < Destination->Height) ? Source->Height : Destination->Height - Source->Y,
  };

  return ClipRect;
}

//
// Font utility functions
//

EFI_STATUS
EFIAPI
GfxGetFontGlyph (
  IN PGFX_FONT_INFO FontInfo,
  IN CHAR16 Char,
  IN OUT EFI_IMAGE_OUTPUT** CharImageOut)
{
  EFI_STATUS Status = EFI_SUCCESS;
  EFI_IMAGE_OUTPUT* RetCharImageOut = NULL;

  //
  // Get the Glyph corresponding to the character
  //

  Status = FontInfo->FontProtocol->GetGlyph (FontInfo->FontProtocol,
                                             Char,
                                             FontInfo->Font,
                                             &RetCharImageOut,
                                             NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GetGlyph() call failed : (%r)\n", Status));
    goto Exit;
  }

  *CharImageOut = RetCharImageOut;
  return Status;

Exit:
  if (RetCharImageOut != NULL) {
    FreePool (RetCharImageOut->Image.Bitmap);
    FreePool (RetCharImageOut);
  }
  return Status;
}

EFI_STATUS
EFIAPI
GfxRasterCharacter (
  IN GFX_FRAMEBUFFER* FrameBuffer,
  IN PGFX_FONT_INFO FontInfo,
  IN CHAR16 Char,
  IN UINTN X,
  IN UINTN Y,
  IN UINTN Attributes)
{
  EFI_STATUS Status = EFI_SUCCESS;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL* GlyphContent = NULL;
  EFI_IMAGE_OUTPUT* CharGlyph = NULL;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Blue = {0xFF, 0, 0, 0};
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL White = {0xFF, 0xFF, 0xFF, 0};
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Empty = {0, 0, 0, 0};

  Status = GfxGetFontGlyph (FontInfo, Char, &CharGlyph);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GfxGetFontGlyph() failed: (%r)\n", Status));
    goto Exit;
  }

  GlyphContent = CharGlyph->Image.Bitmap;

  //
  // Raster glyph on to framebuffer
  //

  for (UINTN i = Y; i < Y + CharGlyph->Height; i++) {
    for (UINTN j = X; j < X + CharGlyph->Width; j++, GlyphContent++) {
      if (i > FrameBuffer->Height || j > FrameBuffer->Width) {
        continue; // Clip to Framebuffer boundaries
      }

      if ((Attributes & RASTER_ATTRIBUTE_INVERT) == RASTER_ATTRIBUTE_INVERT) {
        FrameBuffer->Bitmap[i * FrameBuffer->Width + j] = GfxInvertColor (*GlyphContent);
      }
      else if ((Attributes & RASTER_ATTRIBUTE_BG_BLUE) == RASTER_ATTRIBUTE_BG_BLUE) {
        if (CompareMem (GlyphContent, &Empty, sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)) == 0) {
          FrameBuffer->Bitmap[i * FrameBuffer->Width + j] = Blue;
        }
        else {
          FrameBuffer->Bitmap[i * FrameBuffer->Width + j] = *GlyphContent;
        }
      }
      else if ((Attributes & RASTER_ATTRIBUTE_BG_WHITE) == RASTER_ATTRIBUTE_BG_WHITE) {
        if (CompareMem (GlyphContent, &Empty, sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)) == 0) {
          FrameBuffer->Bitmap[i * FrameBuffer->Width + j] = White;
        }
        else {
          FrameBuffer->Bitmap[i * FrameBuffer->Width + j] = *GlyphContent;
        }
      }
      else {
        FrameBuffer->Bitmap[i * FrameBuffer->Width + j] = *GlyphContent;
      }
    }
  }

Exit:
  if (CharGlyph != NULL) {
    FreePool (CharGlyph->Image.Bitmap);
    FreePool (CharGlyph);
  }
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
GfxGetSystemFont (
  IN PGFX_FONT_INFO FontInfo)
{
  EFI_STATUS Status = EFI_SUCCESS;
  EFI_FONT_HANDLE FontHandle = NULL;
  EFI_FONT_DISPLAY_INFO* FontInfoOut = NULL;
  EFI_HII_FONT_PROTOCOL* FontProtocol = NULL;

  //
  // Get hold of font protocol
  //

  Status = gBS->LocateProtocol (&gEfiHiiFontProtocolGuid, NULL, (VOID**)&FontProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "LocateProtocol() failed : (%r)\n", Status));
    goto Exit;
  }

  //
  // Get System default font
  //

  Status = FontProtocol->GetFontInfo (FontProtocol, &FontHandle, NULL, &FontInfoOut, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GetFontInfo() call failed : (%r)\n", Status));
    goto Exit;
  }

  FontInfo->FontProtocol = FontProtocol;
  FontInfo->Font = FontInfoOut;

Exit:
  return Status;
}
