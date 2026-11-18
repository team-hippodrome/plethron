#ifndef GB_PLAYER_TEXT_H
#define GB_PLAYER_TEXT_H

#include "ultra64.h"

// PORT: translated text for the GB Player's screens (game/src/gb_player_text.c).

// Draws command list `cmd` translated into `fb` (320 pixels a row, N64 byte order). Returns 0 when the original
// should be drawn: the original language, a list with no row, or an empty cell.
s32 GbPlayerText_DrawCommands(u16* fb, u8* cmd);

// Blends status strip word `word` (0 ロードちゅう, 1 セーブちゅう, 2 でメニュー) translated at `dst` in `color`,
// `height` pixels tall, at most `avail` wide. Returns 0 when the original glyphs should be drawn.
s32 GbPlayerText_BlendStatusWord(u16* dst, s32 word, s32 avail, s32 height, s32 color);

#endif
