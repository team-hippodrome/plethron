// Unicode glyphs for translated text (src/port/unicode_font.cpp): GNU Unifont, drawn in the game font's style.
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The IA8 texture of `cp` for a font set `height` pixels tall, or NULL when the subset has no glyph for it.
// *tex_width is the texture's row length (a multiple of 8), *draw_width the texels to draw and *advance the pen step.
// The texture stays valid for the rest of the run.
const unsigned char* Port_Font_Glyph(uint32_t cp, int height, int* tex_width, int* draw_width, int* advance);
int Port_Font_HasGlyph(uint32_t cp);

// Decodes the UTF-8 character at *s and advances *s past it. Returns 0 at the end and U+FFFD for a bad sequence.
uint32_t Port_Utf8Next(const char** s);

// Defined in game/src/font.c: draw a UTF-8 string with these glyphs in the active font set, whatever the language,
// and measure one line of it.
void Port_Font_PrintUtf8(int32_t x, int32_t y, const char* utf8);
int32_t Port_Font_MeasureUtf8(const char* utf8);
// Fits the next translated print between x `left` and `right` (squeezed when over by two characters or less,
// logged to text_overflows.csv when over by more); `align` 1 centres it in the box, 2 right-aligns it at `right`.
// No-op for the original text.
void Port_Font_FitNext(int32_t left, int32_t right, int32_t align);
// Draws translated text at glyph height `height` instead of the active font set's (0 = the set's height again), for
// text the port lays out itself (game/src/subtitles.c).
void Port_Font_SetHeight(int32_t height);

#ifdef __cplusplus
}
#endif
