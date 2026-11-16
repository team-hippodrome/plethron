/* PORT: not part of the decomp. Subtitles for the announcer's voice lines (subtitles.c). */
#ifndef SUBTITLES_H
#define SUBTITLES_H

#include "ultra64.h"

// Effect `id` of the bank whose effect table is `effects` started playing as sound `handle` (func_8003815C).
void Subtitles_OnSound(const void* effects, s32 id, s32 handle);
// Draws the line being said over the finished frame (StageLoader_SetupFrame).
void Subtitles_Draw(void);

#endif
