/* Stadium Zero resident code. */
/* C ported from the Stadium 1 decomp: src/audio_channel.c, src/audio_sound_state.c. */

#include "audio_channel.h"
#include "libnumus/player.h"

#include "libnumus/player.h"

s32 Audio_SetMonoOutput(s32 arg0);
void Audio_IsSoundPlayingWrapper(s32 arg0);

/** Sets mono or stereo audio output. */
// PORT: v0. Was void with no return; Audio_PlayCommand returns its result, which on the N64 is what
// Audio_QueueSetMonoOutputCommand left in v0.
s32 Audio_SetMonoOutput(s32 arg0) {
    return Audio_QueueSetMonoOutputCommand(arg0);
}

/** Wrapper that checks whether a sound is playing. */
void Audio_IsSoundPlayingWrapper(s32 arg0) {
    Audio_IsSoundPlaying(arg0);
}
