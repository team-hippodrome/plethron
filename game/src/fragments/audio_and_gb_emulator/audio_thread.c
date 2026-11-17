/* Stadium Zero fragment000_80C00. */
/* Audio driver, matching OoT src/audio/internal/thread.c in function order. The driver is
 * Nintendo's "Nas" library (original names Nas_*); this revision sits between SM64 Shindou and
 * OoT. See docs/annotation/audio-driver.md for the per-function mapping. */
/* C ported from the Stadium 1 decomp: src/audio_sound_state.c, src/fragments/battle_engine/battle_engine_2FA4D0.c, src/fragments/battle_engine/battle_engine_317E70.c. */

#include "audio_and_gb_emulator.h"

#include "libnumus/player.h"


#include "m2c_macros.h"

// PORT: the decomp called these without a prototype (IDO allows it; clang does not).
u8* AudioLoad_GetFontsForSequence(s32 arg0, s32* arg1);

void AudioThread_Unused1(void);
void AudioThread_Unused2(void);
void AudioThread_Unused3(void);
void AudioThread_Unused4(void);
s32 AudioThread_GetFontsForSequence(s32 arg0, s32* arg1); // PORT: second argument passed through
void AudioThread_ResetExternalPool(void);
void AudioThread_Unused5(void);
void AudioThread_InitMesgQueues(void);

void AudioThread_InitMesgQueuesImpl(void);

/**
 * Empty.
 * No OoT counterpart. Zero's audio thread file starts with three empty stubs; OoT's thread.c starts
 * with AudioThread_Update.
 * No caller (2026-11-14): nothing in the built ROM jumps to it or holds its address, so play
 * cannot reach it.
 * @unverified Name from the code; it never runs.
 */
void AudioThread_Unused1(void) {
}

/**
 * Empty.
 * See AudioThread_Unused1.
 * No caller (2026-11-14): nothing in the built ROM jumps to it or holds its address, so play
 * cannot reach it.
 * @unverified Name from the code; it never runs.
 */
void AudioThread_Unused2(void) {
}

/**
 * Empty.
 * See AudioThread_Unused1.
 * No caller (2026-11-14): nothing in the built ROM jumps to it or holds its address, so play
 * cannot reach it.
 * @unverified Name from the code; it never runs.
 */
void AudioThread_Unused3(void) {
}

extern u8 D_811861DA;
extern u8 D_811861DB;
extern s8 D_811861DC;
// PORT: typed so the lifter gives host-sized storage (the queues hold pointers)
extern OSMesg D_811861E0[1];
extern OSMesgQueue D_811861E8;
extern OSMesgQueue* D_81186200;
extern OSMesg D_81186208[4];
extern OSMesgQueue D_81186218;
extern OSMesgQueue* D_81186230;
extern OSMesg D_81186234[1];
extern OSMesgQueue D_81186238;
extern OSMesgQueue* D_81186250;

/**
 * Corresponds to OoT AudioThread_InitMesgQueuesImpl (original name Nap_AudioPortInit). Same three
 * queues of 1, 4 and 1 messages. Stadium 1's name BattleAnim_DisableEffectSpawning did not fit.
 * Seen in play (2026-11-13) ran when the GB Player started (main menu GB, Yes; second entry after
 * quitting).
 * Seen in play (2026-11-14) ran when the GB Player started (main menu GB, はい "Yes") with JP Green in the
 * Transfer Pak; called from AudioThread_InitMesgQueues.
 */
void AudioThread_InitMesgQueuesImpl(void) {
    D_811861DA = 0;
    D_811861DB = 0;
    D_811861DC = 0;
    D_81186200 = &D_811861E8;
    D_81186230 = &D_81186218;
    D_81186250 = &D_81186238;
    osCreateMesgQueue(D_81186200, D_811861E0, 1);
    osCreateMesgQueue(D_81186230, D_81186208, 4);
    osCreateMesgQueue(D_81186250, D_81186234, 1);
}

extern s32 D_811859D8[][2];

/**
 * Appends a command to the 256-entry audio command ring; drops it when the ring is full.
 * Corresponds to OoT AudioThread_QueueCmd (original name Nap_PortSet).
 * @unverified Name follows the counterpart; not checked in play.
 */
void AudioThread_QueueCmd(u32 arg0, s32* arg1) { // PORT: arg0 is the 32-bit command word (was DisplayObject*)
    s32* entry = D_811859D8[D_811861DA & 0xFF]; // FAKE: no-op mask on a u8

    entry[0] = arg0;
    entry[1] = *arg1;
    if (D_811861DB == ++D_811861DA) {
        D_811861DA--;
    }
}

void AudioThread_QueueCmd(u32 arg0, s32* arg1);
/**
 * Queues a command with an f32 argument, passed by pointer from a stack copy.
 * Corresponds to OoT AudioThread_QueueCmdF32 (original name Nap_SetF32). Compiles to the same
 * machine code as AudioThread_QueueCmdS32; the argument is declared s32 here.
 * @unverified Name follows the counterpart; not checked in play.
 */
void AudioThread_QueueCmdF32(s32 arg0, s32 arg1) {
    AudioThread_QueueCmd(arg0, &arg1);
}

void AudioThread_QueueCmd(u32 arg0, s32* arg1);
/**
 * Queues a command with an s32 argument, passed by pointer from a stack copy. Callers pass a 32-bit
 * command word such as 0xF9000000 or 0xF1000000 as arg0.
 * Corresponds to OoT AudioThread_QueueCmdS32 (original name Nap_SetS32).
 * @unverified Name follows the counterpart; not checked in play.
 */
void AudioThread_QueueCmdS32(s32 arg0, s32 arg1) {
    AudioThread_QueueCmd(arg0, &arg1);
}

void AudioThread_QueueCmd(u32 arg0, s32* arg1);
/**
 * Queues a command with an s8 argument in the top byte.
 * Corresponds to OoT AudioThread_QueueCmdS8 (original name Nap_SetS8).
 * @unverified Name follows the counterpart; not checked in play.
 */
void AudioThread_QueueCmdS8(u32 arg0, s8 arg1) { // PORT: was DisplayObject*
    s32 sp1C = arg1 << 24;

    AudioThread_QueueCmd(arg0, &sp1C);
}

/**
 * Queues a command with a u16 argument in the top half.
 * Corresponds to OoT AudioThread_QueueCmdU16 (original name Nap_SetU16).
 * @unverified Name follows the counterpart; not checked in play.
 */
void AudioThread_QueueCmdU16(u32 arg0, s16 arg1) { // PORT: was DisplayObject*
    s32 sp1C = arg1 << 16;

    AudioThread_QueueCmd(arg0, &sp1C);
}

extern s32 D_811381D8;

/**
 * Sends the queued command range to the audio thread and tracks the largest batch seen.
 * Corresponds to OoT AudioThread_ScheduleProcessCmds (original name Nap_SendStart).
 * No return statement: retail leaves the send result in v0 and AudioThread_ResetAudioHeap returns it.
 * @unverified Name follows the counterpart; not checked in play.
 */
s32 AudioThread_ScheduleProcessCmds(void) {
    s32 head;
    s32 tail;
    s32 count = ((head = D_811861DA) - (tail = D_811861DB) + 0x100) & 0xFF;

    if (D_811381D8 < count) {
        D_811381D8 = count;
    }
    head = ((u8)tail << 8) | (u8)head; // FAKE: reuses head for the message
    // PORT: v0. No return statement in the decomp: the caller gets osSendMesg's result, which stays in v0
    s32 sent = osSendMesg(D_81186230, OS_MESG_32(head), 0); // PORT: OS_MESG_32
    if (sent != -1) {
        D_811861DB = D_811861DA;
    }
    return sent;
}

/**
 * Drops the commands queued since the last send.
 * Corresponds to OoT AudioThread_ResetCmdQueue (original name Nap_FlushPort).
 * @unverified Name follows the counterpart; not checked in play.
 */
void AudioThread_ResetCmdQueue(void) {
    (*(u8 *)&D_811861DB) = (*(u8 *)&D_811861DA);
    (*(s8 *)&D_811861DC) = 0;
}

/**
 * Empty.
 * No OoT counterpart. It sits where OoT has AudioThread_ProcessCmd (Nap_Process1Command) and
 * AudioThread_ProcessCmds (Nap_AudioPortProcess).
 * No caller (2026-11-14): nothing in the built ROM jumps to it or holds its address, so play
 * cannot reach it.
 * @unverified Name from the code; it never runs.
 */
void AudioThread_Unused4(void) {
}

extern OSMesgQueue D_811855D8; // PORT: typed

/**
 * Polls the external load queue: returns the message's top byte and stores its low 24 bits in
 * `arg0`, or returns 0.
 * Corresponds to OoT func_800E5E20. OoT leaves it unnamed; the name is MM's.
 * @unverified Name follows the counterpart; not checked in play.
 */
u32 AudioThread_GetExternalLoadQueueMsg(s32 *arg0) {
    OSMesg msg; // PORT: osRecvMesg stores a host-sized OSMesg
    u32 sp1C;

    if (osRecvMesg(&D_811855D8, &msg, 0) == -1) {
        *arg0 = 0;
        return 0U;
    }
    sp1C = (u32)(uintptr_t)msg; // PORT: the message is a 32-bit value
    *arg0 = sp1C & 0xFFFFFF;
    return sp1C >> 0x18;
}

/**
 * Returns the font list of sequence arg0 from AudioLoad_GetFontsForSequence; the font count
 * pointer that function takes is passed through untouched in the second argument register.
 * Corresponds to OoT AudioThread_GetFontsForSequence. Stadium 1's name Audio_CountCategorySounds
 * did not fit.
 */
s32 AudioThread_GetFontsForSequence(s32 arg0, s32* arg1) { // PORT: arg1 was passed through in a1 implicitly
    return (s32)(intptr_t)AudioLoad_GetFontsForSequence(arg0, arg1);
}

extern u8 D_81184658;

/**
 * Polls the audio reset queue D_81186250 once without blocking (osRecvMesg is osRecvMesg): returns 1 when the audio thread has reported the reset to the
 * current spec D_81184658 as done, otherwise 0.
 * Matching note: the unused leading `pad` local moves `sp18` to the ROM's
 * stack offset (sp+0x18); the two returns are written as separate
 * `return 0` / `return 1` paths because that is the form that compiles to the
 * ROM's branch layout.
 * Corresponds to OoT func_800E5EDC (original name Nap_CheckSpecChange). OoT leaves it unnamed; the
 * name follows the original.
 */
s32 AudioThread_CheckSpecChange(void) {
    UNUSED s32 pad; // FAKE: stack padding
    OSMesg sp18; // PORT: was u32; osRecvMesg stores a host-sized OSMesg

    if (osRecvMesg(D_81186250, &sp18, 0) == -1) {
        return 0;
    }
    if ((u32)(uintptr_t)sp18 != D_81184658) { // PORT: the message is a 32-bit value
        return 0;
    }
    return 1;
}

/**
 * Empties the audio reset queue D_81186250: receives from it without blocking (osRecvMesg
 * with flag 0) until it reports that it is empty (-1), discarding the messages.
 * Matching note: the -1 is kept in a local assigned inside the loop body.
 * With a literal -1 in the condition the compiler hands out the saved
 * registers in a different order. Declaring `neg` before `sp28` puts `sp28`
 * at the ROM's stack offset (sp+0x28).
 * Corresponds to OoT func_800E5F34 (original name __ClearSpecChangeQ). OoT leaves it unnamed; the
 * name follows the original (MM: AudioThread_WaitForAudioResetQueueP).
 */
void AudioThread_ClearSpecChangeQueue(void) {
    s32 neg;
    OSMesg sp28; // PORT: was u32; osRecvMesg stores a host-sized OSMesg

    do {
        neg = -1;
    } while (osRecvMesg(D_81186250, &sp28, 0) != neg);
}

void AudioThread_ClearSpecChangeQueue(void);
void AudioThread_QueueCmdS32(s32, s32);
s32 AudioThread_ScheduleProcessCmds(void);
extern volatile u8 D_81184657;
extern u8 D_81184658;
/**
 * Queues an audio heap reset to spec `arg0` (command 0xF9) and sends it.
 * Corresponds to OoT AudioThread_ResetAudioHeap (original name Nap_StartSpecChange).
 * @unverified Name follows the counterpart; not checked in play.
 */
s32 AudioThread_ResetAudioHeap(s32 arg0) {
    s32 v;
    OSMesg sp18; // PORT: was u32; osRecvMesg stores a host-sized OSMesg

    AudioThread_ClearSpecChangeQueue();
    v = D_81184657;
    if (v != 0) {
        AudioThread_ResetCmdQueue();
        if (arg0 == D_81184658) {
            return 0;
        }
        if (v >= 3) {
            return 0;
        }
        osRecvMesg(D_81186250, &sp18, 1);
    }
    AudioThread_ClearSpecChangeQueue();
    AudioThread_QueueCmdS32(0xF9000000, arg0);
    return AudioThread_ScheduleProcessCmds();
}

extern volatile s32 D_811859D4;

/**
 * Prepares the audio driver for a reset (pre-NMI): starts the reset timer D_811859D4 at 1, queues an
 * audio heap reset to spec 0 and clears the reset status D_81184657.
 * Corresponds to OoT AudioThread_PreNMIInternal (original name Nap_StartReset).
 * @unverified Name follows the counterpart; not checked in play.
 */
void AudioThread_PreNMIInternal(void) {
    D_811859D4 = 1;
    AudioThread_ResetAudioHeap(0);
    D_81184657 = 0;
}


/**
 * Returns IO port `arg2` of channel `arg1` of sequence player `arg0`, or -1 when the player is off.
 * Corresponds to OoT AudioThread_GetChannelIO (original name Nap_ReadSubPort).
 * @unverified Name follows the counterpart; not checked in play.
 */
s8 AudioThread_GetChannelIO(s32 arg0, s32 arg1, s32 arg2) {
    SequencePlayer* ch = &D_81184668[arg0];
    SequenceChannel* p; // PORT: typed (was s8* indexed by N64 offset 0xC4)

    if (ch->enabled) {
        p = ch->channels[arg1];
        return p->soundScriptIO[arg2];
    }
    return -1;
}

/**
 * Returns IO port `arg1` of sequence player `arg0`.
 * Corresponds to OoT AudioThread_GetSeqPlayerIO (original name Nap_ReadGrpPort).
 * @unverified Name follows the counterpart; not checked in play.
 */
s8 AudioThread_GetSeqPlayerIO(s32 arg0, s32 arg1) {
    return D_81184668[arg0].soundScriptIO[arg1]; // PORT: was D_811847BC (player 0's IO ports) + arg0 * 0x15C
}


/**
 * Sets up the external allocation pool.
 * Corresponds to OoT AudioThread_InitExternalPool.
 * @unverified Name follows the counterpart; not checked in play.
 */
void AudioThread_InitExternalPool(void* arg0, s32 arg1) { // PORT: arg0 is the pool's memory (was s32)
    AudioHeap_InitPool(&D_811842A0, arg0, arg1);
}

/**
 * Disables the external allocation pool by clearing its start address (D_811842A0).
 * Corresponds to OoT AudioThread_ResetExternalPool. Stadium 1's name
 * BattleAnim_ClearEffectStateFlag did not fit.
 */
void AudioThread_ResetExternalPool(void) {
    D_811842A0.startRamAddr = NULL; // PORT: was D_811842A0 = 0 (the pool's first word)
}

/**
 * Empty.
 * No OoT counterpart; OoT's no-op command handlers are elsewhere in thread.c.
 * No caller (2026-11-14): nothing in the built ROM jumps to it or holds its address, so play
 * cannot reach it.
 * @unverified Name from the code; it never runs.
 */
void AudioThread_Unused5(void) {
}

/**
 * Sets up the audio thread message queues.
 * Corresponds to OoT AudioThread_InitMesgQueues (original name Nas_InitGAudio). Stadium 1's name
 * BattleAnim_DisableEffectSpawningAlias did not fit.
 * Seen in play (2026-11-14) ran when the GB Player started (main menu GB, はい "Yes") with JP Green in the
 * Transfer Pak; called from AudioLoad_Init.
 */
void AudioThread_InitMesgQueues(void) {
    AudioThread_InitMesgQueuesImpl();
}
