#ifndef _GRAPHICS_COMMON_H_
#define _GRAPHICS_COMMON_H_

#define GLYPH_WIDTH  0x8
#define GLYPH_HEIGHT 0x13

#define DEFAULT_PROGRESS_BAR_HEIGHT 20
#define DEFAULT_LABEL_HEIGHT        GLYPH_HEIGHT

#define WHITE_COLOR 0x00FFFFFF
#define BLACK_COLOR 0x00000000

typedef struct _GFX_FRAMEBUFFER {
  EFI_GRAPHICS_OUTPUT_PROTOCOL* GraphicsProtocol;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL* Bitmap;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL* BackBuffer; // Used to restore the original framebuffer content
  UINTN Width;
  UINTN Height;
} GFX_FRAMEBUFFER, *PGFX_FRAMEBUFFER;

typedef struct _GFX_FONT_INFO {
  EFI_HII_FONT_PROTOCOL* FontProtocol;
  EFI_FONT_DISPLAY_INFO* Font;
} GFX_FONT_INFO, *PGFX_FONT_INFO;

typedef struct _GFX_RECT {
  UINTN X;
  UINTN Y;
  UINTN Width;
  UINTN Height;
} GFX_RECT, *PGFX_RECT;

typedef struct _GFX_PROGRESS_BAR {
  GFX_RECT Bounds;
  UINTN Percentage;
} GFX_PROGRESS_BAR, *PGFX_PROGRESS_BAR;

typedef struct _GFX_LABEL {
  GFX_RECT Bounds;
  CHAR16* Text;
} GFX_LABEL, *PGFX_LABEL;

//
// Low level 2D Drawing functions
//

EFI_STATUS
EFIAPI
GfxFillRectangle (
  IN PGFX_FRAMEBUFFER FrameBuffer,
  IN PGFX_RECT Rect,
  IN UINT32 RGB);

EFI_STATUS
EFIAPI
GfxDrawRectangle (
  IN PGFX_FRAMEBUFFER FrameBuffer,
  IN PGFX_RECT Rect,
  IN UINT32 RGB);

//
// UI component functions
//

EFI_STATUS
EFIAPI
GfxDrawProgressBar (
  IN PGFX_FRAMEBUFFER FrameBuffer,
  IN PGFX_PROGRESS_BAR ProgressBar,
  IN UINT32 RGB);

EFI_STATUS
EFIAPI
GfxDrawLabel (
  IN PGFX_FRAMEBUFFER FrameBuffer,
  IN PGFX_LABEL Label,
  IN PGFX_FONT_INFO FontInfo,
  IN UINT32 RGB);

//
// Direct screen rendering utility functions
//

EFI_STATUS
EFIAPI
GfxUpdateFrameBufferToScreen (
  IN PGFX_FRAMEBUFFER FrameBuffer);

EFI_STATUS
EFIAPI
GfxClearScreen (
  IN PGFX_FRAMEBUFFER FrameBuffer,
  IN UINT32 RGB);

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
  IN UINTN Height);

EFI_GRAPHICS_OUTPUT_BLT_PIXEL
EFIAPI
GfxInvertColor (
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel);

//
// Frame buffer utility functions
//

EFI_STATUS
EFIAPI
GfxSetGraphicsResolution (
  OUT UINT32* PreviousMode);

EFI_STATUS
EFIAPI
GfxAllocateFrameBuffer (
  IN PGFX_FRAMEBUFFER FrameBuffer);

EFI_STATUS
EFIAPI
GfxFillColor (
  IN PGFX_FRAMEBUFFER FrameBuffer,
  IN PGFX_RECT Rect,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL SamplePixel);

//
// General 2D utility functions
//

GFX_RECT
EFIAPI
GfxGetClipRectangle (
  IN PGFX_RECT Source,
  IN PGFX_RECT Destination);

//
// Font utility functions
//
// TODO: Generalize below macros
#define RASTER_ATTRIBUTE_BG_BLUE  0x00000001
#define RASTER_ATTRIBUTE_BG_WHITE 0x00000002
#define RASTER_ATTRIBUTE_INVERT   0x00000004

EFI_STATUS
EFIAPI
GfxGetFontGlyph (
  IN PGFX_FONT_INFO FontInfo,
  IN CHAR16 Char,
  IN OUT EFI_IMAGE_OUTPUT** CharImageOut);

EFI_STATUS
EFIAPI
GfxRasterCharacter (
  IN GFX_FRAMEBUFFER* FrameBuffer,
  IN PGFX_FONT_INFO FontInfo,
  IN CHAR16 Char,
  IN UINTN X,
  IN UINTN Y,
  IN UINTN Attributes);

EFI_STATUS
EFIAPI
GfxGetSystemFont (
  IN PGFX_FONT_INFO FontInfo);

#endif // _GRAPHICS_COMMON_H_
