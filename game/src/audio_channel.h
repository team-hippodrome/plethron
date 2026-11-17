/* Declarations for src/audio_channel.c. */
#ifndef AUDIO_CHANNEL_H
#define AUDIO_CHANNEL_H

#include "global.h"

typedef struct unk_D_800FCF28 {
    /* 0x000 */ s16 unk_000[0x8A0];
} unk_D_800FCF28; // size = 0x1140

// audio_channel.c
s32 Audio_SetMonoOutput(s32 arg0); // PORT: was void (see audio_channel.c)
void Audio_IsSoundPlayingWrapper(s32 arg0);

#endif // AUDIO_CHANNEL_H
