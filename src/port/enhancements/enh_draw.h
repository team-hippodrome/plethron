// Drawing helpers for the enhancements' own 2D panels (summary bars, trainer memory), in the game's display list.
// Coordinates are N64 pixels of a 320x240 stage. Text uses the port's Unicode glyphs (unicode_font.h) rasterised
// ENH_TEXT_SUPERSAMPLE times larger than drawn, so small text stays sharp at the PC's higher resolutions.
#pragma once
#include "global.h"

#define ENH_TEXT_SUPERSAMPLE 8

// Starts / ends a block of enhancement drawing (render state for rectangles and glyphs).
void Enh_Begin(void);
void Enh_End(void);
// A solid rectangle (x, y, w, h may be fractional quarter pixels: values are in pixels, rounded to 1/4).
void Enh_Rect(f32 x, f32 y, f32 w, f32 h, u8 r, u8 g, u8 b, u8 a);
// Draws one line of UTF-8 text `height` pixels tall with its top-left at (x, y), squeezed horizontally to fit
// `max_width` when that is > 0 and the text is wider. Returns the drawn width.
f32 Enh_Text(f32 x, f32 y, f32 height, f32 max_width, const char* utf8, u8 r, u8 g, u8 b, u8 a);
// The width `utf8` takes at `height`.
f32 Enh_TextWidth(f32 height, const char* utf8);
