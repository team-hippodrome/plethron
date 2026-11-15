#ifndef _PLAYER_H_
#define _PLAYER_H_

#include "global.h"
#include "src/libnaudio/n_libaudio_sc.h"
#include "src/libnaudio/n_libaudio_sn_sc.h"
#include "src/libnaudio/n_synthInternals.h"

#define FORNEXT_DEPTH 4

#define MUSFLAG_EFFECTS	1
#define MUSFLAG_SONGS	2

#define FX_PRIORITY 3

typedef struct musConfig {
    /* 0x00 */ u32 channels;
    /* 0x04 */ u32 vvoices;
    /* 0x08 */ u32 pvoices;
    /* 0x0C */ void* heap;
    /* 0x10 */ s32 heap_length;
    /* 0x14 */ u32 syn_updates;
    /* 0x18 */ u32 syn_output_rate;
    /* 0x1C */ u32 syn_rsp_cmds;
    /* 0x20 */ u32 syn_retraceCount;
    /* 0x24 */ u32 syn_num_dma_bufs;
    /* 0x28 */ u32 syn_dma_buf_size;
    /* 0x2C */ u32 command_queue_max_size;
} musConfig; // size >= 0x30

/**
 * byte 0: envelope speed
 * byte 1: padding
 * byte 2: attack speed
 * byte 3: max volume
 * byte 4: decay speed
 * byte 5: sustain volume
 * byte 6: release speed
 */
typedef struct envelope_t {
    /* 0x00 */ char params[0x7];
} envelope_t; // size = 0x7

typedef struct fx_metadata_t {
    /* 0x00 */ u8 params[4]; // [3] = default priority
} fx_metadata_t; // size = 0x4

// PORT: songs are big-endian blobs whose header tables hold 32-bit offsets from the song. On the N64
// func_8003A598 adds the song's address to them; on the host they stay offsets (byte-swapped to host order on first
// use by Audio_PortRelocateSong) and PORT_BLOB_AT turns them into pointers.
typedef struct song_t {
    /* 0x00 */ u32 num_channels;
    /* 0x04 */ u32 channel_data;   // offset of u32 channel_data[num_channels] (offsets of command streams, 0 = none)
    /* 0x08 */ u32 volume_data;    // offset of u32 volume_data[num_channels]
    /* 0x0C */ u32 pitchbend_data; // offset of u32 pitchbend_data[num_channels]
    /* 0x10 */ u32 env_table;      // offset of envelope_t env_table[]
    /* 0x14 */ u32 drum_table;     // offset of s32 drum_table[] (offsets from the song)
} song_t; // size >= 0x18

typedef struct fx_t {
    /* 0x00 */ char unk00[0x8];
    /* 0x08 */ fx_metadata_t** fx_metadata;
    /* 0x0C */ u32* fx_data;
} fx_t; // size >= 0x10

/**
 * Stadium Zero's FX block (the second argument of func_8003834C): a header followed by its tables, loaded from ROM.
 * On the N64 Block_RelocateSelf adds the block's address to `unk08`, `table` and every entry of `table`.
 * PORT: on the host Block_RelocateSelf only byte-swaps the header; `unk08`, `table` and the table's entries stay
 * offsets from the block, and the tables stay big-endian. Read them through the AUDIO_FX_* macros.
 */
typedef struct AudioFxBlock {
    /* 0x00 */ u32 count;
    /* 0x04 */ u32 flags; // bit 0: relocated
    /* 0x08 */ u32 unk08;  // offset of an s32 per entry (read by func_80037828)
    /* 0x0C */ u32 table;  // offset of `count` u32 offsets: the entries' command streams
} AudioFxBlock; // size >= 0x10

#define PORT_BLOB_AT(blob, off) ((u8*)(blob) + (off))
#define PORT_BLOB_PTR_OR_NULL(blob, off) ((off) != 0 ? PORT_BLOB_AT(blob, off) : NULL)
#define PORT_SONG_TABLE(song, table, i) (((u32*)PORT_BLOB_AT(song, (song)->table))[i])
#define AUDIO_FX_TABLE(blk) ((u32*)PORT_BLOB_AT(blk, (blk)->table))
#define AUDIO_FX_UNK08(blk) ((s32*)PORT_BLOB_AT(blk, (blk)->unk08))
#define AUDIO_FX_ENTRY(blk, i) PORT_BLOB_AT(blk, Port_ReadBE32(&AUDIO_FX_TABLE(blk)[i]))
#define AUDIO_FX_UNK08_AT(blk, i) ((s32)Port_ReadBE32(&AUDIO_FX_UNK08(blk)[i]))

/** Configuration block of the sound player initialiser func_800371A4 (filled by Audio_LoadSoundData). */
typedef struct AudioInitConfig {
    /* 0x00 */ s32 channels;
    /* 0x04 */ ALHeap* heap;
    /* 0x08 */ SoundBank* bank;
    /* 0x0C */ u32* unk_0C; // PORT: was s32. The FX block's table (offsets from portFxBlock, see AudioFxBlock)
    /* 0x10 */ s32* unk_10; // PORT: was s32. The FX block's unk08 table
    /* 0x14 */ s32 synUpdates; // @unverified: same offset/use as libmus's musConfig.syn_updates (forwarded as ALSynConfig.maxUpdates)
    /* 0x18 */ s32 outputRate;
    /* 0x1C */ s32 synRspCmds; // @unverified: same offset/use as libmus's musConfig.syn_rsp_cmds (forwarded to amCreateAudioMgr)
    /* 0x20 */ s32 synRetraceCount; // @unverified: same offset/use as libmus's musConfig.syn_retraceCount (amConfig.framesPerField)
    /* 0x24 */ s32 synNumDmaBufs; // @unverified: same offset/use as libmus's musConfig.syn_num_dma_bufs
    /* 0x28 */ s32 synDmaBufSize; // @unverified: same offset/use as libmus's musConfig.syn_dma_buf_size
    /* 0x2C */ s32 unk_2C;
    /* 0x30 */ AudioFxBlock* portFxBlock; // PORT: the block unk_0C's offsets are relative to
} AudioInitConfig; // size = 0x30 on the N64

// Stadium Zero's voice (Stadium 1's audio_voice_t without the leading list node). The player keeps 45 of them in
// D_800B3670. PORT: a real struct instead of raw N64 offsets; on the host N_ALVoice holds 64-bit pointers.
typedef struct audio_voice_t {
    /* 0x00 */ N_ALVoice voice;
    /* 0x1C */ u16 env_release_speed;
    /* 0x1E */ u8 env_phase; // 1-3 playing, 4 releasing, 5 free
    /* 0x20 */ u32 release_end_frame;
} audio_voice_t; // size = 0x24

typedef struct channel_t {
    /* 0x000 */ u64 unk_000; // the channel's current time (frames << 8)
    /* 0x008 */ u64 channel_frame;
    /* 0x010 */ u64 volume_frame;
    /* 0x018 */ u64 pitchbend_frame;
    /* 0x020 */ u64 note_end_frame;
    /* 0x028 */ u64 note_start_frame;
    /* 0x030 */ u8* pdata;
    /* 0x034 */ ALWaveTable* sample; // PORT: was s32
    /* 0x038 */ s32 duration;
    /* 0x03C */ s32 duration_scale;
    /* 0x040 */ f32 vib_amount;
    /* 0x044 */ f32 pitchbend_calc;
    /* 0x048 */ f32 pitch_current;
    /* 0x04C */ f32 base_note;
    /* 0x050 */ f32 freqoffset;
    /* 0x054 */ u8* ppitchbendbase;
    /* 0x058 */ u8* pvolumebase;
    /* 0x05C */ s32 handle;
    /* 0x060 */ s32 priority;
    /* 0x064 */ f32 last_note;
    /* 0x068 */ f32 port_base;
    /* 0x06C */ f32 env_attack_calc;
    /* 0x070 */ f32 env_decay_calc;
    /* 0x074 */ s32 env_speed_calc;
    /* 0x078 */ f32 vibrato;
    /* 0x07C */ f32 bendrange;
    /* 0x080 */ f32 pitchbend;
    /* 0x084 */ song_t* song_addr;
    /* 0x088 */ SoundBank* sample_bank;
    /* 0x08C */ u8* pbase;
    /* 0x090 */ u8* pdrums;
    /* 0x094 */ u8* ppitchbendbase_offset;
    /* 0x098 */ u8* pvolumebase_offset;
    /* 0x09C */ f32 distort;
    /* 0x0A0 */ u32 sweep_frame;
    /* 0x0A4 */ s16 temscale;
    /* 0x0A6 */ u16 note_duration;
    /* 0x0A8 */ u16 channel_tempo;
    /* 0x0AA */ s16 volume_scale;
    /* 0x0AC */ u16 volume_current;
    /* 0x0AE */ u16 cont_vol_repeat_count;
    /* 0x0B0 */ u16 cont_pb_repeat_count;
    /* 0x0B2 */ u16 IsFX;
    /* 0x0B4 */ u16 channel_tempo_save;
    /* 0x0B6 */ u16 wobble_frame;
    /* 0x0B8 */ u16 fixed_length;
    /* 0x0BA */ u16 wave;
    /* 0x0BC */ s16 sweep_amount;
    /* 0x0BE */ u16 cutoff;
    /* 0x0C0 */ u16 endit;
    /* 0x0C2 */ u16 unk_0C2;
    /* 0x0C4 */ u8 vib_delay;
    /* 0x0C5 */ u8 ignore;
    /* 0x0C6 */ u8 port;
    /* 0x0C7 */ s8 transpose;
    /* 0x0C8 */ u8 ignore_transpose;
    /* 0x0C9 */ u8 velocity;
    /* 0x0CA */ u8 volume;
    /* 0x0CB */ u8 pan;
    /* 0x0CC */ u8 pan_current;
    /* 0x0CD */ u8 env_speed;
    /* 0x0CE */ u8 env_init_vol;
    /* 0x0CF */ u8 env_max_vol;
    /* 0x0D0 */ u8 env_sustain_vol;
    /* 0x0D1 */ u8 env_state;
    /* 0x0D2 */ u8 env_current_vol;
    /* 0x0D3 */ u8 env_decay_begin;
    /* 0x0D4 */ u8 env_attack_speed;
    /* 0x0D5 */ u8 env_decay_speed;
    /* 0x0D8 */ u32 pending;
    /* 0x0DC */ u8 env_release_speed;
    /* 0x0DD */ u8 playing;
    /* 0x0DE */ u8 reverb;
    /* 0x0DF */ u8 reverb_intensity;
    /* 0x0E0 */ u8 reverb_mix;
    /* 0x0E1 */ u8 unk_0E1;
    /* 0x0E2 */ u8 unk_0E2;
    /* 0x0E3 */ u8 unk_0E3;
    /* 0x0E4 */ s8 unk_0E4;
    /* 0x0E5 */ u8 wobble_on_speed;
    /* 0x0E6 */ u8 wobble_off_speed;
    /* 0x0E7 */ u8 wobble_counter;
    /* 0x0E8 */ u8 vib_speed;
    /* 0x0E9 */ u8 velocity_on;
    /* 0x0EA */ u8 default_velocity;
    /* 0x0EB */ u8 sweep_speed;
    /* 0x0EC */ u8 wobble_value;
    /* 0x0ED */ u8 env_trigger_off;
    // Zero keeps its for/next stack four bytes lower than Stadium 1 (no trigger_off, wobble_amount, sweep_counter,
    // sweep_direction) and has the FX block and its sound bank before the voice.
    /* 0x0EE */ u8 for_stack_count;
    /* 0x0F0 */ u8* for_stack[FORNEXT_DEPTH];
    /* 0x100 */ u8* for_stackvol[FORNEXT_DEPTH];
    /* 0x110 */ u8* for_stackpb[FORNEXT_DEPTH];
    /* 0x120 */ u16 for_vol_count[FORNEXT_DEPTH];
    /* 0x128 */ u16 for_pb_count[FORNEXT_DEPTH];
    /* 0x130 */ u8 for_count[FORNEXT_DEPTH];
    /* 0x134 */ u8 for_volume[FORNEXT_DEPTH];
    /* 0x138 */ u8 for_pitchbend[FORNEXT_DEPTH];
    /* 0x13C */ struct AudioFxBlock* fx_block;
    /* 0x140 */ SoundBank* fx_sample_bank;
    /* 0x144 */ audio_voice_t* voice;
} channel_t; // size = 0x148

typedef struct bank_remap_descriptor_t {
    /* 0x00 */ u32 num_entries;
    /* 0x04 */ u32 flags;
    /* 0x08 */ u32* metadata_offset;
    /* 0x0C */ u32* entries_offset;
} bank_remap_descriptor_t; // size = 0x10

extern ALHeap* audio_heap;
extern u32 mus_num_vvoices;
extern s32 mus_releasing_voices_count;

u8* Audio_ClearVoicePlaybackState(channel_t*, UNUSED u8*);
u8* Audio_SetVoiceSampleIndex(channel_t*, u8*);
u8* Audio_EnableVoicePitchSlide(channel_t*, u8*);
u8* Audio_DisableVoicePitchSlide(channel_t*, u8*);
u8* Audio_SetVoiceVolumeEnvelope(channel_t*, u8*);
u8* Audio_SetVoiceSequenceTempo(channel_t*, u8*);
u8* Audio_SetVoiceReleaseDuration(channel_t*, u8*);
u8* Audio_SetVoiceFixedDuration(channel_t*, u8*);
u8* Audio_EnableVoiceVibrato(channel_t*, u8*);
u8* Audio_EnableVoiceVibratoDown(channel_t*, u8*);
u8* Audio_DisableVoiceVibrato(channel_t*, u8*);
u8* Audio_SetVoiceDurationOverride(channel_t*, u8*);
u8* Audio_EnableVoiceVariableDuration(channel_t*, u8*);
u8* Audio_SetVoiceDetune(channel_t*, u8*);
u8* Audio_ApplyVoiceDetuneOnce(channel_t*, u8*);
u8* Audio_SetVoiceFinePitch(channel_t*, u8*);
u8* Audio_SetVoiceEnvelopeFromTable(channel_t*, u8*);
u8* Audio_SetVoiceFlagED(channel_t*, u8*);
u8* Audio_ClearVoiceFlagED(channel_t*, u8*);
u8* Audio_SetVoiceFlagEE(channel_t*, u8*);
u8* Audio_ClearVoiceFlagEE(channel_t*, u8*);
u8* Audio_PushVoiceLoopState(channel_t*, u8*);
u8* Audio_PopVoiceLoopState(channel_t*, u8*);
u8* Audio_SetVoicePeriodicPitchModulation(channel_t*, u8*);
u8* Audio_DisableVoicePeriodicPitchModulation(channel_t*, u8*);
u8* Audio_EnableVoiceVelocityFromStream(channel_t*, u8*);
u8* Audio_DisableVoiceVelocityFromStream(channel_t*, u8*);
u8* Audio_SetVoiceDefaultVelocity(channel_t*, u8*);
u8* Audio_SetVoicePan(channel_t*, u8*);
u8* Audio_SkipVoiceCommandWord(UNUSED channel_t*, u8*);
u8* Audio_SetVoiceSubEnvelopePointer(channel_t*, u8*);
u8* Audio_ClearVoiceSubEnvelopePointer(channel_t*, u8*);
u8* Audio_SkipVoiceCommandByte(UNUSED channel_t*, u8*);
u8* Audio_SetVoiceEnvelopePointers(channel_t*, u8*);
u8* Audio_SetVoiceFxMix(channel_t*, u8*);
u8* Audio_SetRandomVoiceDetune(channel_t*, u8*);
u8* Audio_SetRandomVoiceVolume(channel_t*, u8*);
u8* Audio_SetRandomVoicePan(channel_t*, u8*);
u8* Audio_SetVoiceVolume(channel_t*, u8*);
u8* Audio_AllocateSubVoiceFromCommand(channel_t*, u8*);
u8* Audio_SetVoiceVolumeScale(channel_t*, u8*);
u8* Audio_SetVoicePanModulation(channel_t*, u8*);
u8* Audio_AllocateVoiceFromCommand(channel_t*, u8*);
void Audio_WriteU32BE(u8*, u32);
void Audio_WriteU16BE(u8*, u32);
s32 Audio_InitializeVoiceManager(musConfig*);
void Audio_QueueSetVolumeCommand(s32, u32);
s32 Audio_PlayMusicSequence(SoundBank*, song_t*);
s32 Audio_PlaySoundEffect(SoundBank*, fx_t*, s32, s32, u32, s32);
void Audio_QueueFadeCategoryCommand(s32, u32);
s32 Audio_CountActiveSoundsByCategory(s32);
void Audio_QueueFadeSoundCommand(s32, u32);
s32 Audio_IsSoundPlaying(s32);
void Audio_QueueVoiceVolumeScale(s32, u32);
void Audio_QueueVoicePan(s32, u16);
void Audio_QueueVoiceDetune(s32, f32);
void Audio_QueueVoiceVolume(s32, s32);
void Audio_QueueVoiceVolumeByte(u32, s32);
void Audio_RelocateSoundBankWrapper(SoundBank*, uintptr_t);
void Audio_RelocateSoundBankTable(bank_remap_descriptor_t*);
s32 Audio_QueueSetMonoOutputCommand(s32 handle);
void Audio_SetVoiceStartTimingMode(s32);
void Audio_StopAllSoundEffects(void);
s32 Audio_PlayMusicSequenceLooped(SoundBank*, song_t*, u32);
s32 Audio_StartVolumeTransition(s32, u8, u8, u32);
u32 Audio_ReadU32BE(u8*);
u16 Audio_ReadU16BE(u8*);
void Audio_ProcessCommandQueue(void);
u8* Audio_PlayMusicSequenceHandler(u8*);
u8* Audio_PlayMusicSequenceLoopedHandler(u8*);
u8* Audio_PlaySoundEffectHandler(u8*);
u8* Audio_ApplyVoiceDetuneCommand(u8*);
u8* Audio_ApplyVoiceVolumeCommand(u8*);
u8* Audio_ApplyVoiceVolumeScaleCommand(u8*);
u8* Audio_ApplyVoicePanCommand(u8*);
u8* Audio_ApplyVoiceFxMixCommand(u8*);
u8* Audio_FadeSoundHandler(u8*);
u8* Audio_FadeCategoryHandler(u8*);
u8* Audio_SetChannelVolumeHandler(u8*);
u8* Audio_SetMonoOutputHandler(u8*);
s32 Audio_PlayerFrameHandler(void*);
void Audio_DecodeVoiceCommandStream(channel_t*);
s32 Audio_ComputeVoiceVolume(channel_t*);
f32 Audio_AdvanceVoicePitch(channel_t*, f32);
void Audio_UpdateVoicePlaybackEnd(channel_t*);
void Audio_ReleaseVoice(channel_t*);
void Audio_ResetPanModulation(channel_t*);
void Audio_AdvancePanModulation(channel_t*);
f32 Audio_AdvancePeriodicPitchModulation(channel_t*);
f32 Audio_ComputeSinePitchModulation(channel_t*);
void Audio_AdvancePitchEnvelope(channel_t*);
void Audio_AdvanceVolumeEnvelope(channel_t*);
f32 Audio_ExpApproximation(f32);
void Audio_RelocateSoundBank(u8*, uintptr_t);
s32 Audio_RandomScale(s32);
void Audio_ResetVoiceState(channel_t*);
s32 Audio_FindFreeVoice(song_t*, s32);
void Audio_FillBytes(u8*, u8, u32);
void Audio_RelocatePointerArray(u8*, u32, u32);
s32 Audio_InitializeVoice(channel_t*, s32, s32, s32, s32);
void Audio_QueueFreeVoice(audio_voice_t*);
audio_voice_t* Audio_DequeueFreeVoice(void);
void Audio_QueueActiveVoice(audio_voice_t*);
void Audio_ReclaimExpiredVoices(void);

// Stadium Zero's sound player entry points (PORT: typed; callers declared them with s32 arguments)
s32 func_8003834C(SoundBank* bank, AudioFxBlock* fx, s32 id, s32 volume, s32 pan, s32 arg5, s32 priority);
s32 func_800383DC(SoundBank* bank, AudioFxBlock* fx, s32 id, s32 volume, s32 pan, s32 arg5, s32 priority);
s32 func_80037F68(SoundBank* bank, song_t* song);
void func_80037FCC(SoundBank* bank, s32 song);
void func_80038014(SoundBank* bank, s32 song, s32 arg2, s32 arg3, s32 arg4, s32 arg5);
s32 func_800376E8(s32 song);
s32 func_80037828(s32 song, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
s32 func_80037C04(s32 handle, s32 arg1);
void Block_RelocateSelf(AudioFxBlock* block);

#endif // _PLAYER_H_
