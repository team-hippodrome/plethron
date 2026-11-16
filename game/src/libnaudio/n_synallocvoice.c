/* Stadium Zero resident code. */

#include "global.h"

/*
 * n_alSynAllocVoice: initializes the record `arg0` from the descriptor `arg1` and
 * tries to attach a second object to it.
 *
 * `arg1` holds a s16 at +0 (copied to arg0+0x16 and used as the lookup key), a s16 at
 * +2 (copied to arg0+0x18) and a u8 at +4 (copied, widened, to arg0+0x1A).  arg0+0x14
 * and the words at arg0+0x8 / +0xC are cleared.  _allocatePVoice(&obj, key) then looks
 * up an object.  If it leaves a non-NULL `obj`, the two are linked (obj+0x8 and
 * arg0+0x8 point at each other).  If _allocatePVoice also returned non-zero, obj's word
 * at +0x88 is set to 0x200 (after clearing word +0x8 of the record that obj+0x8 pointed
 * at before the link) and two blocks are posted with func_8003AE2C(obj, 3, block): the
 * first has halfword +8 = 0xB (word +4 = D_80075E44[+0x1C], word +0xC = 0, word +0x10 = obj[+0x88] - 0x40) and,
 * if func_800448F8 can supply a second block, one with halfword +8 = 0xF (word +0 = 0,
 * word +4 = D_80075E44[+0x1C] + obj[+0x88]).  If it returned zero, obj[+0x88] is cleared
 * and only the link is made.  Returns 1 if an object was found, else 0.  (The meaning
 * of these records is not established here; the field offsets are taken from the retail code.)
 */

// PORT: typed with the n_audio structs (was M2C_FIELD offsets into void*), so the fields land at their host offsets.
// N64 offsets: voice priority 0x16, fxBus 0x18, unityPitch 0x1A, state 0x14, table 0xC, pvoice 0x8; pvoice offset 0x88;
// synth paramSamples 0x1C. The update types are AL_FILTER_SET_VOLUME (0xB) and AL_FILTER_STOP_VOICE (0xF), posted
// with AL_FILTER_ADD_UPDATE (3).
#include "n_synthInternals.h"

s32 _allocatePVoice(N_PVoice** pvoice, s16 priority);

s32 n_alSynAllocVoice(N_ALVoice* arg0, ALVoiceConfig* arg1) {
    N_PVoice* obj;
    s32 found;
    ALParam* msg_a;
    ALParam* msg_b;

    obj = NULL;
    arg0->priority = (s16)arg1->priority;
    arg0->unityPitch = (s16)arg1->unityPitch;
    arg0->table = NULL;
    arg0->fxBus = (s16)arg1->fxBus;
    arg0->state = 0;
    arg0->pvoice = NULL;
    found = _allocatePVoice(&obj, arg1->priority);
    if (obj) {
        if (found != 0) {
            obj->offset = 0x200;
            obj->vvoice->pvoice = NULL;
            obj->vvoice = arg0;
            arg0->pvoice = obj;
            msg_a = __n_allocParam();
            msg_a->delta = n_syn->paramSamples;
            msg_a->type = AL_FILTER_SET_VOLUME;
            msg_a->data.i = 0;
            msg_a->moredata.i = obj->offset - 0x40;
            n_alEnvmixerParam(arg0->pvoice, AL_FILTER_ADD_UPDATE, msg_a);
            msg_b = __n_allocParam();
            if (msg_b != NULL) {
                msg_b->delta = obj->offset + n_syn->paramSamples;
                msg_b->type = AL_FILTER_STOP_VOICE;
                msg_b->next = NULL;
                n_alEnvmixerParam(arg0->pvoice, AL_FILTER_ADD_UPDATE, msg_b);
            }
        } else {
            obj->offset = 0;
            obj->vvoice = arg0;
            arg0->pvoice = obj;
        }
    }
    return obj != NULL;
}

/**
 * Seen in play (2026-11-13) (tour of menus and a COM battle): ran in the main menu, the Pokédex,
 * the party screen, the Lab list, Lab sort, registration, the Lab PC, Free Battle setup, the battle
 * opening, a switch in battle, running from battle and a Pound turn.
 * @unverified Name from Stadium 1, matched by code similarity.
 */
s32 _allocatePVoice(N_PVoice** pvoice, s16 priority) {
    ALLink* dl;
    N_PVoice* pv;
    s32 stolen = 0;

    if ((dl = n_syn->pLameList.next) != NULL) {
        *pvoice = (N_PVoice*)dl;
        alUnlink(dl);
        alLink(dl, &n_syn->pAllocList);
    } else if ((dl = n_syn->pFreeList.next) != NULL) {
        *pvoice = (N_PVoice*)dl;
        alUnlink(dl);
        alLink(dl, &n_syn->pAllocList);
    } else {
        for (dl = n_syn->pAllocList.next; dl != NULL; dl = dl->next) {
            pv = (N_PVoice*)dl;
            if (pv->vvoice->priority <= priority && pv->offset == 0) {
                *pvoice = pv;
                priority = pv->vvoice->priority;
                stolen = 1;
            }
        }
    }
    return stolen;
}
