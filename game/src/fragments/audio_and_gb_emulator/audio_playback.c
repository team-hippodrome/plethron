/* Stadium Zero fragment000_80C00. */
/* Audio driver, matching OoT src/audio/internal/playback.c in function order. The driver is
 * Nintendo's "Nas" library (original names Nas_*); this revision sits between SM64 Shindou and
 * OoT. See docs/annotation/audio-driver.md for the per-function mapping. */

#include "global.h"
#include "audio_and_gb_emulator.h"
#include "m2c_macros.h"

/* The attributes AudioPlayback_InitSampleState is given (built on the stack from a note's attributes). */
typedef struct NoteSubAttributes {
    /* 0x00 */ u8 reverbVol; // @unverified: same offset/use as OoT's NoteSubAttributes.reverbVol
    /* 0x01 */ u8 gain; // @unverified: same offset/use as OoT's NoteSampleStateAttributes.gain (from channel->gain)
    /* 0x02 */ u8 pan; // @unverified: same offset/use as OoT's NoteSubAttributes.pan
    /* 0x03 */ Stereo reverbBits; // @unverified: same offset/use as OoT's NoteSubAttributes.reverbBits
    /* 0x04 */ f32 freqScale; // @unverified: same offset/use as OoT's NoteSubAttributes.frequency
    /* 0x08 */ f32 velocity; // @unverified: same offset/use as OoT's NoteSubAttributes.velocity
    /* 0x0C */ s32 unk0C;
    /* 0x10 */ s16* filter; // PORT: was s32, holds a pointer. @unverified: takes channel->filter / NoteAttributes.filter, like OoT's NoteSampleStateAttributes.filter
} NoteSubAttributes;
void AudioPlayback_SeqLayerDecayRelease(struct SequenceLayer*, s32);

void AudioPlayback_NoteSetResamplingRate(s8* arg0, f32 arg1);

extern s8 D_811858EE;
extern u16 D_811369D8[];
extern f32 D_81136AD8[];
extern f32 D_81136CD8[];
extern f32 D_81136ED8[];

/**
 * Sets a note's left and right volume, pan and reverb from its attributes.
 * Corresponds to OoT Audio_InitSampleState (original name Nas_smzSetParam).
 * No caller in play (2026-11-13): nothing that runs reaches it, not even through callbacks, data
 * tables or fragment headers.
 * @unverified Name and comment guessed from the code.
 */
void AudioPlayback_InitSampleState(s8* note, NoteSubAttributes* reverb_info) {
    NoteSampleState* sub;
    f32 vol_right;
    f32 vol_left;
    s32 small_pan_index;
    UNUSED u32 pad; // FAKE: stack padding
    UNUSED u32 pad1; // FAKE: stack padding
    u8 strong_right;
    u8 strong_left;
    f32 velocity;
    u8 pan;
    u8 reverb_vol;
    Stereo reverb_bits;

    AudioPlayback_NoteSetResamplingRate(note, reverb_info->freqScale);
    velocity = reverb_info->velocity;
    pan = reverb_info->pan;
    reverb_vol = reverb_info->reverbVol;
    reverb_bits = reverb_info->reverbBits;
    sub = (NoteSampleState*)(note + 0xB4);
    pan &= 0x7F;

    if (sub->stereoHeadsetEffects && D_811858EE == 1) {
        small_pan_index = pan >> 1;
        if (small_pan_index > 0x3F) {
            small_pan_index = 0x3F;
        }
        sub->headsetPanLeft = D_811369D8[small_pan_index];
        sub->headsetPanRight = D_811369D8[0x3F - small_pan_index];
        sub->stereoStrongRight = 0;
        sub->stereoStrongLeft = 0;
        sub->usesHeadsetPanEffects = 1;
        vol_left = D_81136AD8[pan];
        vol_right = D_81136AD8[0x7F - pan];
    } else if (sub->stereoHeadsetEffects && D_811858EE == 0) {
        strong_left = strong_right = 0;
        sub->headsetPanRight = 0;
        sub->headsetPanLeft = 0;
        sub->usesHeadsetPanEffects = 0;
        vol_left = D_81136CD8[pan];
        vol_right = D_81136CD8[0x7F - pan];
        if (pan < 0x20) {
            strong_left = 1;
        } else if (pan > 0x60) {
            strong_right = 1;
        }
        sub->stereoStrongRight = strong_right;
        sub->stereoStrongLeft = strong_left;
        switch (reverb_bits.s.stereoHeadsetEffects) {
            case 0:
                sub->stereoStrongRight = reverb_bits.s.strongRight;
                sub->stereoStrongLeft = reverb_bits.s.strongLeft;
                break;
            case 1:
                break;
            case 2:
                sub->stereoStrongRight = reverb_bits.s.strongRight | strong_right;
                sub->stereoStrongLeft = reverb_bits.s.strongLeft | strong_left;
                break;
            case 3:
                sub->stereoStrongRight = reverb_bits.s.strongRight ^ strong_right;
                sub->stereoStrongLeft = reverb_bits.s.strongLeft ^ strong_left;
                break;
        }
    } else if (D_811858EE == 3) {
        vol_left = 0.707f;
        vol_right = 0.707f;
    } else {
        vol_left = D_81136ED8[pan];
        vol_right = D_81136ED8[0x7F - pan];
    }

    if (velocity < 0.0f) {
        velocity = 0.0f;
    }
    if (velocity > 1.0f) {
        velocity = 1.0f;
    }

    sub->targetVolLeft = (s32)(velocity * vol_left * 4095.999f);
    sub->targetVolRight = (s32)(velocity * vol_right * 4095.999f);
    sub->gain = reverb_info->gain;
    sub->filter = reverb_info->filter;

    if (sub->reverbVol != reverb_vol) {
        sub->reverbVol = reverb_vol;
        sub->envMixerNeedsInit = 1;
    } else if (sub->needsInit) {
        sub->envMixerNeedsInit = 1;
    } else {
        sub->envMixerNeedsInit = 0;
    }
}

/* Byte 1 of the note's sample state (OoT NoteSampleState.bitField1). */
typedef struct SampleStateFlags811105DC {
    /* 0x0 */ u8 unk0;
    /* 0x1 */ u8 pad : 7;
    /* 0x1 */ u8 hasTwoParts : 1;
} SampleStateFlags811105DC;

/**
 * Sets a note's fixed-point resampling rate. Rates of 2.0 or more are halved and flagged
 * hasTwoParts, so the synthesizer resamples in two halves.
 * Corresponds to OoT Audio_NoteSetResamplingRate (original name Nas_smzSetPitch). Same 2.0f split,
 * 1.99998f and 3.99996f clamps, halving and 32768.0f fixed-point scale.
 * No caller in play (2026-11-13): nothing that runs reaches it, not even through callbacks, data
 * tables or fragment headers.
 * @unverified Name and comment guessed from the code.
 */
void AudioPlayback_NoteSetResamplingRate(s8* arg0, f32 arg1) {
    SampleStateFlags811105DC* flags = (SampleStateFlags811105DC*)(arg0 + 0xB4);
    f32 resampling_rate;

    if (arg1 < 2.0f) {
        flags->hasTwoParts = 0;
        if (1.99998f < arg1) {
            resampling_rate = 1.99998f;
        } else {
            resampling_rate = arg1;
        }
    } else {
        flags->hasTwoParts = 1;
        if (3.99996f < arg1) {
            resampling_rate = 1.99998f;
        } else {
            resampling_rate = arg1 * 0.5f;
        }
    }
    M2C_FIELD(arg0, s16*, 0xC0) = resampling_rate * 32768.0f;
}

extern NoteSampleState D_811369C0;
void AudioEffects_InitAdsr(AdsrState*, EnvelopePoint*, s16*); // PORT: matches the definition

/**
 * Starts a note's envelope from its instrument or the layer's own envelope.
 * Corresponds to OoT Audio_NoteInit (original name Nas_StartVoice).
 * No caller in play (2026-11-13): nothing that runs reaches it, not even through callbacks, data
 * tables or fragment headers.
 * @unverified Name and comment guessed from the code.
 */
void AudioPlayback_NoteInit(Note* arg0) {
    SequenceLayer* def = arg0->playbackState.parentLayer;

    if (def->adsr.decayIndex == 0) {
        AudioEffects_InitAdsr(&arg0->playbackState.adsr, def->channel->adsr.envelope, &arg0->playbackState.adsrVolScale);
    } else {
        AudioEffects_InitAdsr(&arg0->playbackState.adsr, def->adsr.envelope, &arg0->playbackState.adsrVolScale);
    }
    arg0->playbackState.unk_04 = 0;
    arg0->playbackState.adsr.state = 1;
    arg0->sampleState = D_811369C0;
}

/**
 * Turns a note off and clears its playback state.
 * Corresponds to OoT Audio_NoteDisable (original name Nas_StopVoice).
 * No caller in play (2026-11-13): nothing that runs reaches it, not even through callbacks, data
 * tables or fragment headers.
 * @unverified Name and comment guessed from the code.
 */
void AudioPlayback_NoteDisable(Note* note) {
    if (note->sampleState.needsInit == 1) {
        note->sampleState.needsInit = 0;
    }
    note->playbackState.priority = 0;
    note->playbackState.unk_04 = 0;
    note->sampleState.enabled = 0;
    note->playbackState.parentLayer = (void*)-1;
    note->playbackState.prevParentLayer = (void*)-1;
    note->sampleState.finished = 0;
    note->playbackState.adsr.state = 0;
    note->playbackState.adsr.current = 0.0f;
}

extern struct Note* D_81184664;
extern s32 D_811858E8;
extern f32 D_811858D0;
extern f32 D_811858D4;
void AudioPlayback_SeqLayerNoteRelease(Particle* arg0);
void AudioPlayback_AudioListPushFront(AudioListItem* arg0, AudioListItem* arg1);
void AudioPlayback_NoteInitForLayer(Note* arg0, SequenceLayer* arg1);
void AudioEffects_InitVibrato(Note*);
f32 AudioEffects_UpdateAdsr(u8*);
void AudioEffects_UpdatePortamentoAndVibrato(Note*);

/**
 * Updates every note once per audio frame: envelopes, releases, pitch, volume and pan.
 * Corresponds to OoT Audio_ProcessNotes (original name Nas_UpdateChannel).
 * No caller in play (2026-11-13): nothing that runs reaches it, not even through callbacks, data
 * tables or fragment headers.
 * @unverified Name and comment guessed from the code.
 */
void AudioPlayback_ProcessNotes(void)
{
  Note *note;
  NotePlaybackState *playback_state;
  NoteSampleState *note_sub_eu;
  NoteAttributes *attrs;
  s32 pad[2]; // FAKE
  NoteSubAttributes sub_attrs;
  u8 book_offset;
  f32 scale;
  s32 i;
  for (i = 0; i < D_811858E8; i++)
  {
    note = i + (Note*)D_81184664; // FAKE: index first, for retail's addu operand order
    playback_state = &note->playbackState;
    if (playback_state->parentLayer != ((SequenceLayer *) (-1)))
    {
      if (((uintptr_t) playback_state->parentLayer) < 0x7FFFFFFF)
      {
        continue;
      }
      if ((note != playback_state->parentLayer->note) && (playback_state->unk_04 == 0))
      {
        playback_state->adsr.action |= 0x10;
        playback_state->adsr.fadeOutVel = D_811858D4;
        playback_state->priority = 1;
        playback_state->unk_04 = 2;
        goto d;
      }
      else
        if (((!playback_state->parentLayer->enabled) && (playback_state->unk_04 == 0)) && (playback_state->priority >= 1)) // FAKE: no-op ^ 0
      {
      }
      else
        if (playback_state->parentLayer->channel->seqPlayer == 0)
      {
        AudioSeq_SequenceChannelDisable(playback_state->parentLayer->channel);
        playback_state->priority = 1;
        playback_state->unk_04 = 1;
        continue;
      }
      else
        if (playback_state->parentLayer->channel->seqPlayer->muted && (playback_state->parentLayer->channel->muteBehavior & 0x40))
      {
      }
      else
      {
        goto d;
      }
      AudioPlayback_SeqLayerNoteRelease((Particle *) playback_state->parentLayer);
      AudioPlayback_AudioListRemove(&note->listItem);
      AudioPlayback_AudioListPushFront(&note->listItem.pool->decaying, &note->listItem);
      playback_state->priority = 1;
      playback_state->unk_04 = 2;
    }
    else
      if ((playback_state->unk_04 == 0) && (playback_state->priority >= 1))
    {
      continue;
    }
    d:
    if (playback_state->priority != 0)
    {
      if (1) // FAKE
      {
        note_sub_eu = &note->sampleState;
      }
      if ((playback_state->unk_04 >= 1) || note_sub_eu->finished)
      {
        if ((playback_state->adsr.state == 0) || note_sub_eu->finished)
        {
          if (playback_state->wantedParentLayer != ((SequenceLayer *) (-1)))
          {
            AudioPlayback_NoteDisable((Note *) note);
            if (playback_state->wantedParentLayer->channel != 0)
            {
              AudioPlayback_NoteInitForLayer(note, playback_state->wantedParentLayer);
              AudioEffects_InitVibrato(note);
              AudioPlayback_AudioListRemove(&note->listItem);
              AudioSeq_AudioListPushBack(&note->listItem.pool->active, &note->listItem);
              playback_state->wantedParentLayer = (SequenceLayer *) (-1);
              goto process;
            }
            AudioPlayback_NoteDisable((Note *) note);
            AudioPlayback_AudioListRemove(&note->listItem);
            AudioSeq_AudioListPushBack(&note->listItem.pool->disabled, &note->listItem);
            playback_state->wantedParentLayer = (SequenceLayer *) (-1);
          }
          else
          {
            if (playback_state->parentLayer != ((SequenceLayer *) (-1)))
            {
              playback_state->parentLayer->bit1 = 1;
            }
            AudioPlayback_NoteDisable((Note *) note);
            AudioPlayback_AudioListRemove(&note->listItem);
            AudioSeq_AudioListPushBack(&note->listItem.pool->disabled, &note->listItem);
          }
        }
        else
        {
          goto process;
        }
      }
      else
        if (playback_state->adsr.state == 0)
      {
        if (playback_state->parentLayer != ((SequenceLayer *) (-1)))
        {
          playback_state->parentLayer->bit1 = 1;
        }
        AudioPlayback_NoteDisable((Note *) note);
        AudioPlayback_AudioListRemove(&note->listItem);
        AudioSeq_AudioListPushBack(&note->listItem.pool->disabled, &note->listItem);
      }
      else
      {
        process:
        attrs = &playback_state->attributes;

        scale = AudioEffects_UpdateAdsr((u8 *) (&playback_state->adsr));
        AudioEffects_UpdatePortamentoAndVibrato(note);
        if ((playback_state->unk_04 == 1) || (playback_state->unk_04 == 2))
        {
          sub_attrs.freqScale = attrs->freqScale;
          sub_attrs.velocity = attrs->velocity;
          sub_attrs.pan = attrs->pan;
          sub_attrs.reverbVol = attrs->reverbVol;
          sub_attrs.reverbBits = attrs->stereo;
          sub_attrs.gain = attrs->gain;
          sub_attrs.filter = attrs->filter;
          book_offset = note_sub_eu->bookOffset;
        }
        else
        {
          SequenceChannel *channel = playback_state->parentLayer->channel;
          SequenceLayer *layer = playback_state->parentLayer;
          sub_attrs.freqScale = layer->noteFreqScale;
          sub_attrs.velocity = layer->noteVelocity;
          sub_attrs.pan = layer->notePan;
          sub_attrs.reverbBits = layer->stereo;
          sub_attrs.reverbVol = channel->reverb;
          sub_attrs.gain = channel->gain;
          sub_attrs.filter = channel->filter;
          book_offset = channel->bookOffset & 7;
          if (channel->seqPlayer->muted && (channel->muteBehavior & 8))
          {
            sub_attrs.freqScale = 0.0f;
            sub_attrs.velocity = 0.0f;
          }
        }
        sub_attrs.freqScale *= playback_state->vibratoFreqScale * playback_state->portamentoFreqScale;
        sub_attrs.freqScale *= D_811858D0;
        sub_attrs.velocity *= scale;
        AudioPlayback_InitSampleState((s8 *) note, &sub_attrs);
        note_sub_eu->bookOffset = book_offset;
      }
    }

  }

}

/**
 * Returns the low, normal or high sample of instrument `arg0` for semitone `arg1`.
 * Corresponds to OoT Audio_GetInstrumentTunedSample (original name NoteToVoice).
 * No caller in play (2026-11-13): nothing that runs reaches it, not even through callbacks, data
 * tables or fragment headers.
 * @unverified Name and comment guessed from the code.
 */
u8* AudioPlayback_GetInstrumentTunedSample(u8* arg0, s32 arg1) {
    u8* ret;

    if (arg1 < arg0[1]) {
        ret = arg0 + 8;
    } else if (arg0[2] >= arg1) {
        ret = arg0 + 0x10;
    } else {
        ret = arg0 + 0x18;
    }
    return ret;
}


s32 AudioLoad_IsFontLoadComplete(s32 arg0);
extern SoundFont* D_811858B0;
extern s32 D_811859D0;

/**
 * Returns instrument `arg1` of font `arg0`, or NULL when the font is not loaded or the id is out of range.
 * Corresponds to OoT Audio_GetInstrumentInner (original name ProgToVp).
 * Not reached in play (2026-11-13): did not run in a GB Player session (boot, Continue, menus) or
 * in a tour of the menus and a COM battle.
 * @unverified Name and comment guessed from the code.
 */
u8* AudioPlayback_GetInstrumentInner(s32 arg0, s32 arg1) {
    SoundFont* bank;
    u8* ret;

    if (AudioLoad_IsFontLoadComplete(arg0) == 0) {
        D_811859D0 = arg0 + 0x10000000;
        return NULL;
    }
    bank = (SoundFont*)((u8*)D_811858B0 + arg0 * sizeof(SoundFont));
    if (arg1 >= bank->numInstruments) {
        D_811859D0 = (arg0 << 8) + arg1 + 0x03000000;
        return NULL;
    }
    ret = bank->instruments[arg1];
    if (ret == NULL) {
        D_811859D0 = (arg0 << 8) + arg1 + 0x01000000;
        return ret;
    }
    return ret;
}

/**
 * Returns drum `arg1` of font `arg0`, or NULL when the font is not loaded or the id is out of range.
 * Corresponds to OoT Audio_GetDrum (original name PercToPp).
 * Not reached in play (2026-11-13): did not run in a GB Player session (boot, Continue, menus) or
 * in a tour of the menus and a COM battle.
 * @unverified Name and comment guessed from the code.
 */
u8* AudioPlayback_GetDrum(s32 arg0, s32 arg1) {
    SoundFont* bank;
    u8* ret;

    if (AudioLoad_IsFontLoadComplete(arg0) == 0) {
        D_811859D0 = arg0 + 0x10000000;
        return NULL;
    }
    bank = (SoundFont*)((u8*)D_811858B0 + arg0 * sizeof(SoundFont));
    if (arg1 >= bank->numDrums) {
        D_811859D0 = (arg0 << 8) + arg1 + 0x04000000;
        return NULL;
    }
    if ((uintptr_t)bank->drums < 0x80000000) {
        return NULL;
    }
    ret = bank->drums[arg1];
    if (ret == NULL) {
        D_811859D0 = (arg0 << 8) + arg1 + 0x05000000;
    }
    return ret;
}

#define ADSR_ACTION_RELEASE_E3C 0x10
#define ADSR_ACTION_DECAY_E3C 0x20

extern f32 D_811858D4;
extern f32 D_811858D8;
void AudioPlayback_AudioListPushFront(AudioListItem* arg0, AudioListItem* arg1);

/**
 * Starts the decay (6) or release (7) of the note a sequence layer is playing.
 * Corresponds to OoT Audio_SeqLayerDecayRelease (original name __Nas_Release_Channel_Main).
 * Not reached in play (2026-11-13): did not run in a GB Player session (boot, Continue, menus) or
 * in a tour of the menus and a COM battle.
 * @unverified Name and comment guessed from the code.
 */
void AudioPlayback_SeqLayerDecayRelease(SequenceLayer* layer, s32 target) {
    Note* note;
    NoteAttributes* attrs;

    if (layer == (SequenceLayer*)-1) {
        return;
    }

    layer->status = 0;

    if (layer->note == NULL) {
        return;
    }

    note = layer->note;
    attrs = &note->playbackState.attributes;

    if (note->playbackState.wantedParentLayer == layer) {
        note->playbackState.wantedParentLayer = (SequenceLayer*)-1;
    }

    if (note->playbackState.parentLayer != layer) {
        if (note->playbackState.parentLayer == (SequenceLayer*)-1 && note->playbackState.wantedParentLayer == (SequenceLayer*)-1 &&
            note->playbackState.prevParentLayer == layer && target != 6) {
            note->playbackState.adsr.fadeOutVel = D_811858D4;
            note->playbackState.adsr.action |= ADSR_ACTION_RELEASE_E3C;
        }
        return;
    }

    if (note->playbackState.adsr.state != 6) {
        attrs->freqScale = layer->noteFreqScale;
        attrs->velocity = layer->noteVelocity;
        attrs->pan = layer->notePan;
        attrs->stereo = layer->stereo;
        if (layer->channel != NULL) {
            attrs->reverbVol = layer->channel->reverb;
            attrs->gain = layer->channel->gain;
            attrs->filter = layer->channel->filter;
            if (layer->channel->seqPlayer->muted && (layer->channel->muteBehavior & 8)) {
                note->sampleState.finished = 1;
            }
            note->playbackState.priority = layer->channel->someOtherPriority;
        } else {
            note->playbackState.priority = 1;
        }

        note->playbackState.prevParentLayer = note->playbackState.parentLayer;
        note->playbackState.parentLayer = (SequenceLayer*)-1;
        if (target == 7) {
            note->playbackState.adsr.fadeOutVel = D_811858D4;
            note->playbackState.adsr.action |= ADSR_ACTION_RELEASE_E3C;
            note->playbackState.unk_04 = 2;
        } else {
            note->playbackState.unk_04 = 1;
            note->playbackState.adsr.action |= ADSR_ACTION_DECAY_E3C;
            if (layer->adsr.decayIndex == 0) {
                note->playbackState.adsr.fadeOutVel = layer->channel->adsr.decayIndex * D_811858D8;
            } else {
                note->playbackState.adsr.fadeOutVel = layer->adsr.decayIndex * D_811858D8;
            }
            note->playbackState.adsr.sustain = ((f32)(s32)layer->channel->adsr.sustain * note->playbackState.adsr.current) / 256.0f;
        }
    }

    if (target == 6) {
        AudioPlayback_AudioListRemove(&note->listItem);
        AudioPlayback_AudioListPushFront(&note->listItem.pool->decaying, &note->listItem);
    }
}

/**
 * Starts the decay of the layer's note.
 * Corresponds to OoT Audio_SeqLayerNoteDecay (original name Nas_Release_Channel).
 * Not reached in play (2026-11-13): did not run in a GB Player session (boot, Continue, menus) or
 * in a tour of the menus and a COM battle.
 * @unverified Name and comment guessed from the code.
 */
void AudioPlayback_SeqLayerNoteDecay(Particle* arg0) {
    AudioPlayback_SeqLayerDecayRelease((struct SequenceLayer*)arg0, 6);
}

/**
 * Starts the release of the layer's note.
 * Corresponds to OoT Audio_SeqLayerNoteRelease (original name Nas_Release_Channel_Force).
 * No caller in play (2026-11-13): nothing that runs reaches it, not even through callbacks, data
 * tables or fragment headers.
 * @unverified Name and comment guessed from the code.
 */
void AudioPlayback_SeqLayerNoteRelease(Particle* arg0) {
    AudioPlayback_SeqLayerDecayRelease((struct SequenceLayer*)arg0, 7);
}

/**
 * Makes an empty circular note list.
 * Corresponds to OoT Audio_InitNoteList (original name __Nas_InitList).
 * Seen in play (2026-11-13) ran when the GB Player started (main menu GB, Yes; second entry after
 * quitting).
 * Seen in play (2026-11-14) ran when the GB Player started (main menu GB, はい "Yes") with JP Green in the
 * Transfer Pak; called from AudioPlayback_InitNoteLists.
 */
void AudioPlayback_InitNoteList(AudioListItem* arg0) { // PORT: typed (was s8* with N64 offsets)
    arg0->prev = arg0;
    arg0->next = arg0;
    arg0->u.count = 0;
}

/**
 * Empties the four lists of a NotePool (disabled, decaying, releasing and active at arg0 + 0x00,
 * 0x10, 0x20 and 0x30) with AudioPlayback_InitNoteList and points each list head's pool field
 * (+0xC) at the pool.
 * Corresponds to OoT Audio_InitNoteLists (original name Nas_InitChNode).
 * Seen in play (2026-11-13) ran when the GB Player started (main menu GB, Yes; second entry after
 * quitting).
 * Seen in play (2026-11-14) ran when the GB Player started (main menu GB, はい "Yes") with JP Green in the
 * Transfer Pak; called from AudioPlayback_InitNoteFreeList.
 */
void AudioPlayback_InitNoteLists(NotePool* arg0) { // PORT: typed (was s8* with N64 offsets)
    AudioPlayback_InitNoteList(&arg0->disabled);
    AudioPlayback_InitNoteList(&arg0->decaying);
    AudioPlayback_InitNoteList(&arg0->releasing);
    AudioPlayback_InitNoteList(&arg0->active);
    arg0->disabled.pool = arg0;
    arg0->decaying.pool = arg0;
    arg0->releasing.pool = arg0;
    arg0->active.pool = arg0;
}

extern Note* D_81184664;
extern s32 D_811858E8;

/**
 * Sets up the global note pool D_81184938 and puts all D_811858E8 notes (D_81184664) on its
 * disabled list: each note's listItem gets the note as its value and is marked as on no list
 * before it is pushed.
 * Corresponds to OoT Audio_InitNoteFreeList (original name Nas_InitChannelList).
 * Seen in play (2026-11-13) ran when the GB Player started (main menu GB, Yes; second entry after
 * quitting).
 * Seen in play (2026-11-14) ran when the GB Player started (main menu GB, はい "Yes") with JP Green in the
 * Transfer Pak; called from AudioHeap_Init.
 */
void AudioPlayback_InitNoteFreeList(void) {
    s32 i;

    AudioPlayback_InitNoteLists(&D_81184938);
    for (i = 0; i < D_811858E8; i++) {
        D_81184664[i].listItem.u.value = &D_81184664[i];
        D_81184664[i].listItem.prev = 0;
        AudioSeq_AudioListPushBack(&D_81184938.disabled, &D_81184664[i].listItem);
    }
}


/**
 * Moves every note in the four lists of a note pool back to the matching
 * global free list (D_81184938, D_81184948, D_81184958, D_81184968). Same
 * structure as note_pool_clear in the Super Mario 64 audio library.
 * Corresponds to OoT Audio_NotePoolClear (original name Nas_DeAllocAllVoices).
 * Seen in play (2026-11-13) ran when the GB Player started (main menu GB, Yes; second entry after
 * quitting).
 * Seen in play (2026-11-14) ran when the GB Player started (main menu GB, はい "Yes") with JP Green in the
 * Transfer Pak; called from AudioSeq_SequencePlayerDisable.
 */
void AudioPlayback_NotePoolClear(NotePool* pool) {
    s32 i;
    AudioListItem* source;
    AudioListItem* cur;
    AudioListItem* dest; // PORT: typed (was s8*; D_81184948..D_81184968 are the lists of D_81184938)

    for (i = 0; i < 4; i++) {
        switch (i) {
            case 0:
                source = &pool->disabled;
                dest = &D_81184938.disabled;
                break;

            case 1:
                source = &pool->decaying;
                dest = &D_81184938.decaying;
                break;

            case 2:
                source = &pool->releasing;
                dest = &D_81184938.releasing;
                break;

            case 3:
                source = &pool->active;
                dest = &D_81184938.active;
                break;
        }

        for (;;) {
            cur = source->next;
            if (cur == source) {
                break;
            }
            if (cur == NULL) {
                break;
            }
            AudioPlayback_AudioListRemove(cur);
            AudioSeq_AudioListPushBack(dest, cur);
        }
    }
}

/**
 * Takes `arg1` notes from the global free lists into a pool.
 * Corresponds to OoT Audio_NotePoolFill (original name Nas_AllocVoices).
 * No caller in play (2026-11-13): nothing that runs reaches it, not even through callbacks, data
 * tables or fragment headers.
 * @unverified Name and comment guessed from the code.
 */
void AudioPlayback_NotePoolFill(NotePool* arg0, s32 arg1) { // PORT: typed (was s8* with N64 offsets)
    s32 pad[3]; // FAKE: stack padding; places src/dst at sp+0x48/0x44 and the frame at 0x58 as in the ROM
    AudioListItem* src;
    AudioListItem* dst;
    s32 n;
    s32 pass;
    Note* r;

    AudioPlayback_NotePoolClear(arg0);
    for (pass = 0, n = 0; n < arg1; pass++) {
        if (pass == 4) {
            return;
        }
        switch (pass) {
            case 0:
                src = &D_81184938.disabled;
                dst = &arg0->disabled;
                break;
            case 1:
                src = &D_81184938.decaying;
                dst = &arg0->decaying;
                break;
            case 2:
                src = &D_81184938.releasing;
                dst = &arg0->releasing;
                break;
            case 3:
                src = &D_81184938.active;
                dst = &arg0->active;
                break;
        }
        while (n < arg1) {
            r = (Note*)AudioSeq_AudioListPopBack(src);
            if (r == NULL) {
                break;
            }
            AudioSeq_AudioListPushBack(dst, &r->listItem);
            n++;
        }
    }
}

/**
 * Pushes list item `arg1` onto the front of list `arg0` if it is on no list.
 * Corresponds to OoT Audio_AudioListPushFront (original name Nas_AddListHead).
 * Not reached in play (2026-11-13): did not run in a GB Player session (boot, Continue, menus) or
 * in a tour of the menus and a COM battle.
 * @unverified Name and comment guessed from the code.
 */
void AudioPlayback_AudioListPushFront(AudioListItem* arg0, AudioListItem* arg1) { // PORT: typed (was N64 offsets)
    if (arg1->prev == NULL) {
        arg1->prev = arg0;
        arg1->next = arg0->next;
        arg0->next->prev = arg1;
        arg0->next = arg1;
        arg0->u.count++;
        arg1->pool = arg0->pool;
    }
}

/**
 * Unlinks a list item from its list.
 * Corresponds to OoT Audio_AudioListRemove (original name Nas_CutList).
 * Not reached in play (2026-11-13): did not run in a GB Player session (boot, Continue, menus) or
 * in a tour of the menus and a COM battle.
 * @unverified Name and comment guessed from the code.
 */
void AudioPlayback_AudioListRemove(AudioListItem* arg0) { // PORT: typed (was s8* with N64 offsets)
    if (arg0->prev != NULL) {
        arg0->prev->next = arg0->next;
        arg0->next->prev = arg0->prev;
        arg0->prev = NULL;
    }
}

/**
 * Returns the note in list `arg0` with the lowest priority below `arg1`, or NULL.
 * Corresponds to OoT Audio_FindNodeWithPrioLessThan (original name __Nas_GetLowerPrio).
 * No caller in play (2026-11-13): nothing that runs reaches it, not even through callbacks, data
 * tables or fragment headers.
 * @unverified Name and comment guessed from the code.
 */
Note* AudioPlayback_FindNodeWithPrioLessThan(AudioListItem* arg0, s32 arg1) { // PORT: typed (was s8* with N64 offsets)
    Note* temp_a0;
    AudioListItem* var_v0;
    AudioListItem* var_v1;

    var_v0 = arg0->next;
    if (var_v0 == arg0) {
        return NULL;
    }
    var_v1 = var_v0;
    if (var_v0 != arg0) {
        do {
            if ((s32)((Note*)var_v1->u.value)->playbackState.priority >= (s32)((Note*)var_v0->u.value)->playbackState.priority) {
                var_v1 = var_v0;
            }
            var_v0 = var_v0->next;
        } while (var_v0 != arg0);
    }
    if (var_v1 == NULL) {
        return NULL;
    }
    temp_a0 = (Note*)var_v1->u.value;
    if ((s32)temp_a0->playbackState.priority >= arg1) {
        return NULL;
    }
    return temp_a0;
}

/**
 * Gives note `arg0` to layer `arg1` and sets it up from the layer's instrument.
 * Corresponds to OoT Audio_NoteInitForLayer (original name Nas_EntryTrack). OoT adds synthetic
 * waves.
 * No caller in play (2026-11-13): nothing that runs reaches it, not even through callbacks, data
 * tables or fragment headers.
 * @unverified Name and comment guessed from the code.
 */
void AudioPlayback_NoteInitForLayer(Note* arg0, SequenceLayer* arg1) { // PORT: typed (was s8* with N64 offsets)
    NoteSampleState* flags;
    s32 v;

    arg0->playbackState.prevParentLayer = (SequenceLayer*)-1;
    arg0->playbackState.parentLayer = arg1;
    arg0->playbackState.priority = arg1->channel->notePriority;
    arg1->notePropertiesNeedInit = 1;
    arg1->status = 3;
    arg1->note = arg0;
    arg1->channel->noteUnused = arg0;
    arg1->channel->layerUnused = arg1;
    arg1->noteVelocity = 0.0f;
    AudioPlayback_NoteInit(arg0);
    v = arg1->instOrWave;
    flags = &arg0->sampleState;
    if (v == 0xFF) {
        v = arg1->channel->instOrWave;
    }
    flags->tunedSample = arg1->sound;
    if (v >= 0x80 && v < 0xC0) {
        flags->isSyntheticWave = 1;
    } else {
        flags->isSyntheticWave = 0;
    }
    arg0->playbackState.fontId = arg1->channel->fontId;
    flags->stereoHeadsetEffects = arg1->channel->stereoHeadsetEffects;
    flags->reverbIndex = arg1->channel->reverbIndex & 3;
}


/**
 * Releases the note's current layer and queues layer `arg1` to take it over.
 * Corresponds to OoT func_800E82C0 (original name __Nas_InterTrack). OoT leaves it unnamed (MM:
 * func_801963E8).
 * No caller in play (2026-11-13): nothing that runs reaches it, not even through callbacks, data
 * tables or fragment headers.
 * @unverified Name and comment guessed from the code.
 */
void AudioPlayback_NoteReleaseOwnerAndQueueLayer(Note* arg0, SequenceLayer* arg1) { // PORT: typed (was N64 offsets)
    AudioPlayback_SeqLayerNoteRelease((Particle*)arg0->playbackState.parentLayer);
    arg0->playbackState.wantedParentLayer = arg1;
}

extern f32 D_FLT_811858D4;
/**
 * Starts releasing the note and hands it to layer `arg1` with the layer's priority.
 * Corresponds to OoT Audio_NoteReleaseAndTakeOwnership (original name __Nas_InterReleaseTrack).
 * No caller in play (2026-11-13): nothing that runs reaches it, not even through callbacks, data
 * tables or fragment headers.
 * @unverified Name and comment guessed from the code.
 */
void AudioPlayback_NoteReleaseAndTakeOwnership(Note* arg0, SequenceLayer* arg1) { // PORT: typed (was N64 offsets)
    arg0->playbackState.wantedParentLayer = arg1;
    arg0->playbackState.priority = arg1->channel->notePriority;
    arg0->playbackState.adsr.fadeOutVel = D_FLT_811858D4;
    arg0->playbackState.adsr.action |= 0x10;
}


/**
 * Allocates a note for a layer from a pool's disabled list.
 * Corresponds to OoT Audio_AllocNoteFromDisabled.
 * No caller in play (2026-11-13): nothing that runs reaches it, not even through callbacks, data
 * tables or fragment headers.
 * @unverified Name and comment guessed from the code.
 */
Note* AudioPlayback_AllocNoteFromDisabled(NotePool* arg0, SequenceLayer* arg1) { // PORT: typed (was N64 offsets)
    Note* ret;

    ret = (Note*)AudioSeq_AudioListPopBack(&arg0->disabled);
    if (ret != NULL) {
        AudioPlayback_NoteInitForLayer(ret, arg1);
        AudioPlayback_AudioListPushFront(&arg0->active, &ret->listItem);
    }
    return ret;
}


/**
 * Takes over a decaying note for a layer.
 * Corresponds to OoT Audio_AllocNoteFromDecaying.
 * No caller in play (2026-11-13): nothing that runs reaches it, not even through callbacks, data
 * tables or fragment headers.
 * @unverified Name and comment guessed from the code.
 */
Note* AudioPlayback_AllocNoteFromDecaying(NotePool* arg0, SequenceLayer* arg1) { // PORT: typed (was N64 offsets)
    Note* ret;

    ret = (Note*)AudioSeq_AudioListPopBack(&arg0->decaying);
    if (ret != NULL) {
        AudioPlayback_NoteReleaseAndTakeOwnership(ret, arg1);
        AudioSeq_AudioListPushBack(&arg0->releasing, &ret->listItem);
    }
    return ret;
}

/**
 * Steals the lowest-priority active note for a layer.
 * Corresponds to OoT Audio_AllocNoteFromActive (original name __Nas_ChLookRelWait).
 * No caller in play (2026-11-13): nothing that runs reaches it, not even through callbacks, data
 * tables or fragment headers.
 * @unverified Name and comment guessed from the code.
 */
Note* AudioPlayback_AllocNoteFromActive(NotePool* pool, SequenceLayer* layer) { // PORT: typed (was N64 offsets)
    Note* r_note;
    Note* a_note;
    s32 r_priority;
    s32 a_priority;

    r_priority = a_priority = 0x10;
    r_note = AudioPlayback_FindNodeWithPrioLessThan(&pool->releasing, layer->channel->notePriority);
    if (r_note != NULL) {
        r_priority = r_note->playbackState.priority;
    }
    a_note = AudioPlayback_FindNodeWithPrioLessThan(&pool->active, layer->channel->notePriority);
    if (a_note != NULL) {
        a_priority = a_note->playbackState.priority;
    }
    if ((r_note == NULL) && (a_note == NULL)) {
        return NULL;
    }
    if (a_priority < r_priority) {
        AudioPlayback_AudioListRemove(&a_note->listItem);
        AudioPlayback_NoteReleaseOwnerAndQueueLayer(a_note, layer);
        AudioSeq_AudioListPushBack(&pool->releasing, &a_note->listItem);
        a_note->playbackState.priority = layer->channel->notePriority;
        return a_note;
    }
    r_note->playbackState.wantedParentLayer = layer;
    r_note->playbackState.priority = layer->channel->notePriority;
    return r_note;
}

/**
 * Allocates a note for a layer by its channel's note-stealing policy.
 * Corresponds to OoT Audio_AllocNote (original name Nas_AllocationOnRequest).
 * No caller in play (2026-11-13): nothing that runs reaches it, not even through callbacks, data
 * tables or fragment headers.
 * @unverified Name and comment guessed from the code.
 */
Note* AudioPlayback_AllocNote(SequenceLayer* arg0) { // PORT: typed (was s8* with N64 offsets)
    Note* voice;
    s32 flags = arg0->channel->noteAllocPolicy;
    NotePool* chPool = &arg0->channel->notePool;
    NotePool* playerPool = &arg0->channel->seqPlayer->notePool;

    if (flags & 1) {
        voice = arg0->note;
        if ((voice != NULL) && (arg0 == voice->playbackState.prevParentLayer) && (voice->playbackState.wantedParentLayer == (SequenceLayer*)-1)) {
            AudioPlayback_NoteReleaseAndTakeOwnership(voice, arg0);
            AudioPlayback_AudioListRemove(&voice->listItem);
            AudioSeq_AudioListPushBack(&voice->listItem.pool->releasing, &voice->listItem);
            return voice;
        }
    }
    if (flags & 2) {
        if (((voice = AudioPlayback_AllocNoteFromDisabled(chPool, arg0)) != NULL) ||
            ((voice = AudioPlayback_AllocNoteFromDecaying(chPool, arg0)) != NULL) ||
            ((voice = AudioPlayback_AllocNoteFromActive(chPool, arg0)) != NULL)) {
            return voice;
        }
    } else if (flags & 4) {
        if (((voice = AudioPlayback_AllocNoteFromDisabled(chPool, arg0)) != NULL) ||
            ((voice = AudioPlayback_AllocNoteFromDisabled(playerPool, arg0)) != NULL) ||
            ((voice = AudioPlayback_AllocNoteFromDecaying(chPool, arg0)) != NULL) ||
            ((voice = AudioPlayback_AllocNoteFromDecaying(playerPool, arg0)) != NULL) ||
            ((voice = AudioPlayback_AllocNoteFromActive(chPool, arg0)) != NULL) ||
            ((voice = AudioPlayback_AllocNoteFromActive(playerPool, arg0)) != NULL)) {
            return voice;
        }
    } else if (flags & 8) {
        if (((voice = AudioPlayback_AllocNoteFromDisabled(&D_81184938, arg0)) != NULL) ||
            ((voice = AudioPlayback_AllocNoteFromDecaying(&D_81184938, arg0)) != NULL) ||
            ((voice = AudioPlayback_AllocNoteFromActive(&D_81184938, arg0)) != NULL)) {
            return voice;
        }
    } else {
        if (((voice = AudioPlayback_AllocNoteFromDisabled(chPool, arg0)) != NULL) ||
            ((voice = AudioPlayback_AllocNoteFromDisabled(&D_81184938, arg0)) != NULL) ||
            ((voice = AudioPlayback_AllocNoteFromDecaying(chPool, arg0)) != NULL) ||
            ((voice = AudioPlayback_AllocNoteFromDecaying(&D_81184938, arg0)) != NULL) ||
            ((voice = AudioPlayback_AllocNoteFromActive(chPool, arg0)) != NULL) ||
            ((voice = AudioPlayback_AllocNoteFromActive(&D_81184938, arg0)) != NULL)) {
            return voice;
        }
    }
    arg0->status = 0;
    return NULL;
}

extern NoteSampleState D_811369A8;
/**
 * Resets every note's state and sample buffers.
 * Corresponds to OoT Audio_NoteInitAll (original name Nas_ChannelInit). Zero allocates with
 * AudioHeap_Alloc; OoT uses AudioHeap_AllocDmaMemory.
 * Seen in play (2026-11-13) ran when the GB Player started (main menu GB, Yes; second entry after
 * quitting).
 * Seen in play (2026-11-14) ran when the GB Player started (main menu GB, はい "Yes") with JP Green in the
 * Transfer Pak; called from AudioHeap_Init.
 */
void AudioPlayback_NoteInitAll(void) {
    s32 i;
    Note* rec;

    for (i = 0; i < D_811858E8; i++) {
        rec = (Note*)&D_81184664[i];
        rec->sampleState = D_811369A8;
        rec->playbackState.priority = 0;
        rec->playbackState.unk_04 = 0;
        rec->playbackState.parentLayer = (SequenceLayer*)-1; // PORT: was -1 (same value, made explicit)
        rec->playbackState.wantedParentLayer = (SequenceLayer*)-1; // PORT: was -1 (same value, made explicit)
        rec->playbackState.prevParentLayer = (SequenceLayer*)-1; // PORT: was -1 (same value, made explicit)
        rec->playbackState.waveId = 0;
        rec->playbackState.attributes.velocity = 0.0f;
        rec->playbackState.adsrVolScale = 0;
        rec->playbackState.adsr.state = 0;
        rec->playbackState.adsr.action = 0;
        rec->playbackState.vibratoState.active = 0;
        rec->playbackState.portamento.cur = 0.0f;
        rec->playbackState.portamento.speed = 0.0f;
        rec->synthesisState.synthesisBuffers = AudioHeap_Alloc(&D_811842C0, 0x80);
    }
}
