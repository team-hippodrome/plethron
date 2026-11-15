/* Stadium Zero resident code. */
/* C ported from the Stadium 1 decomp: src/libnumus/player.c. */

#include "libnumus/player.h"
#include "audio_commands.h"
#include "audio_manager.h"
#include "libnaudio/n_synsetpriority.h"
#include "audio_port.h"
typedef u8* (*command_func_t)(channel_t*, u8*);
typedef u8* (*api_func_t)(u8*);
extern s32 mus_song_stop_countdown;
extern u8 mus_voice_start_immediate_mode;
extern command_func_t command_func_jumptable[];
extern api_func_t api_func_jumptable[];
extern ALPlayer plr_player;
extern ALHeap heap;
extern s32 pad_D_800FC7C8;
extern u32 max_channels;
extern channel_t* mus_channels;
extern u32 mus_vsyncs_per_second;
extern u32 mus_next_frame_time;
extern u16 mus_master_volume_effects;
extern u16 mus_master_volume_songs;
extern u32 mus_current_handle;
extern s32 mus_random_seed;
extern u8* mus_command_queue;
extern s32 mus_command_queue_size;
extern u32 mus_command_queue_max_size;
extern u8 mus_use_fixed_pan_mode;
extern u32 mus_current_frame_counter;
extern audio_voice_t* mus_voices;
extern audio_voice_t* mus_free_voice_1;
extern audio_voice_t* mus_free_voice_2;
extern audio_voice_t* mus_releasing_voice_3;
extern audio_voice_t* mus_releasing_voice_4;
extern ALHeap* audio_heap;
extern u32 mus_num_vvoices;
extern s32 mus_releasing_voices_count;

#include "m2c_macros.h"

u8* Audio_EnableVoiceVelocityFromStream(channel_t* cp, u8* ptr);
u8* Audio_DisableVoiceVelocityFromStream(channel_t* cp, u8* ptr);
u8* Audio_DisableVoicePeriodicPitchModulation(channel_t* cp, u8* ptr);
u8* Audio_SkipVoiceCommandWord(UNUSED channel_t* cp, u8* ptr);
u8* Audio_SkipVoiceCommandByte(UNUSED channel_t* cp, u8* ptr);
void Audio_RelocateSoundBankWrapper(SoundBank* arg0, uintptr_t arg1);
f32 Audio_ExpApproximation(f32 arg0);
void Audio_RelocateSoundBank(u8* arg0, uintptr_t arg1);
s32 Audio_RandomScale(s32 arg0);
void Audio_FillBytes(u8* arg0, u8 arg1, u32 arg2);
void Audio_RelocatePointerArray(u8* arg0, u32 arg1, u32 arg2);

u8* func_80036640(channel_t* arg0, u8* arg1) { // PORT: command handler; arg1 and the result were s32
    arg0->pvolumebase = NULL;
    arg0->ppitchbendbase = NULL;
    arg0->song_addr = NULL;
    arg0->IsFX = 0;
    arg0->handle = 0;
    return NULL;
}

u8* Audio_SetVoiceSampleIndex(channel_t* cp, u8* ptr) {
    u16 wave = *ptr++;

    if (wave & 0x80) {
        wave = (wave & 0x7F) << 8;
        wave |= *ptr++;
    }

    cp->wave = wave;
    return ptr;
}

u8* Audio_EnableVoicePitchSlide(channel_t* cp, u8* ptr) {
    cp->port = *ptr++;
    if (cp->port) {
        cp->port_base = cp->base_note;
    }
    return ptr;
}

u8* func_800366BC(channel_t* arg0, u8* arg1) { // PORT: command handler; arg1 and the result were s32
    arg0->port = 0;
    return arg1;
}

/**
 * Seen in play (2026-11-13) (tour of menus and a COM battle): ran in the main menu, the Pokédex,
 * the party screen, the Lab list, Lab sort, registration, the Lab PC, Free Battle setup, the battle
 * opening, a switch in battle, running from battle and a Pound turn.
 * @unverified Name was previously used only by callers of this address; the definition was still func_.
 */
u8* Audio_SetVoiceVolumeEnvelope(channel_t* cp, u8* ptr) {
    u8 value;

    value = *ptr++;
    if (value == 0) {
        value = 1;
    }
    cp->env_speed = value;
    cp->env_speed_calc = 0x400 / value;
    cp->env_init_vol = 0;

    ptr++;

    value = *ptr++;
    if (value == 0) {
        value = 1;
    }
    cp->env_attack_speed = value;
    cp->env_max_vol = *ptr++;
    cp->env_attack_calc = (1.0 / (f32)value) * (f32)(cp->env_max_vol - cp->env_init_vol);

    value = *ptr++;
    if (value == 0) {
        value = 1;
    }
    cp->env_decay_speed = value;
    cp->env_sustain_vol = *ptr++;
    cp->env_decay_calc = (1.0 / (f32)value) * (f32)(cp->env_sustain_vol - cp->env_max_vol);

    value = *ptr++;
    if (value == 0) {
        value = 1;
    }
    cp->env_release_speed = value;

    return ptr;
}

extern u32 D_800B364C;
u8* Audio_SetVoiceSequenceTempo(channel_t* cp, u8* ptr) {
    s32 i;
    s32 temp_lo;
    s32 temp2;
    channel_t* sp;

    temp_lo = ((*ptr++ * 0x6000) / 120) / (s32)D_800B364C;
    temp2 = (cp->temscale * temp_lo) >> 7;

    if (cp->IsFX != 0) {
        cp->channel_tempo = temp_lo;
    } else {
        for (i = 0, sp = mus_channels; i < (s32)max_channels; i++, sp++) {
            if (sp->song_addr == cp->song_addr) {
                sp->channel_tempo_save = temp_lo;
                sp->channel_tempo = temp2;
            }
        }
    }

    return ptr;
}

u8* Audio_SetVoiceReleaseDuration(channel_t* cp, u8* ptr) {
    cp->endit = *ptr++;
    cp->cutoff = 0;
    return ptr;
}

u8* Audio_SetVoiceFixedDuration(channel_t* cp, u8* ptr) {
    s16 tmp = *ptr++ << 8;

    tmp |= *ptr++;

    cp->cutoff = tmp;
    cp->endit = 0;

    return ptr;
}

u8* Audio_EnableVoiceVibrato(channel_t* cp, u8* ptr) {
    cp->vib_delay = *ptr++;
    cp->vib_speed = *ptr++;
    cp->vib_amount = (f32)*ptr++ / 50.0;
    return ptr;
}

u8* Audio_EnableVoiceVibratoDown(channel_t* cp, u8* ptr) {
    cp->vib_delay = *ptr++;
    cp->vib_speed = *ptr++;
    cp->vib_amount = -(f32)*ptr++ / 50.0;

    return ptr;
}

u8* func_800369DC(channel_t* arg0, u8* arg1) { // PORT: command handler; arg1 and the result were s32
    arg0->vib_speed = 0;
    arg0->vibrato = 0.0f;
    return arg1;
}

u8* Audio_SetVoiceDurationOverride(channel_t* cp, u8* ptr) {
    u8 value = *ptr++;

    if (value < 0x80) {
        cp->fixed_length = value;
    } else {
        cp->fixed_length = (value & 0x7F) << 8;
        cp->fixed_length += *ptr++;
    }
    return ptr;
}

u8* func_80036A34(channel_t* arg0, u8* arg1) { // PORT: command handler; arg1 and the result were s32
    arg0->ignore = 1;
    return arg1;
}

u8* func_80036A44(channel_t* arg0, u8* arg1) {
    arg0->transpose = (u8) *arg1;
    return arg1 + 1;
}

u8* func_80036A54(channel_t* arg0, u8* arg1) { // PORT: command handler; arg1 and the result were s32
    arg0->ignore_transpose = 1;
    return arg1;
}

u8* Audio_SetVoiceFinePitch(channel_t* cp, u8* ptr) {
    s32 value = *ptr++;

    if (value & 0x80) {
        value |= ~0xFF;
    }
    cp->freqoffset -= cp->distort;
    cp->distort = (f32)value / 100.0;
    cp->freqoffset += cp->distort;
    return ptr;
}

u8* Audio_SetVoiceEnvelopeFromTable(channel_t* cp, u8* ptr) {
    s32 value = *ptr++;

    if (value & 0x80) {
        value = (value & 0x7F) << 8;
        value |= *ptr++;
    }

    // PORT: env_table is an offset from the song
    Audio_SetVoiceVolumeEnvelope(cp, PORT_BLOB_AT(cp->song_addr, cp->song_addr->env_table) + value * sizeof(envelope_t));
    return ptr;
}

u8* Audio_EnableVoiceVelocityFromStream(channel_t* cp, u8* ptr) {
    cp->velocity_on = 1;
    return ptr;
}

u8* Audio_DisableVoiceVelocityFromStream(channel_t* cp, u8* ptr) {
    cp->velocity_on = 0;
    return ptr;
}

u8* func_80036B44(channel_t* arg0, u8* arg1) { // PORT: command handler; arg1 and the result were s32
    arg0->default_velocity = 1;
    return arg1;
}

u8* func_80036B54(channel_t* arg0, u8* arg1) { // PORT: command handler; arg1 and the result were s32
    arg0->default_velocity = 0;
    return arg1;
}

u8* Audio_PushVoiceLoopState(channel_t* cp, u8* ptr) {
    u8 index = cp->for_stack_count;

    cp->for_count[index] = *ptr++;

    cp->for_stack[index] = ptr;
    cp->for_stackvol[index] = cp->pvolumebase;
    cp->for_stackpb[index] = cp->ppitchbendbase;

    cp->for_volume[index] = cp->volume;
    cp->for_pitchbend[index] = cp->pitchbend;

    cp->for_vol_count[index] = cp->cont_vol_repeat_count;
    cp->for_pb_count[index] = cp->cont_pb_repeat_count;

    cp->for_stack_count++;
    return ptr;
}

u8* Audio_PopVoiceLoopState(channel_t* cp, u8* ptr) {
    s32 index = cp->for_stack_count - 1;

    if (cp->for_count[index] != 0xFF) {
        cp->for_count[index]--;
        if (!cp->for_count[index]) {
            cp->for_stack_count = index;
            index = -1;
        }
    }

    if (index >= 0) {
        ptr = cp->for_stack[index];

        cp->pvolumebase = cp->for_stackvol[index];
        cp->ppitchbendbase = cp->for_stackpb[index];
        cp->volume = cp->for_volume[index];

        cp->pitchbend = cp->for_pitchbend[index];
        cp->cont_vol_repeat_count = cp->for_vol_count[index];
        cp->cont_pb_repeat_count = cp->for_pb_count[index];
        cp->bendrange = 0.03125f;
        cp->pitchbend_calc = cp->pitchbend * cp->bendrange;
    }

    return ptr;
}

u8* func_80036D04(channel_t* arg0, u8* arg1) {
    arg0->sweep_speed = (u8) M2C_FIELD(arg1, u8 *, 0);
    arg0->unk_0E1 = (u8) M2C_FIELD(arg1, u8 *, 1);
    arg0->unk_0E2 = (u8) M2C_FIELD(arg1, u8 *, 2);
    return arg1 + 3;
}

u8* func_80036D24(channel_t* arg0, u8* arg1) { // PORT: command handler; arg1 and the result were s32
    arg0->unk_0E1 = 0;
    return arg1;
}

u8* func_80036D30(channel_t* arg0, u8* arg1) { // PORT: command handler; arg1 and the result were s32
    arg0->wobble_on_speed = 1;
    return arg1;
}

u8* Audio_DisableVoicePeriodicPitchModulation(channel_t* cp, u8* ptr) {
    cp->wobble_on_speed = 0;
    return ptr;
}

u8* Audio_SetVoiceDefaultVelocity(channel_t* cp, u8* ptr) {
    cp->wobble_off_speed = *ptr++;
    cp->wobble_on_speed = 0;
    return ptr;
}

u8* func_80036D60(channel_t* arg0, u8* arg1) {
    arg0->pan = (s8) ((s32) *arg1 / 2);
    return arg1 + 1;
}

u8* Audio_SkipVoiceCommandWord(UNUSED channel_t* cp, u8* ptr) {
    ptr++;
    ptr++;
    return ptr;
}

u8* Audio_SetVoiceSubEnvelopePointer(channel_t* cp, u8* ptr) {
    // PORT: drum_table is an offset from the song, its entries offsets from the song
    u8* addr = PORT_BLOB_AT(cp->song_addr, (s32)Port_ReadBE32(PORT_BLOB_AT(cp->song_addr, cp->song_addr->drum_table) +
                                                              4 * *ptr++)); // PORT: big-endian table

    cp->pdrums = addr;
    return ptr;
}

u8* func_80036DB4(channel_t* arg0, u8* arg1) { // PORT: command handler; arg1 and the result were s32
    arg0->pdrums = NULL;
    return arg1;
}

u8* Audio_SkipVoiceCommandByte(UNUSED channel_t* cp, u8* ptr) {
    #ifdef _AUDIODEBUG
        osSyncPrintf("PLAYER_COMMANDS.C: Audio_SkipVoiceCommandByte -  %d (channel frame=%d)\n", *ptr++,  cp->channel_frame);
        return ptr;
    #else
        ptr++;
        return ptr;
    #endif
}

u8* Audio_SetVoiceEnvelopePointers(channel_t* cp, u8* ptr) {
    s32 tmp0;
    s32 tmp2;

    tmp0 = *ptr++ << 8;
    tmp0 += *ptr++;

    tmp2 = *ptr++ << 8;
    tmp2 += *ptr++;

    cp->pvolumebase = &cp->pvolumebase_offset[tmp2];
    cp->cont_vol_repeat_count = 1;

    tmp2 = *ptr++ << 8;
    tmp2 += *ptr++;

    cp->ppitchbendbase = &cp->ppitchbendbase_offset[tmp2];
    cp->cont_pb_repeat_count = 1;

    return &cp->pbase[tmp0];
}

u8* func_80036E30(channel_t* arg0, u8* arg1) {
    arg0->reverb = (u8) *arg1;
    return arg1 + 1;
}

u8* Audio_SetRandomVoiceDetune(channel_t* cp, u8* ptr) {
    cp->transpose = Audio_RandomScale(*ptr++);
    cp->transpose += *ptr++;
    return ptr;
}

u8* Audio_SetRandomVoiceVolume(channel_t* cp, u8* ptr) {
    cp->volume = Audio_RandomScale(*ptr++);
    cp->volume += *ptr++;
    return ptr;
}

u8* func_80036EE0(channel_t* cp, u8* ptr) {
    cp->pan = Audio_RandomScale(*ptr++);
    cp->pan += *ptr++;
    return ptr;
}

u8* func_80036F30(channel_t* arg0, u8* arg1) {
    arg0->volume = (u8) *arg1;
    return arg1 + 1;
}

M2C_UNK func_80037828(s32, s32, s32, s32, s32);
extern s32 D_800B3638;
extern channel_t* D_800B3640;
u8* func_80036F40(channel_t* cp, u8* ptr) {
    s32 id = *ptr++;
    s32 ret;
    s32 i;
    channel_t* p;

    if (id >= 0x80) {
        id = ((id & 0x7F) << 8) + *ptr++;
    }
    ret = func_80037828(id, cp->volume, cp->sweep_amount, 0, cp->priority++);
    cp->priority--;
    if (ret == 0) {
        return ptr;
    }
    p = D_800B3640;
    for (i = 0; i < D_800B3638; i++, p++) {
        if (ret == p->handle) {
            p->handle = cp->handle;
            p->sample_bank = cp->sample_bank;
            p->volume_scale = cp->volume_scale;
            p->duration = cp->duration;
            p->duration_scale = cp->duration_scale;
        }
    }
    return ptr;
}

u8* Audio_SetVoiceVolumeScale(channel_t* cp, u8* ptr) {
    cp->bendrange = (f32)*ptr++ * 0.015625;
    cp->pitchbend_calc = SQ(cp->pitchbend);
    return ptr;
}

u8* Audio_SetVoicePanModulation(channel_t* cp, u8* ptr) {
    cp->wobble_counter = *ptr++;
    if (!cp->wobble_counter) {
        cp->sweep_amount = 0x80;
    }
    return ptr;
}

s32 func_8003821C(SoundBank*, AudioFxBlock*, s32, s32, s32, s32, s32);

/**
 * Command handler (same shape as func_80036F40): reads a sound id from `ptr`, either one
 * byte, or, when that byte is >= 0x80, two bytes as ((first & 0x7F) << 8) + second. It
 * then starts that sound with func_8003821C, passing the channel's offsets 0x140 and
 * 0x13C, the id, the s16 at 0xAA, the s16 at 0xBC, 0 and the channel's offset-0x60 value
 * (`priority`), which is incremented across the call and decremented again afterwards.
 * If func_8003821C returns a non-zero handle, every channel whose offset-0x5C handle
 * equals it receives this channel's handle (0x5C), 0x88, 0xAA, 0x38 and 0x3C fields.
 * Returns the advanced command pointer. What the started object is is not established.
 */
u8* func_800370AC(channel_t* cp, u8* ptr) {
    u32 id = *ptr++;
    s32 ret;
    s32 i;
    channel_t* p;

    if (id >= 0x80) {
        id = ((id & 0x7F) << 8) + *ptr++;
    }
    ret = func_8003821C(cp->fx_sample_bank, cp->fx_block, id, cp->volume_scale, cp->sweep_amount, 0, cp->priority++);
    cp->priority--;
    if (ret == 0) {
        return ptr;
    }
    p = D_800B3640;
    for (i = 0; i < D_800B3638; i++, p++) {
        if (ret == p->handle) {
            p->handle = cp->handle;
            p->sample_bank = cp->sample_bank;
            p->volume_scale = cp->volume_scale;
            p->duration = cp->duration;
            p->duration_scale = cp->duration_scale;
        }
    }
    return ptr;
}


extern s32* D_800B3648;
extern SoundBank* D_800B3664;
extern SoundBank* D_800B3668;
extern s32 D_800B365C;
extern s32 D_800B3CC8;
extern s32 D_800B3CCC;
extern s32 D_800B3CD0;
void func_800374E0(s32, s32);
void func_8003A2B8(channel_t*);
extern s32 D_80000300;
extern s32 D_80075C34;
extern u32* D_800B3644; // PORT: was s32
static AudioFxBlock* sPortFxBlock; // PORT: the block whose offsets D_800B3644 holds
extern s32 D_800B3650;
extern s32 D_800B3660;
extern u32 D_800B3CC4;
extern audio_voice_t D_800B3670[45];
extern ALPlayer D_800B3610;
extern ALHeap D_800B3628;
extern void* D_800B363C;
void func_8003A564(void*, s32, s32);
void amCreateAudioMgr(ALSynConfig* c, amConfig* amc, u32 num_dma_buffers, s32 arg3, s32 arg4);
void func_800468F0(ALPlayer*);
s32 func_800388BC(void*);

/**
 * Sound-player initialisation (the Stadium Zero counterpart of the retail libmus
 * initialiser). `arg0` is the caller's configuration block:
 *   0x00 channel count (D_800B3638)        0x04 ALHeap* used for all allocation
 *   0x08 pointer stored in D_800B3664 and D_800B3668
 *   0x0C -> D_800B3644, 0x10 -> D_800B3648 (the song table that func_80037828 indexes)
 *   0x14, 0x18, 0x1C, 0x20, 0x24, 0x28: values forwarded to amCreateAudioMgr.
 * Steps: set D_800B364C to 50 if the word at 0x80000300 is 0 (osTvType == PAL), else 60,
 * and D_800B3650 to 1000000 / D_800B364C; allocate and zero the 0x1C-byte-per-channel
 * table D_800B363C and the 0x148-byte-per-channel array D_800B3640; build the two
 * parameter blocks and call amCreateAudioMgr; call func_800374E0(7, 0x7FFF); reset the
 * handle counter D_800B365C to 1 and D_800B3660 to 0x12345678, and clear D_800B3CC4,
 * D_800B3CC8, D_800B3CCC, D_80075C34 and D_800B3CD0;
 * register the player at D_800B3610 (handler func_800388BC, clientData = itself) with
 * func_800468F0; clear offsets 0xDD and 0x30 and call func_8003A2B8 on every channel;
 * set the byte at offset 0x1E of all 45 entries of D_800B3670 to 5; finally copy the
 * ALHeap into D_800B3628 and return its used size (cur - base).
 * The `pad` local only reserves the stack slot the retail frame has there.
 * What the 0x1C-byte table and several configuration words are is not established.
 */
s32 func_800371A4(AudioInitConfig* arg0) {
    s32 i;
    UNUSED s32* pad;
    ALSynConfig cfg; // PORT: was a 0x20-byte block of s32s; amCreateAudioMgr stores host pointers into it
    amConfig rate;

    D_800B3638 = arg0->channels;
    D_800B3644 = arg0->unk_0C;
    sPortFxBlock = arg0->portFxBlock; // PORT
    D_800B3648 = arg0->unk_10;
    if (D_80000300 == 0) {
        D_800B364C = 0x32;
    } else {
        D_800B364C = 0x3C;
    }
    D_800B3650 = 0xF4240 / (s32)D_800B364C;
    D_800B363C = alHeapDBAlloc(0, 0, arg0->heap, 1, D_800B3638 * 0x1C);
    // PORT: sizeof(channel_t) instead of 0x148 (the host struct holds 64-bit pointers)
    D_800B3640 = alHeapDBAlloc(0, 0, arg0->heap, 1, D_800B3638 * sizeof(channel_t));
    func_8003A564(D_800B363C, 0, D_800B3638 * 0x1C);
    func_8003A564(D_800B3640, 0, D_800B3638 * sizeof(channel_t));
    D_800B3668 = D_800B3664 = arg0->bank;
    cfg.maxVVoices = 0x2D;
    cfg.maxPVoices = D_800B3638;
    cfg.maxUpdates = arg0->synUpdates;
    cfg.dmaproc = NULL;
    cfg.fxType = 2;
    cfg.outputRate = 0;
    cfg.heap = arg0->heap;
    rate.outputRate = arg0->outputRate;
    rate.framesPerField = arg0->synRetraceCount;
    rate.maxACMDSize = arg0->synRspCmds;
    amCreateAudioMgr(&cfg, &rate, arg0->synNumDmaBufs, arg0->synDmaBufSize, D_800B364C);
    func_800374E0(7, 0x7FFF);
    D_800B365C = 1;
    D_800B3660 = 0x12345678;
    D_800B3CC4 = 0;
    D_800B3CC8 = 0;
    D_800B3CCC = 0;
    D_80075C34 = 0;
    D_800B3CD0 = 0;
    D_800B3610.next = NULL;
    D_800B3610.handler = func_800388BC;
    D_800B3610.clientData = &D_800B3610;
    func_800468F0(&D_800B3610);
    for (i = 0; i < D_800B3638; i++) {
        D_800B3640[i].playing = 0;
        D_800B3640[i].pdata = NULL;
        func_8003A2B8(&D_800B3640[i]);
    }
    for (i = 0; i < 45; i++) {
        D_800B3670[i].env_phase = 5;
    }
    D_800B3628 = *arg0->heap;
    return (s32)(D_800B3628.cur - D_800B3628.base);
}

extern s16 D_800B3654;
extern s16 D_800B3656;
void func_800374E0(s32 arg0, s32 arg1) {
    if (arg0 & 1) {
        D_800B3654 = arg1;
    }
    if (arg0 & 2) {
        D_800B3656 = arg1;
    }
}

extern s32 D_800B3CC8;
extern s32 D_800B365C;
extern SoundBank* D_800B366C; // PORT: was s32
void func_8003A598(void*, void*, s32);
void func_8003A2B8(channel_t*);
u32 func_8003A4A8(song_t*, s32);
/**
 * PORT: first use of a freshly loaded song: byte-swap the header and the channel / volume / pitch-bend offset tables
 * to host order. They stay offsets from the song (on the N64 Audio_RelocatePointerArray turns them into addresses).
 * A fresh big-endian header reads as a byte-swapped channel_data offset (>= 0x10000; songs are smaller than 64 KiB);
 * the N64 code tests `< 0x400` on the big-endian word for the same purpose.
 */
static void Audio_PortRelocateSong(song_t* song) {
    u32 n;

    if (song->channel_data >= 0x10000) {
        Port_SwapWords(song, 6);
        n = song->num_channels;
        Port_SwapWords(PORT_BLOB_AT(song, song->channel_data), n);
        if (song->volume_data != song->channel_data) {
            Port_SwapWords(PORT_BLOB_AT(song, song->volume_data), n);
        }
        if (song->pitchbend_data != song->channel_data && song->pitchbend_data != song->volume_data) {
            Port_SwapWords(PORT_BLOB_AT(song, song->pitchbend_data), n);
        }
    }
}

s32 func_80037508(song_t* song) {
    UNUSED s32 pad; // FAKE: stack padding
    s32 handle;
    s32 mask;
    s32 count;
    s32 i;
    channel_t* cp;
    s32 index;
    u32 off;

    mask = osSetIntMask(1);
    D_800B3CC8 = 1;
    osSetIntMask(mask);
    Audio_PortRelocateSong(song); // PORT: replaces the in-place relocation (Audio_RelocatePointerArray)
    count = song->num_channels;
    mask = osSetIntMask(1);
    handle = D_800B365C++;
    osSetIntMask(mask);
    for (i = 0; i < count; i++) {
        if (PORT_SONG_TABLE(song, channel_data, i) == 0) {
            continue;
        }
        index = func_8003A4A8(song, i);
        if (index == -1) {
            continue;
        }
        cp = &D_800B3640[index];
        func_8003A2B8(cp);
        mask = osSetIntMask(1);
        cp->sample_bank = D_800B366C;
        cp->song_addr = song;
        off = PORT_SONG_TABLE(song, volume_data, i);
        cp->pvolumebase = cp->pvolumebase_offset = PORT_BLOB_PTR_OR_NULL(song, off);
        off = PORT_SONG_TABLE(song, pitchbend_data, i);
        cp->ppitchbendbase = cp->ppitchbendbase_offset = PORT_BLOB_PTR_OR_NULL(song, off);
        cp->pdata = cp->pbase = PORT_BLOB_AT(song, PORT_SONG_TABLE(song, channel_data, i));
        cp->handle = handle;
        osSetIntMask(mask);
    }
    mask = osSetIntMask(1);
    D_800B3CC8 = 0;
    osSetIntMask(mask);
    return handle;
}

extern s32 D_800B3CCC;
extern s32* D_800B3648;
s32 func_8003A638(channel_t*, s32, s32, s32, s32);

/**
 * Claims the first free channel and starts it. With the D_800B3CCC flag set to 1 (each
 * write made with interrupts masked via osSetIntMask(1)), it scans the channel array for
 * one whose `pdata` is NULL, marks it in use by storing -1 there, and calls
 * func_8003A638(channel, ...) on it, then clears the flag and returns that call's result.
 * Returns 0 (and clears the flag) if no channel is free.
 * Same as func_80037828 with arg1 = arg2 = 0x80 and the last argument always taken from
 * D_800B3648[song] (D_800B3648 is a pointer to a table of s32).
 */
s32 func_800376E8(s32 song) {
    if (!gAudioPortReady) { // PORT: the sound data is not loaded (PORT_NO_AUDIO, or not yet)
        return 0;
    }
    channel_t* cp;
    s32 i;
    s32 found;
    s32 arg;
    u32 mask;
    s32 ret;

    found = 0;
    mask = osSetIntMask(1);
    D_800B3CCC = 1;
    osSetIntMask(mask);
    cp = mus_channels;
    i = 0;
    arg = D_800B3648[song];
    for (; i < (s32)max_channels; i++, cp++) {
        mask = osSetIntMask(1);
        if (cp->pdata == NULL) {
            cp->pdata = (u8*)-1;
            found = 1;
        }
        osSetIntMask(mask);
        if (found != 0) {
            ret = func_8003A638(cp, song, 0x80, 0x80, arg);
            mask = osSetIntMask(1);
            D_800B3CCC = 0;
            osSetIntMask(mask);
            return ret;
        }
    }
    mask = osSetIntMask(1);
    D_800B3CCC = 0;
    osSetIntMask(mask);
    return 0;
}

extern s32 D_800B3CCC;
extern s32* D_800B3648;
s32 func_8003A638(channel_t*, s32, s32, s32, s32);

/**
 * Claims the first free channel and starts it. With the D_800B3CCC flag set to 1 (each
 * write made with interrupts masked via osSetIntMask(1)), it scans the channel array for
 * one whose `pdata` is NULL, marks it in use by storing -1 there, and calls
 * func_8003A638(channel, ...) on it, then clears the flag and returns that call's result.
 * Returns 0 (and clears the flag) if no channel is free.
 * Arguments: song, arg1, arg2, arg3 (unused), arg4. The call is
 * func_8003A638(channel, song, arg1, arg2, arg4), where arg4 is first replaced by
 * D_800B3648[song] if song is not 0xFFFF and arg4 is -1. D_800B3648 is read as a
 * pointer to a table of s32 (the retail code loads it with lw).
 * What the table and the started object are is not established.
 */
s32 func_80037828(s32 song, s32 arg1, s32 arg2, s32 arg3, s32 arg4) {
    if (!gAudioPortReady) { // PORT: the sound data is not loaded (PORT_NO_AUDIO, or not yet)
        return 0;
    }
    channel_t* cp;
    s32 i;
    s32 found;
    u32 mask;
    s32 ret;

    found = 0;
    mask = osSetIntMask(1);
    D_800B3CCC = 1;
    osSetIntMask(mask);
    if ((song != 0xFFFF) && (arg4 == -1)) {
        arg4 = D_800B3648[song];
    }
    for (i = 0, cp = mus_channels; i < (s32)max_channels; i++, cp++) {
        mask = osSetIntMask(1);
        if (cp->pdata == NULL) {
            cp->pdata = (u8*)-1;
            found = 1;
        }
        osSetIntMask(mask);
        if (found != 0) {
            ret = func_8003A638(cp, song, arg1, arg2, arg4);
            mask = osSetIntMask(1);
            D_800B3CCC = 0;
            osSetIntMask(mask);
            return ret;
        }
    }
    mask = osSetIntMask(1);
    D_800B3CCC = 0;
    osSetIntMask(mask);
    return 0;
}

/**
 * For every channel whose type matches `flags` (MUSFLAG_EFFECTS selects channels whose
 * u16 at offset 0xB2 is nonzero, MUSFLAG_SONGS those where it is 0), sets `duration`
 * (offset 0x38) to arg1 and `duration_scale` (offset 0x3C) to arg1, or to 1 when arg1
 * is 0, with interrupts masked (osSetIntMask(1), then restored). Companion of
 * func_80037AC4, which does the same for one handle.
 */
void func_8003798C(s32 flags, s32 duration) {
    channel_t* cp;
    s32 scale;
    s32 i;
    u32 mask;

    scale = (duration != 0) ? duration : 1;
    mask = osSetIntMask(1);

    for (i = 0, cp = mus_channels; i < (s32)max_channels; i++, cp++) {
        if (((cp->IsFX != 0) && (flags & MUSFLAG_EFFECTS)) || ((cp->IsFX == 0) && (flags & MUSFLAG_SONGS))) {
            cp->duration = duration;
            cp->duration_scale = scale;
        }
    }

    osSetIntMask(mask);
}

/**
 * Counts the channels that are in use: `pdata` (offset 0x30) is neither NULL nor -1, and
 * the channel's type matches `flags` (MUSFLAG_EFFECTS selects channels whose u16 at
 * offset 0xB2 is nonzero, MUSFLAG_SONGS those where it is 0). Does not mask interrupts.
 * Written with the offset-0xB2 field read through M2C_FIELD: player.h places the
 * equivalent `IsFX` at 0xBA, which does not match this build's data.
 * Seen in play (2026-11-13) ran when Start was pressed at the title screen (console reset, then N64
 * logo, opening, title and main menu).
 * @unverified Name was previously used only by callers of this address; the definition was still func_.
 */
s32 Audio_IsSoundPlaying(s32 flags) {
    channel_t* cp;
    s32 i;
    s32 count;

    for (i = 0, cp = mus_channels, count = 0; i < (s32)max_channels; i++, cp++) {
        if ((cp->pdata != NULL) && (cp->pdata != (u8*)-1)) {
            if (((cp->IsFX != 0) && (flags & MUSFLAG_EFFECTS)) || ((cp->IsFX == 0) && (flags & MUSFLAG_SONGS))) {
                count++;
            }
        }
    }

    return count;
}

/**
 * For every channel whose handle equals `handle`, sets `duration` (offset 0x38) to
 * arg1 and `duration_scale` (offset 0x3C) to arg1, or to 1 when arg1 is 0, with
 * interrupts masked (osSetIntMask(1), then restored). Returns the number of channels
 * changed; returns 0 without doing anything for handle 0. Same shape as func_80037D4C.
 * The declaration order of the locals is what produces the retail stack layout.
 */
s32 func_80037AC4(s32 handle, s32 duration) {
    channel_t* cp;
    s32 scale;
    s32 count;
    s32 i;
    u32 mask;

    if (handle == 0) {
        return 0;
    }

    scale = (duration != 0) ? duration : 1;
    mask = osSetIntMask(1);

    for (i = 0, count = 0, cp = mus_channels; i < (s32)max_channels; i++, cp++) {
        if (handle == cp->handle) {
            cp->duration = duration;
            cp->duration_scale = scale;
            count++;
        }
    }

    osSetIntMask(mask);

    return count;
}

s32 Audio_QueueSetMonoOutputCommand(s32 handle) {
    channel_t* cp;
    s32 i;
    s32 count;
    u32 mask;

    if (handle == 0) {
        return 0;
    }

    mask = osSetIntMask(1);

    for (i = 0, count = 0, cp = mus_channels; i < (s32)max_channels; i++, cp++) {
        if (handle == cp->handle) {
            count++;
        }
    }

    osSetIntMask(mask);

    return count;
}

/**
 * Sets the field at channel offset 0xAA (`volume_scale` in player.h) to arg1 on every
 * channel whose handle equals `handle`, with interrupts masked (osSetIntMask(1), then
 * restored). Returns the number of channels changed; returns 0 without doing anything
 * for handle 0. Same shape as func_80037D4C.
 */
s32 func_80037C04(s32 handle, s32 arg1) {
    channel_t* cp;
    s32 i;
    s32 count;
    u32 mask;

    if (handle == 0) {
        return 0;
    }

    mask = osSetIntMask(1);

    for (i = 0, count = 0, cp = mus_channels; i < (s32)max_channels; i++, cp++) {
        if (handle == cp->handle) {
            cp->volume_scale = arg1;
            count++;
        }
    }

    osSetIntMask(mask);

    return count;
}

/**
 * For every channel whose handle equals `handle`, stores arg1 at channel offset 0xBC
 * (`sweep_amount` / `channel_tempo_save` in player.h) and sets the byte at 0xCC
 * (`pan_current`) to 0xFF, with interrupts masked (osSetIntMask(1), then restored).
 * Returns the number of channels changed; returns 0 without doing anything for handle 0.
 * Same shape as func_80037D4C.
 */
s32 func_80037CA4(s32 handle, s32 arg1) {
    channel_t* cp;
    s32 i;
    s32 count;
    u32 mask;

    if (handle == 0) {
        return 0;
    }

    mask = osSetIntMask(1);

    for (i = 0, count = 0, cp = mus_channels; i < (s32)max_channels; i++, cp++) {
        if (handle == cp->handle) {
            cp->sweep_amount = arg1;
            cp->pan_current = 0xFF;
            count++;
        }
    }

    osSetIntMask(mask);

    return count;
}

s32 func_80037D4C(s32 handle, f32 offset) {
    channel_t* cp;
    s32 i;
    s32 count;
    u32 mask;

    if (handle == 0) {
        return 0;
    }

    mask = osSetIntMask(1);

    for (i = 0, count = 0, cp = mus_channels; i < (s32)max_channels; i++, cp++) {
        if (handle == cp->handle) {
            count++;
            cp->freqoffset = cp->distort + offset;
        }
    }

    osSetIntMask(mask);

    return count;
}

/**
 * Sets the tempo scale for every channel whose handle equals `handle`. The tempo is
 * clamped to 1..0x100 (values <= 0 become 1, values above 0x100 become 0x100). For each
 * matching channel it stores the clamped value in `temscale` (offset 0xA4) and sets
 * `channel_tempo` (offset 0xA8) to (u16 at offset 0xB4 * tempo) >> 7. Returns the
 * number of channels changed; returns 0 for handle 0. Unlike its neighbors it does
 * not mask interrupts. The field at 0xB4 is unnamed in player.h.
 */
s32 func_80037DF4(s32 handle, s32 tempo) {
    channel_t* cp;
    s32 i;
    s32 count;

    if (handle == 0) {
        return 0;
    }

    if (tempo <= 0) {
        tempo = 1;
    } else if (tempo > 0x100) {
        tempo = 0x100;
    }

    for (i = 0, cp = mus_channels, count = 0; i < (s32)max_channels; i++, cp++) {
        if (handle == cp->handle) {
            cp->temscale = tempo;
            count++;
            cp->channel_tempo = (cp->channel_tempo_save * tempo) >> 7;
        }
    }

    return count;
}

/**
 * Clamps `value` to 0..0x7F, then for every channel whose handle equals `handle` stores
 * it in `reverb_intensity` (offset 0xDF) and sets `reverb_mix` (offset 0xE0) to 0xFF,
 * with interrupts masked (osSetIntMask(1), then restored). Returns the number of
 * channels changed; returns 0 without doing anything for handle 0.
 */
s32 func_80037E84(s32 handle, s32 value) {
    channel_t* cp;
    s32 i;
    s32 count;
    u32 mask;

    if (handle == 0) {
        return 0;
    }

    if (value < 0) {
        value = 0;
    } else if (value > 0x7F) {
        value = 0x7F;
    }

    mask = osSetIntMask(1);

    for (i = 0, count = 0, cp = mus_channels; i < (s32)max_channels; i++, cp++) {
        if (handle == cp->handle) {
            cp->reverb_intensity = value;
            cp->reverb_mix = 0xFF;
            count++;
        }
    }

    osSetIntMask(mask);

    return count;
}

void Audio_RelocateSoundBankWrapper(SoundBank* arg0, uintptr_t arg1) { // PORT: were s32
    Audio_RelocateSoundBank((u8*)arg0, arg1);
}

/**
 * With interrupts masked (osSetIntMask is libultra's osSetIntMask), records
 * arg0 in D_800B366C if it is non-NULL and bit 0 of its word at offset 0x10 is
 * set, restores the interrupt mask, then calls func_80037508(arg1).
 */
// PORT: returns func_80037508's handle (declared void; on the N64 it is left in $v0 and callers use it)
s32 func_80037F68(SoundBank* arg0, song_t* arg1) {
    if (!gAudioPortReady) { // PORT: the sound data is not loaded (PORT_NO_AUDIO, or not yet)
        return 0;
    }
    s32 prevMask;

    prevMask = osSetIntMask(1);
    if ((arg0 != NULL) && (arg0->flags & 1)) {
        D_800B366C = arg0;
    }
    osSetIntMask(prevMask);
    return func_80037508(arg1);
}

s32 func_800376E8(s32);
extern SoundBank* D_800B3664;
extern SoundBank* D_800B3668;

void func_80037FCC(SoundBank* arg0, s32 arg1) {
    if ((arg0 != NULL) && (arg0->flags & 1)) {
        D_800B3664 = arg0;
    }
    func_800376E8(arg1);
    D_800B3664 = D_800B3668;
}

M2C_UNK func_80037828(s32, s32, s32, s32, s32);     /* extern */
extern SoundBank* D_800B3664;
extern SoundBank* D_800B3668;

void func_80038014(SoundBank* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5) {
    if ((arg0 != NULL) && (arg0->flags & 1)) {
        D_800B3664 = arg0;
    }
    func_80037828(arg1, arg2, arg3, arg4, arg5);
    D_800B3664 = D_800B3668;
}

extern s32 D_800B3638;
extern channel_t* D_800B3640;
s32 Team_IsOpponentIndexUsed(s32 arg0) {
    s32 i;
    channel_t* p;

    if (arg0 == 0) {
        return 0;
    }
    p = D_800B3640;
    for (i = 0; i < D_800B3638; i++, p++) {
        if (arg0 == p->handle) {
            // PORT: returned the bank pointer as an s32 (no callers; battle.h declares it s32)
            return p->sample_bank != NULL;
        }
    }

    return 0;
}

// PORT: D_800B368E and D_800B3694 are D_800B3670[0].env_phase and &D_800B3670[1] at N64 offsets; index the voice
// array instead (the host struct is larger).
extern audio_voice_t D_800B3670[45];

s32 func_800380D0(void) {
    s32 i;

    if (D_800B3670[0].env_phase) {
        return 1;
    }
    for (i = 0; i < 44; i++) {
        if (D_800B3670[1 + i].env_phase) {
            return 1;
        }
    }
    return 0;
}

/**
 * Initializes channel `cp` as a new sound: calls func_8003A2B8(cp) to reset it, then
 * stores arg0 at offsets 0x88 and 0x140, arg1 at 0x13C, arg3 + 0x100 at 0xB2 (the
 * type word that Audio_IsSoundPlaying and func_8003798C test), arg4 at 0xAA, arg5 at 0xBC and
 * arg6 at 0x60 (`priority`). With interrupts masked (osSetIntMask(1) is osSetIntMask),
 * it assigns the next handle D_800B365C++ to offset 0x5C and loads
 * ((s32*)arg1[+0xC])[arg3] into offsets 0x8C and 0x30 (`pbase` / `pdata`).
 * Returns the new handle. What arg0, arg1 and arg3 index is not established.
 */
s32 func_8003815C(SoundBank* arg0, AudioFxBlock* arg1, channel_t* cp, s32 arg3, s32 arg4, s32 arg5, s32 arg6) {
    s32 mask;

    func_8003A2B8(cp);
    cp->sample_bank = arg0;
    cp->IsFX = arg3 + 0x100;
    cp->volume_scale = arg4;
    cp->sweep_amount = arg5;
    cp->priority = arg6;
    cp->fx_sample_bank = arg0;
    cp->fx_block = arg1;
    mask = osSetIntMask(1);
    cp->handle = D_800B365C;
    D_800B365C++;
    cp->pbase = cp->pdata = AUDIO_FX_ENTRY(arg1, arg3); // PORT: the table holds offsets from the block
    osSetIntMask(mask);
    return cp->handle;
}

void Block_RelocateSelf(AudioFxBlock*);
// PORT: written with the block's fields; the matching version spelled them as raw pointer-offset casts (FAKE)
s32 func_8003821C(SoundBank* arg0, AudioFxBlock* arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5, s32 arg6) {
    s32 found;
    channel_t* p;
    u32 i;
    u32 mask;

    found = 0;
    if (!(arg1->flags & 1)) {
        Block_RelocateSelf(arg1);
    }
    if ((u32)arg6 == 0xFFFFFFFFU) {
        arg6 = AUDIO_FX_UNK08_AT(arg1, arg2);
    }
    for (i = 0, p = D_800B3640; i < (u32)D_800B3638; i++, p++) {
        mask = osSetIntMask(1);
        if (p->pdata == NULL) {
            p->pdata = (u8*)-1;
            found = 1;
        }
        osSetIntMask(mask);
        if (found != 0) {
            return func_8003815C(arg0, arg1, p, arg2, arg3, arg4, arg6);
        }
    }

    return 0;
}

/*
 * func_8003834C
 *
 * Calls func_8003821C with all seven of its own arguments and returns the
 * result, with a flag word around the call: D_800B3CCC is set to 1 before
 * the call and back to 0 after it.  Each write to the flag is made with
 * interrupts masked: osSetIntMask (osSetIntMask, per symbol_addrs.txt) is
 * called with 1 (OS_IM_NONE), the flag is written, and the previous mask it
 * returned is restored.  What the flag is used for is not established.
 */
extern s32 D_800B3CCC;
s32 func_8003821C(SoundBank*, AudioFxBlock*, s32, s32, s32, s32, s32); /* extern */

s32 func_8003834C(SoundBank* arg0, AudioFxBlock* arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5, s32 arg6) {
    if (!gAudioPortReady) { // PORT: the sound data is not loaded (PORT_NO_AUDIO, or not yet)
        return 0;
    }
    s32 ret;
    s32 mask;

    mask = osSetIntMask(1);
    D_800B3CCC = 1;
    osSetIntMask(mask);
    ret = func_8003821C(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    mask = osSetIntMask(1);
    D_800B3CCC = 0;
    osSetIntMask(mask);
    return ret;
}

/**
 * Starts a sound on the first free channel without masking interrupts (compare
 * func_8003821C, which does the same with osSetIntMask(1) around each claim). Returns -1
 * at once if D_800B3CCC or D_800B3CC8 is non-zero. Otherwise, if bit 0 of the word at
 * arg1+4 is clear it calls Block_RelocateSelf(arg1); if arg6 is -1 it is replaced by
 * ((s32*)arg1[+8])[arg2]. It then scans the D_800B3640 channel array (0x148 bytes each,
 * D_800B3638 entries) for one whose word at offset 0x30 is 0, marks it in use by storing
 * -1 there, and returns func_8003815C(arg0, arg1, channel, arg2, arg3, arg4, arg6). If
 * no channel is free it returns 0. arg5 is not used. What arg0 and arg1 point to is not
 * established (func_8003815C stores them at channel offsets 0x88/0x140 and 0x13C).
 */
s32 func_800383DC(SoundBank* arg0, AudioFxBlock* arg1, s32 arg2, s32 arg3, s32 arg4, UNUSED s32 arg5, s32 arg6) {
    if (!gAudioPortReady) { // PORT: the sound data is not loaded (PORT_NO_AUDIO, or not yet)
        return 0;
    }
    s32 ret;
    channel_t* p;
    u32 i;

    ret = 0;
    if ((D_800B3CCC != 0) || (D_800B3CC8 != 0)) {
        return -1;
    }
    if (!(arg1->flags & 1)) {
        Block_RelocateSelf(arg1);
    }
    if ((u32)arg6 == 0xFFFFFFFFU) {
        arg6 = AUDIO_FX_UNK08_AT(arg1, arg2);
    }
    for (i = 0, p = D_800B3640; i < (u32)D_800B3638; i++, p++) {
        if (p->pdata == NULL) {
            p->pdata = (u8*)-1;
            ret = func_8003815C(arg0, arg1, p, arg2, arg3, arg4, arg6);
            break;
        }
    }
    return ret;
}

/*
 * Audio_GetActivityScore
 *
 * Returns 1 if Audio_IsSoundPlaying(3) is non-zero. Otherwise scans the byte at
 * offset 0x1E of each of the 45 entries of the 0x24-byte table at
 * D_800B3670 (the same table func_800380D0 scans, which starts one entry
 * later at D_800B3694 and reads the first entry's byte as D_800B368E) and
 * returns that byte plus 10 for the first entry whose byte is not 5;
 * returns 0 if all of them are 5.
 *
 * What the table entries are, and what the value 5 and the +10 mean, is not
 * established. Matching note: the loop is written over 45 entries from
 * D_800B3670; reading the first byte separately as D_800B368E followed by a
 * loop over D_800B3694 gives different register allocation.
 */
s32 Audio_IsSoundPlaying(M2C_UNK);                         /* extern */
extern audio_voice_t D_800B3670[45];
/**
 * Seen in play (2026-11-13) ran when Start was pressed at the title screen (console reset, then N64
 * logo, opening, title and main menu).
 */
s32 Audio_GetActivityScore(void) {
    s32 i;

    if (!gAudioPortReady) { // PORT: the sound data is not loaded (PORT_NO_AUDIO, or not yet): nothing plays
        return 0;
    }
    if (Audio_IsSoundPlaying(3) != 0) {
        return 1;
    }
    for (i = 0; i < 45; i++) {
        if (D_800B3670[i].env_phase != 5) {
            return D_800B3670[i].env_phase + 0xA;
        }
    }
    return 0;
}

/* osSetIntMask, declared with an int mask: the constant 1 is then shared with the int stores below. */
/**
 * Stop all active songs (not FX), fade them out
 * Seen in play (2026-11-13) ran when つづける was picked at トーナメントをつづけますか？ after winning tournament
 * round 1.
 * @unverified Name was previously used only by callers of this address; the definition was still func_. Comment from Stadium 1.
 */
void Audio_StopAllSoundEffects(void) {
    UNUSED s32 pad[4]; // FAKE: stack padding
    s32 mask;
    u32 i;
    channel_t* p;

    mask = osSetIntMask(1);
    D_800B3CC8 = 1;
    osSetIntMask(mask);

    mask = osSetIntMask(1);
    p = D_800B3640;
    for (i = 0; i < D_800B3638; i++, p++) {
        if ((p->IsFX != 0) || (p->pdata == NULL)) {
            continue;
        }

        p->pdata = func_80036640(p, 0);

        if (p->playing == 0) {
            p->sample = NULL;
            continue;
        }

        p->voice->env_phase = 4;
        p->voice->release_end_frame = D_800B3CC4 + 2;
        n_alSynSetVol(&p->voice->voice, 0, D_800B3650 * 2);
        n_alSynSetPriority(&p->voice->voice, 0);

        p->voice = NULL;
        p->pending = 1;
        p->playing = 0;
        p->sample = NULL;
    }
    osSetIntMask(mask);

    mask = osSetIntMask(1);
    D_800B3CC8 = 0;
    osSetIntMask(mask);
}

extern s32 D_800B3638;
extern channel_t* D_800B3640;

s32 func_800386E4(s32 arg0, s32 arg1) {
    s32 temp_a0;
    s32 var_s1;
    s32 var_s2;
    s32 var_s3;
    channel_t* var_s0;

    if (arg0 == 0) {
        return 0;
    }
    if (arg1 != 0) {
        var_s3 = arg1;
    } else {
        var_s3 = 1;
    }
    var_s0 = D_800B3640;
    var_s1 = 0;
    var_s2 = 0;
    if (D_800B3638 > 0) {
        do {
            temp_a0 = osSetIntMask(1);
            if (arg0 == var_s0->handle) {
                var_s0->duration = arg1;
                var_s0->duration_scale = var_s3;
                var_s2 += 1;
            }
            osSetIntMask(temp_a0);
            var_s1 += 1;
            var_s0++;
        } while (var_s1 < D_800B3638);
    }
    return var_s2;
}

extern s32 D_80075C34;
extern s8 D_8007754C;

/**
 * Sets a new value for every channel and for D_80075C34. A zero `arg0` is replaced by 1.
 * Each part runs with interrupts masked via osSetIntMask(1) (osSetIntMask) and the
 * previous mask restored afterwards: first the byte D_8007754C is cleared; then every
 * entry of the D_800B3640 channel array (D_800B3638 entries of 0x148 bytes) gets the value
 * in offsets 0x38 and 0x3C (the compiled code derives the second copy through a
 * redundant `arg0 != 0 ? arg0 : 1`, kept here so the code matches); finally D_80075C34
 * is set to the value. Compare func_800386E4, which writes the same two fields only on
 * the channels whose handle (offset 0x5C) matches. What the fields mean is not
 * established.
 * Matching note: the value is kept in `arg0` itself; a separate local makes the frame
 * 8 bytes larger.
 * Seen in play (2026-11-13) ran when Start was pressed at the title screen (console reset, then N64
 * logo, opening, title and main menu).
 */
void Audio_FadeCategoriesAndStreams(s32 arg0) {
    s32 mask;
    s32 hi;
    s32 i;
    channel_t* p;

    if (arg0 == 0) {
        arg0 = 1;
    }
    if (arg0 != 0) {
        hi = arg0;
    } else {
        hi = 1;
    }
    mask = osSetIntMask(1);
    D_8007754C = 0;
    osSetIntMask(mask);
    mask = osSetIntMask(1);
    for (i = 0, p = D_800B3640; i < D_800B3638; i++, p++) {
        p->duration = arg0;
        p->duration_scale = hi;
    }
    osSetIntMask(mask);
    mask = osSetIntMask(1);
    D_80075C34 = arg0;
    osSetIntMask(mask);
}

extern s8 D_8007754C;
extern s32 D_800B3CD0;

/**
 * Not reached in play (2026-11-13): did not run in a tour of the main menu, Pokédex, party, Lab,
 * registration, Free Battle setup and a COM battle (opening, a switch, running, a Pound turn).
 * @unverified Name was previously used only by callers of this address; the definition was still func_.
 */
void Audio_FadeCategoriesAndStreamsDefault(void) {
    s32 temp_v0;

    temp_v0 = osSetIntMask(1);
    D_8007754C = 0;
    D_800B3CD0 = 1;
    osSetIntMask(temp_v0);
}

void func_800395D0(channel_t* cp);
void func_80039010(channel_t*);
s32 func_80039328(channel_t*);
f32 func_800393F8(channel_t*, f32);

void func_80039A60(channel_t*);
f32 func_80039B2C(channel_t*);
f32 func_80039B7C(channel_t*);
void func_80039C10(channel_t*);
void func_80039D08(channel_t*);
extern s32 D_80075E00;
void func_80046860(N_ALVoice* v, u8 fxmix); /* n_alSynSetFXMix */

#define CH_FRAME(ch) ch->unk_000
#define CH_VOICE(ch) ch->voice

//@unverified
/**
 * Audio player handler (registered as D_800B3610.handler). For each channel
 * with data it advances the frame counter, reads notes that are due
 * (func_80039010) and runs the timed events, steps the envelope (func_800395D0),
 * and updates the voice's pitch, volume, pan and FX mix.
 */
s32 func_800388BC(void* node) {
    u32 i;
    u32 j;
    channel_t* ch;
    UNUSED s32 pad; // FAKE
    f32 detune;
    f32 pitch;
    s32 vol;
    u32 pan;
    u32 fxmix;
    ALVoiceConfig config;
    u8* pdata; // PORT: was u32

    for (i = 0, ch = D_800B3640; i < D_800B3638; i++, ch++) {
        pdata = ch->pdata;
        if ((pdata == NULL) || (pdata == (u8*)-1)) {
            continue;
        }
        CH_FRAME(ch) = ch->channel_tempo + CH_FRAME(ch);
        if ((ch->note_duration != 0x7FFF) && (D_800B3CC8 == 0)) {
            while ((CH_FRAME(ch) > ch->pitchbend_frame) && (ch->pdata != NULL)) {
                func_80039010(ch);
            }
        }
        if (D_800B3CC8 == 0) {
            if (ch->pvolumebase != NULL) {
                if (CH_FRAME(ch) > ch->channel_frame) {
                    func_80039C10(ch);
                }
            }
            if (ch->ppitchbendbase != NULL) {
                if (CH_FRAME(ch) > ch->volume_frame) {
                    func_80039D08(ch);
                }
            }
        }
        if (ch->duration != -1) {
            ch->duration--;
            if (ch->duration == -1) {
                ch->pdata = func_80036640(ch, 0);
                if (ch->playing != 0) {
                    ch->playing = 0;
                    n_alSynStopVoice(&CH_VOICE(ch)->voice);
                    n_alSynFreeVoice(&CH_VOICE(ch)->voice);
                    CH_VOICE(ch)->env_phase = 5;
                    CH_VOICE(ch) = NULL;
                    ch->IsFX = 0;
                }
                ch->sample = 0;
            }
        }
        if (ch->sample == 0) {
            continue;
        }
        func_800395D0((channel_t*)ch);
        if (ch->sample == 0) {
            continue;
        }
        if (ch->wobble_counter != 0) {
            if (CH_FRAME(ch) > ch->sweep_frame) {
                func_80039A60(ch);
            }
        }
        detune = ch->freqoffset;
        if (ch->vib_speed != 0) {
            detune += func_80039B7C(ch);
        }
        if (ch->unk_0E1 != 0) {
            detune += func_80039B2C(ch);
        }
        pitch = func_800393F8(ch, detune);
        if ((s32)ch->pending != 0) {
            vol = func_80039328(ch);
        }
        if (D_80075E00 == 0) {
            pan = 0x40;
        } else {
            pan = ((ch->sweep_amount * ch->pan) >> 7) & 0x7F;
        }
        fxmix = ch->reverb_intensity;
        fxmix += ((0x80 - fxmix) * ch->reverb) / 128;
        ch->wobble_frame = (u32)(CH_FRAME(ch) - ch->note_end_frame) / 256;
        if (ch->playing != 0) {
            if (pitch != 0.0f) {
                n_alSynSetPitch(&CH_VOICE(ch)->voice, pitch);
            }
            if (((s32)ch->pending != 0) && (vol != ch->volume_current)) {
                ch->volume_current = vol;
                n_alSynSetVol(&CH_VOICE(ch)->voice, vol, D_800B3650);
            }
            if (pan != ch->pan_current) {
                ch->pan_current = pan;
                n_alSynSetPan(&CH_VOICE(ch)->voice, pan);
            }
            if (ch->reverb_mix != ch->reverb) {
                ch->reverb_mix = fxmix;
                func_80046860(&CH_VOICE(ch)->voice, fxmix); /* n_alSynSetFXMix */
            }
        } else {
            for (j = 0; j < 45; j++) {
                if (D_800B3670[j].env_phase == 5) {
                    CH_VOICE(ch) = &D_800B3670[j];
                    goto found;
                }
            }
            for (j = 0; j < 45; j++) {
                if (D_800B3670[j].env_phase == 4) {
                    CH_VOICE(ch) = &D_800B3670[j];
                    goto found;
                }
            }
            ch->sample = 0;
            continue;
        found:
            CH_VOICE(ch)->env_phase = ch->env_state;
            CH_VOICE(ch)->env_release_speed = ch->env_release_speed;
            config.priority = 0x3C;
            config.unityPitch = 0;
            config.fxBus = 0;
            n_alSynAllocVoice(&CH_VOICE(ch)->voice, &config);
            ch->volume_current = vol;
            ch->pan_current = pan;
            ch->reverb_mix = fxmix;
            if (pitch == 0.0f) {
                pitch = Audio_ExpApproximation(ch->pitch_current * (5.0 / 60.0));
                if (pitch > 2.0) {
                    pitch = 2.0f;
                }
            }
            n_alSynStartVoiceParams(&CH_VOICE(ch)->voice, ch->sample, pitch, vol, pan,
                                    fxmix, (s32)ch->pending);
            ch->pending = 0;
            ch->playing = 1;
            CH_VOICE(ch)->env_phase = 1;
        }
    }

    for (i = 0; i < 45; i++) {
        if (D_800B3670[i].env_phase == 4) {
            if (D_800B3670[i].release_end_frame < D_800B3CC4) {
                n_alSynStopVoice(&D_800B3670[i].voice);
                n_alSynFreeVoice(&D_800B3670[i].voice);
                D_800B3670[i].env_phase = 5;
            } else if ((D_80075C34 != 0) && (D_80075C34 < (D_800B3670[i].release_end_frame - D_800B3CC4))) {
                n_alSynSetVol(&D_800B3670[i].voice, 0, D_800B3650 * D_80075C34);
                D_800B3670[i].release_end_frame = D_800B3CC4 + 1 + D_80075C34;
            }
        }
    }

    D_800B3CC4++;
    if (D_80075C34 != 0) {
        D_80075C34--;
    }
    if (D_800B3CD0 != 0) {
        for (i = 0; i < 45; i++) {
            n_alSynStopVoice(&D_800B3670[i].voice);
            n_alSynFreeVoice(&D_800B3670[i].voice);
            D_800B3670[i].env_phase = 5;
        }
        for (i = 0, ch = D_800B3640; i < D_800B3638; i++, ch++) {
            ch->playing = 0;
            ch->pdata = 0;
            func_8003A2B8(ch);
        }
        D_800B3CC8 = 0;
        D_800B3CCC = 0;
        D_80075C34 = 0;
        D_800B3CD0 = 0;
    }
    return D_800B3650;
}

extern command_func_t D_80075C38[];
void Audio_UpdateVoicePlaybackEnd(channel_t* arg0);
void func_80039A44(channel_t* cp);
void func_800395D0(channel_t* cp);
//@unverified
/**
 * Processes the channel's data stream at +0x30: runs command bytes (0x80 and
 * up) through the handler table D_80075C38, then reads the next note and its
 * parameters.
 */
void func_80039010(channel_t* arg0) {

    u8* var_s0;
    u8 temp_a0;
    u32 tmp;
    u8 tmp1;
    ALWaveTable* sample; // PORT: was u32
    u16 wave;
    PortSoundBank* bank; // PORT: was song_t*

    var_s0 = arg0->pdata;
    while (var_s0 != NULL && (tmp1 = *var_s0) >= 0x80) {
        var_s0 = D_80075C38[tmp1 & 0x7F](arg0, var_s0 + 1);
    }
    arg0->pdata = var_s0;

    if (arg0->pdata != NULL) {
        arg0->last_note = arg0->port_base;
        tmp = *arg0->pdata++;

        if (arg0->wobble_on_speed != 0) {
            arg0->velocity = *arg0->pdata++;
        } else {
            arg0->velocity = arg0->wobble_off_speed;
        }

        if (arg0->fixed_length != 0) {
            if (arg0->ignore == 0) {
                arg0->note_duration = arg0->fixed_length;
            } else {
                arg0->ignore = 0;
                temp_a0 = *arg0->pdata++;
                if (temp_a0 < 0x80) {
                    arg0->note_duration = (s32)temp_a0; // FAKE: no-op cast; the plain assignment changes register allocation
                } else {
                    arg0->note_duration = *arg0->pdata++ + ((temp_a0 & 0x7F) << 8);
                }
            }
        } else {
            temp_a0 = *arg0->pdata++;
            if (temp_a0 < 0x80) {
                arg0->note_duration = temp_a0;
            } else {
                arg0->note_duration = *arg0->pdata++ + ((temp_a0 & 0x7F) << 8);
            }
        }

        arg0->note_end_frame = arg0->pitchbend_frame;
        arg0->pitchbend_frame = (arg0->note_duration << 8) + arg0->pitchbend_frame;

        arg0->wobble_frame = 0;
        arg0->unk_0E4 = 0;
        arg0->unk_0E3 = arg0->unk_0E2;

        if (tmp != 0x60) {
            bank = PortA_Bank(arg0->sample_bank); // PORT: host view of the bank (audio_port.c)
            if (arg0->pdrums != NULL) {
                arg0->wave = arg0->pdrums[tmp * 4];
                arg0->pan = arg0->pdrums[tmp * 4 + 2] / 2;
                Audio_SetVoiceVolumeEnvelope(arg0, PORT_BLOB_AT(arg0->song_addr, arg0->song_addr->env_table) +
                                                       arg0->pdrums[tmp * 4 + 1] * sizeof(envelope_t)); // PORT
                tmp = arg0->pdrums[tmp * 4 + 3];
            }
            Audio_UpdateVoicePlaybackEnd(arg0);

            if (arg0->wobble_counter != 0) {
                func_80039A44(arg0);
            }

            if (arg0->playing != 0) {
                arg0->sample = NULL;
                func_800395D0(arg0);
            }

            wave = arg0->wave;
            sample = bank->wave_list[wave];
            arg0->sample = sample;
            arg0->base_note = bank->detune[wave] + tmp;
        } else {
            arg0->sample = 0;
            if (arg0->playing != 0) {
                arg0->note_start_frame = (s64)arg0->unk_000;
                func_800395D0(arg0);
            }
            arg0->playing = 0;
        }
    } else {
        arg0->sample = 0;
        if (arg0->playing != 0) {
            arg0->note_start_frame = (s64)arg0->unk_000;
            func_800395D0(arg0);
        }
    }
}

s32 Audio_ComputeVoiceVolume(channel_t* cp) {
    u32 volume = cp->volume * cp->env_current_vol * cp->velocity * cp->volume_scale;

    volume >>= 0xD;
    if (volume >= 0x8000) {
        volume = 0x7FFF;
    }

    if (cp->IsFX == 0) {
        volume *= mus_master_volume_songs;
    } else {
        volume *= mus_master_volume_effects;
    }

    volume >>= 0xF;

    if (cp->duration != -1) {
        volume = (cp->duration * volume) / cp->duration_scale;
    }

    return volume;
}

f32 Audio_AdvanceVoicePitch(channel_t* arg0, f32 offset) {
    f32 temp_fv1;
    f32 var_fv1;
    f32 var_fv1_2;
    s32 temp_lo;

    temp_fv1 = arg0->base_note;
    if (arg0->port != 0) {
        if (arg0->port >= arg0->wobble_frame) {
            var_fv1_2 = ((temp_fv1 - arg0->last_note) / arg0->port);
            var_fv1_2 *= arg0->wobble_frame;
            temp_fv1 = arg0->last_note + var_fv1_2;
        }
        arg0->port_base = temp_fv1;
    }

    temp_lo = arg0->transpose * (1 - arg0->ignore_transpose);

    arg0->ignore_transpose = 0;

    temp_fv1 += (offset + arg0->pitchbend_calc + temp_lo);
    if (temp_fv1 == arg0->pitch_current) {
        return 0.0f;
    }
    arg0->pitch_current = temp_fv1;

    temp_fv1 = Audio_ExpApproximation(temp_fv1 * 0.0833333333333333287);
    if (temp_fv1 > 2.0) {
        temp_fv1 = 2.0f;
        arg0->velocity = 0;
    }

    return temp_fv1;
}

void Audio_UpdateVoicePlaybackEnd(channel_t* arg0) {
    if (arg0->note_duration != 0x7FFF) {
        if (arg0->cutoff != 0) {
            arg0->note_start_frame = (s32)(arg0->cutoff << 8) + (s64)arg0->note_end_frame;
        } else {
            arg0->note_start_frame = (s64)arg0->pitchbend_frame - (s32)(arg0->endit << 8);
        }
    } else {
        arg0->note_start_frame = 0x7FFFFFFF;
    }

    arg0->env_current_vol = arg0->env_init_vol;
    arg0->env_decay_begin = arg0->env_speed;
    arg0->env_state = 1;
}

void func_8003A800(void*, s32);
//@unverified
/**
 * Steps the envelope state (byte 0x1E) of the channel's voice at +0x144. When
 * the note's end time is reached (and the state is below 4) or the channel has
 * stopped, it moves to state 4, sets the release volume with n_alSynSetVol and
 * detaches the voice. Otherwise it advances through the earlier states using
 * the times at +0xD4 and +0xD5 and the volume byte at +0xD2.
 */
void func_800395D0(channel_t* arg0) {
    u32 temp_ft4;
    u32 temp_a0;

    if (arg0->voice != NULL) {
        if (((arg0->unk_000 >= arg0->note_start_frame) && (arg0->voice->env_phase < 4)) ||
            (arg0->sample == NULL)) {
            arg0->voice->env_phase = 4;
            temp_ft4 = (f32)arg0->voice->env_release_speed / ((f32)arg0->channel_tempo * 0.00390625);
            if (temp_ft4 == 0) {
                temp_ft4++;
            }
            arg0->voice->release_end_frame = (u32)D_800B3CC4 + temp_ft4 + 2;
            n_alSynSetVol(&arg0->voice->voice, 0, (u32)D_800B3650 * temp_ft4);
            func_8003A800(arg0->voice, 0);
            arg0->voice = NULL;
            arg0->pending = 1;
            arg0->playing = 0;
            arg0->sample = NULL;
        } else {
            switch (arg0->voice->env_phase) {
                case 0:
                case 1:
                    temp_a0 = (u32)(arg0->unk_000 - arg0->note_end_frame) >> 8;
                    if (temp_a0 >= arg0->env_attack_speed) {
                        arg0->voice->env_phase++;
                        arg0->env_current_vol = arg0->env_max_vol;
                    }
                    break;

                case 2:
                    arg0->pending = 1;
                    temp_a0 = ((u32)(arg0->unk_000 - arg0->note_end_frame) >> 8) - arg0->env_attack_speed;
                    if (temp_a0 < arg0->env_decay_speed) {
                        arg0->env_current_vol = (u32)(arg0->env_decay_calc * temp_a0) + arg0->env_max_vol;
                    } else {
                        arg0->voice->env_phase++;
                        arg0->env_current_vol = arg0->env_sustain_vol;
                    }
                    break;

                case 3:
                    break;
            }
        }
    } else {
        u32 attack = arg0->env_attack_speed;
        arg0->pending = (f32)attack / ((f32)arg0->channel_tempo * 0.00390625) *
                                      (f32)D_800B3650;
        arg0->env_current_vol = arg0->env_max_vol;
        temp_a0 = (u32)(arg0->unk_000 - arg0->note_end_frame) >> 8;
        if (temp_a0 < attack) {
            arg0->env_state = 1;
        } else {
            arg0->env_state = 2;
        }
    }
}

void func_80039A44(channel_t* cp) {
    cp->sweep_frame = cp->note_end_frame;
    cp->wobble_value = 0;
    cp->env_trigger_off = cp->sweep_amount & 0x80;
}

void Audio_AdvancePanModulation(channel_t* arg0) {
    u32 temp_v1;
    u32 var_v0;

    do {
        arg0->sweep_frame += 0x100;

        temp_v1 = (arg0->wobble_counter * 2) + arg0->wobble_value;
        if (temp_v1 < 0x80) {
            arg0->wobble_value = temp_v1;
            continue;
        }

        arg0->wobble_value = temp_v1 & 0x7F;
        temp_v1 >>= 7;
        if (arg0->env_trigger_off == 0) {
            var_v0 = arg0->sweep_amount;
            var_v0 += temp_v1;
            if (var_v0 >= 0xFF) {
                var_v0 = 0xFE;
                arg0->env_trigger_off = 1;
            }
            arg0->sweep_amount = var_v0;
        } else {
            var_v0 = arg0->sweep_amount;
            var_v0 -= temp_v1;
            if ((var_v0 >= 0xFF) || (var_v0 == 0)) {
                var_v0 = 1;
                arg0->env_trigger_off = 0;
            }
            arg0->sweep_amount = var_v0;
        }
    } while (arg0->sweep_frame < arg0->unk_000);

    arg0->pan_current = 0xFF;
}


f32 Audio_AdvancePeriodicPitchModulation(channel_t* arg0) {
    arg0->unk_0E3--;
    if (!arg0->unk_0E3) {
        if (arg0->unk_0E4 == 0) {
            arg0->unk_0E4 = (s8)arg0->sweep_speed;
            arg0->unk_0E3 = arg0->unk_0E1;
        } else {
            arg0->unk_0E4 = 0;
            arg0->unk_0E3 = arg0->unk_0E2;
        }
    }

    return arg0->unk_0E4;
}


f32 Audio_ComputeSinePitchModulation(channel_t* arg0) {
    s32 temp_v0 = arg0->wobble_frame - arg0->vib_delay;
    f32 tmp;

    if (temp_v0 > 0) {
        tmp = arg0->vib_amount * __sinf(((temp_v0 / (f32)(u32)arg0->vib_speed) * 2) * 3.14159260000000007);
        arg0->vibrato = tmp;
    }
    return arg0->vibrato;
}

void Audio_AdvancePitchEnvelope(channel_t* arg0) {
    s32 tmp;

    do {
        arg0->channel_frame += 0x100;

        arg0->cont_vol_repeat_count--;
        if (arg0->cont_vol_repeat_count) {
            continue;
        }

        tmp = *arg0->pvolumebase++;
        if (tmp >= 0x80u) {
            arg0->volume = tmp & 0x7F;

            tmp = *arg0->pvolumebase++;
            if (tmp >= 0x80u) {
                arg0->cont_vol_repeat_count = (tmp & 0x7F) << 8;
                arg0->cont_vol_repeat_count += *arg0->pvolumebase++ + 2;
            } else {
                arg0->cont_vol_repeat_count = tmp + 2;
            }
        } else {
            arg0->volume = tmp;
            arg0->cont_vol_repeat_count = 1;
        }
    } while (arg0->channel_frame < arg0->unk_000);
}

void Audio_AdvanceVolumeEnvelope(channel_t* arg0) {
    u8 tmp;

    do {
        arg0->volume_frame += 0x100;

        arg0->cont_pb_repeat_count--;
        if (arg0->cont_pb_repeat_count) {
            continue;
        }

        tmp = *arg0->ppitchbendbase++;
        if (tmp >= 0x80) {
            arg0->pitchbend = (f32)(tmp & 0x7F) - 64.0;
            arg0->pitchbend_calc = arg0->pitchbend * arg0->bendrange;
            tmp = *arg0->ppitchbendbase++;
            if (tmp >= 0x80) {
                arg0->cont_pb_repeat_count = (tmp & 0x7F) << 8;
                arg0->cont_pb_repeat_count += *arg0->ppitchbendbase++ + 2;
            } else {
                arg0->cont_pb_repeat_count = tmp + 2;
            }
        } else {
            arg0->pitchbend = (f32)(tmp)-64.0;
            arg0->pitchbend_calc = arg0->pitchbend * arg0->bendrange;
            arg0->cont_pb_repeat_count = 1;
        }
    } while (arg0->volume_frame < arg0->unk_000);
}

f32 Audio_ExpApproximation(f32 arg0) {
    f32 temp_fa1;
    f32 temp_fv1;

    if (arg0 == 0.0f) {
        return 1.0f;
    }

    if (arg0 > 0.0f) {
        temp_fv1 = SQ(arg0);
        temp_fa1 = SQ(temp_fv1);
        return (arg0 * 0.693147180559944953) + 1.0 + (temp_fv1 * 0.240226506959100999) +
               ((temp_fv1 * arg0) * 0.055504108664821597) + (temp_fa1 * 0.00961812910762848035) +
               ((temp_fa1 * arg0) * 0.00133335581464283999) + ((temp_fa1 * temp_fv1) * 0.000154035303933816006);
    } else {
        arg0 = -arg0;
        temp_fv1 = SQ(arg0);
        temp_fa1 = SQ(temp_fv1);
        return 1.0 / ((arg0 * 0.693147180559944953) + 1.0 + (temp_fv1 * 0.240226506959100999) +
                      ((temp_fv1 * arg0) * 0.055504108664821597) + (temp_fa1 * 0.00961812910762848035) +
                      ((temp_fa1 * arg0) * 0.00133335581464283999) + ((temp_fa1 * temp_fv1) * 0.000154035303933816006));
    }
}

void Audio_RelocateSoundBank(u8* arg0, uintptr_t arg1) { // PORT: arg1 (wave data ROM address) was s32
    UNUSED s32 pad; // FAKE: stack padding
    u32 i;
    f32 var_fv0;
    u8 temp_v1;
    // PORT: builds the host wave tables (audio_port.c) instead of relocating the blob's pointer fields in place;
    // NULL when the bank was already relocated
    PortSoundBank* soundbank = PortA_RelocateSoundBank(arg0, arg1);

    if (soundbank == NULL) {
        return;
    }

    for (i = 0; i < soundbank->count; i++) {
        f32* ptr = &soundbank->detune[i];

        temp_v1 = *(u8*)ptr;
        if (temp_v1 & 0x80) {
            var_fv0 = -(0x100 - temp_v1);
        } else {
            var_fv0 = (s32)temp_v1;
        }
        *ptr = var_fv0 / 100.0;

        temp_v1 = soundbank->basenote[i] - 0x30;
        if (temp_v1 & 0x80) {
            var_fv0 = -(0x100 - temp_v1);
        } else {
            var_fv0 = (s32)temp_v1;
        }
        *ptr += var_fv0;
    }

    osWritebackDCacheAll();
}

s32 Audio_RandomScale(s32 arg0) {
    s32 i;
    s32 tmp;
    f32 tmpf;

    for (i = 0; i < 8; i++) {
        tmp = mus_random_seed & 0x48000000;
        mus_random_seed <<= 1;
        if ((tmp == 0x48000000) || (tmp == 0x08000000)) {
            mus_random_seed |= 1;
        }
    }

    tmpf = mus_random_seed / 65536.0f;
    tmpf /= 65536.0f;
    return arg0 * tmpf;
}

void func_8003A2B8(channel_t* cp) {
    u8* ptr;
    s32 i;
    s32 tempo;

    cp->sample = NULL;
    cp->voice = NULL;
    cp->unk_000 = 0;
    cp->channel_frame = 0;
    cp->volume_frame = 0;
    cp->pitchbend_frame = 0;
    cp->note_end_frame = 0;
    cp->note_start_frame = 0;

    // PORT: from `sample` to the end of the host struct (was offsets 0x34 to 0x148)
    ptr = (u8*)&cp->sample;
    for (i = 0; i < (s32)(sizeof(channel_t) - offsetof(channel_t, sample)); i++) {
        *ptr++ = 0;
    }

    cp->volume_current = 0xFFFF;
    cp->reverb_mix = 0xFF;
    cp->pan_current = 0xFF;
    cp->pitch_current = 99.9f;
    tempo = 0x6000 / (s32)D_800B364C;
    cp->channel_tempo_save = tempo;
    cp->channel_tempo = tempo;
    cp->pitchbend_frame = cp->unk_000 = 0;
    cp->note_duration = 1;
    cp->wobble_on_speed = 0;
    cp->wobble_off_speed = 0x7F;
    cp->fixed_length = 0;
    cp->ignore = 0;
    cp->volume = 0x7F;
    cp->pan = 0x40;
    cp->cont_vol_repeat_count = 1;
    cp->cont_pb_repeat_count = 1;
    cp->duration = -1;
    cp->wobble_counter = 0;
    cp->bendrange = 0.03125f;
    func_80036640(cp, 0);
    cp->volume_scale = 0x80;
    cp->sweep_amount = 0x80;
    cp->temscale = 0x80;
    cp->env_speed = 1;
    cp->env_attack_speed = 1;
    cp->env_max_vol = 0x7F;
    cp->env_decay_speed = 0xFF;
    cp->env_attack_calc = 1.0f;
    cp->env_decay_calc = 0.003921568859f;
    cp->pitchbend_calc = 0.0f;
    cp->freqoffset = 0.0f;
    cp->port_base = 0.0f;
    cp->distort = 0.0f;
    cp->env_sustain_vol = 0x7F;
    cp->env_release_speed = 0xF;
    cp->pending = 1;
    cp->vib_speed = 0;
    cp->unk_0E1 = 0;
    cp->IsFX = 0;
    cp->playing = 0;
}

extern channel_t* D_800B3640;

u32 func_8003A4A8(UNUSED song_t* arg0, UNUSED s32 arg1) { // PORT: arg0 was s32
    s32 temp_a0;
    s32 var_s2;
    u32 var_s1;
    channel_t* var_s0;

    var_s2 = 0;
    var_s0 = D_800B3640;
    var_s1 = 0;
    if (D_800B3638 != 0) {
loop_2:
        temp_a0 = osSetIntMask(1);
        if (var_s0->pdata == NULL) {
            var_s0->pdata = (u8*)-1;
            var_s2 = 1;
        }
        osSetIntMask(temp_a0);
        if (var_s2 != 0) {
            return var_s1;
        }
        var_s1 += 1;
        var_s0++;
        if (var_s1 >= (u32) D_800B3638) {
            goto block_7;
        }
        goto loop_2;
    }
block_7:
    return -1U;
}

void Audio_FillBytes(u8* arg0, u8 arg1, u32 arg2) {
    u8* a0 = arg0;

    while (arg2--) {
        *a0++ = arg1;
    }
}

// PORT: no longer called on the host (songs and sound banks keep their offsets); u32 entries cannot hold host addresses
void Audio_RelocatePointerArray(u8* arg0, u32 arg1, u32 arg2) {
    u32* ptr = (u32*)arg0;
    u32 i;

    for (i = 0; i < arg2; i++) {
        if (ptr[i] != 0) {
            ptr[i] += arg1;
        }
    }
}

/**
 * Initializes channel `cp` as a new sound (a sibling of func_8003815C): calls
 * func_8003A2B8(cp) to reset it, then stores D_800B3668 at offset 0x88, arg1 at the
 * s16 at 0xB2 (the type word), arg2 at 0xAA, arg3 at 0xBC and arg4 at 0x60 (`priority`).
 * It then raises the interrupt mask with osSetIntMask, takes the next handle
 * D_800B365C++ for offset 0x5C, loads ((s32*)D_800B3644)[arg1] into offsets 0x8C and 0x30,
 * restores the mask and returns the handle. The argument of the first osSetIntMask call is
 * an uninitialised stack local in the retail code (`lw $a0, 0x20($sp)`, not the constant
 * 1 that func_8003815C passes); `uninit` reproduces that and must not be given a value.
 * What the table at D_800B3644 holds is not established.
 */
s32 func_8003A638(channel_t* cp, s32 arg1, s32 arg2, s32 arg3, s32 arg4) {
    s32 val;
    s32 uninit;
    s32 mask;

    func_8003A2B8(cp);
    cp->sample_bank = D_800B3668;
    cp->IsFX = arg1;
    cp->volume_scale = arg2;
    cp->sweep_amount = arg3;
    cp->priority = arg4;
    mask = osSetIntMask(uninit);
    cp->handle = D_800B365C;
    D_800B365C++;
    // PORT: D_800B3644 holds big-endian offsets from sPortFxBlock (see AudioFxBlock)
    cp->pbase = cp->pdata = PORT_BLOB_AT(sPortFxBlock, Port_ReadBE32(&D_800B3644[arg1]));
    osSetIntMask(mask);
    return cp->handle;
}
