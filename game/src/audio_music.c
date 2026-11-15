#include <stdlib.h>
/* Stadium Zero resident code. */
/* C ported from the Stadium 1 decomp: src/DDC0.c. */

#include <PR/os_internal_reg.h>
#include "global.h"
#include "audio_music.h"
#include "scheduler.h"
#include "audio_sound_state.h"
#include "fragments/battle/battle.h"
#include "rom_device.h"
#include "m2c_macros.h"

// PORT: the decomp called these without a prototype (IDO allows it; clang does not).
void Audio_FadeCategoriesAndStreamsDefault(void);
void Audio_FadeCategoriesAndStreamsWithDuration(s32 arg0);

// Declared here instead of in a header: the callers of these disagree on their types.
s32 Audio_StartMusicTrack(s32 track_id);
void Audio_FadeOutAll(s32);
extern s32 D_800AAE30;
extern s32 D_800AAF3C;

void Audio_ResetCurrentMusic(void);
void Audio_PlayMusicIfChanged(s32 arg0);
void Audio_StopMusic(s32 arg0);
s32 Audio_WaitForIdle(s32 arg0);
void Audio_FadeOutForShutdown(void);
void Audio_Disable(void);
void Audio_Enable(void);

/** Marks that no music is current. */
void Audio_ResetCurrentMusic(void) {
    D_800AAF3C = -1;
}

/** Starts a music track unless it is already the current one. */
void Audio_PlayMusicIfChanged(s32 arg0) {
    if (arg0 != D_800AAF3C) {
        if (D_800AAF3C >= 0) {
            Audio_FadeOutAll(0);
        }
        Audio_StartMusicTrack(arg0);
        D_800AAF3C = arg0;
    }
}

/** Fades out the current music over the given time. */
void Audio_StopMusic(s32 arg0) {
    if (D_800AAF3C >= 0) {
        Audio_FadeOutAll(arg0);
        D_800AAF3C = -1;
    }
}

/** Waits for the audio system to become idle, returning nonzero on timeout. */
s32 Audio_WaitForIdle(s32 arg0) {
    s32 retvar = 0;

    if (arg0 != 0) {
        Audio_FadeCategoriesAndStreams(2);
    }
    while (Audio_GetActivityScore() != 0) {
        if (retvar < 1000000) {
            retvar++;
        } else if (retvar == 1000000) {
            Audio_FadeCategoriesAndStreamsDefault();
            retvar++;
        }
    }
    return retvar;
}

/** Fades out all audio categories ahead of a shutdown. */
void Audio_FadeOutForShutdown(void) {
    Audio_FadeCategoriesAndStreamsWithDuration(16);
}

/** Disables audio output. */
void Audio_Disable(void) {
    D_800AAE30 = 0;
    Audio_SetEnabled(0);
}

/** Enables audio output. */
void Audio_Enable(void) {
    Audio_SetEnabled(1);
    D_800AAE30 = 1;
}

void SchedClient_WaitMessage(SchedClient*); // PORT: typed prototypes (were M2C_UNK)
void Sched_AddClient(SchedClient*);
void Profiler_LogThread4Time(void);
OSTask* __amHandleFrame(OSTask*);
void Audio_SetOutputMode(s32);
void Audio_LoadSoundData(void);
void Audio_SetChannelVolume(s32 arg0, u32 arg1);
extern SchedClient D_800AAE38; // PORT: typed (was s8): its queue is D_800AAE48, at another host offset
// PORT: typed (was s8 D_800AAE68, D_800AAE6C, D_800AAED0, D_800AAED4 and raw 0x68 strides): the two audio frame tasks
extern SchedTask D_800AAE68[2];
extern s32 D_800AAF38;
/**
 * Audio thread (id 4, started by Idle_ThreadEntry): sets up its queues, registers as a scheduler client,
 * loads the sound data, then each retrace builds and sends an audio frame task (__amHandleFrame) while
 * the console is not about to reset.
 */
void Audio_ThreadEntry(s32 arg0) {
    osCreateMesgQueue(&D_800AAE38.queue, D_800AAE38.mesgBuf, 1);
    Sched_AddClient(&D_800AAE38);
    D_800AAE30 = 1;
    D_800AAF3C = -1;
    D_800AAF38 = 0;
    osCreateMesgQueue(&D_800AAE68[0].queue, &D_800AAE68[0].mesg, 1);
    osCreateMesgQueue(&D_800AAE68[1].queue, &D_800AAE68[1].mesg, 1);
    osSendMesg(&D_800AAE68[0].queue, OS_MESG_32(0x444F4E45), 0); // 'DONE'
    osSendMesg(&D_800AAE68[1].queue, OS_MESG_32(0x444F4E45), 0); // 'DONE'
    // PORT: PORT_NO_AUDIO=1 runs the game without the sound driver (while its 64-bit port is in progress)
    if (getenv("PORT_NO_AUDIO") != NULL) {
        for (;;) {
            SchedClient_WaitMessage(&D_800AAE38);
        }
    }
    Audio_LoadSoundData();
    Audio_SetOutputMode(1);
    Audio_SetChannelVolume(3, 4);
loop_1:
    SchedClient_WaitMessage(&D_800AAE38);
    Profiler_LogThread4Time();
    if ((D_800AAE30 != 0) && (D_800A9710.preNmiFrames < 21)) {
        __amHandleFrame(&D_800AAE68[D_800AAF38].task);
        Sched_SubmitTask(&D_800AAE68[D_800AAF38], 0);
    }
    D_800AAF38 ^= 1;
    Profiler_LogThread4Time();
    goto loop_1; // FAKE: goto loop
}
